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

#define DRV_PREFIX "end"
#include "common.h"

#include "nic.h"

#define END_NAPI_WEIGHT 64

typedef struct _DrvPvt
{
	struct net_device *ndev;
	struct napi_struct napi;
} DrvPvt;

static DrvPvt *npvt;

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

static void handler(void *handler_param)
{
	DrvPvt *pvt = (DrvPvt *)(handler_param);

	nic_hw_disable_intr();
	napi_schedule(&pvt->napi);
}

static int end_open(struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);

	iprintk("open\n");
	nic_setup_buffers();
	napi_enable(&pvt->napi);
	nic_register_handler(handler, pvt);
	nic_hw_init();
	return 0;
}
static int end_close(struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);

	iprintk("close\n");
	nic_hw_shut();
	nic_unregister_handler();
	napi_disable(&pvt->napi);
	nic_cleanup_buffers(); // In turn, also clears the pkts, if any
	// Clear the stats
	memset(&dev->stats, 0, sizeof(dev->stats));
	return 0;
}
static int end_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int len;

	iprintk("tx\n");
	display_packet(skb);
	len = skb->len; // HACK: To avoid using skb after packet transmission
	if (nic_hw_tx_pkt(skb)) // Buffer Full & hence dropped
	{
		dev->stats.tx_dropped++;
		dev_kfree_skb(skb);
	}
	else
	{
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += len;
	}
	return 0;
}
static int end_set_mac_address(struct net_device *dev, void *addr)
{
	iprintk("set_mac\n");
	return eth_mac_addr(dev, addr);
}

static const struct net_device_ops end_netdev_ops =
{
	.ndo_open = end_open,
	.ndo_stop = end_close,
	.ndo_start_xmit = end_start_xmit,
	.ndo_set_mac_address = end_set_mac_address,
};

static void end_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, "end", sizeof(info->driver));
	strlcpy(info->bus_info, "VNIC", sizeof(info->bus_info));
}

static const struct ethtool_ops end_ethtool_ops =
{
	.get_drvinfo = end_get_drvinfo,
};

static int end_poll(struct napi_struct *napi_ptr, int budget)
{
	DrvPvt *pvt = container_of(napi_ptr, DrvPvt, napi);
	struct net_device *dev = pvt->ndev;
	unsigned int work_done;
	struct sk_buff *skb;

	iprintk("poll\n");
	work_done = 0;
	while ((work_done < budget) && (skb = nic_hw_rx_pkt()))
	{
		display_packet(skb);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += skb->len;
		napi_gro_receive(&pvt->napi, skb); // Handover to the network stack
		work_done++;
	}
	if (work_done < budget) {
		napi_complete(napi_ptr);
		nic_hw_enable_intr();
	}
	return work_done;
}

static int end_init(void)
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
	netif_napi_add(dev, &pvt->napi, end_poll, END_NAPI_WEIGHT);
	// Setting up some MAC Addr - 00:01:02:03:04:05 to be specific
	for (i = 0; i < dev->addr_len; i++)
	{
		dev->dev_addr[i] = i;
	}
	dev->netdev_ops = &end_netdev_ops;
	//dev->ethtool_ops = &end_ethtool_ops;
	if ((ret = register_netdev(dev)))
	{
		eprintk("%s network interface registration failed w/ error %i\n", dev->name, ret);
		free_netdev(dev);
	}
	else
	{
		npvt = pvt; // Hack using global variable in absence of a horizontal layer
	}
	return ret;
}
static void end_exit(void)
{
	DrvPvt *pvt = npvt;
	struct net_device *dev = pvt->ndev;

	iprintk("exit\n");
	unregister_netdev(dev);
	netif_napi_del(&pvt->napi);
	free_netdev(dev);
}

module_init(end_init);
module_exit(end_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <anil@sysplay.in>");
MODULE_DESCRIPTION("Packeted Network Device Driver");
