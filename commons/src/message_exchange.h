#ifndef MESSAGE_EXCHANGE_H
#define MESSAGE_EXCHANGE_H

#include "protocol.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

struct message_buffer {
    char chars[MAX(MAX_REQUEST_STRLEN, MAX_RESPONSE_STRLEN)];
    union {
        struct request request;
        struct response response;
    } u;
};

/**
 * TODO documentate
 */
void send_request(int fd, int method, const char *uri, const char *data,
                  struct message_buffer *);

/**
 * TODO documentate
 */
void send_respond(int fd, int status, const char *data,
                  struct message_buffer *);

#endif /* MESSAGE_EXCHANGE_H */
