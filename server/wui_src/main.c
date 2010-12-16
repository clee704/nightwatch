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

#define MAX_OPTIONS 4
#define DEFAULT_SOCKET "/var/run/nitch-proxyd.sock"
#define DEFAULT_PID_FILE "/var/run/nitch-httpd.pid"
#define DEFAULT_DOCUMENT_ROOT "/var/lib/nitch-httpd"
#define DEFAULT_LISTENING_PORTS "8080"
#define DEFAULT_ERROR_LOG_FILE "/var/log/nitch-httpd.err"

const char *sock_file;  // filename for the proxy's socket to connect
const char *pid_file;   // filename of the pid file

static const char *ajax_reply_start =
    "HTTP/1.1 200 OK\r\n"
    "Cache: no-cache\r\n"
    "Content-Type: application/json\r\n"
    "\r\n";

static const char *ajax_error_start =
    "HTTP/1.1 409 Conflict\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n";

static void
get_commandline_options(int argc, char **argv, const char **mg_options,
                        const char **sock_file, const char **pid_file);

static void
display_help_and_exit(void);

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

static int
connect_to(const char *sock_file);

/**
 * Get the value of the specified variable in the query string.
 */
static void
get_qsvar(const struct mg_request_info *, const char *name, char *buffer,
          size_t max_len);

int
main(int argc, char **argv) {
    struct mg_context *ctx;
    const char *mg_options[MAX_OPTIONS * 2 + 1];

    if (geteuid() != 0) {
        fprintf(stderr, "must be run as root or setuid root");
        exit(1);
    }

    // Parse the command line options
    get_commandline_options(argc, argv, mg_options, &sock_file, &pid_file);

    // Make the process a daemon
    daemonize(program_invocation_short_name);

    if (write_pid(pid_file) < 0)
        syslog(LOG_WARNING, "can't write PID file to %s: %s", pid_file,
            strerror(errno));
    if (setuid(getuid()) < 0)
        syslog(LOG_WARNING, "can't drop the root privileges: %s",
            strerror(errno));
    if (register_signal_handler(SIGTERM, sigterm))
        syslog(LOG_WARNING, "can't catch SIGTERM: %s", strerror(errno));

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
get_commandline_options(int argc, char **argv, const char **mg_options,
                        const char **sock_file, const char **pid_file)
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

    *sock_file = DEFAULT_SOCKET;
    *pid_file = DEFAULT_PID_FILE;

    // Get arguments
    while (1) {
        int opt_index;
        c = getopt_long(argc, argv, short_options, long_options, &opt_index);
        if (c == -1)  // end of the options
            break;
        switch (c) {
        case 's': *sock_file = optarg; break;
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

static void
sigterm(int signum)
{
    (void) signum;  // unused
    syslog(LOG_INFO, "got SIGTERM; exiting");
    if (unlink(pid_file) < 0)
        syslog(LOG_ERR, "can't unlink %s: %s", pid_file, strerror(errno));
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
    char device_id[24];  // In fact, 17 is just enough for MAC address
    int sock;

    get_qsvar(request_info, "deviceId", device_id, sizeof(device_id));
    if (strlen(device_id) == 0) {
        mg_printf(conn, "%s", ajax_error_start);
        mg_printf(conn, "%s", "deviceId must be specified");
        return;
    }

    sock = connect_to(sock_file);
    if (sock < 0) {
        syslog(LOG_WARNING, "can't connect to %s: %s", sock_file,
            strerror(errno));
        return;
    }
    //
    // TODO remove and implement ajax calls
    //
    // echo test
    //{
    //    static char buffer[256];
    //    int nbytes;

    //    if (write(sock, "hello", 5) < 0)
    //        syslog(LOG_WARNING, "can't write: %s", strerror(errno));
    //    nbytes = read(sock, buffer, 256);
    //    buffer[nbytes] = 0;
    //    syslog(LOG_INFO, "message from the proxy: %s", buffer);
    //}
    if (close(sock) < 0)
        syslog(LOG_WARNING, "can't close the socket: %s", strerror(errno));

    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "%s", "{\"success\": true, \"message\": \"ok\"}");
}

static void
ajax_suspend(const char *sock_file, struct mg_connection *conn,
             const struct mg_request_info *request_info)
{
    char device_id[24];  // In fact, 17 is just enough for MAC address
    int sock;

    get_qsvar(request_info, "deviceId", device_id, sizeof(device_id));
    if (strlen(device_id) == 0) {
        mg_printf(conn, "%s", ajax_error_start);
        mg_printf(conn, "%s", "deviceId must be specified");
        return;
    }

    sock = connect_to(sock_file);
    if (sock < 0) {
        syslog(LOG_WARNING, "can't connect to %s: %s", sock_file,
            strerror(errno));
        return;
    }
    //
    // TODO implement ajax calls
    //
    if (close(sock) < 0)
        syslog(LOG_WARNING, "can't close the socket: %s", strerror(errno));

    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "%s", "{\"success\": true, \"message\": \"ok\"}");
}

static void
ajax_device_list(const char *sock_file, struct mg_connection *conn,
                 const struct mg_request_info *unused)
{
    int sock;
    (void) unused;  // Suppress warning

    sock = connect_to(sock_file);
    if (sock < 0) {
        syslog(LOG_WARNING, "can't connect to %s: %s", sock_file,
            strerror(errno));
        return;
    }
    //
    // TODO implement ajax calls
    //
    if (close(sock) < 0)
        syslog(LOG_WARNING, "can't close the socket: %s", strerror(errno));

    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "%s", "[]");
}

static void
get_qsvar(const struct mg_request_info *request_info,
          const char *name, char *buffer, size_t max_len)
{
    const char *qs = request_info->query_string;
    mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, buffer, max_len);
}

static int
connect_to(const char *sock_file)
{
    int sock, size;
    struct sockaddr_un addr;
    size_t addr_len;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;

    addr.sun_family = AF_UNIX;
    size = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_file);
    if (size < 0 || (int) sizeof(addr.sun_path) < size)
        return -1;
    addr_len = offsetof(struct sockaddr_un, sun_path) + (size_t) size;

    if (connect(sock, (struct sockaddr *) &addr, addr_len) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}
