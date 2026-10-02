/*
 * edu_msi.c - Interrupt-driven DMA using MSI on QEMU edu device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/completion.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

#define REG_IRQ_STATUS 0x24  /* RO: 中断状态寄存器 */
#define REG_IRQ_ACK    0x64  /* WO: 中断应答/清除寄存器 */
#define REG_DMA_SRC    0x80
#define REG_DMA_DST    0x88
#define REG_DMA_CNT    0x90
#define REG_DMA_CMD    0x98

#define DMA_IRQ_FLAG       0x100 /* bit 8: 表示 DMA 中断 */
#define EDU_DMA_RUN        0x1
#define EDU_DMA_DIR_TO_RAM 0x2
#define EDU_DMA_IRQ        0x4   /* 传输完毕触发中断使能位 */
#define EDU_DEVICE_RAM_OFF 0x40000

#define DMA_BUFFER_SIZE    4096
#define TEST_TRANSFER_LEN  64

struct edu_msi_dev {
	struct pci_dev     *pdev;
	void __iomem       *mmio;
	void               *dma_cpu_addr;
	dma_addr_t          dma_bus_addr;
	int                 irq;
	struct completion   dma_done;
};

static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

/*
 * edu_irq_handler - MSI 中断服务例程 (Top Half)
 */
static irqreturn_t
edu_irq_handler(int irq, void *dev_id)
{
	struct edu_msi_dev *edu = dev_id;
	u32 status;

	/* 1. 检查中断状态寄存器，确认是否是属于我们设备的 DMA 中断 */
	status = readl(edu->mmio + REG_IRQ_STATUS);
	if (!(status & DMA_IRQ_FLAG))
		return IRQ_NONE;

	/* 2. 硬件必须显式 ACK，否则中断信号不会撤下 */
	writel(DMA_IRQ_FLAG, edu->mmio + REG_IRQ_ACK);

	/* 3. 唤醒正在 wait_for_completion 的驱动主线程 */
	complete(&edu->dma_done);

	return IRQ_HANDLED;
}

