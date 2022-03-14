#ifndef NIC_H
#define NIC_H

#ifdef __KERNEL__

#include <linux/skbuff.h>

typedef void (*Handler)(void *);

void nic_setup_buffers(void);
void nic_cleanup_buffers(void);
void nic_register_handler(Handler handler, void *handler_param);
void nic_unregister_handler(void);
void nic_hw_enable_intr(void);
void nic_hw_disable_intr(void);
void nic_hw_init(void); // Should be called after everything is set up
void nic_hw_shut(void); // Should be called before anything is cleaned up
int nic_hw_tx_pkt(struct sk_buff *skb);
struct sk_buff *nic_hw_rx_pkt(void);

#endif

#endif
