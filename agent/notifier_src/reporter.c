#include <sys/types.h> 
#include <syslog.h>
#include <sys/stat.h> 
#include <sys/socket.h> 
#include <unistd.h> 
#include <sys/un.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 

int
main(int argc, char **argv){
	int reporter_fd;

	struct sockaddr_un reporter_addr;
	syslog(LOG_DEBUG, "sleep is detected");

	if((reporter_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		syslog(LOG_ERR, "reporter socket open error");
	}

	bzero(&reporter_addr, sizeof(reporter_addr));
	reporter_addr.sun_family = AF_UNIX;
	strcpy(reporter_addr.sun_path, "/tmp/nitchsocket");
	if(connect(reporter_fd, (struct sockaddr *)&reporter_addr, sizeof(reporter_addr))< 0){
		syslog(LOG_CRIT, "reporter connection error %m");
		return 0;
	}
	syslog(LOG_DEBUG, "connection estabilished");
	
	close(reporter_fd);

	return 0;
}
