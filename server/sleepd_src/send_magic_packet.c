#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>

#include "send_magic_packet.h"

#define MAGIC_PACKET_SIZE 116

static size_t build_packet(const u_int8_t *dst_addr_octet,
                           const u_int8_t *src_addr_octet,
                           unsigned char *packet);

int send_magic_packet(const struct ether_addr *dst_addr, const char *ifname_in)
{
    const char *ifname = ifname_in == NULL ? "eth0" : ifname_in;
    struct ether_addr src_addr;
    int sock;
    struct sockaddr_ll dst_sockaddr;
    unsigned char packet[MAGIC_PACKET_SIZE];
    size_t packet_sz;

    sock = socket(PF_PACKET, SOCK_RAW, 0);
    if (sock < 0)
        return -1;

    // Get the source address from the interface name
    {
        struct ifreq ifr;
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
            return -1;
        memcpy(src_addr.ether_addr_octet, ifr.ifr_hwaddr.sa_data, 6);
    }

    // Make dst_sockaddr for sendto()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0)
            return -1;

        memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
        dst_sockaddr.sll_family = AF_PACKET;
        dst_sockaddr.sll_ifindex = ifr.ifr_ifindex;
        dst_sockaddr.sll_halen = 6;
        memcpy(dst_sockaddr.sll_addr, dst_addr->ether_addr_octet, 6);
    }

    packet_sz = build_packet(dst_addr->ether_addr_octet,
                             src_addr.ether_addr_octet,
                             packet);

    if (sendto(sock, packet, packet_sz, 0,
               (struct sockaddr *) &dst_sockaddr, sizeof(dst_sockaddr)) < 0)
        return -1;

    if (close(sock))
        return -1;

    return 0;
}

static size_t build_packet(const u_int8_t *dst_addr_octet,
                           const u_int8_t *src_addr_octet,
                           unsigned char *packet)
{
    size_t offset, i;

    // Ethernet header
    memcpy(packet + 0, dst_addr_octet, 6);  // MAC destination
    memcpy(packet + 6, src_addr_octet, 6);  // MAC source
    memcpy(packet + 12, "\x08\x42", 2);     // EtherType (0x0842 for WoL)

    // Ethernet payload 
    memset(packet + 14, 0xff, 6);
    offset = 20;
    for (i = 0; i < 16; ++i, offset += 6)
        memcpy(packet + offset, dst_addr_octet, 6);
    return offset;
}
