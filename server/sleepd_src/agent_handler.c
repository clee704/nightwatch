#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <netinet/ether.h>

#include "agent_handler.h"
#include "agent.h"
#include "network.h"
#include "protocol.h"
#include "message_exchange.h"

#define BACKLOG_SIZE 64
#define MAX_AGENTS 32

struct in_conn {
    int fd;
    struct sockaddr_in addr;
};

static void *accept_agents(void *agent_list);
static void start_subhandler(struct in_conn *);
static void *handle_agent(void *conn_);
int read_info(int fd, struct message_buffer *, char **hostname, struct ether_addr *);
static void manage_agent(struct agent *, struct message_buffer *);
static void close_connection(int fd);

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
        syslog(LOG_ERR, "[agent_handler] can't listen on port %d: %m", port);
        return -1;
    }
    list = list_;
    if (pthread_create(tid, NULL, accept_agents, NULL)) {
        syslog(LOG_ERR, "[agent_handler] can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *accept_agents(void *unused)
{
    struct in_conn conn;
    socklen_t addr_len;

    while (1) {
        addr_len = sizeof(conn.addr);
        conn.fd = accept(sock, (struct sockaddr *) &conn.addr, &addr_len);
        if (conn.fd < 0) {
            syslog(LOG_WARNING, "[agent_handler] accept() failed: %m");
            continue;
        }
        syslog(LOG_INFO, "[agent_handler] new agent connected");
        start_subhandler(&conn);
    }

    // should not reach here
    syslog(LOG_WARNING, "[agent_handler] accept_agents() exits");
    unused = unused;
    return (void *) 0;
}

static void start_subhandler(struct in_conn *conn)
{
    pthread_t tid;

    if (list->size >= MAX_AGENTS)
        syslog(LOG_INFO,
               "[agent_handler] maximum number of agents (%d) exceeded",
               MAX_AGENTS);
    else if (pthread_create(&tid, NULL, handle_agent, (void *) conn))
        syslog(LOG_WARNING, "[agent_handler] can't create a thread: %m");
    else
        return;
    close_connection(conn->fd);
}

static void *handle_agent(void *conn_)
{
    struct in_conn *conn = (struct in_conn *) conn_;
    struct message_buffer buf;
    struct agent *agent;
    char *hostname;
    struct ether_addr mac;
    int status;
    struct sockaddr_in sa;

    status = read_info(conn->fd, &buf, &hostname, &mac);
    respond(conn->fd, status, &buf);
    if (status != 200) {
        syslog(LOG_WARNING, "[agent_handler] illegal agent");
        goto close;
    }
    agent = find_agent_by_mac(list, &mac);
    if (agent == NULL) {  // new agent
        agent = (struct agent *) malloc(sizeof(struct agent));
        if (agent == NULL) {
            syslog(LOG_WARNING, "[agent_handler] can't allocate memory: %m");
            goto close;
        }
        strcpy(agent->hostname, hostname);
        agent->ip = conn->addr.sin_addr;
        agent->mac = mac;
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
            syslog(LOG_WARNING,
                   "[agent_handler] can't connect to the agent: %m");
            goto free_agent;
        }
        if (pthread_mutex_init(&agent->mutex, NULL)) {
            syslog(LOG_WARNING, "[agent_handler] can't create a mutex: %m");
            goto free_agent;
        }
        if (add_new_agent(list, agent))
            goto destroy_mutex;
        manage_agent(agent, &buf);

        // should not reach here
        syslog(LOG_WARNING, "[agent_handler] manage_agent() exits");
        goto destroy_mutex;
    } else {
        char in_addr_str[INET_ADDRSTRLEN];

        lock_agent(agent);  // since the agent is in the list

        if (agent->state == UP) {
            syslog(LOG_WARNING,
                   "[agent_handler] multiple connections with one agent");
            unlock_agent(agent);
            goto close;
        }

        // If MAC addresses match but IP addresses doesn't,
        if (!IN_ADDR_EQ(agent->ip, conn->addr.sin_addr)) {
            struct agent *agent2 = find_agent_by_ip(list, &agent->ip);

            // If the new IP address is already in use
            if (agent2 != NULL) {
                inet_ntop(AF_INET, &agent->ip, in_addr_str, INET_ADDRSTRLEN);
                syslog(LOG_WARNING,
                       "[agent_handler] IP addresses collide: %s",
                       in_addr_str);
                unlock_agent(agent);
                goto close;
            }

            // Replace the old IP address with the new one
            agent->ip = conn->addr.sin_addr;
        }

        agent->fd1 = conn->fd;
        set_sockaddr_in(&sa, AF_INET, 4444, agent->ip.s_addr);
        agent->fd2 = connect_to(SOCK_STREAM,
                                (struct sockaddr *) &sa,
                                sizeof(struct sockaddr_in));
        agent->state = UP;

        unlock_agent(agent);
        goto end;  // since manage_agent() is running
    }
destroy_mutex:
    if (pthread_mutex_destroy(&agent->mutex))
        syslog(LOG_WARNING, "[agent_handler] can't destroy the mutex");
free_agent:
    free(agent);
close:
    close_connection(conn->fd);
end:
    return (void *) 0;
}

int read_info(int fd, struct message_buffer *buf,
              char **hostname, struct ether_addr *mac)
{
    struct request *request = &buf->u.request;
    char *chars = buf->chars;
    char *c;
    int n;

    n = read(fd, chars, sizeof(buf->chars));
    if (n < 0) {
        syslog(LOG_WARNING, "[agent_handler] read() failed: %m");
        return 500;
    } else if (n == 0) {
        syslog(LOG_WARNING, "[agent_handler] unexpected EOF");
        return 400;
    }
    chars[n] = 0;
    if (parse_request(chars, request)) {
        syslog(LOG_WARNING, "[agent_handler] invalid request");
        return 400;
    }
    if (request->method != INFO) {
        syslog(LOG_WARNING, "[agent_handler] unexpected method");
        return 418;
    }
    if (!request->has_data) {
        syslog(LOG_WARNING, "[agent_handler] got INFO with no data");
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
    syslog(LOG_WARNING, "[agent_handler] got INFO with malformed data");
    return 400;
}

static void manage_agent(struct agent *agent, struct message_buffer *buf)
{
    struct request *request = &buf->u.request;
    int n;

    while (1) {
        while (agent->state != UP || agent->fd1 < 0)
            sleep(1);
        n = read(agent->fd1, buf->chars, sizeof(buf->chars) - 1);
        if (n < 0) {
            syslog(LOG_WARNING, "[agent_handler] read() failed: %m");
            continue;
        } else if (n == 0) {
            syslog(LOG_WARNING, "[agent_handler] unexpected EOF");
            lock_agent(agent);
            close_connection(agent->fd1);
            close_connection(agent->fd2);
            agent->fd1 = -1;
            agent->fd2 = -1;
            agent->state = DOWN;
            unlock_agent(agent);
            continue;
        }
        buf->chars[n] = 0;
        if (parse_request(buf->chars, request)) {
            syslog(LOG_WARNING, "[agent_handler] invalid request");
            respond(agent->fd1, 400, buf);
            break;
        }
        switch (request->method) {
        case PING:
            break;
        case NTFY:
            lock_agent(agent);
            respond(agent->fd1, 200, buf);
            close_connection(agent->fd1);
            close_connection(agent->fd2);
            agent->fd1 = -1;
            agent->fd2 = -1;
            agent->state = SUSPENDED;
            unlock_agent(agent);
            break;
        default:
            respond(agent->fd1, 501, buf);
            break;
        }
    }
}

static void close_connection(int fd)
{
    syslog(LOG_INFO, "[agent_handler] closing the connection");
    if (close(fd))
        syslog(LOG_WARNING, "[agent_handler] can't close the connection: %m");
}
