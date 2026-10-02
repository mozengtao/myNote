/*
 * edu_bar_info.c - Print physical BAR resource information for QEMU edu device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

static int
edu_bar_info_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int i;

	pr_info("edu_bar_info: probing device %s\n", pci_name(pdev));

	/* 遍历 6 个标准 BAR (BAR0 .. BAR5) */
	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		resource_size_t start = pci_resource_start(pdev, i);
		resource_size_t len   = pci_resource_len(pdev, i);
		unsigned long   flags = pci_resource_flags(pdev, i);

		if (!len)
			continue;

		pr_info("edu_bar_info: BAR%d -> start: 0x%llx, len: 0x%llx (flags: 0x%lx, %s)\n",
			i,
			(unsigned long long)start,
			(unsigned long long)len,
			flags,
			(flags & IORESOURCE_MEM) ? "MMIO (Memory)" : "IO Port");
	}

	return 0;
}

static void
edu_bar_info_remove(struct pci_dev *pdev)
{
	pr_info("edu_bar_info: removing device %s\n", pci_name(pdev));
}

static struct pci_driver edu_bar_info_driver = {
	.name     = "edu_bar_info",
	.id_table = edu_id_table,
	.probe    = edu_bar_info_probe,
	.remove   = edu_bar_info_remove,
};

module_pci_driver(edu_bar_info_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 4.1: Inspect BAR Physical Resources");
