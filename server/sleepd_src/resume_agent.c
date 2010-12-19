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

static void *loop(void *agent_) {
    struct agent *agent = (struct agent *) agent_;
    int i;

    for (i = 0; i < MAX_MAGIC_PACKETS; ++i) {
        if (agent->state == UP)
            break;
        send_magic_packet(&agent->mac, NULL);
        sleep(WOL_INTERVAL);
    }
    return (void *) 0;
}
