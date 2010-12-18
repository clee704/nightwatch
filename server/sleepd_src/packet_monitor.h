#ifndef PACKET_MONITOR_H
#define PACKET_MONITOR_H

#include <sys/types.h>

struct agent_list;

/**
 * TODO documentate
 */
int start_packet_monitor(pthread_t *, struct agent_list *);

#endif /* PACKET_MONITOR_H */
