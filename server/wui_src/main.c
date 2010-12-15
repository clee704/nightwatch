#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <error.h>
#include <getopt.h>

#include "mongoose.h"
#include "ajax.h"
#include "daemon.h"

#define MAX_OPTIONS 4
#define DEFAULT_SOCKET "/var/run/nitch-proxyd.sock"
#define DEFAULT_PID_FILE "/var/run/nitch-httpd.pid"
#define DEFAULT_DOCUMENT_ROOT "/var/lib/nitch-httpd"
#define DEFAULT_LISTENING_PORTS "8080"
#define DEFAULT_ERROR_LOG_FILE "/var/log/nitch-httpd.err"

int sock;  // file descriptor for the socket
const char *pid_file;  // filename of the pid file

static void
get_options(int argc, char **argv, const char **mg_options,
              const char **sock_name, const char **pid_file);

static void
display_help_and_exit(void);

static int
connect_to(const char *name, struct sockaddr_un *, size_t *);

static void *
event_handler(enum mg_event, struct mg_connection *,
               const struct mg_request_info *);

static void
sigterm(int signum);

int
main(int argc, char **argv) {
    struct mg_context *ctx;
    const char *mg_options[MAX_OPTIONS * 2 + 1];
    const char *sock_name;
    struct sockaddr_un addr;
    size_t addr_len;

    if (geteuid() != 0) {
        fprintf(stderr, "must be run as root or setuid root");
        exit(1);
    }

    // Parse the command line options
    get_options(argc, argv, mg_options, &sock_name, &pid_file);

    // Make the process a daemon
    daemonize(program_invocation_short_name);

    // Connect to the proxy server
    sock = connect_to(sock_name, &addr, &addr_len);

    if (write_pid(pid_file) < 0)
        syslog(LOG_WARNING, "can't write PID file to %s: %s", pid_file,
            strerror(errno));
    if (setuid(getuid()) < 0)
        syslog(LOG_WARNING, "can't drop the root privileges: %s",
            strerror(errno));
    if (register_signal_handler(SIGTERM, sigterm))
        syslog(LOG_WARNING, "can't catch SIGTERM: %s", strerror(errno));

    //
    // TODO remove and implement ajax calls
    //
    // echo test
    {
        static char buffer[256];
        int nbytes;

        if (write(sock, "hello", 5) < 0)
            syslog(LOG_WARNING, "can't write: %s", strerror(errno));
        nbytes = read(sock, buffer, 256);
        buffer[nbytes] = 0;
        syslog(LOG_INFO, "message from the proxy: %s", buffer);
    }

    // Start mongoose
    ctx = mg_start(&event_handler, mg_options);
    if (ctx == NULL) {
        syslog(LOG_ERR, "can't start mongoose");
        exit(2);  
    }
    while (1)
        sleep(3600);  // sleep, mongoose will do all the jobs
    mg_stop(ctx);

    return 0;
}

static void
get_options(int argc, char **argv, const char **mg_options,
              const char **sock_name, const char **pid_file)
{
    static const char *short_options = "s:p:d:l:e:h";
    static struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"pid-file", required_argument, 0, 'p'},
        {"document-root", required_argument, 0, 'd'},
        {"listening-ports", required_argument, 0, 'l'},
        {"error-log-file", required_argument, 0, 'e'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    const char *document_root = DEFAULT_DOCUMENT_ROOT;
    const char *listening_ports = DEFAULT_LISTENING_PORTS;
    const char *error_log_file = DEFAULT_ERROR_LOG_FILE;
    int c, i;

    *sock_name = DEFAULT_SOCKET;
    *pid_file = DEFAULT_PID_FILE;

    // Get arguments
    while (1) {
        int opt_index;
        c = getopt_long(argc, argv, short_options, long_options, &opt_index);
        if (c == -1)  // end of the options
            break;
        switch (c) {
        case 's': *sock_name = optarg; break;
        case 'p': *pid_file = optarg; break;
        case 'd': document_root = optarg; break;
        case 'l': listening_ports = optarg; break;
        case 'e': error_log_file = optarg; break;
        case 'h': display_help_and_exit(); break;
        case '?':
            // getopt_long already printed an error message
            break;
        default:
            abort();
        }
    }

    // Set options for mangoose
    i = 0;
    mg_options[i++] = "document_root";
    mg_options[i++] = document_root;
    mg_options[i++] = "listening_ports";
    mg_options[i++] = listening_ports;
    mg_options[i++] = "error_log_file";
    mg_options[i++] = error_log_file;
    mg_options[i++] = NULL;
}

static void
display_help_and_exit()
{
    printf("Usage: %s [option]...\n"
        "\n"
        "Options:\n"
        "  -s, --socket=FILE              locate nitch-proxyd's socket file\n"
        "                                   (defaults to %s)\n"
        "  -p, --pid-file=FILE            set the PID file\n"
        "                                   (defaults to %s)\n"
        "  -d, --document-root=DIRECTORY  set the document root\n"
        "                                   (defaults to %s)\n"
        "  -l, --listening-ports=PORTS    set the listening ports\n"
        "                                   (defaults to %s)\n"
        "  -e, --error-log-file=FILE      set the error log file\n"
        "                                   (defaults to %s)\n"
        "  -h, --help                     display this help and exit\n"
        "\n",
        program_invocation_short_name, DEFAULT_SOCKET, DEFAULT_PID_FILE,
        DEFAULT_DOCUMENT_ROOT, DEFAULT_LISTENING_PORTS,
        DEFAULT_ERROR_LOG_FILE);
    exit(0);
}

static int
connect_to(const char *name, struct sockaddr_un *addr, size_t *addr_len)
{
    int sock, size;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        syslog(LOG_ERR, "can't create a socket: %s", strerror(errno));
        exit(1);
    }

    addr->sun_family = AF_UNIX;
    size = snprintf(addr->sun_path, sizeof(addr->sun_path), "%s", name);
    if (size < 0 || (int) sizeof(addr->sun_path) < size) {
        syslog(LOG_ERR, "socket name too long");
        exit(1);
    }
    *addr_len = offsetof(struct sockaddr_un, sun_path) + (size_t) size;

    if (connect(sock, (struct sockaddr *) addr, *addr_len) < 0) {
        syslog(LOG_ERR, "can't connect to the socket %s: %s", name,
            strerror(errno));
        exit(1);
    }

    return sock;
}

static void *
event_handler(enum mg_event event, struct mg_connection *conn,
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

static void
sigterm(int signum)
{
    (void) signum;  // unused
    syslog(LOG_INFO, "got SIGTERM; exiting");
    if (close(sock) < 0)
        syslog(LOG_ERR, "can't close the socket: %s", strerror(errno));
    if (unlink(pid_file) < 0)
        syslog(LOG_ERR, "can't unlink %s: %s", pid_file, strerror(errno));
    exit(2);
}
