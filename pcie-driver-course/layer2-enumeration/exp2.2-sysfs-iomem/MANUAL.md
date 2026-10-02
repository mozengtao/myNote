# Experiment 2.2 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动核验 PCI 子系统枚举完成后在用户可见空间的双重视图：`/sys/bus/pci/devices/` 中的结构化属性文件以及 `/proc/iomem` 全局物理内存映射区间登记。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. build-initramfs.sh -> 制作基础 rootfs                    |
| 2. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - ls -F /sys/bus/pci/devices/0000:00:04.0/                  |
| - cat resource                                              |
| - cat /proc/iomem | grep -i "00:04"                         |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与打包 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer2-enumeration/exp2.2-sysfs-iomem

# 1. 准备空打包目录 (纯手动交互)
rm -rf build && mkdir -p build

# 2. 生成 initramfs
../../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动 QEMU 虚拟机 (Host)

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 控制台内依次手动执行以下命令：

```sh
BDF="0000:00:04.0"
SYS_DEV="/sys/bus/pci/devices/$BDF"

# 1. 查看设备 sysfs 目录下的关键属性文件列表
ls -F "$SYS_DEV"

# 2. 检查基本身份属性
cat "$SYS_DEV/vendor"
cat "$SYS_DEV/device"
cat "$SYS_DEV/class"

# 3. 关键检查：查看设备导出的 resource 列表 (起始物理地址 终止物理地址 Flags)
cat "$SYS_DEV/resource" | head -n 3

# 4. 关键检查：在全局物理内存地址表 (/proc/iomem) 中确认该区间归属
cat /proc/iomem | grep -B 1 -A 2 -i "00:04"

# 5. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `cat resource` 第一行 | `0x00000000feb... 0x00000000feb... 0x0000000000040200` | 对应 BAR0 的起始、结束物理地址与属性标记位 (0x40200 表示 IORESOURCE_MEM) |
| `cat /proc/iomem` | `feb00000-febfffff : PCI Bus 0000:00`<br>`  feb00000-febfffff : 0000:00:04.0` | 内核 `insert_resource` 成功在父总线内存资源池下登记了 edu 设备的独占物理区间 |
| `ls -F $SYS_DEV` | 存在 `config`, `resource`, `resource0`, `rom` | sysfs 自动为 `struct pci_dev` 建立的二进制定制属性接口 |

---

## 6. 进阶探索

对比 `/sys/bus/pci/devices/0000:00:04.0/resource` 中的第一行物理地址范围，与 `/proc/iomem` 中 `0000:00:04.0` 对应的一行，二者的十六进制数值完全一致，证明 sysfs 导出的正是内核向物理总线仲裁出的硬件基地址。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理临时打包目录与内存盘镜像（本实验无需编译内核驱动）：

```bash
# 在宿主机当前实验目录下执行：
rm -rf build initramfs.cpio.gz
```

