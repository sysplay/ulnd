#ifndef NETWORK_DRIVER_H
#define NETWORK_DRIVER_H

#ifdef __KERNEL__

#include <linux/pci.h>

int nd_init(struct pci_dev *pdev);
void nd_exit(struct pci_dev *pdev);

#endif

#endif
