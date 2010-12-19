#include <pthread.h>
#include <sys/types.h>

#include "packet_monitor.h"
#include "agent.h"
#include "logger.h"

#define LOGGER_PREFIX "[packet_monitor] "

static void *monitor_packets(void *agent_list);

int start_packet_monitor(pthread_t *tid, struct agent_list *list)
{
    if (pthread_create(tid, NULL, monitor_packets, (void *) list)) {
        ERROR("can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *monitor_packets(void *agent_list)
{
    struct agent_list *list = (struct agent_list *) agent_list;
    //
    // TODO impl
    // monitor the network
    // if the IP address of a SYN packet matches,
    //   resume the agent
    //   forward the SYN packet to the agent
    //
    return (void *) 0;
}