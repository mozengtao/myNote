/*
 * edu_hotplug.c - Demonstrates dynamic hotplug addition and removal robustness
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

#define REG_IDENT 0x00

struct edu_hotplug_dev {
	struct pci_dev *pdev;
	void __iomem   *mmio;
};

static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

static int
edu_hotplug_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_hotplug_dev *edu;
	u32 ident;
	int ret;

	pr_info("edu_hotplug: >>> HOTPLUG EVENT: device ARRIVED at %s <<<\n", pci_name(pdev));

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;
	edu->pdev = pdev;
	pci_set_drvdata(pdev, edu);

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	ret = pci_request_regions(pdev, "edu_hotplug");
	if (ret)
		goto err_disable;

	edu->mmio = pci_iomap(pdev, 0, 0);
	if (!edu->mmio) {
		ret = -EIO;
		goto err_release;
	}

	ident = readl(edu->mmio + REG_IDENT);
	pr_info("edu_hotplug: Verified device hardware IDENT = 0x%08x\n", ident);

	return 0;

err_release:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void
edu_hotplug_remove(struct pci_dev *pdev)
{
	struct edu_hotplug_dev *edu = pci_get_drvdata(pdev);

	pr_info("edu_hotplug: >>> HOTPLUG EVENT: device REMOVED from %s <<<\n", pci_name(pdev));
	if (edu) {
		if (edu->mmio)
			pci_iounmap(pdev, edu->mmio);
		pci_release_regions(pdev);
		pci_disable_device(pdev);
	}
	pr_info("edu_hotplug: All resources released cleanly!\n");
}

static struct pci_driver edu_hotplug_driver = {
	.name     = "edu_hotplug",
	.id_table = edu_id_table,
	.probe    = edu_hotplug_probe,
	.remove   = edu_hotplug_remove,
};

module_pci_driver(edu_hotplug_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 7.1: PCIe Hotplug demonstration");
