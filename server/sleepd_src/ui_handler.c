#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ui_handler.h"
#include "agent.h"
#include "send_magic_packet.h"
#include "resume_agent.h"
#include "logger.h"
#include "network.h"
#include "protocol.h"
#include "message_exchange.h"

#define LOGGER_PREFIX "[ui_handler] "
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
        ERROR("can't unlink %s: %m", sock_file);
        return -1;
    }
    addr_len = set_sockaddr_un(&addr, sock_file);
    if (addr_len < 0) {
        ERROR("set_sockaddr_un() failed: %m");
        return -1;
    }
    sock = init_server(SOCK_STREAM,
                       (struct sockaddr *) &addr, (socklen_t) addr_len,
                       BACKLOG_SIZE);
    if (sock < 0) {
        ERROR("can't listen on %s: %m", sock_file);
        return -1;
    }
    list = list_;
    if (pthread_create(tid, NULL, accept_ui, NULL)) {
        ERROR("can't create a thread: %m");
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
            WARNING("accept() failed: %m");
            continue;
        }
        handle_requests(conn);
        if (close(conn))
            WARNING("can't close the connection: %m");
    }

    // should not reach here
    WARNING("accept_ui() exits (it should not happen)");
    unused = unused;
    return NULL;
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
            WARNING("invalid request");
            send_respond(fd, 400, NULL, &buf);
            continue;
        }
        switch (request->method) {
        case RSUM:
        case SUSP:
            if (ether_aton_r(request->uri, &mac) == NULL) {
                WARNING("URI not a MAC address: %s", request->uri);
                send_respond(fd, 404, NULL, &buf);
                break;
            }
            if (request->method == RSUM)
                status = resume(&mac);
            else // req.method == SUSP 
                status = suspend(&mac, &buf);
            send_respond(fd, status, NULL, &buf);
            break; 
        case GETA:
            // TODO impl
            send_respond(fd, 200, NULL, &buf);
            break;
        default:
            send_respond(fd, 501, NULL, &buf);
            break;
        }
    }
    if (n == -1)
        WARNING("read() failed: %m");
}

static int resume(const struct ether_addr *mac)
{
    struct agent *agent = find_agent_by_mac(list, mac);

    if (agent == NULL)
        return 404;
    if (agent->state == UP || agent->state == RESUMING)
        return 409;
    if (resume_agent(agent)) {
        WARNING("can't resume the agent");
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
    send_request(agent->fd2, SUSP, NULL, NULL, buf);
    n = read(agent->fd2, buf->chars, sizeof(buf->chars) - 1);
    if (n <= 0) {
        if (n < 0)
            WARNING("read() failed: %m");
        else
            WARNING("unexpected EOF while waiting for a response");
        return 500;
    }
    buf->chars[n] = 0;
    if (parse_response(buf->chars, response)) {
        WARNING("invalid response");
        return 500;
    }
    switch (response->status) {
    case 200:
        return 200;
    default:
        WARNING("got %d", response->status);
        return 500;
    }
}
