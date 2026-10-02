# Layer 6 — 中断系统：Legacy INTx / MSI / MSI-X

## 1. Mental Model

```
       [Polling 模式 (Layer 5)]                     [Interrupt-driven 模式 (Layer 6)]
CPU: 检查 -> 检查 -> 检查 (占用 100% 算力)        CPU: 睡眠挂起，转去干其他活
                ^                                                ^
                |                                                |
            硬件仍在搬运                                   设备 DMA 传输完成
                                                                 |
                                                          向总线发送 MSI 报文
                                                          (物理上是一次带特定 Data 的 Memory Write TLP)
                                                                 |
                                                                 v
                                                          APIC 中断控制器捕获
                                                                 |
                                                                 v
                                                          触发 CPU 硬中断 (ISR)
                                                                 |
                                                          唤醒驱动线程/清理完成队列
```

## 2. Why

在 Layer 5 中，驱动采用 `while (readl(REG_DMA_CMD) & 0x1)` 死循环轮询。如果一次 DMA 搬运需要 5 毫秒，CPU 将在这 5 毫秒内空转上千万次指令，无法调度其他进程。
**中断机制彻底倒转了主被动关系：CPU 下发任务后立即休眠，硬件完成传输后主动“拍醒”CPU。**

## 3. 中断技术演进：为什么现代 PCIe 首选 MSI / MSI-X？

### 3.1 传统边沿/电平引脚中断（Legacy INTx: INTA#, INTB#, INTC#, INTD#）
- **物理机制**：主板上有 4 根真实物理铜线连接到中断控制器。
- **致命缺陷**：
  1. **引脚共享（Interrupt Sharing）**：主板插槽多但引脚只有 4 根，多个 PCI 设备必须共用同一根中断线。当引脚被拉低时，内核不知道是哪个设备拉的，必须把挂在这根线上的所有驱动 ISR 挨个叫醒问一遍（`IRQ_NONE` vs `IRQ_HANDLED`），开销巨大；
  2. **乱序风险**：INTx 引脚信号走独立电线，而 DMA 数据走 PCI 总线。中断信号可能比正在总线 FIFO 里排队的 DMA 数据先到达 CPU，导致 ISR 跑起来时内存里的数据其实还没刷完（Data Hazard）。

### 3.2 MSI（Message Signaled Interrupts）
- **物理机制**：彻底废弃物理中断引脚！**所谓 MSI，本质上就是设备向系统 Host 写入一个预先约定好的 32 位特殊地址（MSI Address）的 Memory Write TLP 包**。
- **巨大优势**：
  1. 不存在物理引脚共享，每个设备独享中断向量；
  2. **保证强保序（Ordering Guarantee）**：因为 MSI 中断报文本身就是一条普通的 PCI Memory Write TLP，它与前面的 DMA 写数据包走同一条物理 PCIe Link，严格遵守 PCI 事务排序规则，中断绝不可能“超车”跑到 DMA 数据前面！

### 3.3 MSI-X
- **演进点**：MSI 在配置空间最多只支持 32 个中断向量，且要求分配给设备的向量号在数字上必须连续；
- **MSI-X 的突破**：引入独立的 **MSI-X Table**（通常放在 BAR0 或 BAR1 中），支持多达 2048 个中断向量，每个向量可以独立配置目标 CPU Core（实现完美的多队列 RSS / SMP 亲和性负载均衡），且不需要连续向量号。现代高性能网卡、NVMe SSD 标配 MSI-X。

## 4. 现代 Linux 中断 API：`pci_alloc_irq_vectors`

在 Linux 4.8 之后，内核统一了所有中断申请接口，废弃了老旧零散的 `pci_enable_msi()` / `pci_enable_msix()`，提供高度整合的：
```c
int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
                          unsigned int max_vecs, unsigned int flags);
```
- `flags` 传入 `PCI_IRQ_MSIX | PCI_IRQ_MSI | PCI_IRQ_LEGACY`：内核会按最高性能顺序依次尝试（先试 MSI-X，不行试 MSI，再不行降级到 INTx），一站式搞定！
- 获得分配后，通过 `pci_irq_vector(pdev, 0)` 获取分配到的真实 Linux 全局 IRQ 号。
- 随后使用标准的 `request_irq()` 注册中断服务例程（ISR）。

## 5. QEMU edu 设备的 MSI 中断特性

从 `edu.c` 源码可知：
- edu 设备支持 1 个 MSI 向量（`msi_init(pdev, 0, 1, true, false, errp)`）。
- 在 DMA 命令中设置 `EDU_DMA_IRQ (0x4)`：传输完成时，硬件会自动调用 `edu_raise_irq(edu, DMA_IRQ)` 并向 CPU 发射 MSI 报文！
- 中断状态寄存器 `0x24 (REG_IRQ_STATUS)` 对应 bit 8 (`0x100`) 会被置位。
- 驱动 ISR 在处理完毕后，必须向 `0x64 (REG_IRQ_ACK)` 写入 `0x100` 以确认并清除中断（Acknowledge IRQ），否则中断无法撤回。

## Layer 6 实验列表

| Experiment | 目标 | 目录 |
|---|---|---|
| 6.1 | 使用现代 API 启用 MSI 中断，将 Layer 5 的 Polling 改造为 Interrupt-Driven DMA | [exp6.1-msi-interrupt/](exp6.1-msi-interrupt/) |

## Checkpoint

- Legacy INTx 中断为什么会发生“中断共享风暴”？
- MSI 中断在物理电信号层面到底是什么？为什么它能天然杜绝中断比 DMA 数据先到的问题？
- 现代内核推荐的 `pci_alloc_irq_vectors()` 相比老旧的 `pci_enable_msi()` 有何优势？
- 为什么在 ISR 中必须及时向设备硬件执行 ACK（应答）操作？
