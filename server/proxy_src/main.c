#include <stdio.h>

#include <syslog.h>

#include "daemonize.h"
#include "send_magic_packet.h"

int main(int argc, char **argv)
{
    (void) argc;

    daemonize();
    //
    // TODO
    //
    syslog(LOG_INFO, "hello from %s\n", argv[0]);
    return 0;
}
