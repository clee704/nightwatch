#include <sys/types.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <sys/stat.h>
#include <sys/un.h>
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
void send_host_info(int);
void send_ok(int);
void *sleep_listener(void*);
void *request_handler(void*);
