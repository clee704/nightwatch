#include <errno.h>
#include <stddef.h>
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
#include "network.h"
#include "protocol.h"
#include "message_exchange.h"

#define BACKLOG_SIZE 16

static void *accept_ui(void *agent_list);
static void handle_requests(int fd, struct agent_list *);
static int resume_agent(struct agent_list *, const struct ether_addr *,
                        struct message_buffer *);
static int suspend_agent(struct agent_list *, const struct ether_addr *,
                         struct message_buffer *);

static int sock;

int start_ui_handler(pthread_t *tid,
                     struct agent_list *list,
                     const char *sock_file)
{
    struct sockaddr_un addr;
    socklen_t addr_len;
    int n;

    if (unlink(sock_file) && errno != ENOENT) {
        syslog(LOG_ERR, "[ui_handler] can't unlink %s: %m", sock_file);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = PF_UNIX;
    n = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_file);
    if (n < 0 || (int) sizeof(addr.sun_path) < n) {
        syslog(LOG_ERR,
               "[ui_handler]"
               " snprintf failed: %m (sock_file might be too long)");
        return -1;
    }
    addr_len = offsetof(struct sockaddr_un, sun_path) + (socklen_t) n;
    sock = init_server(SOCK_STREAM,
                       (struct sockaddr *) &addr, addr_len,
                       BACKLOG_SIZE);
    if (sock < 0) {
        syslog(LOG_ERR, "[ui_handler] can't listen on %s: %m", sock_file);
        return -1;
    }
    if (pthread_create(tid, NULL, accept_ui, (void *) list)) {
        syslog(LOG_ERR, "[ui_handler] can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *accept_ui(void *agent_list)
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
        syslog(LOG_INFO, "[ui_handler] new UI connected");
        handle_requests(conn, (struct agent_list *) agent_list);
        syslog(LOG_INFO, "[ui_handler] closing the connection");
        if (close(conn))
            syslog(LOG_WARNING, "[ui_handler] can't close the connection: %m");
    }
    // should not reach here
    syslog(LOG_WARNING, "[ui_handler] thread terminated");
    return (void *) 0;
}

static void handle_requests(int fd, struct agent_list *list)
{
    struct message_buffer buf;
    struct ether_addr mac;
    int n, status;

    while ((n = read(fd, buf.chars, sizeof(buf.chars) - 1)) > 0) {
        buf.chars[n] = 0;
        if (parse_request(buf.chars, &buf.u.request)) {
            syslog(LOG_NOTICE, "[ui_handler] invalid request");
            respond(fd, 400, &buf);
            continue;
        }
        switch (buf.u.request.method) {
        case RSUM:
        case SUSP:
            if (ether_aton_r(buf.u.request.uri, &mac) == NULL) {
                syslog(LOG_NOTICE,
                       "[ui_handler] URI not a MAC address: %s",
                       buf.u.request.uri);
                respond(fd, 404, &buf);
                break;
            }
            if (buf.u.request.method == RSUM)
                status = resume_agent(list, &mac, &buf);
            else // req.method == SUSP 
                status = suspend_agent(list, &mac, &buf);
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
    else if (n == 0)
        syslog(LOG_WARNING, "[ui_handler] unexpected EOF");
}

static int resume_agent(struct agent_list *list, const struct ether_addr *mac,
                        struct message_buffer *buf)
{
    struct agent *agent;

    agent = find_agent_by_mac(list, mac);
    if (agent == NULL)
        return 404;
    if (agent->state == UP || agent->state == RESUMING)
        return 409;
    // resume!
    return 200;
}

static int suspend_agent(struct agent_list *list, const struct ether_addr *mac,
                         struct message_buffer *buf)
{
    struct agent *agent;
    int n;

    agent = find_agent_by_mac(list, mac);
    if (agent == NULL)
        return 404;
    if (agent->state == SUSPENDED)
        return 409;
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
    if (parse_response(buf->chars, &buf->u.response)) {
        syslog(LOG_NOTICE, "[ui_handler] invalid response");
        return 500;
    }
    switch (buf->u.response.status) {
    case 200:
        return 200;
    default:
        syslog(LOG_NOTICE, "[ui_handler] got %d", buf->u.response.status);
        return 500;
    }
}
