#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/io.h>

#define DRV_PREFIX "pd"
#include "common.h"
#include "network_driver.h"

/* TODO: Change the following defines as per your NIC */
#define PD_VENDOR_ID PCI_VENDOR_ID_REALTEK
#define PD_PRODUCT_ID 0x8136
#define PD_BAR_NO 2

#if 0
static void display_pci_config_space(struct pci_dev *dev)
{
	int i;
	uint8_t b;
	uint16_t w;
	uint32_t dw;

	for (i = 0; i < 6; i++)
	{
		iprintk("Bar %d: 0x%016llX-%016llX : %08lX : %s %s\n", i,
			(unsigned long long)(pci_resource_start(dev, i)),
			(unsigned long long)(pci_resource_end(dev, i)),
			pci_resource_flags(dev, i),
			pci_resource_flags(dev, i) & PCI_BASE_ADDRESS_SPACE_IO ? " IO" : "MEM",
			pci_resource_flags(dev, i) & PCI_BASE_ADDRESS_MEM_PREFETCH ? "PREFETCH" : "NON-PREFETCH");
	}

	iprintk("PCI Configuration Space:\n");
	pci_read_config_word(dev, 0, &w); printk("%04X:", w);
	pci_read_config_word(dev, 2, &w); printk("%04X:", w);
	pci_read_config_word(dev, 4, &w); printk("%04X:", w);
	pci_read_config_word(dev, 6, &w); printk("%04X:", w);
	pci_read_config_dword(dev, 8, &dw);
	printk("%02X:", dw & 0xFF); printk("%06X:", dw >> 8);
	pci_read_config_byte(dev, 12, &b); printk("%02X:", b);
	pci_read_config_byte(dev, 13, &b); printk("%02X:", b);
	pci_read_config_byte(dev, 14, &b); printk("%02X:", b);
	pci_read_config_byte(dev, 15, &b); printk("%02X\n", b);

	pci_read_config_dword(dev, 16, &dw); printk("%08X:", dw);
	pci_read_config_dword(dev, 20, &dw); printk("%08X:", dw);
	pci_read_config_dword(dev, 24, &dw); printk("%08X:", dw);
	pci_read_config_dword(dev, 28, &dw); printk("%08X\n", dw);

	pci_read_config_dword(dev, 32, &dw); printk("%08X:", dw);
	pci_read_config_dword(dev, 36, &dw); printk("%08X:", dw);
	pci_read_config_dword(dev, 40, &dw); printk("%08X:", dw);
	pci_read_config_word(dev, 44, &w); printk("%04X:", w);
	pci_read_config_word(dev, 46, &w); printk("%04X\n", w);

	pci_read_config_dword(dev, 48, &dw); printk("%08X:", dw);
	pci_read_config_dword(dev, 52, &dw); printk("%08X:", dw);
	pci_read_config_dword(dev, 56, &dw); printk("%08X:", dw);
	pci_read_config_byte(dev, 60, &b); printk("%02X:", b);
	pci_read_config_byte(dev, 61, &b); printk("%02X:", b);
	pci_read_config_byte(dev, 62, &b); printk("%02X:", b);
	pci_read_config_byte(dev, 63, &b); printk("%02X\n", b);
}
#endif

static int pd_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	void __iomem *reg_base;
	int ret;

	ret = pci_enable_device(dev);
	if (ret)
	{
		eprintk("Unable to enable this PCI Network device\n");
		return ret;
	}
	else
	{
		iprintk("PCI Network device enabled\n");
	}

	//display_pci_config_space(dev);

	ret = pci_request_regions(dev, "pn");
	if (ret)
	{
		eprintk("Unable to acquire regions of this PCI Network device\n");
		pci_disable_device(dev);
		return ret;
	}
	else
	{
		iprintk("PCI Network device regions acquired\n");
	}

	if ((reg_base = ioremap(pci_resource_start(dev, PD_BAR_NO), pci_resource_len(dev, PD_BAR_NO))) == NULL)
	{
		eprintk("Unable to map registers of this PCI Network device\n");
		pci_release_regions(dev);
		pci_disable_device(dev);
		return -ENODEV;
	}
	iprintk("Register Base: %p\n", reg_base);

	iprintk("IRQ: %u\n", dev->irq);

	pci_set_drvdata(dev, reg_base);

	ret = nd_init(dev);
	if (ret)
	{
		eprintk("Unable to map registers of this PCI Network device\n");
		iounmap(reg_base);
		pci_release_regions(dev);
		pci_disable_device(dev);
		return ret;
	}

	iprintk("PCI Network device registered\n");

	return 0;
}

static void pd_remove(struct pci_dev *dev)
{
	void __iomem *reg_base;

	nd_exit(dev);

	reg_base = pci_get_drvdata(dev);
	pci_set_drvdata(dev, NULL);

	iounmap(reg_base);
	iprintk("PCI Network device memory unmapped\n");

	pci_release_regions(dev);
	iprintk("PCI Network device regions released\n");

	pci_disable_device(dev);
	iprintk("PCI Network device disabled\n");

	iprintk("PCI Network device unregistered\n");
}

/* Table of devices that work with this driver */
static struct pci_device_id pd_table[] =
{
	{
		PCI_DEVICE(PD_VENDOR_ID, PD_PRODUCT_ID)
	},
	{} /* Terminating entry */
};
MODULE_DEVICE_TABLE (pci, pd_table);

static struct pci_driver pci_drv =
{
	.name = "pn",
	.probe = pd_probe,
	.remove = pd_remove,
	.id_table = pd_table,
};

static int __init pd_pci_init(void)
{
	int result;

	/* Register this driver with the PCI subsystem */
	if ((result = pci_register_driver(&pci_drv)))
	{
		eprintk("pci_register_driver failed. Error number %d\n", result);
	}
	iprintk("PCI Network driver registered\n");
	return result;
}

static void __exit pd_pci_exit(void)
{
	/* Deregister this driver with the PCI subsystem */
	pci_unregister_driver(&pci_drv);
	iprintk("PCI Network driver unregistered\n");
}

module_init(pd_pci_init);
module_exit(pd_pci_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <anil@sysplay.in>");
MODULE_DESCRIPTION("PCI Network Device Driver");
