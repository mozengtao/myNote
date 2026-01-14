# Section 9: Memory Management in FreeRTOS

## 9.1 Why FreeRTOS Does Not Assume malloc

```
THE EMBEDDED MEMORY PROBLEM:
+==================================================================+
||                                                                ||
||  Standard malloc/free have problems for embedded:              ||
||                                                                ||
||  1. NON-DETERMINISTIC timing                                   ||
||  +----------------------------------------------------------+  ||
||  | malloc may take 10us or 10ms depending on fragmentation  |  ||
||  | Real-time systems need bounded execution time             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  2. FRAGMENTATION                                              ||
||  +----------------------------------------------------------+  ||
||  | Long-running systems: memory becomes Swiss cheese        |  ||
||  | Eventually malloc fails even with "enough" free memory   |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  3. NOT THREAD-SAFE                                            ||
||  +----------------------------------------------------------+  ||
||  | Standard library malloc is often not thread-safe         |  ||
||  | Needs wrapping with mutexes                               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  4. NOT ALWAYS AVAILABLE                                       ||
||  +----------------------------------------------------------+  ||
||  | Some embedded toolchains have no heap support            |  ||
||  | Some systems deliberately exclude dynamic allocation      |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

嵌入式内存问题：标准malloc/free对嵌入式有问题：

1. 非确定性时间：malloc可能花10us或10ms取决于碎片化，实时系统需要有界执行时间。

2. 碎片化：长期运行系统内存变成"瑞士奶酪"，即使有"足够"空闲内存malloc最终也会失败。

3. 非线程安全：标准库malloc通常非线程安全，需要用互斥锁包装。

4. 不总是可用：某些嵌入式工具链无堆支持，某些系统故意排除动态分配。

---

## 9.2 The Heap Implementations

```
FREERTOS PROVIDES FIVE HEAP IMPLEMENTATIONS:
+==================================================================+
||                                                                ||
||  portable/MemMang/                                             ||
||  +----------------------------------------------------------+  ||
||  | heap_1.c | Allocate only, never free                     |  ||
||  | heap_2.c | Best-fit, no coalescing                       |  ||
||  | heap_3.c | Wrapper around standard malloc                |  ||
||  | heap_4.c | Best-fit with coalescing (RECOMMENDED)        |  ||
||  | heap_5.c | heap_4 + multiple memory regions              |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  CHOOSE ONE:                                                   ||
||  You compile exactly ONE heap_x.c into your project            ||
||  Or provide your own pvPortMalloc/vPortFree                    ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

FreeRTOS提供五种堆实现：

- heap_1.c：只分配，从不释放
- heap_2.c：最佳适配，不合并
- heap_3.c：标准malloc包装器
- heap_4.c：最佳适配带合并（推荐）
- heap_5.c：heap_4 + 多内存区域

选择一个：只编译一个heap_x.c到项目中，或提供自己的pvPortMalloc/vPortFree。

### heap_1.c - Allocate Only

