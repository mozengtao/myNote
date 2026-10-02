# Experiment 4.2 — MMIO Mapping and Register Read/Write

## 1. Why

驱动只有真正向硬件寄存器发起读写，才能赋予硬件生命。
本实验实现 MMIO 访问的标准生命周期闭环：
`pci_enable_device` -> `pci_request_regions` -> `pci_iomap` -> `readl` / `writel`。
我们通过读取 IDENT 寄存器，并对 LIVENESS 寄存器写入测试值并回读其反转值，彻底打通 CPU 与 PCIe 设备之间的寄存器级交互。

## 2. Current Mental Model

我们已经知道 BAR0 对应的物理基地址，但 CPU 虚拟地址尚未建立映射，硬件 Command 寄存器尚未使能 Memory 译码。

## 3. New Concept

本实验唯一新增的核心概念是：**使用 `pci_enable_device()` 打开硬件地址译码开关，使用 `pci_iomap()` 建立虚拟内存页表，使用 `readl()`/`writel()` 执行严格有序的 MMIO 访问**。

## 4. Architecture

```
+--------------------------------------------------------------+
|                         CPU (Driver)                         |
|                                                              |
|   void __iomem *mmio = pci_iomap(pdev, 0, 0);                |
|                                                              |
|   writel(0x12345678, mmio + 0x04);  (写入 LIVENESS)           |
|   u32 val = readl(mmio + 0x04);     (读出 ~0x12345678)       |
+--------------------------------------------------------------+
                               |
                               | PCIe TLP (Memory Write / Read)
                               v
+--------------------------------------------------------------+
|                    QEMU Edu Device (BAR0)                    |
|                                                              |
|   Offset 0x00: IDENT    = 0x010000ed                         |
|   Offset 0x04: LIVENESS = ~写入值                            |
+--------------------------------------------------------------+
```

## 5. Data Flow

```
writel(0xa5a55a5a, mmio + 0x04)
        |
        | CPU 发起 Memory Write TLP 事务
        v
edu_mmio_write(addr=0x04, val=0xa5a55a5a) [QEMU 内部执行 edu->addr4 = ~val]
        |
        | 存储到虚拟硬件内部状态
        v
readl(mmio + 0x04)
        |
        | CPU 发起 Memory Read TLP 事务
        v
edu_mmio_read(addr=0x04) 返回 ~0xa5a55a5a = 0x5a5aa5a5
        |
        v
内核驱动校验: (val == ~0xa5a55a5a) ? 成功 : 失败
```

## 6. Control Flow

```
edu_mmio_probe(pdev, id)
       |
       +--> 1. pci_enable_device(pdev)        (激活配置空间 Command: Memory Enable)
       |
       +--> 2. pci_request_regions(pdev, ...) (向内核资源树申请独占占用)
       |
       +--> 3. mmio = pci_iomap(pdev, 0, 0)   (映射 BAR0 获得内核虚拟基址)
       |
       +--> 4. readl(mmio + 0x00)             (读 IDENT，核对是否等于 0x010000ed)
       |
       +--> 5. writel(test_val, mmio + 0x04)  (写 LIVENESS)
       |
       +--> 6. readl(mmio + 0x04)             (回读并验证按位取反正确性)
       |
edu_mmio_remove(pdev)
       |
       +--> 1. pci_iounmap(pdev, mmio)        (释放虚拟内存页表)
       |
       +--> 2. pci_release_regions(pdev)      (归还物理资源独占权)
       |
       +--> 3. pci_disable_device(pdev)       (关闭硬件译码)
```

## 7. State

```
DEVICE_BOUND
     |
     | pci_enable_device()
     v
HARDWARE_ENABLED (Command 寄存器 Memory/IO 置位)
     |
     | pci_request_regions()
     v
REGIONS_CLAIMED (避免其他驱动同时访问该物理区间)
     |
     | pci_iomap()
     v
MMIO_MAPPED (获得合法的 void __iomem * 虚拟指针)
     |
     | readl / writel 交互
     v
OPERATIONAL
```

## 8. Code

### edu_mmio.c

```c
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
```

### Makefile

```makefile
obj-m += edu_mmio.o

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
# test.sh - 验证 MMIO 读写交互与寄存器翻转

echo "=================================================="
echo " Experiment 4.2: MMIO 读写与寄存器协议验证"
echo "=================================================="

insmod /root/extra/edu_mmio.ko
dmesg | grep -A 5 "edu_mmio: read IDENT"
rmmod edu_mmio
poweroff -f
```

## 9. Build

```bash
cd layer4-bar-mmio/exp4.2-mmio-readwrite
make
```

## 10. Run

```bash
mkdir -p build
cp edu_mmio.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

检查输出：
- `read IDENT = 0x010000ed` 说明读操作成功。
- `read back LIVENESS = 0xedcba987 (expected: ~0x12345678 = 0xedcba987)` 说明写操作与读操作均严格符合硬件预期。
- `MMIO readl/writel verification PASSED successfully!` 说明整个 MMIO 通路构建完全正确。

## 12. Expected Result

```
edu_mmio: read IDENT = 0x010000ed (expected: 0x010000ed)
edu_mmio: writing 0x12345678 to LIVENESS register...
edu_mmio: read back LIVENESS = 0xedcba987 (expected: ~0x12345678 = 0xedcba987)
edu_mmio: MMIO readl/writel verification PASSED successfully!
```

## 13. Source Code

- `drivers/pci/pci.c`: `pci_enable_device()`
- `drivers/pci/pci.c`: `pci_request_regions()`
- `lib/iomap.c` & `arch/x86/include/asm/io.h`: `pci_iomap()`, `readl()`, `writel()`。

## 14. Failure Injection

### 若遗漏调用 `pci_enable_device()`
如果注释掉 `pci_enable_device(pdev)`：
- **Symptom**：`readl(IDENT)` 读取到的将不是 `0x010000ed`，而是 `0xFFFFFFFF`（全 1）！
- **Suspected Layer**：Layer 4（Hardware Enablement / MMIO）。
- **Root Cause**：PCI 配置空间 0x04 处的 Command 寄存器的 `PCI_COMMAND_MEMORY` 位未开启。PCI 芯片物理接口处于忽略所有 Memory Read/Write 事务的关闭状态，总线超时返回全 1。

## 15. Mental Model Summary

```
Before: 以为 MMIO 映射之后就能直接像读写结构体指针一样随意赋值
   |
New Concept: 必须严格遵循:
             1. 硬件使能 (pci_enable_device)
             2. 资源独占 (pci_request_regions)
             3. 页表映射 (pci_iomap)
             4. 架构安全访问 (readl/writel)
   |
After: 彻底打通 CPU 掌控外设寄存器状态的物理通道
```
