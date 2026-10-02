# Experiment 4.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动编译加载 `edu_bar_info.ko`，在 `probe()` 回调中运用内核标准 API（`pci_resource_start`, `pci_resource_len`, `pci_resource_flags`）提取并解析物理 BAR0 参数，建立物理空间与内核资源结构体的对应。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_bar_info.ko                         |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_bar_info.ko                        |
| - dmesg | grep "edu_bar_info: BAR"                          |
| - rmmod edu_bar_info                                        |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer4-bar-mmio/exp4.1-bar-resources

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_bar_info.ko build/

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
# 1. 加载驱动模块
insmod /root/extra/edu_bar_info.ko

# 2. 检查内核日志：读取驱动输出的 BAR 物理参数
dmesg | grep "edu_bar_info: BAR"

# 3. 对照 sysfs 导出的 resource 文件首行做数据交叉验证
cat /sys/bus/pci/devices/0000:00:04.0/resource | head -n 1

# 4. 手动卸载驱动模块
rmmod edu_bar_info

# 5. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `dmesg \| grep "edu_bar_info: BAR"` | `edu_bar_info: BAR 0: start=0xFEB00000, size=1048576 (1024 KB), flags=0x... (MEM)` | 成功解析出 BAR0 占用 1MB 物理 MMIO 范围，属性包含 `IORESOURCE_MEM` |
| 对比 `sysfs resource` 首行 | 前两个十六进制数精确匹配 `dmesg` 中的 start 和 end 地址 | 内核 API 直接读取并返回 `pdev->resource[0]` 的底层字段 |
| `rmmod edu_bar_info` | `edu_bar_info: remove() called` | 模块清理无悬挂内存泄漏 |

---

## 6. 进阶排查

若 `flags` 显示 `IORESOURCE_IO`，说明设备使用的是传统 x86 I/O 端口（Port I/O，经由 `in`/`out` 汇编指令访问）；而此处 `IORESOURCE_MEM` 标志明确指示该 BAR 为 MMIO，必须通过虚拟内存映射才能被驱动读写。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

