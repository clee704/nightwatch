#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ui_handler.h"
#include "agent.h"
#include "send_magic_packet.h"
#include "resume_agent.h"
#include "network.h"
#include "protocol.h"
#include "message_exchange.h"

#define BACKLOG_SIZE 16

static void *accept_ui(void *);
static void handle_requests(int fd);
static int resume(const struct ether_addr *);
static int suspend(const struct ether_addr *, struct message_buffer *);

static int sock;
static struct agent_list *list;

int start_ui_handler(pthread_t *tid,
                     struct agent_list *list_,
                     const char *sock_file)
{
    struct sockaddr_un addr;
    int addr_len;

    if (unlink(sock_file) && errno != ENOENT) {
        syslog(LOG_ERR, "[ui_handler] can't unlink %s: %m", sock_file);
        return -1;
    }
    addr_len = set_sockaddr_un(&addr, sock_file);
    if (addr_len < 0) {
        syslog(LOG_ERR, "[ui_handler] set_sockaddr_un() failed: %m");
        return -1;
    }
    sock = init_server(SOCK_STREAM,
                       (struct sockaddr *) &addr, (socklen_t) addr_len,
                       BACKLOG_SIZE);
    if (sock < 0) {
        syslog(LOG_ERR, "[ui_handler] can't listen on %s: %m", sock_file);
        return -1;
    }
    list = list_;
    if (pthread_create(tid, NULL, accept_ui, NULL)) {
        syslog(LOG_ERR, "[ui_handler] can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *accept_ui(void *unused)
{
    struct sockaddr_un addr;
    socklen_t addr_len;
    int conn;

    while (1) {
        addr_len = sizeof(addr);
        conn = accept(sock, (struct sockaddr *) &addr, &addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "[ui_handler] accept() failed: %m");
            continue;
        }
        handle_requests(conn);
        if (close(conn))
            syslog(LOG_WARNING, "[ui_handler] can't close the connection: %m");
    }

    // should not reach here
    syslog(LOG_WARNING, "[ui_handler] accept_ui() exits");
    unused = unused;
    return (void *) 0;
}

static void handle_requests(int fd)
{
    struct message_buffer buf;
    struct request *request = &buf.u.request;
    struct ether_addr mac;
    int n, status;

    while ((n = read(fd, buf.chars, sizeof(buf.chars) - 1)) > 0) {
        buf.chars[n] = 0;
        if (parse_request(buf.chars, request)) {
            syslog(LOG_WARNING, "[ui_handler] invalid request");
            respond(fd, 400, &buf);
            continue;
        }
        switch (request->method) {
        case RSUM:
        case SUSP:
            if (ether_aton_r(request->uri, &mac) == NULL) {
                syslog(LOG_WARNING,
                       "[ui_handler] URI not a MAC address: %s", request->uri);
                respond(fd, 404, &buf);
                break;
            }
            if (request->method == RSUM)
                status = resume(&mac);
            else // req.method == SUSP 
                status = suspend(&mac, &buf);
            respond(fd, status, &buf);
            break; 
        case GETA:
            // TODO impl
            respond(fd, 200, &buf);
            break;
        default:
            respond(fd, 501, &buf);
            break;
        }
    }
    if (n == -1)
        syslog(LOG_WARNING, "[ui_handler] read() failed: %m");
}

static int resume(const struct ether_addr *mac)
{
    struct agent *agent = find_agent_by_mac(list, mac);

    if (agent == NULL)
        return 404;
    if (agent->state == UP || agent->state == RESUMING)
        return 409;
    if (resume_agent(agent)) {
        syslog(LOG_WARNING, "[ui_handler] can't resume the agent");
        return 500;
    }
    return 200;
}

static int suspend(const struct ether_addr *mac, struct message_buffer *buf)
{
    struct response *response = &buf->u.response;
    struct agent *agent = find_agent_by_mac(list, mac);
    int n;

    if (agent == NULL)
        return 404;
    if (agent->state == SUSPENDED)
        return 409;
    if (agent->fd2 < 0)  // no connection to the agent
        return 500;
    request(agent->fd2, SUSP, buf);
    n =  read(agent->fd2, buf->chars, sizeof(buf->chars) - 1);
    if (n <= 0) {
        if (n < 0)
            syslog(LOG_WARNING, "[ui_handler] read() failed: %m");
        else
            syslog(LOG_WARNING, "[ui_handler] unexpected EOF");
        return 500;
    }
    buf->chars[n] = 0;
    if (parse_response(buf->chars, response)) {
        syslog(LOG_WARNING, "[ui_handler] invalid response");
        return 500;
    }
    switch (response->status) {
    case 200:
        return 200;
    default:
        syslog(LOG_WARNING, "[ui_handler] got %d", response->status);
        return 500;
    }
}
