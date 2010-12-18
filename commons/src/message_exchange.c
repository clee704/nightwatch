#include <string.h>
#include <syslog.h>

#include "message_exchange.h"
#include "network.h"
#include "protocol.h"

void request(int fd, int method, struct message_buffer *buf)
{
    int len;

    buf->u.request.method = method;
    buf->u.request.has_uri = 0;
    buf->u.request.has_data = 0;
    len = serialize_request(&buf->u.request, buf->chars);
    if (len < 0)
        syslog(LOG_WARNING, "[message_exchange] serialize_request() failed");
    if (write_string(fd, buf->chars))
        syslog(LOG_WARNING, "[message_exchange] write() failed: %m");
}

void respond(int fd, int status, struct message_buffer *buf)
{
    int len;
    const char *msg = "";

    switch (status) {
    case 200: msg = "OK"; break;
    case 400: msg = "Bad Request"; break;
    case 404: msg = "Not Found"; break;
    case 409: msg = "Conflict"; break;
    case 501: msg = "Not Implemented"; break;
    default:
        syslog(LOG_WARNING, "[message_exchange] invalid status %d", status);
        return;
    }
    buf->u.response.status = status;
    strcpy(buf->u.response.message, msg);
    buf->u.response.has_data = 0;
    len = serialize_response(&buf->u.response, buf->chars);
    if (len < 0)
        syslog(LOG_WARNING, "[message_exchange] serialize_response() failed");
    if (write_string(fd, buf->chars))
        syslog(LOG_WARNING, "[message_exchange] write() failed: %m");
}
