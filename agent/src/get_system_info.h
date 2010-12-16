#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
struct _CpuStat
{
	unsigned int User;
	unsigned int Nice;
	unsigned int System;
	unsigned int Idle;
};

double get_cpu_usage();
void get_hwaddr(char*);
