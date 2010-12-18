#ifndef MESSAGE_EXCHANGE_H
#define MESSAGE_EXCHANGE_H

struct request;
struct response;

/**
 * TODO documentate
 */
void request(int fd, int method, struct request *req, char *buf);

/**
 * TODO documentate
 */
void respond(int fd, int status, struct response *resp, char *buf);

#endif /* MESSAGE_EXCHANGE_H */
