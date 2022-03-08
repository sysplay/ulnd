#include <stdio.h>
#include <string.h>
#include <linux/if.h> // for IFNAMSIZ

// For open, ...
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// For close, ...
#include <unistd.h>
// For various protocol headers and types
#include <linux/if_ether.h> // struct ethhdr, Ethernet protocol definitions
#include <linux/ip.h> // struct iphdr
#include <linux/udp.h> // struct udphdr, UDP definitions
#include <linux/tcp.h> // struct tcphdr, TCP definitions
#include <arpa/inet.h> // IP protocol definitions, hton...

#include "net_apis.h"

#define MTU 1500

int enable_ipv6(const char *iface, int yes)
{
	char fn[37 + IFNAMSIZ];
	int fd;
	char c;

	sprintf(fn, "/proc/sys/net/ipv6/conf/%s/disable_ipv6", iface);
	if ((fd = open(fn, O_WRONLY)) == -1)
	{
		perror("open");
		return -1;
	}
	c = '0' + !yes;
	if (write(fd, &c, 1) != 1)
	{
		perror("write");
		return -1;
	}
	if (close(fd) == -1)
	{
		perror("close");
		return -1;
	}
	return 0;
}
int prepare_pkt(unsigned char *pkt, int len)
{
	unsigned int parsed_hdr_size;
	struct ethhdr *eh;
	struct iphdr *ih;
	struct udphdr *uh;
	struct tcphdr *th;
	unsigned char *addr;
	int i;

	parsed_hdr_size = 0;
	eh = (struct ethhdr *)(pkt + parsed_hdr_size);
	if (len < (parsed_hdr_size + sizeof(struct ethhdr)))
	{
		printf("Insufficient Packet Length. Aborting ... \n");
		return parsed_hdr_size;
	}
	eh->h_dest[0] = eh->h_dest[1] = eh->h_dest[2] = eh->h_dest[3] = eh->h_dest[4] = eh->h_dest[5] = 0x5A;
	eh->h_source[0] = 0x00;
	eh->h_source[1] = 0x01;
	eh->h_source[2] = 0x02;
	eh->h_source[3] = 0x03;
	eh->h_source[4] = 0x04;
	eh->h_source[5] = 0x05;
	eh->h_proto = 0;
	parsed_hdr_size += sizeof(struct ethhdr);
	for (i = parsed_hdr_size; i < len; i++)
	{
		pkt[i] = i;
	}
	return len;
}
void parse_pkt(const unsigned char *pkt, int len)
{
	unsigned int parsed_hdr_size;
	struct ethhdr *eh;
	struct iphdr *ih;
	struct udphdr *uh;
	struct tcphdr *th;
	unsigned char *addr;
	int i;

	parsed_hdr_size = 0;
	eh = (struct ethhdr *)(pkt + parsed_hdr_size);
	if (len < (parsed_hdr_size + sizeof(struct ethhdr)))
	{
		printf("Incomplete Packet. Aborting ... \n");
		return;
	}
	printf("Dst MAC: %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n",
		eh->h_dest[0], eh->h_dest[1], eh->h_dest[2], eh->h_dest[3], eh->h_dest[4], eh->h_dest[5]);
	printf("Src MAC: %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n",
		eh->h_source[0], eh->h_source[1], eh->h_source[2],
		eh->h_source[3], eh->h_source[4], eh->h_source[5]);
	printf("MAC Packet Type ID (Protocol): 0x%04hX (Refer include/uapi/linux/if_ether.h)\n",
		ntohs(eh->h_proto));
	if (ntohs(eh->h_proto) != ETH_P_IP)
	{
		printf("Non-IP Hdr follows. Skipping ...\n");
		return;
	}
	parsed_hdr_size += sizeof(struct ethhdr);
	ih = (struct iphdr *)(pkt + parsed_hdr_size);
	if (len < (parsed_hdr_size + sizeof(struct iphdr)))
	{
		printf("Incomplete Packet. Aborting ... \n");
		return;
	}
	printf("IP Version: %u, Hdr Len: %u, ToS: 0x%02hhX, Total Length: %hu\n",
		ih->version, ih->ihl, ih->tos, ntohs(ih->tot_len));
	printf("ID: 0x%04hX, Fragment Offset: %hu\n", ntohs(ih->id), ntohs(ih->frag_off));
	printf("TTL: %hhu, Protocol: 0x%02hhX (Refer include/uapi/linux/in.h), Chksum: 0x%04hX\n",
		ih->ttl, ih->protocol, ntohs(ih->check));
	addr = (unsigned char *)(&ih->saddr);
	printf("Src IP: %hhu.%hhu.%hhu.%hhu\n", addr[0], addr[1], addr[2], addr[3]);
	addr = (unsigned char *)(&ih->daddr);
	printf("Dst IP: %hhu.%hhu.%hhu.%hhu\n", addr[0], addr[1], addr[2], addr[3]);
	parsed_hdr_size += sizeof(struct iphdr);
	if (ih->protocol == IPPROTO_UDP)
	{
		uh = (struct udphdr *)(pkt + parsed_hdr_size);
		if (len < (parsed_hdr_size + sizeof(struct udphdr)))
		{
			printf("Incomplete Packet. Aborting ... \n");
			return;
		}
		printf("UDP Src Port: %hu, Dst Port: %hu, Hdr + Data Len: %hu, Chksum: 0x%04X\n",
			ntohs(uh->source), ntohs(uh->dest), ntohs(uh->len), ntohs(uh->check));
		parsed_hdr_size += sizeof(struct udphdr);
	}
	else if (ih->protocol == IPPROTO_TCP)
	{
		th = (struct tcphdr *)(pkt + parsed_hdr_size);
		if (len < (parsed_hdr_size + sizeof(struct tcphdr)))
		{
			printf("Incomplete Packet. Aborting ... \n");
			return;
		}
		printf("TCP Src Port: %hu, Dst Port: %hu, Window: %hu, Chksum: 0x%04hX\n",
			ntohs(th->source), ntohs(th->dest), ntohs(th->window), ntohs(th->check));
		printf("Seq: %u, Ack Seq: %u\n", ntohl(th->seq), ntohl(th->ack_seq));
		parsed_hdr_size += sizeof(struct tcphdr);
	}
	else
	{
		printf("Non-UDT/TCP Hdr follows. Skipping ...\n");
		return;
	}
	printf("Payload (%d bytes):", len - parsed_hdr_size);
	for (i = parsed_hdr_size; i < len; i++)
	{
		printf(" %02hhX", pkt[i]);
	}
	printf("\n");
}
void dump_pkt(const unsigned char *pkt, int len)
{
	int fd;

	if ((fd = open("pkt", O_CREAT | O_WRONLY, S_IRUSR | S_IRGRP | S_IROTH)) == -1) return;
	if (write(fd, pkt, len) == -1) return;
	close(fd);
}
int main(int argc, char *argv[])
{
	int choice, ret, iface_up, iface_promisc, iface_index;
	char iface[IFNAMSIZ], iface_new[IFNAMSIZ];
	unsigned char mac_addr[18], ip_addr[16], ip_mask[16];
	unsigned char pkt[MTU];
	int len, tx_len, rx_len;

	if (argc != 2)
	{
		printf("Usage : %s <nw i/f>\n", argv[0]);
		return 1;
	}
	strncpy(iface, argv[1], IFNAMSIZ - 1);

	if (enable_ipv6(iface, 0) == -1) // To stop sending out default IPv6 packets
	{
		return 2;
	}
	else
	{
		printf("Disabled IPv6 on %s\n", iface);
	}

	do
	{
		printf("1: Get MAC / HW Address\n");
		printf("2: Set MAC / HW Address\n");
		printf("3: Get IP Address\n");
		printf("4: Set IP Address\n");
		printf("5: Get I/F State\n");
		printf("6: Set I/F State\n");
		printf("7: Get I/F Index\n");
		printf("8: Change I/F Name\n");
		printf("9: Transmit Packet\n");
		printf("10: Receive Packet\n");
		printf("0: Exit\n");
		printf("Choice: ");
		scanf("%d%*c", &choice);
		switch (choice)
		{
			case 1:
				if (get_mac_addr(iface, mac_addr) != -1)
				{
					printf("%s's MAC addr: %s\n", iface, mac_addr);
				}
				break;
			case 2:
				printf("Enter MAC / HW Addr in XX:XX:XX:XX:XX:XX hex format: ");
				scanf("%s%*c", mac_addr);
				if (set_mac_addr(iface, mac_addr) != -1)
				{
					printf("%s's MAC addr set to %s\n", iface, mac_addr);
				}
				break;
			case 3:
				if (get_ip_addr(iface, ip_addr, ip_mask) != -1)
				{
					printf("%s's IP addr: %s / %s\n", iface, ip_addr, ip_mask);
				}
				break;
			case 4:
				printf("Enter IP Addr in D.D.D.D decimal format: ");
				scanf("%s%*c", ip_addr);
				printf("Enter Net Mask in D.D.D.D decimal format: ");
				//printf("Enter Net Mask in D.D.D.D decimal format [<Enter> for none]: ");
				if (scanf("%s%*c", ip_mask) == 1) // Net Mask provided
				{
					ret = set_ip_addr(iface, ip_addr, ip_mask);
				}
				else
				{
					ret = set_ip_addr(iface, ip_addr, NULL);
				}
				if (ret != -1)
				{
					printf("%s's IP addr: %s / %s\n", iface, ip_addr, ip_mask);
				}
				break;
			case 5:
				if (get_if_state(iface, &iface_up, &iface_promisc) != -1)
				{
					printf("%s is currently %s%s\n", iface, (iface_up ? "UP" : "DOWN"),
							(iface_promisc ? " (PROMISCUOUS)" : ""));
				}
				break;
			case 6:
				printf("Enter I/F State (0 for DOWN, 1 for UP): ");
				scanf("%d", &iface_up);
				// TODO 1: Comment the first, and uncomment the 2nd & 3rd lines to play w/ promiscuous mode
				iface_promisc = 0;
				//printf("Enter I/F Mode (0 for NORMAL, 1 for PROMISCUOUS): ");
				//scanf("%d", &iface_promisc);
				if (set_if_state(iface, &iface_up, &iface_promisc) != -1)
				{
					printf("%s is now %s%s\n", iface, (iface_up ? "UP" : "DOWN"),
							(iface_promisc ? " (PROMISCUOUS)" : ""));
				}
				break;
			case 7:
				if (get_if_index(iface, &iface_index) != -1)
				{
					printf("%s's I/F Index: %d\n", iface, iface_index);
				}
				break;
			case 8:
				printf("Enter new I/F Name: ");
				scanf("%s%*c", iface_new);
				if (set_if_name(iface, iface_new) != -1)
				{
					printf("%s changed to %s\n", iface, iface_new);
					strncpy(iface, iface_new, IFNAMSIZ - 1);
				}
				break;
			case 9:
				len = 100;
				len = prepare_pkt(pkt, len);
				if ((tx_len = tx_pkt(iface, pkt, len)) != -1)
				{
					printf("Transmitted %d/%d bytes of packet through %s\n", tx_len, len, iface);
				}
				break;
			case 10:
				if ((rx_len = rx_pkt(iface, pkt, MTU)) != -1)
				{
					printf("Received %d bytes of packet through %s\n", rx_len, iface);
					parse_pkt(pkt, rx_len);
					//dump_pkt(pkt, rx_len);
				}
				break;
			default:
				break;
		}
	} while (choice);

	return 0;
}
