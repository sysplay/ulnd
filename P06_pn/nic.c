#include "nic.h"

/* Following are the defines for NIC related implementation */
/* TODO: Add more &/or Change the following defines as per your NIC */
//#define NUM_TX_DESC 1024 // Number of transmit descriptors
//#define NUM_RX_DESC 1024 // Number of receive descriptors
#define	MAC_ADDR_REG_START 0

/* Following are the functions for NIC related implementation */
/*
 * TODO: Implement the following functionality as per your NIC
 * Prototypes of the following functions may be appropriately modified,
 * as needed. For Example:
 * 	Pointer to DrvPvt may need to be passed as a parameter
 * 	An int return value may be reqd instead of void for proper error handling
 */
void nic_setup_buffers(DrvPvt *pvt)
{
}
void nic_cleanup_buffers(DrvPvt *pvt)
{
}
void nic_register_handler(DrvPvt *pvt, Handler handler)
{
}
void nic_unregister_handler(DrvPvt *pvt)
{
}

void nic_hw_get_mac_addr(void __iomem *reg_base, unsigned char addr[6])
{
	int i;

	for (i = 0; i < 6; i++)
	{
		addr[i] = ioread8(reg_base + MAC_ADDR_REG_START + i);
	}
}

void nic_hw_enable_intr(void __iomem *reg_base)
{
}
void nic_hw_disable_intr(void __iomem *reg_base)
{
}
void nic_hw_init(DrvPvt *pvt)
{
}
void nic_hw_shut(DrvPvt *pvt)
{
}

int nic_hw_tx_pkt(DrvPvt *pvt, struct sk_buff *skb)
{
	dev_kfree_skb(skb);
	return 0;
}
struct sk_buff *nic_hw_rx_pkt(DrvPvt *pvt)
{
	return NULL;
}
