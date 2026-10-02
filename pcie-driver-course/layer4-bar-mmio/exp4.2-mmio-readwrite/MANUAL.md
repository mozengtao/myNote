# Experiment 4.2 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动加载 `edu_mmio.ko`，完成 `pci_enable_device` -> `pci_request_regions` -> `pci_iomap` 完整的 MMIO 映射全生命周期，通过 `readl` 读取识别寄存器，通过 `writel` 向 LIVENESS 寄存器写入数据并验证硬件反转回读。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_mmio.ko                             |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_mmio.ko                            |
| - dmesg | grep -A 5 "edu_mmio: read IDENT"                  |
| - rmmod edu_mmio                                            |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer4-bar-mmio/exp4.2-mmio-readwrite

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_mmio.ko build/

# 3. 生成 initramfs
../../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动 QEMU 虚拟机 (Host)

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 控制台内依次手动执行以下步骤：

```sh
# 1. 加载 MMIO 驱动模块
insmod /root/extra/edu_mmio.ko

# 2. 检查内核日志：观察寄存器读写交互与协议校验
dmesg | grep -A 6 "edu_mmio: read IDENT"

# 3. 观察 /proc/iomem 中独占标记（pci_request_regions 将该区域标记为驱动名字）
cat /proc/iomem | grep -B 1 -A 2 "edu_mmio"

# 4. 手动卸载驱动模块
rmmod edu_mmio

# 5. 检查内核日志确认 iounmap 与 release_regions 正常回收
dmesg | tail -n 4

# 6. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `dmesg \| grep "IDENT"` | `edu_mmio: read IDENT (offset 0x00) = 0x010000ed (EXPECTED)` | 虚拟硬件固定特征标识码读取无误，TLB/MMU 映射成功 |
| `dmesg \| grep "LIVENESS"` | `edu_mmio: wrote to LIVENESS (0x04) = 0x12345678`<br>`edu_mmio: read back from LIVENESS = 0xedcba987 (BITWISE INVERTED: MATCH!)` | 驱动向硬件寄存器写数据，硬件内部取反后由驱动读回，双向总线交互完全成立 |
| `/proc/iomem` | 显示 `0000:00:04.0` 下级挂载 `edu_mmio` | `pci_request_regions()` 成功独占该物理资源，防范驱动并发抢占冲突 |

---

## 6. 核心规则警示

在内核代码中访问 MMIO 地址时：
* **严禁**直接解引用指针（如 `*(volatile u32 *)addr = val;`），在某些架构会导致乱序、Cache 脏读或总线死锁。
* **必须**统一使用内核内联函数 `readl()` / `writel()`（以及 16 位的 `readw()`、8 位的 `readb()`），它们内置了平台强内存屏障（Memory Barriers）。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

