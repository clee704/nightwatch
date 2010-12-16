#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <error.h>
#include <getopt.h>
#include <netinet/ether.h>

#include "agent.h"
#include "send_magic_packet.h"
#include "daemon.h"
#include "protocol.h"

#define ETHER_ADDRS_EQUAL(a, b) \
    (strncmp((char *) (a).ether_addr_octet, (char *) (b).ether_addr_octet, \
             ETH_ALEN) == 0)

// Whenever you add more command-line options, update MAX_OPTIONS accordingly
#define MAX_OPTIONS 4

// Options for the agents
#define DEFAULT_AGN_IF "eth0"
#define DEFAULT_AGN_PORT 4444

// Option for the web UI
#define DEFAULT_WUI_SOCKET "/var/run/nitch-sleepd.sock"

// etc.
#define DEFAULT_PID_FILE "/var/run/nitch-sleepd.pid"

// Options that are not configurable by the user
#define AGN_MAX_AGENTS 64
#define AGN_BACKLOG 32
#define WUI_BACKLOG 8

const char *pid_file;
const char *wui_sock_file;

int agn_sock;
int wui_sock;

pthread_mutex_t agn_mutex = PTHREAD_MUTEX_INITIALIZER;

// TODO add a hash table (find library or make it yourself)
struct agent *agents[AGN_MAX_AGENTS];

int agn_alloc[AGN_MAX_AGENTS];  // 0 for free, 1 for allocated

//
// TODO implement SYN forwarding
//

static void
get_commandline_options(int argc, char **argv);

static void
display_help_and_exit(void);

static void
cleanup(void);

static void
sigterm(int signum);

static int
agn_listen(const char *ifname, int port);

static void *
agn_accept(void *fd);

static void *
agn_read(void *conn);

static int
agn_find_empty_slot(void);

static void
agn_return_slot(int index);

static void *
agn_monitor_network(void *);

static int
wui_listen(const char *sock_file);

static void  // not thread-safe
wui_accept_and_read(int fd);

static void
request(int fd, int method, char *char_buf, struct request *req_buf);

static void
respond(int fd, int status, char *char_buf, struct response *resp_buf);

static void  // not to be called without mutex locked
resume(struct agent *);

static void  // not to be called without mutex locked
suspend(struct agent *);

int
main()
{
    const char *agn_if;
    int agn_port;
    pthread_t tid;
    int err;

    if (geteuid() != 0) {
        fprintf(stderr, "%s: must be run as root or setuid root\n",
                        program_invocation_short_name);
        exit(1);
    }

    //
    // TODO use getopt
    //
    agn_if = DEFAULT_AGN_IF;
    agn_port = DEFAULT_AGN_PORT;
    wui_sock_file = DEFAULT_WUI_SOCKET;
    pid_file = DEFAULT_PID_FILE;

    // Make the process a daemon
    daemonize(program_invocation_short_name);

    if (write_pid(pid_file))
        syslog(LOG_WARNING, "can't write PID file to %s: %m", pid_file);
    if (setuid(getuid()))
        syslog(LOG_WARNING, "can't drop the root privileges: %m");
    if (atexit(cleanup))
        syslog(LOG_WARNING, "atexit() failed: %m");
    if (register_signal_handler(SIGTERM, sigterm))
        syslog(LOG_WARNING, "can't catch SIGTERM: %m");

    // Handle connections from the agents
    agn_sock = agn_listen(agn_if, agn_port);
    if (agn_sock < 0) {
        syslog(LOG_ERR, "can't listen on %s port %d: %m", agn_if, agn_port);
        exit(2);
    }
    err = pthread_create(&tid, NULL, agn_accept, (void *) agn_sock);
    if (err) {
        syslog(LOG_ERR, "can't create a thread: %s", strerror(err));
        exit(2);
    }

    // Monitor the network for SYN packets to suspended agents
    err = pthread_create(&tid, NULL, agn_monitor_network, 0);
    if (err) {
        syslog(LOG_ERR, "can't create a thread: %s", strerror(err));
        exit(2);
    }

    // The main thread deals with the web UI
    wui_sock = wui_listen(wui_sock_file);
    if (wui_sock < 0) {
        syslog(LOG_ERR, "can't listen on %s: %m", wui_sock_file);
        exit(2);
    }
    wui_accept_and_read(wui_sock);

    return 0;
}

static void
get_commandline_options(int argc, char **argv)
{
    //
    // TODO
    //
}

static void
display_help_and_exit()
{
    //
    // TODO
    //
}

