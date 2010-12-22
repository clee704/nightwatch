#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>

#include <pcap.h>

#include "packet_monitor.h"
#include "agent.h"
#include "logger.h"
#include "resume_agent.h"

#define LOGGER_PREFIX "[packet_monitor] "

struct agent_syn {
    struct agent *agent;
    char *packet;
    int packet_size;
};

static void *monitor_packets(void *agent_list);
static int syn_detect(struct agent_syn *);
static int forward_syn_packet(struct agent_syn *);

int start_packet_monitor(pthread_t *tid, struct agent_list *list)
{
    if (pthread_create(tid, NULL, monitor_packets, (void *) list)) {
        ERROR("can't create a thread: %m");
        return -1;
    }
    return 0;
}

static void *monitor_packets(void *agent_list)
{
    struct agent_list *agentList = (struct agent_list *) agent_list;
    pcap_t                         *handle;        /* Session handle */
    char                         *dev;            /* The device to sniff on */
    char                         errbuf[PCAP_ERRBUF_SIZE]; /*Err string */
    bpf_u_int32                 mask;            /* Our netmask */
    bpf_u_int32                 net;            /* Our IP */
    struct pcap_pkthdr             header;            /* Header that pcap gives us */
    const u_char                 *packet;        /* The actual packet */

    const struct ether_header     *ether_header;     /* The Ethernet header */
    const struct ip             *ip;             /* The IP header */
    const struct tcphdr         *tcphdr;         /* The TCP header */
    //const char                     *payload;         /* Packet payload */


    /* Used to filter packets */
    char                         *proto;
    //u_short                     src_port;
    //u_short                     dst_port;

    /* Sizes of some relevant headers */
    int size_ethernet             = sizeof(struct ether_header); 
    int size_ip                 = sizeof(struct ip);
    //int size_tcp                 = sizeof(struct tcphdr);

    /* Setup Pcap - packet capture interface */
    /* Define the device */
    dev = pcap_lookupdev(errbuf);
    //DEBUG("Listening on %s.", dev);

    /* Find the properties for the device */
    pcap_lookupnet(dev, &net, &mask, errbuf);

    /* Open the session in promiscuous mode */
    handle = pcap_open_live(dev, BUFSIZ, 1, 0, errbuf);

    /* You decide when to break */

    //int state = 0;
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
                    //DEBUG("count = %d\n",count);
                    //DEBUG("%s : %d    ======>  ", inet_ntoa(ip->ip_src), ntohs(tcphdr->source));
                    //DEBUG("%s : %d \n", inet_ntoa(ip->ip_dst),  ntohs(tcphdr->dest));
                    //DEBUG("adsfsad\n");
                    //DEBUG("%x\n",ip->ip_dst.s_addr);

                    struct agent *agent = find_agent_by_ip( agentList, &(ip->ip_dst) );
                    if( agent != NULL && agent->state != UP)
                    {
                        //DEBUG("find agent");
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
    return NULL;
}

static int syn_detect(struct agent_syn *agent_syn)
{
    pthread_t tid;
    if( agent_syn->agent->state == SUSPENDED ) {
        //create wakeup thread
        if (resume_agent(agent_syn->agent))
            return -1;
    }
    if( pthread_create(&tid, NULL, (void * (*)(void *))forward_syn_packet, agent_syn) < 0) {
        return -1;
    }
    return 0;
}

static int forward_syn_packet(struct agent_syn *agent_syn)
{
    int sock;
    struct sockaddr_ll dst_sockaddr;

    const char *ifname = "eth0";

    while( agent_syn->agent->state != UP ) {
        sleep(1);
    }

    //DEBUG("forward_syn_packet");

    sock = socket(PF_PACKET, SOCK_RAW, 0);
    if( sock < 0 )
    {
        //printf("socket error\n");
        return -1;
    }


    if( setuid(getuid()) < 0) {
        //printf("setuid error\n");
        return -1;
    }


    // Make dst_sockaddr for sendto()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
            //printf("ioctl error\n");
            return -1;
        }

        memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
        dst_sockaddr.sll_family = AF_PACKET;
        dst_sockaddr.sll_ifindex = ifr.ifr_ifindex;
        dst_sockaddr.sll_halen = 6;
        memcpy(dst_sockaddr.sll_addr, &(agent_syn->agent->mac) , 6);
        //memcpy(dst_sockaddr.sll_addr, "\xff\xff\xff\xff\xff\xff" , 6);
    }

	memcpy( agent_syn->packet, &(agent_syn->agent->mac), 6);

    if(sendto(sock, agent_syn->packet, agent_syn->packet_size, 0, (struct sockaddr *)&dst_sockaddr, sizeof(dst_sockaddr)) < 0) 
    {

        //printf("sendto error: %m\n");
        //printf("%d\n",agent_syn->packet_size);
        return -1;
    }

    //printf("forwarding syn packet\n");

    free(agent_syn->packet);
    free(agent_syn);
    return 0;    
}
