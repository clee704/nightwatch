#include "get_system_info.h"

struct _CpuStat CpuStat[2];

int cidx = 0;

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
