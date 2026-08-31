# Shared Memory 设计模式 —— 可运行 C 示例集

本目录是 [../share_memory_design.md](../share_memory_design.md) 的配套代码：针对文档第 2~13 节描述的每一种共享内存设计模式，提供一份**完整、可编译、可运行**的多进程 C 示例（基于 POSIX `shm_open`/`mmap`，而非线程内存共享），帮助建立"共享内存常用典型设计"的心智模型。

读完并跑通全部示例后，请阅读最终的心智模型总结：[SUMMARY.md](SUMMARY.md)。

## 环境要求

- Linux，gcc（已在 gcc 11.5.0 下验证）
- POSIX 共享内存 / 实时库：`librt`（现代 glibc 已并入 libc，Makefile 仍显式链接以保证可移植）
- 部分示例可选依赖（缺失会自动降级并打印提示，不影响编译）：
  - `libnuma`（11_numa_aware）

## 目录一览

| 目录 | 对应文档章节 | 模式 |
|------|------|------|
| [01_ring_buffer_spsc](01_ring_buffer_spsc/) | 2. Ring Buffer | 单生产者单消费者环形缓冲区 |
| [02_lockfree_queue_mpmc](02_lockfree_queue_mpmc/) | 3. Lock-Free Queue | CAS 实现的多生产者多消费者无锁队列 |
| [03_object_pool](03_object_pool/) | 4. Shared Object Pool | 无锁 Free List 对象池 |
| [04_hash_table](04_hash_table/) | 5. Shared Hash Table | 每桶一把进程间共享锁的哈希表 |
| [05_pipeline](05_pipeline/) | 6. Producer-Consumer Pipeline | 三级 Ring Buffer 串联的流水线 |
| [06_shared_config](06_shared_config/) | 7. Shared Configuration | Write Once, Read Many + Version 热更新 |
| [07_snapshot](07_snapshot/) | 8. Snapshot | 原子 Index 切换的双快照 |
| [08_double_buffer](08_double_buffer/) | 9. Double Buffer | 生产者/消费者各占一块缓冲区的双缓冲 |
| [09_event_notification](09_event_notification/) | 10. Shared Memory + Event Notification | eventfd / POSIX 信号量 / futex 三种唤醒机制 |
| [10_hugepage](10_hugepage/) | 11. Shared Memory + HugePage | mmap + MADV_HUGEPAGE |
| [11_numa_aware](11_numa_aware/) | 12. NUMA-Aware Shared Memory | numa_alloc_onnode（自动降级） |
| [12_header_version_magic](12_header_version_magic/) | 13. Header + Version + Magic | Magic/Version/CRC 校验器 |

## 构建与运行

```bash
# 编译全部示例
make -C /home/morrism/myNote/c/shm_examples

# 清理全部产物 + 残留的 /dev/shm 对象
make -C /home/morrism/myNote/c/shm_examples clean
```

每个子目录也可以单独 `make`，并附带自己的 `README.md` 说明具体运行命令、参数与预期输出。所有示例结束时都会调用 `shm_unlink()` 清理共享内存对象，不会在 `/dev/shm` 下残留文件。

## 通用约定

- 所有共享内存对象命名为 `/shm_examples_<pattern>`，避免与系统其他共享内存冲突。
- 所有示例遵循同一套错误处理方式（见 [common/shm_common.h](common/shm_common.h)）：任何系统调用失败立即打印文件名、行号与 `errno` 并退出。
- 代码风格：返回类型独立成行，K&R 风格花括号，与仓库其它 C 代码保持一致。
