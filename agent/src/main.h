#include <sys/types.h>
#include <pthread.h>
#include <dbus/dbus.h>
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
void read_config();
void send_host_name(int);
void *sleep_listener();
void *request_handler();
