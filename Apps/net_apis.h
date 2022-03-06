/* This header lists all the Network interfacing API's */
#ifndef NET_APIS_H
#define NET_APIS_H

int get_mac_addr(const char *iface, char *macaddr);
int set_mac_addr(const char *iface, const char *macaddr);
int get_ip_addr(const char *iface, char *ipaddr, char *nmask);
int set_ip_addr(const char *iface, const char *ipaddr, const char *nmask);
int get_if_state(const char *iface, int *up, int *promisc);
int set_if_state(const char *iface, const int *up, const int *promisc);
int get_if_index(const char *iface, int *iface_index);
int set_if_name(const char *iface, const char *iface_new);
int tx_pkt(const char *iface, const unsigned char *pkt, int len);
int rx_pkt(const char *iface, unsigned char *pkt, int len);

#endif
