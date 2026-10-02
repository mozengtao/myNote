# PCIe Linux Driver 全层级故障诊断与调试地图 (Debugging Map)

本排查地图严格遵循 **Which Layer -> Which Object -> Which State -> Which Relationship -> What Observation -> What Failure** 的诊断法则。

```
+------------------------------------+
| 1. Device 物理/虚拟硬件根本不存在  |
+------------------------------------+
                  |
                  v
       [Layer 2: PCI Enumeration]
       - 检查: 宿主机/QEMU 启动参数是否挂载 `-device edu` 或 `-device mypcie`
       - 观测: `lspci` 是否列出对应 BDF；`dmesg | grep -i pci`
       - 根因: 硬件未供电、插槽松动或 Vendor ID 读出为全 1 (0xFFFF)

+------------------------------------+
| 2. Device 存在但 probe() 始终不执行|
+------------------------------------+
                  |
                  v
       [Layer 3: Driver Matching]
       - 检查: 驱动的 `struct pci_device_id id_table[]` 中的 Vendor/Device ID
       - 观测: `ls /sys/bus/pci/devices/<BDF>/driver` 是否存在链接
       - 根因: ID 哪怕错 1 比特，PCI Core 也会静默跳过，但 `insmod` 仍显示返回 0

+------------------------------------+
| 3. probe() 执行但返回负数退出失败   |
+------------------------------------+
                  |
                  v
       [Layer 4: Resource Initialization]
       - 检查: `pci_enable_device()` / `pci_request_regions()` / `pci_iomap()` 返回值
       - 观测: `cat /proc/iomem` 查看地址是否被其他驱动抢占独占冲突
       - 根因: 资源未释放、BAR 长度为 0 或内核物理内存资源耗尽

+------------------------------------+
| 4. BAR MMIO 读取恒为 0xFFFFFFFF    |
+------------------------------------+
                  |
                  v
       [Layer 4: MMIO / Hardware Enablement]
       - 检查: 是否漏调了 `pci_enable_device(pdev)`
       - 观测: `lspci -vv` 中设备的 `Control: Mem-` 是否处于关闭状态
       - 根因: Command 寄存器的 Memory 空间访问位未使能，总线未将事务路由给芯片

+------------------------------------+
| 5. DMA 传输超时、无法清零 RUN 位   |
+------------------------------------+
                  |
                  v
       [Layer 5: DMA Mapping & Bus Master]
       - 检查: 是否遗漏了 `pci_set_master(pdev)`；是否配置了正确的 DMA 寻址 Mask
       - 观测: 传入寄存器的是否为 CPU 虚拟地址而非 `dma_addr_t` 物理总线地址
       - 根因: 硬件无 Bus Master 权限被总线阻断，或设备无法寻址 32位以上物理内存

+------------------------------------+
| 6. 中断永不触发 / wait_for_completion 超时 |
+------------------------------------+
                  |
                  v
       [Layer 6: MSI-X / IRQ Acknowledgement]
       - 检查: `cat /proc/interrupts` 中断计数是否完全不增长；硬件中断使能位是否置 1
       - 观测: ISR 中是否向硬件写入了 `IRQ_ACK` 寄存器清除状态
       - 根因: 硬件未发送 MSI 事务包，或因未清除 Pending 锁存导致无法产生后续中断跳变

+------------------------------------+
| 7. DMA Ring 卡住不前进             |
+------------------------------------+
                  |
                  v
       [Layer 8 & Capstone: Ring Protocol]
       - 检查: Producer、Consumer 索引回环计算 (`% RING_SIZE`) 是否越界
       - 观测: 是否向 `DOORBELL` 门铃寄存器写入了通知信号
       - 根因: 软件填了描述符但硬件不知情，或消费者追上生产者导致环形队列假死溢出

+------------------------------------+
| 8. rmmod 时内核崩溃 (Kernel Oops)  |
+------------------------------------+
                  |
                  v
       [Layer 8: Lifecycle Teardown]
       - 检查: `remove()` 中的资源释放顺序是否与 `probe()` 严格对称逆序
       - 观测: 是否在释放 DMA Buffer 之前未先发指令叫停硬件 DMA 搬运
       - 根因: 硬件继续往已被 `dma_free_coherent` 释放的野指针物理内存写入数据
```
