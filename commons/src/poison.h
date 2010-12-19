#ifndef POISON_H
#define POISON_H

struct in_addr;
struct ether_addr;

int send_poison_packet(const struct in_addr *, const struct ether_addr *,
                       const char *ifname);

#endif /* POISON_H */
