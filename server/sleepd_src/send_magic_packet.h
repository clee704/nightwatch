#ifndef SEND_MAGIC_PACKET_H
#define SEND_MAGIC_PACKET_H

struct ether_addr;

/**
 * Send a magic packet to the host specified by dst_addr via the specified
 * interface (ifname).
 *
 * Return 0 on success and -1 on failure.
 *
 * ifname defaults to "eth0" if it is NULL.
 */
int send_magic_packet(const struct ether_addr *dst_addr, const char *ifname);

#endif /* SEND_MAGIC_PACKET_H */
