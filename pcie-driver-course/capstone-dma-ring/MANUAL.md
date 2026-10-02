# Capstone Project 手动实践与调试指南 (Manual Guide)

## 1. 项目架构与终极硬件环境

* **核心目标**：从零构建现代高性能 PCIe 设备驱动闭环——包含自定义 QEMU 虚拟硬件（`mypcie`）、Linux 内核 DMA Ring + MSI-X 驱动（`mypcie_driver.ko`）以及用户态测试套件（`test_app`），全面打通 Doorbell 门铃、环形描述符队列与异步通知。
* **硬件依赖**：**必须**使用包含 `mypcie` 设备驱动的专用 QEMU 编译版本，并在启动参数中携带 `-device mypcie`（Vendor: `0x1234`，Device: `0x22e8`）。

```
+---------------------------------------------------------------------------------+
| Userspace Application (/root/extra/test_app)                                    |
| 1. open("/dev/mypcie0", O_RDWR)                                                 |
| 2. ioctl(fd, MYPCIE_IOCTL_SUBMIT_REQ, &req)                                     |
+---------------------------------------------------------------------------------+
                                      |
                                      | ioctl 系统调用
                                      v
+---------------------------------------------------------------------------------+
| Linux Kernel Driver (/root/extra/mypcie_driver.ko)                              |
| - 填充描述符: desc[slot].dma_addr = buf_dma, desc[slot].length = 64             |
| - 敲 Doorbell 门铃: writel(slot + 1, mmio + REG_PRODUCER); writel(1, REG_DOORBELL)|
| - wait_for_completion_interruptible 进程休眠                                    |
+---------------------------------------------------------------------------------+
                                      |
                                      | PCIe MMIO Doorbell Write TLP
                                      v
+---------------------------------------------------------------------------------+
| Virtual PCIe Device (qemu-src/hw/misc/mypcie.c)                                 |
| - 硬件通过 DMA 读取主机描述符                                                   |
| - 执行 DMA 搬运并回写 DONE 标志位                                                |
| - 发射 MSI-X 向量 0 报文唤醒 CPU                                                 |
+---------------------------------------------------------------------------------+
                                      |
                                      | MSI-X Interrupt TLP
                                      v
+---------------------------------------------------------------------------------+
| Driver MSI-X ISR (mypcie_msix_handler)                                          |
| - 读取 IRQ_STATUS 并写入 REG_IRQ_ACK                                            |
| - complete(&req_done) 唤醒用户态进程                                            |
+---------------------------------------------------------------------------------+
```

---

## 2. 宿主机准备与全栈编译构建 (Host)

### 步骤 2.1: 准备与编译自定义 QEMU 二进制
本工程自定义设备源码 `mypcie.c` 与补丁已保存在 `capstone-dma-ring/qemu-device/`。体验 Capstone 项目需按需编译专用 QEMU：

**方式一（一键准备）：**
```bash
# 在工程根目录下执行
./scripts/setup-qemu-mypcie.sh
```

**方式二（手动步骤）：**
```bash
git clone --depth 1 --branch v8.2.2 https://gitlab.com/qemu-project/qemu.git qemu-src
cp capstone-dma-ring/qemu-device/mypcie.c qemu-src/hw/misc/
patch -d qemu-src -p1 < capstone-dma-ring/qemu-device/mypcie.patch

cd qemu-src
mkdir -p build && cd build
../configure --target-list=x86_64-softmmu --disable-docs --disable-gtk --disable-sdl
ninja qemu-system-x86_64
# 编译完成后，产出可执行文件: /home/morrism/repos/pcie-driver-course/qemu-src/build/qemu-system-x86_64
```

### 步骤 2.2: 编译驱动与用户态测试程序
```bash
# 1. 编译内核驱动模块
cd /home/morrism/repos/pcie-driver-course/capstone-dma-ring/driver
make

# 2. 静态编译用户态测试客户端 (静态链接可确保在 busybox rootfs 中直接运行)
cd /home/morrism/repos/pcie-driver-course/capstone-dma-ring/userspace-test
make
```

