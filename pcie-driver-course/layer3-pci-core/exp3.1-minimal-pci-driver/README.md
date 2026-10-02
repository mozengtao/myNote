# Experiment 3.1 — Minimal PCI Driver matching QEMU edu

## 1. Why

迈入真实 PCI 驱动世界的第一步：编写一个能被 Linux PCI 核心识别并成功执行 `probe()` 的最小内核模块。
在此实验中，我们暂时不碰任何寄存器读写（MMIO）或 DMA，只专注验证 **Driver Registration -> Driver Matching -> Probe Execution** 这一核心控制流。

## 2. Current Mental Model

我们知道 QEMU 提供了 Vendor ID 为 `0x1234`、Device ID 为 `0x11e8` 的 edu 设备，且内核已经枚举并创建了 `struct pci_dev`。
现在我们要写出对应的 `struct pci_driver` 与它完成对接。

## 3. New Concept

本实验唯一新增的核心概念是：**使用 `struct pci_device_id` 建立硬件特征白名单，并通过 `pci_register_driver()` 触发 PCI Core 的设备绑定机制**。

## 4. Architecture

```
+-----------------------------------------------------------+
|                     Linux PCI Bus                         |
|                                                           |
|  +------------------------+     +----------------------+  |
|  | struct pci_dev         |     | struct pci_driver    |  |
|  | - vendor: 0x1234       |     | - name: "edu_minimal"|  |
|  | - device: 0x11e8       |     | - id_table: [        |  |
|  |                        |     |     {0x1234, 0x11e8} |  |
|  |                        |     |   ]                  |  |
|  +------------------------+     +----------------------+  |
|               |                             |             |
|               +----------- MATCH -----------+             |
|                                |                          |
|                                v                          |
|                        edu_probe(dev, id)                 |
+-----------------------------------------------------------+
```

## 5. Data Flow

```
Hardware ID in Config Space (0x1234:0x11e8)
                    |
                    v
struct pci_dev (dev->vendor, dev->device)
                    |
                    | 与 driver->id_table 比对
                    v
匹配成功，将 pci_dev 指针作为第一参数，匹配到的 pci_device_id 指针作为第二参数
                    |
                    v
进入 edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
```

## 6. Control Flow

```
insmod edu_minimal.ko
          |
          v
edu_init() -> pci_register_driver(&edu_driver)
          |
          v
drivers/pci/pci-driver.c: __pci_register_driver()
          |
          v
drivers/base/bus.c: bus_add_driver()
          |
          v
drivers/base/dd.c: driver_attach() -> bus_for_each_dev()
          |
          v
drivers/pci/pci-driver.c: pci_bus_match()
          |
          | (比对 vendor == 0x1234 && device == 0x11e8 -> 返回 1)
          v
drivers/pci/pci-driver.c: pci_device_probe()
          |
          v
edu_probe() (我们写的 probe 函数被执行！)
```

## 7. State

```
UNREGISTERED
     |
     | pci_register_driver()
     v
DRIVER_REGISTERED
     |
     | matching 成功 -> edu_probe() 返回 0
     v
DEVICE_BOUND (sysfs 下出现关联链接)
     |
     | pci_unregister_driver()
     v
edu_remove() -> DRIVER_UNBOUND -> UNREGISTERED
```

## 8. Code

### edu_minimal.c

