#ifndef SEND_MAGIC_PACKET_H
#define SEND_MAGIC_PACKET_H

/**
 * Send a magic packet to the host specified by the 48-bit Ethernet host
 * address given in the standard hex-digits-and-colons notation (dst_addr)
 * via the specified interface (ifname) and return 0 for success, -1 for
 * failure.
 *
 * If the interface is not specified (ifname is NULL), then the default
 * interface name "eth0" is used.
 */
int
send_magic_packet(const char *dst_addr, const char *ifname);

#endif /* SEND_MAGIC_PACKET_H */
