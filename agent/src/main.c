#include "main.h"
#define log_location "/var/log"
#define daemon_name "nitch_agent"
#define server_ip 147.46.242.78
#define server_port 5678
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
	close(STDIN_FILENO);
	close(STDERR_FILENO);
	close(STDOUT_FILENO);
	while (1) {
		sleep(1);
	}
	exit(EXIT_SUCCESS);

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
