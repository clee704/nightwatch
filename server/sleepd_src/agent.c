#include "agent.h"

struct agent_list *new_agent_list(int max_agents)
{
    return NULL;
}

/*
                // Find the agent
                for (i = 0; i < AGN_MAX_AGENTS; ++i) {
                    if (agn_alloc[i] == 0)
                        continue;
                    a = agents[i];
                    if (ETH_ADDR_EQ(a->mac, mac))
                        break;
                }
                
                // ... not found
                if (i == AGN_MAX_AGENTS) {
                    respond(conn, 404, resp_buf, &resp);
                    pthread_mutex_unlock(&agn_mutex);
                    break;
                }

                if (req.method == RSUM)
                    if (a->state == UP || a->state == RESUMING)
                        respond(conn, 409, resp_buf, &resp);
                    else {
                        resume(a);
                        respond(conn, 200, resp_buf, &resp);
                    }
                else  // req.method == SUSP
                    if (a->state == SUSPENDED)
                        respond(conn, 409, resp_buf, &resp);
                    else {
                        request(a->fd, SUSP, req_buf, &req);
                        respond(conn, 200, resp_buf, &resp);
                    }

                pthread_mutex_unlock(&agn_mutex);
*/
