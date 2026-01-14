# Section 10: Memory Management

FreeRTOS does not mandate a specific memory allocator. Instead, it provides a portable interface and several implementations to choose from.

## 10.1 Heap Models

### Why FreeRTOS Does Not Force One Allocator

```
EMBEDDED MEMORY CONSTRAINTS VARY WIDELY:
+------------------------------------------------------------------+
|                                                                  |
|  Safety-Critical System:                                         |
|  - No dynamic allocation allowed after startup                   |
|  - All memory statically allocated                               |
|  - Need: Simple allocator that never frees (heap_1)              |
|                                                                  |
|  Simple IoT Sensor:                                              |
|  - Create tasks at startup, never delete                         |
|  - Fixed set of queues/semaphores                                |
|  - Need: No-fragment allocator (heap_1 or heap_2)                |
|                                                                  |
|  Complex Gateway:                                                |
|  - Dynamic task creation/deletion                                |
|  - Variable-size buffers                                         |
|  - Need: Full-featured allocator with coalescing (heap_4)        |
|                                                                  |
|  Multi-Region Memory:                                            |
|  - External RAM + Internal RAM                                   |
|  - Different regions for different purposes                      |
|  - Need: Multiple-region support (heap_5)                        |
|                                                                  |
+------------------------------------------------------------------+
```

### The Five Heap Implementations

```
HEAP IMPLEMENTATIONS (in portable/MemMang/):
+------------------------------------------------------------------+
|                                                                  |
|  heap_1.c: ALLOCATE ONLY                                         |
|  +------------------------------------------------------------+  |
|  | - pvPortMalloc() works                                     |  |
|  | - vPortFree() does NOTHING                                 |  |
|  | - Simplest, smallest code                                  |  |
|  | - No fragmentation (nothing ever freed)                    |  |
|  | - Best for: Static systems, safety-critical                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  heap_2.c: BEST FIT, NO COALESCING                               |
|  +------------------------------------------------------------+  |
|  | - pvPortMalloc() uses best-fit algorithm                   |  |
|  | - vPortFree() returns memory to pool                       |  |
|  | - Does NOT combine adjacent free blocks                    |  |
|  | - Can fragment if sizes vary                               |  |
|  | - Best for: Fixed-size allocations                         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  heap_3.c: WRAPPER FOR STANDARD MALLOC                           |
|  +------------------------------------------------------------+  |
|  | - Wraps standard library malloc()/free()                   |  |
|  | - Adds thread-safety (suspends scheduler)                  |  |
|  | - Uses compiler's heap, not FreeRTOS heap                  |  |
|  | - Best for: Using existing tested allocator                |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  heap_4.c: FIRST FIT WITH COALESCING                             |
|  +------------------------------------------------------------+  |
|  | - pvPortMalloc() uses first-fit algorithm                  |  |
|  | - vPortFree() combines adjacent free blocks                |  |
|  | - Reduces fragmentation significantly                      |  |
|  | - Most commonly used                                       |  |
|  | - Best for: General purpose dynamic allocation             |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  heap_5.c: HEAP_4 + MULTIPLE REGIONS                             |
|  +------------------------------------------------------------+  |
|  | - Same as heap_4 but supports non-contiguous memory        |  |
|  | - Must call vPortDefineHeapRegions() before use            |  |
|  | - Can span internal + external RAM                         |  |
|  | - Best for: Systems with multiple memory regions           |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### Heap Comparison Table

| Feature | heap_1 | heap_2 | heap_3 | heap_4 | heap_5 |
|---------|--------|--------|--------|--------|--------|
| Malloc | Yes | Yes | Yes | Yes | Yes |
| Free | No | Yes | Yes | Yes | Yes |
| Coalescing | N/A | No | Depends | Yes | Yes |
| Algorithm | Linear | Best-fit | Library | First-fit | First-fit |
| Multi-region | No | No | No | No | Yes |
| Deterministic | Yes | Mostly | No | Mostly | Mostly |
| Code size | Tiny | Small | Tiny | Medium | Medium |
| Best for | Static | Fixed-size | Reuse lib | General | Multi-RAM |

**Chinese Explanation (中文说明):**

为什么FreeRTOS不强制使用一种分配器？嵌入式系统内存约束差异很大：安全关键系统启动后不允许动态分配（用heap_1），简单IoT传感器固定任务集合（用heap_1或heap_2），复杂网关需要动态创建/删除（用heap_4），多区域内存系统（用heap_5）。

五种堆实现：
- heap_1：只分配不释放，最简单最小
- heap_2：最佳适配无合并，适合固定大小分配
- heap_3：包装标准malloc/free，添加线程安全
- heap_4：首次适配带合并，最常用，通用动态分配
- heap_5：heap_4+多区域支持，适合有内外部RAM的系统

### heap_4 Internals (Most Common)

```
HEAP_4 MEMORY LAYOUT:
+------------------------------------------------------------------+
|                                                                  |
|  ucHeap[] array (configTOTAL_HEAP_SIZE bytes):                   |
|                                                                  |
|  +------+--------+------+--------+------+-----------+------+     |
|  | HDR  | USED   | HDR  | FREE   | HDR  | USED      | END  |     |
|  | BLK  | BLOCK  | BLK  | BLOCK  | BLK  | BLOCK     | MARK |     |
|  +------+--------+------+--------+------+-----------+------+     |
|  ^                       ^                                       |
|  xStart                  Linked in free list                     |
|                                                                  |
|  BlockLink_t (header):                                           |
|  +--------------------+                                          |
|  | pxNextFreeBlock    | -> Next free block (or NULL if allocated)|
|  | xBlockSize         | -> Size (MSB = allocated flag)           |
|  +--------------------+                                          |
|                                                                  |
+------------------------------------------------------------------+