```
heap_1: SIMPLEST, NO FREE
+------------------------------------------------------------------+
|                                                                  |
|  Initial:                                                        |
|  +------------------------------------------------------------+  |
|  |                    Free (10KB)                             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  After allocations:                                              |
|  +--------+--------+--------+---------------------------------+  |
|  | TCB1   | Stack1 | TCB2   |           Free (8KB)            |  |
|  | 200B   | 1KB    | 200B   |                                 |  |
|  +--------+--------+--------+---------------------------------+  |
|           ^                                                      |
|           | Next allocation starts here                          |
|                                                                  |
|  PROS:                                                           |
|  - Deterministic O(1) allocation                                 |
|  - Zero fragmentation                                            |
|  - Very simple (~100 lines)                                      |
|  - Easy to analyze and verify                                    |
|                                                                  |
|  CONS:                                                           |
|  - Memory can never be reclaimed                                 |
|  - Only for systems that create everything at startup            |
|                                                                  |
|  USE WHEN:                                                       |
|  - Tasks and queues created once at startup                      |
|  - Never delete tasks or queues                                  |
|  - Certification requirements (simplest to analyze)              |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

heap_1：最简单，不能释放。分配是O(1)确定性、零碎片、非常简单（约100行）、易于分析验证。缺点是内存永远无法回收，只适用于启动时创建所有东西的系统。使用场景：任务和队列启动时创建一次、从不删除、有认证要求。

### heap_4.c - Best Fit with Coalescing (Recommended)

```
heap_4: GENERAL PURPOSE, RECOMMENDED
+==================================================================+
||                                                                ||
||  FREE LIST STRUCTURE:                                          ||
||                                                                ||
||  xStart -> [Free Block A] -> [Free Block B] -> pxEnd           ||
||            size: 1KB          size: 3KB                        ||
||                                                                ||
||  ALLOCATION (pvPortMalloc):                                    ||
||  +----------------------------------------------------------+  ||
||  | 1. Walk free list looking for best fit                   |  ||
||  | 2. If block too large, split it                          |  ||
||  | 3. Mark block as allocated                               |  ||
||  | 4. Return pointer to user area                           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  FREE (vPortFree):                                             ||
||  +----------------------------------------------------------+  ||
||  | 1. Mark block as free                                    |  ||
||  | 2. Insert into free list (sorted by address)             |  ||
||  | 3. COALESCE: merge with adjacent free blocks             |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  COALESCING EXAMPLE:                                           ||
||                                                                ||
||  Before free(B):                                               ||
||  [A:free][B:used][C:free]                                      ||
||                                                                ||
||  After free(B):                                                ||
||  [  A + B + C : free    ]  <- One big block!                   ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

heap_4：通用，推荐使用。

空闲列表结构：xStart -> [空闲块A] -> [空闲块B] -> pxEnd。

分配（pvPortMalloc）：遍历空闲列表找最佳适配 -> 如果块太大则分割 -> 标记块为已分配 -> 返回用户区指针。

释放（vPortFree）：标记块为空闲 -> 插入空闲列表（按地址排序） -> 合并：与相邻空闲块合并。

合并示例：free(B)前：[A:空闲][B:使用][C:空闲]。free(B)后：[A+B+C:空闲]——一个大块！

### heap_5.c - Multiple Memory Regions

```
heap_5: FOR NON-CONTIGUOUS MEMORY
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO: Memory map with gaps                                  |
|                                                                  |
|  0x20000000 +------------------+                                 |
|             | Internal SRAM    |  32KB                           |
|  0x20008000 +------------------+                                 |
|             | (gap - periph)   |                                 |
|  0x60000000 +------------------+                                 |
|             | External SRAM    |  512KB                          |
|  0x60080000 +------------------+                                 |
|                                                                  |
|  heap_5 manages both regions as one logical heap:                |
|                                                                  |
|  HeapRegion_t xHeapRegions[] = {                                 |
|      { (uint8_t *)0x20000000, 0x8000 },   /* 32KB internal */    |
|      { (uint8_t *)0x60000000, 0x80000 },  /* 512KB external */   |
|      { NULL, 0 }  /* Terminator */                               |
|  };                                                              |
|  vPortDefineHeapRegions(xHeapRegions);                           |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

heap_5：用于非连续内存。

场景：有间隙的内存映射。内部SRAM 32KB在0x20000000，外部SRAM 512KB在0x60000000。

heap_5将两个区域作为一个逻辑堆管理。配置HeapRegion_t数组定义各区域，然后调用vPortDefineHeapRegions()。

### Comparison Table

| Feature | heap_1 | heap_2 | heap_3 | heap_4 | heap_5 |
|---------|--------|--------|--------|--------|--------|
| Free memory | No | Yes | Yes | Yes | Yes |
| Coalesce | N/A | No | Depends | Yes | Yes |
| Thread-safe | Yes | Yes | Depends | Yes | Yes |
| Deterministic | Yes | Yes | No | ~Yes | ~Yes |
| Multiple regions | No | No | No | No | Yes |
| Best for | Static | Simple | Porting | General | Complex |

---

## 9.3 Static vs Dynamic Allocation

```
DYNAMIC ALLOCATION (default):
+==================================================================+
||                                                                ||
||  xTaskCreate() internally calls pvPortMalloc for:              ||
||  - TCB structure                                               ||
||  - Task stack                                                  ||
||                                                                ||
||  xQueueCreate() internally calls pvPortMalloc for:             ||
||  - Queue structure                                             ||
||  - Queue storage                                               ||
||                                                                ||
||  PROS:                                                         ||
||  - Simpler API                                                 ||
||  - Sizes determined at runtime                                 ||
||                                                                ||
||  CONS:                                                         ||
||  - Heap needed                                                 ||
||  - Fragmentation possible                                      ||
||  - Allocation can fail                                         ||
||                                                                ||
+==================================================================+

