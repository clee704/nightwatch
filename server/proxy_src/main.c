#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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
// backlog sizes for listen()
#define AGN_MAX_AGENTS 256
#define AGN_BACKLOG 100
#define WUI_BACKLOG 10

const char *pid_file;
const char *wui_sock_file;

int agn_sock;
int wui_sock;

struct agent *agents[AGN_MAX_AGENTS];

static void
get_commandline_options(int argc, char **argv);

static void
display_help_and_exit(void);

static void
cleanup(void);

static void
sigterm(int signum);

static int
agn_bind_and_listen(const char *ifname, int port);

static void *
agn_handle_connections(void *sock);

static int
wui_bind_and_listen(const char *sock_file);

static void
wui_handle_connections(int sock);

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
    agn_sock = agn_bind_and_listen(agn_if, agn_port);
    if (agn_sock < 0) {
        syslog(LOG_ERR, "can't bind and listen on %s port %d: %m", agn_if, 
            agn_port);
        exit(2);
    }
    err = pthread_create(&tid, NULL, agn_handle_connections,
        (void *) agn_sock);
    if (err != 0) {
        syslog(LOG_ERR, "can't create a thread: %s", strerror(err));
        exit(2);
    }

    // The main thread deals with the web UI
    wui_sock = wui_bind_and_listen(wui_sock_file);
    if (wui_sock < 0) {
        syslog(LOG_ERR, "can't bind and listen on %s: %m", wui_sock_file);
        exit(2);
    }
    wui_handle_connections(wui_sock);

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
sigterm(int signum)
{
    (void) signum;  // unused
    syslog(LOG_INFO, "got SIGTERM; exiting");
    cleanup();
    exit(2);
}

static int
agn_bind_and_listen(const char *ifname, int port)
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
agn_handle_connections(void *sock)
{
    struct sockaddr_in addr;
    socklen_t addr_len;
    int conn;

    while (1) {
        addr_len = sizeof(addr);
        conn = accept((int) sock, (struct sockaddr *) &addr, &addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "(agn) can't accept: %m");
            continue;
        }
        //
        // TODO manage agents
        //
    }
    return (void *) 0;
}

static int
wui_bind_and_listen(const char *sock_file)
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
wui_handle_connections(int sock)
{
    static char buffer[4096];
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
            // TODO impl
            //
            // simply echo for now
            if (write(conn, buffer, n) != n) {
                syslog(LOG_WARNING, "(wui) can't write: %m");
                break;
            }
        }
        if (close(conn) < 0)
            syslog(LOG_WARNING, "(wui) can't close the connection: %m");
    }
}
