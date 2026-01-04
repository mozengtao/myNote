# pthread 深度指南 | A Deep Dive into POSIX Threads

```
+------------------------------------------------------------------+
|                                                                  |
|     ██████╗ ████████╗██╗  ██╗██████╗ ███████╗ █████╗ ██████╗     |
|     ██╔══██╗╚══██╔══╝██║  ██║██╔══██╗██╔════╝██╔══██╗██╔══██╗    |
|     ██████╔╝   ██║   ███████║██████╔╝█████╗  ███████║██║  ██║    |
|     ██╔═══╝    ██║   ██╔══██║██╔══██╗██╔══╝  ██╔══██║██║  ██║    |
|     ██║        ██║   ██║  ██║██║  ██║███████╗██║  ██║██████╔╝    |
|     ╚═╝        ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═════╝     |
|                                                                  |
|              A Practical Guide for Systems Programmers           |
|                                                                  |
+------------------------------------------------------------------+
```

## 目录 | Table of Contents

| Section | File | Focus |
|---------|------|-------|
| **WHY** | [01-WHY-pthreads.md](01-WHY-pthreads.md) | 为什么需要 pthreads，何时使用 |
| **HOW** | [02-HOW-pthreads.md](02-HOW-pthreads.md) | 核心设计模型，执行上下文 |
| **WHAT (Threads)** | [03-WHAT-threads.md](03-WHAT-threads.md) | 线程生命周期，join/detach |
| **WHAT (Sync)** | [03-WHAT-sync.md](03-WHAT-sync.md) | 互斥锁，条件变量 |
| **WHAT (Advanced)** | [03-WHAT-advanced.md](03-WHAT-advanced.md) | 读写锁，屏障，内存可见性 |
| **WHERE** | [04-WHERE-pthreads.md](04-WHERE-pthreads.md) | 架构模式，代码组织 |
| **PITFALLS** | [05-PITFALLS-pthreads.md](05-PITFALLS-pthreads.md) | 常见 bug，调试技巧 |
| **TRANSFER** | [06-TRANSFER-pthreads.md](06-TRANSFER-pthreads.md) | 决策框架，可迁移概念 |

---

## 阅读路径 | Reading Path

```
+------------------------------------------------------------------+
|                    RECOMMENDED READING ORDER                     |
+------------------------------------------------------------------+
|                                                                  |
|   BEGINNER: New to pthreads                                      |
|   +------------------------------------------------------------+ |
|   |   01-WHY --> 03-WHAT-threads --> 03-WHAT-sync --> 05-PITFALLS |
|   +------------------------------------------------------------+ |
|                                                                  |
|   INTERMEDIATE: Know basics, need architecture                   |
|   +------------------------------------------------------------+ |
|   |   02-HOW --> 04-WHERE --> 03-WHAT-advanced --> 06-TRANSFER   |
|   +------------------------------------------------------------+ |
|                                                                  |
|   DEBUGGING: Fixing a threading bug                              |
|   +------------------------------------------------------------+ |
|   |   05-PITFALLS --> 03-WHAT-sync --> 04-WHERE (code analysis)  |
|   +------------------------------------------------------------+ |
|                                                                  |
|   DESIGN: Designing a new system                                 |
|   +------------------------------------------------------------+ |
|   |   01-WHY --> 06-TRANSFER --> 04-WHERE --> 02-HOW             |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

---

## memcached 代码示例索引 | Code Examples from memcached

本指南大量引用 memcached 源代码，特别是 `crawler.c`。以下是关键代码位置：

| Pattern | File | Lines | Description |
|---------|------|-------|-------------|
| Lock Dance | crawler.c | 734-765 | 线程启动协调模式 |
| Thread Loop | crawler.c | 593-713 | 工作线程主循环 |
| Graceful Shutdown | crawler.c | 717-732 | 优雅关闭线程 |
| Static Mutex Init | crawler.c | 116-117 | 静态互斥锁初始化 |
| Condvar Wait | crawler.c | 603 | 条件变量等待模式 |
| Fine-grained Locking | crawler.c | 631, 774 | 细粒度锁 (lru_locks) |

---

## 核心原则 | Core Principles

```
+------------------------------------------------------------------+
|                    THE FIVE COMMANDMENTS                         |
+------------------------------------------------------------------+
|                                                                  |
|   1. MUTEX PROTECTS DATA, NOT CODE                               |
|      互斥锁保护数据不变量，不是代码块                              |
|                                                                  |
|   2. ALWAYS USE while() WITH CONDITION VARIABLES                 |
|      条件变量必须配合 while 循环使用                               |
|                                                                  |
|   3. EVERY LOCK MUST HAVE A DOCUMENTED ORDERING                  |
|      每个锁必须有文档化的获取顺序                                  |
|                                                                  |
|   4. NEVER PASS STACK POINTERS TO OTHER THREADS                  |
|      永远不要把栈指针传给其他线程                                  |
|                                                                  |
|   5. SIGNAL, DON'T CANCEL                                        |
|      用信号协调关闭，不要用 pthread_cancel                         |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 快速参考 | Quick Reference

### 常用 API 速查

```c
// Thread lifecycle
pthread_create(&tid, NULL, func, arg);  // 创建线程
pthread_join(tid, &retval);             // 等待线程结束
pthread_detach(tid);                    // 分离线程

// Mutex
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock(&mtx);
pthread_mutex_unlock(&mtx);
pthread_mutex_trylock(&mtx);  // Returns 0 if acquired

// Condition variable
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_wait(&cond, &mtx);     // Must hold mtx!
pthread_cond_signal(&cond);         // Wake one
pthread_cond_broadcast(&cond);      // Wake all
```

### 常用编译选项

```bash
# Thread Sanitizer
gcc -fsanitize=thread -g -O1 program.c -lpthread

# Debug symbols for GDB
gcc -g -O0 program.c -lpthread

# Production
gcc -O2 -DNDEBUG program.c -lpthread
```

---

## 参考资料 | References

- **POSIX Threads Programming**: https://computing.llnl.gov/tutorials/pthreads/
- **The Linux Programming Interface** by Michael Kerrisk (Chapters 29-33)
- **memcached source**: https://github.com/memcached/memcached
- **glibc NPTL implementation**: glibc/nptl/

---

*Created as a companion guide while studying memcached's threading model.*
