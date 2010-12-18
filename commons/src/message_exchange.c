#include <string.h>
#include <syslog.h>

#include "message_exchange.h"
#include "network.h"
#include "protocol.h"

void request(int fd, int method, struct request *req, char *buf)
{
    int len;

    req->method = method;
    req->has_uri = 0;
    req->has_data = 0;
    len = serialize_request(req, buf);
    if (len < 0)
        syslog(LOG_WARNING, "[message_exchange] serialize_request() failed");
    if (write_string(fd, buf))
        syslog(LOG_WARNING, "[message_exchange] can't write: %m");
}

void respond(int fd, int status, struct response *resp, char *buf)
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
    resp->status = status;
    strcpy(resp->message, msg);
    resp->has_data = 0;
    len = serialize_response(resp, buf);
    if (len < 0)
        syslog(LOG_WARNING, "[message_exchange] serialize_response() failed");
    if (write_string(fd, buf))
        syslog(LOG_WARNING, "[message_exchange] can't write: %m");
}
