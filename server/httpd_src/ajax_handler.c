#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ajax_handler.h"
#include "logger.h"
#include "network.h"
#include "message_exchange.h"
#include "mongoose.h"

#define LOGGER_PREFIX "[ajax_handler] "

static void handle_simple_message(struct mg_connection *,
                                  const struct mg_request_info *,
                                  enum method method);
static void print_device_list(struct mg_connection *, char *data);

static int connect_to_proxy();
static void send_ajax_response(struct mg_connection *, int status);
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
    handle_simple_message(conn, request_info, RSUM);
}

void ajax_suspend(struct mg_connection *conn,
                  const struct mg_request_info *request_info)
{
    handle_simple_message(conn, request_info, SUSP);
}

static void handle_simple_message(struct mg_connection *conn,
                                  const struct mg_request_info *request_info,
                                  enum method method)
{
    struct message_buffer buf;
    struct response *response = &buf.u.response;
    int sock, n;
    char device_id[MAX_URI_LEN] = {0};

    // Get the argument
    mg_get_qsvar(request_info, "deviceId", device_id, sizeof(device_id));
    if (strlen(device_id) == 0) {
        send_ajax_response(conn, 400);
        return;
    }

    // Connect to the proxy
    sock = connect_to_proxy(sock_file);
    if (sock < 0) {
        WARNING("can't connect to %s: %m", sock_file);
        WARNING("make sure the sleep proxy (nitch-sleepd) is running");
        send_ajax_response(conn, 500);
        return;
    }

    // Send the request
    send_request(sock, method, device_id, NULL, &buf);

    // Read the response from the proxy
    n = read(sock, buf.chars, sizeof(buf.chars) - 1);
    if (n <= 0) {
        if (n == 0)
            WARNING("unexpected EOF from the proxy");
        else
            WARNING("can't read: %m");
        send_ajax_response(conn, 500);
        return;
    }
    buf.chars[n] = 0;
    if (parse_response(buf.chars, response)) {
        WARNING("invalid response");
        send_ajax_response(conn, 500);
        return;
    }

    // Close the connection to the proxy
    if (close(sock))
        WARNING("can't close the socket: %m");

    switch (response->status) {
    case 200:
    case 400:
    case 404:
    case 409:
    case 500:
        send_ajax_response(conn, response->status);
        break;
    default:
        WARNING("got unexpected response status code: %d", response->status);
        send_ajax_response(conn, 500);
        return;
    }
}

void ajax_device_list(struct mg_connection *conn,
                      const struct mg_request_info *unused)
{
    struct message_buffer buf;
    struct response *response = &buf.u.response;
    char *data = response->data;
    int sock, n;

    // Connect to the proxy
    sock = connect_to_proxy(sock_file);
    if (sock < 0) {
        WARNING("can't connect to %s: %m", sock_file);
        WARNING("make sure the sleep proxy (nitch-sleepd) is running");
        send_ajax_response(conn, 500);
        return;
    }

    // Send the request
    send_request(sock, GETA, NULL, NULL, &buf);

    // Read the response from the proxy
    n = read(sock, buf.chars, sizeof(buf.chars) - 1);
    if (n <= 0) {
        if (n == 0)
            WARNING("unexpected EOF from the proxy");
        else
            WARNING("can't read: %m");
        send_ajax_response(conn, 500);
        return;
    }
    buf.chars[n] = 0;
    if (parse_response(buf.chars, response)) {
        WARNING("invalid response");
        send_ajax_response(conn, 500);
        return;
    }

    // Close the connection to the proxy
    if (close(sock))
        WARNING("can't close the socket: %m");

    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "{\"status\":200,\"message\":\"ok\",\"data\":");
    print_device_list(conn, data);
    mg_printf(conn, "}");
    unused = unused;
}

static void print_device_list(struct mg_connection *conn, char *data)
{
    const char *keys[][2] = {
        {"hostname", "string"},
        {"ip", "string"},
        {"mac", "string"},
        {"state", "string"},
        {"monitoredSince", "number"},
        {"totalUptime", "number"},
        {"sleepTime", "number"},
        {"totalDowntime", "number"}
    };
    char *s1, *s2;
    const char *key, *type;
    char c;
    int i, j;

    s1 = data;
    s2 = data;
    i = 0;
    j = 0;
    mg_printf(conn, "[");
    while ((c = *s2) != 0) {
        switch (c) {
        case ',':
        case '\n':
            *s2 = 0;
            key = keys[j][0];
            type = keys[j][1];
            mg_printf(conn, j == 0 ? (i == 0 ? "{" : ",{") : ",");
            mg_printf(conn, "\"%s\":", key);
            if (strcmp(type, "string") == 0)
                mg_printf(conn, "\"%s\"", s1);
            else 
                mg_printf(conn, "%s", s1);
            if (c == ',')
                ++j;
            else {
                mg_printf(conn, "}");
                ++i;
                j = 0;
            }
            s1 = s2 + 1;
            break;
        }
        ++s2;
    }
    mg_printf(conn, "]");
}

static int connect_to_proxy()
{
    struct sockaddr_un addr;
    int addr_len;

    addr_len = set_sockaddr_un(&addr, sock_file);
    if (addr_len < 0)
        return -1;
    return connect_to(SOCK_STREAM,
                      (struct sockaddr *) &addr,
                      sizeof(struct sockaddr_un));
}

static void send_ajax_response(struct mg_connection *conn, int status)
{
    const char *message;

    switch (status) {
    case 200: message = "ok"; break;
    case 400: message = "bad request"; break;
    case 404: message = "no such device"; break;
    case 409: message = "already up, resuming or suspended"; break;
    case 500: message = "internal server error"; break;
    default:
        WARNING("invalid status code: %d", status);
        return;
    }
    mg_printf(conn, "%s", ajax_reply_start);
    mg_printf(conn, "{\"status\":%d,\"message\":\"%s\"}", status, message);
}

static void mg_get_qsvar(const struct mg_request_info *request_info,
                         const char *name,
                         char *buffer, size_t max_len)
{
    const char *qs = request_info->query_string;
    mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, buffer, max_len);
}
