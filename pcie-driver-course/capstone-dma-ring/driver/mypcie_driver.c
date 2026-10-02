/*
 * mypcie_driver.c - Complete Linux PCIe Driver with DMA Ring, MSI-X and ioctl interface
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/spinlock.h>

#include "mypcie_uapi.h"

#define VENDOR_ID 0x1234
#define DEVICE_ID 0x22e8

#define REG_MAGIC        0x00
#define REG_CONTROL      0x04
#define REG_STATUS       0x08
#define REG_RING_BASE_LO 0x10
#define REG_RING_BASE_HI 0x14
#define REG_RING_SIZE    0x18
#define REG_PRODUCER     0x20
#define REG_CONSUMER     0x24
#define REG_DOORBELL     0x28
#define REG_IRQ_STATUS   0x30
#define REG_IRQ_ACK      0x34

#define CTRL_ENABLE      0x1
#define CTRL_RING_START  0x2
#define STAT_READY       0x1
#define STAT_RING_ACTIVE 0x2

#define RING_ENTRIES     16
#define BUF_SIZE         256

/*
 * 硬件环形描述符定义 (严格 16 字节对齐)
 */
struct __packed my_hw_desc {
	__u64 dma_addr;
	__u32 length;
	__u32 flags; /* bit 0: DIR, bit 1: DONE */
};

/*
 * 驱动状态机
 */
enum mypcie_state {
	STATE_RESET = 0,
	STATE_PROBED,
	STATE_RESOURCES_READY,
	STATE_READY,
	STATE_RUNNING,
	STATE_STOPPING,
	STATE_STOPPED,
	STATE_ERROR,
};

static const char *mypcie_state_names[] = {
	"RESET", "PROBED", "RESOURCES_READY", "READY",
	"RUNNING", "STOPPING", "STOPPED", "ERROR",
};

struct mypcie_dev {
	struct pci_dev         *pdev;
	void __iomem           *mmio;
	enum mypcie_state       state;
	spinlock_t              lock;

	/* 字符设备接口 */
	dev_t                   devt;
	struct cdev             cdev;
	struct class           *class;

	/* MSI-X 资源 */
	int                     msix_irq;

	/* DMA 环形缓冲区资源 */
	struct my_hw_desc      *ring_cpu;
	dma_addr_t              ring_dma;
	uint32_t                producer;

	/* 负载数据缓冲区 (每个描述符对应一个) */
	void                   *buf_cpu[RING_ENTRIES];
	dma_addr_t              buf_dma[RING_ENTRIES];
	struct completion       req_done[RING_ENTRIES];
};

static const struct pci_device_id mypcie_id_table[] = {
	{ PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, mypcie_id_table);

static void
mypcie_set_state(struct mypcie_dev *dev, enum mypcie_state new_state)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	pr_info("mypcie: State [%s] -> [%s]\n",
		mypcie_state_names[dev->state], mypcie_state_names[new_state]);
	dev->state = new_state;
	spin_unlock_irqrestore(&dev->lock, flags);
}

/*
 * MSI-X 中断服务例程 (Vector 0: Ring Completion)
 */
static irqreturn_t
mypcie_msix_handler(int irq, void *data)
{
	struct mypcie_dev *dev = data;
	uint32_t consumer;
	int i;

	/* ACK 硬件中断 */
	writel(0x1, dev->mmio + REG_IRQ_ACK);

	consumer = readl(dev->mmio + REG_CONSUMER);

	/* 遍历并唤醒已完成的描述符请求 */
	for (i = 0; i < RING_ENTRIES; i++) {
		if (dev->ring_cpu[i].flags & 0x2) { /* DONE bit */
			complete(&dev->req_done[i]);
		}
	}

	return IRQ_HANDLED;
}

