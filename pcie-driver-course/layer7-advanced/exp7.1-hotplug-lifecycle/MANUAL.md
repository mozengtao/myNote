# Experiment 7.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动加载 `edu_hotplug.ko`，利用 Linux 内核 sysfs 提供的运行时热插拔仿真接口（`/sys/bus/pci/devices/.../remove` 与 `/sys/bus/pci/rescan`），手动触发真实 PCIe 热拔（Hot-Unplug）与热插（Hot-Plug），验证驱动的 `probe()` 与 `remove()` 回调在突发物理拓扑变更下的健全性。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_hotplug.ko                          |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_hotplug.ko                         |
| - 模拟热拔: echo 1 > .../remove                             |
| - 观察: lspci 消失, remove() 回调触发                       |
| - 模拟热插: echo 1 > /sys/bus/pci/rescan                    |
| - 观察: 重新枚举, probe() 自动重新绑定                      |
| - rmmod edu_hotplug & poweroff -f                           |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer7-advanced/exp7.1-hotplug-lifecycle

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_hotplug.ko build/

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
# 1. 加载支持热插拔感知的驱动
insmod /root/extra/edu_hotplug.ko

# 2. 检查初始状态：确认 probe 已成功绑定静态存在的 0000:00:04.0
dmesg | tail -n 5
lspci -d 1234:11e8

# 3. 手动模拟 PCIe 物理热拔 (Hot-Unplug)
echo 1 > /sys/bus/pci/devices/0000:00:04.0/remove

# 4. 观察热拔效果：内核注销设备，驱动 remove() 被触发，总线上该设备彻底消失
dmesg | tail -n 4
lspci -d 1234:11e8 || echo "设备已成功从 PCIe 总线拓扑移除"

# 5. 手动模拟 PCIe 物理热插 (Hot-Plug / 重新扫描总线)
echo 1 > /sys/bus/pci/rescan

# 6. 观察热插效果：内核总线重新发现硬件，驱动 probe() 再次自动绑定上线
dmesg | tail -n 6
lspci -d 1234:11e8

# 7. 手动卸载驱动并关机
rmmod edu_hotplug
dmesg | tail -n 3
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `insmod` 初始加载 | `edu_hotplug: >>> HOTPLUG PROBE for 0000:00:04.0 <<<`<br>`edu_hotplug: IDENT register verified = 0x010000ed` | 初始设备被成功识别，MMIO 映射与寄存器自检正常 |
| `echo 1 > .../remove` | `edu_hotplug: >>> HOTPLUG REMOVE for 0000:00:04.0 <<<` | 内核收到物理卸载事件，安全回收虚拟内存映射与 I/O 资源 |
| `lspci -d 1234:11e8` (热拔后) | 输出完全为空 | `pci_stop_and_remove_bus_device` 将设备节点从系统总线树剥离 |
| `echo 1 > /sys/bus/pci/rescan` | `pci 0000:00:04.0: [1234:11e8] ...`<br>`edu_hotplug: >>> HOTPLUG PROBE for 0000:00:04.0 <<<` | 总线重新发起配置空间探测，生成全新 `struct pci_dev` 并完成驱动再绑定 |

---

## 6. 生产级启示

合格的 PCIe 驱动必须做到：`probe()` 与 `remove()` 可以被系统无限次交叉交替调用，且每一次注销都绝不能在系统中留下孤儿内存、未释放的 IRQ 向量或未反解的 MMIO 虚拟页表。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

