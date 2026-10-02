# Experiment 3.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动编译、打包并加载最小 PCI 驱动 `edu_minimal.ko`，验证 Linux PCI 核心的 `pci_device_id` 匹配机制、`probe()` 自动调用以及 `remove()` 安全注销链路。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_minimal.ko                          |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_minimal.ko                         |
| - dmesg / sysfs 双向驱动绑定检查                            |
| - rmmod edu_minimal                                         |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer3-pci-core/exp3.1-minimal-pci-driver

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_minimal.ko build/

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
# 1. 加载最小 PCI 驱动
insmod /root/extra/edu_minimal.ko

# 2. 检查内核日志：确认 probe 函数是否被 PCI 核心触发并成功执行
dmesg | tail -n 5

# 3. 检查驱动目录中绑定的 PCI 设备节点（证明 matching 成功）
ls -l /sys/bus/pci/drivers/edu_minimal/

# 4. 反向检查设备目录中的 driver 链接
ls -l /sys/bus/pci/devices/0000:00:04.0/driver

# 5. 手动卸载驱动
rmmod edu_minimal

# 6. 检查内核日志确认 remove 回调执行
dmesg | tail -n 5

# 7. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `dmesg \| tail` (加载后) | `edu_minimal: probe() called for 0000:00:04.0`<br>`edu_minimal: device enabled successfully` | PCI 核心匹配 ID 成功后，按规范回调执行 `edu_probe()` |
| `ls -l .../edu_minimal/` | 包含符号链接 `0000:00:04.0 -> ../../../../devices/...` | 驱动与设备在总线层建立强引用关联 |
| `dmesg \| tail` (卸载后) | `edu_minimal: remove() called for 0000:00:04.0`<br>`edu_minimal: driver unregistered` | 模块卸载触发 `pci_unregister_driver()`，主动剥离绑定的设备 |

---

## 6. 故障排查提示

若 `dmesg` 中未看到 `probe() called`，请执行以下检查：
1. 确认 QEMU 启动命令行是否遗漏了 `-device edu`。
2. 在虚拟机内执行 `lspci -d 1234:11e8`，核验该虚拟设备是否存在。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

