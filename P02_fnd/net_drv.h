#ifndef NET_DRV_H
#define NET_DRV_H

#ifdef __KERNEL__

// TODO 1: Uncomment the following line
#define PND

#include <linux/kernel.h> /* printk() */
#ifdef PND
#include <linux/netdevice.h> /* struct net_device */
#endif

#define dprintk(fmt, args...)	do { printk(KERN_DEBUG DRV_PREFIX ": " fmt, ## args); } while (0)
#define iprintk(fmt, args...)	do { printk(KERN_INFO DRV_PREFIX ": " fmt, ## args); } while (0)
#define nprintk(fmt, args...)	do { printk(KERN_NOTICE DRV_PREFIX ": " fmt, ## args); } while (0)
#define wprintk(fmt, args...)	do { printk(KERN_WARNING DRV_PREFIX ": " fmt, ## args); } while (0)
#define eprintk(fmt, args...)	do { printk(KERN_ERR DRV_PREFIX ": " fmt, ## args); } while (0)
#define cprintk(fmt, args...)	do { printk(KERN_CRIT DRV_PREFIX ": " fmt, ## args); } while (0)
#define egprintk(fmt, args...)	do { printk(KERN_EMERG DRV_PREFIX ": " fmt, ## args); } while (0)

typedef struct _DrvPvt
{
#ifdef PND
	struct net_device *ndev;
#endif
} DrvPvt;

#endif

#endif
