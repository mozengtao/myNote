# Experiment 0.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：脱离自动化脚本，手动编译、打包最小 Linux 内核模块，在 QEMU 交互 Shell 中完成加载、卸载与内核生命周期观察。
* **硬件依赖**：无额外硬件需求，使用基础 QEMU 虚拟机环境即可。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. make -> 编译产出 hello.ko                                |
| 2. build-initramfs.sh -> 制作仅含 hello.ko 的内存盘         |
| 3. run-qemu.sh -> 启动串口交互虚拟机                        |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - insmod /root/extra/hello.ko                               |
| - lsmod / dmesg / ls /sys/module/hello                      |
| - rmmod hello                                               |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与编译构建 (Host)

在宿主机终端进入实验目录，编译模块并打包进交互式 initramfs：

```bash
cd /home/morrism/repos/pcie-driver-course/layer0-device-model/exp0.1-minimal-module

# 1. 编译内核模块 (out-of-tree 方式)
make

# 2. 准备纯手动交互打包目录 (注意：不要拷贝 test.sh，避免开机自动执行关机)
rm -rf build && mkdir -p build
cp hello.ko build/

# 3. 生成最小 initramfs
../../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动 QEMU 虚拟机 (Host)

启动虚拟机并进入控制台：

```bash
../../common/run-qemu.sh initramfs.cpio.gz
```

> **控制台操作提示**：
> * QEMU 启动后直接进入系统 `/bin/sh` 命令行界面。
> * 实验完毕在虚拟机内执行 `poweroff -f` 关机。
> * 若遇到异常挂起，可在宿主机终端按 `Ctrl-A` 松开后再按 `x` 强制退出 QEMU。

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 内依次输入以下命令：

```sh
# 1. 查看当前加载的模块列表（此时应该为空）
lsmod

# 2. 手动加载模块
insmod /root/extra/hello.ko

# 3. 检查内核日志确认 module_init 执行
dmesg | tail -n 5

# 4. 观察模块常驻状态
lsmod

# 5. 观察 Device Model 在 sysfs 中自动建立的模块属性节点
ls -la /sys/module/hello/

# 6. 手动卸载模块
rmmod hello

# 7. 检查内核日志确认 module_exit 执行
dmesg | tail -n 5

# 8. 验证 sysfs 节点随之注销
ls /sys/module/hello/ 2>/dev/null || echo "sysfs 节点已安全注销"

# 9. 实验完成，退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `insmod hello.ko` | 返回码为 0，无报错 | ELF 格式校验与未解析符号绑定成功 |
| `dmesg \| tail` | `hello: module_init() called, module is now LOADED` | 内核 `do_init_module()` 成功回调 `hello_init` |
| `lsmod` | 显示 `hello 16384 0` (大小随平台略有差异) | 模块已进入系统 `LOADED` 链表常驻 |
| `ls /sys/module/hello/` | 存在 `refcnt`, `sections`, `notes` 等文件 | `kobject` 机制自动为新模块建立 sysfs 反射 |
| `rmmod hello` | 返回码为 0 | 内核找到模块并触发 `hello_exit` |
| `dmesg \| tail` | `hello: module_exit() called, module is now UNLOADING` | 模块清理钩子正常执行，内存安全释放 |

---

## 6. 故障注入尝试 (可选实战)

修改 `hello.c` 中的 `hello_init()` 返回 `-EINVAL`：
```c
static int __init hello_init(void)
{
    pr_info("hello: inject error test\n");
    return -EINVAL;
}
```
重新 `make` 并打包运行，手动执行 `insmod`，观察：
* `insmod` 直接报错 `Operation not permitted` 或 `Invalid argument`。
* `dmesg` 仅打印注入错误日志，**绝不会**触发 `hello_exit`。
* `lsmod` 中没有 `hello`。
证明 `module_init` 返回非 0 时内核状态机直接回滚。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理内核编译生成的目标文件、模块文件、临时打包目录与内存盘镜像：

```bash
# 在宿主机当前实验目录下执行：
make clean
rm -rf build initramfs.cpio.gz
```

