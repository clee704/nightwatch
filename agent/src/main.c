#include "main.h"
#define daemon_name "nitch_agent"
#define connection_retry_num 10
char server_ip[20];
char server_hwip[30];
int server_port = 5678;
int server_socket;

enum REQ {SUSP, NTFY, STAT};
int
main (int argc, char **argv)
{
	pthread_t p_thread[2];
	int thr_id;
	pid_t pid, sid;
	
	pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "daemon initialize error");
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}
	umask(0);
	sid = setsid();
	if (sid < 0) {
		syslog(LOG_ERR, "daemon initialize error");
		exit(EXIT_FAILURE);
	}
	if ((chdir("/")) < 0) {
		syslog(LOG_ERR, "daemon initialize error");
		exit(EXIT_FAILURE);
	}
	read_config();

	if( pthread_create(&p_thread[0], NULL, sleep_listener, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}
	if ( pthread_create(&p_thread[1], NULL, request_handler, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}

	int i;
	for(i = 0 ; i < connection_retry_num &&
			((server_socket = make_connect(server_ip, server_port)) < 0); i++) {
		//retry connect in 1 second
		sleep(1);		
	}
	if( connection_retry_num == i ) {
		syslog(LOG_ERR, "connection fail");
		exit(EXIT_FAILURE);
	}


	pthread_join(p_thread[0], NULL);
	pthread_join(p_thread[1], NULL);

	close(STDIN_FILENO);
	close(STDERR_FILENO);
	close(STDOUT_FILENO);
	exit(EXIT_SUCCESS);

}
void *
request_handler()
{
	char buf[100];
	while(1){
		read(server_socket, buf, 100);
		if(buf == NULL)
			sleep(1);
		else if(strncmp(buf, "SUSP", 4) == 0){
			go_to_sleep();
		}
		else if(strncmp(buf, "STAT", 4) == 0){
			sprintf(buf, "%f", get_cpu_usage());
			write(server_socket, buf, strlen(buf));
		}
	}
}
void
send_host_name(int serverfd)
{
	char hostname[100];
	gethostname(hostname, 100);
	if( write(serverfd, hostname, 100) < 0 ){
		syslog(LOG_WARNING, "can't send host name to server");
	}
	else{
		syslog(LOG_INFO, "host name was successfully sent");
	}
}
void
read_config()
{
	char buf[100];
	FILE *conf = fopen("/etc/nitch_agent.conf", "r");
	if ( conf < 0) {
		syslog(LOG_ERR, "can't open configuration file. use default option");
		strncopy(server_ip, "147.46.242.78", strlen("147.46.24.78"));
		server_port = 5678;
		strncopy(server_hwip, "20:cf:30:08:56:cd", strlen("20:cf:30:08:56:cd"));
	}
	else{
		fgets(buf, 100, conf);
		strncpy(server_ip, buf, strlen(buf));
		fgets(buf, 100, conf);
		server_port = atoi(buf);
		fgets(buf, 100, conf);
		strncpy(server_hwip, buf, strlen(buf));
		syslog(LOG_INFO, "use %s on port %d, with hwaddr %s", server_ip, server_port, server_hwip);
	}
}

int
make_connect(char *server, int port)
{
	int server_socket;
	server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if( -1 == server_socket){
		syslog(LOG_ERR, "socket open error. retry in 1 second");
		return -1;
	}
	struct sockaddr_in server_addr;
	memset( &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(server);
	if( -1 == connect( server_socket, (struct sockaddr*)&server_addr, sizeof( server_addr))){
		syslog(LOG_ERR, "can't connect to server. retry in 1 seconnd");
		return -1;
	}
	return server_socket;
}


int
go_to_sleep ()
{
	syslog(LOG_NOTICE, "this agent machine is going to sleep");
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

	if( connection == NULL ) {
		syslog(LOG_ERR, "Failed to open connection to bus: %s\n",error.message);
		dbus_error_free(&error);
		return ;
	}
	
	ret = dbus_bus_request_name(connection, "nitch.agent.signal.sink", DBUS_NAME_FLAG_REPLACE_EXISTING , &error);
	if( dbus_error_is_set(&error)){
		syslog(LOG_ERR, "Name Error %s\n", error.message);
		dbus_error_free(&error);
		return ;
	}
	dbus_bus_add_match(connection, "type='signal',interface='agent.signal.Type'", &error);
	if(dbus_error_is_set(&error)){
		syslog(LOG_ERR, "Match Error %s\n", error.message);
		return ;
	}

	while(1)
	{
		dbus_connection_read_write(connection, 0);
		msg = dbus_connection_pop_message(connection);

		if (NULL== msg) {
			sleep(0.3);
			continue;
		}
		if(dbus_message_is_signal(msg, "agent.signal.Type","Test")) {
			syslog(LOG_NOTICE, "got sleep signal. send notify message to server");
		}
	}
}
