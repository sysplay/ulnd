#include <linux/module.h>
#include <linux/errno.h>

#include <linux/netdevice.h> // struct net_device..., struct net_device_stats, ...
#include <linux/etherdevice.h> // alloc_etherdev, ...
#include <linux/if_ether.h> // struct ethhdr, Ethernet protocol definitions
#include <linux/ip.h> // struct iphdr
#include <linux/in.h> // IP protocol definitions
#include <linux/udp.h> // struct udphdr, UDP definitions
#include <linux/tcp.h> // struct tcphdr, TCP definitions
#include <linux/byteorder/generic.h> // ntoh...

#define DRV_PREFIX "tnd"
#include "common.h"

typedef struct _DrvPvt
{
	struct net_device *ndev;
} DrvPvt;

static struct net_device *ndev;

static void display_packet(struct sk_buff *skb)
{
	unsigned char *pkt = skb->data;
	int len = skb->len;
	unsigned int parsed_hdr_size;
	struct ethhdr *eh;
	struct iphdr *ih;
	struct udphdr *uh;
	struct tcphdr *th;
	unsigned char *addr;
	int i;

	iprintk("Pkt Len: %u\n", len);
	parsed_hdr_size = 0;
	eh = (struct ethhdr *)(pkt + parsed_hdr_size);
	if (len < (parsed_hdr_size + sizeof(struct ethhdr)))
	{
		iprintk("Incomplete Packet. Aborting ... \n");
		return;
	}
	iprintk("Dst MAC: %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n",
		eh->h_dest[0], eh->h_dest[1], eh->h_dest[2], eh->h_dest[3], eh->h_dest[4], eh->h_dest[5]);
	iprintk("Src MAC: %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n",
		eh->h_source[0], eh->h_source[1], eh->h_source[2],
		eh->h_source[3], eh->h_source[4], eh->h_source[5]);
	iprintk("MAC Packet Type ID (Protocol): 0x%04hX (Refer include/uapi/linux/if_ether.h)\n",
		ntohs(eh->h_proto));
	if (ntohs(eh->h_proto) != ETH_P_IP)
	{
		iprintk("Non-IP Hdr follows. Skipping ...\n");
		return;
	}
	parsed_hdr_size += sizeof(struct ethhdr);
	ih = (struct iphdr *)(pkt + parsed_hdr_size);
	if (len < (parsed_hdr_size + sizeof(struct iphdr)))
	{
		iprintk("Incomplete Packet. Aborting ... \n");
		return;
	}
	iprintk("IP Version: %u, Hdr Len: %u, ToS: 0x%02hhX, Total Length: %hu\n",
		ih->version, ih->ihl, ih->tos, ntohs(ih->tot_len));
	iprintk("ID: 0x%04hX, Fragment Offset: %hu\n", ntohs(ih->id), ntohs(ih->frag_off));
	iprintk("TTL: %hhu, Protocol: 0x%02hhX (Refer include/uapi/linux/in.h), Chksum: 0x%04hX\n",
		ih->ttl, ih->protocol, ntohs(ih->check));
	addr = (unsigned char *)(&ih->saddr);
	iprintk("Src IP: %hhu.%hhu.%hhu.%hhu\n", addr[0], addr[1], addr[2], addr[3]);
	addr = (unsigned char *)(&ih->daddr);
	iprintk("Dst IP: %hhu.%hhu.%hhu.%hhu\n", addr[0], addr[1], addr[2], addr[3]);
	parsed_hdr_size += sizeof(struct iphdr);
	if (ih->protocol == IPPROTO_UDP)
	{
		uh = (struct udphdr *)(pkt + parsed_hdr_size);
		if (len < (parsed_hdr_size + sizeof(struct udphdr)))
		{
			iprintk("Incomplete Packet. Aborting ... \n");
			return;
		}
		iprintk("UDP Src Port: %hu, Dst Port: %hu, Hdr + Data Len: %hu, Chksum: 0x%04X\n",
			ntohs(uh->source), ntohs(uh->dest), ntohs(uh->len), ntohs(uh->check));
		parsed_hdr_size += sizeof(struct udphdr);
	}
	else if (ih->protocol == IPPROTO_TCP)
	{
		th = (struct tcphdr *)(pkt + parsed_hdr_size);
		if (len < (parsed_hdr_size + sizeof(struct tcphdr)))
		{
			iprintk("Incomplete Packet. Aborting ... \n");
			return;
		}
		iprintk("TCP Src Port: %hu, Dst Port: %hu, Window: %hu, Chksum: 0x%04hX\n",
			ntohs(th->source), ntohs(th->dest), ntohs(th->window), ntohs(th->check));
		iprintk("Seq: %u, Ack Seq: %u\n", ntohl(th->seq), ntohl(th->ack_seq));
		parsed_hdr_size += sizeof(struct tcphdr);
	}
	else
	{
		iprintk("Non-UDP/TCP Hdr follows. Skipping ...\n");
		return;
	}
	iprintk("Payload is %d bytes\n", len - parsed_hdr_size);
	for (i = parsed_hdr_size; i < len; i++)
	{
		//iprintk("%02hhX", pkt[i]);
	}
}

