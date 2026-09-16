# GCC / Clang `__atomic_*` 原子内建函数总结

> 核心目标：不要把 `__atomic_*` 当成一堆需要背诵的 API。
>
> 应该建立这样的 Mental Model：
>
> **Atomic Operation = 对共享对象做什么 + 如何与其他内存操作建立顺序关系**
>
> ```text
>                  __atomic_*
>                       │
>             ┌─────────┴─────────┐
>             │                   │
>        What operation?       Ordering?
>             │                   │
>       ┌─────┼─────┐       memory_order
>       │     │     │
>     Load  Store   RMW
>                   │
>              ┌────┼────┐
>              │    │    │
>            Add   CAS  Exchange
> ```

---

## 1. 为什么需要 `__atomic_*`

普通 C 代码：

```c
counter++;
```

看起来是一条语句，但实际上通常包含：

```text
        counter++
           │
      ┌────┼────┐
      │    │    │
     Load Add  Store
```

多个线程同时执行时可能发生：

```text
Thread A              Thread B

read counter = 0
                      read counter = 0

write counter = 1
                      write counter = 1

最终 counter = 1
```

发生了 Lost Update。

而：

```c
__atomic_fetch_add(&counter, 1, __ATOMIC_RELAXED);
```

把：

```text
READ
  +
MODIFY
  +
WRITE
```

作为一个原子 RMW（Read-Modify-Write）操作。

## 2. 整体分类

GCC/Clang `__atomic_*` 可以按照操作类型分成：

```text
                         __atomic_*
                              │
        ┌─────────────────────┼──────────────────────┐
        │                     │                      │
      Basic                  RMW                Synchronization
        │                     │                      │
   ┌────┼────┐        ┌───────┼────────┐       ┌─────┴─────┐
   │    │    │        │       │        │       │           │
 Load Store Exchange  Add     CAS     BitOps  Thread    Signal
                                             Fence      Fence
```

最重要的 API：

| 类别 | API | 核心作用 |
| --- | --- | --- |
| Load | `__atomic_load_n` | 原子读取 |
| Store | `__atomic_store_n` | 原子写入 |
| Exchange | `__atomic_exchange_n` | 原子交换 |
| CAS | `__atomic_compare_exchange_n` | 条件式原子修改 |
| Add | `__atomic_fetch_add` | 原子加，返回旧值 |
| Sub | `__atomic_fetch_sub` | 原子减，返回旧值 |
| And | `__atomic_fetch_and` | 原子 AND |
| Or | `__atomic_fetch_or` | 原子 OR |
| Xor | `__atomic_fetch_xor` | 原子 XOR |
| Nand | `__atomic_fetch_nand` | 原子 NAND |
| Add Fetch | `__atomic_add_fetch` | 原子加，返回新值 |
| Sub Fetch | `__atomic_sub_fetch` | 原子减，返回新值 |
| And Fetch | `__atomic_and_fetch` | 原子 AND，返回新值 |
| Or Fetch | `__atomic_or_fetch` | 原子 OR，返回新值 |
| Xor Fetch | `__atomic_xor_fetch` | 原子 XOR，返回新值 |
| Nand Fetch | `__atomic_nand_fetch` | 原子 NAND，返回新值 |
| Fence | `__atomic_thread_fence` | 建立线程间内存排序 |
| Signal Fence | `__atomic_signal_fence` | 主要约束编译器重排 |
| Lock-Free | `__atomic_is_lock_free` | 运行时检查是否 lock-free |
| Lock-Free | `__atomic_always_lock_free` | 编译期判断是否始终 lock-free |

## 3. 第一类：Atomic Load

### API

```c
__atomic_load_n(ptr, memorder);
```

例如：

```c
int value;

int x = __atomic_load_n(
    &value,
    __ATOMIC_ACQUIRE
);
```

### 做什么？

读取一个共享对象：

```text
shared variable
      │
      ▼
  atomic load
      │
      ▼
    value
```

与普通：

```c
x = value;
```

不同的是，它明确告诉编译器：

这是一个原子访问。

### Mental Model

```text
Load
 │
 └── “我想原子地看一眼这个共享状态”
```

典型用途：

- atomic flag
- state
- reference count
- sequence number
- lock state

## 4. 第二类：Atomic Store

### API

```c
__atomic_store_n(ptr, value, memorder);
```

例如：

```c
__atomic_store_n(
    &ready,
    1,
    __ATOMIC_RELEASE
);
```

### 做什么？

原子地写入：

```text
value
  │
  ▼
atomic store
  │
  ▼
shared variable
```

### Mental Model

