# PCIe 学习路线

## 目录

- [整体脉络](#整体脉络)
- [一、先建立整个 PCIe Driver Mental Model](#一先建立整个-pcie-driver-mental-model)
- [二、建议按照 8 个层次学习](#二建议按照-8-个层次学习)
- [三、Layer 0：先补 Linux Kernel Driver 基础](#三layer-0先补-linux-kernel-driver-基础)
- [四、Layer 1：PCI / PCIe 硬件基础](#四layer-1pci--pcie-硬件基础)
- [五、Layer 2：PCI Enumeration](#五layer-2pci-enumeration)
- [六、必须理解 PCI Config Space](#六必须理解-pci-config-space)
- [七、Layer 3：Linux PCI Core](#七layer-3linux-pci-core)
- [八、一定要彻底理解 PCI Driver 的生命周期](#八一定要彻底理解-pci-driver-的生命周期)
- [九、Layer 4：BAR / MMIO](#九layer-4bar--mmio)
- [十、Layer 5：DMA](#十layer-5dma)
- [十一、DMA 的真正难点：Ownership](#十一dma-的真正难点ownership)
- [十二、Layer 6：IRQ / MSI / MSI-X](#十二layer-6irq--msi--msi-x)
- [十三、把 DMA + IRQ + MMIO 串起来](#十三把-dma--irq--mmio-串起来)
- [十四、Layer 7：PCIe Advanced Features](#十四layer-7pcie-advanced-features)
- [十五、最终还需要理解 PCIe Transaction Layer](#十五最终还需要理解-pcie-transaction-layer)
- [十六、你最终需要掌握的知识地图](#十六你最终需要掌握的知识地图)
- [十七、按照你的背景，我建议这样学](#十七按照你的背景我建议这样学)
- [十八、最适合你的第一个实验](#十八最适合你的第一个实验)
- [十九、你真正应该达到的最终 Mental Model](#十九你真正应该达到的最终-mental-model)

---

## 整体脉络

```text
PCIe Hardware
     │
     ▼
PCIe Bus / Enumeration
     │
     ▼
Linux PCI Core
     │
     ├── struct pci_dev
     ├── struct pci_driver
     ├── BAR / MMIO
     ├── DMA
     ├── IRQ / MSI / MSI-X
     ├── PCI Config Space
     └── Power Management
             │
             ▼
       Your Device Driver
             │
             ├── probe()
             ├── remove()
             ├── interrupt handler
             ├── DMA / buffer management
             └── device-specific protocol
```

你真正需要掌握的是：

> PCIe 硬件如何被发现 → Linux 如何表示它 → driver 如何绑定 → driver 如何访问设备 → 设备如何通过 DMA/IRQ 与 CPU 协作。

---

## 一、先建立整个 PCIe Driver Mental Model

你可以把 PCIe driver 理解成：

```text
                  PCIe Device
                      │
          ┌───────────┴───────────┐
          │                       │
    Configuration Space       BAR / MMIO
          │                       │
          │                       │
          ▼                       ▼
     PCIe Discovery          Device Registers
          │                       │
          └───────────┬───────────┘
                       ▼
                 Linux PCI Core
                       │
                 struct pci_dev
                       │
              driver matching
                       │
                       ▼
              struct pci_driver
                       │
                    probe()
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
        MMIO          DMA          IRQ
          │            │            │
          └────────────┼────────────┘
                       ▼
                 Device Driver
```

从 driver 作者角度，最重要的几个对象是：

| 对象 | 你可以先这样理解 |
|---|---|
| PCIe device | 一块插在 PCIe 总线上的硬件 |
| PCIe bus | CPU/Root Complex 与设备之间的总线拓扑 |
| `struct pci_dev` | Linux 对一个 PCI device 的软件表示 |
| `struct pci_driver` | 你的 driver |
| BAR | 设备暴露给 CPU 的 MMIO/IO 地址窗口 |
| Config Space | PCIe 设备的标准管理区域 |
| MMIO register | 真正控制设备的寄存器 |
| DMA | 设备直接访问内存 |
| MSI/MSI-X | 设备向 CPU 发中断的机制 |
| `probe()` | Linux 把 driver 与设备绑定后调用的初始化入口 |

如果这张图能真正理解，后面的知识就会比较容易组织。

---

## 二、建议按照 8 个层次学习

我建议你的学习路线是：

```text
Layer 0  Linux Device Driver 基础
              │
Layer 1  PCI / PCIe 基础
              │
Layer 2  PCI Enumeration
              │
Layer 3  Linux PCI Core
              │
Layer 4  BAR / MMIO
              │
Layer 5  DMA
              │
Layer 6  IRQ / MSI / MSI-X
              │
Layer 7  PCIe Advanced Features
              │
Layer 8  Real Device Driver Architecture
```

不要一开始就啃 PCIe Base Specification。

---

## 三、Layer 0：先补 Linux Kernel Driver 基础

如果你已经熟悉 Linux kernel C，这一层可以快速过。

需要掌握：

### 1. Kernel Module

理解：

```text
insmod
   │
   ▼
module_init()
   │
   ▼
driver registration
   │
   ▼
kernel
```

以及：

```text
rmmod
   │
   ▼
module_exit()
   │
   ▼
driver unregister
```

### 2. Linux Device Model

这是 PCI driver 非常重要的基础。

至少理解：

```text
struct device
     │
     ├── bus
     ├── driver
     ├── parent
     └── kobject
```

以及：

```text
Bus
 │
 ├── Device
 │
 ├── Driver
 │
 └── Match
```

PCI 只是 Linux device model 中的一种 bus：

```text
Linux Device Model
       │
       ├── PCI bus
       ├── USB bus
       ├── I2C bus
       ├── SPI bus
       └── platform bus
```

所以你之前学的：

> Framework owns the protocol; driver/module owns the policy.

在 PCI driver 中会再次出现。

---

## 四、Layer 1：PCI / PCIe 硬件基础

这里第一次真正进入 PCIe。

你不需要马上学习所有 PCIe protocol details。

先理解：

### 1. PCIe 拓扑

最重要的结构：

```text
                    CPU
                     │
                 Root Complex
                     │
                  Root Port
                     │
             ┌───────┴───────┐
             │               │
          PCIe Device     PCIe Switch
                             │
                       ┌─────┴─────┐
                       │           │
                    Device      Device
```

需要知道：

- Root Complex
- Root Port
- Endpoint
- PCIe Switch
- Bus
- Device
- Function

尤其是 **BDF**：

```text
Bus : Device : Function

03 : 00 : 0
```

Linux 经常用：

```text
0000:03:00.0
```

表示：

```text
Domain   = 0000
Bus      = 03
Device   = 00
Function = 0
```

这个你以后会天天看到。

---

## 五、Layer 2：PCI Enumeration

这是 PCI driver 的第一个核心知识。

系统启动以后，不是你的 driver 主动"寻找"PCIe 设备。

而是：

```text
Firmware / PCI Host
        │
        ▼
PCI Enumeration
        │
        ▼
发现 PCI devices
        │
        ▼
Linux PCI Core
        │
        ▼
创建 struct pci_dev
```

例如：

```bash
lspci
```

看到：

```text
03:00.0 Ethernet controller: ...
```

实际上意味着 Linux 已经发现：

```text
PCI Device
     │
     ▼
struct pci_dev
```

---

## 六、必须理解 PCI Config Space

这是 PCIe driver 最核心的概念之一。

可以先把它理解成：

```text
PCIe Device
┌─────────────────────────┐
│ Configuration Space     │
│                         │
│ Vendor ID               │
│ Device ID               │
│ Command                 │
│ Status                  │
│ Class Code              │
│ BAR0                    │
│ BAR1                    │
│ ...                     │
│ Capability              │
│ MSI/MSI-X               │
│ PCIe Capability         │
└─────────────────────────┘
```

Linux 可以通过：

```bash
lspci -vv
```

看到大量信息。

你需要重点理解：

- Vendor ID
- Device ID
- Class
- BAR
- Command
- Status
- Capabilities
- PCI Express Capability
- MSI / MSI-X

---

## 七、Layer 3：Linux PCI Core

这是你真正开始写 driver 的地方。

最核心的两个结构：

- `struct pci_dev`
- `struct pci_driver`

可以建立这样的 mental model：

```text
                 PCI Core
                    │
          ┌─────────┴─────────┐
          │                   │
     struct pci_dev      struct pci_driver
          │                   │
          │                   │
       Hardware              Driver
          │                   │
          └─────────┬─────────┘
                     │
                   match
                     │
                     ▼
                   probe()
```

典型 driver：

```c
static struct pci_driver my_driver = {
    .name = "my_driver",
    .id_table = my_ids,
    .probe = my_probe,
    .remove = my_remove,
};
```

---

## 八、一定要彻底理解 PCI Driver 的生命周期

这是你以后排查问题最重要的 mental model。

```text
Kernel
  │
  ▼
pci_register_driver()
  │
  ▼
PCI Core
  │
  │ match PCI device
  ▼
probe()
  │
  ├── enable device
  ├── request BAR
  ├── map MMIO
  ├── allocate DMA
  ├── setup IRQ
  └── initialize hardware
          │
          ▼
      Device Running
          │
          ▼
       interrupt
          │
          ▼
       DMA / MMIO
          │
          ▼
      remove()
          │
          ├── stop hardware
          ├── free IRQ
          ├── free DMA
          ├── unmap BAR
          └── release PCI
```

这条生命周期建议你以后一直保留在脑子里。

---

## 九、Layer 4：BAR / MMIO

这是 PCI driver 第二个核心。

PCI device 通常有：

```text
BAR0
BAR1
BAR2
...
BAR5
```

BAR 本质上是：

> 设备告诉系统："我需要一段 CPU 可以访问的地址空间。"

例如：

```text
CPU virtual address

0xffff....

       │
       ▼
     MMIO
       │
       ▼
┌───────────────┐
│ PCIe Device   │
│               │
│ CTRL          │
│ STATUS        │
│ TX_RING       │
│ RX_RING       │
│ ...           │
└───────────────┘
```

Driver 中常见：

```c
pci_iomap()
```

或者现代代码中的：

```c
pcim_iomap()
```

然后：

```c
readl()
writel()
readq()
writeq()
```

例如：

```c
u32 status;

status = readl(bar0 + STATUS_REG);

writel(CTRL_ENABLE, bar0 + CTRL_REG);
```

这里你必须理解：

```text
readl()
   │
   ▼
CPU
   │
PCIe transaction
   │
   ▼
Device Register
```

这不是普通 RAM。

---

## 十、Layer 5：DMA

这是 PCIe driver 最重要、也最容易踩坑的部分之一。

先建立最简单的模型：

```text
             CPU
              │
              │ MMIO
              ▼
        PCIe Device
              │
              │ DMA
              ▼
         System RAM
```

CPU 不一定负责搬运数据：

传统：

```text
Device → CPU → RAM
RAM → CPU → Device
```

PCIe 高性能设备通常：

```text
Device ─────DMA─────> RAM
Device <────DMA───── RAM
```

因此需要掌握：

- Streaming DMA
  - `dma_map_single()`
  - `dma_unmap_single()`
- Coherent DMA
  - `dma_alloc_coherent()`
  - `dma_free_coherent()`
- DMA address

非常重要：

```text
CPU virtual address
        │
        ▼
     CPU physical
        │
        ▼
    DMA address
```

不要简单认为：

> virtual address == physical address == DMA address

这是 PCIe driver 中非常危险的误区。

---

## 十一、DMA 的真正难点：Ownership

你以后学习 DMA 时，我强烈建议围绕：

> 谁拥有这块 buffer？

来理解。

例如：

```text
CPU owns buffer
      │
      │ give to device
      ▼
Device owns buffer
      │
      │ DMA complete
      ▼
CPU owns buffer
```

这比单纯背：

```c
dma_map_single()
dma_unmap_single()
```

重要得多。

---

## 十二、Layer 6：IRQ / MSI / MSI-X

PCIe 设备不能只靠 CPU polling。

典型流程：

```text
Device
  │
  │ DMA completed
  │
  ▼
MSI-X
  │
  ▼
CPU interrupt
  │
  ▼
Linux IRQ subsystem
  │
  ▼
driver ISR
  │
  ▼
schedule bottom-half / work
```

你需要依次理解：

```text
Legacy INTx
      ↓
     MSI
      ↓
    MSI-X
```

现代 PCIe driver 通常大量使用 **MSI-X**。

Linux API 会涉及：

```c
pci_alloc_irq_vectors()
pci_irq_vector()
request_irq()
free_irq()
```

---

## 十三、把 DMA + IRQ + MMIO 串起来

这是 PCIe driver 最重要的端到端 mental model。

假设：

> Network-like PCIe Device

发送数据：

```text
CPU
 │
 │ prepare descriptor
 │
 ▼
DMA Ring
 │
 │ MMIO doorbell
 ▼
PCIe Device
 │
 │ DMA
 ▼
RAM
```

完成后：

```text
Device
 │
 │ DMA complete
 ▼
MSI-X
 │
 ▼
CPU
 │
 ▼
ISR
 │
 ▼
check descriptor
 │
 ▼
process completion
```

所以 PCIe driver 的核心其实就是：

```text
MMIO
  +
DMA
  +
IRQ
  +
State Machine
```

---

## 十四、Layer 7：PCIe Advanced Features

基础 driver 能写之后，再学习这些。

```text
PCIe Capability
PCI Config Space
      │
      └── PCI Express Capability
```

重点：

- Link Speed
- Link Width
- PCIe generation
- Max Payload Size
- Max Read Request Size
- ASPM
- AER

### Advanced Error Reporting (AER)

```text
Device Error
     │
     ▼
    PCIe
     │
     ▼
    AER
     │
     ▼
Linux PCI Error Recovery
```

对于生产级 PCIe driver 很重要。

### Power Management

学习：

- D0
- D1
- D2
- D3hot
- D3cold

以及：

```c
suspend()
resume()
```

### Hotplug

如果设备可能动态插拔，需要理解：

```text
Device Added
     │
     ▼
Enumeration
     │
     ▼
   probe()
```

以及：

```text
Device Removed
     │
     ▼
   remove()
```

---

## 十五、最终还需要理解 PCIe Transaction Layer

到了这个阶段再学习 PCIe protocol。

至少理解：

```text
CPU
 │
 ▼
Memory Read / Write
 │
 ▼
TLP
 │
 ▼
PCIe Link
 │
 ▼
Endpoint
```

重点理解：

- TLP
- Requester
- Completer
- Address
- Tag
- Payload
- Completion

例如：

### CPU read device register

```text
CPU
 │
 │ Memory Read TLP
 ▼
PCIe
 │
 ▼
Device
 │
 │ Completion TLP
 ▼
CPU
```

而 DMA：

```text
Device
 │
 │ Memory Write TLP
 ▼
System Memory
```

这时候你会突然发现：

> BAR、MMIO、DMA 其实都可以统一到 PCIe transaction 上理解。

---

## 十六、你最终需要掌握的知识地图

可以整理成：

```text
                         PCIe Driver
                              │
        ┌─────────────────────┼──────────────────────┐
        │                     │                       │
     Hardware               Linux                   Driver
        │                     │                       │
        │              Device Model                   │
        │                     │                       │
        ▼                     ▼                       ▼
  PCIe Topology         struct pci_dev            probe/remove
  BDF                   PCI Core                  state machine
  Config Space          Driver Binding             error handling
  BAR                   sysfs
  Capability
        │
        ├──────────────┬──────────────┬───────────────┐
        ▼              ▼              ▼               ▼
      MMIO            DMA            IRQ              PM
        │              │              │               │
      readl         mapping         MSI-X           suspend
      writel        ownership        ISR             resume
        │              │              │
        └──────────────┴──────────────┘
                        │
                        ▼
                 Device Protocol
                        │
                        ▼
                Real Device Driver
```

---

## 十七、按照你的背景，我建议这样学

你已经有比较强的 C/Linux/kernel 基础，所以没必要从 Linux driver 基础重新学很久。

我会建议走：

```text
阶段 1
PCIe Hardware Mental Model
        ↓
阶段 2
PCI Enumeration + Config Space
        ↓
阶段 3
Linux PCI Core
        ↓
阶段 4
BAR + MMIO
        ↓
阶段 5
DMA
        ↓
阶段 6
MSI/MSI-X + IRQ
        ↓
阶段 7
完整 PCIe Driver
        ↓
阶段 8
PCIe TLP / Link / AER / PM
```

尤其不要：

```text
PCIe Specification
        ↓
     读几百页
        ↓
    尝试写 driver
```

而应该：

```text
lspci
  ↓
观察真实 PCIe device
  ↓
理解 struct pci_dev
  ↓
写最小 pci_driver
  ↓
probe()
  ↓
读 Config Space
  ↓
访问 BAR
  ↓
MMIO
  ↓
DMA
  ↓
MSI-X
  ↓
完整 driver
```

---

## 十八、最适合你的第一个实验

我建议不要一开始找真实硬件。

可以做：

```text
QEMU
 │
 ├── 虚拟 PCIe device
 │
 ▼
Linux Kernel
 │
 ▼
你的 PCI driver
```

第一版甚至只做：

```c
static int my_probe(struct pci_dev *pdev,
                     const struct pci_device_id *id)
{
    pr_info("PCI device found\n");

    return 0;
}
```

然后逐步增加：

```text
Experiment 1
    ↓
发现 PCI device

Experiment 2
    ↓
读取 Vendor ID / Device ID

Experiment 3
    ↓
读取 Config Space

Experiment 4
    ↓
enable PCI device

Experiment 5
    ↓
发现 BAR

Experiment 6
    ↓
ioremap BAR

Experiment 7
    ↓
readl()/writel()

Experiment 8
    ↓
DMA

Experiment 9
    ↓
MSI-X

Experiment 10
    ↓
DMA Ring + IRQ

Experiment 11
    ↓
完整 device driver
```

这样学习会非常顺。

---

## 十九、你真正应该达到的最终 Mental Model

最后希望你看到下面代码时，不再把它当 API 列表：

```c
static int my_probe(struct pci_dev *pdev,
                     const struct pci_device_id *id)
{
    pci_enable_device(pdev);

    pci_request_regions(pdev, "mydev");

    bar = pci_iomap(pdev, 0, 0);

    dma_set_mask_and_coherent(...);

    dma_alloc_coherent(...);

    pci_alloc_irq_vectors(...);

    request_irq(...);

    writel(...);

    return 0;
}
```

而是能够在脑子里自动展开成：

```text
PCI Core
   │
   │ gives me
   ▼
pci_dev
   │
   ├── enable hardware
   │
   ├── claim BAR
   │
   ├── map device registers
   │
   ├── establish DMA addressability
   │
   ├── establish CPU ↔ Device buffer ownership
   │
   ├── establish interrupt channel
   │
   └── initialize device state
              │
              ▼
         Device Running
              │
       ┌──────┴──────┐
       ▼             ▼
      DMA           MMIO
       │             │
       └──────┬──────┘
              ▼
             IRQ
              │
              ▼
         Driver State
```

**这就是 PCIe Linux driver 最核心的 mental model。**
