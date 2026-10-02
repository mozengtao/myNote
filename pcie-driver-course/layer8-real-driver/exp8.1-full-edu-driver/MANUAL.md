# Experiment 8.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：手动加载生产级整合驱动 `edu_full_driver.ko`，观察其具备显式状态机（Explicit State Machine: `RESET` -> `PROBED` -> `RESOURCES_READY` -> `READY` -> `RUNNING` -> `STOPPING` -> `STOPPED`）的单调演进与自检流程，体验规范驱动在资源申请失败时的逆序回滚设计。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 edu_full_driver.ko                      |
| 2. build-initramfs.sh -> 制作仅含 .ko 的内存盘              |
| 3. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/edu_full_driver.ko                     |
| - dmesg | grep "State Transition"                           |
| - rmmod edu_full_driver                                     |
| - dmesg | tail -n 8 (观察逆序优雅排空)                      |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer8-real-driver/exp8.1-full-edu-driver

# 1. 编译内核模块
make

# 2. 准备纯手动交互打包目录 (仅放置 .ko)
rm -rf build && mkdir -p build
cp edu_full_driver.ko build/

# 3. 生成 initramfs
../../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动 QEMU 虚拟机 (Host)

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 控制台内依次手动执行以下分析步骤：

```sh
# 1. 加载生产级整合驱动
insmod /root/extra/edu_full_driver.ko

# 2. 关键检查：查看驱动状态机在 probe 期间的单调递增跃迁
dmesg | grep "State Transition"

# 3. 查看自检数据传输结果
dmesg | grep -A 4 "Self-test finished"

# 4. 查看系统分配的中断向量与独占 MMIO 资源
cat /proc/interrupts | grep "edu_full"
cat /proc/iomem | grep "edu_full"

# 5. 手动卸载模块，观察 STOPPING -> STOPPED -> RESET 的严密排空注销链路
rmmod edu_full_driver

# 6. 检查内核日志确认逆序释放
dmesg | tail -n 8

# 7. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `dmesg \| grep "State Transition"` (加载) | `[RESET] -> [PROBED]`<br>`[PROBED] -> [RESOURCES_READY]`<br>`[RESOURCES_READY] -> [READY]`<br>`[READY] -> [RUNNING]` | 驱动按部就班使能设备、映射 MMIO、申请 Coherent DMA 内存、挂载 MSI 向量并运行自检 |
| `dmesg` 自检结果 | `edu_full_driver: Self-test finished successfully: PASSED` | MMIO 寄存器反转校验与 MSI 中断唤醒下的 Coherent DMA 双向搬运全部正确 |
| `dmesg \| tail` (卸载) | `[RUNNING] -> [STOPPING]`<br>`[STOPPING] -> [STOPPED]`<br>`[STOPPED] -> [RESET]`<br>`edu_full_driver: Removed safely` | 驱动首先禁用硬件 DMA 中断源，再释放 IRQ 向量，最后注销 DMA 缓冲区与 MMIO 映射 |

---

## 6. 架构反思

对照 `edu_full_driver.c` 源码中的 `err_free_irq`, `err_free_dma`, `err_iounmap`, `err_release_regions`, `err_disable` 标签：
任何环节若在现场出错（例如内存不足或 IRQ 耗尽），驱动均能沿着 `goto` 链路实现 100% 对称的逆序回退，绝不遗留半分配的野指针或挂起资源。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

