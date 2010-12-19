struct in_addr;
struct ether_addr;

int send_poison_packet(struct in_addr *ip_addr,struct ether_addr *mac_addr, const char *ifname_in) ;
