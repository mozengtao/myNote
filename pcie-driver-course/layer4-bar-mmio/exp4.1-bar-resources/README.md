# Experiment 4.1 — Inspecting BAR Physical Resources

## 1. Why

在驱动代码里调用 `pci_iomap()` 之前，我们必须清楚知道这个设备究竟声明了几个 BAR、属于哪种类型（Memory 还是 I/O 端口）、物理基地址从哪里开始、总共占用多少字节。
本实验建立对 `struct resource` 与硬件 BAR 的物理映射认知。

## 2. Current Mental Model

我们知道硬件在配置空间 0x10 处有 BAR0，操作系统枚举阶段已经算出了这块区域的大小并分配了物理地址。

## 3. New Concept

本实验唯一新增的核心概念是：**BAR 的内核结构表示 `pci_resource_start()`、`pci_resource_len()` 与 `pci_resource_flags()`，以及它们与物理硬件 Memory BAR 的精确对应**。

## 4. Architecture

```
+--------------------------------------------------------------+
| struct pci_dev                                               |
|                                                              |
| resource[0]:                                                 |
|   .start = 0xfeb90000 (物理起始地址)                         |
|   .end   = 0xfec8ffff (物理终止地址)                         |
|   .flags = IORESOURCE_MEM | IORESOURCE_PCI_FIXED             |
|                                                              |
| length   = end - start + 1 = 1048576 字节 (1 MB)             |
+--------------------------------------------------------------+
```

## 5. Data Flow

```
Hardware Config Space BAR0
          |
          | PCI Enumeration
          v
dev->resource[0]
          |
          +--> pci_resource_start(pdev, 0) -> 0xfeb90000
          +--> pci_resource_len(pdev, 0)   -> 0x00100000
          +--> pci_resource_flags(pdev, 0) -> IORESOURCE_MEM
```

## 6. Control Flow

```
edu_bar_probe(pdev, id)
         |
         v
检查 pci_resource_flags(pdev, 0) & IORESOURCE_MEM
         |
         v
获取起始地址: pci_resource_start(pdev, 0)
         |
         v
获取长度: pci_resource_len(pdev, 0)
         |
         v
打印并记录到 dmesg
```

## 7. State

```
DEVICE_PROBED
      |
      | 检查硬件资源
      v
RESOURCES_IDENTIFIED (确认 BAR0 为 1MB MMIO，尚未建立内核页表映射)
```

## 8. Code

### edu_bar_info.c

```c
/*
 * edu_bar_info.c - Print physical BAR resource information for QEMU edu device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

static int
edu_bar_info_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int i;

	pr_info("edu_bar_info: probing device %s\n", pci_name(pdev));

	/* 遍历 6 个标准 BAR (BAR0 .. BAR5) */
	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		resource_size_t start = pci_resource_start(pdev, i);
		resource_size_t len   = pci_resource_len(pdev, i);
		unsigned long   flags = pci_resource_flags(pdev, i);

		if (!len)
			continue;

		pr_info("edu_bar_info: BAR%d -> start: 0x%llx, len: 0x%llx (flags: 0x%lx, %s)\n",
			i,
			(unsigned long long)start,
			(unsigned long long)len,
			flags,
			(flags & IORESOURCE_MEM) ? "MMIO (Memory)" : "IO Port");
	}

	return 0;
}

static void
edu_bar_info_remove(struct pci_dev *pdev)
{
	pr_info("edu_bar_info: removing device %s\n", pci_name(pdev));
}

static struct pci_driver edu_bar_info_driver = {
	.name     = "edu_bar_info",
	.id_table = edu_id_table,
	.probe    = edu_bar_info_probe,
	.remove   = edu_bar_info_remove,
};

module_pci_driver(edu_bar_info_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 4.1: Inspect BAR Physical Resources");
```

### Makefile

```makefile
obj-m += edu_bar_info.o

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
# test.sh - 运行 BAR 资源打印实验

echo "=================================================="
echo " Experiment 4.1: BAR 物理资源解析验证"
echo "=================================================="

insmod /root/extra/edu_bar_info.ko
dmesg | grep "edu_bar_info: BAR"
rmmod edu_bar_info
poweroff -f
```

## 9. Build

```bash
cd layer4-bar-mmio/exp4.1-bar-resources
make
```

## 10. Run

```bash
mkdir -p build
cp edu_bar_info.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

观察内核日志中的输出行：
`edu_bar_info: BAR0 -> start: 0xfeb90000, len: 0x100000 (flags: 0x..., MMIO (Memory))`。
这直接证明了：
1. 只有 BAR0 具有有效长度（1MB）。
2. 其类型被准确判定为 `IORESOURCE_MEM`。

## 12. Expected Result

```
edu_bar_info: probing device 0000:00:04.0
edu_bar_info: BAR0 -> start: 0xfeb90000, len: 0x100000 (flags: 0x40200, MMIO (Memory))
edu_bar_info: removing device 0000:00:04.0
```

## 13. Source Code

- `include/linux/pci.h`: 宏 `pci_resource_start()`, `pci_resource_len()`, `pci_resource_flags()`。
- `include/linux/ioport.h`: `struct resource` 定义与标志位（`IORESOURCE_MEM`, `IORESOURCE_IO` 等）。

## 14. Failure Injection

### 如果访问未启用的 BAR（如 BAR1）
在代码中若强行去映射 `pci_resource_start(pdev, 1)`（其长度为 0）：
- **Symptom**：`pci_iomap()` 返回 `NULL`。
- **Root Cause**：硬件根本没有在 BAR1 声明地址窗口，长度为 0，内核拒绝为不存在的物理空间建立虚拟页表映射。

## 15. Mental Model Summary

```
Before: 只知道 lspci 打印了 Region 0
   |
New Concept: 在驱动内部，内核用 struct resource 记录每个 BAR 的物理基址与大小，
             必须先通过 pci_resource_flags 确认其为 MMIO 且 len > 0
   |
After: 具备了映射 BAR 之前的物理资源合法性校验能力
```
