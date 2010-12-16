#ifndef AGENT_H
#define AGENT_H

#include <time.h>

#include <netinet/in.h>

#include <net/ethernet.h>
#include <sys/param.h>

struct agent {
    int fd;  // connected socket file descriptor
    char hostname[MAXHOSTNAMELEN];
    struct in_addr ip;
    struct ether_addr mac;
    enum STATE { UP, SUSPENDED, RESUMING, DOWN } state;
    time_t monitored_since;
    time_t total_uptime;
    time_t total_downtime;
    time_t sleep_time;
};

#endif /* AGENT_H */
