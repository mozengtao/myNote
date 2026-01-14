# Section 5: Lists - The Hidden Backbone

Lists are the fundamental data structure in FreeRTOS. Understanding `list.c` and `list.h` is essential because every kernel mechanism depends on them.

## 5.1 Why Lists Matter

### Where Lists Are Used

```
LIST USAGE THROUGHOUT FREERTOS:
+------------------------------------------------------------------+
|                                                                  |
|  TASK STATE TRACKING:                                            |
|  +------------------------------------------------------------+  |
|  | pxReadyTasksLists[]  - Ready tasks (one list per priority) |  |
|  | xDelayedTaskList1    - Blocked tasks waiting on time       |  |
|  | xDelayedTaskList2    - Overflow list for tick wraparound   |  |
|  | xPendingReadyList    - Tasks to be readied after suspend   |  |
|  | xSuspendedTaskList   - Suspended tasks                     |  |
|  | xTasksWaitingTermination - Deleted tasks awaiting cleanup  |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  QUEUE/SEMAPHORE WAITING:                                        |
|  +------------------------------------------------------------+  |
|  | xTasksWaitingToSend    - Tasks blocked on full queue       |  |
|  | xTasksWaitingToReceive - Tasks blocked on empty queue      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  EVENT GROUPS:                                                   |
|  +------------------------------------------------------------+  |
|  | xTasksWaitingForBits - Tasks waiting for event bits        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  TIMERS:                                                         |
|  +------------------------------------------------------------+  |
|  | xActiveTimerList1/2 - Active software timers               |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

链表在FreeRTOS中无处不在：

任务状态跟踪：pxReadyTasksLists[]（每个优先级一个就绪列表）、xDelayedTaskList（延时任务）、xSuspendedTaskList（挂起任务）等。

队列/信号量等待：xTasksWaitingToSend（等待发送的任务）、xTasksWaitingToReceive（等待接收的任务）。

事件组：xTasksWaitingForBits（等待事件位的任务）。

定时器：xActiveTimerList（活动软件定时器）。

### Why FreeRTOS Uses Intrusive Lists

```
TRADITIONAL LINKED LIST:
+------------------------------------------------------------------+
|                                                                  |
|  +--------+     +--------+     +--------+                        |
|  | Node 1 |---->| Node 2 |---->| Node 3 |                        |
|  +---+----+     +---+----+     +---+----+                        |
|      |              |              |                             |
|      v              v              v                             |
|  +------+       +------+       +------+                          |
|  | Data |       | Data |       | Data |                          |
|  +------+       +------+       +------+                          |
|                                                                  |
|  PROBLEMS:                                                       |
|  - Extra memory allocation for each node                         |
|  - Poor cache locality (node and data separate)                  |
|  - Data can only be in ONE list at a time                        |
|                                                                  |
+------------------------------------------------------------------+

INTRUSIVE LINKED LIST (FreeRTOS):
+------------------------------------------------------------------+
|                                                                  |
|  +------------------+     +------------------+                    |
|  | TCB              |     | TCB              |                    |
|  | +-------------+  |     | +-------------+  |                    |
|  | |xStateListItem|-------->|xStateListItem|----->...            |
|  | +-------------+  |     | +-------------+  |                    |
|  | |xEventListItem|--.    | |xEventListItem|--.                  |
|  | +-------------+  | |   | +-------------+  | |                 |
|  | other fields...  | |   | other fields...  | |                 |
|  +------------------+ |   +------------------+ |                  |
|                       |                        |                  |
|                       v                        v                  |
|                 Event wait list (separate chain)                 |
|                                                                  |
|  ADVANTAGES:                                                     |
|  - NO extra allocation (ListItem embedded in TCB)                |
|  - Good cache locality (ListItem near other TCB fields)          |
|  - TCB can be in MULTIPLE lists (state + event)                  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

FreeRTOS使用侵入式链表的原因：

传统链表问题：每个节点需要额外内存分配、缓存局部性差（节点和数据分离）、数据只能在一个列表中。

侵入式链表优势：无需额外分配（ListItem嵌入TCB）、缓存局部性好、TCB可以同时在多个列表中（状态列表+事件列表）。

每个TCB包含两个ListItem：xStateListItem（用于状态列表，如就绪、阻塞）和xEventListItem（用于事件等待列表，如队列等待）。

---

## 5.2 List Data Structures

### ListItem_t Structure