static int
edu_msi_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_msi_dev *edu;
	char *tx_buf, *rx_buf;
	unsigned long timeout_res;
	int ret;

	pr_info("edu_msi: probing device %s\n", pci_name(pdev));

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;
	edu->pdev = pdev;
	init_completion(&edu->dma_done);
	pci_set_drvdata(pdev, edu);

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(28));
	if (ret)
		goto err_disable;

	ret = pci_request_regions(pdev, "edu_msi");
	if (ret)
		goto err_disable;

	edu->mmio = pci_iomap(pdev, 0, 0);
	if (!edu->mmio) {
		ret = -EIO;
		goto err_release;
	}

	/*
	 * 1. 申请分配 MSI 中断向量 (使用现代统一 API pci_alloc_irq_vectors)
	 */
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
	if (ret < 0) {
		pr_err("edu_msi: failed to allocate MSI vector (%d)\n", ret);
		goto err_unmap;
	}

	/* 获取分配到的内核全局 IRQ 编号 */
	edu->irq = pci_irq_vector(pdev, 0);
	pr_info("edu_msi: MSI vector allocated successfully, IRQ line = %d\n", edu->irq);

	/*
	 * 2. 挂载中断服务例程 (ISR)
	 */
	ret = request_irq(edu->irq, edu_irq_handler, 0, "edu_msi", edu);
	if (ret) {
		pr_err("edu_msi: failed to request IRQ %d (%d)\n", edu->irq, ret);
		goto err_free_vectors;
	}

	/* 分配 DMA 缓冲区 */
	edu->dma_cpu_addr = dma_alloc_coherent(&pdev->dev, DMA_BUFFER_SIZE,
					       &edu->dma_bus_addr, GFP_KERNEL);
	if (!edu->dma_cpu_addr) {
		ret = -ENOMEM;
		goto err_free_irq;
	}

	tx_buf = edu->dma_cpu_addr;
	rx_buf = edu->dma_cpu_addr + 256;
	memset(edu->dma_cpu_addr, 0, DMA_BUFFER_SIZE);
	snprintf(tx_buf, TEST_TRANSFER_LEN, "Interrupt-driven DMA with MSI works perfectly!");
	pr_info("edu_msi: Prepared TX: '%s'\n", tx_buf);

	/*
	 * 3. 发起 DMA 阶段 1 (RAM -> Device)，开启 EDU_DMA_IRQ 标志
	 */
	reinit_completion(&edu->dma_done);
	writeq(edu->dma_bus_addr, edu->mmio + REG_DMA_SRC);
	writeq(EDU_DEVICE_RAM_OFF, edu->mmio + REG_DMA_DST);
	writeq(TEST_TRANSFER_LEN, edu->mmio + REG_DMA_CNT);
	writel(EDU_DMA_RUN | EDU_DMA_IRQ, edu->mmio + REG_DMA_CMD);

	/* 驱动进入睡眠，等待中断唤醒 */
	timeout_res = wait_for_completion_timeout(&edu->dma_done, msecs_to_jiffies(1000));
	if (!timeout_res) {
		pr_err("edu_msi: Step 1 timed out waiting for MSI interrupt!\n");
		ret = -ETIMEDOUT;
		goto err_free_dma;
	}
	pr_info("edu_msi: Step 1 completed via MSI interrupt notification!\n");

	/*
	 * 4. 发起 DMA 阶段 2 (Device -> RAM)，同样开启 EDU_DMA_IRQ
	 */
	reinit_completion(&edu->dma_done);
	writeq(EDU_DEVICE_RAM_OFF, edu->mmio + REG_DMA_SRC);
	writeq(edu->dma_bus_addr + 256, edu->mmio + REG_DMA_DST);
	writeq(TEST_TRANSFER_LEN, edu->mmio + REG_DMA_CNT);
	writel(EDU_DMA_RUN | EDU_DMA_DIR_TO_RAM | EDU_DMA_IRQ, edu->mmio + REG_DMA_CMD);

	timeout_res = wait_for_completion_timeout(&edu->dma_done, msecs_to_jiffies(1000));
	if (!timeout_res) {
		pr_err("edu_msi: Step 2 timed out waiting for MSI interrupt!\n");
		ret = -ETIMEDOUT;
		goto err_free_dma;
	}
	pr_info("edu_msi: Step 2 completed via MSI interrupt notification!\n");

	/* 5. 校验数据 */
	pr_info("edu_msi: RX Received: '%s'\n", rx_buf);
	if (memcmp(tx_buf, rx_buf, TEST_TRANSFER_LEN) == 0) {
		pr_info("edu_msi: >>> SUCCESS: MSI Interrupt-Driven DMA loopback PASSED! <<<\n");
	} else {
		pr_err("edu_msi: >>> ERROR: Data mismatch! <<<\n");
		ret = -EIO;
		goto err_free_dma;
	}

	return 0;

err_free_dma:
	dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE, edu->dma_cpu_addr, edu->dma_bus_addr);
err_free_irq:
	free_irq(edu->irq, edu);
err_free_vectors:
	pci_free_irq_vectors(pdev);
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
edu_msi_remove(struct pci_dev *pdev)
{
	struct edu_msi_dev *edu = pci_get_drvdata(pdev);

	pr_info("edu_msi: removing device %s\n", pci_name(pdev));
	if (edu) {
		if (edu->dma_cpu_addr)
			dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE,
					  edu->dma_cpu_addr, edu->dma_bus_addr);
		if (edu->irq)
			free_irq(edu->irq, edu);
		pci_free_irq_vectors(pdev);
		if (edu->mmio)
			pci_iounmap(pdev, edu->mmio);
		pci_release_regions(pdev);
		pci_clear_master(pdev);
		pci_disable_device(pdev);
	}
}

static struct pci_driver edu_msi_driver = {
	.name     = "edu_msi",
	.id_table = edu_id_table,
	.probe    = edu_msi_probe,
	.remove   = edu_msi_remove,
};

module_pci_driver(edu_msi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 6.1: MSI Interrupt-Driven DMA");
