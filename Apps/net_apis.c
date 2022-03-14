/* This file contains the Network interfacing APIs */
/*
 * References:
 * man netdevice - for interface configuration
 * man packet - for packet communication
 * man inet_ntoa - for IP related translations
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/if_arp.h> // ARP i/f
#include <arpa/inet.h> // hton...
#include <netinet/in.h>
#include <linux/if_packet.h> // Packet i/f
#include <net/ethernet.h> // L2 Protocols

int get_mac_addr(const char *iface, char *macaddr)
{
	int fd;
	struct ifreq ifr;
	unsigned char *mac;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	if (ioctl(fd, 0 /* TODO 1: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl get i/f mac addr");
		close(fd);
		return -1;
	}
	mac = ifr.ifr_hwaddr.sa_data;
	sprintf(macaddr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	close(fd);

	return 0;
}
int set_mac_addr(const char *iface, const char *macaddr)
{
	int fd;
	struct ifreq ifr;
	unsigned char *mac;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	mac = ifr.ifr_hwaddr.sa_data;
	sscanf(macaddr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
	if (ioctl(fd, 0 /* TODO 2: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl set i/f mac addr");
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}
int get_ip_addr(const char *iface, char *ipaddr, char *nmask)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_in *saddr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	if (ioctl(fd, 0 /* TODO 3: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl get i/f ip addr");
		close(fd);
		return -1;
	}
	saddr = (struct sockaddr_in *)(&ifr.ifr_addr);
	strcpy(ipaddr, inet_ntoa(saddr->sin_addr));
	if (nmask)
	{
		strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
		if (ioctl(fd, 0 /* TODO 4: Fill in the correct command */, &ifr) == -1)
		{
			perror("ioctl get i/f net mask");
			close(fd);
			return -1;
		}
		saddr = (struct sockaddr_in *)(&ifr.ifr_netmask);
		strcpy(nmask, inet_ntoa(saddr->sin_addr));
	}
	close(fd);

	return 0;
}
int set_ip_addr(const char *iface, const char *ipaddr, const char *nmask)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_in *saddr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	saddr = (struct sockaddr_in *)(&ifr.ifr_addr);
	saddr->sin_family = AF_INET;
	inet_aton(ipaddr, &saddr->sin_addr);
	if (ioctl(fd, 0 /* TODO 5: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl set i/f ip addr");
		close(fd);
		return -1;
	}
	if (nmask)
	{
		strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
		saddr = (struct sockaddr_in *)(&ifr.ifr_netmask);
		saddr->sin_family = AF_INET;
		inet_aton(nmask, &saddr->sin_addr);
		if (ioctl(fd, 0 /* TODO 6: Fill in the correct command */, &ifr) == -1)
		{
			perror("ioctl set i/f net mask");
			close(fd);
			return -1;
		}
	}
	close(fd);

	return 0;
}
int get_if_state(const char *iface, int *up, int *promisc)
{
	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	if (ioctl(fd, 0 /* TODO 7: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl get i/f state");
		close(fd);
		return -1;
	}
	if (up)
	{
		*up = !!(ifr.ifr_flags & IFF_UP);
	}
	if (promisc)
	{
		*promisc = !!(ifr.ifr_flags & IFF_PROMISC);
	}
	close(fd);

	return 0;
}
int set_if_state(const char *iface, const int *up, const int *promisc)
{
	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1)
	{
		perror("ioctl get i/f state");
		close(fd);
		return -1;
	}
	if (up)
	{
		if (*up)
		{
			ifr.ifr_flags |= IFF_UP;
			// TODO: ip addr show still shows state UNKNOWN. Why? IFF_RUNNING doesn't help
		}
		else
		{
			ifr.ifr_flags &= ~IFF_UP;
		}
	}
	if (promisc)
	{
		if (*promisc)
		{
			ifr.ifr_flags |= IFF_PROMISC;
		}
		else
		{
			ifr.ifr_flags &= ~IFF_PROMISC;
		}
	}
	if (ioctl(fd, 0 /* TODO 8: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl set i/f state");
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}
int get_if_index(const char *iface, int *iface_index)
{
	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	if (ioctl(fd, 0 /* TODO 9: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl get i/f index");
		close(fd);
		return -1;
	}
	*iface_index = ifr.ifr_ifindex;
	close(fd);

	return 0;
}
int set_if_name(const char *iface, const char *iface_new)
{
	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	strncpy(ifr.ifr_newname, iface_new, IFNAMSIZ - 1);
	if (ioctl(fd, 0 /* TODO 10: Fill in the correct command */, &ifr) == -1)
	{
		perror("ioctl set i/f name");
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}
int tx_pkt(const char *iface, const unsigned char *pkt, int len)
{
	int fd, iface_index;
	struct sockaddr_ll my_addr;
	int bytes;

	if ((fd = socket(AF_PACKET, SOCK_RAW, 0)) == -1)
	{
		perror("socket");
		return -1;
	}
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sll_family = AF_PACKET; // Always
	if (get_if_index(iface, &iface_index) == -1)
	{
		close(fd);
		return -1;
	}
	my_addr.sll_ifindex = iface_index;
	if ((bytes = sendto(fd, pkt, len, 0, (struct sockaddr *)(&my_addr), sizeof(my_addr))) == -1)
	{
		perror("send");
	}
	close(fd);

	return bytes;
}
int rx_pkt(const char *iface, unsigned char *pkt, int len)
{
	int fd, iface_index;
	struct sockaddr_ll my_addr;
	int bytes;

	if ((fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1)
	{
		perror("socket");
		return -1;
	}
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sll_family = AF_PACKET; // Always
	my_addr.sll_protocol = htons(ETH_P_ALL);
	if (get_if_index(iface, &iface_index) == -1)
	{
		close(fd);
		return -1;
	}
	my_addr.sll_ifindex = iface_index;
	if (bind(fd, (struct sockaddr *)(&my_addr), sizeof(my_addr)) == -1)
	{
		perror("bind");
		close(fd);
		return -1;
	}
	if ((bytes = recv(fd, pkt, len, 0)) == -1)
	{
		perror("recv");
	}
	close(fd);

	return bytes;
}
