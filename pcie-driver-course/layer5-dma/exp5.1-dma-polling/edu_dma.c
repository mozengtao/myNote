/*
 * edu_dma.c - Coherent DMA transfer with Polling completion on QEMU edu
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/delay.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

#define REG_DMA_SRC    0x80
#define REG_DMA_DST    0x88
#define REG_DMA_CNT    0x90
#define REG_DMA_CMD    0x98

#define EDU_DMA_RUN        0x1
#define EDU_DMA_DIR_TO_RAM 0x2
#define EDU_DEVICE_RAM_OFF 0x40000

#define DMA_BUFFER_SIZE    4096
#define TEST_TRANSFER_LEN  64

struct edu_dma_dev {
	struct pci_dev    *pdev;
	void __iomem      *mmio;
	void              *dma_cpu_addr;
	dma_addr_t         dma_bus_addr;
};

static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

/*
 * edu_poll_dma_complete - 轮询等待硬件清除 EDU_DMA_RUN 运行位
 */
static int
edu_poll_dma_complete(void __iomem *mmio)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	while (readl(mmio + REG_DMA_CMD) & EDU_DMA_RUN) {
		if (time_after(jiffies, timeout)) {
			pr_err("edu_dma: DMA transfer timeout!\n");
			return -ETIMEDOUT;
		}
		cpu_relax();
	}
	return 0;
}

static int
edu_dma_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_dma_dev *edu;
	char *tx_buf, *rx_buf;
	int ret;

	pr_info("edu_dma: probing device %s\n", pci_name(pdev));

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;
	edu->pdev = pdev;
	pci_set_drvdata(pdev, edu);

	/* 1. 使能 PCI 设备 */
	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	/* 2. 使能总线主控 (Bus Master) - 允许设备向主机发起 DMA 事务 */
	pci_set_master(pdev);

	/*
	 * 3. 设置 DMA 寻址掩码:
	 * QEMU edu 设备源码定义: edu->dma_mask = (1UL << 28) - 1;
	 * 表明该设备内部只实现了 28 位物理地址线！因此必须向内核声明 28-bit DMA mask
	 */
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(28));
	if (ret) {
		pr_err("edu_dma: failed to set 28-bit DMA mask (%d)\n", ret);
		goto err_disable;
	}

	/* 4. 请求物理资源并映射 MMIO */
	ret = pci_request_regions(pdev, "edu_dma");
	if (ret)
		goto err_disable;

	edu->mmio = pci_iomap(pdev, 0, 0);
	if (!edu->mmio) {
		ret = -EIO;
		goto err_release;
	}

	/*
	 * 5. 分配一致性 DMA 缓冲区 (Coherent DMA Buffer)
	 * dma_cpu_addr: CPU 读写使用的虚拟内存地址
	 * dma_bus_addr: 设备硬件发起 DMA 事务时寻址使用的 PCI 物理总线地址
	 */
	edu->dma_cpu_addr = dma_alloc_coherent(&pdev->dev, DMA_BUFFER_SIZE,
					       &edu->dma_bus_addr, GFP_KERNEL);
	if (!edu->dma_cpu_addr) {
		pr_err("edu_dma: failed to allocate coherent DMA buffer\n");
		ret = -ENOMEM;
		goto err_unmap;
	}

	pr_info("edu_dma: Coherent DMA buffer allocated:\n");
	pr_info("  CPU Virtual Address : %p\n", edu->dma_cpu_addr);
	pr_info("  DMA Bus Address     : 0x%llx\n", (unsigned long long)edu->dma_bus_addr);

	tx_buf = edu->dma_cpu_addr;
	rx_buf = edu->dma_cpu_addr + 256;

	/* 6. CPU 拥有缓冲区，准备发送数据 */
	memset(edu->dma_cpu_addr, 0, DMA_BUFFER_SIZE);
	snprintf(tx_buf, TEST_TRANSFER_LEN, "Hello PCIe DMA: Learning Linux Driver Architecture!");
	pr_info("edu_dma: TX Data prepared: '%s'\n", tx_buf);

	/* 7. 所有权移交给设备: 发起 DMA 阶段 1 (RAM -> Device 内部缓冲区) */
	writeq(edu->dma_bus_addr, edu->mmio + REG_DMA_SRC);
	writeq(EDU_DEVICE_RAM_OFF, edu->mmio + REG_DMA_DST);
	writeq(TEST_TRANSFER_LEN, edu->mmio + REG_DMA_CNT);
	writel(EDU_DMA_RUN, edu->mmio + REG_DMA_CMD); /* bit 0 = 1, bit 1 = 0 */

	ret = edu_poll_dma_complete(edu->mmio);
	if (ret)
		goto err_free_dma;

	pr_info("edu_dma: Step 1 completed (RAM -> Device)\n");

	/* 8. 发起 DMA 阶段 2 (Device 内部缓冲区 -> RAM rx_buf) */
	writeq(EDU_DEVICE_RAM_OFF, edu->mmio + REG_DMA_SRC);
	writeq(edu->dma_bus_addr + 256, edu->mmio + REG_DMA_DST);
	writeq(TEST_TRANSFER_LEN, edu->mmio + REG_DMA_CNT);
	writel(EDU_DMA_RUN | EDU_DMA_DIR_TO_RAM, edu->mmio + REG_DMA_CMD); /* bit 0=1, bit 1=1 */

	ret = edu_poll_dma_complete(edu->mmio);
	if (ret)
		goto err_free_dma;

	pr_info("edu_dma: Step 2 completed (Device -> RAM)\n");

	/* 9. 所有权归还 CPU: 校验数据一致性 */
	pr_info("edu_dma: RX Data received: '%s'\n", rx_buf);
	if (memcmp(tx_buf, rx_buf, TEST_TRANSFER_LEN) == 0) {
		pr_info("edu_dma: >>> SUCCESS: DMA loopback verification MATCHED perfectly! <<<\n");
	} else {
		pr_err("edu_dma: >>> ERROR: DMA loopback data mismatch! <<<\n");
		ret = -EIO;
		goto err_free_dma;
	}

	return 0;

err_free_dma:
	dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE, edu->dma_cpu_addr, edu->dma_bus_addr);
err_unmap:
	pci_iounmap(pdev, edu->mmio);
err_release:
	pci_release_regions(pdev);
err_disable:
	pci_clear_master(pdev);
	pci_disable_device(pdev);
	return ret;
}

static void
edu_dma_remove(struct pci_dev *pdev)
{
	struct edu_dma_dev *edu = pci_get_drvdata(pdev);

	pr_info("edu_dma: removing device %s\n", pci_name(pdev));
	if (edu) {
		if (edu->dma_cpu_addr)
			dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE,
					  edu->dma_cpu_addr, edu->dma_bus_addr);
		if (edu->mmio)
			pci_iounmap(pdev, edu->mmio);
		pci_release_regions(pdev);
		pci_clear_master(pdev);
		pci_disable_device(pdev);
	}
}

static struct pci_driver edu_dma_driver = {
	.name     = "edu_dma",
	.id_table = edu_id_table,
	.probe    = edu_dma_probe,
	.remove   = edu_dma_remove,
};

module_pci_driver(edu_dma_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 5.1: Coherent DMA transfer with polling");
