#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>
#include <pthread.h>

#include "network.h"
#include "protocol.h"
#include "get_system_info.h"

#define BACKLOG_SIZE 64
#define UDP_PATH "/tmp/nitchsocket"
//TODO open port 4444 request
//ARP poisoning 
//sleep marking for re connection after awake
int make_connect(char *, int);
int go_to_sleep();
time_t sleep_time;
void read_config();
void send_host_info(int);
void send_ok(int);
void *sleep_listener();
void *request_handler();
void *time_stamper();
#define connection_retry_num 10
char server_ip[100];
int server_port;
int socket_to_listen;
int socket_to_tell;
pthread_t p_thread[3];
int
main (int argc, char **argv)
{
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

	initialize();

	if( pthread_create(&p_thread[0], NULL, sleep_listener, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}
	if ( pthread_create(&p_thread[1], NULL, request_handler, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}
	if ( pthread_create(&p_thread[2], NULL, time_stamper, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}

	pthread_join(p_thread[0], NULL);
	pthread_join(p_thread[1], NULL);
	pthread_join(p_thread[2], NULL);


	close(STDIN_FILENO);
	close(STDERR_FILENO);
	close(STDOUT_FILENO);
	close(socket_to_tell);
	close(socket_to_listen);
	exit(EXIT_SUCCESS);

}
void
initialize()
{
	int i;
	for(i = 0 ; i < connection_retry_num &&
			((socket_to_tell = make_connect(server_ip, server_port)) < 0); i++) {
		//retry connect in 1 second
		sleep(1);		
	}
	if( connection_retry_num == i ) {
		syslog(LOG_ERR, "connection fail");
		exit(EXIT_FAILURE);
	}
	///////////////////////
	
	struct sockaddr_in addr;
	set_sockaddr_in(&addr, AF_INET, 4444, INADDR_ANY);
	int sock = init_server(SOCK_STREAM, &addr, sizeof(addr), BACKLOG_SIZE);

	if ( sock < 0) {
		syslog(LOG_ERR, "can't listen on port %d: %m", 4444);
		return -1;
	}
	struct sockaddr_in proxy_addr;
	socklen_t addr_len;

	addr_len = sizeof(proxy_addr);
	socket_to_listen = accept(sock, (struct sockaddr *)&proxy_addr, &addr_len);
	if (socket_to_listen < 0){
		syslog(LOG_ERR, "accept() failed: %m");
	}

	// send host information
	send_host_info(socket_to_tell);

	//initialize time stamp
	time(&sleep_time);
	syslog(LOG_DEBUG, "initialization complete");
}
	
void *
time_stamper()
{
	time_t now;
	while(1){
		sleep(1);
		time(&now);
		if(now - sleep_time > 5){
			close(socket_to_tell);
			close(socket_to_listen);

			initialize();
			//i slept
		}
		time(&sleep_time);
	}
}
void *
request_handler()
{
	struct response res;
	struct request req;
	char req_buf[MAX_REQUEST_STRLEN];
	char resp_buf[MAX_RESPONSE_STRLEN];
	while(1){
		if(read(socket_to_listen, req_buf, MAX_REQUEST_STRLEN)==0){
			//////////////////
		};
		parse_request(req_buf, &req);

		switch(req.method){
			case SUSP:
				respond(socket_to_tell, 200, resp_buf, &res);
				go_to_sleep();
				break;
			case PING:
				respond(socket_to_tell, 200, resp_buf, &res);
				break;
		}
	}
}
void
request(int fd, int method, char *char_buf, struct request *req_buf)
{
	int len;
	req_buf->method = method;
	req_buf->has_uri = 0;
	req_buf->has_data = 0;
	len = serialize_request(req_buf, char_buf);
	if (len < 0)
		syslog(LOG_WARNING, "serialize_request() failed");
	if (write_string(fd, char_buf))
		syslog(LOG_WARNING, "can't write: %m");
}
void
respond(int fd, int status, char *char_buf, struct response *resp_buf)
{
	int len;
	const char *msg="";
	switch(status){
		case 200: msg = "OK"; break;
	}
	resp_buf->status = status;
	strcpy(resp_buf->message, msg);
	resp_buf->has_data = 0;
	len =	serialize_response(resp_buf, char_buf);
	if(len< 0)
		syslog(LOG_WARNING, "serialize_response() failed");
	if(write_string(fd, char_buf))
		syslog(LOG_WARNING, "can't write: %m");
}
void
send_host_info(int fd)
{
	char hostname[100];
	char hwaddr[20];
	char buf[200];
	struct request req;
	gethostname(hostname, 100);
	get_hwaddr(hwaddr);
	sprintf(buf,"%s\n%s", hostname, hwaddr);

	request(fd, INFO, buf, &req);
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
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if( -1 == server_socket){
		syslog(LOG_ERR, "socket open error. retry in 1 second");
		return -1;
	}
	struct sockaddr_in addr;
	set_sockaddr_in(&addr, AF_INET, port, sizeof(addr));
	if( -1 == connect( server_socket, (struct sockaddr*)&addr, sizeof(addr))){ 
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
sleep_listener()
{
	
	struct sockaddr_un my_addr, reporter_addr;
	int my_sockfd, reporter_sockfd;

	while(1){
		//use unix domain socket
		if((my_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
			syslog(LOG_ERR, "socket open error");
		}

		set_sockaddr_un(&my_addr, UDP_PATH);

		unlink(my_addr.sun_path);
		if(bind(my_sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0){
			syslog(LOG_ERR, "socket bind error");
		}
		syslog(LOG_DEBUG, "listening on %s", UDP_PATH);
		listen(my_sockfd, 5);

		//if anyone connect to socket file,
		//	we accept and assume this is sleeping condition
		int clilen = sizeof(reporter_addr);
		reporter_sockfd = accept(my_sockfd, (struct sockaddr *)&reporter_addr, &clilen);

		syslog(LOG_NOTICE, "got sleep signal. send notification message to server");

		char req_buf[MAX_REQUEST_STRLEN];
		struct request req;
		request(socket_to_tell, NTFY, req_buf, &req);
		
		close(reporter_sockfd);
		close(my_sockfd);
	}
}