```c
// From list.h
struct xLIST_ITEM
{
    TickType_t xItemValue;              // Sort key
    struct xLIST_ITEM *pxNext;          // Next in list
    struct xLIST_ITEM *pxPrevious;      // Previous in list  
    void *pvOwner;                      // Points back to TCB
    struct xLIST *pxContainer;          // Which list contains this item
};
typedef struct xLIST_ITEM ListItem_t;
```

```
ListItem_t Memory Layout:
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------+                                          |
|  | xItemValue         |  <- Sort key (wake time, inverse prio)   |
|  +--------------------+                                          |
|  | pxNext             |  -----> Next ListItem_t                  |
|  +--------------------+                                          |
|  | pxPrevious         |  <----- Previous ListItem_t              |
|  +--------------------+                                          |
|  | pvOwner            |  -----> TCB that owns this item          |
|  +--------------------+                                          |
|  | pxContainer        |  -----> List_t containing this item      |
|  +--------------------+                                          |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

ListItem_t字段说明：
- xItemValue：排序键（如唤醒时间、优先级反转值）
- pxNext/pxPrevious：双向链表指针
- pvOwner：指向拥有此项的TCB
- pxContainer：指向包含此项的列表

pvOwner实现双向引用：列表可以找到TCB，TCB的ListItem知道它在哪个列表中。

### List_t Structure

```c
// From list.h
typedef struct xLIST
{
    UBaseType_t uxNumberOfItems;    // Current item count
    ListItem_t *pxIndex;            // Walking pointer for round-robin
    MiniListItem_t xListEnd;        // Sentinel with max value
} List_t;
```

```
List_t Structure:
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------+                                          |
|  | uxNumberOfItems    |  <- Current count                        |
|  +--------------------+                                          |
|  | pxIndex            |  -----> Current iteration position       |
|  +--------------------+                                          |
|  | xListEnd           |  <- Sentinel node (always present)       |
|  |   .xItemValue=MAX  |     Marks end of sorted list             |
|  |   .pxNext          |     Points to first real item            |
|  |   .pxPrevious      |     Points to last real item             |
|  +--------------------+                                          |
|                                                                  |
+------------------------------------------------------------------+
```

### List with Items

```
Empty List:
+------------------------------------------------------------------+
|                                                                  |
|  List_t:                                                         |
|  +-------------------+                                           |
|  | uxNumberOfItems=0 |                                           |
|  | pxIndex=&xListEnd |                                           |
|  | xListEnd:         |                                           |
|  |   xItemValue=MAX  |                                           |
|  |   pxNext=&xListEnd|---+                                       |
|  |   pxPrevious=     |   |  (points to itself)                   |
|  |     &xListEnd     |<--+                                       |
|  +-------------------+                                           |
|                                                                  |
+------------------------------------------------------------------+

List with 3 Items (sorted by xItemValue):
+------------------------------------------------------------------+
|                                                                  |
|  List_t:                                                         |
|  +-------------------+                                           |
|  | uxNumberOfItems=3 |                                           |
|  | pxIndex           |----------------------------------------+  |
|  | xListEnd:         |                                        |  |
|  |   xItemValue=MAX  |                                        |  |
|  |   pxNext----------|--+                                     |  |
|  |   pxPrevious------|--|---------------------------+         |  |
|  +-------------------+  |                           |         |  |
|                         v                           |         |  |
|  +-------------------+  +-------------------+  +-------------------+
|  | ListItem A        |  | ListItem B        |  | ListItem C        |
|  | xItemValue=100    |  | xItemValue=200    |  | xItemValue=300    |
|  | pxNext------------|->| pxNext------------|->| pxNext=&xListEnd  |
|  | pxPrevious=       |<-| pxPrevious        |<-| pxPrevious        |
|  |   &xListEnd       |  |                   |  |                   |
|  | pvOwner=&TCB_A    |  | pvOwner=&TCB_B    |  | pvOwner=&TCB_C    |
|  +-------------------+  +-------------------+  +-------------------+
|                                                        ^
|                                                        |
|                                               pxIndex points here
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

List_t字段说明：
- uxNumberOfItems：当前项数
- pxIndex：遍历指针，用于轮询
- xListEnd：哨兵节点，值为MAX，始终存在

列表结构：xListEnd作为哨兵，其xItemValue为最大值确保它始终在排序列表的末尾。空列表时xListEnd的next/previous指向自己。有项时，项按xItemValue排序，xListEnd.pxNext指向第一项，xListEnd.pxPrevious指向最后一项。

---

## 5.3 List Operations

### vListInitialise

