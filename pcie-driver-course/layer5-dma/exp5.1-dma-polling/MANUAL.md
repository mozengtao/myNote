# Experiment 5.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动加载 `edu_dma.ko`，完成 `pci_set_master` 授权总线主控权、`dma_set_mask_and_coherent` 配置 28 位寻址掩码、`dma_alloc_coherent` 分配一致性物理内存、配置硬件 DMA 搬运并采用轮询（Polling）机制监测完成，最终验证数据回环完整性。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_dma.ko                              |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_dma.ko                             |
| - dmesg | grep -A 8 "edu_dma: Coherent DMA buffer"          |
| - rmmod edu_dma                                             |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer5-dma/exp5.1-dma-polling

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_dma.ko build/

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
# 1. 加载 DMA 轮询传输驱动
insmod /root/extra/edu_dma.ko

# 2. 检查内核日志：观察一致性 DMA 地址分配与双向传输回环校验
dmesg | grep -A 8 "edu_dma: Coherent DMA buffer"

# 3. 详细查看完整的 probe 初始化日志
dmesg | tail -n 15

# 4. 手动卸载驱动模块 (内部自动调用 dma_free_coherent 释放 4KB 缓冲区)
rmmod edu_dma

# 5. 检查注销日志确认无资源挂起
dmesg | tail -n 4

# 6. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `dmesg \| grep "Coherent DMA"` | `edu_dma: Coherent DMA buffer allocated: CPU vaddr=..., DMA bus_addr=0x...` | 内核分配连续物理页，屏蔽 CPU Cache 脏读影响 |
| `dmesg \| grep "Step 1"` | `edu_dma: DMA Transfer 1 (RAM -> Device SRAM) finished` | 硬件直接读取主存 TX 区并写入片上 0x40000 偏移 |
| `dmesg \| grep "Step 2"` | `edu_dma: DMA Transfer 2 (Device SRAM -> RAM) finished` | 硬件从片上 SRAM 读取并写回主存 RX 接收区 |
| `dmesg \| grep "PASSED"` | `edu_dma: DMA loopback verification: PASSED!` | 接收区数据与发送区完全一致，端到端 DMA 闭环成功 |

---

## 6. 关键知识点提醒

1. **为什么必须调用 `pci_set_master()`？**
   PCIe 设备发起 DMA 本质上是硬件作为 Requester 发射 Memory Write/Read TLP。若未使能配置空间 Command 寄存器的 Bit 2（Bus Master Enable），硬件发射的所有 DMA 事务都将被 Root Complex 直接拦截并报 Unsupported Request 错误！
2. **edu 设备的 28 位 DMA 限制**：
   QEMU `edu` 设备内部寄存器仅支持 28 位 DMA 寻址（最大 256MB）。因此驱动必须通过 `dma_set_mask_and_coherent(dev, DMA_BIT_MASK(28))` 通知内核，确保分配的物理内存落在低 256MB 空间。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

