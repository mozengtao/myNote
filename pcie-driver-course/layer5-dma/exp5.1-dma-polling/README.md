# Experiment 5.1 — Coherent DMA Transfer with Polling Completion

## 1. Why

在引入中断系统（Layer 6）之前，我们先把数据传输链路做纯粹。
本实验实现一个完整的端到端 DMA 闭环：
`pci_set_master` -> `dma_set_mask_and_coherent` -> `dma_alloc_coherent` -> 发起硬件 DMA 搬运 -> 轮询完成 -> 校验数据。

## 2. Current Mental Model

我们已经能够通过 MMIO（`readl`/`writel`）精准操控设备的控制寄存器。
现在我们要操控设备的 DMA 控制器，让它直接从我们的宿主内存读取数据，再写回我们的内存。

## 3. New Concept

本实验唯一新增的核心概念是：**使用 `pci_set_master()` 授予总线主控权，使用 `dma_alloc_coherent()` 获取具有总线寻址能力的物理内存，并建立 Buffer Ownership 轮询转移机制**。

## 4. Architecture

```
+-------------------------------------------------------------+
| System RAM (Coherent DMA Buffer - 4KB)                      |
|                                                             |
| [0x000..0x0ff] TX Buffer: 包含测试字符串 "Hello PCIe DMA!"   |
| [0x100..0x1ff] RX Buffer: 初始为空，准备接收回传数据         |
+-------------------------------------------------------------+
          ^                                           ^
          | (Step 1: DMA Read)                        | (Step 2: DMA Write)
          v                                           v
+-------------------------------------------------------------+
| QEMU edu PCIe Device (Bus Master)                           |
|                                                             |
| 内部 4KB SRAM 暂存区 (Offset 0x40000)                        |
|                                                             |
| BAR0 DMA 控制寄存器:                                        |
| 0x80: SRC  | 0x88: DST  | 0x90: CNT  | 0x98: CMD (RUN/DIR)  |
+-------------------------------------------------------------+
```

## 5. Data Flow

```
1. CPU 写入: tx_buf = "Hello PCIe DMA: Learning Linux Driver Architecture!"
2. 所有权移交 -> Device Owned
3. DMA 阶段 1: 内存 -> 设备内部
   - MMIO 设置: SRC = dma_handle (RAM), DST = 0x40000 (Device SRAM), CNT = 64
   - MMIO 触发: CMD = RUN | DIR_FROM_PCI (0x1 | 0x0)
   - 轮询等待 CMD 的 bit 0 归零
4. DMA 阶段 2: 设备内部 -> 内存
   - MMIO 设置: SRC = 0x40000 (Device SRAM), DST = dma_handle + 256 (RAM), CNT = 64
   - MMIO 触发: CMD = RUN | DIR_TO_PCI (0x1 | 0x2 = 0x3)
   - 轮询等待 CMD 的 bit 0 归零
5. 所有权归还 -> CPU Owned
6. CPU 读取: 对比 rx_buf 与 tx_buf 的 64 字节数据
```

## 6. Control Flow

```
edu_dma_probe(pdev, id)
      |
      +--> pci_enable_device(pdev)
      +--> pci_set_master(pdev)                 [关键：设置 Command 寄存器 Bus Master 位]
      +--> dma_set_mask_and_coherent(28位) [关键：edu 设备只支持 28 位 DMA 寻址]
      +--> pci_request_regions() + pci_iomap()
      +--> dma_alloc_coherent()                 [分配一致性内存，获得 cpu_addr 与 dma_addr]
      +--> 填充待发数据，执行 Step 1 (RAM -> Device)
      +--> while (readl(REG_DMA_CMD) & 0x1) cpu_relax();
      +--> 执行 Step 2 (Device -> RAM)
      +--> while (readl(REG_DMA_CMD) & 0x1) cpu_relax();
      +--> memcmp 比较数据 -> 输出验证成功日志
```

## 7. State

```
IDLE
  |
  | CPU 写入内存
  v
CPU_PREPARED
  |
  | writel(REG_DMA_CMD, 0x1)
  v
DEVICE_OWNED
  |
  | 硬件状态变为传输中
  v
DMA_RUNNING
  |
  | 轮询检测到 bit 0 清 0
  v
DMA_DONE
  |
  | CPU 开始校验内存
  v
CPU_OWNED
```

## 8. Code

### edu_dma.c

```c
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
```

### Makefile

```makefile
obj-m += edu_dma.o

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
# test.sh - 验证 DMA 内存传输与回环校验

echo "=================================================="
echo " Experiment 5.1: Coherent DMA 轮询传输验证"
echo "=================================================="

insmod /root/extra/edu_dma.ko
dmesg | grep -A 8 "edu_dma: Coherent DMA buffer"
rmmod edu_dma
poweroff -f
```

## 9. Build

```bash
cd layer5-dma/exp5.1-dma-polling
make
```

## 10. Run

```bash
mkdir -p build
cp edu_dma.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

观察内核日志：
- `DMA Bus Address: 0x...` 是否在 28 位物理内存寻址范围内（即低于 256MB）。
- `Step 1 completed (RAM -> Device)` 与 `Step 2 completed (Device -> RAM)` 顺利通过。
- `RX Data received: 'Hello PCIe DMA: Learning Linux Driver Architecture!'` 与 TX 完全一致。
- `SUCCESS: DMA loopback verification MATCHED perfectly!` 确认搬运成功。

## 12. Expected Result

```
edu_dma: Coherent DMA buffer allocated:
  CPU Virtual Address : ffff888005430000
  DMA Bus Address     : 0x5430000
edu_dma: TX Data prepared: 'Hello PCIe DMA: Learning Linux Driver Architecture!'
edu_dma: Step 1 completed (RAM -> Device)
edu_dma: Step 2 completed (Device -> RAM)
edu_dma: RX Data received: 'Hello PCIe DMA: Learning Linux Driver Architecture!'
edu_dma: >>> SUCCESS: DMA loopback verification MATCHED perfectly! <<<
```

## 13. Source Code

- `include/linux/dma-mapping.h`: `dma_alloc_coherent()`, `dma_free_coherent()`, `dma_set_mask_and_coherent()`。
- `kernel/dma/mapping.c`: 真实分配逻辑与 DMA 掩码过滤。
- `drivers/pci/pci.c`: `pci_set_master()`，负责开启 Command 寄存器中的 `PCI_COMMAND_MASTER` 位。

## 14. Failure Injection

### 忘记调用 `pci_set_master(pdev)`
如果注释掉 `pci_set_master(pdev)`：
- **Symptom**：DMA 命令下发后，`REG_DMA_CMD` 的运行位永远无法自动归零，轮询产生 `DMA transfer timeout!`。
- **Suspected Layer**：Layer 5（PCI Bus Master Permission）。
- **Root Cause**：PCI 配置空间 Command 寄存器的 `Bus Master` 位未置 1，硬件在物理层面被总线仲裁器封杀，禁止向宿主内存发送任何读写请求包！

## 15. Mental Model Summary

```
Before: 以为 DMA 只是内核里类似 memcpy 的一种高级函数
   |
New Concept: DMA 是外设作为主控主动读写宿主物理内存的硬件行为，
             必须声明 Bus Master、声明寻址 Mask、移交 Ownership 并使用 DMA 地址
   |
After: 建立了 CPU 准备 -> 移交硬件 -> 硬件搬运 -> 回收所有权 的严密 DMA 状态机
```
