/*
 * This structure is used to store the user time, nice time, system time and idle time.
 */
struct _CpuStat
{
	unsigned int User;
	unsigned int Nice;
	unsigned int System;
	unsigned int Idle;
};

/**
 * Return the hardware address of this agent machine.
 * This function is called, after connection with proxy server estabilished.
 *
 */
double get_cpu_usage();


/**
 * Return the cpu usage in percentage term by using /proc/stat.
 * It takes 1 second to get cpu usage.
 * This function works regardless of number of cpus.
 */
void get_hwaddr(char*);
