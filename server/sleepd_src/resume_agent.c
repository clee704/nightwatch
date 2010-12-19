#include <pthread.h>
#include <unistd.h>

#include "resume_agent.h"
#include "agent.h"
#include "send_magic_packet.h"
#include "logger.h"

#define LOGGER_PREFIX "[resume_agent] "
#define WOL_INTERVAL 1
#define MAX_MAGIC_PACKETS 15

static void *loop(void *agent);

int resume_agent(struct agent *agent)
{
    pthread_t tid;

    lock_agent(agent);
    if (agent->state == SUSPENDED)
        agent->state = RESUMING;
    unlock_agent(agent);
    if (pthread_create(&tid, NULL, loop, (void *) agent)) {
        WARNING("can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *loop(void *an_agent) {
    struct agent *agent = (struct agent *) an_agent;
    int i;

    for (i = 0; i < MAX_MAGIC_PACKETS; ++i) {
        if (agent->state == UP)
            break;
        if (send_magic_packet(&agent->mac, NULL))
            WARNING("send_magic_packet() failed: %m");
        sleep(WOL_INTERVAL);
    }
    return NULL;
}
