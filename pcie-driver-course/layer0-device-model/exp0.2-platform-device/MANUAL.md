# Experiment 0.2 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动加载包含 `platform_device` 与 `platform_driver` 的演示驱动，直观观测 Linux 设备模型在总线上的 matching 机制与 `probe()` 自动触发流程。
* **硬件依赖**：无额外硬件需求，使用基础 QEMU 虚拟机环境即可。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 platform_demo.ko                        |
| 2. build-initramfs.sh -> 制作仅含 platform_demo.ko 的内存盘 |
| 3. run-qemu.sh -> 启动串口交互虚拟机                        |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/platform_demo.ko                       |
| - dmesg / sysfs 总线符号链接检查                            |
| - rmmod platform_demo                                       |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

在宿主机终端进入实验目录：

```bash
cd /home/morrism/repos/pcie-driver-course/layer0-device-model/exp0.2-platform-device

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp platform_demo.ko build/

# 3. 打包生成 initramfs
../../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动 QEMU 虚拟机 (Host)

```bash
../../common/run-qemu.sh initramfs.cpio.gz
```

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 内依次输入以下命令：

```sh
# 1. 加载平台设备驱动模块
insmod /root/extra/platform_demo.ko

# 2. 检查内核日志：观察 device 与 driver 注册顺序及 matching 触发的 probe
dmesg | tail -n 6

# 3. 检查 platform 总线下的设备节点
ls -la /sys/bus/platform/devices/my_pdev/

# 4. 关键验证：检查设备是否建立了指向驱动的 driver 软链接（matching 成功的铁证）
ls -l /sys/bus/platform/devices/my_pdev/driver

# 5. 检查驱动目录中反向绑定的设备链接
ls -l /sys/bus/platform/drivers/my_pdev/

# 6. 手动卸载驱动模块
rmmod platform_demo

# 7. 检查内核日志确认 remove 过程
dmesg | tail -n 4

# 8. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `dmesg \| tail` | `platform_demo: registering platform_device 'my_pdev'`<br>`platform_demo: registering platform_driver 'my_pdev'`<br>`platform_demo: my_probe() called for device 'my_pdev' -> MATCHED!` | 总线在 `platform_driver_register` 时遍历设备，命中同名 `my_pdev`，立刻回调 `probe` |
| `ls -l .../my_pdev/driver` | `driver -> ../../../../bus/platform/drivers/my_pdev` | 总线驱动模型将设备与匹配到的驱动建立双向引用 |
| `rmmod platform_demo` | `platform_demo: my_remove() called for device 'my_pdev'` | 注销驱动前，总线先安全解除绑定并回调驱动的 `remove` 方法 |

---

## 6. 故障注入尝试 (可选实战)

在 `platform_demo.c` 中将驱动名称改错：
```c
static struct platform_driver my_driver = {
    .driver = {
        .name = "my_pdev_typo",
        ...
    },
    ...
};
```
重新编译打包运行后执行 `insmod`，观察现象：
* `insmod` 返回成功（退出码 0）。
* `dmesg` 仅显示两条 registering 日志，**绝对没有** `my_probe() called`。
* `/sys/bus/platform/devices/my_pdev/driver` 报错不存在。
这充分证明：**模块加载成功不代表驱动匹配成功**，matching 是总线核心独立判定的。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

