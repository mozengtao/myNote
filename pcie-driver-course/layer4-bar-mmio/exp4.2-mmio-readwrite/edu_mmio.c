/*
 * edu_mmio.c - Demonstrate BAR0 mapping and MMIO readl/writel on QEMU edu
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

#define REG_IDENT    0x00  /* RO: 0x010000ed */
#define REG_LIVENESS 0x04  /* RW: 返回写入值的取反值 ~val */

struct edu_dev {
	struct pci_dev    *pdev;
	void __iomem      *mmio;
};

static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

static int
edu_mmio_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_dev *edu;
	u32 ident, test_val, resp_val;
	int ret;

	pr_info("edu_mmio: probe starting for %s\n", pci_name(pdev));

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;
	edu->pdev = pdev;
	pci_set_drvdata(pdev, edu);

	/* 1. 使能 PCI 设备 (打开 Command 寄存器的 Memory 译码与电源) */
	ret = pci_enable_device(pdev);
	if (ret) {
		pr_err("edu_mmio: failed to enable PCI device (%d)\n", ret);
		return ret;
	}

	/* 2. 申请独占 BAR0 物理区域 */
	ret = pci_request_regions(pdev, "edu_mmio");
	if (ret) {
		pr_err("edu_mmio: failed to request PCI regions (%d)\n", ret);
		goto err_disable;
	}

	/* 3. 映射 BAR0 到内核虚拟地址空间 (长度传 0 表示映射整个 BAR0) */
	edu->mmio = pci_iomap(pdev, 0, 0);
	if (!edu->mmio) {
		pr_err("edu_mmio: failed to iomap BAR0\n");
		ret = -EIO;
		goto err_release;
	}
	pr_info("edu_mmio: BAR0 mapped at kernel virtual address %p\n", edu->mmio);

	/* 4. 测试读取 IDENT 寄存器 */
	ident = readl(edu->mmio + REG_IDENT);
	pr_info("edu_mmio: read IDENT = 0x%08x (expected: 0x010000ed)\n", ident);
	if (ident != 0x010000ed) {
		pr_err("edu_mmio: unexpected IDENT value!\n");
		ret = -ENODEV;
		goto err_unmap;
	}

	/* 5. 测试写入并回读 LIVENESS 寄存器 */
	test_val = 0x12345678;
	pr_info("edu_mmio: writing 0x%08x to LIVENESS register...\n", test_val);
	writel(test_val, edu->mmio + REG_LIVENESS);

	resp_val = readl(edu->mmio + REG_LIVENESS);
	pr_info("edu_mmio: read back LIVENESS = 0x%08x (expected: ~0x%08x = 0x%08x)\n",
		resp_val, test_val, ~test_val);

	if (resp_val != ~test_val) {
		pr_err("edu_mmio: LIVENESS test failed!\n");
		ret = -EIO;
		goto err_unmap;
	}

	pr_info("edu_mmio: MMIO readl/writel verification PASSED successfully!\n");
	return 0;

err_unmap:
	pci_iounmap(pdev, edu->mmio);
err_release:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void
edu_mmio_remove(struct pci_dev *pdev)
{
	struct edu_dev *edu = pci_get_drvdata(pdev);

	pr_info("edu_mmio: removing device %s\n", pci_name(pdev));
	if (edu) {
		pci_iounmap(pdev, edu->mmio);
		pci_release_regions(pdev);
		pci_disable_device(pdev);
	}
}

static struct pci_driver edu_mmio_driver = {
	.name     = "edu_mmio",
	.id_table = edu_id_table,
	.probe    = edu_mmio_probe,
	.remove   = edu_mmio_remove,
};

module_pci_driver(edu_mmio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 4.2: MMIO readl/writel demonstration");
