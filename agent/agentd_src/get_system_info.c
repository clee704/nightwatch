#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "get_system_info.h"

struct _CpuStat CpuStat[2];

void *_cpu_usage();

void
get_hwaddr(char *hw_address)
{
	int fd;
	struct ifreq ifr;
	
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

	ioctl(fd, SIOCGIFHWADDR, &ifr);
	close(fd);

	sprintf(hw_address,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			(unsigned char)ifr.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr.ifr_hwaddr.sa_data[5]);
	return ;
}

double 
get_cpu_usage()
{
	pthread_t p_thread;
	double cpu_use;
	pthread_create(&p_thread, NULL, _cpu_usage, NULL);
	pthread_join(p_thread, (void *)&cpu_use);
	return cpu_use;
}
void *
_cpu_usage()
{
	FILE *stat_fp;
	char buf[80];
	char cpuid[8];

	int i;
	for(i = 0; i < 2; i++){
		stat_fp = fopen("/proc/stat","r");
		while( fgets(buf, 80, stat_fp))
		{
			if (strncmp(buf, "cpu ", 4) == 0)
			{
				sscanf( buf, "%s %u %u %u %u", cpuid, 
						&CpuStat[i].User, 
						&CpuStat[i].Nice, 
						&CpuStat[i].System, 
						&CpuStat[i].Idle
						);
				break;
			}
		}
		fclose(stat_fp);
		sleep(1);
	}
	int diff1, diff2, diff3, diff4;
	diff1 = CpuStat[1].User - CpuStat[0].User;
	diff2 = CpuStat[1].Nice - CpuStat[0].Nice;
	diff3 = CpuStat[1].System- CpuStat[0].System;
	diff4 = CpuStat[1].Idle- CpuStat[0].Idle;

	return (void *)((diff1+diff2+diff3)*100/(diff1+diff2+diff3+diff4));
}
