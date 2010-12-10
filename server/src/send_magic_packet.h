#ifndef SEND_MAGIC_PACKET_H
#define SEND_MAGIC_PACKET_H

/**
 * Send a magic packet to the host specified by the 48-bit Ethernet host
 * address given in the standard hex-digits-and-colons notation.
 */
void send_magic_packet(const char *host_addr);

#endif /* SEND_MAGIC_PACKET_H */
