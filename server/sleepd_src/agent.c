#include <stdlib.h>
#include <pthread.h>
#include "agent.h"
#include "logger.h"
#include "network.h"

#define LOGGER_PREFIX "[agent] "

void lock_agent(struct agent *agent)
{
    pthread_mutex_lock(&agent->mutex);
} 

void unlock_agent(struct agent *agent)
{
    pthread_mutex_unlock(&agent->mutex);
}

struct agent_list *new_agent_list()
{
    struct agent_list *list;

    list = (struct agent_list *) malloc(sizeof(struct agent_list));
    if (list == NULL)
        goto error;
    list->size = 0;
    list->head = (struct _agent_list_node *)
        malloc(sizeof(struct _agent_list_node));  // dummy node
    if (list->head == NULL) {
        free(list);
        goto error;
    }
    list->head->next = NULL;
    list->tail = list->head;
    return list;
error:
    WARNING("can't allocate memory: %m");
    return NULL;
}

struct agent *find_agent_by_ip(struct agent_list *list,
                               const struct in_addr *ip)
{
    struct agent *agent = NULL;
    struct _agent_list_node *node;

    for (node = list->head->next; node != NULL; node = node->next)
        if (IN_ADDR_EQ(node->agent->ip, *ip)) {
            agent = node->agent;
            break;
        }
    return agent;
}

struct agent *find_agent_by_mac(struct agent_list *list,
                                const struct ether_addr *mac)
{
    struct agent *agent = NULL;
    struct _agent_list_node *node;

    for (node = list->head->next; node != NULL; node = node->next)
        if (ETHER_ADDR_EQ(node->agent->mac, *mac)) {
            agent = node->agent;
            break;
        }
    return agent;
}

int add_new_agent(struct agent_list *list, const struct agent *agent)
{
    struct _agent_list_node *node;

    node = (struct _agent_list_node *) malloc(sizeof(struct _agent_list_node));
    if (node == NULL) {
        WARNING("can't allocate memory: %m");
        return -1;
    }
    node->agent = agent;
    node->next = NULL;
    list->tail->next = node;
    list->tail = node;
    list->size += 1;
    return 0;
}

struct agent_list_iterator *new_agent_list_iterator(struct agent_list *list)
{
    struct agent_list_iterator *iter;

    iter = (struct agent_list_iterator *)
        malloc(sizeof(struct agent_list_iterator));
    if (iter == NULL) {
        WARNING("can't allocate memory: %m");
        return NULL;
    }
    iter->next = list->head->next;
    return iter;
}

void delete_agent_list_iterator(struct agent_list_iterator *iter)
{
    free(iter);
}

struct agent *next_agent(struct agent_list_iterator *iter)
{
    struct _agent_list_node *next = iter->next;

    if (next == NULL)
        return NULL;
    else {
        iter->next = next->next;
        return next->agent;
    }
}
