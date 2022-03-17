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
/* Following are the NIC Simulation related headers */
#include <linux/spinlock.h> // spinlock_t, ...

#define DRV_PREFIX "nic"
#include "common.h"

#include "nic.h"

#define NIC_NAPI_WEIGHT 64

/* Following are the NIC Simulation related defines */
#define NUM_TX_DESC 1024 /* Number of transmit descriptors */
#define NUM_RX_DESC 1024 /* Number of receive descriptors */

typedef struct _DrvPvt
{
	struct net_device *ndev;
	struct napi_struct napi;

	/* Following are the NIC Simulation related fields */
	spinlock_t lock; // Protect the following buffers & handler related fields
	// Note: Define anything below in such a way that its value of zero indicates its default value
	/*
	 * tx_drv is the current index to be filled by driver for transmit
	 * tx_nic is the current index to be transmitted by the NIC
	 * tx_nic == tx_drv => Ring buffer empty
	 * tx_drv one position behind tx_nic => Ring buffer full (we'll always keep one index free, even on full)
	 * 	i.e. ((tx_nic - (tx_drv + 1) + NUM_TX_DESC) % NUM_TX_DESC == 0)
	 * Count of pkts in the ring buffer = (tx_drv - tx_nic + NUM_TX_DESC) % NUM_TX_DESC;
	 *
	 * rx_drv is the current index to be picked by driver for receive
	 * rx_nic is the current index to be received by the NIC
	 * rx_drv == rx_nic => Ring buffer empty
	 * rx_nic one position behind rx_drv => Ring buffer full (we'll always keep one index free, even on full)
	 * 	i.e. ((rx_drv - (rx_nic + 1) + NUM_RX_DESC) % NUM_RX_DESC == 0)
	 * Count of pkts in the ring buffer = (rx_nic - rx_drv + NUM_RX_DESC) % NUM_RX_DESC;
	 */
	int tx_drv, tx_nic, rx_drv, rx_nic;
	struct sk_buff *tx_ring_buffer[NUM_TX_DESC];
	struct sk_buff *rx_ring_buffer[NUM_RX_DESC];

	void *handler_param; // Parameter to be passed to handler
	Handler handler;

	/*
	 * Indicates if this NIC is initialized or not.
	 * All NIC operations should depend on this. Why?
	 * Because NIC is intialized means everything else is assumed to be setup.
	 * Moreover, these should not be protected by spinlock in general, as these indicate hw state
	 */
	int nic_ready;
	int nic_intr_enabled; // NIC level interrupt is enabled or not
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

static int nic_open(struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);

	iprintk("open\n");
	napi_enable(&pvt->napi);
	return 0;
}
static int nic_close(struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);

	iprintk("close\n");
	napi_disable(&pvt->napi);
	// Clear the stats
	memset(&dev->stats, 0, sizeof(dev->stats));
	return 0;
}
// VNIC Hack: For transmitting packets from the other end of the NIC
static int nic_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	DrvPvt *pvt = netdev_priv(dev);
	unsigned long flags;

	iprintk("tx\n");
	display_packet(skb);

	spin_lock_irqsave(&pvt->lock, flags);
	if ((pvt->nic_ready) &&
		((pvt->rx_drv - (pvt->rx_nic + 1) + NUM_RX_DESC) % NUM_RX_DESC != 0)) // Not Full
	{
		pvt->rx_ring_buffer[pvt->rx_nic] = skb;
		pvt->rx_nic = (pvt->rx_nic + 1) % NUM_RX_DESC;
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += skb->len;
		if ((pvt->nic_intr_enabled) && (pvt->handler)) // VNIC Hack: Trigger the rx interrupt for the driver
		{
			(*(pvt->handler))(pvt->handler_param);
		}
	}
	else
	{
		dev->stats.tx_dropped++;
		dev_kfree_skb(skb);
	}
	spin_unlock_irqrestore(&pvt->lock, flags);

	return 0;
}

static const struct net_device_ops nic_netdev_ops =
{
	.ndo_open = nic_open,
	.ndo_stop = nic_close,
	.ndo_start_xmit = nic_start_xmit,
};

