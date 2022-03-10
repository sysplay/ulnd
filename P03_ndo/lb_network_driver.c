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
#include <linux/spinlock.h> // spinlock_t, ...

#define DRV_PREFIX "lnd"
#include "common.h"

#define LND_NAPI_WEIGHT 64

typedef struct _DrvPvt
{
	struct net_device *ndev;
	spinlock_t lock; // Protect the skb ptr
	struct sk_buff *skb;
	struct napi_struct napi;
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
		iprintk("Non-UDT/TCP Hdr follows. Skipping ...\n");
		return;
	}
	iprintk("Payload is %d bytes\n", len - parsed_hdr_size);
	for (i = parsed_hdr_size; i < len; i++)
	{
		//iprintk("%02hhX", pkt[i]);
	}
}

static int lnd_open(struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);

	iprintk("open\n");
	napi_enable(&pvt->napi);
	return 0;
}
static int lnd_close(struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);
	unsigned long flags;

	iprintk("close\n");
	napi_disable(&pvt->napi);
	// Clear the pkts, if any
	spin_lock_irqsave(&pvt->lock, flags);
	pvt->skb = NULL;
	spin_unlock_irqrestore(&pvt->lock, flags);
	// Clear the stats
	memset(&dev->stats, 0, sizeof(dev->stats));
	return 0;
}
static int lnd_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);
	unsigned long flags;

	iprintk("tx\n");
	display_packet(skb);

	spin_lock_irqsave(&pvt->lock, flags);
	if (pvt->skb) // Loopback Hack: Previous pkt not yet transmitted. Drop it :)
	{
		dev->stats.tx_dropped++;
		dev_kfree_skb(pvt->skb);
		pvt->skb = NULL;
	}

	// Loopback Hack: Store & Trigger poll for pkt received
	pvt->skb = skb;
	//napi_schedule(&pvt->napi); // TODO
	spin_unlock_irqrestore(&pvt->lock, flags);

	return 0;
}
static int lnd_set_mac_address(struct net_device *dev, void *addr)
{
	iprintk("set_mac\n");
	return eth_mac_addr(dev, addr);
}

static const struct net_device_ops lnd_netdev_ops =
{
	.ndo_open = lnd_open,
	.ndo_stop = lnd_close,
	.ndo_start_xmit = lnd_start_xmit,
	.ndo_set_mac_address = lnd_set_mac_address,
};

static int lnd_poll(struct napi_struct *napi_ptr, int budget)
{
	DrvPvt *pvt = container_of(napi_ptr, DrvPvt, napi);
	struct net_device *dev = pvt->ndev;
	unsigned long flags;
	struct sk_buff *skb;
	int pkt_size;
	unsigned int work_done;

	iprintk("poll\n");
	spin_lock_irqsave(&pvt->lock, flags);
	skb = pvt->skb;
	pvt->skb = NULL;
	spin_unlock_irqrestore(&pvt->lock, flags);

	if (!skb) // Should not happen
	{
		dev->stats.tx_errors++; // Loopback Hack: Update for pkt transmission error
		dev->stats.rx_errors++;
	}
	else
	{
		// Loopback Hack: Update for pkt transmission complete
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += skb->len;

		// Loopback Hack: Get size of for received pkt
		pkt_size = skb->len;

		dev->stats.rx_packets++;
		dev->stats.rx_bytes += pkt_size;
		//skb_put(skb, pkt_size); // Not to be done here as it is already set
		//skb->protocol = eth_type_trans(skb, dev); // Not needed here as it is already set
		//napi_gro_receive(&pvt->napi, skb); // TODO: Handover to the network stack
	}

	work_done = 1; // Always as currently max support of one packet only
	if (work_done < budget) {
		napi_complete(napi_ptr);
	}

	return work_done;
}

static int lnd_init(void)
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
	dev->netdev_ops = &lnd_netdev_ops;
	spin_lock_init(&pvt->lock);
	pvt->skb = NULL;
	netif_napi_add(dev, &pvt->napi, lnd_poll, LND_NAPI_WEIGHT);
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
		iprintk("network interface registered successfully\n");
	}
	return ret;
}
static void lnd_exit(void)
{
	struct net_device *dev = ndev;
	DrvPvt *pvt = netdev_priv(dev);

	iprintk("exit\n");
	unregister_netdev(dev);
	netif_napi_del(&pvt->napi);
	free_netdev(dev);
}

module_init(lnd_init);
module_exit(lnd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <anil@sysplay.in>");
MODULE_DESCRIPTION("Loopback Network Device Driver");
