#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/io.h>

#define DRV_PREFIX "fpd"
#include "common.h"

/* TODO: Change the following four defines as per your NIC */
#define FPD_VENDOR_ID PCI_VENDOR_ID_REALTEK
#define FPD_PRODUCT_ID 0x8136
#define FPD_BAR_NO 2

#define	MAC_ADDR_REG_START 0

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

void get_mac_addr(void __iomem *reg_base, unsigned char addr[6])
{
	int i;

	for (i = 0; i < 6; i++)
	{
		addr[i] = ioread8(reg_base + MAC_ADDR_REG_START + i);
	}
}

static int fpd_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	void __iomem *reg_base;
	int retval;
	unsigned char mac[6];

	retval = pci_enable_device(dev);
	if (retval)
	{
		eprintk("Unable to enable this PCI device\n");
		return retval;
	}
	else
	{
		iprintk("PCI device enabled\n");
	}

	display_pci_config_space(dev);

	retval = pci_request_regions(dev, "fpd_pci");
	if (retval)
	{
		eprintk("Unable to acquire regions of this PCI device\n");
		pci_disable_device(dev);
		return retval;
	}
	else
	{
		iprintk("PCI device regions acquired\n");
	}

	if ((reg_base = ioremap(pci_resource_start(dev, FPD_BAR_NO), pci_resource_len(dev, FPD_BAR_NO))) == NULL)
	{
		eprintk("Unable to map registers of this PCI device\n");
		pci_release_regions(dev);
		pci_disable_device(dev);
		return -ENODEV;
	}
	iprintk("Register Base: %p\n", reg_base);

	get_mac_addr(reg_base, mac);
	iprintk("MAC: %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	iprintk("IRQ: %u\n", dev->irq);

	pci_set_drvdata(dev, reg_base);

	iprintk("PCI device registered\n");

	return 0;
}

static void fpd_remove(struct pci_dev *dev)
{
	void __iomem *reg_base = pci_get_drvdata(dev);

	pci_set_drvdata(dev, NULL);

	iounmap(reg_base);
	iprintk("PCI device memory unmapped\n");

	pci_release_regions(dev);
	iprintk("PCI device regions released\n");

	pci_disable_device(dev);
	iprintk("PCI device disabled\n");

	iprintk("PCI device unregistered\n");
}

/* Table of devices that work with this driver */
static struct pci_device_id fpd_table[] =
{
	{
		PCI_DEVICE(FPD_VENDOR_ID, FPD_PRODUCT_ID)
	},
	{} /* Terminating entry */
};
MODULE_DEVICE_TABLE (pci, fpd_table);

static struct pci_driver pci_drv =
{
	.name = "fpd_pci",
	.probe = fpd_probe,
	.remove = fpd_remove,
	.id_table = fpd_table,
};

static int __init fpd_pci_init(void)
{
	int result;

	/* Register this driver with the PCI subsystem */
	if ((result = pci_register_driver(&pci_drv)))
	{
		eprintk("pci_register_driver failed. Error number %d\n", result);
	}
	iprintk("First PCI driver registered\n");
	return result;
}

static void __exit fpd_pci_exit(void)
{
	/* Deregister this driver with the PCI subsystem */
	pci_unregister_driver(&pci_drv);
	iprintk("First PCI driver unregistered\n");
}

module_init(fpd_pci_init);
module_exit(fpd_pci_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <anil@sysplay.in>");
MODULE_DESCRIPTION("First PCI Device Driver");