// VNIC Hack: For receiving packets on the other end of the NIC
static int nic_poll(struct napi_struct *napi_ptr, int budget)
{
	DrvPvt *pvt = container_of(napi_ptr, DrvPvt, napi);
	struct net_device *dev = pvt->ndev;
	unsigned long flags;
	struct sk_buff *skb = NULL;
	int pkt_size;
	unsigned int work_done;

	iprintk("poll\n");

	spin_lock_irqsave(&pvt->lock, flags);
	if (pvt->tx_nic != pvt->tx_drv) // Not Empty
	{
		skb = pvt->tx_ring_buffer[pvt->tx_nic];
		pvt->tx_ring_buffer[pvt->tx_nic] = NULL;
		pvt->tx_nic = (pvt->tx_nic + 1) % NUM_TX_DESC;
		// VNIC Hack: Get size of the pkt received on the other end of the NIC
		pkt_size = skb->len;
	}
	spin_unlock_irqrestore(&pvt->lock, flags);

	if (!skb) // Most probably all buffers were cleared due to close from the driver
	{
		dev->stats.rx_errors++;
	}
	else
	{
		display_packet(skb);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += pkt_size;
		//skb_put(skb, pkt_size); // VNIC Hack: Not to be done here as it is already set
		//skb->protocol = eth_type_trans(skb, dev); // VNIC Hack: Not needed here as it is already set
		napi_gro_receive(&pvt->napi, skb); // Handover to the network stack
	}

	work_done = 1; // VNIC Hack: As currently napi is being scheduled for every packet received
	if (work_done < budget)
	{
		napi_complete(napi_ptr);
	}
	// TODO: This function may be enhanced to process packets till budget.
	// Just that there would be redundant napi schedules, which would come here & complete

	return work_done;
}

static int nic_init(void)
{
	struct net_device *dev;
	DrvPvt *pvt;
	int ret;

	iprintk("init\n");

	dev = alloc_netdev(sizeof(DrvPvt), "nic", NET_NAME_UNKNOWN, ether_setup);
	if (!dev)
	{
		eprintk("device allocation failed\n");
		return -ENOMEM;
	}
	pvt = netdev_priv(dev);
	pvt->ndev = dev;
	netif_napi_add(dev, &pvt->napi, nic_poll, NIC_NAPI_WEIGHT);
	// Setting up some MAC Addr - 00:56:4E:49:43:53 to be specific
	memcpy(dev->dev_addr, "\0VNICS", 6); // Virtual NIC Simulation
	dev->netdev_ops = &nic_netdev_ops;
	if ((ret = register_netdev(dev)))
	{
		eprintk("%s network interface registration failed w/ error %i\n", dev->name, ret);
		free_netdev(dev);
	}
	else
	{
		npvt = pvt; // Hack using global variable in absence of a horizontal layer
	}

	/* Following are the NIC Simulation related initializations */
	spin_lock_init(&pvt->lock);
	pvt->nic_ready = 0;

	return ret;
}
static void nic_exit(void)
{
	DrvPvt *pvt = npvt;
	struct net_device *dev = pvt->ndev;

	iprintk("exit\n");

	/* Following are the NIC Simulation related cleanups */
	/* None required */

	unregister_netdev(dev);
	netif_napi_del(&pvt->napi);
	free_netdev(dev);
}

module_init(nic_init);
module_exit(nic_exit);

/* Following are the NIC Simulation related functions */
void nic_setup_buffers(void)
{
	DrvPvt *pvt = npvt;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&pvt->lock, flags);
	pvt->tx_drv = pvt->tx_nic = 0;
	for (i = 0; i < NUM_TX_DESC; i++)
	{
		pvt->tx_ring_buffer[i] = NULL;
	}
	pvt->rx_drv = pvt->rx_nic = 0;
	for (i = 0; i < NUM_RX_DESC; i++)
	{
		pvt->rx_ring_buffer[i] = NULL;
	}
	spin_unlock_irqrestore(&pvt->lock, flags);
}
void nic_cleanup_buffers(void) // Free all non-processed skbs
{
	DrvPvt *pvt = npvt;
	unsigned long flags;
	int pkts_left;

	spin_lock_irqsave(&pvt->lock, flags);
	pkts_left = (pvt->tx_drv - pvt->tx_nic + NUM_TX_DESC) % NUM_TX_DESC;
	while (pkts_left--)
	{
		dev_kfree_skb(pvt->tx_ring_buffer[pvt->tx_nic]);
		pvt->tx_ring_buffer[pvt->tx_nic] = NULL;
		pvt->tx_nic = (pvt->tx_nic + 1) % NUM_TX_DESC;
	}
	pvt->tx_drv = pvt->tx_nic = 0;
	pkts_left = (pvt->rx_nic - pvt->rx_drv + NUM_RX_DESC) % NUM_RX_DESC;
	while (pkts_left--)
	{
		dev_kfree_skb(pvt->rx_ring_buffer[pvt->rx_drv]);
		pvt->rx_ring_buffer[pvt->rx_drv] = NULL;
		pvt->rx_drv = (pvt->rx_drv + 1) % NUM_RX_DESC;
	}
	pvt->rx_drv = pvt->rx_nic = 0;
	spin_unlock_irqrestore(&pvt->lock, flags);
}
void nic_register_handler(Handler handler, void *handler_param)
{
	DrvPvt *pvt = npvt;
	unsigned long flags;

	spin_lock_irqsave(&pvt->lock, flags);
	pvt->handler_param = handler_param;
	pvt->handler = handler;
	spin_unlock_irqrestore(&pvt->lock, flags);
}
void nic_unregister_handler(void)
{
	DrvPvt *pvt = npvt;
	unsigned long flags;

	spin_lock_irqsave(&pvt->lock, flags);
	pvt->handler = NULL;
	pvt->handler_param = NULL;
	spin_unlock_irqrestore(&pvt->lock, flags);
}

