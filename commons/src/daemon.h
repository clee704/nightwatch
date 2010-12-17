#ifndef DAEMON_H
#define DAEMON_H

/**
 * Make the calling process a daemon.
 */
void daemonize(const char *cmd);

/**
 * Write the current process's PID to the specified file.
 *
 * Return 0 on success and -1 on error. errno is set on error.
 */
int write_pid(const char *filename);

/**
 * Register the signal handler for the specified signal.
 *
 * Return 0 on success and -1 on error. errno is set on error.
 */
int register_signal_handler(int signum, void (*handler)(int));

#endif /* DAEMON_H */
