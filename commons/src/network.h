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

struct sockaddr_un;

/**
 * TODO documentate
 */
int init_server(int type, const struct sockaddr *, socklen_t alen, int qlen);

/**
 * TODO documentate
 */
int connect_to(int type, const struct sockaddr *, socklen_t alen);

/**
 * TODO documentate
 */
void set_sockaddr_in(struct sockaddr_in *,
                     short family, unsigned short port, unsigned long addr);

/**
 * TODO documentate
 */
int set_sockaddr_un(struct sockaddr_un *, const char *path);

/**
 * TODO documentate
 */
int write_string(int fd, const char *);

#endif /* NETWORK_H */
