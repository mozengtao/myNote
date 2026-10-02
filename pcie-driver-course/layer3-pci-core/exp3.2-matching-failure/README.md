# Experiment 3.2 — Failure Injection: Device ID Mismatch

## 1. Why

“为什么我的驱动 insmod 成功了，没有任何报错，但 probe 就是不跑？”
这是每一个初学者都会踩入的深坑。
必须通过一次刻意的故障注入，亲眼见证并牢固建立一个核心常识：**insmod 的成功仅仅代表 module_init() 没报错，根本不代表硬件已经被接管！**

## 2. Current Mental Model

我们知道 `pci_register_driver()` 依据 `id_table` 去匹配系统中现有的 `pci_dev`。

## 3. New Concept

本实验唯一新增的核心概念是：**当 `id_table` 中的 Vendor ID 或 Device ID 与硬件实际值哪怕有 1 比特不一致时，Matching 就会静默跳过，probe() 绝不会被调用，且 insmod 仍旧返回 0（成功）**。

## 4. Architecture

```
+-------------------------------------------------------------+
| 硬件实际: [Vendor: 0x1234, Device: 0x11e8]                  |
+-------------------------------------------------------------+
                               |
                               | (PCI Core matching)
                               v
+-------------------------------------------------------------+
| 驱动声明: id_table = [{ 0x1234, 0x9999 }] (故意写错 Device ID)|
+-------------------------------------------------------------+
                               |
                               | 匹配失败！ (pci_bus_match 返回 0)
                               v
                       [ 什么都不发生 ]
                  (不会调用 probe, 驱动空转挂起)
```

## 5. Data Flow

```
Hardware Device ID = 0x11e8
          |
          | 比对
          v
Driver id_table = 0x9999
          |
          v
Mismatch -> Control Flow 终止，不向 probe() 传递任何对象
```

## 6. Control Flow

```
insmod edu_mismatch.ko
         |
         v
pci_register_driver(&edu_mismatch_driver)
         |
         v
pci_bus_match() 比对: (0x1234 == 0x1234) && (0x11e8 == 0x9999) -> FALSE!
         |
         v
driver_attach() 放弃该设备，继续检查下一个设备（没有了）
         |
         v
module_init() 返回 0
         |
         v
终端提示模块加载成功，但 dmesg 毫无动静！
```

## 7. State

```
DRIVER_REGISTERED (在内核驱动链表中占位)
        |
        | 匹配失败
        v
DEVICE_UNBOUND (驱动没有任何绑定的设备，孤立状态)
        |
        | rmmod
        v
DRIVER_UNREGISTERED (卸载时也不会调用 remove，因为从未 probe 成功)
```

## 8. Code

### edu_mismatch.c

```c
/*
 * edu_mismatch.c - Demonstrates silent matching failure with wrong Device ID
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR_ID 0x1234
#define WRONG_DEVICE_ID 0x9999  /* 故意篡改：硬件实际是 0x11e8 */

static const struct pci_device_id edu_wrong_id_table[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, WRONG_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, edu_wrong_id_table);

static int
edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	/* 预期绝不会执行到这里！ */
	pr_err("edu_mismatch: ERROR! probe() should NEVER be called!\n");
	return 0;
}

static void
edu_remove(struct pci_dev *pdev)
{
	pr_info("edu_mismatch: remove() called\n");
}

static struct pci_driver edu_mismatch_driver = {
	.name     = "edu_mismatch",
	.id_table = edu_wrong_id_table,
	.probe    = edu_probe,
	.remove   = edu_remove,
};

static int __init
edu_mismatch_init(void)
{
	pr_info("edu_mismatch: module loaded, registering driver with wrong ID (0x%04x:0x%04x)...\n",
		EDU_VENDOR_ID, WRONG_DEVICE_ID);
	return pci_register_driver(&edu_mismatch_driver);
}

static void __exit
edu_mismatch_exit(void)
{
	pr_info("edu_mismatch: module exiting...\n");
	pci_unregister_driver(&edu_mismatch_driver);
}

module_init(edu_mismatch_init);
module_exit(edu_mismatch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 3.2: Matching failure injection");
```

### Makefile

```makefile
obj-m += edu_mismatch.o

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
# test.sh - 观察匹配失败时的系统静默状态

echo "=================================================="
echo " Experiment 3.2: ID 不匹配故障注入观察"
echo "=================================================="

echo "[1] 加载包含错误 Device ID 的驱动:"
insmod /root/extra/edu_mismatch.ko
echo "insmod 退出码: $?"  # 必定为 0

echo ""
echo "[2] 检查 dmesg (绝不应出现 probe 信息):"
dmesg | tail -n 5

echo ""
echo "[3] 检查 driver 目录下的绑定的设备:"
ls -l /sys/bus/pci/drivers/edu_mismatch/

echo ""
echo "[4] 卸载模块:"
rmmod edu_mismatch

echo ""
echo "[5] 检查 dmesg (绝不应出现 remove 信息):"
dmesg | tail -n 3

echo ""
echo "=== 观察完成，关机 ==="
poweroff -f
```

## 9. Build

```bash
cd layer3-pci-core/exp3.2-matching-failure
make
```

## 10. Run

```bash
mkdir -p build
cp edu_mismatch.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

- `insmod` 返回成功（退出状态为 0）。
- 内核日志中只有 `module loaded, registering driver...`，没有任何 `probe()` 或 `remove()` 的日志。
- `/sys/bus/pci/drivers/edu_mismatch/` 内部空空如也，没有任何指向 `0000:00:04.0` 的软链接！

## 12. Expected Result

```
edu_mismatch: module loaded, registering driver with wrong ID (0x1234:0x9999)...
ls -l /sys/bus/pci/drivers/edu_mismatch/:
(仅有 bind, uevent, unbind 等默认文件，无具体设备链接)
edu_mismatch: module exiting...
```

## 13. Source Code

- `drivers/pci/pci-driver.c`: `pci_match_device()`
  如果 `id_table` 比对一圈返回 `NULL`，直接静默退出。

## 14. Failure Injection Summary

- **Symptom**：驱动加载了，但没有任何功能起效，毫无崩溃痕迹。
- **Suspected Layer**：Layer 3（PCI Core Driver Matching）。
- **Observation**：查看 `/sys/bus/pci/devices/<BDF>/driver` 是否存在。若不存在，说明未绑定！
- **Root Cause**：`id_table` 中的 Vendor / Device / Subsystem ID 与物理配置空间不匹配。

## 15. Mental Model Summary

```
Before: 以为只要代码编过了、insmod 了，硬件就能动了
   |
New Concept: Driver 和 Device 是两条平行的河流，唯有 id_table 完全吻合，
             PCI Core 才会把它们汇合并调用 probe()
   |
After: 排查指南第 2 条“Device 存在但 probe 不执行”形成肌肉记忆
```
