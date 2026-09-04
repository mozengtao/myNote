# C 语言共享内存 Seqlock 工程级精通指南与实战代码库

本项目为系统级 C 语言工程师量身打造，涵盖基于 POSIX 共享内存（Shared Memory IPC）、C11/GCC 原子内建函数（`__atomic_*`）、DPDK 风格高频遥测统计以及 Linux 内核 `seqcount` 架构的完整理论与工程实现。

---

## 项目导航与目录结构

```text
seqlock_mastery/
├── Makefile                # 严谨工业级构建配置 (-Werror -pedantic -std=c11)
├── README.md               # 本导航说明文档
├── run_tests.sh            # 一键自动化并发竞态与抗崩溃测试套件
├── seqlock_guide.md        # 完整 16 章节核心理论白皮书与速查表
├── include/
│   └── seqlock.h           # 可复用的工业级 Seqlock / seqcount 头文件
└── src/
    ├── 01_simplest_seqlock.c    # 示例一：单写单读最简共享内存 Seqlock
    ├── 02_multi_reader_stats.c  # 示例二：单写多读高频网络统计无锁并发验证
    ├── 03_posix_shm_writer.c    # 示例三：POSIX 共享内存跨进程写者进程
    ├── 03_posix_shm_reader.c    # 示例三：POSIX 共享内存跨进程只读客户端 (PROT_READ)
    ├── 04_dpdk_telemetry.c      # 示例四：DPDK 风格 64 字节缓存行对齐遥测统计
    └── 05_kernel_seqcount.c     # 示例五：Linux 内核 seqcount_t 与多写者自旋锁模拟
```

---

## 快速上手与运行

### 1. 一键编译所有示例
项目已配置严苛的编译参数（`-O2 -Wall -Wextra -Werror -pedantic -std=c11 -pthread -lrt -D_GNU_SOURCE`）：

```bash
make clean
make all
```

编译输出的所有可执行文件均存放在 `bin/` 目录下。

### 2. 执行自动化全量测试与混沌注入测试
执行测试套件会自动编译并依次运行所有 5 个场景，并在示例三中自动注入 `kill -9` 强杀读者进程以检验写者进程的崩溃免疫能力：

```bash
make test
# 或直接运行：
./run_tests.sh
```

---

## 五大完整工程实战示例说明

| 编号 | 源码文件 | 核心工程场景 | 关键验证指标 |
| :--- | :--- | :--- | :--- |
| **示例一** | `src/01_simplest_seqlock.c` | 最简单写单读共享内存通信 | 验证 `checksum == x + y` 数学不变式，证明绝无数据撕裂（Torn Read） |
| **示例二** | `src/02_multi_reader_stats.c` | 单写者 + 4 读者高频网络统计 | 验证多读者并发纯读无写、缓存行保持 S 态，实现近 3 亿次/秒高吞吐 |
| **示例三** | `src/03_posix_shm_writer.c`<br>`src/03_posix_shm_reader.c` | POSIX 共享内存跨进程 IPC (`shm_open` + `mmap`) | 读者采用纯 `PROT_READ` 映射；验证读者进程遭 `kill -9` 强杀时写者完全不卡死 |
| **示例四** | `src/04_dpdk_telemetry.c` | DPDK 风格 64 字节缓存行对齐遥测 | 快路径（Fast-Path）微秒级高频更新，慢路径（Slow-Path CLI）稳定解耦读取 |
| **示例五** | `src/05_kernel_seqcount.c` | Linux 内核 `seqcount_t` 与组合锁模拟 | 4 写者（自旋锁串行化）+ 4 读者（无锁一致性校验），验证完整多写多读安全 |

---

## 核心理论白皮书索引 (`seqlock_guide.md`)

详细理论推导、内存模型证明、硬件微架构与全景对比请阅读 [seqlock_guide.md](seqlock_guide.md)：

- **[第一部分：为什么需要 Seqlock](seqlock_guide.md#第一部分为什么需要-seqlock)**：多字段撕裂快照、原子变量局限、Mutex 与 RWLock 在高频统计下的瓶颈；
- **[第二部分：Seqlock 的整体架构](seqlock_guide.md#第二部分seqlock-的整体架构)**：序列计数器与数据载荷的职责划分；
- **[第三部分：Sequence Counter 深度剖析](seqlock_guide.md#第三部分sequence-counter-深度剖析)**：奇偶语义学、`if (seq & 1)` 冲突侦测与代际溢出回绕安全性；
- **[第四部分：GCC Atomic API 与 C11 内存模型](seqlock_guide.md#第四部分gcc-atomic-builtins-与-c11-内存模型)**：Acquire / Release / Fence 与 Happens-Before 关系的建立；
- **[第十部分：Memory Ordering 深入理解](seqlock_guide.md#第十部分memory-ordering-深入理解)**：为什么不能弱化为 Relaxed、编译器与 CPU 乱序执行机制；
- **[第十一部分：Memory Fence 与硬件微架构](seqlock_guide.md#第十一部分memory-fence-与硬件微架构)**：Store Buffer、Load Queue、TSO 与弱内存模型的底层映射；
- **[第十三部分：为什么写者必须串行化](seqlock_guide.md#第十三部分为什么写者必须串行化)**：Seqlock $\neq$ Writer Lock-Free 的本质推演；
- **[第十四部分：工程最佳实践与安全红线](seqlock_guide.md#第十四部分工程最佳实践与安全红线)**：为什么严禁保护堆指针与动态拓扑，RCU / Hazard Pointer 的边界；
- **[第十五部分：并发同步机制横向大比拼](seqlock_guide.md#第十五部分seqlock-与其它并发同步机制全景大比拼)**：Atomic、Mutex、Spinlock、RWLock、Seqlock、RCU 六维对比表；
- **[第十六部分：五层递进心智模型](seqlock_guide.md#第十六部分建立完整并发同步心智模型)**：从单变量原子到内存生命周期管理的架构金字塔；
- **[附录：Seqlock 学习速查表](seqlock_guide.md#附录seqlock-学习速查表cheat-sheet)**：生产环境模板与避坑指南。
