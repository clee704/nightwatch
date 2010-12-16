#include <stdio.h>
#include <string.h>

#include "protocol.h"

int
parse_request(const char *str, struct request *req)
{
    size_t i, n, p;

    n = strlen(str);
    if (n > MAX_REQUEST_LEN - 1)
        return -1;
    if (strncmp(str, "GETA", 4) == 0)
        req->method = GETA;
    else if (strncmp(str, "RSUM", 4) == 0)
        req->method = RSUM;
    else if (strncmp(str, "SUSP", 4) == 0)
        req->method = SUSP;
    else if (strncmp(str, "PING", 4) == 0)
        req->method = PING;
    else if (strncmp(str, "INFO", 4) == 0)
        req->method = INFO;
    else if (strncmp(str, "NTFY", 4) == 0)
        req->method = NTFY;
    else
        return -1;
    req->has_uri = req->has_data = 0;
    i = 4;
    if (str[i] == '\n' && n == i + 1)  // no URI, no data
        return 0;
    if (str[i] == ' ') {  // has a URI
        for (i = 5; i < n; ++i)
            if (str[i] == '\n')
                break;
        if (i == n)
            return -1;
        p = i - 5;
        if (p > MAX_URI_LEN - 1)
            return -1;
        strncpy(req->uri, str + 5, p);
        req->uri[p] = 0;
        req->has_uri = 1;
        if (n == i + 1)  // no data
            return 0;
    }
    if (str[i + 1] != '\n' || str[n - 2] != '\n' || str[n - 1] != '\n')
        return -1;
    p = n - i - 4;
    strncpy(req->data, str + i + 2, p);
    req->data[p] = 0;
    req->has_data = 1;
    return 0;
}

int
parse_response(const char *str, struct response *resp)
{
    size_t n, i, p;
    int status;

    n = strlen(str);
    if (n > MAX_RESPONSE_LEN - 1)
        return -1;
    if (sscanf(str, "%d", &status) != 1)
        return -1;
    if (status < 100 || status > 999)
        return -1;
    resp->status = status;
    i = 3;
    if (str[i] != ' ')
        return -1;
    for (i = 4; i < n; ++i)
        if (str[i] == '\n')
            break;
    if (i == n)
        return -1;
    p = i - 4;
    if (p > MAX_MESSAGE_LEN - 1)
        return -1;
    strncpy(resp->message, str + 4, p);
    resp->message[p] = 0;
    resp->has_data = 0;
    if (n == i + 1)  // no data
        return 0;
    if (str[i + 1] != '\n' || str[n - 2] != '\n' || str[n - 1] != '\n')
        return -1;
    p = n - i - 4;
    strncpy(resp->data, str + i + 2, p);
    resp->data[p] = 0;
    resp->has_data = 1;
    return 0;
}

int
serialize_request(const struct request *req, char *str)
{
    const char *m;
    size_t i, ulen, dlen;

    switch (req->method) {
    case GETA: m = "GETA"; break;
    case RSUM: m = "RSUM"; break;
    case SUSP: m = "SUSP"; break;
    case PING: m = "PING"; break;
    case INFO: m = "INFO"; break;
    case NTFY: m = "NTFY"; break;
    default: return -1;
    }
    i = 0;
    strncpy(str + i, m, 4); i += 4;
    if (req->has_uri) {
        str[i] = ' '; i += 1;
        ulen = strlen(req->uri);
        if (ulen > MAX_URI_LEN - 1)
            return -1;
        strncpy(str + i, req->uri, ulen); i += ulen;
    }
    str[i] = '\n'; i += 1;
    if (req->has_data) {
        str[i] = '\n'; i += 1;
        dlen = strlen(req->data);
        if (dlen > MAX_DATA_LEN - 1)
            return -1;
        strncpy(str + i, req->data, dlen); i += dlen;
        str[i] = '\n'; i += 1;
        str[i] = '\n'; i += 1;
    }
    str[i] = 0;
    return 0;
}

int
serialize_response(const struct response *resp, char *str)
{
    size_t i, mlen, dlen;

    if (resp->status < 100 || resp->status > 999)
        return -1;
    i = 0;
    i += sprintf(str, "%d", resp->status);
    str[i] = ' '; i += 1;
    mlen = strlen(resp->message);
    if (mlen > MAX_MESSAGE_LEN - 1)
        return -1;
    strncpy(str + i, resp->message, mlen); i += mlen;
    str[i] = '\n'; i += 1;
    if (resp->has_data) {
        str[i] = '\n'; i += 1;
        dlen = strlen(resp->data);
        if (dlen > MAX_DATA_LEN - 1)
            return -1;
        strncpy(str + i, resp->data, dlen); i += dlen;
        str[i] = '\n'; i += 1;
        str[i] = '\n'; i += 1;
    }
    str[i] = 0;
    return 0;
}