ALLOCATION (pvPortMalloc):
+------------------------------------------------------------------+
|                                                                  |
|  1. Add header size + alignment to requested size                |
|  2. Walk free list, find first block >= needed size              |
|  3. If block much larger, split it                               |
|  4. Mark block as allocated (set MSB of xBlockSize)              |
|  5. Return pointer to memory AFTER header                        |
|                                                                  |
+------------------------------------------------------------------+

FREEING (vPortFree) WITH COALESCING:
+------------------------------------------------------------------+
|                                                                  |
|  BEFORE FREE:                                                    |
|  [USED A] [FREE] [USED B] [FREE]                                 |
|                                                                  |
|  Free block B:                                                   |
|  [USED A] [FREE] [FREE B] [FREE]                                 |
|                                                                  |
|  Coalesce adjacent:                                              |
|  [USED A] [====== LARGE FREE ======]                             |
|                                                                  |
|  This prevents fragmentation!                                    |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

heap_4内部结构：ucHeap数组被分成多个块，每个块有BlockLink_t头（pxNextFreeBlock指向下一个空闲块，xBlockSize存储大小和分配标志）。

分配过程：添加头大小和对齐、遍历空闲列表找首个足够大的块、如果块太大则分割、标记为已分配、返回头后面的内存指针。

释放与合并：释放块后检查相邻块，如果也是空闲则合并成更大的空闲块，防止碎片化。

---

## 10.2 Stack Management

### Per-Task Stacks

```
TASK STACK ALLOCATION:
+------------------------------------------------------------------+
|                                                                  |
|  Each task has its own stack:                                    |
|                                                                  |
|  xTaskCreate( pvTaskCode,                                        |
|               "Name",                                            |
|               usStackDepth,  // <-- Stack size in WORDS          |
|               pvParameters,                                      |
|               uxPriority,                                        |
|               pxCreatedTask );                                   |
|                                                                  |
|  Stack size in WORDS, not bytes!                                 |
|  - 32-bit MCU: 1 word = 4 bytes                                  |
|  - usStackDepth = 256 means 256 * 4 = 1024 bytes                 |
|                                                                  |
+------------------------------------------------------------------+

STACK CONTENTS:
+------------------------------------------------------------------+
|                                                                  |
|  High Address (stack bottom for descending stack)                |
|  +------------------------------------------------------+        |
|  | Initial context (PC, LR, R0-R12, xPSR for Cortex-M) |        |
|  +------------------------------------------------------+        |
|  | Local variables                                      |        |
|  +------------------------------------------------------+        |
|  | Return addresses (nested function calls)             |        |
|  +------------------------------------------------------+        |
|  | Saved registers during context switch                |        |
|  +------------------------------------------------------+        |
|  | (unused space)                                       |        |
|  +------------------------------------------------------+        |
|  Low Address (stack top for descending stack)                    |
|       ^                                                          |
|       pxTopOfStack points here                                   |
|                                                                  |
+------------------------------------------------------------------+
```

### Stack Overflow Detection

