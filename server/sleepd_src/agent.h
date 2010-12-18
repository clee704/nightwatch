#ifndef AGENT_H
#define AGENT_H

#include <time.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <sys/param.h>

struct agent {
    char hostname[MAXHOSTNAMELEN];
    struct in_addr ip;
    struct ether_addr mac;
    enum state { UP, SUSPENDED, RESUMING, DOWN } state;
    time_t monitored_since;
    time_t total_uptime;
    time_t total_downtime;
    time_t sleep_time;
    int in;   // file descriptor for reading
    int out;  // file descriptor for writing
    pthread_mutex_t mutex;
};

struct agent_list {
};

/**
 * TODO documentate
 */
struct agent_list *new_agent_list(int max_agents);

#endif /* AGENT_H */
