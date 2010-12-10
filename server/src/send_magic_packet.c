#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/ether.h>
#include "send_magic_packet.h"

static int build_packet(const unsigned char *dst_addr_octet,
                        const unsigned char *src_addr_octet,
                        unsigned char *packet);

void send_magic_packet(const char *dst_addr_str, const char *ifname_in)
{
    int sock;
    struct sockaddr_ll dst_sockaddr;
    struct ether_addr dst_addr, src_addr, *addrp;
    const char *ifname = ifname_in == NULL ? "eth0" : ifname_in;
    unsigned char *packet;
    int packet_sz;

    sock = socket(PF_PACKET, SOCK_RAW, 0);
    if (sock < 0)
        error(2, errno, "nitch_server: socket");

    // drop root privileges if the executable is suid root
    if (setuid(getuid()) < 0)
        error(2, errno, "nitch_server: setuid");

    addrp = ether_aton(dst_addr_str);
    if (addrp == NULL)
        error(2, errno, "nitch_server: ether_aton");
    dst_addr = *addrp;

    // get the source address from the interface name
    {
        struct ifreq ifr;
        unsigned char *hwaddr = ifr.ifr_hwaddr.sa_data;

        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
            error(2, errno, "nitch_server: ioctl (SIOCGIFHWADDR)");
        memcpy(src_addr.ether_addr_octet, ifr.ifr_hwaddr.sa_data, 6);
    }

    // build the magic packet
    packet = (unsigned char *) malloc(1000);
    if (packet == NULL)
        error(2, errno, "nitch_server: malloc");
    packet_sz = build_packet(dst_addr.ether_addr_octet,
                             src_addr.ether_addr_octet,
                             packet);

    // make dst_sockaddr for sendto()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0)
            error(2, errno, "nitch_server: ioctl (SIOCGIFINDEX)");

        memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
        dst_sockaddr.sll_family = AF_PACKET;
        dst_sockaddr.sll_ifindex = ifr.ifr_ifindex;
        dst_sockaddr.sll_halen = 6;
        memcpy(dst_sockaddr.sll_addr, dst_addr.ether_addr_octet, 6);
    }

    // finally, send the packet!
    if (sendto(sock, packet, packet_sz, 0, (struct sockaddr *) &dst_sockaddr,
               sizeof(dst_sockaddr)) < 0)
        error(2, errno, "nitch_server: sendto");
}

static int build_packet(const unsigned char *dst_addr_octet,
                        const unsigned char *src_addr_octet,
                        unsigned char *packet)
{
    int offset, i;

    memcpy(packet + 0, dst_addr_octet, 6);  // MAC destination
    memcpy(packet + 6, src_addr_octet, 6);  // MAC source
    memcpy(packet + 12, "\x08\x42", 2);     // EtherType (0x0842 for WoL)
    memset(packet + 14, 0xff, 6);
    offset = 20;
    for (i = 0; i < 16; ++i, offset += 6)
        memcpy(packet + offset, dst_addr_octet, 6);
    return offset;
}
