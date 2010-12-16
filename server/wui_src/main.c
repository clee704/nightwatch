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
#include "daemon.h"
#include "protocol.h"

// Whenever you add more command-line options, update MAX_OPTIONS accordingly
#define MAX_OPTIONS 5

#define DEFAULT_DOCUMENT_ROOT "/var/lib/nitch-httpd"
#define DEFAULT_LISTENING_PORTS "8080"
#define DEFAULT_ERROR_LOG_FILE "/var/log/nitch-httpd.err"
#define DEFAULT_PID_FILE "/var/run/nitch-httpd.pid"
#define DEFAULT_SOCKET "/var/run/nitch-proxyd.sock"

const char *pid_file;   // filename of the pid file
const char *sock_file;  // filename for the proxy's socket to connect

static const char * const ajax_reply_start =
    "HTTP/1.1 200 OK\r\n"
    "Cache: no-cache\r\n"
    "Content-Type: application/json\r\n"
    "\r\n";

static void
get_commandline_options(int argc, char **argv, const char **mg_options,
                        const char **pid_file, const char **sock_file);

static void
display_help_and_exit(void);

static void
cleanup(void);

static void
sigterm(int signum);

static void *
event_handler(enum mg_event, struct mg_connection *,
              const struct mg_request_info *);

/**
 * Resume the specified device. The request URI is /ajax/resume?deviceId=<id>,
 * where <id> is replaced by the actual value.
 *
 * The deviceId field in the query string defines the device to be resumed.
 * Currently it is the MAC address of the device. The web server makes a call
 * to the sleep proxy server to resume the device. Then the server returns a
 * result, which is returned to the web client as a JSON object.
 *
 * The returned object has two properties: "success" and "message". "success"
 * can be either true or false. "message" is "ok" for success, and
 * "no such device" or "already up or resuming" for failure.
 */
static void
ajax_resume(const char *sock_file, struct mg_connection *,
            const struct mg_request_info *);

/**
 * Suspend (to memory) the specified device. The request URI is
 * /ajax/suspend?deviceId=<id>, where <id> is replaced by the actual value.
 *
 * The detail is similar to ajax_resume(). The messages for failure are
 * "no such device" and "already suspended".
 */
static void
ajax_suspend(const char *sock_file, struct mg_connection *,
             const struct mg_request_info *);

/**
 * Return the list of the devices that are connected to the sleep proxy server.
 * The request URI is /ajax/devicelist.
 *
 * There is no field in the query string. The web server makes a call to the
 * sleep proxy server to get the list. The list is returned to the web client
 * as a JSON object.
 *
 * The returned object is an array of objects, each for one device. The
 * following is an example of the result.
 *
 * [
 *     {
 *         "hostname": "mimosa",
 *         "ip": "10.0.0.2",
 *         "mac": "00:11:22:33:44:55",
 *         "state": "up",
 *         "monitoredSince": 1267369283000,
 *         "totalUptime": 3517003,
 *         "sleepTime": 2300910,
 *         "totalDowntime": 36713
 *     }
 * ]
 *
 * Hostname can be an empty string. Total uptime includes sleep time. Times
 * are represented in milliseconds. Currently there are 6 states for a device:
 * "up", "resuming", "suspended", and "down". "down" means the device is not
 * responding, not it is actually down.
 *
 * Note that a device may have many network interfaces but only the interface
 * that communicates with the sleep proxy server is relevant.
 */
static void
ajax_device_list(const char *sock_file, struct mg_connection *,
                 const struct mg_request_info *);

static void
ajax_simple_method(const char *sock_file, struct mg_connection *,
                   const struct mg_request_info *, const char *method);

static void
ajax_print_response(struct mg_connection *, const char *message);

static int
connect_to(const char *sock_file);

/**
 * Get the value of the specified variable in the query string.
 */
static void
mg_get_qsvar(const struct mg_request_info *, const char *name, char *buffer,
             size_t max_len);

int
main(int argc, char **argv) {
    struct mg_context *ctx;
    const char *mg_options[MAX_OPTIONS * 2 + 1];

    if (geteuid() != 0) {
        fprintf(stderr, "%s: must be run as root or setuid root\n",
                        program_invocation_short_name);
        exit(1);
    }

    // Parse the command line options
    get_commandline_options(argc, argv, mg_options, &pid_file, &sock_file);

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

    // Start mongoose
    ctx = mg_start(&event_handler, mg_options);
    if (ctx == NULL) {
        syslog(LOG_ERR, "can't start mongoose; exiting");
        exit(2);  
    }
    while (1)
        sleep(3600);  // mongoose will do all the jobs
    mg_stop(ctx);

    return 0;
}

static void
get_commandline_options(int argc, char **argv, const char **mg_options,
                        const char **pid_file, const char **sock_file)
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