static long
mypcie_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mypcie_dev *dev = file->private_data;
	struct mypcie_ioc_transfer req;
	uint32_t slot;
	unsigned long timeout_res;

	if (cmd != MYPCIE_IOCTL_SUBMIT_REQ)
		return -EINVAL;

	if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
		return -EFAULT;

	if (dev->state != STATE_RUNNING)
		return -EBUSY;

	spin_lock(&dev->lock);
	slot = dev->producer % RING_ENTRIES;
	dev->producer++;

	/* 准备数据 */
	if (req.direction == 1) {
		memcpy(dev->buf_cpu[slot], req.data, req.length);
	}

	/* 填充环形描述符 */
	dev->ring_cpu[slot].dma_addr = dev->buf_dma[slot];
	dev->ring_cpu[slot].length   = req.length;
	dev->ring_cpu[slot].flags    = req.direction & 0x1; /* 清零 DONE 位 */

	reinit_completion(&dev->req_done[slot]);

	/* 更新生产者并敲门铃 */
	writel(dev->producer, dev->mmio + REG_PRODUCER);
	writel(1, dev->mmio + REG_DOORBELL);
	spin_unlock(&dev->lock);

	/* 睡眠等待 MSI-X 唤醒 */
	timeout_res = wait_for_completion_interruptible_timeout(&dev->req_done[slot], msecs_to_jiffies(2000));
	if (timeout_res == 0) {
		pr_err("mypcie: Ring DMA timeout on slot %u!\n", slot);
		return -ETIMEDOUT;
	} else if (timeout_res < 0) {
		return -ERESTARTSYS;
	}

	/* 如果是读模式，把硬件回填的数据拷贝回用户态 */
	if (req.direction == 0) {
		memcpy(req.data, dev->buf_cpu[slot], req.length);
		if (copy_to_user((void __user *)arg, &req, sizeof(req)))
			return -EFAULT;
	}

	return 0;
}

static int
mypcie_open(struct inode *inode, struct file *file)
{
	struct mypcie_dev *dev = container_of(inode->i_cdev, struct mypcie_dev, cdev);
	file->private_data = dev;
	return 0;
}

static const struct file_operations mypcie_fops = {
	.owner          = THIS_MODULE,
	.open           = mypcie_open,
	.unlocked_ioctl = mypcie_ioctl,
};

static int
mypcie_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct mypcie_dev *dev;
	uint32_t magic;
	int ret, i;

	pr_info("mypcie: >>> probe started for %s <<<\n", pci_name(pdev));

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->pdev = pdev;
	spin_lock_init(&dev->lock);
	pci_set_drvdata(pdev, dev);
	dev->state = STATE_RESET;
	mypcie_set_state(dev, STATE_PROBED);

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret)
		goto err_disable;

	ret = pci_request_regions(pdev, "mypcie");
	if (ret)
		goto err_disable;

	dev->mmio = pci_iomap(pdev, 0, 0);
	if (!dev->mmio) {
		ret = -EIO;
		goto err_release_regions;
	}

	magic = readl(dev->mmio + REG_MAGIC);
	if (magic != 0x50434945) {
		pr_err("mypcie: Invalid MAGIC (0x%08x)\n", magic);
		ret = -ENODEV;
		goto err_unmap;
	}

	mypcie_set_state(dev, STATE_RESOURCES_READY);

	/* 1. 分配并挂载 MSI-X 向量 */
	ret = pci_alloc_irq_vectors(pdev, 1, 2, PCI_IRQ_MSIX);
	if (ret < 0) {
		pr_err("mypcie: Failed to allocate MSI-X vectors\n");
		goto err_unmap;
	}
	dev->msix_irq = pci_irq_vector(pdev, 0);
	ret = request_irq(dev->msix_irq, mypcie_msix_handler, 0, "mypcie_msix", dev);
	if (ret)
		goto err_free_irq_vec;

	/* 2. 分配 DMA 环形描述符缓冲区 (Coherent) */
	dev->ring_cpu = dma_alloc_coherent(&pdev->dev,
					   RING_ENTRIES * sizeof(struct my_hw_desc),
					   &dev->ring_dma, GFP_KERNEL);
	if (!dev->ring_cpu) {
		ret = -ENOMEM;
		goto err_free_irq;
	}

	/* 3. 分配数据载荷 Buffer */
	for (i = 0; i < RING_ENTRIES; i++) {
		init_completion(&dev->req_done[i]);
		dev->buf_cpu[i] = dma_alloc_coherent(&pdev->dev, BUF_SIZE,
						     &dev->buf_dma[i], GFP_KERNEL);
		if (!dev->buf_cpu[i]) {
			ret = -ENOMEM;
			goto err_free_buffers;
		}
	}

	/* 4. 配置硬件环形队列寄存器 */
	writeq(dev->ring_dma, dev->mmio + REG_RING_BASE_LO);
	writel(RING_ENTRIES, dev->mmio + REG_RING_SIZE);
	writel(0, dev->mmio + REG_PRODUCER);
	writel(CTRL_ENABLE | CTRL_RING_START, dev->mmio + REG_CONTROL);

	mypcie_set_state(dev, STATE_READY);

	/* 5. 注册字符设备接口 /dev/mypcie0 */
	ret = alloc_chrdev_region(&dev->devt, 0, 1, "mypcie");
	if (ret)
		goto err_free_buffers;

	cdev_init(&dev->cdev, &mypcie_fops);
	dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev->cdev, dev->devt, 1);
	if (ret)
		goto err_unregister_chrdev;

	dev->class = class_create("mypcie");
	if (IS_ERR(dev->class)) {
		ret = PTR_ERR(dev->class);
		goto err_del_cdev;
	}
	device_create(dev->class, NULL, dev->devt, NULL, "mypcie0");

	mypcie_set_state(dev, STATE_RUNNING);
	pr_info("mypcie: >>> Driver initialization COMPLETE. /dev/mypcie0 is READY! <<<\n");
	return 0;

