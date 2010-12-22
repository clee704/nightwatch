#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>
#include <pthread.h>

#include "daemon.h"
#include "network.h"
#include "protocol.h"
#include "poison.h"
#include "get_system_info.h"

#define BACKLOG_SIZE 64
#define UDP_PATH "/tmp/nitchsocket"
void get_option(int, char**);
void initialize();

int make_connect(char *, int);
void go_to_sleep();
void send_host_info(int);
void send_ok(int);
void *sleep_listener();
void *request_handler();
void *time_stamper();
void *still_alive();
void request(int, int, char *, struct request *);
void respond(int, int, char *, struct response *);

#define connection_retry_num 10
time_t sleep_time;
char *server_ip;
int server_port;
int socket_response;
int socket_request;
int sock;
int sleeping = 0;
/*
 * p_thread[0] sleep_listener
 * p_thread[1] request_handler
 * p_thread[2] still_alive
 * p_thread[3] time_stamper
 */
pthread_t p_thread[4];

int
main (int argc, char **argv)
{
	get_option(argc, argv);

	daemonize("nitch-agentd");

	struct sockaddr_in addr;
	set_sockaddr_in(&addr, AF_INET, 4444, INADDR_ANY);
	sock = init_server(SOCK_STREAM, (struct sockaddr *) &addr, sizeof(addr), BACKLOG_SIZE);

	if ( sock < 0) {
		syslog(LOG_ERR, "can't listen on port %d: %m", 4444);
		exit(EXIT_FAILURE);
	}
	syslog(LOG_DEBUG, "listening on 4444");

	initialize();

	if ( pthread_create(&p_thread[2], NULL, still_alive, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}
	if ( pthread_create(&p_thread[3], NULL, time_stamper, NULL)){
		syslog(LOG_ERR, "can't make thread");
	}

	pthread_join(p_thread[0], NULL);
	pthread_join(p_thread[1], NULL);
	pthread_join(p_thread[2], NULL);
	pthread_join(p_thread[3], NULL);


	close(STDIN_FILENO);
	close(STDERR_FILENO);
	close(STDOUT_FILENO);
	close(socket_request);
	close(socket_response);
	close(sock);
	exit(EXIT_SUCCESS);

}
void
get_option(int argc, char **argv)
{
	if(argc < 3){
		printf("usage:\nnitch-agentd [proxy-ip] [port]\n");
		exit(EXIT_FAILURE);
	}
	server_ip = argv[1];
	server_port = atoi(argv[2]);
}
void
initialize()
{
	int i;
	for(i = 0 ; i < connection_retry_num &&
			((socket_request = make_connect(server_ip, server_port)) < 0); i++) {
		//retry connect in 1 second
		sleep(1);		
	}
	if( connection_retry_num == i ) {
		syslog(LOG_ERR, "connection fail");
		exit(EXIT_FAILURE);
	}
	syslog(LOG_DEBUG, "socket to tell estabilised");
	send_host_info(socket_request);
	///////////////////////
	
	struct sockaddr_in proxy_addr;
	socklen_t addr_len;

	addr_len = sizeof(proxy_addr);
	socket_response = accept(sock, (struct sockaddr *)&proxy_addr, &addr_len);
	syslog(LOG_DEBUG, "socket to listen estabilised");
	if (socket_response < 0){
		syslog(LOG_ERR, "accept() failed: %m");
	}

	if( pthread_create(&p_thread[0], NULL, sleep_listener, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}
	if ( pthread_create(&p_thread[1], NULL, request_handler, NULL)){
		syslog(LOG_ERR, "daemon can't make thread");
	}


	///initialize time stamp
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
		if(sleeping && now - sleep_time > 5){
			syslog(LOG_DEBUG, "i slept");

			struct ether_addr hwaddr;
			struct in_addr  ipaddr;
			get_hwaddr(&hwaddr);
			get_ipaddr(&ipaddr);
			send_poison_packet(&ipaddr,&hwaddr, NULL);

			initialize();
			sleeping = 0;
			//i slept
		}
		time(&sleep_time);
	}
}
void *
still_alive()
{
	while(1){
		if (!sleeping)
			write(socket_request, "PING\n", 6);
		sleep(5);
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
		if(read(socket_response, req_buf, MAX_REQUEST_STRLEN)==0){
			syslog(LOG_DEBUG, "return value of read is 0");
			//prepare for sleeping
			pthread_cancel(p_thread[2]);
			close(socket_response);
			close(socket_request);
			break;
		};
		syslog(LOG_DEBUG,"received :%s",req_buf);
		parse_request(req_buf, &req);

		switch(req.method){
			case SUSP:
				respond(socket_response, 200, resp_buf, &res);
				go_to_sleep();
				break;
			case PING:
				respond(socket_response, 200, resp_buf, &res);
				break;
			case GETA:
			case RSUM:
			case INFO:
			case NTFY:
				break;
		}
		if(req.method == SUSP){
			break;
		}
	}
	return NULL;
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
	char hwaddrbuf[20];
	char buf[200];
	//struct request req;
	struct ether_addr hwaddr;
	gethostname(hostname, 100);
	get_hwaddr(&hwaddr);
	sprintf(hwaddrbuf,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			(unsigned char)hwaddr.ether_addr_octet[0],
			(unsigned char)hwaddr.ether_addr_octet[1],
			(unsigned char)hwaddr.ether_addr_octet[2],
			(unsigned char)hwaddr.ether_addr_octet[3],
			(unsigned char)hwaddr.ether_addr_octet[4],
			(unsigned char)hwaddr.ether_addr_octet[5]);
	sprintf(buf,"INFO\n\n%s\n%s\n\n", hostname, hwaddrbuf);
	write(fd, buf, 200);
	syslog(LOG_DEBUG, "name and hw are sent %s",buf);
	//request(fd, INFO, buf, &req);
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
	set_sockaddr_in(&addr, AF_INET, port, inet_addr(server));
	if( -1 == connect( server_socket, (struct sockaddr*)&addr, sizeof(addr))){ 
		syslog(LOG_ERR, "can't connect to server. retry in 1 seconnd");
		return -1;
	}
	return server_socket;
}


void
go_to_sleep ()
{
	sleeping = 1;
	//use pm-suspend command to sleep
	syslog(LOG_NOTICE, "this agent machine is going to sleep");
	system("pm-suspend");
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
		socklen_t clilen = sizeof(reporter_addr);
		reporter_sockfd = accept(my_sockfd, (struct sockaddr *)&reporter_addr, &clilen);

		syslog(LOG_NOTICE, "got sleep signal. send notification message to server");

		char req_buf[MAX_REQUEST_STRLEN];
		struct request req;
		request(socket_request, NTFY, req_buf, &req);
		
		close(reporter_sockfd);
		close(my_sockfd);
	}
}
