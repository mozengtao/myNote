# Layer 1 — PCI / PCIe 硬件基础

## 1. Mental Model

```
    CPU
     |
Root Complex                <-- CPU 与 PCIe 世界的桥梁
     |
  Root Port                 <-- Root Complex 对外暴露的一个"端口"
     |
 PCIe Switch (可选)          <-- 把一个端口扇出成多个端口
     |
  Endpoint                   <-- 真正的设备 (网卡/NVMe/显卡/...)
```

PCIe 是 **point-to-point**（点对点）的：每一条物理链路只连接两个端点（一端是 Root Port
或 Switch 的下行端口，另一端是下一级设备）。这和老的 PCI（所有设备共享一条并行总线）完全不同。
点对点的好处是每条链路可以独立全速工作，不用像老 PCI 那样"总线仲裁"。又因为是点对点，
PCIe 可以通过 Switch 一层层扇出，天然长成一棵**树**。

## 2. Why

### 2.1 PCI 解决了什么问题 / PCIe 为什么出现

PCI（1992 年）解决的问题是：**标准化"主板上插卡"这件事**，让网卡、显卡这些扩展设备不用每个厂商
各自发明一套总线协议。但 PCI 是并行总线、共享带宽、所有设备抢同一组电气信号——频率越高信号完整性越差，
物理上很难再提速。

PCIe（2003 年起）用串行、点对点链路代替了并行共享总线，每条 Link 可以独立达到很高的频率，
并且可以通过"多条 Lane 并联"（x1/x4/x8/x16）线性提升带宽。这是 PCIe 相对 PCI 最根本的区别。

### 2.2 为什么可以长成树

因为每条链路只连接两个点，一个 Root Port/Switch 的下行端口可以各自接一个独立的下一级设备，
相互之间不干扰，天然支持"一个父节点接多个子节点"的拓扑——这正是树状结构。

## 3. BDF — 怎么给树上每个节点编号

```
Domain : Bus : Device . Function

例如:   0000 : 03    : 00     . 0
```

- **Domain**：当系统有多个独立的 PCI 根（罕见，通常是大型服务器），用 Domain 区分，普通 PC 上几乎总是
  `0000`。
- **Bus**：PCIe 树里每一条"段"（Root Port 到下一级，或 Switch 的每个下行端口）都会被分配一个 bus
  number，这是 enumeration（Layer 2）过程中动态分配的，不是硬件出厂固定的。
- **Device**：同一条 bus 上挂的第几个设备（PCIe 下通常每条 bus 只有一个 Device，因为是点对点；
  Device 字段更多是历史遗留，兼容老 PCI 的多设备共享总线场景）。
- **Function**：一个物理设备内部可能暴露多个"逻辑子设备"（Function），比如一张多功能网卡
  同时暴露网络 function 和管理 function。

**BDF 的本质**：它不是硬件的"身份证号"，而是**软件在某一次 enumeration 过程中分配的地址**。
拔掉重插、换个插槽，甚至只是换一次 BIOS，BDF 都可能变化。这就是为什么驱动代码永远不应该
把 BDF 硬编码当作设备的"真实身份"，而要用 Vendor ID / Device ID 做匹配（Layer 3 会看到）。

## 4. PCI Configuration Space — 设备的"自我介绍"

每个 PCI/PCIe 设备上电后，在能被正常访问之前，先暴露一块很小的**只读/可写寄存器区**，叫
Configuration Space。软件（BIOS 或 Linux 的 PCI Core）靠读取这块区域来认识这个设备是什么、
需要多大的地址空间、支持哪些特性：

```
+-----------------------------------------+
|       PCI Configuration Space           |
|                                          |
|  Vendor ID      <- 谁造的芯片 (如 Intel) |
|  Device ID      <- 具体哪款芯片          |
|  Command        <- 控制位: 是否允许 MMIO |
|                    访问/总线主控(DMA)等   |
|  Status         <- 中断挂起等状态位       |
|  Class Code     <- 这是网卡?显卡?存储控制器?|
|  BAR0 .. BAR5   <- 这个设备需要多大的      |
|                    地址空间来放寄存器      |
|  Capability指针  <- 指向一串"能力链表"：   |
|                    PCIe能力/MSI/MSI-X等   |
+-----------------------------------------+
```

**为什么重要**：Configuration Space 是整条课程的起点——Layer 2 的 enumeration 就是在扫描这块区域；
Layer 3 的 driver matching 比较的就是其中的 Vendor ID / Device ID；Layer 4 的 BAR 映射，
映射的就是 BAR0 这几个字段描述的地址窗口。**后面所有的"Linux 怎么知道的"，根源都是读了这块区域。**

## Layer 1 实验

| Experiment | 目标 | 目录 |
|---|---|---|
| 1.1 | 用 `lspci` 发现 QEMU 虚拟机里的 PCI 设备 | [exp1.1-lspci-basic/](exp1.1-lspci-basic/) |
| 1.2 | 用 `lspci -nn/-vv/-xxx` 逐字段解读 Config Space | [exp1.2-lspci-deep/](exp1.2-lspci-deep/) |

## Checkpoint

- Root Complex / Root Port / Switch / Endpoint 各自是什么角色？
- 为什么 PCIe 的拓扑天然是一棵树？
- BDF 的 4 个字段分别代表什么？为什么 BDF 不能当作设备的"永久身份证"？
- Configuration Space 里哪个字段决定了"这个设备需要多大的 MMIO 空间"？