```text
Store
 │
 └── “我原子地把一个状态发布出去”
```

特别典型的是：

```c
data = 42;

__atomic_store_n(
    &ready,
    1,
    __ATOMIC_RELEASE
);
```

这里 `ready` 不只是一个变量。

它承担：

```text
              Producer
                  │
             prepare data
                  │
                  ▼
          RELEASE store
                  │
                  │
             publish
                  │
                  ▼
              Consumer
```

## 5. 第三类：Atomic Exchange

### API

```c
__atomic_exchange_n(ptr, value, memorder);
```

例如：

```c
int old;

old = __atomic_exchange_n(
    &state,
    RUNNING,
    __ATOMIC_ACQ_REL
);
```

如果：

```text
state = IDLE
```

执行后：

```text
old   = IDLE
state = RUNNING
```

### Mental Model

```text
          old value
              │
              ▼
        ┌───────────┐
        │  Exchange │
        └───────────┘
              │
       ┌──────┴──────┐
       ▼             ▼
   return old     write new
```

可以把它理解成：

“把旧状态拿出来，同时把状态换成新状态。”

典型场景：

- state transition
- ownership transfer
- lock acquisition
- flag claiming

## 6. 第四类：Compare-And-Exchange / CAS

这是 `__atomic_*` 中最重要的操作之一。

### API

```c
__atomic_compare_exchange_n(
    ptr,
    expected,
    desired,
    weak,
    success_memorder,
    failure_memorder
);
```

例如：

```c
int expected = IDLE;

bool ok =
    __atomic_compare_exchange_n(
        &state,
        &expected,
        RUNNING,
        false,
        __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE
    );
```

## 7. CAS 到底做什么？

逻辑上：

```c
if (*ptr == *expected) {
    *ptr = desired;
    return true;
} else {
    *expected = *ptr;
    return false;
}
```

但：

```text
Compare
+
Modify
```

整体是原子的。

### Mental Model

```text
             current value
                    │
                    ▼
             compare with
               expected
                    │
             ┌──────┴──────┐
             │             │
           equal        not equal
             │             │
             ▼             ▼
       ptr = desired   expected = current
             │             │
             ▼             ▼
          success         failure
```

CAS 的核心思想：

“只有当共享状态还是我预期的状态时，我才允许修改它。”

## 8. CAS = Lock-Free Algorithm 的核心

大量 lock-free 算法可以抽象成：

```c
while (true) {

    old = atomic_load();

    new = calculate(old);

    if (CAS(old, new))
        break;
}
```

Mental Model：

```text
             Read current
                  │
                  ▼
             Calculate
                  │
                  ▼
                 CAS
              ┌───┴───┐
              │       │
           success   failure
              │       │
              ▼       │
             done ◄───┘
```

因此 CAS 常用于：

- lock-free stack
- lock-free queue
- reference counting
- state machine
- ownership transfer

## 9. 第五类：Atomic Fetch RMW

这一组 API：

```c
__atomic_fetch_add()
__atomic_fetch_sub()

__atomic_fetch_and()
__atomic_fetch_or()
__atomic_fetch_xor()

__atomic_fetch_nand()
```

统一理解成：

Atomic Read-Modify-Write

即：

```text
       ┌──────────────┐
       │ Atomic RMW   │
       └──────┬───────┘
              │
       ┌──────┼──────┐
       ▼      ▼      ▼
      READ   MODIFY  WRITE
```

## 10. fetch_add

```c
int old = __atomic_fetch_add(
    &counter,
    1,
    __ATOMIC_RELAXED
);
```

逻辑：

```c
old = counter;
counter = counter + 1;
return old;
```

例如：

```text
counter = 10

fetch_add(..., 5)

old     = 10
counter = 15
```

## 11. add_fetch

```c
int new = __atomic_add_fetch(
    &counter,
    5,
    __ATOMIC_RELAXED
);
```

逻辑：

```c
counter = counter + 5;
return counter;
```

例如：

```text
counter = 10

add_fetch(..., 5)

counter = 15
new     = 15
```

## 12. Fetch 与 Fetch-Result 的核心区别

只需要记：

```text
fetch_add
    │
    └── return OLD


add_fetch
    │
    └── return NEW
```

同样适用于：

```text
fetch_sub      vs sub_fetch
fetch_and      vs and_fetch
fetch_or       vs or_fetch
fetch_xor      vs xor_fetch
fetch_nand     vs nand_fetch
```

## 13. Bitwise RMW

### AND

```c
__atomic_fetch_and(
    &flags,
    mask,
    order
);
```

Mental Model：

```c
flags = flags & mask
```

常用于：

