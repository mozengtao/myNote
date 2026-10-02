# Capstone Project: Custom QEMU PCIe Device (`mypcie`)

## 1. 为什么需要自定义 QEMU PCIe 设备？

前面的 Layer 0 ~ 8 使用了 QEMU 自带的 `edu` 设备。`edu` 非常适合学习基础的 BAR、单个 DMA 传输与简单 MSI。
但是，现代真实的高性能硬件（例如 NVMe SSD 控制器、Virtio-net、Intel e1000e/ixgbe 等网卡）**绝不会为每次传输都单独去敲一次 MMIO 启动寄存器**！
现代硬件全部使用 **DMA 环形缓冲区（DMA Ring Buffer / Circular Queue）**：
- 驱动只管在内存的环形队列中不断追加填充描述符（Descriptors）；
- 驱动只需单次写入一次门铃寄存器（Doorbell）；
- 硬件控制器自主通过 DMA 抓取描述符，并行搬运数据，并通过 **MSI-X** 高级向量化中断向 CPU 发起批量完成通知。

为了完成这一终极跨越，我们编写了专属的虚拟设备：**`mypcie`**。

## 2. 硬件技术规格

| 属性 | 规格参数 |
|---|---|
| **PCI Vendor ID** | `0x1234` |
| **PCI Device ID** | `0x22e8` |
| **BAR0 空间** | 64 KB 内存映射 I/O (MMIO) |
| **中断支持** | **MSI-X**（分配 2 个独立向量，向量 0 用于 Ring Completion 异步通知） |
| **DMA 传输引擎**| 64 位宽总线寻址，基于环形描述符自动批量拉取并处理 |

## 3. BAR0 寄存器地址映射表

```
+----------------------------------------------------------------------------------+
| 偏移量  | 名称          | 访问 | 位宽 | 描述                                       |
+---------+---------------+------+------+--------------------------------------------+
| 0x00    | REG_MAGIC     | RO   | 32   | 固定返回 0x50434945 ("PCIE" ASCII)         |
| 0x04    | REG_CONTROL   | RW   | 32   | 控制位: bit 0(ENABLE), bit 1(RING_START)   |
| 0x08    | REG_STATUS    | RO   | 32   | 状态位: bit 0(READY), bit 1(ACTIVE), bit 2(BUSY)|
| 0x10    | REG_RING_BASE | RW   | 64   | 描述符环在主机物理内存中的 64位基地址 (Lo+Hi) |
| 0x18    | REG_RING_SIZE | RW   | 32   | 描述符环包含的槽位总数 (如 16)              |
| 0x20    | REG_PRODUCER  | RW   | 32   | 生产者索引 (由驱动每次写入更新)              |
| 0x24    | REG_CONSUMER  | RO   | 32   | 消费者索引 (由硬件完成时自动递增回写)        |
| 0x28    | REG_DOORBELL  | WO   | 32   | 门铃寄存器 (驱动写 1 踢硬件开始处理)         |
| 0x30    | REG_IRQ_STATUS| RW   | 32   | 中断状态 (bit 0 表示 Ring 批量完成)         |
| 0x34    | REG_IRQ_ACK   | WO   | 32   | 中断应答 (写 1 清除中断)                   |
| 0x1000  | MSI-X Table   | --   | --   | MSI-X 向量配置表结构                       |
| 0x2000  | MSI-X PBA     | --   | --   | MSI-X 挂起位掩码阵列 (Pending Bit Array)   |
+----------------------------------------------------------------------------------+
```

## 4. 硬件描述符格式 (`struct my_hw_desc`)

每个描述符严格占用 16 字节：
```c
struct my_hw_desc {
    uint64_t dma_addr;  /* 64位主机数据缓冲区物理地址 */
    uint32_t length;    /* 传输字节长度 */
    uint32_t flags;     /* bit 0: 方向 (0: Device->RAM, 1: RAM->Device), bit 1: DONE 完成位 */
};
```

## 5. 编译 QEMU 与生成自定义二进制

为了保持课程笔记仓库轻量，我们不将庞大的 QEMU 源码常驻在顶层目录中。
自定义硬件设备的源码 `mypcie.c` 与对应的 Meson 注册补丁 `mypcie.patch` 均持久化保存在本目录中（`capstone-dma-ring/qemu-device/`）。

### 方法 A: 使用一键构建脚本（推荐）
在工程根目录下直接运行自动化脚本：
```bash
./scripts/setup-qemu-mypcie.sh
```
该脚本会自动浅克隆 QEMU `v8.2.2`、拷贝 `mypcie.c`、应用补丁并调用 ninja 完成编译。

### 方法 B: 手动按需克隆与编译
```bash
# 1. 浅克隆 QEMU v8.2.2
git clone --depth 1 --branch v8.2.2 https://gitlab.com/qemu-project/qemu.git qemu-src

# 2. 拷贝硬件源码并打补丁
cp capstone-dma-ring/qemu-device/mypcie.c qemu-src/hw/misc/
patch -d qemu-src -p1 < capstone-dma-ring/qemu-device/mypcie.patch

# 3. 配置并编译 x86_64 target
cd qemu-src
mkdir -p build && cd build
../configure --target-list=x86_64-softmmu --disable-docs --disable-gtk --disable-sdl
ninja qemu-system-x86_64
```

产出的 `qemu-src/build/qemu-system-x86_64` 将完整支持 `-device mypcie` 参数！
实验完毕后，可随时 `rm -rf qemu-src` 清理释放磁盘空间。