void nic_hw_enable_intr(void)
{
	DrvPvt *pvt = npvt;
	//unsigned long flags;

	pvt->nic_intr_enabled = 1;
#ifdef TODO
	// VNIC Hack: Check for pending interrupt by checking pkts in rx buffer & call handler, if pending
	spin_lock_irqsave(&pvt->lock, flags);
	if ((pvt->rx_drv != pvt->rx_nic) && (pvt->handler))
	{
		(*(pvt->handler))(pvt->handler_param);
	}
	spin_unlock_irqrestore(&pvt->lock, flags);
#endif
}
void nic_hw_disable_intr(void)
{
	DrvPvt *pvt = npvt;

	pvt->nic_intr_enabled = 0;
}
void nic_hw_init(void)
{
	DrvPvt *pvt = npvt;

	pvt->nic_ready = 1;
	nic_hw_enable_intr();
}
void nic_hw_shut(void)
{
	DrvPvt *pvt = npvt;

	nic_hw_disable_intr();
	pvt->nic_ready = 0;
}

int nic_hw_tx_pkt(struct sk_buff *skb)
{
	DrvPvt *pvt = npvt;
	unsigned long flags;
	int ret = -1;

	spin_lock_irqsave(&pvt->lock, flags);
	if ((pvt->nic_ready) &&
		((pvt->tx_nic - (pvt->tx_drv + 1) + NUM_TX_DESC) % NUM_TX_DESC != 0)) // Not Full
	{
		pvt->tx_ring_buffer[0 /* TODO 1: index of the tx ring buffer to put the pkt for transmit */] = skb;
		pvt->tx_drv = (pvt->tx_drv + 1) % NUM_TX_DESC;
		ret = 0;
	}
	spin_unlock_irqrestore(&pvt->lock, flags);

	if (ret == 0)
	{
		napi_schedule(&pvt->napi); // VNIC Hack: Trigger the rx poll for the other end of the NIC
	}

	return ret;
}
struct sk_buff *nic_hw_rx_pkt(void)
{
	DrvPvt *pvt = npvt;
	unsigned long flags;
	struct sk_buff *skb = NULL;

	spin_lock_irqsave(&pvt->lock, flags);
	if (pvt->rx_drv != pvt->rx_nic) // Not Empty
	{
		skb = pvt->rx_ring_buffer[pvt->rx_drv];
		pvt->rx_ring_buffer[0 /* TODO 2: index of the rx ring buffer to get the pkt for receive */] = NULL;
		pvt->rx_drv = (pvt->rx_drv + 1) % NUM_RX_DESC;
	}
	spin_unlock_irqrestore(&pvt->lock, flags);

	return skb;
}

EXPORT_SYMBOL(nic_setup_buffers);
EXPORT_SYMBOL(nic_cleanup_buffers);
EXPORT_SYMBOL(nic_register_handler);
EXPORT_SYMBOL(nic_unregister_handler);
EXPORT_SYMBOL(nic_hw_enable_intr);
EXPORT_SYMBOL(nic_hw_disable_intr);
EXPORT_SYMBOL(nic_hw_init);
EXPORT_SYMBOL(nic_hw_shut);
EXPORT_SYMBOL(nic_hw_tx_pkt);
EXPORT_SYMBOL(nic_hw_rx_pkt);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <anil@sysplay.in>");
MODULE_DESCRIPTION("Network Interface Card Simulation");
