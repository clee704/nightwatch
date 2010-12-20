#ifndef AGENT_HANDLER_H
#define AGENT_HANDLER_H

#include <sys/types.h>

struct agent_list;

/**
 * TODO documentate
 */
int start_agent_handler(pthread_t *,
                        struct agent_list *,
                        int listening_port,
                        int listening_port_on_agent);

#endif /* AGENT_HANDLER_H */
