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
#include "network.h"
#include "protocol.h"
#include "message_exchange.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

#define BACKLOG_SIZE 16

static void *accept_ui(void *agent_list);
static void handle_requests(int fd, struct agent_list *);

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
    char buffer[max(MAX_REQUEST_LEN, MAX_RESPONSE_LEN)];
    struct request req;
    struct response resp;
    struct ether_addr mac;
    int n;

    while ((n = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = 0;
        if (parse_request(buffer, &req)) {
            syslog(LOG_NOTICE, "[ui_handler] invalid request");
            respond(fd, 400, &resp, buffer);
            continue;
        }
        switch (req.method) {
        case RSUM:
        case SUSP:
            if (ether_aton_r(req.uri, &mac) == NULL) {
                syslog(LOG_NOTICE,
                       "[ui_handler] invalid URI: %s (not a MAC address)",
                       req.uri);
                respond(fd, 400, &resp, buffer);
                break;
            }
            if (req.method == RSUM)
                resume_agent(list, &mac);
            else // req.method == SUSP
                suspend_agent(list, &mac);
            break; 
        case GETA:
            // TODO impl
            respond(fd, 200, &resp, buffer);
            break;
        default:
            respond(fd, 501, &resp, buffer);
            break;
        }
    }
    if (n == -1)
        syslog(LOG_WARNING, "[ui_handler] can't read: %m");
}
