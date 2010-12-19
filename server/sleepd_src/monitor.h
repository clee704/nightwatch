#ifndef MONITOR_H
#define MONITOR_H

#include "agent.h"

int monitor_syn_packet(struct agent_list *agentList);
int wakeup_agent(struct agent *agent);

#endif
