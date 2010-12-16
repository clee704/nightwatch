#include "main.h"
#define daemon_name "nitch_agent"
#define connection_retry_num 10
char server_ip[100];
int server_port;
enum REQ {SUSP, NTFY, STAT};
int
main (int argc, char **argv)
{
	pthread_t p_thread[2];
	int thr_id;
	int server_socket;
	pid_t pid, sid;

	if(geteuid() != 0) {
		fprintf(stderr, "%s: must be run as root or setuid root\n", "nitch_agent");
		exit(EXIT_SUCCESS);
	}
	//initialize to make daemon process	
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

	//make thread to listen sleeping signal and handle proxy server command
	if( pthread_create(&p_thread[0], NULL, sleep_listener, (void*)server_socket)){
		syslog(LOG_ERR, "daemon can't make thread");
	}
	if ( pthread_create(&p_thread[1], NULL, request_handler, (void*)server_socket)){
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

	// send host information
	send_host_info(server_socket);

	pthread_join(p_thread[0], NULL);
	pthread_join(p_thread[1], NULL);

	close(STDIN_FILENO);
	close(STDERR_FILENO);
	close(STDOUT_FILENO);
	close(server_socket);
	exit(EXIT_SUCCESS);

}
void *
request_handler(void *socket)
{
	syslog(LOG_DEBUG, "start request handler");
	char buf[100];
	int server_socket = (int)socket;
	while(1){
		read(server_socket, buf, 100);
		if(strncmp(buf, "SUSP\n", 5) == 0){
			send_ok(server_socket);
			go_to_sleep();
		}
		else if(strncmp(buf, "PING\n", 5) == 0){
			send_ok(server_socket);
		}
	}
}
void
send_ok(int server_socket)
{
	char buf[10];
	strncpy(buf,"200 OK\n", 7);
	write(server_socket, buf, strlen(buf));
	syslog(LOG_INFO, "agent sent OK message");
}
void
send_host_info(int serverfd)
{
	char hostname[100];
	char hwaddr[20];
	char buf[200];
	gethostname(hostname, 100);
	get_hwaddr(hwaddr);
	sprintf(buf,"INFO\n\n%s\n%s\n\n", hostname, hwaddr);
	
	if( write(serverfd, buf, strlen(buf)) < 0 ){
		syslog(LOG_WARNING, "can't send host name to server");
	}
	else{
		syslog(LOG_INFO, "host name and mac was successfully sent : %s", buf);
	}
}
void
read_config()
{
	char buf[100];
	FILE *conf = fopen("/etc/nitch_agent.conf", "r");
	if ( conf < 0) {
		syslog(LOG_ERR, "can't open configuration file.");
		exit(EXIT_FAILURE);
	}
	else{
		fgets(buf, 100, conf);
		strncpy(server_ip, buf, strlen(buf)-1);
		fgets(buf, 100, conf);
		server_port = atoi(buf);
		syslog(LOG_INFO, "use %s on port %d", server_ip, server_port);
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
	//use pm-suspend command to sleep
	syslog(LOG_NOTICE, "this agent machine is going to sleep");
	system("pm-suspend");
	return 0;
}
void *
sleep_listener(void *socketfd)
{
	int server_socket = (int)socketfd;
	char buf[10];
	
	struct sockaddr_un my_addr, reporter_addr;
	int my_sockfd, reporter_sockfd;

	//use unix domain socket
	if((my_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		syslog(LOG_ERR, "socket open error");
	}
	bzero(&my_addr, sizeof(my_addr));
	my_addr.sun_family = AF_UNIX;
	strcpy(my_addr.sun_path, "/tmp/nitchsocket");

	unlink(my_addr.sun_path);
	if(bind(my_sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0){
		syslog(LOG_ERR, "socket bind error");
	}
	syslog(LOG_DEBUG, "listening on /tmp/nitchsocket");
	listen(my_sockfd, 5);

	//if anyone connect to socket file,
	//	we accept and assume this is sleeping condition
	int clilen = sizeof(reporter_addr);
	reporter_sockfd = accept(my_sockfd, (struct sockaddr *)&reporter_addr, &clilen);

	syslog(LOG_NOTICE, "got sleep signal. send notification message to server");
	write(server_socket, "NTFY\n", 5);
	
	close(reporter_sockfd);
	close(my_sockfd);
}