STATIC ALLOCATION (configSUPPORT_STATIC_ALLOCATION = 1):
+==================================================================+
||                                                                ||
||  YOU provide the memory:                                       ||
||                                                                ||
||  static StaticTask_t xTaskBuffer;                              ||
||  static StackType_t xStack[256];                               ||
||                                                                ||
||  xTaskCreateStatic(TaskFunc, "Name", 256, NULL, 1,             ||
||                    xStack, &xTaskBuffer);                      ||
||                                                                ||
||  PROS:                                                         ||
||  - No heap needed (can use heap_1 or no heap)                  ||
||  - Memory always available (compile-time allocation)           ||
||  - Easier to analyze memory usage                              ||
||  - Required for some safety certifications                     ||
||                                                                ||
||  CONS:                                                         ||
||  - More verbose API                                            ||
||  - Must calculate sizes at compile time                        ||
||  - Memory always allocated (even if not used)                  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

动态分配（默认）：xTaskCreate内部调用pvPortMalloc分配TCB和栈，xQueueCreate分配队列结构和存储。优点：更简单的API、运行时确定大小。缺点：需要堆、可能碎片化、分配可能失败。

静态分配（configSUPPORT_STATIC_ALLOCATION = 1）：你提供内存。声明静态缓冲区，调用xTaskCreateStatic。优点：不需要堆、内存总是可用（编译时分配）、更容易分析内存使用、某些安全认证要求。缺点：更冗长的API、必须编译时计算大小、内存总是分配（即使未使用）。

### Why Static Allocation Matters for Safety

```
SAFETY-CRITICAL SYSTEMS:
+------------------------------------------------------------------+
|                                                                  |
|  Standards like DO-178C (aviation), ISO 26262 (automotive):      |
|                                                                  |
|  1. DETERMINISTIC resource usage                                 |
|     - Must know EXACTLY how much memory is needed                |
|     - No "maybe we run out" scenarios                            |
|                                                                  |
|  2. NO DYNAMIC ALLOCATION after startup                          |
|     - All objects created during initialization                  |
|     - No malloc/free during operation                            |
|                                                                  |
|  3. ANALYZABLE                                                   |
|     - Static analysis tools can verify memory bounds             |
|     - Worst-case stack usage calculable                          |
|                                                                  |
|  STATIC ALLOCATION ENABLES:                                      |
|  - Link-time memory verification                                 |
|  - No runtime allocation failures                                |
|  - Simpler certification arguments                               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

安全关键系统：DO-178C（航空）、ISO 26262（汽车）等标准要求：

1. 确定性资源使用：必须确切知道需要多少内存，无"可能耗尽"场景。
2. 启动后无动态分配：所有对象在初始化期间创建，操作期间无malloc/free。
3. 可分析：静态分析工具可验证内存边界，最坏情况栈使用可计算。

静态分配使得：链接时内存验证、无运行时分配失败、更简单的认证论证。

---

## 9.4 Stack Management

```
TASK STACK ALLOCATION:
+==================================================================+
||                                                                ||
||  DYNAMIC:                                                      ||
||  xTaskCreate(..., stackDepth, ...)                             ||
||  -> pvPortMalloc(stackDepth * sizeof(StackType_t))             ||
||                                                                ||
||  STATIC:                                                       ||
||  StackType_t myStack[STACK_SIZE];                              ||
||  xTaskCreateStatic(..., STACK_SIZE, ..., myStack, ...)         ||
||                                                                ||
||  STACK USAGE DURING EXECUTION:                                 ||
||                                                                ||
||  High addr  +---------------------------+                      ||
||             | (unused space)            |                      ||
||             |                           |                      ||
||             +---------------------------+                      ||
||             | Saved context (R4-R11)    | <- After ctx switch  ||
||             +---------------------------+                      ||
||             | Exception frame           |                      ||
||             +---------------------------+                      ||
||             | Local variables           |                      ||
||             | Function call frames      |                      ||
||  SP -->     +---------------------------+                      ||
||             | (remaining free)          |                      ||
||  Low addr   +---------------------------+ <- Stack base        ||
||                                                                ||
||  OVERFLOW HAPPENS HERE!                                        ||
||  If SP goes below stack base, you corrupt memory.              ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

