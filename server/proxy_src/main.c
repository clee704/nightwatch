#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>

#include <syslog.h>

#include "daemon.h"
#include "send_magic_packet.h"

int main(int argc, char **argv)
{
    (void) argc;

    daemonize(program_invocation_name);
    //
    // TODO
    //
    syslog(LOG_INFO, "hello from %s\n", argv[0]);
    return 0;
}
