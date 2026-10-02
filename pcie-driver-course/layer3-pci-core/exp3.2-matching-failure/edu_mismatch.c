/*
 * edu_mismatch.c - Demonstrates silent matching failure with wrong Device ID
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR_ID 0x1234
#define WRONG_DEVICE_ID 0x9999  /* 故意篡改：硬件实际是 0x11e8 */

static const struct pci_device_id edu_wrong_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, WRONG_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_wrong_id_table);

static int
edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	/* 预期绝不会执行到这里！ */
	pr_err("edu_mismatch: ERROR! probe() should NEVER be called!\n");
	return 0;
}

static void
edu_remove(struct pci_dev *pdev)
{
	pr_info("edu_mismatch: remove() called\n");
}

static struct pci_driver edu_mismatch_driver = {
	.name     = "edu_mismatch",
	.id_table = edu_wrong_id_table,
	.probe    = edu_probe,
	.remove   = edu_remove,
};

static int __init
edu_mismatch_init(void)
{
	pr_info("edu_mismatch: module loaded, registering driver with wrong ID (0x%04x:0x%04x)...\n",
		EDU_VENDOR_ID, WRONG_DEVICE_ID);
	return pci_register_driver(&edu_mismatch_driver);
}

static void __exit
edu_mismatch_exit(void)
{
	pr_info("edu_mismatch: module exiting...\n");
	pci_unregister_driver(&edu_mismatch_driver);
}

module_init(edu_mismatch_init);
module_exit(edu_mismatch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 3.2: Matching failure injection");
