# Experiment 1.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：在没有加载自定义驱动的前提下，利用宿主机与虚拟机内置的 `lspci` 工具和 sysfs，手动探查物理 PCI 总线拓扑、定位 QEMU `edu` 设备并获取其 BDF（Bus:Device.Function）地址。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`（PCI 教学虚拟设备，Vendor: `0x1234`，Device: `0x11e8`）。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. 无需编译内核模块                                         |
| 2. build-initramfs.sh -> 制作基础 rootfs 内存盘             |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - lspci                                                     |
| - lspci -d 1234:11e8                                        |
| - ls -la /sys/bus/pci/devices/                              |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与打包 (Host)

本实验无需编译内核模块。直接打包基础 initramfs：

```bash
cd /home/morrism/repos/pcie-driver-course/layer1-pci-basics/exp1.1-lspci-basic

# 1. 创建空额外文件目录 (无需放入 .ko 或 test.sh)
rm -rf build && mkdir -p build

# 2. 生成最小 initramfs
../../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动 QEMU 虚拟机 (Host)

启动 QEMU 并附加 `edu` 虚拟硬件：

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 控制台内依次手动执行以下命令：

```sh
# 1. 查看总线枚举出的所有 PCI 物理设备
lspci

# 2. 精确定位厂商为 1234 (QEMU/Bochs), 设备为 11e8 (edu) 的设备
lspci -d 1234:11e8

# 3. 检查 Linux 内核在 sysfs 中为 PCI 总线设备建立的设备目录
ls -l /sys/bus/pci/devices/

# 4. 进入 edu 设备的对应目录 (典型 BDF 为 0000:00:04.0)
cd /sys/bus/pci/devices/0000:00:04.0

# 5. 手工打印该设备导出的 Vendor ID 与 Device ID
cat vendor
cat device

# 6. 观察此时的驱动绑定状态（由于尚未加载驱动，不存在 driver 软链接）
ls -l driver 2>/dev/null || echo "当前设备尚未绑定任何 Driver"

# 7. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `lspci` | 包含 `00:04.0 ... 1234:11e8 (rev 10)` | QEMU 主板模拟在 Slot 4 挂载了 edu 设备 |
| `lspci -d 1234:11e8` | `00:04.0 Unassigned class [ff00]: Device 1234:11e8 (rev 10)` | 成功通过 Vendor/Device ID 过滤器锁定设备 |
| `/sys/bus/pci/devices/` | 存在名为 `0000:00:04.0` 的目录 | 内核 PCI 子系统为该物理 BDF 建立了 `struct pci_dev` |
| `cat vendor` | `0x1234` | 从该设备配置空间偏移 0x00 读取的厂商 ID |
| `cat device` | `0x11e8` | 从该设备配置空间偏移 0x02 读取的设备 ID |

---

## 6. 故障注入尝试 (可选实战)

在宿主机启动时不加 `-device edu`：
```bash
../../common/run-qemu.sh initramfs.cpio.gz
```
在虚拟机内运行 `lspci -d 1234:11e8`，观察：
* 输出完全为空。
* `/sys/bus/pci/devices/` 下没有 `0000:00:04.0`。
这说明 PCI 设备无法凭空产生，枚举的前提是硬件总线上真实探测到 Vendor ID 不全为 `0xFFFF`。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理临时打包目录与内存盘镜像（本实验无需编译内核驱动）：

```bash
# 在宿主机当前实验目录下执行：
rm -rf build initramfs.cpio.gz
```

