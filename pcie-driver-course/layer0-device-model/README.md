# Layer 0 — Linux Device Driver 基础

## 1. Mental Model

在学 PCIe 之前，必须先建立 Linux Device Model 的 Mental Model。PCI driver 不是一个独立的世界，
它只是 Linux Device Model 的一个**具体实现**：

```
                     Linux Device Model

                           Bus
                            |
                 +----------+----------+
                 |                     |
               Device                Driver
                 |                     |
                 +---------match-------+
                            |
                            v
                          probe()
```

- `Bus`：一种连接 Device 和 Driver 的"撮合平台"。PCI bus、platform bus、USB bus 都是 `struct bus_type`
  的具体实例。
- `Device`：硬件的内核表示。PCI 设备对应 `struct pci_dev`，它内部包含一个通用的 `struct device`。
- `Driver`：软件侧的处理逻辑。PCI driver 对应 `struct pci_driver`，内部包含一个通用的
  `struct device_driver`。
- `probe()`：Bus 发现 Device 和 Driver "匹配"后调用的函数——这是几乎所有 Linux driver 代码的真正入口。

**为什么要先学这个？** 因为后面整本课程看到的 `pci_register_driver()`、`.probe`、`.remove` 字段，
全部都是这个通用模型在 PCI bus 上的"特化"。理解了这张图，PCI driver 的结构就不再是"要背的 API"，
而是"通用模式在 PCI 上的一个实例"。

## 2. Why

没有 Device Model 之前，每种总线（PCI/USB/platform/...）都会各写一套"发现硬件 -> 匹配驱动 -> 初始化"
的逻辑，大量重复代码。Linux Device Model 把这件事情抽象成统一的三元组 (Bus, Device, Driver) +
一个统一的匹配机制，所有总线复用同一套基础设施（sysfs 展示、电源管理、热插拔等）。

## 3. Module 生命周期

在深入 Device Model 之前，先建立最基础的"代码如何进入内核运行"的生命周期：

```
insmod
   |
   v
module_init()       <-- 模块代码第一次执行的地方
   |
   v
register driver     <-- 通常在 module_init() 里调用，比如 pci_register_driver()
   |
   v
driver active        <-- 等待 Bus 做 matching，可能立刻触发 probe()，也可能永远不触发
   |
   v
rmmod
   |
   v
unregister driver     <-- 通常在 module_exit() 里调用
   |
   v
module_exit()
```

**关键点**：`module_init()` 本身并不等于"硬件已经初始化"。它通常只是"向某个 bus 注册了一个 driver"，
真正的硬件初始化发生在 `probe()`，而 `probe()` 什么时候被调用、是否会被调用，完全由 bus 的匹配机制决定。
这是初学者最容易搞混的一点。

## Layer 0 实验列表

| Experiment | 新概念 | 目录 |
|---|---|---|
| 0.1 | `module_init`/`module_exit`，模块生命周期 | [exp0.1-minimal-module/](exp0.1-minimal-module/) |
| 0.2 | `struct device` / `struct bus_type` / `struct device_driver`，driver matching | [exp0.2-platform-device/](exp0.2-platform-device/) |

## Checkpoint（完成本 Layer 后应该能回答）

- `insmod` 之后，内核做了哪些事情，才执行到我写的 `module_init()`？
- `module_init()` 返回之后，我的驱动代码还会不会继续执行？什么时候会？
- Bus / Device / Driver 这三者，分别对应代码里的哪个结构体？
- "matching" 具体指的是什么被拿来比较？
- 为什么 PCI driver 的 `struct pci_driver` 里会有一个 `.driver` 字段（嵌套的 `struct device_driver`）？
