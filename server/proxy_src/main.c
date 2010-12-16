#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

#include "agent.h"
#include "daemon.h"
#include "send_magic_packet.h"

// Whenever you add more command-line options, update MAX_OPTIONS accordingly
#define MAX_OPTIONS 4

// Options for the agents
#define DEFAULT_AGN_IF "eth0"
#define DEFAULT_AGN_PORT 4444

// Option for the web UI
#define DEFAULT_WUI_SOCKET "/var/run/nitch-proxyd.sock"

// etc.
#define DEFAULT_PID_FILE "/var/run/nitch-proxyd.pid"

// Options that are not configurable by the user
#define AGN_MAX_AGENTS 64
#define AGN_BACKLOG 100
#define WUI_BACKLOG 10

const char *pid_file;
const char *wui_sock_file;

int agn_sock;
int wui_sock;

pthread_mutex_t agn_mutex = PTHREAD_MUTEX_INITIALIZER;

struct agent *agents[AGN_MAX_AGENTS];

// TODO+ implemented it using hash table (find library or make it yourself)
// hash key: MAC address
void *agents_by_hash[AGN_MAX_AGENTS];

int agn_alloc[AGN_MAX_AGENTS];  // 0 for free, 1 for allocated

// Cache for TCP SYN packets to suspended agents
// TODO+ replace void * to the actual type
void *agn_syn_packets[AGN_MAX_AGENTS];

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
agn_accept(void *sock);

static void *
agn_new_connection(void *conn);

static int
agn_find_slot(void);

static void
agn_free_slot(int index);

static void *
agn_monitor_network(void *);

static int
wui_listen(const char *sock_file);

static void
wui_accept(int sock);

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

    if (write_pid(pid_file) < 0)
        syslog(LOG_WARNING, "can't write PID file to %s: %m", pid_file);
    if (setuid(getuid()) < 0)
        syslog(LOG_WARNING, "can't drop the root privileges: %m");
    if (atexit(cleanup))
        syslog(LOG_WARNING, "atexit() failed: %m");
    if (register_signal_handler(SIGTERM, sigterm) < 0)
        syslog(LOG_WARNING, "can't catch SIGTERM: %m");

    // Handle connections from the agents
    agn_sock = agn_listen(agn_if, agn_port);
    if (agn_sock < 0) {
        syslog(LOG_ERR, "can't listen on %s port %d: %m", agn_if, agn_port);
        exit(2);
    }
    err = pthread_create(&tid, NULL, agn_accept, (void *) agn_sock);
    if (err != 0) {
        syslog(LOG_ERR, "can't create a thread: %s", strerror(err));
        exit(2);
    }

    // Monitor the network for SYN packets to suspended agents
    err = pthread_create(&tid, NULL, agn_monitor_network, 0);
    if (err != 0) {
        syslog(LOG_ERR, "can't create a thread: %s", strerror(err));
        exit(2);
    }

    // The main thread deals with the web UI
    wui_sock = wui_listen(wui_sock_file);
    if (wui_sock < 0) {
        syslog(LOG_ERR, "can't listen on %s: %m", wui_sock_file);
        exit(2);
    }
    wui_accept(wui_sock);

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
    //if (wui_sock >= 0 && close(wui_sock) < 0)
    //    syslog(LOG_ERR, "can't close the socket: %m");
    if (unlink(pid_file) < 0 && errno != ENOENT)
        syslog(LOG_ERR, "can't unlink %s: %m", pid_file);
    if (unlink(wui_sock_file) < 0 && errno != ENOENT)
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
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    if (listen(sock, AGN_BACKLOG) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

static void *
agn_accept(void *sock)
{
    struct sockaddr_in addr;
    socklen_t addr_len;
    int conn, err;
    pthread_t tid;

    while (1) {
        addr_len = sizeof(addr);
        conn = accept((int) sock, (struct sockaddr *) &addr, &addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "(agn) can't accept: %m");
            continue;
        }
        err = pthread_create(&tid, NULL, agn_new_connection, (void *) conn);
        if (err != 0) {
            syslog(LOG_ERR, "(agn) can't create a thread: %s; "
                "closing the connection", strerror(err));
            if (close(conn) < 0)
                syslog(LOG_WARNING, "(agn) can't close the connection: %m");
        }
    }
    return (void *) 0;
}

static void *
agn_new_connection(void *conn)
{
    char buffer[128];
    struct agent *a;
    int index, n;

    index = agn_find_slot();
    if (index < 0) {
        syslog(LOG_ERR, "(agn) maximum agents limit (%d) exceeded; "
            "closing the new connection", AGN_MAX_AGENTS);
        goto end;
    }
    a = agents[index] = (struct agent *) malloc(sizeof(struct agent));
    if (a == NULL) {
        syslog(LOG_ERR, "(agn) can't allocate memory: %m");
        goto end;
    }
    a->fd = (int) conn;
    a->state = UP;
    a->monitored_since = time(NULL);
    a->total_uptime = 0;
    a->total_downtime = 0;
    a->sleep_time = 0;
    while ((n = read(a->fd, buffer, sizeof(buffer))) > 0) {
        //
        // TODO+
        // catch the sleep signal
        //
        // simply echo for now
        if (write(a->fd, buffer, n) != n) {
            syslog(LOG_WARNING, "(agn) can't write: %m");
            break;
        }
    }
end:
    if (a != NULL)
        free(a);
    agn_free_slot(index);
    if (close((int) conn) < 0)
        syslog(LOG_WARNING, "(agn) can't close the connection: %m");
    return (void *) 0;
}

static int
agn_find_slot()
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
agn_free_slot(int index)
{
    pthread_mutex_lock(&agn_mutex);
    agn_alloc[index] = 0;
    pthread_mutex_unlock(&agn_mutex);
}

static void *
agn_monitor_network(void *unsued)
{
    (void) unused;
    //
    // TODO+
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

    if (unlink(sock_file) < 0 && errno != ENOENT)
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
    if (bind(sock, (struct sockaddr *) &addr, addr_len) < 0) {
        close(sock);
        return -1;
    }
    if (listen(sock, WUI_BACKLOG) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

static void
wui_accept(int sock)
{
    static char buffer[256];
    struct sockaddr_un addr;
    socklen_t addr_len;
    int conn, n;

    while (1) {
        addr_len = sizeof(addr);
        conn = accept(sock, (struct sockaddr *) &addr, &addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "(wui) can't accept: %m");
            continue;
        }
        while ((n = read(conn, buffer, sizeof(buffer))) > 0) {
            //
            // TODO+
            // read the request from the web UI and respond accordingly;
            // requests:
            //   "RSUM <deviceId>\n": resume the device
            //   "SUSP <deviceId>\n": suspend the device
            //   "GETA\n": send the agent list to the web UI
            // responses (for simple methods, RSUM and SUSP):
            //   "200 OK\n"
            //   "404 Device Not Found\n"
            // response for GETA:
            //   "200 OK\n"
            //   "\n"
            //   "<hostname>, <ip-address>, <mac-address>, ...\n"
            //   ...
            //   "\n"  # empty line means the end of the list
            //
            // simply echo for now
            //
            if (write(conn, buffer, n) != n) {
                syslog(LOG_WARNING, "(wui) can't write: %m");
                break;
            }
        }
        if (close(conn) < 0)
            syslog(LOG_WARNING, "(wui) can't close the connection: %m");
    }
}
