#ifndef DAEMONIZE_H
#define DAEMONIZE_H

/**
 * Make the calling process a daemon.
 *
 * It terminates the program on error by calling error().
 */
void daemonize(const char *cmd);

/**
 * Write the current process's PID to <dir>/<name>.pid and return
 * 0 on success, -1 on error.
 */
int write_pid(const char *dir, const char *name);

#endif /* DAEMONIZE_H */
