#ifndef NETWORK_H
#define NETWORK_H

#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include <netinet/ether.h>

#define IN_ADDR_EQ(a, b) ((a).s_addr == (b).s_addr)

#define ETHER_ADDR_EQ(a, b) \
    (strncmp((char *) (a).ether_addr_octet, \
             (char *) (b).ether_addr_octet, \
             ETH_ALEN) == 0)

/**
 * TODO documentate
 */
int init_server(int type, const struct sockaddr *, socklen_t alen, int qlen);

/**
 * TODO documentate
 */
int write_string(int fd, const char *);

#endif /* NETWORK_H */
