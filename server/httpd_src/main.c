#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>

#include <getopt.h>

#include "ajax_handler.h"
#include "daemon.h"
#include "logger.h"
#include "protocol.h"
#include "mongoose.h"

#define LOGGER_PREFIX "[main] "

// Whenever you add more command-line options, update MAX_OPTIONS accordingly
#define MAX_OPTIONS 5

#define DEFAULT_DOCUMENT_ROOT "/var/lib/nitch-httpd"
#define DEFAULT_LISTENING_PORTS "8080"
#define DEFAULT_ERROR_LOG_FILE "/var/log/nitch-httpd.err"
#define DEFAULT_PID_FILE "/var/run/nitch-httpd.pid"
#define DEFAULT_SOCKET "/var/run/nitch-sleepd.sock"

static void get_commandline_options(int argc, char **argv,
                                    const char **mg_options,
                                    const char **pid_file,
                                    const char **sock_file);
static void display_help_and_exit(void);
static void cleanup(void);
static void sigterm(int signum);

static void exit_if_not_root(const char *progname);
static void init(const char *pid_file);
static void start(const char **mg_options);

static void *event_handler(enum mg_event,
                           struct mg_connection *,
                           const struct mg_request_info *);

static const char *pid_file;   // filename of the pid file
static const char *sock_file;  // filename for the sleep proxy's socket to connect

int main(int argc, char **argv) {
    const char *mg_options[MAX_OPTIONS * 2 + 1];

    exit_if_not_root(program_invocation_short_name);
    get_commandline_options(argc, argv, mg_options, &pid_file, &sock_file);
    daemonize(program_invocation_short_name);
    init(pid_file);
    start(mg_options);
    return 0;
}

static void get_commandline_options(int argc, char **argv,
                                    const char **mg_options,
                                    const char **pid_file,
                                    const char **sock_file)
{
    static const char *short_options = "s:p:d:l:e:h";
    static struct option long_options[] = {
        {"document-root", required_argument, 0, 'd'},
        {"listening-ports", required_argument, 0, 'l'},
        {"error-log-file", required_argument, 0, 'e'},
        {"pid-file", required_argument, 0, 'p'},
        {"socket", required_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    const char *document_root = DEFAULT_DOCUMENT_ROOT;
    const char *listening_ports = DEFAULT_LISTENING_PORTS;
    const char *error_log_file = DEFAULT_ERROR_LOG_FILE;
    int c, i;

    *pid_file = DEFAULT_PID_FILE;
    *sock_file = DEFAULT_SOCKET;

    // Get arguments
    while (1) {
        int opt_index;
        c = getopt_long(argc, argv, short_options, long_options, &opt_index);
        if (c == -1)  // end of the options
            break;
        switch (c) {
        case 'd': document_root = optarg; break;
        case 'l': listening_ports = optarg; break;
        case 'e': error_log_file = optarg; break;
        case 'p': *pid_file = optarg; break;
        case 's': *sock_file = optarg; break;
        case 'h': display_help_and_exit(); break;
        case '?':
            // getopt_long already printed an error message
            exit(1);
        default:
            abort();
        }
    }

    // Set options for mongoose
    i = 0;
    mg_options[i++] = "document_root";
    mg_options[i++] = document_root;
    mg_options[i++] = "listening_ports";
    mg_options[i++] = listening_ports;
    mg_options[i++] = "error_log_file";
    mg_options[i++] = error_log_file;
    mg_options[i++] = NULL;
}

static void display_help_and_exit()
{
    printf("Usage: %s [option]...\n"
        "\n"
        "Options:\n"
        "  -d, --document-root=DIRECTORY  set the document root\n"
        "                                   (defaults to %s)\n"
        "  -l, --listening-ports=PORTS    set the listening ports\n"
        "                                   (defaults to %s)\n"
        "  -e, --error-log-file=FILE      set the error log file\n"
        "                                   (defaults to %s)\n"
        "  -p, --pid-file=FILE            set the PID file\n"
        "                                   (defaults to %s)\n"
        "  -s, --socket=FILE              locate nitch-sleepd's socket file\n"
        "                                   (defaults to %s)\n"
        "  -h, --help                     display this help and exit\n"
        "\n",
        program_invocation_short_name, DEFAULT_DOCUMENT_ROOT,
        DEFAULT_LISTENING_PORTS, DEFAULT_ERROR_LOG_FILE, DEFAULT_PID_FILE,
        DEFAULT_SOCKET);
    exit(0);
}

static void cleanup()
{
    if (unlink(pid_file) && errno != ENOENT)
        ERROR("can't unlink %s: %m", pid_file);
    INFO("exits");
}

static void sigterm(int unused)
{
    INFO("got SIGTERM");
    exit(2);
    unused = unused;
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
        WARNING("can't write PID file to %s: %m", pid_file);
    if (atexit(cleanup))
        WARNING("atexit() failed: %m");
    if (register_signal_handler(SIGTERM, sigterm))
        WARNING("can't catch SIGTERM: %m");
}

static void start(const char **mg_options)
{
    struct mg_context *ctx;

    ajax_set_socket_file(sock_file);
    ctx = mg_start(&event_handler, mg_options);
    if (ctx == NULL) {
        ERROR("can't start mongoose: mg_start() returned NULL");
        exit(2);
    }
    while (1)
        sleep(3600);  // mongoose will do all the jobs
    mg_stop(ctx);
    ERROR("start(): should not reach here");
}

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info)
{
    const char *method = request_info->request_method;
    const char *uri = request_info->uri;

    if (event != MG_NEW_REQUEST)
        return NULL;
    else if (strcmp(method, "GET") != 0)
        return NULL;
    else if (strcmp(uri, "/ajax/resume") == 0)
        ajax_resume(conn, request_info);
    else if (strcmp(uri, "/ajax/suspend") == 0)
        ajax_suspend(conn, request_info);
    else if (strcmp(uri, "/ajax/devicelist") == 0)
        ajax_device_list(conn, request_info);
    else
        return NULL;
    return "processed";
}
