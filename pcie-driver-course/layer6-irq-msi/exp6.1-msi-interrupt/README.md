# Experiment 6.1 — Interrupt-Driven DMA with MSI

## 1. Why

在真实的高性能驱动中，没有人会在 CPU 里写 `while (busy)` 傻等硬件。
本实验将 Layer 5 的 Polling 机制升级为 **MSI 中断驱动架构**。
驱动发起 DMA 请求后，使用 Linux 内核等待队列 `wait_for_completion()` 让当前进程进入睡眠；硬件搬运完毕触发 MSI 中断，CPU 跳入 ISR 回调，执行 ACK 并通过 `complete()` 唤醒等待进程。

## 2. Current Mental Model

我们已经实现过完整的 DMA 搬运与回环，但完成状态依赖软件在循环中不断读取 MMIO 寄存器。

## 3. New Concept

本实验唯一新增的核心概念是：**使用 `pci_alloc_irq_vectors(PCI_IRQ_MSI)` 申请中断向量，使用 `request_irq()` 挂载 ISR，并在硬件与驱动之间建立 Completion 异步唤醒链路**。

## 4. Architecture

```
+--------------------------------------------------------------+
| Thread Context (edu_msi_probe)                               |
|                                                              |
|  init_completion(&dma_done)                                  |
|  writel(EDU_DMA_RUN | EDU_DMA_IRQ, mmio + REG_DMA_CMD)       |
|  wait_for_completion_timeout(&dma_done, 1秒)                 |
|  [当前进程进入 TASK_UNINTERRUPTIBLE 休眠，释放 CPU]           |
+--------------------------------------------------------------+
                               |
                               | (硬件搬运中...)
                               v
+--------------------------------------------------------------+
| QEMU edu Hardware                                            |
|                                                              |
| DMA 传输完毕 -> 发送 MSI 报文 -> edu_raise_irq()              |
+--------------------------------------------------------------+
                               |
                               | MSI Memory Write TLP 到达 APIC
                               v
+--------------------------------------------------------------+
| Hard IRQ Context (edu_irq_handler - Top Half)                |
|                                                              |
|  1. readl(REG_IRQ_STATUS) 确认是否为 DMA 中断 (0x100)        |
|  2. writel(0x100, REG_IRQ_ACK) 清除硬件中断状态               |
|  3. complete(&dma_done) 唤醒睡眠的主线程                     |
|  4. return IRQ_HANDLED                                       |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| Thread Context 被唤醒，继续执行数据一致性校验                  |
+--------------------------------------------------------------+
```

## 5. Data Flow

```
1. 启动传输: writel(DMA_RUN | DMA_IRQ, mmio + 0x98)
2. 硬件处理: 内存 -> 设备内部缓冲区
3. 中断触发: 硬件拉起 MSI -> CPU 捕获中断
4. ISR 执行: 读取 0x24 (Status) -> 写入 0x64 (Ack) -> 唤醒主线程
5. 主线程恢复: 启动反向传输 (设备内部 -> 内存) 同样走中断唤醒
6. 数据校验: 确认回传数据 100% 正确
```

## 6. Control Flow

```
edu_msi_probe(pdev, id)
     |
     +--> pci_enable_device() + pci_set_master()
     +--> dma_set_mask_and_coherent()
     +--> pci_request_regions() + pci_iomap()
     |
     +--> 1. pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI)
     +--> 2. irq = pci_irq_vector(pdev, 0)
     +--> 3. request_irq(irq, edu_irq_handler, 0, "edu_msi", edu)
     |
     +--> 4. 发起 Step 1 DMA (带 EDU_DMA_IRQ 标志)
     +--> 5. wait_for_completion_timeout()  <-- 睡眠
     |           ^
     |           | 硬件完成，触发 edu_irq_handler() -> complete()
     |           |
     +--> 6. 被唤醒，发起 Step 2 DMA (带 EDU_DMA_IRQ 标志)
     +--> 7. wait_for_completion_timeout()  <-- 睡眠
     |           ^
     |           | 硬件完成，触发 edu_irq_handler() -> complete()
     |           |
     +--> 8. 被唤醒，memcmp 验证成功
     |
edu_msi_remove(pdev)
     |
     +--> free_irq(irq, edu)
     +--> pci_free_irq_vectors(pdev)
     +--> dma_free_coherent() + pci_iounmap() + pci_release_regions()
```

