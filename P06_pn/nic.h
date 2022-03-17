#ifndef NIC_H
#define NIC_H

#ifdef __KERNEL__

#include <linux/netdevice.h>

/* Following are the headers for NIC related implementation */
/* TODO: Add more as needed */
#include <linux/pci.h>
#include <linux/io.h>
//#include <linux/spinlock.h>

#include <linux/skbuff.h>

typedef void (*Handler)(void *);
typedef struct _DrvPvt
{
	struct net_device *ndev;
	struct napi_struct napi;

	/* Following are the fields for NIC related implementation */
	/* TODO: Add more as needed */
	struct pci_dev *pdev;
	void __iomem *reg_base;
	//spinlock_t lock; // Protect the following buffers & handler related fields
	//...
} DrvPvt;

/* Following are the function prototypes for NIC related implementation */
/* TODO: Modify as per the modifications in nic.c */
void nic_setup_buffers(void);
void nic_cleanup_buffers(void);
void nic_register_handler(Handler handler, void *handler_param);
void nic_unregister_handler(void);

void nic_hw_get_mac_addr(void __iomem *reg_base, unsigned char addr[6]);

void nic_hw_enable_intr(void);
void nic_hw_disable_intr(void);
void nic_hw_init(void); // Should be called after everything is set up
void nic_hw_shut(void); // Should be called before anything is cleaned up

int nic_hw_tx_pkt(struct sk_buff *skb);
struct sk_buff *nic_hw_rx_pkt(void);

#endif

#endif