```c
/*
 * edu_minimal.c - Minimal PCI driver matching QEMU edu device (1234:11e8)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

/*
 * 设备 ID 表：内核用来匹配硬件
 * 必须以全 0 的条目结尾，否则会导致 PCI Core 遍历数组越界溢出！
 */
static const struct pci_device_id edu_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ 0, } /* 终止符 */
};
MODULE_DEVICE_TABLE(pci, edu_id_table);

/*
 * edu_probe - 设备与驱动匹配成功时由 PCI 核心调用
 */
static int
edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	pr_info("edu_minimal: probe() triggered for device %04x:%04x at %s\n",
		pdev->vendor, pdev->device, pci_name(pdev));
	return 0;
}

/*
 * edu_remove - 驱动注销或设备移除时调用
 */
static void
edu_remove(struct pci_dev *pdev)
{
	pr_info("edu_minimal: remove() triggered for device %s\n", pci_name(pdev));
}

static struct pci_driver edu_driver = {
	.name     = "edu_minimal",
	.id_table = edu_id_table,
	.probe    = edu_probe,
	.remove   = edu_remove,
};

/*
 * edu_init - 模块加载入口
 */
static int __init
edu_init(void)
{
	pr_info("edu_minimal: registering pci driver...\n");
	return pci_register_driver(&edu_driver);
}

/*
 * edu_exit - 模块卸载出口
 */
static void __exit
edu_exit(void)
{
	pr_info("edu_minimal: unregistering pci driver...\n");
	pci_unregister_driver(&edu_driver);
}

module_init(edu_init);
module_exit(edu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 3.1: Minimal PCI Driver matching QEMU edu");
```

### Makefile

```makefile
obj-m += edu_minimal.o

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
# test.sh - 验证 edu_minimal 驱动的加载、匹配、probe 与卸载

echo "=================================================="
echo " Experiment 3.1: 最小 PCI 驱动加载与匹配验证"
echo "=================================================="

echo "[1] 加载驱动模块:"
insmod /root/extra/edu_minimal.ko

echo ""
echo "[2] 检查 dmesg 是否输出 probe 成功信息:"
dmesg | tail -n 5

echo ""
echo "[3] 验证 sysfs 中设备与驱动的绑定关系:"
ls -l /sys/bus/pci/drivers/edu_minimal/

echo ""
echo "[4] 卸载驱动模块:"
rmmod edu_minimal

echo ""
echo "[5] 检查 dmesg 是否输出 remove 成功信息:"
dmesg | tail -n 3

echo ""
echo "=== 实验完成，关机 ==="
poweroff -f
```

## 9. Build

```bash
cd layer3-pci-core/exp3.1-minimal-pci-driver
make
```

## 10. Run

```bash
mkdir -p build
cp edu_minimal.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

- `dmesg` 中是否清晰打印出：
  `edu_minimal: probe() triggered for device 1234:11e8 at 0000:00:04.0`
- 检查 `/sys/bus/pci/drivers/edu_minimal/` 目录下是否存在指向 `0000:00:04.0` 的软链接。
- `rmmod` 后是否打印 `edu_minimal: remove() triggered for device 0000:00:04.0`。

## 12. Expected Result

```
edu_minimal: registering pci driver...
edu_minimal: probe() triggered for device 1234:11e8 at 0000:00:04.0
edu_minimal: unregistering pci driver...
edu_minimal: remove() triggered for device 0000:00:04.0
```

## 13. Source Code

- `drivers/pci/pci-driver.c`
  - `pci_register_driver()`
  - `pci_bus_match()`: 核心匹配函数，比对 `pci_match_id(pci_dev, pci_driver->id_table)`。
  - `pci_device_probe()`: 在匹配成功后，封装调用具体驱动的 `probe()`。

## 14. Failure Injection

### 忘记添加终止符 `{ 0, }`
如果在 `edu_id_table` 末尾删掉 `{ 0, }`，内核在 `pci_match_id()` 中进行 `while (id->vendor || id->subvendor ...)` 遍历时，将无法识别数组结束边界，继续越界扫描内存，极大概率导致内核 **Kernel Oops / General Protection Fault**。

## 15. Mental Model Summary

```
Before: 以为写驱动就是写一堆操作寄存器的代码
   |
New Concept: 驱动首先是向总线管理者声明"我能服务哪类硬件(id_table)"，
             总线撮合成功后才会把硬件对象(pci_dev)交给 probe()
   |
After: 确立了真实 PCI 驱动的标准骨架结构与生命周期起点
```
