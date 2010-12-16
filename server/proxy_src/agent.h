#ifndef AGENT_H
#define AGENT_H

#include <time.h>

#include <arpa/inet.h>

#include <sys/param.h>

struct agent {
    int fd;  // connected socket file descriptor
    char hostname[MAXHOSTNAMELEN];
    char ip[INET_ADDRSTRLEN];  //  "111.111.111.111"
    char mac[18];              // "00:11:22:33:44:55"
    enum STATE { UP, SUSPENDED, RESUMING, DOWN } state;
    time_t monitored_since;
    time_t total_uptime;
    time_t total_downtime;
    time_t sleep_time;
};

#endif /* AGENT_H */
