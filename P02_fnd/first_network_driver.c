#include <linux/module.h>
#include <linux/errno.h>

#include <linux/etherdevice.h> // struct alloc_etherdev
#include <linux/netdevice.h> // struct net_device, ...

#define DRV_PREFIX "fnd"
#include "common.h"

typedef struct _DrvPvt
{
} DrvPvt;

static struct net_device *ndev;
static const struct net_device_ops fnd_netdev_ops;

static int fnd_init(void)
{
	int ret;

	iprintk("init\n");

	ndev = alloc_etherdev(sizeof(DrvPvt));
	if (!ndev)
	{
		eprintk("device allocation failed\n");
		return -ENOMEM;
	}
	ndev->netdev_ops = &fnd_netdev_ops;
	if ((ret = register_netdev(ndev)))
	{
		eprintk("%s network interface registration failed w/ error %i\n", ndev->name, ret);
		free_netdev(ndev);
	}
	else
	{
		iprintk("network interface registered successfully\n");
	}
	return ret;
}
static void fnd_exit(void)
{
	iprintk("exit\n");
	unregister_netdev(ndev);
	free_netdev(ndev);
}

module_init(fnd_init);
module_exit(fnd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <anil@sysplay.in>");
MODULE_DESCRIPTION("First Network Device Driver");
