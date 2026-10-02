# Capstone Project: Linux PCIe Driver (`mypcie_driver`)

## 1. 终极架构图 (End-to-End System Architecture)

```
+---------------------------------------------------------------------------------+
| Userspace Application (userspace-test/test_app.c)                               |
|                                                                                 |
| 1. open("/dev/mypcie0", O_RDWR)                                                 |
| 2. ioctl(fd, MYPCIE_IOCTL_SUBMIT_REQ, &req)                                     |
+---------------------------------------------------------------------------------+
                                      |
                                      | ioctl 系统调用
                                      v
+---------------------------------------------------------------------------------+
| Linux Kernel PCIe Driver (capstone-dma-ring/driver/mypcie_driver.c)             |
|                                                                                 |
| [1] 申请/获取当前空闲槽位: slot = dev->producer % RING_SIZE                     |
| [2] 填充描述符: desc[slot].dma_addr = buf_dma, desc[slot].length = 64          |
| [3] 递增 Producer 并敲门铃:                                                     |
|       writel(slot + 1, mmio + REG_PRODUCER);                                    |
|       writel(1, mmio + REG_DOORBELL);                                           |
| [4] 当前线程进入 sleep: wait_for_completion_interruptible(&req_done)             |
+---------------------------------------------------------------------------------+
                                      |
                                      | PCIe MMIO Doorbell Write TLP
                                      v
+---------------------------------------------------------------------------------+
| Virtual PCIe Device (qemu-src/hw/misc/mypcie.c)                                 |
|                                                                                 |
| [1] 捕获 Doorbell 写入 -> 激活 mypcie_process_ring()                            |
| [2] DMA 读取 Host 内存中的描述符结构体 (pci_dma_read)                           |
| [3] 执行实际数据 DMA 搬运 (RAM <-> Device)                                      |
| [4] 回写描述符 DONE 标志位 (pci_dma_write)                                       |
| [5] 递增硬件 Consumer 索引: consumer++                                          |
| [6] 发射 MSI-X 向量 0 中断报文到 APIC 控制器 (msix_notify)                      |
+---------------------------------------------------------------------------------+
                                      |
                                      | MSI-X Interrupt TLP
                                      v
+---------------------------------------------------------------------------------+
| Driver MSI-X ISR (mypcie_msix_handler)                                          |
|                                                                                 |
| [1] 读取并确认 IRQ_STATUS                                                       |
| [2] 向 REG_IRQ_ACK 写入 1 消除硬件中断                                          |
| [3] 遍历完成的描述符，回收缓冲区所有权                                           |
| [4] complete(&req_done) 唤醒用户态进程返回结果                                  |
+---------------------------------------------------------------------------------+
```

## 2. 核心源码构成

- `mypcie_driver.c`：包含全套字符设备 `cdev` 注册、`file_operations`、`ioctl` 接口、一致性 DMA Ring 分配、MSI-X 向量挂载、全套 8 级状态机；
- `mypcie_uapi.h`：用户态与内核态共享的通信数据结构与 ioctl 编号；
- `test_app.c`：用户态测试程序，发起真实传输请求并校验硬件回填结果。
