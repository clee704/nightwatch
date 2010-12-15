#include "main.h"
#define log_location "/var/log"
#define daemon_name "nitch_agent"
char server_ip[20];
char server_hwip[30];
int server_port = 5678;

enum REQ {SUSP, NTFY, STAT};
int
main (int argc, char **argv)
{
	pid_t pid, sid;
	
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}
	umask(0);
	//open log file
	sid = setsid();
	if (sid < 0) {
		exit(EXIT_FAILURE);
	}
	if ((chdir("/")) < 0) {
		exit(EXIT_FAILURE);
	}
	read_config();

	close(STDIN_FILENO);
	close(STDERR_FILENO);
	close(STDOUT_FILENO);
	while (1) {
		sleep(1);
	}
	exit(EXIT_SUCCESS);

}
void
send_host_name(int serverfd)
{
	char hostname[100];
	gethostname(hostname, 100);
	write(serverfd, hostname, 100);
}
void
read_config()
{
	char buf[100];
	FILE *conf = fopen("/etc/nitch_agent.conf", "r");
	if ( conf < 0) 
	{
	}

	//read ip addr
	fgets(buf, 100, conf);
	strncpy(server_ip, buf, strlen(buf));
	
	//read port
	fgets(buf, 100, conf);
	server_port = atoi(buf);

	//read hwaddr
	fgets(buf, 100, conf);
	strncpy(server_hwip, buf, strlen(buf));
}

int 
write_log (FILE *logf, char *log_message)
{
	fwrite(log_message, strlen(log_message), 1, logf);
}
int
make_connect(char *server, int port)
{
	int client_socket;
	client_socket = socket(PF_INET, SOCK_STREAM, 0);
	if( -1 == client_socket)
	{
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in server_addr;
	memset( &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(server);
	if( -1 == connect( client_socket, (struct sockaddr*)&server_addr, sizeof( server_addr)))
	{
		exit(EXIT_FAILURE);
	}
	return client_socket;
}


int
go_to_sleep ()
{
	system("echo -n mem > /sys/power/state");
	return 0;
}
