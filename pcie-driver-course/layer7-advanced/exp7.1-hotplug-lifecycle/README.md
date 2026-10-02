# Experiment 7.1 — PCIe Hotplug Lifecycle with QEMU Monitor

## 1. Why

许多初级驱动在编写时预设“硬件一旦上电就永远在线”，导致在硬件故障拔出或虚拟机热迁移时发生崩溃。
通过在运行中的 QEMU 系统内动态注入 `device_add` 与 `device_del`，我们可以真实检验驱动在面对突发硬件下线与上线时，其 `probe()` 与 `remove()` 回调是否真正做到了资源无泄漏、状态机严密闭合。

## 2. Current Mental Model

在前面的实验中，PCI 设备都是在 QEMU 启动命令行预先写死的（静态挂载）。

## 3. New Concept

本实验唯一新增的核心概念是：**PCIe 运行时热插拔事件（Runtime Hotplug）——内核在总线中断通知下动态创建或销毁 `struct pci_dev`，并无条件触发驱动的 `probe()` 与 `remove()` 状态解耦**。

## 4. Architecture

```
+--------------------------------------------------------------+
| Host OS (QEMU Monitor UNIX Socket / stdio)                   |
|                                                              |
|   (qemu) device_add edu,id=my_hot_edu,bus=pci.0,addr=0x06    |
|   (qemu) device_del my_hot_edu                               |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
| Guest Linux Kernel (pciehp / acpiphp)                        |
|                                                              |
| 1. 产生总线热插拔中断                                         |
| 2. 热插 -> 扫描槽位 -> 创建 pci_dev -> 调用 driver->probe()   |
| 3. 热拔 -> 释放槽位 -> 调用 driver->remove() -> 销毁 pci_dev  |
+--------------------------------------------------------------+
```

## 5. Control Flow

```
1. 启动 QEMU，初态时不添加 edu 设备
2. 模块提前加载: insmod edu_hotplug.ko
   -> 驱动注册成功，但 bus 上无匹配设备，静静等待
3. QEMU 执行: device_add edu,id=dev1
   -> 内核收到热插事件 -> 枚举生成 pci_dev -> 触发 edu_hotplug_probe()
4. 验证驱动与新插入的硬件通信正常
5. QEMU 执行: device_del dev1
   -> 内核收到热拔事件 -> 触发 edu_hotplug_remove() -> 驱动释放 MMIO/资源
6. 检查内核日志与 sysfs 目录干净清理
```

## 6. Code

### edu_hotplug.c

```c
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
```

### Makefile

```makefile
obj-m += edu_hotplug.o

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
# test.sh - 热插拔自动测试脚本（利用 sysfs 的 rescan / remove 机制模拟热插拔操作）

echo "=================================================="
echo " Experiment 7.1: PCIe 运行时热插拔生命周期验证"
echo "=================================================="

insmod /root/extra/edu_hotplug.ko

echo ""
echo "[1] 初始状态：驱动已就绪并绑定静态设备"
dmesg | grep "edu_hotplug: >>> HOTPLUG"

echo ""
echo "[2] 模拟热拔：向设备的 remove 接口写入 1"
BDF=$(lspci -d 1234:11e8 | awk '{print $1}')
echo 1 > "/sys/bus/pci/devices/0000:${BDF}/remove"
sleep 1
dmesg | tail -n 3

echo ""
echo "[3] 模拟热插：触发 PCI 总线全盘重新扫描 (rescan)"
echo 1 > /sys/bus/pci/rescan
sleep 1
dmesg | tail -n 4

echo ""
echo "[4] 卸载驱动"
rmmod edu_hotplug
poweroff -f
```

## 7. Build

```bash
cd layer7-advanced/exp7.1-hotplug-lifecycle
make
```

## 8. Run

```bash
mkdir -p build
cp edu_hotplug.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 9. Observe & Expected Result

```
edu_hotplug: >>> HOTPLUG EVENT: device ARRIVED at 0000:00:04.0 <<<
edu_hotplug: Verified device hardware IDENT = 0x010000ed
[模拟热拔后]
edu_hotplug: >>> HOTPLUG EVENT: device REMOVED from 0000:00:04.0 <<<
edu_hotplug: All resources released cleanly!
[模拟重新扫描热插后]
edu_hotplug: >>> HOTPLUG EVENT: device ARRIVED at 0000:00:04.0 <<<
edu_hotplug: Verified device hardware IDENT = 0x010000ed
```

## 10. Mental Model Summary

```
Before: 认为硬件与系统的连接是静态不变的
   |
New Concept: 硬件随时可能动态下线或重新接入，
             驱动的 probe() 和 remove() 必须对称、幂等、无内存泄漏
   |
After: 具备了面向真实云服务器热迁移、设备故障自愈的弹性驱动设计思维
```
