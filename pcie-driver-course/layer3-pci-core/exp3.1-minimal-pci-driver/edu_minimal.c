/*
 * edu_minimal.c - Minimal PCI driver matching QEMU edu device (1234:11e8)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

/*
 * 设备 ID 表：内核用来匹配硬件
 * 必须以全 0 的条目结尾，否则会导致 PCI Core 遍历数组越界溢出！
 */
static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, } /* 终止符 */
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

/*
 * edu_probe - 设备与驱动匹配成功时由 PCI 核心调用
 */
static int
edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	pr_info("edu_minimal: probe() triggered for device %04x:%04x at %s\n",
		pdev->vendor, pdev->device, pci_name(pdev));
	return 0;
}

/*
 * edu_remove - 驱动注销或设备移除时调用
 */
static void
edu_remove(struct pci_dev *pdev)
{
	pr_info("edu_minimal: remove() triggered for device %s\n", pci_name(pdev));
}

static struct pci_driver edu_driver = {
	.name     = "edu_minimal",
	.id_table = edu_id_table,
	.probe    = edu_probe,
	.remove   = edu_remove,
};

/*
 * edu_init - 模块加载入口
 */
static int __init
edu_init(void)
{
	pr_info("edu_minimal: registering pci driver...\n");
	return pci_register_driver(&edu_driver);
}

/*
 * edu_exit - 模块卸载出口
 */
static void __exit
edu_exit(void)
{
	pr_info("edu_minimal: unregistering pci driver...\n");
	pci_unregister_driver(&edu_driver);
}

module_init(edu_init);
module_exit(edu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 3.1: Minimal PCI Driver matching QEMU edu");
