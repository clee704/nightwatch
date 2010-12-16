#ifndef AGENT_H
#define AGENT_H

#include <time.h>

struct agent {
    int fd;  // connected socket file descriptor
    const char *hostname;
    const char *ip;
    const char *mac;
    enum STATE { UP, SUSPENDED, RESUMING, DOWN } state;
    time_t monitored_since;
    time_t total_uptime;
    time_t total_downtime;
    time_t sleep_time;
};

#endif /* AGENT_H */
