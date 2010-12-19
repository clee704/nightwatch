#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include <sys/types.h>

struct agent_list;

/**
 * TODO documentate
 */
int start_ui_handler(pthread_t *, struct agent_list *, const char *sock_file);

#endif /* UI_HANDLER_H */
