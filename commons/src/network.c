#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <sys/un.h>
#include <unistd.h>

#include "network.h"

int init_server(int type, const struct sockaddr *addr, socklen_t alen, int qlen)
{
    int fd = socket(addr->sa_family, type, 0);
    int reuse = 1;

    if (fd < 0)
        return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
        goto error;
    if (bind(fd, addr, alen))
        goto error;
    if ((type == SOCK_STREAM || type == SOCK_SEQPACKET) && listen(fd, qlen))
        goto error;
    return fd;
error:
    close(fd);
    return -1;
}

int connect_to(int type, const struct sockaddr *addr, socklen_t alen)
{
    int fd = socket(addr->sa_family, type, 0);

    if (fd < 0)
        return -1;
    if (connect(fd, addr, alen))
        return -1;
    return fd;
}

void set_sockaddr_in(struct sockaddr_in *sa,
                     short family, unsigned short port, unsigned long addr)
{
    memset(sa, 0, sizeof(struct sockaddr_in));
    sa->sin_family = family;
    sa->sin_port = htons(port);
    sa->sin_addr.s_addr = addr;
}

int set_sockaddr_un(struct sockaddr_un *un, const char *path)
{
    int n;

    memset(un, 0, sizeof(struct sockaddr_un));
    un->sun_family = PF_UNIX;
    n = snprintf(un->sun_path, sizeof(un->sun_path), "%s", path);
    if (n < 0 || (int) sizeof(un->sun_path) < n)
        return -1;
    return offsetof(struct sockaddr_un, sun_path) + (socklen_t) n;
}

int write_string(int fd, const char *str)
{
    size_t chars = strlen(str);
    ssize_t n;

    do {
        n = write(fd, str, chars);
        if (n == -1)
            return -1;
        str += n;
        chars -= n;
    } while (chars > 0);
    return 0;
}
