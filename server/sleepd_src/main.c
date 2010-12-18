#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include "agent.h"
#include "agent_handler.h"
#include "ui_handler.h"
#include "packet_monitor.h"
#include "daemon.h"

#define DEFAULT_PID_FILE "/var/run/nitch-sleepd.pid"
#define DEFAULT_SOCK_FILE "/var/run/nitch-sleepd.sock"
#define DEFAULT_PORT 4444
#define MAX_AGENTS 32

static void get_commandline_options(int argc, char **argv,
                                    const char **, const char **, int *);
static void display_help_and_exit(void);
static void cleanup(void);
static void sigterm(int signum);

static void exit_if_not_root(const char *progname);
static void init(const char *pid_file);
static void start(int port, const char *sock_file);

static const char *pid_file;
static const char *sock_file;

int main(int argc, char **argv)
{
    int port;

    exit_if_not_root(program_invocation_short_name);
    get_commandline_options(argc, argv, &pid_file, &sock_file, &port);
    daemonize(program_invocation_short_name);
    init(pid_file);
    start(port, sock_file);
    return 0;
}

static void get_commandline_options(int argc, char **argv,
                                    const char **pid_file,
                                    const char **sock_file,
                                    int *port)
{
    // TODO impl
    *pid_file = DEFAULT_PID_FILE;
    *sock_file = DEFAULT_SOCK_FILE;
    *port = DEFAULT_PORT;
}

static void display_help_and_exit()
{
    // TODO impl
}

static void cleanup()
{
    if (unlink(pid_file) && errno != ENOENT)
        syslog(LOG_ERR, "can't unlink %s: %m", pid_file);
    if (unlink(sock_file) && errno != ENOENT)
        syslog(LOG_ERR, "can't unlink %s: %m", sock_file);
}

static void sigterm(int unused)
{
    (void) unused;
    syslog(LOG_INFO, "got SIGTERM; exiting");
    cleanup();
    exit(2);
}

static void exit_if_not_root(const char *progname)
{
    if (geteuid() != 0) {
        fprintf(stderr, "%s: must be run as root or setuid root\n", progname);
        exit(1);
    }
}

static void init(const char *pid_file)
{
    if (write_pid(pid_file))
        syslog(LOG_WARNING, "can't write PID file to %s: %m", pid_file);
    if (setuid(getuid()))
        syslog(LOG_WARNING, "can't drop the root privileges: %m");
    if (atexit(cleanup))
        syslog(LOG_WARNING, "atexit() failed: %m");
    if (register_signal_handler(SIGTERM, sigterm))
        syslog(LOG_WARNING, "can't catch SIGTERM: %m");
}

static void start(int port, const char *sock_file)
{
    struct agent_list *list;
    pthread_t tid1, tid2, tid3;

    list = new_agent_list(MAX_AGENTS);
    if (start_agent_handler(&tid1, list, port)
            || start_ui_handler(&tid2, list, sock_file)
            || start_packet_monitor(&tid3, list))
        exit(2);
    if (pthread_join(tid1, NULL)
            || pthread_join(tid2, NULL)
            || pthread_join(tid3, NULL))
        syslog(LOG_WARNING, "can't join a thread: %m");
    syslog(LOG_WARNING, "all thread terminated");  // should not happen
}