- bit flags
- feature flags
- status bits

### OR

```c
__atomic_fetch_or(
    &flags,
    mask,
    order
);
```

Mental Model：

```c
flags = flags | mask
```

常用于：

- set flag
- enable feature
- mark state

### XOR

```c
__atomic_fetch_xor(
    &flags,
    mask,
    order
);
```

Mental Model：

```c
flags = flags ^ mask
```

常用于：

- toggle bit

## 14. 第六类：Generic `__atomic_*`

除了：

```c
__atomic_load_n()
__atomic_store_n()
__atomic_exchange_n()
__atomic_compare_exchange_n()
```

还有 generic 版本：

```c
__atomic_load()
__atomic_store()
__atomic_exchange()
__atomic_compare_exchange()
```

Mental Model：

```text
_n
 │
 └── 简洁的标量类型 API


generic
 │
 └── 更通用的对象/任意大小数据 API
```

例如：

```c
int result;

__atomic_load(
    &object,
    &result,
    __ATOMIC_ACQUIRE
);
```

实际使用时：

优先使用 `_n` 版本理解基本原子语义；需要处理更一般的对象时再考虑 generic API。

## 15. Memory Order：第二个维度

理解 `__atomic_*` 时必须把：

Operation

和：

Memory Order

分开。

例如：

```c
__atomic_load_n(
    &state,
    __ATOMIC_ACQUIRE
);
```

可以拆成：

```text
          Atomic Load
               │
               +
            Acquire
```

所以：

```text
__atomic_*
    │
    ├── What?
    │     ├── Load
    │     ├── Store
    │     ├── Exchange
    │     ├── CAS
    │     └── RMW
    │
    └── How ordered?
          ├── Relaxed
          ├── Acquire
          ├── Release
          ├── AcqRel
          └── SeqCst
```

## 16. `__ATOMIC_RELAXED`

```c
__ATOMIC_RELAXED
```

保证：

这个原子对象的访问是原子的。

但不建立普通内存访问之间的同步关系。

典型：

```c
__atomic_fetch_add(
    &counter,
    1,
    __ATOMIC_RELAXED
);
```

### Mental Model

```text
RELAXED

“只管这个 atomic variable 本身，
不要额外要求跨线程的内存排序。”
```

典型场景：

- statistics
- counters
- metrics
- 某些 reference count

## 17. `__ATOMIC_ACQUIRE`

```c
__ATOMIC_ACQUIRE
```

常用于：

- Load
- CAS success
- RMW

核心 Mental Model：

```text
Acquire
   │
   ▼
“我看到别人发布的状态后，
后面的操作可以安全地使用那个状态所发布的数据。”
```

经典：

```c
if (__atomic_load_n(
        &ready,
        __ATOMIC_ACQUIRE)) {

    use(data);
}
```

## 18. `__ATOMIC_RELEASE`

```c
__ATOMIC_RELEASE
```

常用于：

- Store
- CAS
- RMW

Mental Model：

```text
Release
   │
   ▼
“我把前面完成的工作发布出去。”
```

经典：

```c
data = 42;

__atomic_store_n(
    &ready,
    1,
    __ATOMIC_RELEASE
);
```

## 19. Acquire + Release

```c
__ATOMIC_ACQ_REL
```

表示：

```text
Acquire
   +
Release
```

主要用于：

- RMW
- CAS
- Exchange

Mental Model：

```text
          RMW
           │
      ┌────┴────┐
      ▼         ▼
   Acquire    Release
      │         │
      ▼         ▼
   observe    publish
```

## 20. Sequential Consistency

```c
__ATOMIC_SEQ_CST
```

它提供最强的常见内存排序保证。

Mental Model：

```text
SEQ_CST

多个线程的 seq_cst atomic operations
            │
            ▼
      一个一致的全局顺序
```

可以把它作为：

```text
最容易理解
    ↓
最强约束
    ↓
但可能限制优化
```

学习并发编程时，可以先使用：

```c
__ATOMIC_SEQ_CST
```

确保理解正确后，再研究：

```text
Acquire / Release
        ↓
Relaxed
```

## 21. Memory Order Cheat Sheet

| Order | Mental Model |
| --- | --- |
| RELAXED | 只保证 atomic 本身 |
| ACQUIRE | 获取别人发布的数据后再继续 |
| RELEASE | 把之前完成的数据/工作发布出去 |
| ACQ_REL | Acquire + Release |
| SEQ_CST | 所有 seq_cst atomic 有统一全局顺序 |

## 22. `__atomic_thread_fence`

API：

```c
__atomic_thread_fence(order);
```

它不是访问某个 atomic variable。