```c
// Set up an empty list
void vListInitialise( List_t * const pxList )
{
    // pxIndex points to end marker
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );
    
    // End marker has maximum value (always last in sorted order)
    pxList->xListEnd.xItemValue = portMAX_DELAY;
    
    // Empty list: end marker points to itself
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );
    
    pxList->uxNumberOfItems = 0;
}
```

### vListInsert (Sorted Insert)

```c
// Insert item in sorted order by xItemValue
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    ListItem_t *pxIterator;
    const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;
    
    // Find insertion point (items with same value go after existing)
    if( xValueOfInsertion == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        // Walk list to find correct position
        for( pxIterator = &pxList->xListEnd;
             pxIterator->pxNext->xItemValue <= xValueOfInsertion;
             pxIterator = pxIterator->pxNext )
        {
            // Just finding position
        }
    }
    
    // Insert after pxIterator
    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewListItem;
    
    // Remember which list this item is in
    pxNewListItem->pxContainer = pxList;
    
    pxList->uxNumberOfItems++;
}
```

```
vListInsert Example (insert value=150):
+------------------------------------------------------------------+
|                                                                  |
|  BEFORE:                                                         |
|  xListEnd <-> [100] <-> [200] <-> [300] <-> xListEnd             |
|                                                                  |
|  Find position: 100 <= 150, continue; 200 > 150, stop            |
|  Insert after [100], before [200]                                |
|                                                                  |
|  AFTER:                                                          |
|  xListEnd <-> [100] <-> [150] <-> [200] <-> [300] <-> xListEnd   |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

vListInsert按xItemValue排序插入。遍历列表找到正确位置（值相同的项放在现有项之后），然后执行标准的双向链表插入操作。项的pxContainer设置为所在列表。

用途：延时任务按唤醒时间排序插入xDelayedTaskList，定时器按到期时间排序插入xActiveTimerList。

### vListInsertEnd (Round-Robin Insert)

```c
// Insert at current pxIndex position (for fair round-robin)
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    ListItem_t * const pxIndex = pxList->pxIndex;
    
    // Insert just before pxIndex
    pxNewListItem->pxNext = pxIndex;
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;
    pxIndex->pxPrevious->pxNext = pxNewListItem;
    pxIndex->pxPrevious = pxNewListItem;
    
    pxNewListItem->pxContainer = pxList;
    pxList->uxNumberOfItems++;
}
```

```
vListInsertEnd for Round-Robin:
+------------------------------------------------------------------+
|                                                                  |
|  Ready list at priority N, pxIndex at [B]:                       |
|                                                                  |
|  xListEnd <-> [A] <-> [B] <-> [C] <-> xListEnd                   |
|                        ^                                         |
|                        pxIndex                                   |
|                                                                  |
|  Insert new task [D] using vListInsertEnd:                       |
|  - Inserted just BEFORE pxIndex                                  |
|  - [D] will be serviced AFTER [A] walks through                  |
|                                                                  |
|  xListEnd <-> [A] <-> [D] <-> [B] <-> [C] <-> xListEnd           |
|                               ^                                  |
|                               pxIndex (unchanged)                |
|                                                                  |
|  This ensures [D] waits its turn, doesn't jump queue             |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

vListInsertEnd在pxIndex之前插入，用于公平轮询。在就绪列表中，这确保新就绪的任务排在队列末尾，等待轮到它才运行，而不是插队。

用途：将任务添加到就绪列表，确保同优先级任务公平轮询。

### uxListRemove

```c
// Remove item from its list
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
    List_t * const pxList = pxItemToRemove->pxContainer;
    
    // Standard doubly-linked removal
    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;
    
    // If pxIndex pointed to removed item, move it back
    if( pxList->pxIndex == pxItemToRemove )
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    
    // Item no longer in a list
    pxItemToRemove->pxContainer = NULL;
    pxList->uxNumberOfItems--;
    
    return pxList->uxNumberOfItems;
}
```

**Chinese Explanation (中文说明):**

uxListRemove从列表中移除项。标准双向链表移除，如果pxIndex指向被移除的项则后移。项的pxContainer设为NULL表示不在任何列表中。返回剩余项数。

---

## 5.4 How Lists Power Everything

### Ready Tasks