```
STACK OVERFLOW DETECTION METHODS:
+------------------------------------------------------------------+
|                                                                  |
|  configCHECK_FOR_STACK_OVERFLOW = 0: DISABLED                    |
|  +------------------------------------------------------------+  |
|  | No checking - fastest but dangerous                        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  configCHECK_FOR_STACK_OVERFLOW = 1: END-OF-STACK CHECK          |
|  +------------------------------------------------------------+  |
|  | At context switch, check if pxTopOfStack is within bounds  |  |
|  | Detects: Large overflows                                   |  |
|  | Misses: Small overflows between context switches           |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  configCHECK_FOR_STACK_OVERFLOW = 2: PATTERN CHECK               |
|  +------------------------------------------------------------+  |
|  | Fill stack with 0xA5A5A5A5 at creation                     |  |
|  | Check last 16 bytes for pattern at context switch          |  |
|  | Detects: Most overflows                                    |  |
|  | Cost: Slight overhead, initial fill time                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  CALLBACK: vApplicationStackOverflowHook()                       |
|  +------------------------------------------------------------+  |
|  | Called when overflow detected                              |  |
|  | Parameters: Task handle, task name                         |  |
|  | WARNING: Stack is already corrupted at this point!         |  |
|  | Action: Log, halt, reset - DO NOT RETURN                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### Why Stack Sizing Is Critical

```
STACK SIZE TOO SMALL:
+------------------------------------------------------------------+
|                                                                  |
|  1. Function calls nest deeply                                   |
|  2. Stack grows toward other memory                              |
|  3. Overwrites: TCB, other task's stack, global variables        |
|  4. CORRUPTION - system behaves erratically                      |
|  5. Hard to debug (symptoms appear elsewhere)                    |
|                                                                  |
+------------------------------------------------------------------+

STACK SIZE TOO LARGE:
+------------------------------------------------------------------+
|                                                                  |
|  1. Wastes precious RAM                                          |
|  2. Fewer tasks can be created                                   |
|  3. May not fit in available memory                              |
|                                                                  |
+------------------------------------------------------------------+

SIZING METHODOLOGY:
+------------------------------------------------------------------+
|                                                                  |
|  1. Calculate minimum:                                           |
|     - Deepest call chain * average frame size                    |
|     - + ISR nesting (if interrupt uses task stack)               |
|     - + Context save area                                        |
|                                                                  |
|  2. Add safety margin (25-50%)                                   |
|                                                                  |
|  3. Test with stack watermarking (uxTaskGetStackHighWaterMark)   |
|                                                                  |
|  4. In production, consider method 2 overflow checking           |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

每任务栈：每个任务有自己的栈，xTaskCreate的usStackDepth参数是字数（不是字节！）。32位MCU上256字=1024字节。

栈内容包括：初始上下文、局部变量、返回地址、上下文切换时保存的寄存器。

栈溢出检测：
- 方法0：禁用，最快但危险
- 方法1：上下文切换时检查pxTopOfStack是否越界，检测大溢出
- 方法2：创建时填充0xA5模式，上下文切换时检查最后16字节，检测大多数溢出

回调vApplicationStackOverflowHook在检测到溢出时调用，此时栈已损坏，应该记录/停止/复位，不要返回。

栈大小调整：太小导致损坏难以调试，太大浪费RAM。方法：计算最深调用链、加安全裕度、用watermark测试、生产环境启用溢出检测。

---

## Summary

```
MEMORY MANAGEMENT MENTAL MODEL:
+==================================================================+
||                                                                ||
||  1. Choose heap implementation based on needs:                ||
||     - heap_1: Never free, simplest                            ||
||     - heap_2: Free but no coalesce, fixed-size allocs         ||
||     - heap_3: Wrap standard malloc                            ||
||     - heap_4: Most versatile, general purpose                 ||
||     - heap_5: Multiple memory regions                         ||
||                                                                ||
||  2. Stack sizing is YOUR responsibility                       ||
||     - No virtual memory to catch overflow                     ||
||     - Overflow = silent corruption                            ||
||     - Use overflow detection in development                   ||
||                                                                ||
||  3. Prefer static allocation when possible                    ||
||     - configSUPPORT_STATIC_ALLOCATION = 1                     ||
||     - xTaskCreateStatic(), xQueueCreateStatic(), etc.         ||
||     - No allocation failures at runtime                       ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Porting FreeRTOS](11-porting-freertos.md)