而是在当前线程建立：

Memory Ordering Boundary

Mental Model：

```text
ordinary operations
        │
        ▼
   ┌─────────┐
   │  Fence  │
   └────┬────┘
        │
        ▼
ordinary operations
```

重点：

```text
atomic operation
        ≠
memory fence
```

两者是不同层次的同步工具。

## 23. `__atomic_signal_fence`

API：

```c
__atomic_signal_fence(order);
```

Mental Model：

```text
thread_fence
    │
    ├── compiler ordering
    └── CPU/hardware ordering


signal_fence
    │
    └── primarily compiler ordering
```

它主要用于约束编译器优化，尤其涉及：

- signal handler
- compiler-visible ordering

普通多线程同步中，更重要的是先理解：

```c
__atomic_thread_fence()
```

## 24. Lock-Free 查询

### `__atomic_is_lock_free`

```c
__atomic_is_lock_free(size, ptr);
```

作用：

当前平台/当前对象
是否能够以 lock-free 方式执行该 atomic operation？

### `__atomic_always_lock_free`

```c
__atomic_always_lock_free(size, ptr);
```

用于判断：

编译器是否能够保证
这个 size 在目标平台上始终 lock-free

Mental Model：

```text
             Atomic Operation
                    │
                    ▼
             ┌─────────────┐
             │ Hardware    │
             │ lock-free ? │
             └──────┬──────┘
                    │
              ┌─────┴─────┐
             YES          NO
              │            │
              ▼            ▼
        hardware      compiler/runtime
        primitive        fallback
```

## 25. 一个完整的 Publish / Subscribe 示例

Producer：

```c
data = 42;

__atomic_store_n(
    &ready,
    1,
    __ATOMIC_RELEASE
);
```

Consumer：

```c
if (__atomic_load_n(
        &ready,
        __ATOMIC_ACQUIRE)) {

    printf("%d\n", data);
}
```

整体关系：

```text
Producer                         Consumer

data = 42
   │
   ▼
RELEASE store
ready = 1
   │
   │
   │ synchronizes-with
   │
   ▼
ACQUIRE load
ready == 1
   │
   ▼
read data
   │
   ▼
42
```

这里最重要的不是：

`ready` 是 atomic

而是：

```text
Release
   │
   ▼
Acquire
   │
   ▼
happens-before
   │
   ▼
data 可见
```

## 26. 一个完整的 Counter 示例

```c
#include <stdio.h>
#include <pthread.h>

static int counter;

void *worker(void *arg)
{
    for (int i = 0; i < 100000; i++) {
        __atomic_fetch_add(
            &counter,
            1,
            __ATOMIC_RELAXED
        );
    }

    return NULL;
}
```

核心：

```text
多个线程
    │
    ├── fetch_add
    ├── fetch_add
    ├── fetch_add
    └── fetch_add
          │
          ▼
       counter
```

这里通常只关心：

`counter` 的更新不能丢

不需要通过 `counter` 发布其他普通数据，因此 RELAXED 可以表达这种较弱需求。

## 27. 一个完整的 CAS 状态机

```c
enum {
    IDLE,
    RUNNING,
    STOPPED
};

int state = IDLE;

bool start(void)
{
    int expected = IDLE;

    return __atomic_compare_exchange_n(
        &state,
        &expected,
        RUNNING,
        false,
        __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE
    );
}
```

Mental Model：

```text
                  state
                    │
                  IDLE
                    │
              CAS(IDLE,RUNNING)
                    │
             ┌──────┴──────┐
             │             │
           success       failure
             │             │
             ▼             ▼
          RUNNING       somebody
                        changed state
```

CAS 本质上就是：

“只有状态仍然符合我的预期，我才执行状态转换。”

## 28. `__atomic_*` 与 Seqlock 的关系

Seqlock：

```text
Writer

seq = even
   │
   ▼
atomic increment
   │
   ▼
seq = odd
   │
   ▼
modify data
   │
   ▼
atomic increment
   │
   ▼
seq = even
```

Reader：

```text
        load seq
           │
           ▼
        read data
           │
           ▼
       load seq again
           │
           ▼
       compare seq
        /        \
      same      changed
       │           │
       ▼           ▼
     valid        retry
```

这里：

```text
Seqlock
   │
   └── 是一个 synchronization algorithm


__atomic_*
   │
   └── 是它可以使用的 atomic primitives
```

所以：

```text
Algorithm
    │
    ▼
Seqlock protocol
    │
    ▼
__atomic_*
    │
    ▼
compiler
    │
    ▼
CPU atomic instructions
```

## 29. 最重要的关系：Atomicity vs Ordering

