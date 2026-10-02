# Experiment 3.2 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：通过在驱动 ID 白名单中故意声明错误的 Device ID（`0x9999` 代替 `0x11e8`），亲身体验“`insmod` 成功执行但 `probe()` 静默失效”的经典故障模式，掌握通过 sysfs 排查匹配失效的实操手段。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_mismatch.ko                         |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_mismatch.ko                        |
| - echo $? (确认返回值成功但 probe 未触发)                   |
| - ls /sys/bus/pci/drivers/edu_mismatch/ (确认无绑定设备)    |
| - rmmod edu_mismatch                                        |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer3-pci-core/exp3.2-matching-failure

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_mismatch.ko build/

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

在 QEMU 控制台内依次手动执行以下故障对比步骤：

```sh
# 1. 加载包含错误 ID 声明的驱动
insmod /root/extra/edu_mismatch.ko
echo "insmod 退出码: $?"                                            # 关键：返回值必定为 0（成功）

# 2. 检查内核日志：只有 init 日志，绝无 probe 信息！
dmesg | tail -n 5

# 3. 查看驱动目录：目录下没有任何绑定的设备符号链接（孤立驱动）
ls -l /sys/bus/pci/drivers/edu_mismatch/

# 4. 反向检查真实物理设备目录：依然没有 driver 软链接
ls -l /sys/bus/pci/devices/0000:00:04.0/driver 2>&1

# 5. 卸载驱动模块
rmmod edu_mismatch

# 6. 检查内核日志：只有注销日志，绝无 remove 信息！
dmesg | tail -n 3

# 7. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `insmod` | 返回码 `0`，终端无任何异常提示 | `module_init` 返回 0，内核加载器认为模块正常 |
| `dmesg \| tail` (加载后) | `edu_mismatch: module_init() called...`<br>**无任何** `probe() called` 输出 | `pci_bus_match()` 发现 `0x11e8 != 0x9999`，静默放弃设备 |
| `ls .../drivers/edu_mismatch/` | 仅有 `bind`, `unbind`, `uevent` 等控制接口，**无任何 BDF 链接** | 驱动在总线上未匹配到任何设备，处于空转挂起状态 |
| `dmesg \| tail` (卸载后) | 仅显示注销完成，**无任何** `remove()` 日志 | 设备从未进入绑定状态，无需回调 `remove` |

---

## 6. 排错铁律 (Mental Rule)

当开发实际驱动遇到 `insmod` 成功但未见任何硬件交互时，第一步排查动作：
```sh
ls -l /sys/bus/pci/drivers/<driver_name>/
```
若目录下没有任何 `0000:xx:xx.x` 符号链接，100% 为 `pci_device_id` 匹配条件未命中（检查 Vendor ID、Device ID、Subsystem ID 或 Class Code）。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

