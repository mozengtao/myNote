/*
 * edu_full_driver.c - Production-grade PCIe driver with explicit state machine
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/completion.h>
#include <linux/spinlock.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

#define REG_IDENT      0x00
#define REG_IRQ_STATUS 0x24
#define REG_IRQ_ACK    0x64
#define REG_DMA_SRC    0x80
#define REG_DMA_DST    0x88
#define REG_DMA_CNT    0x90
#define REG_DMA_CMD    0x98

#define DMA_IRQ_FLAG       0x100
#define EDU_DMA_RUN        0x1
#define EDU_DMA_DIR_TO_RAM 0x2
#define EDU_DMA_IRQ        0x4
#define EDU_DEVICE_RAM_OFF 0x40000

#define DMA_BUFFER_SIZE    4096
#define TEST_TRANSFER_LEN  64

/*
 * 驱动状态机枚举定义
 */
enum edu_state {
	STATE_RESET = 0,
	STATE_PROBED,
	STATE_RESOURCES_READY,
	STATE_READY,
	STATE_RUNNING,
	STATE_STOPPING,
	STATE_STOPPED,
	STATE_ERROR,
};

static const char *state_names[] = {
	"RESET",
	"PROBED",
	"RESOURCES_READY",
	"READY",
	"RUNNING",
	"STOPPING",
	"STOPPED",
	"ERROR",
};

struct edu_full_dev {
	struct pci_dev     *pdev;
	void __iomem       *mmio;
	void               *dma_cpu_addr;
	dma_addr_t          dma_bus_addr;
	int                 irq;
	struct completion   dma_done;
	enum edu_state      state;
	spinlock_t          state_lock;
};

static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

/*
 * edu_set_state - 状态机跃迁封装并打印日志
 */
static void
edu_set_state(struct edu_full_dev *edu, enum edu_state new_state)
{
	unsigned long flags;

	spin_lock_irqsave(&edu->state_lock, flags);
	pr_info("edu_full: State Transition: [%s] -> [%s]\n",
		state_names[edu->state], state_names[new_state]);
	edu->state = new_state;
	spin_unlock_irqrestore(&edu->state_lock, flags);
}

/*
 * edu_irq_handler - MSI 中断服务例程
 */
static irqreturn_t
edu_irq_handler(int irq, void *dev_id)
{
	struct edu_full_dev *edu = dev_id;
	u32 status;

	status = readl(edu->mmio + REG_IRQ_STATUS);
	if (!(status & DMA_IRQ_FLAG))
		return IRQ_NONE;

	writel(DMA_IRQ_FLAG, edu->mmio + REG_IRQ_ACK);
	complete(&edu->dma_done);

	return IRQ_HANDLED;
}

