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
    time_t sleep_time;
    time_t total_downtime;
    time_t _last_update_time;
    int fd1;  // agent requests something and sleep proxy respond
    int fd2;  // sleep proxy requests something and agent respond
    pthread_mutex_t mutex;
};

struct _agent_list_node {
    struct agent *agent;
    struct _agent_list_node *next;
};

struct agent_list {
    int size;
    struct _agent_list_node *head;
    struct _agent_list_node *tail;
};

struct agent_list_iterator {
    struct _agent_list_node *next;
};

const char *agent_state_to_string(const struct agent *);

/**
 * TODO documentate
 */
void lock_agent(struct agent *);

/**
 * TODO documentate
 */
void unlock_agent(struct agent *);

/**
 * TODO documentate
 */

/**
 * TODO documentate
 */
struct agent_list *new_agent_list();

/**
 * TODO documentate
 */
struct agent *find_agent_by_ip(struct agent_list *, const struct in_addr *);

/**
 * TODO documentate
 */
struct agent *find_agent_by_mac(struct agent_list *,
                                const struct ether_addr *);

/**
 * TODO documentate
 */
int add_new_agent(struct agent_list *, struct agent *);

/**
 * TODO documentate
 */
struct agent_list_iterator *new_agent_list_iterator(struct agent_list *);

/**
 * TODO documentate
 */
void delete_agent_list_iterator(struct agent_list_iterator *);

/**
 * TODO documentate
 */
struct agent *next_agent(struct agent_list_iterator *);

#endif /* AGENT_H */
