#ifndef AGENT_H
#define AGENT_H

struct agent {
    int fd;  // connected socket file descriptor
    const char *hostname;
    const char *ip;
    const char *mac;
    enum STATE { UP, SUSPENDED, RESUMING, DOWN } state;
    int monitored_since;
    int total_uptime;
    int total_downtime;
    int sleep_time;
};

#endif /* AGENT_H */