这是学习 `__atomic_*` 最容易混淆的地方。

### Atomicity

解决：

多个线程同时访问同一个对象时，这个操作会不会被拆开？

例如：

```c
__atomic_fetch_add(...)
```

解决：

```text
READ + MODIFY + WRITE
```

的原子性。

### Ordering

解决：

这个 atomic 操作与其他内存访问之间是什么顺序关系？

例如：

```c
__ATOMIC_ACQUIRE
__ATOMIC_RELEASE
```

解决：

```text
data
  │
  ▼
ready
```

之间如何建立可见性/顺序关系。

因此：

```text
        __atomic operation
                │
        ┌───────┴────────┐
        │                │
    Atomicity         Ordering
        │                │
        ▼                ▼
   “不可分割”       “怎么排序”
```

这是整个 `__atomic_*` 最核心的 Mental Model。

## 30. API → 问题映射

实际编程时不要背函数，而是问：

我现在遇到什么问题？

| 问题 | 首先考虑 |
| --- | --- |
| 我要原子读取？ | `__atomic_load_n` |
| 我要原子写入？ | `__atomic_store_n` |
| 我要交换状态？ | `__atomic_exchange_n` |
| 我要“如果还是旧状态才修改”？ | `__atomic_compare_exchange_n` |
| 我要原子加减？ | `__atomic_fetch_add/sub` |
| 我要原子操作 bit flags？ | `fetch_and/or/xor` |
| 我要同时获取新值？ | `add_fetch/sub_fetch/...` |
| 我要建立 memory ordering？ | `__atomic_thread_fence` |
| 我要检查 lock-free？ | `__atomic_is_lock_free` |
| 我要编译期确认 lock-free？ | `__atomic_always_lock_free` |

## 31. 最值得记住的 5 个 API

如果只掌握 80/20：

```c
__atomic_load_n()
__atomic_store_n()

__atomic_fetch_add()

__atomic_exchange_n()

__atomic_compare_exchange_n()
```

它们分别代表：

```text
Load
  ↓
“看”

Store
  ↓
“写”

Fetch Add
  ↓
“原子修改”

Exchange
  ↓
“交换状态”

CAS
  ↓
“条件修改”
```

## 32. 最重要的 5 个 Memory Order

```c
__ATOMIC_RELAXED
__ATOMIC_ACQUIRE
__ATOMIC_RELEASE
__ATOMIC_ACQ_REL
__ATOMIC_SEQ_CST
```

可以建立如下直觉：

```text
RELAXED
   │
   │ atomicity only
   ▼
ACQUIRE / RELEASE
   │
   │ synchronization
   ▼
ACQ_REL
   │
   │ both directions
   ▼
SEQ_CST
   │
   │ strongest common ordering
   ▼
```

## 33. 最终 Mental Model

不要记：

“`__atomic_fetch_add` 是一个 GCC 函数”

而应该记：

```text
                 __atomic_*
                      │
                      ▼
             Atomic Operation
                      │
          ┌───────────┼───────────┐
          │           │           │
         Read        Write        RMW
          │           │           │
          │           │      ┌────┼────┐
          │           │      │    │    │
         Load        Store   Add  CAS Exchange
                                  │
                                  ▼
                              Lock-Free
                              Algorithm
```

再叠加：

```text
Atomic Operation
       +
Memory Order
       │
       ▼
Synchronization Semantics
       │
       ▼
Happens-Before
       │
       ▼
Correct Concurrent Program
```

## 34. 一句话总结

GCC/Clang `__atomic_*` 的本质不是“原子变量 API”，而是一组构建并发算法的底层 primitive：

- Load / Store —— 原子地观察和发布状态
- Exchange —— 原子地替换状态
- CAS —— “如果状态仍符合预期，就修改它”
- Fetch/Add/... —— 原子 RMW
- Fence —— 建立内存排序边界
- Memory Order —— 定义这些操作如何与其他内存访问发生顺序关系

最终应该形成这个 Mental Model：

```text
                Concurrent Algorithm
                        │
                        ▼
                __atomic_* primitive
                        │
            ┌───────────┴───────────┐
            │                       │
         Atomicity               Ordering
            │                       │
      “操作不可分割”          “操作如何排序”
            │                       │
            └───────────┬───────────┘
                        ▼
                  Synchronization
                        │
                        ▼
                   Happens-Before
                        │
                        ▼
               Correct Concurrency
```

真正掌握 `__atomic_*` 的关键，是从“记 API”升级到“识别并发问题 → 选择 atomic primitive → 选择 memory order → 建立 happens-before 关系”。