```
Ready Lists Usage:
+------------------------------------------------------------------+
|                                                                  |
|  pxReadyTasksLists[ configMAX_PRIORITIES ]:                      |
|                                                                  |
|  [Priority 4]: <-> TaskA <-> xListEnd                            |
|  [Priority 3]: <-> TaskB <-> TaskC <-> xListEnd                  |
|  [Priority 2]: <-> xListEnd (empty)                              |
|  [Priority 1]: <-> TaskD <-> xListEnd                            |
|  [Priority 0]: <-> IdleTask <-> xListEnd                         |
|                                                                  |
|  Selection: Find highest non-empty, use listGET_OWNER_OF_NEXT_ENTRY |
|                                                                  |
|  listGET_OWNER_OF_NEXT_ENTRY(pxTCB, &pxReadyTasksLists[3]):      |
|  - Advances pxIndex                                              |
|  - Returns pvOwner of next item                                  |
|  - Implements round-robin within priority                        |
|                                                                  |
+------------------------------------------------------------------+
```

### Delayed Tasks

```
Delayed Tasks Usage:
+------------------------------------------------------------------+
|                                                                  |
|  xDelayedTaskList (sorted by wake time):                         |
|                                                                  |
|  xListEnd <-> [wake=100] <-> [wake=150] <-> [wake=300] <-> xListEnd
|                   ^                                              |
|                   First to wake                                  |
|                                                                  |
|  xNextTaskUnblockTime = 100 (cached for efficiency)              |
|                                                                  |
|  In xTaskIncrementTick():                                        |
|    if( xTickCount >= xNextTaskUnblockTime )                      |
|    {                                                             |
|        // O(1) check for wake time                               |
|        // Move task(s) to ready list                             |
|        // Update xNextTaskUnblockTime                            |
|    }                                                             |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

就绪任务：每个优先级一个列表，选择时找最高非空优先级，listGET_OWNER_OF_NEXT_ENTRY实现同优先级轮询。

延时任务：按唤醒时间排序，xNextTaskUnblockTime缓存最近唤醒时间。xTaskIncrementTick()中O(1)检查是否需要唤醒。

### Why Lists Were Chosen

| Alternative | Problem |
|-------------|---------|
| **Array** | Insertion/removal O(n), fixed size |
| **Binary Heap** | More complex, larger code size |
| **Skip List** | More memory, more complexity |
| **Linked List** | Simple, O(1) insert/remove (if position known), variable size |

```
FreeRTOS Optimization: O(1) for Common Cases
+------------------------------------------------------------------+
|                                                                  |
|  READY LIST:                                                     |
|  - Insert: O(1) with vListInsertEnd (position known)             |
|  - Remove: O(1) (item knows its neighbors)                       |
|  - Select: O(priorities) worst case, usually O(1)                |
|                                                                  |
|  DELAYED LIST:                                                   |
|  - Insert: O(n) for vListInsert (must find position)             |
|  - BUT: Typically few delayed tasks at once                      |
|  - Remove: O(1)                                                  |
|  - Check: O(1) with xNextTaskUnblockTime cache                   |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

为什么选择链表？数组插入/删除O(n)且固定大小；二叉堆更复杂、代码更大；跳表更多内存和复杂度。链表简单，位置已知时插入/删除O(1)。

FreeRTOS优化：
- 就绪列表：插入O(1)（vListInsertEnd位置已知）、移除O(1)、选择最坏O(优先级数)
- 延时列表：插入O(n)但通常延时任务不多、移除O(1)、检查O(1)（xNextTaskUnblockTime缓存）

---

## Summary

```
LIST MENTAL MODEL:
+==================================================================+
||                                                                ||
||  1. ListItem_t is EMBEDDED in TCB (intrusive design)          ||
||                                                                ||
||  2. Each TCB has TWO ListItems:                               ||
||     - xStateListItem: which state list (ready/blocked/etc.)   ||
||     - xEventListItem: which event waiting list               ||
||                                                                ||
||  3. pvOwner provides reverse link: List -> TCB                ||
||                                                                ||
||  4. pxContainer tracks: which list is this item in?           ||
||                                                                ||
||  5. Two insert modes:                                         ||
||     - vListInsert: sorted by xItemValue (delays, timers)      ||
||     - vListInsertEnd: at tail for round-robin (ready lists)   ||
||                                                                ||
||  6. O(1) removal because item knows its neighbors             ||
||                                                                ||
+==================================================================+
```

| Operation | Function | Complexity | Use Case |
|-----------|----------|------------|----------|
| Init list | vListInitialise | O(1) | Setup |
| Sorted insert | vListInsert | O(n) | Delays, timers |
| End insert | vListInsertEnd | O(1) | Ready lists |
| Remove | uxListRemove | O(1) | State change |
| Iterate | listGET_OWNER_OF_NEXT_ENTRY | O(1) | Round-robin |

**Next Section**: [Time Management](06-time-management.md)
