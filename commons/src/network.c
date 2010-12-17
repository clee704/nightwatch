#include <string.h>
#include <unistd.h>
#include "network.h"

int init_server(int type, const struct sockaddr *addr, socklen_t alen, int qlen)
{
    int fd = socket(addr->sa_family, type, 0);

    if (fd < 0)
        return -1;
    if (bind(fd, addr, alen))
        goto error;
    if ((type == SOCK_STREAM || type == SOCK_SEQPACKET) && listen(fd, qlen))
        goto error;
    return fd;
error:
    close(fd);
    return -1;
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