### 步骤 2.3: 打包 initramfs 镜像
```bash
cd /home/morrism/repos/pcie-driver-course/capstone-dma-ring
rm -rf build && mkdir -p build
cp driver/mypcie_driver.ko build/
cp userspace-test/test_app build/

# 打包生成仅含测试二进制与驱动的 initramfs
../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动自定义 QEMU 虚拟机 (Host)

使用环境变量 `QEMU_BIN` 指向自定义编译产物，并指定 `-device mypcie`：

```bash
cd /home/morrism/repos/pcie-driver-course/capstone-dma-ring

QEMU_BIN="/home/morrism/repos/pcie-driver-course/qemu-src/build/qemu-system-x86_64" \
../common/run-qemu.sh initramfs.cpio.gz -device mypcie
```

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 控制台内依次手动执行以下步骤：

```sh
# 1. 确认自定义硬件在 PCI 总线上已被识别
lspci -d 1234:22e8

# 2. 加载 DMA Ring 驱动模块
insmod /root/extra/mypcie_driver.ko

# 3. 检查内核日志：确认 MSI-X 向量分配与字符设备创建
dmesg | tail -n 12

# 4. 检查动态生成的设备节点
ls -la /dev/mypcie0

# 5. 检查系统 MSI-X 中断向量表登记
cat /proc/interrupts | grep "mypcie"

# 6. 运行用户态测试客户端，发起真实的 RAM <-> Device 双向 DMA 环形传输
/root/extra/test_app

# 7. 查看执行后驱动的中断计数变化
cat /proc/interrupts | grep "mypcie"

# 8. 卸载驱动模块并关机
rmmod mypcie_driver
dmesg | tail -n 5
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `lspci -d 1234:22e8` | `00:04.0 ... Device 1234:22e8 (rev 01)` | 自定义 QEMU PCI 设备成功仿真并枚举上线 |
| `dmesg \| tail` (加载) | `mypcie: allocated 2 MSI-X vectors`<br>`mypcie: DMA Ring allocated (16 slots)`<br>`mypcie: created char device /dev/mypcie0` | 驱动完成 MMIO 映射、一致性环形队列创建、MSI-X 向量挂载和 cdev 注册 |
| `/root/extra/test_app` | `[1] 成功打开设备文件 /dev/mypcie0`<br>`[2] 发起写请求 ... 成功收到 MSI-X 中断唤醒！`<br>`[3] 发起读请求 ... 硬件回填字节 (期望均为 0x5a)`<br>`>>> SUCCESS: 硬件 DMA Ring + MSI-X 完整闭环验证全部通过！ <<<` | 用户态 ioctl -> 驱动填槽敲门铃 -> 硬件自主搬运 -> MSI-X 异步通知 -> ISR 唤醒用户态的全链路成功闭环 |

---

## 6. 技术架构总结

Capstone 项目是整个课程技术栈的最终集大成者：
1. **摆脱轮询与简单 DMA**：采用与现代 NVMe/Virtio 相同的生产者/消费者环形队列模型（Circular Descriptor Ring）；
2. **硬件 Doorbell 机制**：将写寄存器开销从“每次传输一次”降低为批量敲门铃；
3. **MSI-X 向量化**：摆脱传统 Pin 中断与单向量 MSI，使用独立的 MSI-X 表项实现向量级精确隔离；
4. **全套字符设备驱动**：向上为用户空间提供合法的 POSIX 文件接口，向下驾驭复杂的物理硬件总线。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理驱动编译产物、用户态测试可执行文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机 capstone-dma-ring/ 目录下执行：
make -C driver clean
make -C userspace-test clean
rm -rf build initramfs.cpio.gz

# 如需释放宿主机磁盘空间，可删除 QEMU 源码目录（不会影响 capstone 教程与设备源码）：
rm -rf ../qemu-src

```

