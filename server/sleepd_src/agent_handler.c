#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include "agent_handler.h"
#include "agent.h"
#include "network.h"
#include "protocol.h"
#include "message_exchange.h"

#define BACKLOG_SIZE 64
#define MAX_AGENTS 32

static void *accept_agents(void *agent_list);
/*static void *read(void *conn);
static int find_empty_slot(void);
static void return_slot(int index);
static void *monitor_network(void *);*/

static int sock;

int start_agent_handler(pthread_t *tid, struct agent_list *list, int port)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    sock = init_server(SOCK_STREAM,
                       (struct sockaddr *) &addr, sizeof(addr),
                       BACKLOG_SIZE);
    if (sock < 0) {
        syslog(LOG_ERR, "[agent_handler] can't listen on port %d: %m", port);
        return -1;
    }
    if (pthread_create(tid, NULL, accept_agents, (void *) list)) {
        syslog(LOG_ERR, "[agent_handler] can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *accept_agents(void *agent_list)
{
    struct agent_list *list = (struct agent_list *) agent_list;
    /*struct sockaddr_in addr;
    socklen_t addr_len;
    int conn, index, err;
    pthread_t tid;
    struct agent *a = NULL;

    while (1) {
        addr_len = sizeof(addr);
        conn = accept((int) fd, (struct sockaddr *) &addr, &addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "[agent_handler] accept() failed: %m");
            continue;
        }
        syslog(LOG_INFO, "[agent_handler] new connection at fd=%d", conn);
        index = find_empty_slot();
        if (index < 0) {
            syslog(LOG_NOTICE, "[agent_handler] maximum agents limit (%d) exceeded",
                               AGN_MAX_AGENTS);
            goto cleanup;
        }
        a = agents[index] = (struct agent *) calloc(1, sizeof(struct agent));
        if (a == NULL) {
            syslog(LOG_ERR, "[agent_handler] can't allocate memory: %m");
            goto cleanup;
        }
        a->fd = (int) conn;
        a->state = UP;
        a->ip = addr.sin_addr;
        a->monitored_since = time(NULL);
        a->total_uptime = 0;
        a->total_downtime = 0;
        a->sleep_time = 0;
        err = pthread_create(&tid, NULL, read, (void *) index);
        if (err) {
            syslog(LOG_WARNING, "[agent_handler] can't create a thread: %s",
                                strerror(err));
            goto cleanup;
        }
        continue;
    cleanup:
        if (a != NULL)
            free(a);
        if (index >= 0)
            return_slot(index);
        syslog(LOG_WARNING, "[agent_handler] closing the new connection");
        if (close(conn))
            syslog(LOG_WARNING, "[agent_handler] can't close the connection: %m");
    }*/
    return (void *) 0;
}

/*static void *read(void *index)
{
    struct agent *a = agents[(int) index];

    char req_buf[MAX_REQUEST_LEN];
    char resp_buf[MAX_RESPONSE_LEN];
    struct request req;
    struct response resp;

    char *c;
    int n;

    while ((n = read(a->fd, req_buf, sizeof(req_buf) - 1)) > 0) {
        req_buf[n] = 0;
        if (parse_request(req_buf, &req)) {
            syslog(LOG_NOTICE, "[agent_handler] invalid request");
            respond(a->fd, 400, resp_buf, &resp);
            break;
        }
        switch (req.method) {
        case INFO:
            if (!req.has_data) {
                syslog(LOG_NOTICE, "[agent_handler] INFO without data");
                respond(a->fd, 400, resp_buf, &resp);
                goto cleanup;
            }
            c = strchr(req.data, '\n');

            // no newline or more than one newline = malformed data
            if (c == NULL || strchr(c + 1, '\n') != NULL) {
                syslog(LOG_NOTICE, "[agent_handler] malformed INFO data");
                respond(a->fd, 400, resp_buf, &resp);
                goto cleanup;
            }

            *c = 0;
            strcpy(a->hostname, req.data);
            if (ether_aton_r(c + 1, &a->mac) == NULL) {
                syslog(LOG_NOTICE, "[agent_handler] malformed INFO data");
                respond(a->fd, 400, resp_buf, &resp);
                goto cleanup;
            }
            respond(a->fd, 200, resp_buf, &resp);
            break;
        case NTFY:
            respond(a->fd, 200, resp_buf, &resp);
            suspend(a);
            return (void *) 0;
        default:
            respond(a->fd, 501, resp_buf, &resp);
            goto cleanup;
        }
    }
cleanup:
    syslog(LOG_NOTICE, "[agent_handler] closing the connection");
    if (close(a->fd))
        syslog(LOG_WARNING, "[agent_handler] can't close the connection: %m");
    free(a);
    return_slot((int) index);
    return (void *) 0;
}

static int find_empty_slot()
{
    int i, ret = -1;

    pthread_mutex_lock(&mutex);
    for (i = 0; i < AGN_MAX_AGENTS; ++i)
        if (alloc[i] == 0) {
            alloc[i] = 1;
            ret = i;
            break;
        }
    pthread_mutex_unlock(&mutex);
    return ret;
}

static void return_slot(int index)
{
    pthread_mutex_lock(&mutex);
    alloc[index] = 0;
    pthread_mutex_unlock(&mutex);
}*/
