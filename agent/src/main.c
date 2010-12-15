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
	pthread_t p_thread[2];
	int thr_id;
	int a=1;
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
	

	//make thread
	if( pthread_create(&p_thread[0], NULL, sleep_listener, NULL))
	{
		//thread error
		printf("thread make error\n");
	}
	if ( pthread_create(&p_thread[1], NULL, request_handler, NULL)){
		printf("thread make error2\n");
	}
	pthread_join(p_thread[0], NULL);
	pthread_join(p_thread[1], NULL);

	close(STDIN_FILENO);
	close(STDERR_FILENO);
//	close(STDOUT_FILENO);
	exit(EXIT_SUCCESS);

}
void *
request_handler()
{
	while(1){
		sleep(1);
	}
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
	system("pm-suspend");
	return 0;
}
void *
sleep_listener()
{
	DBusConnection *connection;
	DBusMessage *msg;
	DBusError error;
	int ret;
	

	dbus_error_init(&error);

	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

	if( connection == NULL )
	{
		printf("Failed to open connection to bus: %s\n",
				error.message);
		dbus_error_free(&error);
		return ;
	}
	
	ret = dbus_bus_request_name(connection, "nitch.agent.signal.sink", DBUS_NAME_FLAG_REPLACE_EXISTING , &error);
	if( dbus_error_is_set(&error)){
		printf("Name Error %s\n", error.message);
		dbus_error_free(&error);
		return ;
	}
	dbus_bus_add_match(connection, "type='signal',interface='agent.signal.Type'", &error);
	if(dbus_error_is_set(&error)){
		printf("Match Error %s\n", error.message);
		return ;
	}

	while(1)
	{
		dbus_connection_read_write(connection, 0);
		msg = dbus_connection_pop_message(connection);

		if (NULL== msg)
		{
			sleep(0.1);
			continue;
		}
		if(dbus_message_is_signal(msg, "agent.signal.Type","Test")){
			//do something here
			system("echo -n 'test' > /home/ninkin/test");
			printf("gotit\n");
		}
	}
}