static void
cleanup()
{
    // close() makes an error "Bad file descriptor" (I don't know why)
    // Since it is exiting,
    // I guess I don't have to close the socket explicitly...
    //
    //if (wui_sock >= 0 && close(wui_sock))
    //    syslog(LOG_ERR, "can't close the socket: %m");
    if (unlink(pid_file) && errno != ENOENT)
        syslog(LOG_ERR, "can't unlink %s: %m", pid_file);
    if (unlink(wui_sock_file) && errno != ENOENT)
        syslog(LOG_ERR, "can't unlink %s: %m", wui_sock_file);
}

static void
sigterm(int unused)
{
    (void) unused;
    syslog(LOG_INFO, "got SIGTERM; exiting");
    cleanup();
    exit(2);
}

static int
agn_listen(const char *ifname, int port)
{
    struct sockaddr_in addr;
    int sock;

    //
    // TODO bind on the specified ifname
    //
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        close(sock);
        return -1;
    }
    if (listen(sock, AGN_BACKLOG)) {
        close(sock);
        return -1;
    }
    return sock;
}

static void *
agn_accept(void *fd)
{
    struct sockaddr_in addr;
    socklen_t addr_len;
    int conn, index, err;
    pthread_t tid;
    struct agent *a = NULL;

    while (1) {
        addr_len = sizeof(addr);
        conn = accept((int) fd, (struct sockaddr *) &addr, &addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "(agn) can't accept: %m");
            continue;
        }
        syslog(LOG_INFO, "(agn) new connection at fd=%d", conn);
        index = agn_find_empty_slot();
        if (index < 0) {
            syslog(LOG_NOTICE, "(agn) maximum agents limit (%d) exceeded",
                               AGN_MAX_AGENTS);
            goto cleanup;
        }
        a = agents[index] = (struct agent *) calloc(1, sizeof(struct agent));
        if (a == NULL) {
            syslog(LOG_ERR, "(agn) can't allocate memory: %m");
            goto cleanup;
        }
        a->fd = (int) conn;
        a->state = UP;
        a->ip = addr.sin_addr;
        a->monitored_since = time(NULL);
        a->total_uptime = 0;
        a->total_downtime = 0;
        a->sleep_time = 0;
        err = pthread_create(&tid, NULL, agn_read, (void *) index);
        if (err) {
            syslog(LOG_WARNING, "(agn) can't create a thread: %s",
                                strerror(err));
            goto cleanup;
        }
        continue;
    cleanup:
        if (a != NULL)
            free(a);
        if (index >= 0)
            agn_return_slot(index);
        syslog(LOG_WARNING, "(agn) closing the new connection");
        if (close(conn))
            syslog(LOG_WARNING, "(agn) can't close the connection: %m");
    }
    return (void *) 0;
}

static void *
agn_read(void *index)
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
            syslog(LOG_NOTICE, "(agn) invalid request");
            respond(a->fd, 400, resp_buf, &resp);
            break;
        }
        switch (req.method) {
        case INFO:
            if (!req.has_data) {
                syslog(LOG_NOTICE, "(agn) INFO without data");
                respond(a->fd, 400, resp_buf, &resp);
                goto cleanup;
            }
            c = strchr(req.data, '\n');

            // no newline or more than one newline = malformed data
            if (c == NULL || strchr(c + 1, '\n') != NULL) {
                syslog(LOG_NOTICE, "(agn) malformed INFO data");
                respond(a->fd, 400, resp_buf, &resp);
                goto cleanup;
            }

            *c = 0;
            strcpy(a->hostname, req.data);
            if (ether_aton_r(c + 1, &a->mac) == NULL) {
                syslog(LOG_NOTICE, "(agn) malformed INFO data");
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
    syslog(LOG_NOTICE, "(agn) closing the connection");
    if (close(a->fd))
        syslog(LOG_WARNING, "(agn) can't close the connection: %m");
    free(a);
    agn_return_slot((int) index);
    return (void *) 0;
}

static int
agn_find_empty_slot()
{
    int i, ret = -1;

    pthread_mutex_lock(&agn_mutex);
    for (i = 0; i < AGN_MAX_AGENTS; ++i)
        if (agn_alloc[i] == 0) {
            agn_alloc[i] = 1;
            ret = i;
            break;
        }
    pthread_mutex_unlock(&agn_mutex);
    return ret;
}

static void
agn_return_slot(int index)
{
    pthread_mutex_lock(&agn_mutex);
    agn_alloc[index] = 0;
    pthread_mutex_unlock(&agn_mutex);
}

static void *
agn_monitor_network(void *unused)
{
    (void) unused;
    //
    // TODO+2
    // monitor the network
    // if the IP address of a SYN packet matches,
    //   resume the agent
    //   forward the SYN packet to the agent
    //
    return (void *) 0;
}

static int
wui_listen(const char *sock_file)
{
    struct sockaddr_un addr;
    socklen_t addr_len;
    int sock, n;

    if (unlink(sock_file) && errno != ENOENT)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    n = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_file);
    if (n < 0 || (int) sizeof(addr.sun_path) < n)
        return -1;
    addr_len = offsetof(struct sockaddr_un, sun_path) + (socklen_t) n;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;
    if (bind(sock, (struct sockaddr *) &addr, addr_len)) {
        close(sock);
        return -1;
    }
    if (listen(sock, WUI_BACKLOG)) {
        close(sock);
        return -1;
    }
    return sock;
}

