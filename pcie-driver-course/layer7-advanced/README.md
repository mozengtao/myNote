# Layer 7 — PCIe 高级特性与生产级机制

## 1. Mental Model

```
                          Production PCIe Driver Architecture

      +-------------------------------------------------------------------------+
      | PCIe Link Control: Link Speed (Gen1..5), Link Width (x1..x16), MPS/MRRS |
      +-------------------------------------------------------------------------+
                                          |
      +-----------------------------------+-------------------------------------+
      |                                   |                                     |
      v                                   v                                     v
+-----------------------+     +-----------------------+     +-----------------------+
| AER 错误处理与故障自愈 |     | PM 电源管理 (D0..D3)  |     | Hotplug 热插拔与动态注销|
| - error_detected()    |     | - suspend() 状态保存  |     | - probe() 动态加入    |
| - slot_reset()        |     | - resume() 状态恢复   |     | - remove() 安全析构   |
| - resume()            |     |                       |     |                       |
+-----------------------+     +-----------------------+     +-----------------------+
```

## 2. PCIe Capability 与链路状态协商

真实的 PCIe 设备在配置空间的 `0x34` 之后维护着能力链表（Capabilities List）。其中最核心的是 **PCI Express Capability (ID: 0x10)**：
- **Link Capabilities / Status / Control**：
  - **Link Speed**：Gen1 (2.5 GT/s), Gen2 (5 GT/s), Gen3 (8 GT/s), Gen4 (16 GT/s), Gen5 (32 GT/s)。
  - **Link Width**：x1, x2, x4, x8, x16。
  - **MPS（Max Payload Size）**：一个 TLP 数据包能携带的最大数据载荷（通常为 128B ~ 4096B）。
  - **MRRS（Max Read Request Size）**：一次发起内存读请求的最大字节数（通常为 512B）。
  - 驱动可以通过 `pcie_get_readrq()` / `pcie_set_readrq()` 调优吞吐量性能。

## 3. PCIe TLP（Transaction Layer Packet）事务包模型

在 PCIe 链路上传递的所有读写交互，在物理线路上都不是裸电平，而是被封装成标准协议报文——**TLP**：
```
+-------------+----------------+----------------+----------------+-------------+
| Header      | Requester ID   | Address / Tag  | Length         | Data Payload|
| (3-4 Dwords)| (Bus:Dev.Func) |                | (以 4字节为单位)| (0-4096B)   |
+-------------+----------------+----------------+----------------+-------------+
```
- **Posted 事务**（如 Memory Write）：发射后不需要对端回复确认，只管写，速度极快（“发射即遗忘”）。
- **Non-Posted 事务**（如 Memory Read）：发射后 Requester 必须等待 Completer 发回 **CplD (Completion with Data)** 报文，耗时较长。
- 这也是为什么在驱动设计中，大量写入优于反复读取；DMA 批量写入能跑满总线带宽。

## 4. AER（Advanced Error Reporting）与驱动容灾恢复

在机房和企业级服务器中，PCIe 信号受高频干扰可能会出现瞬时误码（PCIe Correctable Error）或严重链路断开（Uncorrectable Fatal Error）。生产级驱动通过实现 `struct pci_error_handlers` 来参与容灾：
```c
static const struct pci_error_handlers my_err_handler = {
    .error_detected = my_error_detected,
    .slot_reset     = my_slot_reset,
    .resume         = my_resume,
};
```
- **`error_detected()`**：总线报告链路故障。驱动必须立刻停止向硬件下发新的 MMIO 与 DMA 请求，等待复位。
- **`slot_reset()`**：PCIe 插槽硬件复位完成。驱动在此处重新初始化寄存器和硬件状态机。
- **`resume()`**：恢复正常的系统 I/O 通信。

## 5. 电源管理（Power Management: D0..D3）

PCI 设备支持 4 种电源状态：
- **D0 (Fully On)**：全功能工作状态，所有寄存器处于活动状态。
- **D3hot / D3cold (Off)**：节能断电状态，设备内部上下文全部丢失。
驱动在系统休眠（`suspend`）时必须把网卡或 DMA 队列的关键配置暂存在主机内存中；系统唤醒（`resume`）时重新灌入硬件寄存器，完成设备上下文与软件状态的无缝对齐。

## 6. Hotplug（热插拔）

PCIe 支持硬件带电拔插。
当硬件在运行时被拔下：
1. Root Port 检测到 Link Down 事件；
2. PCIe Native Hotplug 驱动（`pciehp`）捕获中断；
3. 内核遍历该槽位绑定的驱动，强制调用驱动的 **`remove()`**；
4. 驱动必须安全释放所有未完成的 DMA、注销 IRQ 并反映射 BAR。
若驱动在 `remove()` 中发生死锁或野指针访问，整个操作系统将直接 Kernel Panic！

## Layer 7 实验

| Experiment | 目标 | 目录 |
|---|---|---|
| 7.1 | 在 QEMU Monitor 中动态执行 `device_add` 与 `device_del`，验证驱动的动态热插拔与生命周期安全 | [exp7.1-hotplug-lifecycle/](exp7.1-hotplug-lifecycle/) |

## Checkpoint

- TLP 中 Posted 与 Non-Posted 事务的根本区别是什么？对驱动读写性能有何影响？
- AER 错误恢复流程的三部曲（`error_detected` -> `slot_reset` -> `resume`）分别需要驱动做些什么？
- 为什么在支持热插拔的驱动中，`remove()` 的健壮性必须做到“可以在任何异常时刻被随时调用”？
