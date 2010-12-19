
#include "monitor.h"
#include <unistd.h>
#include <net/if.h>
#include <unistd.h>

#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <pcap.h>

#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <arpa/inet.h>


#include "send_magic_packet.h"
struct agent_syn {
	struct agent *agent;
	char *packet;
	int packet_size;
};


void printChar(char *s, int len) {
	int i;
	for(i=0;i<len;i++)
	{
		if(i > 0 && i % 8 == 0)
			printf("\n");
		printf("%3x",s[i] & 0xff);
	}
	printf("\n");
}

int wakeup_agent(struct agent *agent) {
	agent->state = RESUMING;
	while( agent->state != UP ) {
		send_magic_packet( &(agent->mac), NULL );
//		sleep(5);  // for test
//		agent->state = UP; // 
//		printf("wakeup!\n");
		sleep(1);
	}
}


	
int forward_syn_packet(struct agent_syn *agent_syn) {

	int sock;
	struct sockaddr_ll dst_sockaddr;

	const char *ifname = "eth0";

	while( agent_syn->agent->state != UP ) {
		sleep(1);
	}

//	printf("forward_syn_packet\n");

	sock = socket(PF_PACKET, SOCK_RAW, 0);
	if( sock < 0 )
	{
		printf("socket error\n");
		return -1;
	}
		
	
	if( setuid(getuid()) < 0) {
		printf("setuid error\n");
		return -1;
	}
	
	
    // Make dst_sockaddr for sendto()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
			printf("ioctl error\n");
            return -1;
		}

        memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
        dst_sockaddr.sll_family = AF_PACKET;
        dst_sockaddr.sll_ifindex = ifr.ifr_ifindex;
        dst_sockaddr.sll_halen = 6;
        //memcpy(dst_sockaddr.sll_addr, &(agent_syn->agent->mac) , 6);
        memcpy(dst_sockaddr.sll_addr, "\xff\xff\xff\xff\xff\xff" , 6);
    }

	if(sendto(sock, agent_syn->packet, agent_syn->packet_size, 0, (struct sockaddr *)&dst_sockaddr, sizeof(dst_sockaddr)) < 0) 
	{
		
		printf("sendto error: %m\n");
		printf("%d\n",agent_syn->packet_size);
		return -1;
	}

//	printf("forwarding syn packet\n");
	
	free(agent_syn->packet);
	free(agent_syn);
	return 0;	
}
	
int syn_detect(struct agent_syn *agent_syn) {
	pthread_t tid[2];
	if( agent_syn->agent->state == SUSPENDED ) {
		//create wakeup thread
		if( pthread_create(&tid[0], NULL, (void * (*)(void *))wakeup_agent, agent_syn->agent )  < 0 )
			return -1;
	}
	if( pthread_create(&tid[1], NULL, (void * (*)(void *))forward_syn_packet, agent_syn) < 0) {
		return -1;
	}
}

//#define _TEST_

#ifdef _TEST_
int main(int argc, char *argv[])
{
	int retval;

	struct agent_list *list = new_agent_list();

	struct agent agents[100];

	agents[0].state = SUSPENDED;
	inet_aton("147.46.242.79", &agents[0].ip);
	agents[1].state = SUSPENDED;
	inet_aton("147.46.242.78", &agents[1].ip);
//	ether_aton("00:11:22:33:44:55", &agents[0].mac);

	add_new_agent(list, &agents[0]);
	add_new_agent(list, &agents[1]);
	retval = monitor_syn_packet(list);
	return retval;
}

#endif 

int monitor_syn_packet(struct agent_list *agentList)
{
	pcap_t 						*handle;		/* Session handle */
	char 						*dev;			/* The device to sniff on */
	char 						errbuf[PCAP_ERRBUF_SIZE]; /*Err string */
	bpf_u_int32 				mask;			/* Our netmask */
	bpf_u_int32 				net;			/* Our IP */
	struct pcap_pkthdr 			header;			/* Header that pcap gives us */
	const u_char 				*packet;		/* The actual packet */

	const struct ether_header 	*ether_header; 	/* The Ethernet header */
	const struct ip 			*ip; 			/* The IP header */
	const struct tcphdr 		*tcphdr; 		/* The TCP header */
	const char 					*payload; 		/* Packet payload */

	
	/* Used to filter packets */
	char 						*proto;
	u_short 					src_port;
	u_short 					dst_port;

	/* Sizes of some relevant headers */
	int size_ethernet 			= sizeof(struct ether_header); 
	int size_ip 				= sizeof(struct ip);
	int size_tcp 				= sizeof(struct tcphdr);
	
	/* Setup Pcap - packet capture interface */
	/* Define the device */
	dev = pcap_lookupdev(errbuf);
	printf("Listening on %s.\n", dev);

	/* Find the properties for the device */
	pcap_lookupnet(dev, &net, &mask, errbuf);

	/* Open the session in promiscuous mode */
	handle = pcap_open_live(dev, BUFSIZ, 1, 0, errbuf);

	/* You decide when to break */

	int state = 0;
	int count = 0;
	while (1)
	{
		count ++;
		packet = pcap_next(handle, &header);

		/* Get the Ethernet header */
		ether_header = (struct ether_header*)(packet);
		
		/* If we have an IP packet... */
		if (ntohs(ether_header->ether_type) == ETHERTYPE_IP) 
		{
			/* Get the IP header */
			ip = (struct ip*)(packet + size_ethernet);
			
			/* If we have a TCP packet... */
			if (ip->ip_p == IPPROTO_TCP) 
			{
				proto = "ip:tcp";

				tcphdr = (struct tcphdr*)(packet + size_ethernet + size_ip);

				if(tcphdr->syn) {
					//printf("count = %d\n",count);
					//printf("%s : %d    ======>  ", inet_ntoa(ip->ip_src), ntohs(tcphdr->source));
                    //printf("%s : %d \n", inet_ntoa(ip->ip_dst),  ntohs(tcphdr->dest));
					//printf("adsfsad\n");
					//printf("%x\n",ip->ip_dst);
					
					struct agent *agent = find_agent_by_ip( agentList, &(ip->ip_dst) );
					if( agent != NULL && agent->state != UP)
					{
						printf("find agent\n");
						struct agent_syn *temp = (struct agent_syn *)malloc(sizeof(struct agent_syn));
						temp->agent = agent;
						temp->packet_size = size_ethernet + ntohs(ip->ip_len);
						temp->packet = (char *)malloc( temp->packet_size );
						memcpy( temp->packet, packet, temp->packet_size );
						syn_detect( temp );										
					}
				}				
			}
		}
	}
	/* And close the session */
	pcap_close(handle);
	return(0);
}


     
