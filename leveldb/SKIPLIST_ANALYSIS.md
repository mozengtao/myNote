# LevelDB SkipList 实现分析

> **文件位置**: `db/skiplist.h`

---

## 目录

1. [模板设计思想](#1-模板设计思想)
2. [并发控制机制](#2-并发控制机制)
3. [迭代器模式实现](#3-迭代器模式实现)
4. [性能分析与对比](#4-性能分析与对比)
5. [内存布局图解](#5-内存布局图解)

---

## 1. 模板设计思想

### 1.1 模板参数设计

```cpp
template <typename Key, class Comparator>
class SkipList {
  // ...
};
```

**两个模板参数**：

| 参数 | 类型 | 用途 |
|------|------|------|
| `Key` | `typename` | 键的数据类型，支持任意类型 |
| `Comparator` | `class` | 比较器类型，实现键的比较逻辑 |

### 1.2 Comparator 策略模式

```cpp
// 比较器作为成员变量存储
Comparator const compare_;

// 使用比较器
bool Equal(const Key& a, const Key& b) const { 
  return (compare_(a, b) == 0); 
}

bool KeyIsAfterNode(const Key& key, Node* n) const {
  return (n != nullptr) && (compare_(n->key, key) < 0);
}
```

**设计优势**：
- ✅ **类型安全**：编译期确定比较逻辑
- ✅ **零开销抽象**：内联展开，无虚函数调用开销
- ✅ **灵活扩展**：支持自定义比较规则

### 1.3 实际使用示例

```cpp
// db/memtable.h:75 - 在 MemTable 中的使用
struct KeyComparator {
  const InternalKeyComparator comparator;
  explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
  int operator()(const char* a, const char* b) const;
};

typedef SkipList<const char*, KeyComparator> Table;
```

**Key 类型**：`const char*`（指向编码后的键值对）
**Comparator**：`KeyComparator`（实现了 `operator()`）

---

## 2. 并发控制机制

### 2.1 线程安全模型

```cpp
// Thread safety
// -------------
//
// Writes require external synchronization, most likely a mutex.
// Reads require a guarantee that the SkipList will not be destroyed
// while the read is in progress.  Apart from that, reads progress
// without any internal locking or synchronization.
```

**核心设计**：**单写者多读者模型**

| 操作 | 同步要求 |
|------|----------|
| 写入 (Insert) | 需要外部互斥锁 |
| 读取 (Contains, Iterator) | 无需锁，但需保证 SkipList 存活 |

### 2.2 原子操作与内存屏障

```cpp
// Node 结构中的原子指针数组
std::atomic<Node*> next_[1];

// 带 acquire 语义的读取
Node* Next(int n) {
  assert(n >= 0);
  // acquire load: 保证看到完整初始化的 Node
  return next_[n].load(std::memory_order_acquire);
}

// 带 release 语义的写入
void SetNext(int n, Node* x) {
  assert(n >= 0);
  // release store: 保证插入的节点完全可见
  next_[n].store(x, std::memory_order_release);
}
```

**内存序使用**：

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    内存屏障使用场景                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  写入者 (Insert)                     读取者 (Contains)                  │
│  ─────────────────                   ─────────────────                  │
│                                                                         │
│  1. 分配并初始化 Node                                                   │
│     x = NewNode(key, height);                                           │
│                                                                         │
│  2. 设置新节点的 next 指针                                              │
│     x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));               │
│     ↑ relaxed: 还未发布节点                                             │
│                                                                         │
│  3. 发布节点到链表                                                       │
│     prev[i]->SetNext(i, x);                                             │
│     ↑ release: 确保步骤1,2的写入对读者可见                              │
│                                                                         │
│                                      4. 读取链表                        │
│                                         node = x->Next(level);          │
│                                         ↑ acquire: 与 release 同步      │
│                                                                         │
│  Release-Acquire 配对保证：                                             │
│  - 读者看到的节点一定是完全初始化的                                     │
│  - 读者看到新节点后，节点的 key 和 next 指针都是有效的                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.3 无屏障变体

```cpp
// 在某些安全位置使用的无屏障版本
Node* NoBarrier_Next(int n) {
  return next_[n].load(std::memory_order_relaxed);
}
void NoBarrier_SetNext(int n, Node* x) {
  next_[n].store(x, std::memory_order_relaxed);
}
```

**使用场景**：
- 在 `Insert()` 中设置新节点的 next 指针时（节点尚未发布）
- 因为后续的 `SetNext()` 会提供必要的屏障

### 2.4 max_height_ 的并发访问

```cpp
std::atomic<int> max_height_;

// 读取 - relaxed 足够
inline int GetMaxHeight() const {
  return max_height_.load(std::memory_order_relaxed);
}

// 写入 - Insert() 中
max_height_.store(height, std::memory_order_relaxed);
```

**为什么 relaxed 足够**？

```cpp
// 代码注释解释：
// It is ok to mutate max_height_ without any synchronization
// with concurrent readers.  A concurrent reader that observes
// the new value of max_height_ will see either the old value of
// new level pointers from head_ (nullptr), or a new value set in
// the loop below.  In the former case the reader will
// immediately drop to the next level since nullptr sorts after all
// keys.  In the latter case the reader will use the new node.
```

读者看到过期的 max_height_ 也是安全的：
- 看到旧值：在高层级遇到 nullptr，直接下降
- 看到新值：使用新的节点，也正确

---

## 3. 迭代器模式实现

### 3.1 嵌套迭代器类

```cpp
class Iterator {
 public:
  explicit Iterator(const SkipList* list);
  
  bool Valid() const;              // 是否指向有效节点
  const Key& key() const;          // 当前键
  void Next();                     // 前进
  void Prev();                     // 后退
  void Seek(const Key& target);    // 定位到 >= target
  void SeekToFirst();              // 定位到首元素
  void SeekToLast();               // 定位到末元素

 private:
  const SkipList* list_;
  Node* node_;
};
```

### 3.2 迭代器实现细节

```cpp
// 前进操作 - O(1)
inline void SkipList<Key, Comparator>::Iterator::Next() {
  assert(Valid());
  node_ = node_->Next(0);  // 沿最底层链表前进
}

// 后退操作 - O(log n) 
inline void SkipList<Key, Comparator>::Iterator::Prev() {
  assert(Valid());
  // 没有 prev 指针，需要从头搜索
  node_ = list_->FindLessThan(node_->key);
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

// Seek 操作 - O(log n)
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, nullptr);
}
```

### 3.3 设计权衡

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| Next() | O(1) | 直接跟随 level-0 链表 |
| Prev() | O(log n) | 无 prev 指针，需重新搜索 |
| Seek() | O(log n) | 标准跳表查找 |

**不使用 prev 指针的原因**：
- ✅ 减少内存占用
- ✅ 简化并发控制
- ✅ LevelDB 主要使用前向迭代

---

## 4. 性能分析与对比

### 4.1 时间复杂度分析

| 操作 | 平均 | 最坏 |
|------|------|------|
| 插入 (Insert) | O(log n) | O(n) |
| 查找 (Contains) | O(log n) | O(n) |
| 删除 | N/A | N/A |

**注意**：LevelDB 的 SkipList 不支持删除操作！
- 节点分配后永不释放，直到整个 SkipList 销毁
- 这简化了并发控制

### 4.2 随机高度生成

```cpp
int SkipList<Key, Comparator>::RandomHeight() {
  static const unsigned int kBranching = 4;  // 分支因子
  int height = 1;
  while (height < kMaxHeight && rnd_.OneIn(kBranching)) {
    height++;
  }
  return height;
}
```

**概率分析**：
- 高度为 1 的概率: 3/4
- 高度为 2 的概率: 3/16
- 高度为 k 的概率: (3/4) × (1/4)^(k-1)
- 平均高度: 1 + 1/3 ≈ 1.33

### 4.3 对比红黑树/AVL树

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SkipList vs 平衡树对比                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  维度              SkipList                 红黑树/AVL树                │
│  ─────────────────────────────────────────────────────────────────────  │
│  实现复杂度        简单 (~200行)            复杂 (~500行)               │
│  插入复杂度        O(log n) 平均            O(log n) 保证               │
│  删除复杂度        不支持                   O(log n)                    │
│  空间开销          每节点 1.33 指针(平均)   每节点 3+ 指针              │
│  并发读支持        优秀(无锁)               需要复杂锁                  │
│  范围查询          天然有序                 需要中序遍历                │
│  缓存局部性        较差                     较好                        │
│  最坏情况          O(n) (极罕见)            O(log n) 保证               │
│                                                                         │
│  LevelDB 选择 SkipList 的原因:                                          │
│  1. 实现简单，代码量少                                                  │
│  2. 无锁读取，高并发性能                                                │
│  3. 不需要删除（MemTable 只追加）                                       │
│  4. 天然有序，支持范围扫描                                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. 内存布局图解

### 5.1 Node 结构

```cpp
template <typename Key, class Comparator>
struct SkipList<Key, Comparator>::Node {
  Key const key;                      // 键数据
  std::atomic<Node*> next_[1];        // 柔性数组
};
```

**节点分配**：

```cpp
Node* NewNode(const Key& key, int height) {
  char* const node_memory = arena_->AllocateAligned(
      sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1));
  return new (node_memory) Node(key);
}
```

### 5.2 单节点内存布局

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Node 内存布局 (height = 4)                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  低地址                                                    高地址       │
│    │                                                          │         │
│    ▼                                                          ▼         │
│  ┌─────────────┬──────────┬──────────┬──────────┬──────────┐           │
│  │    key      │ next_[0] │ next_[1] │ next_[2] │ next_[3] │           │
│  │             │ (level 0)│ (level 1)│ (level 2)│ (level 3)│           │
│  └─────────────┴──────────┴──────────┴──────────┴──────────┘           │
│  │            │                                              │         │
│  │← sizeof(Key)→│←──── sizeof(atomic<Node*>) × height ─────→│         │
│                                                                         │
│  实际大小 = sizeof(Node) + sizeof(atomic<Node*>) × (height - 1)        │
│           = sizeof(Key) + sizeof(atomic<Node*>) × height               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.3 完整 SkipList 结构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      SkipList 完整结构示例                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  max_height_ = 4                                                        │
│                                                                         │
│  Level 3:  head ────────────────────────────────────────────────► nil  │
│              │                                                          │
│  Level 2:  head ─────────────────────────► [25] ────────────────► nil  │
│              │                               │                          │
│  Level 1:  head ──────► [10] ──────────────► [25] ────► [45] ──► nil  │
│              │           │                   │           │              │
│  Level 0:  head ► [5] ► [10] ► [15] ► [20] ► [25] ► [45] ► [50] ► nil │
│                                                                         │
│                                                                         │
│  head 节点固定高度为 kMaxHeight (12)                                    │
│  其他节点高度随机生成 (1-12)                                            │
│                                                                         │
│  查找 key=20 的路径:                                                    │
│    1. Level 3: head → nil (20 < ∞)，下降                                │
│    2. Level 2: head → 25 (20 < 25)，下降                                │
│    3. Level 1: head → 10 → 25 (10 < 20 < 25)，下降                     │
│    4. Level 0: 10 → 15 → 20 ✓ 找到                                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.4 查找算法图解

```cpp
Node* FindGreaterOrEqual(const Key& key, Node** prev) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  
  while (true) {
    Node* next = x->Next(level);
    if (KeyIsAfterNode(key, next)) {
      x = next;                    // 在当前层继续前进
    } else {
      if (prev != nullptr) 
        prev[level] = x;           // 记录前驱
      if (level == 0) {
        return next;               // 返回结果
      } else {
        level--;                   // 下降一层
      }
    }
  }
}
```

```
查找路径示意 (查找 key=22):

Level 3:  head ══════════════════════════════════════════════►(nil)
            ║                                                    ↓
Level 2:  head ════════════════════════►[25]════════════════►(nil)
            ║                             ↓                      ↓
Level 1:  head ═══►[10]════════════════►[25]═══►[45]════════►(nil)
            ║       ║                     ↓       ↓              ↓
Level 0:  head ─►[5]─►[10]─►[15]─►[20]─►[25]─►[45]─►[50]──►(nil)
                                    ↑
                              返回 [25] (第一个 >= 22 的节点)

═══ 表示搜索路径
─── 表示普通链接
```

### 5.5 插入操作图解

```
插入 key=22 (随机高度=2):

Step 1: FindGreaterOrEqual(22, prev)
        prev[0] = [20], prev[1] = [10]

Step 2: 创建新节点
        new_node = NewNode(22, height=2)

Step 3: 连接新节点
        
Before:
Level 1:  [10] ─────────────────► [25]
Level 0:  [20] ─────────────────► [25]

After:
Level 1:  [10] ────► [22] ────► [25]
Level 0:  [20] ────► [22] ────► [25]

代码:
for (int i = 0; i < height; i++) {
  x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));  // 新节点指向后继
  prev[i]->SetNext(i, x);                               // 前驱指向新节点
}
```

---

## 总结

LevelDB 的 SkipList 实现体现了以下设计理念：

1. **简洁优先**：~200行代码实现完整功能
2. **并发友好**：单写多读，无锁读取
3. **零开销抽象**：模板编程，编译期展开
4. **精确内存控制**：配合 Arena，无单独释放

这是一个为 MemTable 场景量身定制的数据结构，不追求通用性，但在其应用场景中达到了极致的性能和简洁性。

---

*文档生成时间: 2024年*

