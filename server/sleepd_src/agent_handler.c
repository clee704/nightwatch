#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <netinet/ether.h>

#include "agent_handler.h"
#include "agent.h"
#include "logger.h"
#include "network.h"
#include "protocol.h"
#include "message_exchange.h"

#define LOGGER_PREFIX "[agent_handler] "

#define BACKLOG_SIZE 64
#define MAX_AGENTS 32

struct in_conn {
    int fd;
    struct sockaddr_in addr;
};

static void *accept_agents(void *agent_list);
static void assign_thread(struct in_conn);
static void *handle_agent(void *conn);
static int read_info(int fd, struct message_buffer *,
                     const char **hostname,
                     struct ether_addr *);
static void handle_new_agent(const struct in_conn *,
                             const char *hostname,
                             const struct ether_addr *);
static void handle_existing_agent(const struct in_conn *, struct agent *);
static void manage_agent(struct agent *);
static void close_connection(int fd);
static void close_connections_with_agent(struct agent *, int state);

static int sock;
static struct agent_list *list;

int start_agent_handler(pthread_t *tid, struct agent_list *list_, int port)
{
    struct sockaddr_in addr;

    set_sockaddr_in(&addr, AF_INET, port, INADDR_ANY);
    sock = init_server(SOCK_STREAM,
                       (struct sockaddr *) &addr, sizeof(addr),
                       BACKLOG_SIZE);
    if (sock < 0) {
        ERROR("can't listen on port %d: %m", port);
        return -1;
    }
    list = list_;
    if (pthread_create(tid, NULL, accept_agents, NULL)) {
        ERROR("can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *accept_agents(void *unused)
{
    struct in_conn conn;
    socklen_t addr_len;
    char ip_str[INET_ADDRSTRLEN];

    while (1) {
        addr_len = sizeof(conn.addr);
        conn.fd = accept(sock, (struct sockaddr *) &conn.addr, &addr_len);
        if (conn.fd < 0) {
            WARNING("accept() failed: %m");
            continue;
        }
        inet_ntop(AF_INET, &conn.addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        INFO("new connection (fd=%d) from %s", conn.fd, ip_str);
        assign_thread(conn);
    }

    // should not reach here
    ERROR("accept_agents() exits (it should not happen)");
    unused = unused;
    return NULL;
}

static void assign_thread(struct in_conn conn)
{
    pthread_t tid;

    if (list->size >= MAX_AGENTS)
        INFO("maximum number of agents (%d) exceeded", MAX_AGENTS);
    else
        if (pthread_create(&tid, NULL, handle_agent, (void *) &conn))
            WARNING("can't create a thread for the connection (fd=%d): %m",
                    conn.fd);
        else
            return;
    close_connection(conn.fd);
}

static void *handle_agent(void *conn_)
{
    const struct in_conn *conn = (struct in_conn *) conn_;
    int status;
    struct message_buffer buf;
    const char *hostname;
    struct ether_addr mac;
    struct agent *agent;

    status = read_info(conn->fd, &buf, &hostname, &mac);
    respond(conn->fd, status, &buf);
    if (status != 200) {
        close_connection(conn->fd);
        return NULL;
    }
    agent = find_agent_by_mac(list, &mac);
    if (agent == NULL) {  // new agent
        INFO("the connection (fd=%d) is from a new agent", conn->fd);
        handle_new_agent(conn, hostname, &mac);
    } else {
        INFO("the connection (fd=%d) may be from an existing agent", conn->fd);
        handle_existing_agent(conn, agent);
    }
    return NULL;
}

static int read_info(int fd,
                     struct message_buffer *buf,
                     const char **hostname,
                     struct ether_addr *mac)
{
    struct request *request = &buf->u.request;
    char *chars = buf->chars;
    char *c;
    int n;

    n = read(fd, chars, sizeof(buf->chars));
    if (n < 0) {
        WARNING("read() failed: %m");
        return 500;
    } else if (n == 0) {
        WARNING("unexpected EOF while waiting for INFO request");
        return 400;
    }
    chars[n] = 0;
    if (parse_request(chars, request)) {
        WARNING("invalid request");
        return 400;
    }
    if (request->method != INFO) {
        WARNING("unexpected method while waiting for INFO request");
        return 418;
    }
    if (!request->has_data) {
        WARNING("got INFO with no data");
        return 402;
    }
    *hostname = request->data;
    c = strchr(request->data, '\n');
    if (c == NULL)
        goto malformed_data;
    c[0] = 0;
    if (ether_aton_r(c + 1, mac) == NULL)
        goto malformed_data;
    c = strchr(c + 1, '\n');
    if (c == NULL)
        goto malformed_data;
    c[0] = 0;
    if (c[1] != 0)
        goto malformed_data;
    return 200;
malformed_data:
    WARNING("got INFO with malformed data");
    return 400;
}

static void handle_new_agent(const struct in_conn *conn,
                             const char *hostname,
                             const struct ether_addr *mac)
{
    struct agent *agent;
    struct sockaddr_in sa;

    agent = (struct agent *) malloc(sizeof(struct agent));
    if (agent == NULL) {
        WARNING("can't allocate memory for the new agent: %m");
        goto close;
    }
    strcpy(agent->hostname, hostname);
    agent->ip = conn->addr.sin_addr;
    agent->mac = *mac;
    agent->state = UP;
    agent->monitored_since = time(NULL);
    agent->total_uptime = 0;
    agent->total_downtime = 0;
    agent->sleep_time = 0;
    agent->fd1 = conn->fd;
    set_sockaddr_in(&sa, AF_INET, 4444, agent->ip.s_addr);
    agent->fd2 = connect_to(SOCK_STREAM,
                            (struct sockaddr *) &sa,
                            sizeof(struct sockaddr_in));
    if (agent->fd2 < 0) {
        WARNING("can't connect to the agent: %m");
        goto close;
    }
    if (pthread_mutex_init(&agent->mutex, NULL)) {
        WARNING("can't create a mutex: %m");
        goto close_both;
    }
    if (add_new_agent(list, agent))
        goto destroy_mutex;

    manage_agent(agent);

    // should not reach here
    ERROR("manage_agent() exits (it should not happen)");
    goto destroy_mutex;

destroy_mutex:
    if (pthread_mutex_destroy(&agent->mutex))
        WARNING("can't destroy the mutex: %m");

close_both:
    close_connection(agent->fd2);

close:
    close_connection(conn->fd);

    if (agent != NULL)
        free(agent);
}

static void handle_existing_agent(const struct in_conn *conn,
                                  struct agent *agent)
{
    const struct agent *agent2;
    struct sockaddr_in sa;
    char ip_str[INET_ADDRSTRLEN];

    lock_agent(agent);  // since the agent is in the list
    if (agent->state == UP) {
        WARNING("multiple connections with one agent");
        goto close;
    }
    agent->fd1 = conn->fd;
    set_sockaddr_in(&sa, AF_INET, 4444, agent->ip.s_addr);
    agent->fd2 = connect_to(SOCK_STREAM,
                            (struct sockaddr *) &sa,
                            sizeof(struct sockaddr_in));
    if (agent->fd2 < 0) {
        WARNING("can't connect to the agent: %m");
        unlock_agent(agent);
        goto close;
    }
    // Check if MAC addresses match but IP addresses doesn't,
    if (!IN_ADDR_EQ(agent->ip, conn->addr.sin_addr)) {
        agent2 = find_agent_by_ip(list, &conn->addr.sin_addr);
        // Check if the new IP address is already in use
        if (agent2 != NULL) {
            inet_ntop(AF_INET, &agent->ip, ip_str, INET_ADDRSTRLEN);
            WARNING("IP addresses collide: %s", ip_str);
            goto close_both;
        }
        // Replace the old IP address with the new one
        agent->ip = conn->addr.sin_addr;
    }
    agent->state = UP;
    unlock_agent(agent);
    return;

close_both:
    close_connection(agent->fd2);

close:
    close_connection(conn->fd);

    if (agent->state != UP) {
        agent->fd1 = -1;
        agent->fd2 = -1;
    }
    unlock_agent(agent);
}

static void manage_agent(struct agent *agent)
{
    struct message_buffer buf;
    struct request *request = &buf.u.request;
    int n;
    char ip_str[INET_ADDRSTRLEN];

    while (1) {
        while (agent->state != UP)
            sleep(1);
        n = read(agent->fd1, buf.chars, sizeof(buf.chars) - 1);
        inet_ntop(AF_INET, &agent->ip, ip_str, INET_ADDRSTRLEN);
        if (n < 0) {
            WARNING("read() failed: %m");
            continue;
        } else if (n == 0) {
            WARNING("unexpected EOF while reading a request from %s", ip_str);
            INFO("closing connections with %s", ip_str);
            close_connections_with_agent(agent, DOWN);
            continue;
        }
        buf.chars[n] = 0;
        if (parse_request(buf.chars, request)) {
            WARNING("invalid request from %s", ip_str);
            respond(agent->fd1, 400, &buf);
            break;
        }
        switch (request->method) {
        case PING:
            respond(agent->fd1, 200, &buf);
            break;
        case NTFY:
            respond(agent->fd1, 200, &buf);
            INFO("closing connections with %s", ip_str);
            close_connections_with_agent(agent, SUSPENDED);
            break;
        default:
            respond(agent->fd1, 501, &buf);
            break;
        }
    }
}

static void close_connection(int fd)
{
    INFO("closing the connection (fd=%d)", fd);
    if (close(fd))
        WARNING("can't close the connection (fd=%d): %m", fd);
}

static void close_connections_with_agent(struct agent *agent, int state)
{
    lock_agent(agent);
    close_connection(agent->fd1);
    close_connection(agent->fd2);
    agent->fd1 = -1;
    agent->fd2 = -1;
    agent->state = state;
    unlock_agent(agent);
}
