#include <string.h>
#include "mongoose.h"
#include "ajax.h"

static const char *ajax_reply_start =
    "HTTP/1.1 200 OK\r\n"
    "Cache: no-cache\r\n"
    "Content-Type: application/json\r\n"
    "\r\n";

static const char *ajax_error_start =
    "HTTP/1.1 409 Conflict\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n";

/**
 * Get the value of the specified variable in the query string.
 */
static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len);

void ajax_wake_up(struct mg_connection *conn,
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
    mg_printf(conn, "%s", "{\"success\": true, \"message\": \"ok\"}");
}

void ajax_get_device_list(struct mg_connection *conn,
                          const struct mg_request_info *unused)
{
    (void) unused;
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