static void
wui_accept_and_read(int fd)
{
    static char req_buf[MAX_REQUEST_LEN];
    static char resp_buf[MAX_RESPONSE_LEN];
    static struct request req;
    static struct response resp;

    struct sockaddr_un addr;
    socklen_t addr_len;
    int conn, n, i;

    struct ether_addr mac;
    struct agent *a;

    while (1) {
        addr_len = sizeof(addr);
        conn = accept(fd, (struct sockaddr *) &addr, &addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "(wui) can't accept: %m");
            continue;
        }
        while ((n = read(conn, req_buf, sizeof(req_buf) - 1)) > 0) {
            req_buf[n] = 0;
            if (parse_request(req_buf, &req)) {
                syslog(LOG_NOTICE, "(wui) invalid request");
                respond(conn, 400, resp_buf, &resp);
                break;
            }
            switch (req.method) {
            case RSUM:
            case SUSP:
                if (ether_aton_r(req.uri, &mac) == NULL) {
                    syslog(LOG_NOTICE, "(wui) invalid URI: %s", req.uri);
                    respond(conn, 400, resp_buf, &resp);
                    break;
                }
                pthread_mutex_lock(&agn_mutex);

                // Find the agent
                for (i = 0; i < AGN_MAX_AGENTS; ++i) {
                    if (agn_alloc[i] == 0)
                        continue;
                    if (ETHER_ADDRS_EQUAL((a = agents[i])->mac, mac))
                        break;
                }
                
                // ... not found
                if (i == AGN_MAX_AGENTS) {
                    respond(conn, 404, resp_buf, &resp);
                    pthread_mutex_unlock(&agn_mutex);
                    break;
                }

                if (req.method == RSUM)
                    if (a->state == UP || a->state == RESUMING)
                        respond(conn, 409, resp_buf, &resp);
                    else {
                        resume(a);
                        respond(conn, 200, resp_buf, &resp);
                    }
                else  // req.method == SUSP
                    if (a->state == SUSPENDED)
                        respond(conn, 409, resp_buf, &resp);
                    else {
                        request(a->fd, SUSP, req_buf, &req);
                        respond(conn, 200, resp_buf, &resp);
                    }

                pthread_mutex_unlock(&agn_mutex);
                break; 
            case GETA:
                //
                // TODO+0
                //
                break;
            default:
                respond(conn, 501, resp_buf, &resp);
                break;
            }
        }
        if (n == -1)
            syslog(LOG_WARNING, "(wui) can't read: %m");
        if (close(conn))
            syslog(LOG_WARNING, "(wui) can't close the connection: %m");
    }
}

static void
request(int fd, int method, char *char_buf, struct request *req_buf)
{
    int len;

    req_buf->method = method;
    req_buf->has_uri = 0;
    req_buf->has_data = 0;
    len = serialize_request(req_buf, char_buf);
    if (len < 0)
        syslog(LOG_WARNING, "serialize_request() failed");
    if (write(fd, char_buf, len) != len)
        syslog(LOG_WARNING, "can't write: %m");
}

static void
respond(int fd, int status, char *char_buf, struct response *resp_buf)
{
    int len;
    const char *msg = "";

    switch (status) {
    case 200: msg = "OK"; break;
    case 400: msg = "Bad Request"; break;
    case 404: msg = "Not Found"; break;
    case 409: msg = "Conflict"; break;
    case 501: msg = "Not Implemented"; break;
    default: abort(); break;
    }
    resp_buf->status = status;
    strcpy(resp_buf->message, msg);
    resp_buf->has_data = 0;
    len = serialize_response(resp_buf, char_buf);
    if (len < 0)
        syslog(LOG_WARNING, "serialize_response() failed");
    if (write(fd, char_buf, len) != len)
        syslog(LOG_WARNING, "can't write: %m");
}

static void
resume(struct agent *a)
{
    //
    // TODO+
    //
    syslog(LOG_DEBUG, "resume(): a->fd=%d", a->fd);
}

static void
suspend(struct agent *a)
{
    //
    // TODO+
    //
    syslog(LOG_DEBUG, "suspend(): a->fd=%d", a->fd);
}
