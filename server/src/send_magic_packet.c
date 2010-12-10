#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include "send_magic_packet.h"

void send_magic_packet(const char *host_addr)
{
    int retval, sock;
    struct ether_addr *addr;

    sock = socket(PF_PACKET, SOCK_RAW, 0);
    if (sock < 0)
        error(2, errno, "nitch_server: socket");

    retval = setuid(getuid());
    if (retval < 0)
        error(2, errno, "nitch_server: setuid");

    addr = ether_aton(host_addr);
    if (addr == NULL)
        error(2, errno, "nitch_server: ether");
}
