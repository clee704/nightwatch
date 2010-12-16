#include "get_system_info.h"

struct _CpuStat CpuStat[2];

int cidx = 0;
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
	FILE *stat_fp;
	char buf[80];
	stat_fp = fopen("/proc/stat","r");
	char cpuid[8];

	while( fgets(buf, 80, stat_fp))
	{
		if (strncmp(buf, "cpu ", 4) == 0)
		{
			sscanf( buf, "%s %u %u %u %u", cpuid, 
					&CpuStat[cidx%2].User, 
					&CpuStat[cidx%2].Nice, 
					&CpuStat[cidx%2].System, 
					&CpuStat[cidx%2].Idle
					);
			break;
		}
	}
	fclose(stat_fp);
	if (!cidx)
	{
		printf("wating\n");
		sleep(1);
		cidx++;
		get_cpu_usage();
	}
	int diff1, diff2, diff3, diff4;
	int Idle, Nice, System, User;
	diff1 = CpuStat[(cidx+1)%2].User - CpuStat[cidx%2].User;
	diff2 = CpuStat[(cidx+1)%2].Nice - CpuStat[cidx%2].Nice;
	diff3 = CpuStat[(cidx+1)%2].System- CpuStat[cidx%2].System;
	diff4 = CpuStat[(cidx+1)%2].Idle- CpuStat[cidx%2].Idle;

	return (diff1+diff2+diff3)*100/(diff1+diff2+diff3+diff4);
}