static void
display_help_and_exit()
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
        "  -s, --socket=FILE              locate nitch-proxyd's socket file\n"
        "                                   (defaults to %s)\n"
        "  -h, --help                     display this help and exit\n"
        "\n",
        program_invocation_short_name, DEFAULT_DOCUMENT_ROOT,
        DEFAULT_LISTENING_PORTS, DEFAULT_ERROR_LOG_FILE, DEFAULT_PID_FILE,
        DEFAULT_SOCKET);
    exit(0);
}

static void
cleanup()
{
    if (unlink(pid_file) && errno != ENOENT)
        syslog(LOG_ERR, "can't unlink %s: %m", pid_file);
}

static void
sigterm(int unused)
{
    (void) unused;
    syslog(LOG_INFO, "got SIGTERM; exiting");
    cleanup();
    exit(2);
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
        ajax_resume(sock_file, conn, request_info);
    else if (strcmp(uri, "/ajax/suspend") == 0)
        ajax_suspend(sock_file, conn, request_info);
    else if (strcmp(uri, "/ajax/devicelist") == 0)
        ajax_device_list(sock_file, conn, request_info);
    else
        return NULL;
    return "processed";
}

static void
ajax_resume(const char *sock_file, struct mg_connection *conn,
            const struct mg_request_info *request_info)
{
    ajax_simple_method(sock_file, conn, request_info, "RSUM");
}

static void
ajax_suspend(const char *sock_file, struct mg_connection *conn,
             const struct mg_request_info *request_info)
{
    ajax_simple_method(sock_file, conn, request_info, "SUSP");
}

static void
ajax_device_list(const char *sock_file, struct mg_connection *conn,
                 const struct mg_request_info *unused)
{
    int sock;
    (void) unused;  // Suppress warning

    // Connect to the proxy
    sock = connect_to(sock_file);
    if (sock < 0) {
        syslog(LOG_WARNING, "can't connect to %s: %m", sock_file);
        syslog(LOG_WARNING, "make sure nitch-proxyd is running");
        ajax_print_response(conn, "internal server error");
        return;
    }

    //
    // TODO+
    // request by "GETA\n"
    // parse the response into a JSON object
    //

    // Close the connection to the proxy
    if (close(sock))
        syslog(LOG_WARNING, "can't close the socket: %m");

    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "%s", "[]");
}

static void
ajax_simple_method(const char *sock_file, struct mg_connection *conn,
                   const struct mg_request_info *request_info,
                   const char *method)
{
    char buffer[MAX_REQUEST_LEN] = {0};
    char device_id[MAX_URI_LEN] = {0};
    int sock, n;

    // Get the argument
    mg_get_qsvar(request_info, "deviceId", device_id, sizeof(device_id));
    if (strlen(device_id) == 0) {
        ajax_print_response(conn, "deviceId must be specified");
        return;
    }

    // Connect to the proxy
    sock = connect_to(sock_file);
    if (sock < 0) {
        syslog(LOG_WARNING, "can't connect to %s: %m", sock_file);
        syslog(LOG_WARNING, "make sure nitch-proxyd is running");
        ajax_print_response(conn, "internal server error");
        return;
    }

    // Make a request
    n = snprintf(buffer, sizeof(buffer), "%s %s\n", method, device_id);
    if (n < 0 || (int) sizeof(buffer) < n) {
        ajax_print_response(conn, "internal server error");
        return;
    }

    // Send the request to the proxy
    if (write(sock, buffer, n) != n) {
        syslog(LOG_WARNING, "can't write: %m");
        ajax_print_response(conn, "internal server error");
        return;
    }

    // Read the response from the proxy
    n = read(sock, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        if (n == 0)
            syslog(LOG_NOTICE, "unexpected EOF from the proxy");
        else
            syslog(LOG_WARNING, "can't read: %m");
        ajax_print_response(conn, "internal server error");
        return;
    }
    buffer[n] = 0;

    // Close the connection to the proxy
    if (close(sock))
        syslog(LOG_WARNING, "can't close the socket: %m");

    //
    // TODO+ parse the response into a JSON object
    //
    syslog(LOG_DEBUG, "%s", buffer);
    ajax_print_response(conn, "ok");
}

static void
ajax_print_response(struct mg_connection *conn, const char *message)
{
    int success;

    success = strncmp(message, "ok", 2) == 0;
    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "{\"success\": %s, \"message\": \"%s\"}",
              success ? "true" : "false", message);
}

static void
mg_get_qsvar(const struct mg_request_info *request_info,
            const char *name, char *buffer, size_t max_len)
{
    const char *qs = request_info->query_string;
    mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, buffer, max_len);
}

static int
connect_to(const char *sock_file)
{
    int sock, n;
    struct sockaddr_un addr;
    socklen_t addr_len;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    n = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_file);
    if (n < 0 || (int) sizeof(addr.sun_path) < n)
        return -1;
    addr_len = offsetof(struct sockaddr_un, sun_path) + (socklen_t) n;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;
    if (connect(sock, (struct sockaddr *) &addr, addr_len)) {
        close(sock);
        return -1;
    }
    return sock;
}
