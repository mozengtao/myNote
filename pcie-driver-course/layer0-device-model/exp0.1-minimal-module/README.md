# Experiment 0.1 — Minimal Kernel Module

## 1. Why

在学习任何 bus/device/driver 概念之前，必须先确认一件最基础的事：
**我写的代码真的可以被内核加载并执行**。这是后面所有实验的地基——如果连这一步都不稳，
后面调试 PCI driver 时会很难分辨"是我的 PCI 逻辑错了"还是"模块根本没加载成功"。

## 2. Current Mental Model

目前我们什么都不知道，这是课程的起点。

## 3. New Concept

本实验唯一新增的核心概念是：**`module_init()` / `module_exit()` 定义了一个内核模块的生命周期入口和出口**。

## 4. Architecture

```
+-------------------------------------------+
|              Linux Kernel                  |
|                                             |
|   +-------------------------------------+   |
|   |     hello.ko  (our module)          |   |
|   |                                      |   |
|   |   module_init(hello_init)            |   |
|   |   module_exit(hello_exit)            |   |
|   +-------------------------------------+   |
|                                             |
+-------------------------------------------+
```

## 5. Data Flow

本实验没有硬件数据流，只有"代码被加载/卸载"这件事本身。

## 6. Control Flow

```
insmod hello.ko
      |
      v
内核的 module loader 解析 ELF, 解决符号引用
      |
      v
调用 hello_init()     <-- 我们写的函数
      |
      v
hello_init() 返回 0 (成功)
      |
      v
模块处于 "已加载" 状态，代码常驻内存，但不会自己继续跑
      |
      v
rmmod hello
      |
      v
调用 hello_exit()
      |
      v
模块被从内核中卸载
```

## 7. State

```
UNLOADED
   | insmod
   v
LOADING  (module loader 解析 ELF / resolve symbols)
   | module_init() 执行
   v
LOADED   (hello_init() 返回 0，模块常驻)
   | rmmod
   v
UNLOADING (module_exit() 执行: hello_exit())
   |
   v
UNLOADED
```

如果 `module_init()` 返回非 0 值，内核认为模块初始化失败，会直接把模块卸载，
状态直接回到 `UNLOADED`，并**不会**调用 `module_exit()`（因为从未真正进入 LOADED 状态）。

## 8. Code

### hello.c

```c
// hello.c
// 最小内核模块：只做一件事——证明 module_init/module_exit 被正确调用

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

/*
 * hello_init - 模块加载时的入口函数
 *
 * 由 insmod/modprobe 触发。返回值的含义：
 *   0      : 初始化成功，模块进入 LOADED 状态
 *   非 0   : 初始化失败，内核会立刻卸载模块（不会调用 exit 函数）
 */
static int __init hello_init(void)
{
	pr_info("hello: module_init() called, module is now LOADED\n");
	return 0;
}

/*
 * hello_exit - 模块卸载时的出口函数
 *
 * 由 rmmod 触发。只有成功进入过 LOADED 状态的模块才会执行到这里。
 */
static void __exit hello_exit(void)
{
	pr_info("hello: module_exit() called, module is now UNLOADING\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 0.1: minimal kernel module lifecycle");
```

### Makefile

```makefile
# Makefile
# out-of-tree kernel module 标准写法：
#   当 make 不带 -C KERNELDIR 参数直接运行时（第一次调用），
#   会进入下面的 "else" 分支，反过来调用内核构建系统，
#   并通过 M=$(PWD) 告诉内核构建系统："到这个目录里找 obj-m"。
obj-m += hello.o

KERNELDIR ?= ../../kernel
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
```

### test.sh（注入 initramfs 自动运行）

```bash
#!/bin/sh
# test.sh - 在 initramfs 启动后自动执行，验证 module_init/exit 生命周期
echo "=== Experiment 0.1: insmod hello.ko ==="
insmod /root/extra/hello.ko
echo "=== dmesg 应该看到 'module_init() called' ==="
dmesg | tail -5
echo "=== lsmod 应该看到 hello 模块 ==="
lsmod
echo "=== cat /sys/module/hello/... ==="
ls /sys/module/hello/
echo "=== rmmod hello.ko ==="
rmmod hello
echo "=== dmesg 应该看到 'module_exit() called' ==="
dmesg | tail -3
echo "=== Experiment 0.1 done, powering off ==="
poweroff -f
```

## 9. Build

```bash
cd layer0-device-model/exp0.1-minimal-module
make
# 产出 hello.ko
```

## 10. Run

```bash
mkdir -p build
cp hello.ko test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz
```

## 11. Observe

- `dmesg` 中出现 `hello: module_init() called, module is now LOADED`
- `lsmod` 列出 `hello` 模块
- `/sys/module/hello/` 目录存在（这是 Device Model 底层 `kobject` 机制自动生成的 sysfs 节点——
  即使是最简单的模块，一旦加载，内核也会在 sysfs 里给它建一个"档案"）
- `rmmod` 后 `dmesg` 出现 `hello: module_exit() called`，且 `/sys/module/hello/` 消失

## 12. Expected Result

```
=== Experiment 0.1: insmod hello.ko ===
=== dmesg 应该看到 'module_init() called' ===
[    1.234567] hello: module_init() called, module is now LOADED
=== lsmod 应该看到 hello 模块 ===
hello                  16384  0
=== rmmod hello.ko ===
=== dmesg 应该看到 'module_exit() called' ===
[    1.345678] hello: module_exit() called, module is now UNLOADING
```

## 13. Source Code

- `include/linux/module.h` — `module_init()` / `module_exit()` 宏定义
- `kernel/module/main.c` — `do_init_module()`，真正调用我们 `module_init()` 注册的函数的地方
- `/sys/module/` — 这是 `kernel/module/sysfs.c` 自动维护的，和我们写的代码无关，
  是 Device Model 的 kobject 机制自动附赠的

## 14. Failure Injection

把 `hello_init()` 改成：

```c
static int __init hello_init(void)
{
	pr_info("hello: about to fail on purpose\n");
	return -EINVAL;   /* 故意返回失败 */
}
```

**现象**：`insmod` 命令本身会失败并返回非 0 退出码，`dmesg` 只会看到
`about to fail on purpose`，**不会**看到任何 "UNLOADING" 相关的日志，因为模块从未进入
LOADED 状态，`module_exit()` 根本不会被调用。`lsmod` 也不会列出这个模块。

**结论**：`module_init()` 的返回值不是"随便填个 0"——它是模块生命周期状态机的真正触发条件。

## 15. Mental Model Summary

```
Before: 不知道代码怎么进入内核运行
   |
New concept: module_init()/module_exit() 是模块生命周期的两个固定钩子，
             由 insmod/rmmod 分别触发，返回值决定状态机走向
   |
After: 理解了"加载代码"这件事本身的生命周期，
       后面 PCI driver 的 pci_register_driver() 调用，
       都是在这个 module_init() 里面发生的
```
