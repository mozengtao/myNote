# Experiment 6.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动加载 `edu_msi.ko`，完成 `pci_alloc_irq_vectors(PCI_IRQ_MSI)` 申请消息信号中断（MSI）向量、`request_irq` 注册顶半部 ISR、发起带中断标志的 DMA 传输、通过内核 `wait_for_completion_timeout` 让线程进入休眠，并观察 ISR 完成中断应答（ACK）与唤醒主线程的全过程。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_msi.ko                              |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_msi.ko                             |
| - cat /proc/interrupts | grep -E "edu_msi|CPU"              |
| - dmesg | grep -A 7 "edu_msi: MSI vector allocated"         |
| - rmmod edu_msi                                             |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer6-irq-msi/exp6.1-msi-interrupt

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_msi.ko build/

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
# 1. 加载 MSI 中断驱动模块
insmod /root/extra/edu_msi.ko

# 2. 关键检查：在系统中断计数表 (/proc/interrupts) 中查看分配的 MSI 向量
cat /proc/interrupts | grep -E "edu_msi|CPU"

# 3. 检查内核日志：观察 ISR 触发、ACK 应答、主线程被唤醒以及回环校验
dmesg | grep -A 7 "edu_msi: MSI vector allocated"

# 4. 查看完整的 ISR 触发记录
dmesg | grep "edu_msi_isr"

# 5. 手动卸载驱动模块 (内部调用 free_irq 与 pci_free_irq_vectors)
rmmod edu_msi

# 6. 再次查看中断表确认该中断向量已被系统彻底释放
cat /proc/interrupts | grep "edu_msi" || echo "MSI 向量已注销"

# 7. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `cat /proc/interrupts` | `XX:   2   0   PCI-MSI ...-edge   edu_msi` | 成功分配独立 MSI 边缘触发向量，且已产生中断处理计数（2次分别对应两次 DMA） |
| `dmesg \| grep "allocated"` | `edu_msi: MSI vector allocated: irq=...` | `pci_alloc_irq_vectors` 从 APIC 控制器成功分配到全局中断号 |
| `dmesg \| grep "isr"` | `edu_msi_isr() called: irq_status=0x100 (DMA finished)` | 硬件 DMA 结束发送 MSI TLP 触发 CPU 硬中断，顶半部执行硬件 ACK 并调用 `complete()` |
| `dmesg \| grep "PASSED"` | `edu_msi: DMA loopback verification: PASSED!` | 两次中断唤醒完成数据双向搬运，回环校验成功 |

---

## 6. 顶半部 ISR 黄金守则

在编写 PCIe 顶半部 ISR（Interrupt Service Routine）时：
1. **绝对禁止**调用可能引发进程上下文休眠的函数（如 `msleep()`, `mutex_lock()`, `kmalloc(..., GFP_KERNEL)` 等）。
2. **必须**在 ISR 内第一时间读取中断状态寄存器并向 ACK 寄存器写应答清除硬件电平/报文状态，否则硬件可能持续重发中断导致系统硬卡死。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

