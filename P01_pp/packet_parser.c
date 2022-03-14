#include <stdio.h>
#include <string.h>

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

#define MTU 1500

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
	/* TODO 1: Print the Ethernet Hdr Fields */
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
	/* TODO 2: Print the IP Hdr Fields */
	parsed_hdr_size += sizeof(struct iphdr);
	if (ih->protocol == IPPROTO_UDP)
	{
		uh = (struct udphdr *)(pkt + parsed_hdr_size);
		if (len < (parsed_hdr_size + sizeof(struct udphdr)))
		{
			printf("Incomplete Packet. Aborting ... \n");
			return;
		}
		/* TODO 3: Print the UDP Hdr Fields */
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
		/* TODO 4: Print the TCP Hdr Fields */
		parsed_hdr_size += sizeof(struct tcphdr);
	}
	else
	{
		printf("Non-UDP/TCP Hdr follows. Skipping ...\n");
		return;
	}
	printf("Payload (%d bytes):", len - parsed_hdr_size);
	for (i = parsed_hdr_size; i < len; i++)
	{
		printf(" %02hhX", pkt[i]);
	}
	printf("\n");
}
int main(int argc, char *argv[])
{
	char *fn;
	int fd;
	unsigned char pkt[MTU];
	int len = MTU;

	if (argc != 2)
	{
		printf("Usage : %s <pkt_file_name>\n", argv[0]);
		return 1;
	}
	fn = argv[1];
	if ((fd = open(fn, O_RDONLY)) == -1)
	{
		perror("open");
		return 2;
	}
	if ((len = read(fd, pkt, MTU)) == -1)
	{
		perror("read");
		close(fd);
		return 3;
	}
	parse_pkt(pkt, len);
	close(fd);

	return 0;
}
