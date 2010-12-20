#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ajax_handler.h"
#include "logger.h"
#include "message_exchange.h"
#include "mongoose.h"

#define LOGGER_PREFIX "[ajax_handler] "

static void handle_simple_message(struct mg_connection *,
                                  const struct mg_request_info *,
                                  const char *method);
static void handle_message(struct mg_connection *,
                           const struct mg_request_info *,
                           const char *method);

static int connect_to_proxy();
static void print_response(struct mg_connection *, const char *message);
static void mg_get_qsvar(const struct mg_request_info *,
			 const char *name,
			 char *buffer, size_t max_len);

static const char *sock_file = NULL;
static const char * const ajax_reply_start =
    "HTTP/1.1 200 OK\r\n"
    "Cache: no-cache\r\n"
    "Content-Type: application/json\r\n"
    "\r\n";

void ajax_set_socket_file(const char *socket_file)
{
    sock_file = socket_file;
}

void ajax_resume(struct mg_connection *conn,
                 const struct mg_request_info *request_info)
{
    handle_simple_message(conn, request_info, "RSUM");
}

void ajax_suspend(struct mg_connection *conn,
                  const struct mg_request_info *request_info)
{
    handle_simple_message(conn, request_info, "SUSP");
}

void ajax_device_list(struct mg_connection *conn,
                      const struct mg_request_info *unused)
{
    unused = unused;
}

static void handle_simple_message(struct mg_connection *conn,
                                  const struct mg_request_info *request_info,
                                  const char *method)
{
}

static void handle_message(struct mg_connection *conn,
                           const struct mg_request_info *request_info,
                           const char *method)
{
    struct message_buffer buf;
    char buffer[1000];
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
        WARNING("can't connect to %s: %m", sock_file);
        WARNING("make sure the sleep proxy (nitch-sleepd) is running");
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
        WARNING("can't write: %m");
        ajax_print_response(conn, "internal server error");
        return;
    }

    // Read the response from the proxy
    n = read(sock, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        if (n == 0)
            NOTICE("unexpected EOF from the proxy");
        else
            WARNING("can't read: %m");
        ajax_print_response(conn, "internal server error");
        return;
    }
    buffer[n] = 0;

    // Close the connection to the proxy
    if (close(sock))
        WARNING("can't close the socket: %m");

    //
    // TODO+ parse the response into a JSON object
    //
    DEBUG("%s", buffer);
    ajax_print_response(conn, "ok");
}

static int connect_to_proxy()
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

static void print_response(struct mg_connection *conn, const char *message)
{
    int success;

    success = strncmp(message, "ok", 2) == 0;
    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "{\"success\": %s, \"message\": \"%s\"}",
              success ? "true" : "false", message);
}

static void mg_get_qsvar(const struct mg_request_info *request_info,
                         const char *name,
                         char *buffer, size_t max_len)
{
    const char *qs = request_info->query_string;
    mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, buffer, max_len);
}
