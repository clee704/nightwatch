#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "get_system_info.h"

int make_connect(char *, int);
int go_to_sleep();
