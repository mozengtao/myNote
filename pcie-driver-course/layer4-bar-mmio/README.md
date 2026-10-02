# Layer 4 — BAR 与 MMIO（内存映射 I/O）

## 1. Mental Model

```
PCI Configuration Space
          |
          | BAR0 (硬件基地址寄存器声明: 我需要 1MB)
          v
Physical Memory Window (物理地址，例如 0xfeb90000)
          |
          | pci_iomap() / pcim_iomap() / ioremap() (建立内核页表映射)
          v
CPU Kernel Virtual Address (内核虚拟地址，例如 0xffffc90000100000)
          |
          | readl() / writel() (使用专门指令与内存屏障)
          v
Device Registers (硬件寄存器被读写)
```

## 2. Why

CPU 怎么控制外部设备？
在现代计算机架构中，CPU 无法直接通过普通的内存总线“摸到”外设的物理硅片。
设备通过 **BAR（Base Address Register）**，让 Host Bridge 将一段物理地址“截胡”重定向到 PCI 总线上。
但是，操作系统内核运行在虚拟内存开启的保护模式下，CPU 只能通过虚拟地址寻址！
因此，驱动程序必须：
1. **宣告占有权**（`pci_request_regions`），防止其他驱动误用这块物理地址；
2. **建立页表映射**（`pci_iomap`），将这段物理 MMIO 地址映射到内核虚拟地址空间；
3. **安全读写**（`readl`/`writel`），必须遵循严格的 Memory Ordering 与 MMIO 语义，绝不能当成普通内存来读写。

## 3. 核心概念与致命误区

### 3.1 为什么不能用 `*(volatile u32 *)addr`？
许多嵌入式或应用层开发者习惯用强转指针来读写寄存器。在 Linux 内核驱动中，这是**严重违反架构抽象与稳定性的低级错误**：
1. **编译器重排（Compiler Reordering）**：即使有 `volatile`，编译器也只保证单个变量访问不被优化，但不保证与其他变量之间的指令顺序。
2. **CPU 乱序执行与写合并（Store Buffering / Write Combining）**：现代 CPU 拥有乱序执行引擎和写入缓冲，两个连续的写寄存器操作可能会被 CPU 颠倒或合并为一个写入！对于寄存器来说，这直接导致硬件状态机爆炸。
3. **架构差异与总线屏障**：在 ARM / RISC-V / PowerPC 等弱内存模型（Weakly-Ordered Memory）架构下，访问设备必须插入专属的总线级内存屏障（如 `dmb ld`/`mb`）。`readl()` 与 `writel()` 在底层自动内联了对应架构的强内存屏障（Memory Barrier），并保证 PCI 写入已经真正刷新到外部总线（PCI Posting Write Flush）。

### 3.2 资源管理现代化：`pcim_` (Managed PCI)
传统驱动需要手动配对：
```c
pci_request_regions()  <---> pci_release_regions()
pci_iomap()            <---> pci_iounmap()
```
一旦在 `probe()` 出错分支或 `remove()` 中漏掉一个，就会发生物理资源泄漏或页表挂起。
现代 Linux 引入了 `pcim_`（PCI Managed Resource）系列函数（由 Device Model 的 `devres` 机制托管）。只要使用 `pcim_enable_device()` 和 `pcim_iomap()`，驱动在卸载或出错退出时，内核会自动逆序释放所有映射和资源占用！

## 4. QEMU edu 设备的寄存器映射（BAR0）

根据 `edu.c` 源码，edu 设备在 BAR0 暴露了如下经典寄存器：
- **0x00 [RO] IDENT**：识别码，读取始终返回 `0x010000ed`。
- **0x04 [RW] LIVENESS**：心跳测试寄存器。写入一个 32 位值，硬件会按位取反（`~val`）存入；再次读取可以验证 MMIO 双向通路是否完好！
- **0x08 [RW] FACTORIAL**：阶乘计算寄存器。写入数值后硬件计算阶乘。
- **0x20 [RW] STATUS**：状态寄存器（bit 0 表示是否正在计算）。

## Layer 4 实验列表

| Experiment | 目标 | 目录 |
|---|---|---|
| 4.1 | 观察 BAR 资源与地址分配（`lspci -vv` 与内核资源结构体对应） | [exp4.1-bar-resources/](exp4.1-bar-resources/) |
| 4.2 | 编写驱动实现 BAR0 映射，通过 `readl`/`writel` 读写 IDENT 与 LIVENESS 寄存器 | [exp4.2-mmio-readwrite/](exp4.2-mmio-readwrite/) |

## Checkpoint

- 物理地址与虚拟地址在 MMIO 访问中分别扮演什么角色？
- 为什么在 Linux 驱动中绝对禁止使用 `*(u32 *)ptr` 直接解引用访问 MMIO？
- `readl()` 和 `writel()` 相比普通的内存读写，内核在底层额外做了什么保障？
- 什么是 PCI Posting Write？`writel` 之后为什么有时候需要紧跟一个 `readl`？
