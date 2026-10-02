# Layer 5 — DMA（Direct Memory Access）与 Buffer Ownership

## 1. Mental Model

```
        CPU (Driver)
             |
             | 1. dma_alloc_coherent() (分配共享内存，得到 CPU虚址 与 DMA物理/总线地址)
             | 2. CPU 写入待传输数据
             | 3. writel() 将 DMA 地址写入设备寄存器并触发传输
             |
             v [所有权移交: Transfer Ownership to Device]
             |
        PCIe Device (edu DMA Engine)
             |
             | 4. 设备发起 PCIe Bus Master 读/写事务，直接与内存交互
             v
       System RAM (物理内存)
             |
             | 5. 传输完成 (DMA_RUN 标志位清 0)
             v [所有权归还: Transfer Ownership to CPU]
             |
        CPU (Driver)
             |
             | 6. 读取校验数据
```

## 2. Why

为什么需要 DMA？
如果每次要把 1MB 数据从设备读入内存，都需要 CPU 像 Layer 4 那样一遍又一遍地执行 `readl()` 并压入寄存器再写入内存（这种模式叫 PIO / Programmed I/O），不仅会把 CPU 的 100% 算力全部榨干，而且带宽受限于 CPU 指令周期。
**DMA（Direct Memory Access）让外设变身为总线主控（Bus Master），跳过 CPU，直接在设备内部缓冲区与系统主机物理内存之间大吞吐量搬运数据。**
CPU 的职责变成了：**准备内存、告诉设备地址和长度、下达开始指令、等待结束通知。**

## 3. 核心概念与致命误区

### 3.1 三种地址的区别（绝对不能混淆！）
- **CPU Virtual Address (VA)**：CPU 执行指令 `*ptr = data` 时使用的虚拟地址，由 MMU 负责转换。
- **Physical Address (PA)**：物理内存条上的真实物理地址。
- **DMA Address (`dma_addr_t`)**：**设备从 PCI 总线一侧看到的内存物理地址**（如果系统存在 IOMMU，DMA 地址是 IOMMU 映射的 IOVA；如果无 IOMMU，通常等于物理地址）。
- **致命误区**：把 CPU 的虚拟指针 `virt_to_phys(ptr)` 或 `ptr` 本身传给设备寄存器！外设根本不知道 CPU 页表长什么样，一旦传入虚拟地址，设备发起 DMA 读写会引发致命的 IOMMU Fault、PCI SERR 或写入随机物理内存导致系统瞬间崩溃。

### 3.2 谁拥有缓冲区？（Buffer Ownership 状态机）
DMA 编程最核心的纪律是：**在任意时刻，一块 DMA Buffer 只能归属于一方（要么 CPU 独占，要么 Device 独占）**。
```
        IDLE (Buffer 已分配，双方均未读写)
          |
          | CPU 准备数据
          v
    CPU_PREPARED (CPU 写入完毕，准备交出所有权)
          |
          | MMIO 写入设备启动位 (writel DMA_CMD)
          v
    DEVICE_OWNED (CPU 严禁再读写该内存！违者数据竞争爆炸)
          |
          | 设备 DMA 硬件搬运进行中
          v
    DMA_RUNNING
          |
          | 硬件完成传输，清除运行标志 (或发出中断)
          v
     DMA_DONE (设备退出访问)
          |
          | 软件确认完成
          v
     CPU_OWNED (CPU 重新接管内存，可以安全读取校验结果)
```

### 3.3 Coherent DMA vs Streaming DMA
- **Coherent (一致性 DMA)**：通过 `dma_alloc_coherent()` 分配。保证 CPU 与外设始终看到一致的数据，不需要手动执行 Cache 刷写（`flush/invalidate`），非常适合通信控制块、描述符环（Ring Buffer）和中小型收发缓冲区。
- **Streaming (流式 DMA)**：通过 `dma_map_single()` / `dma_unmap_single()` 映射既有的 `kmalloc()` 或 `vmalloc()` 页面。传输前/后必须显式调用 `dma_sync_*` 同步 CPU 高速缓存。本 Layer 专注建立首个一致性 DMA 闭环。

## 4. QEMU edu 的 DMA 寄存器规范

在 BAR0 中：
- **0x80 [RW] DMA_SRC**：DMA 源物理地址（64位或32位）。
- **0x88 [RW] DMA_DST**：DMA 目的物理地址（64位或32位）。
- **0x90 [RW] DMA_CNT**：要传输的字节数（count）。
- **0x98 [RW] DMA_CMD**：DMA 控制命令寄存器：
  - `bit 0 (0x1)`：`EDU_DMA_RUN`，写入 1 立即触发硬件 DMA 搬运；完成时硬件会自动将该位清 0。
  - `bit 1 (0x2)`：`EDU_DMA_DIR`，0 表示从内存读入设备内部缓冲区（From PCI/RAM to Device）；1 表示从设备缓冲区写出到系统内存（From Device to PCI/RAM）。
  - `bit 2 (0x4)`：`EDU_DMA_IRQ`，是否在完成时拉高中断（Layer 6 会用到，本 Layer 设为 0，采用 Polling 轮询）。

注意：edu 设备内部自带一个 4KB 的 SRAM 缓冲区（`0x40000`）。一次完整传输通常是：
1. 先把系统内存数据 DMA 写到设备内部缓冲区；
2. 再把设备内部缓冲区的数据 DMA 读回到系统内存另一片区域，对比两者的一致性！

## Layer 5 实验列表

| Experiment | 目标 | 目录 |
|---|---|---|
| 5.1 | 使用 `dma_alloc_coherent` 分配缓冲区，通过 Polling 轮询完成 edu DMA 环回传输 | [exp5.1-dma-polling/](exp5.1-dma-polling/) |

## Checkpoint

- 为什么不能直接把 CPU 虚拟地址直接写入设备的 DMA 基址寄存器？
- 什么是 DMA Buffer Ownership？如果 CPU 在 DEVICE_OWNED 状态下去读取 Buffer，可能发生什么？
- 为什么在执行 DMA 之前，驱动必须调用 `pci_set_master(pdev)`？
- Polling（轮询）模式判断 DMA 完成的优缺点分别是什么？
