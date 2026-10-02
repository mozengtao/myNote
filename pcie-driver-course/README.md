# PCIe Linux Kernel Driver 学习课程

这是一套从零开始、以 **Mental Model 优先** 的方式学习 PCIe Linux Driver 开发的完整课程。
目标不是记住一堆 API，而是建立：

```
PCIe Hardware
      |
PCIe Bus / Topology
      |
PCI Enumeration
      |
Linux PCI Core (struct pci_dev / struct pci_driver)
      |
   probe()
      |
  +---+---+-------+
  |       |       |
 BAR/MMIO DMA  IRQ/MSI-X
  |       |       |
  +---+---+-------+
      |
  Device State
      |
Device-specific Protocol
      |
Real PCIe Driver (DMA Ring)
```

这张图贯穿整个课程，每学一个新概念都要回答：它在这张图的哪个位置？

---

## 快速导航 (Table of Contents)

- [仓库轻量化设计哲学](#仓库轻量化设计哲学)
- [基础设施与脚本](#基础设施与脚本)
- [源码按需下载与环境搭建](#源码按需下载与环境搭建)
- [全课程路线与实验导航](#全课程路线与实验导航)
- [文档与调试地图](#文档与调试地图)
- [编译内核模块（所有 Layer 通用方式）](#编译内核模块所有-layer-通用方式)
- [运行一个实验（通用方式）](#运行一个实验通用方式)
- [已知限制](#已知限制)

---

## 仓库轻量化设计哲学

本仓库为 **Linux PCIe 驱动心智模型与实验教程笔记**，设计上追求极简与纯粹：
- **不常驻数 GB 第三方源码**：大型开源项目源码（Linux 内核、QEMU）不直接存放在顶层目录，平时作为纯净的教程与代码笔记保存，体积仅约数百 KB。
- **按需下载与一键构建**：在需要动手执行实验时，通过提供的脚本一键浅克隆（shallow clone）并编译内核或 QEMU。
- **自定义硬件代码本地持久化**：Capstone 虚拟硬件设备代码 `mypcie.c` 与对应的 Meson 构建补丁持久保存在 `capstone-dma-ring/qemu-device/`，清理 QEMU 源码后绝不丢失。
- **做完即删，零负担归档**：实验完成后可随时 `rm -rf kernel qemu-src` 释放数 GB 磁盘空间，仓库依然是结构清晰、可随时复现的独立知识库。

---

## 基础设施与脚本

| 组件 / 脚本 | 说明 |
|---|---|
| [`scripts/setup-kernel.sh`](scripts/setup-kernel.sh) | **按需内核准备脚本**：浅克隆 `v6.12` LTS 内核，配置 `x86_64_defconfig` 并编译 `bzImage` 与模块准备环境。 |
| [`scripts/setup-qemu-mypcie.sh`](scripts/setup-qemu-mypcie.sh) | **Capstone 专用 QEMU 准备脚本**：仅在 Capstone 阶段使用，浅克隆 `v8.2.2`，自动打上 `mypcie` 设备驱动补丁并编译生成 `qemu-system-x86_64`。 |
| [`common/build-initramfs.sh`](common/build-initramfs.sh) | 所有 Layer 共用的最小 initramfs 构建脚本，基于本机静态链接的 `busybox`。 |
| [`common/run-qemu.sh`](common/run-qemu.sh) | 所有 Layer 共用的 QEMU 启动脚本，默认使用系统自带 QEMU 或自定义编译的二进制。 |
| `capstone-dma-ring/qemu-device/mypcie.c` | Capstone 实验自定义 PCIe 硬件模型源文件（持久化备份，不随源码删除而丢失）。 |
| `capstone-dma-ring/qemu-device/mypcie.patch` | QEMU `hw/misc/meson.build` 注册补丁。 |

---

## 源码按需下载与环境搭建

### 1. 宿主机前置依赖安装 (Ubuntu / Debian / WSL2)

```bash
# 基础编译与内核构建工具
sudo apt-get update
sudo apt-get install -y build-essential libncurses-dev bison flex libssl-dev libelf-dev bc git qemu-system-x86 busybox-static

# 如果需要进行 Capstone 实验（编译自定义 QEMU），还需安装：
sudo apt-get install -y ninja-build python3 pkg-config libglib2.0-dev libpixman-1-dev
```

### 2. 下载并准备 Linux 内核 (Layer 0 ~ 8 必需)

执行实验前，在工程根目录运行：

```bash
./scripts/setup-kernel.sh
```

> **手动逐步执行等效命令**：
> ```bash
> # 浅克隆 v6.12 (约 200MB，不克隆完整历史)
> git clone --depth 1 --branch v6.12 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git kernel
> # 生成默认配置并编译 bzImage
> make -C kernel x86_64_defconfig
> make -C kernel -j$(nproc) bzImage modules_prepare
> ```

### 3. 下载并准备自定义 QEMU (仅 Capstone 阶段需要)

Layer 0 ~ 8 使用系统自带的 `qemu-system-x86_64` 配合内置 `edu` 设备即可。只有做 Capstone 项目时才需运行：

```bash
./scripts/setup-qemu-mypcie.sh
```

> **手动逐步执行等效命令**：
> ```bash
> git clone --depth 1 --branch v8.2.2 https://gitlab.com/qemu-project/qemu.git qemu-src
> cp capstone-dma-ring/qemu-device/mypcie.c qemu-src/hw/misc/
> patch -d qemu-src -p1 < capstone-dma-ring/qemu-device/mypcie.patch
> cd qemu-src && mkdir -p build && cd build
> ../configure --target-list=x86_64-softmmu --disable-docs --disable-gtk --disable-sdl
> ninja qemu-system-x86_64
> ```

### 4. 实验完成后的磁盘释放

实验完成后，若想恢复仓库为纯净的笔记与工程代码状态：

```bash
# 释放约 2.8 GB 磁盘空间
rm -rf kernel qemu-src
```


---

## 全课程路线与实验导航

按推荐顺序依序学习。每个 Layer 均包含独立实验与详尽设计文档：

| 层级 / 阶段 | 核心主题 | 实验与文档链接 |
|---|---|---|
| **[Layer 0: Linux Device Model](layer0-device-model/README.md)** | Linux 设备模型心智模型：`bus` / `device` / `driver` 三元组与内核模块生命周期 | • [Exp 0.1: 最小内核模块](layer0-device-model/exp0.1-minimal-module/README.md) · [实操手册](layer0-device-model/exp0.1-minimal-module/MANUAL.md)<br>• [Exp 0.2: Platform 设备与驱动匹配](layer0-device-model/exp0.2-platform-device/README.md) · [实操手册](layer0-device-model/exp0.2-platform-device/MANUAL.md) |
| **[Layer 1: PCIe 硬件基础](layer1-pci-basics/README.md)** | PCIe 硬件拓扑、BDF 寻址规范与 PCI 配置空间（Config Space）物理结构 | • [Exp 1.1: lspci 拓扑探测与设备识别](layer1-pci-basics/exp1.1-lspci-basic/README.md) · [实操手册](layer1-pci-basics/exp1.1-lspci-basic/MANUAL.md)<br>• [Exp 1.2: Config Space 深度转储与字段解析](layer1-pci-basics/exp1.2-lspci-deep/README.md) · [实操手册](layer1-pci-basics/exp1.2-lspci-deep/MANUAL.md) |
| **[Layer 2: PCI Enumeration](layer2-enumeration/README.md)** | Linux 内核 PCI 枚举机制（`pci_scan_bus`）、资源分配与 `struct pci_dev` 构建 | • [Exp 2.1: 内核 PCI 枚举链路 Trace](layer2-enumeration/exp2.1-scan-trace/README.md) · [实操手册](layer2-enumeration/exp2.1-scan-trace/MANUAL.md)<br>• [Exp 2.2: sysfs 与 /proc/iomem 资源映射观察](layer2-enumeration/exp2.2-sysfs-iomem/README.md) · [实操手册](layer2-enumeration/exp2.2-sysfs-iomem/MANUAL.md) |
| **[Layer 3: Linux PCI Core](layer3-pci-core/README.md)** | `struct pci_driver`、ID Table 匹配原理与 `probe()` / `remove()` 机制 | • [Exp 3.1: 匹配 QEMU edu 的最小 PCI 驱动](layer3-pci-core/exp3.1-minimal-pci-driver/README.md) · [实操手册](layer3-pci-core/exp3.1-minimal-pci-driver/MANUAL.md)<br>• [Exp 3.2: 故障注入：Device ID 不匹配实验](layer3-pci-core/exp3.2-matching-failure/README.md) · [实操手册](layer3-pci-core/exp3.2-matching-failure/MANUAL.md) |
| **[Layer 4: BAR 与 MMIO](layer4-bar-mmio/README.md)** | Base Address Register (BAR) 物理资源解析、MMIO 内存映射与寄存器读写规范 | • [Exp 4.1: BAR 物理资源探测与解析](layer4-bar-mmio/exp4.1-bar-resources/README.md) · [实操手册](layer4-bar-mmio/exp4.1-bar-resources/MANUAL.md)<br>• [Exp 4.2: MMIO 映射与寄存器读写实操](layer4-bar-mmio/exp4.2-mmio-readwrite/README.md) · [实操手册](layer4-bar-mmio/exp4.2-mmio-readwrite/MANUAL.md) |
| **[Layer 5: DMA 与 Buffer Ownership](layer5-dma/README.md)** | 一致性 DMA（`dma_alloc_coherent`）、硬件所有权流转状态机与 Polling 轮询 | • [Exp 5.1: Coherent DMA 传输与轮询完成验证](layer5-dma/exp5.1-dma-polling/README.md) · [实操手册](layer5-dma/exp5.1-dma-polling/MANUAL.md) |
| **[Layer 6: 中断系统 (MSI/MSI-X)](layer6-irq-msi/README.md)** | 从 Polling 到 Interrupt-Driven、MSI 中断向量分配与顶半部中断处理例程（ISR） | • [Exp 6.1: MSI 中断驱动传输与 ISR 流程实战](layer6-irq-msi/exp6.1-msi-interrupt/README.md) · [实操手册](layer6-irq-msi/exp6.1-msi-interrupt/MANUAL.md) |
| **[Layer 7: PCIe 高级特性](layer7-advanced/README.md)** | Extended Capabilities、AER 错误报告机制、PCI 电源管理与设备热插拔生命周期 | • [Exp 7.1: QEMU Monitor 热插拔生命周期验证](layer7-advanced/exp7.1-hotplug-lifecycle/README.md) · [实操手册](layer7-advanced/exp7.1-hotplug-lifecycle/MANUAL.md) |
| **[Layer 8: 生产级真实 PCI 驱动](layer8-real-driver/README.md)** | 整合 Layer 3~6 完整链路，构建具备严密状态机与健壮错误回滚的完整生产级驱动 | • [Exp 8.1: 状态机完备的生产级 edu 驱动](layer8-real-driver/exp8.1-full-edu-driver/README.md) · [实操手册](layer8-real-driver/exp8.1-full-edu-driver/MANUAL.md) |
| **[Capstone: DMA Ring 项目](capstone-dma-ring/driver/README.md)** | 自定义 QEMU PCIe 虚拟设备 + 环形缓冲区 DMA Ring 引擎 + MSI-X + 用户态测试套件 | • [QEMU `mypcie` 自定义设备实现](capstone-dma-ring/qemu-device/README.md)<br>• [Linux `mypcie_driver` 内核驱动](capstone-dma-ring/driver/README.md)<br>• [用户态测试程序 (`userspace-test`)](capstone-dma-ring/userspace-test/test_app.c)<br>• [Capstone 完整实操手册](capstone-dma-ring/MANUAL.md) |

每个 Layer 目录下的每个 `expX.Y-xxx/` 子目录是一个**独立可运行**的实验，包含：

```
expX.Y-xxx/
├── README.md     按照 Why / Architecture / Data Flow / Control Flow /
│                 State / Code / Build / Run / Observe / Failure Injection
│                 / Summary 的固定模板编写
├── MANUAL.md     脱离自动化脚本的手动编译、打包、QEMU 启动与控制台交互实操手册
├── Makefile      内核模块 Makefile（out-of-tree module 编译方式）
├── *.c           驱动源码（代码/术语英文，注释中文）
└── test.sh       (如适用) 注入进 initramfs 自动运行的测试脚本
```

---

## 文档与调试地图

| 文档 | 说明 | 链接 |
|---|---|---|
| 全层级 Mental Model 概念地图 | 汇总 Layer 0 到 Layer 8 及 Capstone 的完整 ASCII 架构图与思维模型 | [`docs/mental-model-maps/all_layers.md`](docs/mental-model-maps/all_layers.md) |
| 全层级故障诊断与调试地图 | 严格遵循 Layer -> Object -> State -> Relationship -> Symptom -> Action 的排错指南 | [`docs/debugging-map.md`](docs/debugging-map.md) |

---

## 编译内核模块（所有 Layer 通用方式）

所有实验模块都是 **out-of-tree kernel module**，统一用这种方式编译：

```bash
make -C /home/morrism/repos/pcie-driver-course/kernel M=$(pwd) modules
```

## 运行一个实验（通用方式）

```bash
# 1. 构建该实验专属的 initramfs（把编译好的 .ko 和 test.sh 塞进去）
../../common/build-initramfs.sh ./build initramfs.cpio.gz

# 2. 启动 QEMU（Layer 0-6 使用发行版自带 QEMU + edu device；按需加 -device edu）
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 已知限制

本课程按照 "all at once" 的节奏一次性产出全部内容（已与用户确认）。基础设施（kernel 编译、initramfs
工具链、QEMU 启动）做了一次 smoke test 确认链路通畅，但不是每一个 Layer 的每一行代码都逐一在 QEMU 里跑过。
建议读者按 Layer 顺序亲自 build + run 一遍，这也正是整个课程方法论的核心——**实验优先于源码，源码优先于记忆 API**。