static int tnd_open(struct net_device *dev)
{
	iprintk("open\n");
	return 0;
}
static int tnd_close(struct net_device *dev)
{
	iprintk("close\n");
	// Clear the stats on i/f down
	memset(&dev->stats, 0, sizeof(dev->stats));
	return 0;
}
static int tnd_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	iprintk("tx\n");
	display_packet(skb);
	// TODO 4: Uncomment the following to see the statistics effect
	//dev->stats.tx_dropped++;
	dev_kfree_skb(skb); // As we are not using it any further
	return 0;
}
static void tnd_change_rx_flags(struct net_device *dev, int flags)
{
	iprintk("rx_flags: 0x%08X\n", flags);
}
static int tnd_set_mac_address(struct net_device *dev, void *addr)
{
	iprintk("set_mac\n");
	return eth_mac_addr(dev, addr);
}
//static int (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
//static int (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
//static void (*ndo_tx_timeout)(struct net_device *dev, unsigned int txqueue);

#if 0 // Not required
static struct net_device_stats *tnd_get_stats(struct net_device *dev)
{
	iprintk("stats\n");
	return &dev->stats;
}
#endif

static const struct net_device_ops tnd_netdev_ops =
{
	// TODO 2: Uncomment the following only if required to do some custom initialization & see up connection
	//.ndo_open = tnd_open,
	// TODO 3: Uncomment the following only if required to do some custom cleanup & see down connection
	//.ndo_stop = tnd_close,
	.ndo_start_xmit = tnd_start_xmit, // Required on "up", as packets are being sent out on up
	// For specific hardware level rx configs. TODO 5: Uncoment to see the trigger for promiscuous mode
	//.ndo_change_rx_flags = tnd_change_rx_flags,
	// TODO 1: Uncomment to allow MAC Address change
	//.ndo_set_mac_address = tnd_set_mac_address,
	// Not really required
	//.ndo_get_stats = tnd_get_stats,
};

static int tnd_init(void)
{
	struct net_device *dev;
	DrvPvt *pvt;
	int i, ret;

	iprintk("init\n");

	dev = alloc_etherdev(sizeof(DrvPvt));
	if (!dev)
	{
		eprintk("device allocation failed\n");
		return -ENOMEM;
	}
	pvt = netdev_priv(dev);
	pvt->ndev = dev;
	dev->netdev_ops = &tnd_netdev_ops;
	//dev->watchdog_timeo = 6 * HZ; // Used for ndo_tx_timeout
	// Setting up some MAC Addr - 00:01:02:03:04:05 to be specific
	for (i = 0; i < dev->addr_len; i++)
	{
		dev->dev_addr[i] = i;
	}
	if ((ret = register_netdev(dev)))
	{
		eprintk("%s network interface registration failed w/ error %i\n", dev->name, ret);
		free_netdev(dev);
	}
	else
	{
		ndev = dev; // Hack using global variable in absence of a horizontal layer
	}
	return ret;
}
static void tnd_exit(void)
{
	struct net_device *dev = ndev;

	iprintk("exit\n");
	unregister_netdev(dev);
	free_netdev(dev);
}

module_init(tnd_init);
module_exit(tnd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <anil@sysplay.in>");
MODULE_DESCRIPTION("Transmit Network Device Driver");
