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
void request(int fd, int method, struct message_buffer *);

/**
 * TODO documentate
 */
void respond(int fd, int status, struct message_buffer *);

//void request_with_uri();
//void respond_with_data();

#endif /* MESSAGE_EXCHANGE_H */
