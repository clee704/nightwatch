#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <error.h>

#include "daemon.h"
#include "send_magic_packet.h"

#define DEFAULT_SOCKET "/var/run/nitch-proxyd.sock"
#define DEFAULT_PID_FILE "/var/run/nitch-proxyd.pid"

#define QLEN 10

int sock;  // file descriptor for the Unix domain socket for the web UI
const char *sock_file;  // filename for the Unix domain socket for the web UI
const char *pid_file;   // filename of the pid file

//
// TODO communicate with the agent
//

// Make a Unix domain socket to communicate with the web UI
static int
bind_and_listen(const char *sock_file, struct sockaddr_un *, size_t *);

// Communicate with the web UI
static void
handle_connections(int sock, struct sockaddr_un *, size_t *);

static void
sigterm(int signo);

int
main()
{
    struct sockaddr_un addr;
    size_t addr_len;

    if (geteuid() != 0) {
        fprintf(stderr, "must be run as root or setuid root");
        exit(1);
    }

    //
    // TODO getopt
    //
    sock_file = DEFAULT_SOCKET;
    pid_file = DEFAULT_PID_FILE;

    // Make the process a daemon
    daemonize(program_invocation_short_name);

    sock = bind_and_listen(sock_file, &addr, &addr_len);

    if (write_pid(pid_file) < 0)
        syslog(LOG_WARNING, "can't write PID file to %s: %m", pid_file);
    if (setuid(getuid()) < 0)
        syslog(LOG_WARNING, "can't drop the root privileges: %m");
    if (register_signal_handler(SIGTERM, sigterm) < 0)
        syslog(LOG_WARNING, "can't catch SIGTERM: %m");
    handle_connections(sock, &addr, &addr_len);

    return 0;
}

static int
bind_and_listen(const char *sock_file, struct sockaddr_un *addr, size_t *addr_len)
{
    int sock, n;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        syslog(LOG_ERR, "can't create a socket: %m");
        exit(1);
    }

    if (unlink(sock_file) < 0 && errno != ENOENT) {
        syslog(LOG_ERR, "can't unlink %s: %m", sock_file);
        exit(1);
    }
    addr->sun_family = AF_UNIX;
    n = snprintf(addr->sun_path, sizeof(addr->sun_path), "%s", sock_file);
    if (n < 0 || (int) sizeof(addr->sun_path) < n) {
        syslog(LOG_ERR, "socket path too long");
        exit(1);
    }
    *addr_len = offsetof(struct sockaddr_un, sun_path) + (size_t) n;

    if (bind(sock, (struct sockaddr *) addr, *addr_len) < 0) {
        syslog(LOG_ERR, "can't bind the socket %s: %m", sock_file);
        exit(1);
    }
    if (listen(sock, QLEN) < 0) {
        syslog(LOG_ERR, "can't listen on the socket %s: %m", sock_file);
        exit(1);
    }

    return sock;
}

static void
handle_connections(int sock, struct sockaddr_un *addr, size_t *addr_len)
{
    static char buffer[4096];
    int conn, n;

    while (1) {
        conn = accept(sock, (struct sockaddr *) addr, addr_len);
        if (conn < 0) {
            syslog(LOG_WARNING, "can't accept: %m");
            continue;  // ...?
        }
        while ((n = read(conn, buffer, sizeof(buffer))) > 0) {
            //
            // TODO impl
            //
            // simply echo for now
            if (write(conn, buffer, n) != n) {
                syslog(LOG_WARNING, "can't write: %m");
                break;
            }
        }
        if (close(conn) < 0)
            syslog(LOG_WARNING, "can't close the connection: %m");
    }
}

static void
sigterm(int signum)
{
    (void) signum;  // unused
    syslog(LOG_INFO, "got SIGTERM; exiting");
    if (close(sock) < 0)
        syslog(LOG_ERR, "can't close the socket: %m");
    if (unlink(sock_file) < 0)
        syslog(LOG_ERR, "can't unlink %s: %m", sock_file);
    if (unlink(pid_file) < 0)
        syslog(LOG_ERR, "can't unlink %s: %m", pid_file);
    exit(2);
}
