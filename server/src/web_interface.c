#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mongoose.h"

#define PROGNAME "nitch_server_httpd"

//
// TODO we need to get some options from the config file
//
static const char *mg_options[] = {
    "listening_ports", "8080",
    "num_threads", "3",
    NULL
};

static const char *ajax_reply_start =
    "HTTP/1.1 200 OK\r\n"
    "Cache: no-cache\r\n"
    "Content-Type: application/json\r\n"
    "\r\n";

static const char *ajax_error_start =
    "HTTP/1.1 409 Conflict\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n";

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info);

/**
 * Process the URI /ajax/wake_up.
 *
 * The device_id field in the query string defines the device to be woken up.
 * Currently it is the MAC address of the device. The web server makes a call
 * to the sleep-proxy server to wake up the device. Then the server returns a
 * result, either success or failure, which is returned to the web client as
 * a JSON object (true for success and false for failure).
 */
static void ajax_wake_up(struct mg_connection *,
                         const struct mg_request_info *);

/**
 * Process the URI /ajax/get_device_list.
 *
 * There is no field in the query string. The web server makes a call to the
 * sleep-proxy server to get the list. The list is returned to the web client
 * as a JSON object. The following example shows the structure of the object.
 * Note that a device may have many network interfaces but only the interface
 * that communicates with the sleep-proxy server is relevant.
 *
 * [
 *     {"mac": "00:11:22:33:44:55", "ip": "10.0.0.1", "status": "on"},
 *     {"mac": "00:11:22:33:44:56", "ip": "10.0.0.2", "status": "waking up"},
 *     {"mac": "00:11:22:33:44:57", "ip": "10.0.0.3", "status": "sleeping"}
 * ]
 *
 * Currently there are three status for a device: "on", "waking up", and
 * "sleeping".
 */
static void ajax_get_device_list(struct mg_connection *,
                                 const struct mg_request_info *);

/**
 * Get the value of the specified variable in the query string.
 */
static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len);

int main(void) {
    struct mg_context *ctx;

    //
    // TODO the program should be a daemon
    //
    chdir(".");

    ctx = mg_start(&event_handler, mg_options);
    if (ctx == NULL)
        error(2, errno, PROGNAME ": mg_start");

    printf("Press enter to quit.\n");
    getchar();
    mg_stop(ctx);
    return 0;
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
    else if (strcmp(uri, "/ajax/wake_up") == 0)
        ajax_wake_up(conn, request_info);
    else if (strcmp(uri, "/ajax/get_device_list") == 0)
        ajax_get_device_list(conn, request_info);
    else
        return NULL;
    return "processed";
}

static void ajax_wake_up(struct mg_connection *conn,
                         const struct mg_request_info *request_info)
{
    char device_id[24];  // currently 17 is just enough for MAC address

    get_qsvar(request_info, "device_id", device_id, sizeof(device_id));
    if (strlen(device_id) == 0) {
        mg_printf(conn, "%s", ajax_error_start);
        mg_printf(conn, "%s", "device_id must be specified");
        return;
    }
    //
    // TODO call the server to wake up the device
    //
    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "%s", "true");  // it never fails?
}

static void ajax_get_device_list(struct mg_connection *conn,
                                 const struct mg_request_info *request_info)
{
    //
    // TODO call the server to get the list
    //
    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "%s", "[]");
}

static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len)
{
    const char *qs = request_info->query_string;
    mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, dst, dst_len);
}