err_del_cdev:
	cdev_del(&dev->cdev);
err_unregister_chrdev:
	unregister_chrdev_region(dev->devt, 1);
err_free_buffers:
	for (i = 0; i < RING_ENTRIES; i++) {
		if (dev->buf_cpu[i])
			dma_free_coherent(&pdev->dev, BUF_SIZE, dev->buf_cpu[i], dev->buf_dma[i]);
	}
	dma_free_coherent(&pdev->dev, RING_ENTRIES * sizeof(struct my_hw_desc),
			  dev->ring_cpu, dev->ring_dma);
err_free_irq:
	free_irq(dev->msix_irq, dev);
err_free_irq_vec:
	pci_free_irq_vectors(pdev);
err_unmap:
	pci_iounmap(pdev, dev->mmio);
err_release_regions:
	pci_release_regions(pdev);
err_disable:
	pci_clear_master(pdev);
	pci_disable_device(pdev);
	return ret;
}

static void
mypcie_remove(struct pci_dev *pdev)
{
	struct mypcie_dev *dev = pci_get_drvdata(pdev);
	int i;

	pr_info("mypcie: >>> remove started for %s <<<\n", pci_name(pdev));
	if (!dev)
		return;

	mypcie_set_state(dev, STATE_STOPPING);

	/* 停止硬件 Ring 传输 */
	writel(0, dev->mmio + REG_CONTROL);

	device_destroy(dev->class, dev->devt);
	class_destroy(dev->class);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(dev->devt, 1);

	mypcie_set_state(dev, STATE_STOPPED);

	for (i = 0; i < RING_ENTRIES; i++) {
		if (dev->buf_cpu[i])
			dma_free_coherent(&pdev->dev, BUF_SIZE, dev->buf_cpu[i], dev->buf_dma[i]);
	}
	dma_free_coherent(&pdev->dev, RING_ENTRIES * sizeof(struct my_hw_desc),
			  dev->ring_cpu, dev->ring_dma);

	free_irq(dev->msix_irq, dev);
	pci_free_irq_vectors(pdev);
	pci_iounmap(pdev, dev->mmio);
	pci_release_regions(pdev);
	pci_clear_master(pdev);
	pci_disable_device(pdev);

	mypcie_set_state(dev, STATE_RESET);
	pr_info("mypcie: Full teardown complete.\n");
}

static struct pci_driver mypcie_pci_driver = {
	.name     = "mypcie_driver",
	.id_table = mypcie_id_table,
	.probe    = mypcie_probe,
	.remove   = mypcie_remove,
};

module_pci_driver(mypcie_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Capstone: Full PCIe Driver with DMA Ring and MSI-X");