## 7. State

```
MSI_ALLOCATED (内核中断子系统建立好 IRQ 映射)
      |
      | request_irq()
      v
ISR_REGISTERED (CPU 准备好响应外部中断)
      |
      | 发起传输并 wait_for_completion()
      v
THREAD_SLEEPING (CPU 0% 负载)
      |
      | 硬件发出 MSI -> edu_irq_handler() 响应
      v
INTERRUPT_SERVICED (ACK 硬件并唤醒主线程)
      |
      v
TRANSFER_COMPLETED
```

## 8. Code

### edu_msi.c

```c
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
```

### Makefile

```makefile
obj-m += edu_msi.o

KERNELDIR ?= ../../kernel
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
```

### test.sh

```bash
#!/bin/sh
# test.sh - 验证基于 MSI 中断的异步 DMA 唤醒与传输正确性

echo "=================================================="
echo " Experiment 6.1: MSI 中断驱动 DMA 验证"
echo "=================================================="

insmod /root/extra/edu_msi.ko
echo ""
echo "[1] 查看 /proc/interrupts 中挂载的 edu_msi 中断计数:"
cat /proc/interrupts | grep -E "edu_msi|CPU"

echo ""
echo "[2] 查看内核日志确认中断通知与回环测试结果:"
dmesg | grep -A 7 "edu_msi: MSI vector allocated"

rmmod edu_msi
poweroff -f
```

## 9. Build

```bash
cd layer6-irq-msi/exp6.1-msi-interrupt
make
```

## 10. Run

```bash
mkdir -p build
cp edu_msi.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

- 观察 `/proc/interrupts`：
  `edu_msi` 对应的中断计数是否从 0 增加到了 2（阶段 1 和阶段 2 各产生一次 MSI 中断）。
- 确认 dmesg 中出现：
  `Step 1 completed via MSI interrupt notification!`
  `Step 2 completed via MSI interrupt notification!`
  `SUCCESS: MSI Interrupt-Driven DMA loopback PASSED!`

## 12. Expected Result

```
edu_msi: MSI vector allocated successfully, IRQ line = 24
edu_msi: Prepared TX: 'Interrupt-driven DMA with MSI works perfectly!'
edu_msi: Step 1 completed via MSI interrupt notification!
edu_msi: Step 2 completed via MSI interrupt notification!
edu_msi: RX Received: 'Interrupt-driven DMA with MSI works perfectly!'
edu_msi: >>> SUCCESS: MSI Interrupt-Driven DMA loopback PASSED! <<<
```

## 13. Source Code

- `drivers/pci/msi/msi.c`: `pci_alloc_irq_vectors_affinity()`
- `kernel/irq/manage.c`: `request_threaded_irq()`, `free_irq()`
- `include/linux/completion.h`: `init_completion()`, `wait_for_completion_timeout()`, `complete()`

## 14. Failure Injection

### 忘记向硬件写 ACK 寄存器
如果 ISR 中注释掉 `writel(DMA_IRQ_FLAG, edu->mmio + REG_IRQ_ACK)`：
- **Symptom**：系统可能会陷入“中断风暴（Interrupt Storm）”导致 CPU 100% 死锁，或者下一个 DMA 请求因中断引脚未释放而再也无法产生新的中断跳变，导致第二次 `wait_for_completion_timeout` 超时！
- **Suspected Layer**：Layer 6（Device Interrupt Acknowledgement）。
- **Root Cause**：外设硬件内部的 Interrupt Pending 锁存器未被复位，必须遵从芯片手册要求向清除寄存器写入特征位。

## 15. Mental Model Summary

```
Before: 驱动死等硬件，浪费系统算力
   |
New Concept: 驱动下发请求后休眠挂起，
             硬件 DMA 结束通过 MSI 报文向 CPU 注入硬中断，唤醒驱动线程
   |
After: 彻底解耦了 CPU 计算吞吐与外设异步 I/O 延迟
```