任务栈分配：

动态：xTaskCreate(..., stackDepth, ...) -> pvPortMalloc(stackDepth * sizeof(StackType_t))。

静态：声明StackType_t myStack[STACK_SIZE]，调用xTaskCreateStatic。

执行期间栈使用：从高地址到低地址依次是：未使用空间、保存的上下文、异常帧、局部变量、函数调用帧、SP指向当前位置、剩余空闲空间、栈基。

溢出在这里发生！如果SP低于栈基，就会损坏内存。

### Stack Overflow Detection

```
OVERFLOW DETECTION METHODS:
+------------------------------------------------------------------+
|                                                                  |
|  configCHECK_FOR_STACK_OVERFLOW = 1 (Method 1):                  |
|  +------------------------------------------------------------+  |
|  | Check if SP went past stack base after context switch     |  |
|  | Fast but may miss overflow that recovered                  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  configCHECK_FOR_STACK_OVERFLOW = 2 (Method 2):                  |
|  +------------------------------------------------------------+  |
|  | Fill stack with known pattern (0xA5) at creation          |  |
|  | Check if pattern corrupted at bottom of stack             |  |
|  | Catches more overflows but slightly slower                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  Stack pattern check:                                            |
|  +------------------------------------------------------------+  |
|  | Stack bottom: [0xA5][0xA5][0xA5][0xA5]...                 |  |
|  | After overflow: [0x12][0x34][0xA5][0xA5]... <- Corrupted! |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CALLBACK:                                                       |
|  void vApplicationStackOverflowHook(TaskHandle_t xTask,          |
|                                     char *pcTaskName)            |
|  {                                                               |
|      /* Log error, halt system, trigger watchdog reset */        |
|      for(;;);                                                    |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

溢出检测方法：

方法1（configCHECK_FOR_STACK_OVERFLOW = 1）：上下文切换后检查SP是否越过栈基。快速但可能遗漏恢复的溢出。

方法2（configCHECK_FOR_STACK_OVERFLOW = 2）：创建时用已知模式(0xA5)填充栈，检查栈底模式是否被破坏。捕获更多溢出但略慢。

回调函数vApplicationStackOverflowHook在检测到溢出时被调用。

---

## Summary

```
MEMORY MANAGEMENT KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  WHY NOT STANDARD MALLOC:                                      ||
||  - Non-deterministic timing                                    ||
||  - Fragmentation                                               ||
||  - Not always thread-safe                                      ||
||  - Not always available                                        ||
||                                                                ||
||  HEAP SELECTION:                                               ||
||  - heap_1: Never free, simplest, for static systems            ||
||  - heap_4: General purpose, coalescing (recommended)           ||
||  - heap_5: Multiple memory regions                             ||
||                                                                ||
||  STATIC VS DYNAMIC:                                            ||
||  - Dynamic: Simpler API, but needs heap                        ||
||  - Static: No heap needed, required for safety-critical        ||
||                                                                ||
||  STACK MANAGEMENT:                                             ||
||  - Size carefully (no auto-grow)                               ||
||  - Enable overflow detection during development                ||
||  - Use stack usage analysis tools                              ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Configuration Philosophy](10-configuration-philosophy.md)
