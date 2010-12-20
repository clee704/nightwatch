#include <string.h>
#include "message_exchange.h"
#include "logger.h"
#include "network.h"
#include "protocol.h"

#define LOGGER_PREFIX "[message_exchange] "

void send_request(int fd, int method, const char *uri, const char *data,
                  struct message_buffer *buf)
{
    int n;

    buf->u.request.method = method;
    buf->u.request.has_uri = (uri != NULL);
    if (uri != NULL) {
        n = strlen(uri);
        if (n > MAX_URI_LEN - 1)
            WARNING("URI [%.24s...] is too long and will be truncated", uri);
        strncpy(buf->u.request.uri, uri, n);
        buf->u.request.uri[n] = 0;
    }
    buf->u.request.has_data = (data != NULL);
    if (data != NULL) {
        n = strlen(data);
        if (n > MAX_DATA_LEN - 1)
            WARNING("data [%.24s...] is too long and will be truncated", data);
        strncpy(buf->u.request.data, data, n);
        buf->u.request.data[n] = 0;
    }
    n = serialize_request(&buf->u.request, buf->chars);
    if (n < 0)
        WARNING("serialize_request() failed");
    if (write_string(fd, buf->chars))
        WARNING("write() failed: %m");
}

void send_response(int fd, int status, const char *data,
                   struct message_buffer *buf)
{
    int n;
    const char *msg = "";

    switch (status) {
    case 200: msg = "OK"; break;
    case 400: msg = "Bad Request"; break;
    case 402: msg = "Payment Required"; break;
    case 404: msg = "Not Found"; break;
    case 409: msg = "Conflict"; break;
    case 418: msg = "Unexpected Method"; break;
    case 500: msg = "Internal Server Error"; break;
    case 501: msg = "Not Implemented"; break;
    default:
        WARNING("invalid status %d", status);
        return;
    }
    buf->u.response.status = status;
    strcpy(buf->u.response.message, msg);
    buf->u.response.has_data = (data != NULL);
    if (data != NULL) {
        n = strlen(data);
        if (n > MAX_DATA_LEN - 1)
            WARNING("data [%.24s...] is too long and will be truncated", data);
        strncpy(buf->u.response.data, data, n);
        buf->u.response.data[n] = 0;
    }
    n = serialize_response(&buf->u.response, buf->chars);
    if (n < 0)
        WARNING("serialize_response() failed");
    if (write_string(fd, buf->chars))
        WARNING("write() failed: %m");
}
