#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <error.h>

#include "daemon.h"

// This is based on code originally by Richard Stevens APUE
void
daemonize(const char *cmd)
{
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;
    int fd0, fd1, fd2;
    unsigned int i;

    // Clear file creation mask
    umask(0);

    // Get maximum number of file descriptors
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        error(2, errno, "can't get file limit");

    // Become a session leader to lose controlling TTY
    pid = fork();
    if (pid < 0)
        error(2, errno, "can't fork");
    else if (pid != 0)  // parent
        exit(0);
    setsid();

    // Ensure future open()s won't allocate controlling TTYs
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        error(2, errno, "can't ignore SIGHUP");
    pid = fork();
    if (pid < 0)
        error(2, errno, "can't fork");
    else if (pid != 0)  // parent
        exit(0);

    // Change the current working directory to the root so
    // we won't prevent file systems from being unmounted
    if (chdir("/") < 0)
        error(2, errno, "can't change directory to /");

    // Close all open file descriptors
    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;
    for (i = 0; i < rl.rlim_max; ++i)
        close(i);

    // Attach file descriptors 0, 1, and 2 to /dev/null
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    // Initialize the log file
    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

int
write_pid(const char *filename)
{
    static char buffer[512];
    FILE *fp;
    int size;
    pid_t pid;

    fp = fopen(filename, "w");
    if (fp == NULL)
        return -1;
    pid = getpid();
    size = snprintf(buffer, sizeof(buffer), "%d", pid);
    if (size < 0 || (int) sizeof(buffer) < size)
        return -1;
    if (fwrite(buffer, 1, size, fp) != (size_t) size)
        return -1;
    if (fclose(fp) == EOF)
        return -1;
    return 0;
}

int
register_signal_handler(int signum, void handler(int))
{
    struct sigaction sa;

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, signum);
    sa.sa_flags = 0;
    return sigaction(signum, &sa, NULL);
}