static int
edu_full_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_full_dev *edu;
	char *tx_buf, *rx_buf;
	unsigned long timeout_res;
	u32 ident;
	int ret;

	pr_info("edu_full: >>> probe() started for device %s <<<\n", pci_name(pdev));

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;

	edu->pdev = pdev;
	spin_lock_init(&edu->state_lock);
	init_completion(&edu->dma_done);
	pci_set_drvdata(pdev, edu);
	edu->state = STATE_RESET;

	/* [跃迁] RESET -> PROBED */
	edu_set_state(edu, STATE_PROBED);

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(28));
	if (ret)
		goto err_disable_device;

	ret = pci_request_regions(pdev, "edu_full");
	if (ret)
		goto err_disable_device;

	edu->mmio = pci_iomap(pdev, 0, 0);
	if (!edu->mmio) {
		ret = -EIO;
		goto err_release_regions;
	}

	ident = readl(edu->mmio + REG_IDENT);
	if (ident != 0x010000ed) {
		pr_err("edu_full: invalid hardware IDENT (0x%08x)\n", ident);
		ret = -ENODEV;
		goto err_unmap_bar;
	}

	/* [跃迁] PROBED -> RESOURCES_READY */
	edu_set_state(edu, STATE_RESOURCES_READY);

	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
	if (ret < 0)
		goto err_unmap_bar;

	edu->irq = pci_irq_vector(pdev, 0);
	ret = request_irq(edu->irq, edu_irq_handler, 0, "edu_full", edu);
	if (ret)
		goto err_free_vectors;

	edu->dma_cpu_addr = dma_alloc_coherent(&pdev->dev, DMA_BUFFER_SIZE,
					       &edu->dma_bus_addr, GFP_KERNEL);
	if (!edu->dma_cpu_addr) {
		ret = -ENOMEM;
		goto err_free_irq;
	}

	/* [跃迁] RESOURCES_READY -> READY */
	edu_set_state(edu, STATE_READY);

	/* [跃迁] READY -> RUNNING (正式处理业务) */
	edu_set_state(edu, STATE_RUNNING);

	/* 运行测试 DMA */
	tx_buf = edu->dma_cpu_addr;
	rx_buf = edu->dma_cpu_addr + 256;
	memset(edu->dma_cpu_addr, 0, DMA_BUFFER_SIZE);
	snprintf(tx_buf, TEST_TRANSFER_LEN, "Full PCIe Driver State Machine is Rock Solid!");

	reinit_completion(&edu->dma_done);
	writeq(edu->dma_bus_addr, edu->mmio + REG_DMA_SRC);
	writeq(EDU_DEVICE_RAM_OFF, edu->mmio + REG_DMA_DST);
	writeq(TEST_TRANSFER_LEN, edu->mmio + REG_DMA_CNT);
	writel(EDU_DMA_RUN | EDU_DMA_IRQ, edu->mmio + REG_DMA_CMD);

	timeout_res = wait_for_completion_timeout(&edu->dma_done, msecs_to_jiffies(1000));
	if (!timeout_res) {
		edu_set_state(edu, STATE_ERROR);
		ret = -ETIMEDOUT;
		goto err_free_dma;
	}

	reinit_completion(&edu->dma_done);
	writeq(EDU_DEVICE_RAM_OFF, edu->mmio + REG_DMA_SRC);
	writeq(edu->dma_bus_addr + 256, edu->mmio + REG_DMA_DST);
	writeq(TEST_TRANSFER_LEN, edu->mmio + REG_DMA_CNT);
	writel(EDU_DMA_RUN | EDU_DMA_DIR_TO_RAM | EDU_DMA_IRQ, edu->mmio + REG_DMA_CMD);

	timeout_res = wait_for_completion_timeout(&edu->dma_done, msecs_to_jiffies(1000));
	if (!timeout_res) {
		edu_set_state(edu, STATE_ERROR);
		ret = -ETIMEDOUT;
		goto err_free_dma;
	}

	if (memcmp(tx_buf, rx_buf, TEST_TRANSFER_LEN) != 0) {
		edu_set_state(edu, STATE_ERROR);
		ret = -EIO;
		goto err_free_dma;
	}

	pr_info("edu_full: >>> Driver in RUNNING state, self-test verified perfectly! <<<\n");
	return 0;

err_free_dma:
	dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE, edu->dma_cpu_addr, edu->dma_bus_addr);
err_free_irq:
	free_irq(edu->irq, edu);
err_free_vectors:
	pci_free_irq_vectors(pdev);
err_unmap_bar:
	pci_iounmap(pdev, edu->mmio);
err_release_regions:
	pci_release_regions(pdev);
err_disable_device:
	pci_clear_master(pdev);
	pci_disable_device(pdev);
	return ret;
}

static void
edu_full_remove(struct pci_dev *pdev)
{
	struct edu_full_dev *edu = pci_get_drvdata(pdev);

	pr_info("edu_full: >>> remove() started for %s <<<\n", pci_name(pdev));
	if (!edu)
		return;

	/* [跃迁] RUNNING -> STOPPING */
	edu_set_state(edu, STATE_STOPPING);

	/* 停止硬件 DMA */
	writel(0, edu->mmio + REG_DMA_CMD);

	/* [跃迁] STOPPING -> STOPPED */
	edu_set_state(edu, STATE_STOPPED);

	/* 逆序解构全部系统资源 */
	if (edu->dma_cpu_addr)
		dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE, edu->dma_cpu_addr, edu->dma_bus_addr);
	if (edu->irq)
		free_irq(edu->irq, edu);
	pci_free_irq_vectors(pdev);
	if (edu->mmio)
		pci_iounmap(pdev, edu->mmio);
	pci_release_regions(pdev);
	pci_clear_master(pdev);
	pci_disable_device(pdev);

	/* [跃迁] STOPPED -> RESET */
	edu_set_state(edu, STATE_RESET);
	pr_info("edu_full: Driver completely torn down, back in RESET state\n");
}

static struct pci_driver edu_full_driver = {
	.name     = "edu_full",
	.id_table = edu_id_table,
	.probe    = edu_full_probe,
	.remove   = edu_full_remove,
};

module_pci_driver(edu_full_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 8.1: Full PCIe driver with explicit state machine");
