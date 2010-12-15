#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <syslog.h>
#include <unistd.h>

#include <error.h>
#include <getopt.h>

#include "mongoose.h"
#include "ajax.h"
#include "daemon.h"

#define MAX_OPTIONS 4
#define DEFAULT_DOCUMENT_ROOT "/var/lib/nitch-httpd"
#define DEFAULT_LISTENING_PORTS "8080"
#define DEFAULT_ERROR_LOG_FILE "/var/log/nitch-httpd.err"

static void parse_options(int argc, char **argv, const char **mg_options);

static void display_help_and_exit();

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info);

int main(int argc, char **argv) {
    struct mg_context *ctx;
    const char *mg_options[MAX_OPTIONS * 2 + 1];

    if (geteuid() != 0)
        error(1, errno, "must be run as root or setuid root");
    parse_options(argc, argv, mg_options);
    daemonize(program_invocation_name);
    if (write_pid("/var/run", program_invocation_short_name) < 0)
        syslog(LOG_WARNING, "can't write PID file");
    if (setuid(getuid()) < 0)
        syslog(LOG_WARNING, "can't drop the root privileges");
    ctx = mg_start(&event_handler, mg_options);
    if (ctx == NULL) {
        syslog(LOG_ERR, "can't start mongoose");
        exit(2);
    }
    while (1)
        sleep(3600);  // just sleep, mongoose will do all the jobs
    mg_stop(ctx);
    return 0;
}

static void parse_options(int argc, char **argv, const char **mg_options)
{
    static struct option long_options[] = {
        {"document-root", required_argument, 0, 'd'},
        {"listening-ports", required_argument, 0, 'p'},
        {"error-log-file", required_argument, 0, 'e'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    const char *document_root = DEFAULT_DOCUMENT_ROOT;
    const char *listening_ports = DEFAULT_LISTENING_PORTS;
    const char *error_log_file = DEFAULT_ERROR_LOG_FILE;
    int c, i;

    while (1) {
        int option_index;
        c = getopt_long(argc, argv, "d:p:e:h", long_options, &option_index);
        if (c == -1)  // end of the options
            break;
        switch (c) {
        case 'd':
            document_root = optarg;
            break;
        case 'p':
            listening_ports = optarg;
            break;
        case 'e':
            error_log_file = optarg;
            break;
        case 'h':
            display_help_and_exit();
            break;
        case '?':
            // getopt_long already printed an error message
            break;
        default:
            abort();
        }
    }
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
        "  -p, --listening-ports=PORTS    set the listening ports\n"
        "                                   (defaults to %s)\n"
        "  -e, --error-log-file=FILE      set the error log file\n"
        "                                   (defaults to %s)\n"
        "  -h, --help                     display this help and exit\n",
        program_invocation_short_name, DEFAULT_DOCUMENT_ROOT,
        DEFAULT_LISTENING_PORTS, DEFAULT_ERROR_LOG_FILE);
    exit(0);
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
    else if (strcmp(uri, "/ajax/devicelist") == 0)
        ajax_device_list(conn, request_info);
    else
        return NULL;
    return "processed";
}
