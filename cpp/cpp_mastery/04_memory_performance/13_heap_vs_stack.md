# Topic 13: Heap vs Stack Allocation

## 1. Problem Statement

### What real engineering problem does this solve?

Memory allocation strategy affects performance dramatically:

```
STACK ALLOCATION                     HEAP ALLOCATION
┌─────────────────────┐              ┌─────────────────────┐
│ Instant (~1 cycle)  │              │ Slow (~100+ cycles) │
│ Auto cleanup        │              │ Manual/RAII cleanup │
│ Limited size (~1MB) │              │ Large size possible │
│ LIFO only           │              │ Any lifetime        │
│ No fragmentation    │              │ Can fragment        │
└─────────────────────┘              └─────────────────────┘
```

### What goes wrong with wrong choice?

```cpp
// Stack overflow
void bad() {
    int hugeArray[10000000];  // 40MB on stack - CRASH!
}

// Unnecessary heap allocation
void slow() {
    for (int i = 0; i < 1000000; ++i) {
        auto p = std::make_unique<int>(i);  // Heap alloc in hot loop!
    }
}
```

**中文说明：**
栈分配极快（只是移动栈指针），但空间有限（通常 1-8MB）；堆分配灵活但慢（需要搜索空闲块、可能需要系统调用）。选择错误会导致栈溢出或不必要的性能损失。经验法则：小、短生命周期对象用栈；大或长生命周期对象用堆。

---

## 2. Core Idea

### Stack vs Heap Comparison

```
┌─────────────┬────────────────────┬────────────────────┐
│ ASPECT      │ STACK              │ HEAP               │
├─────────────┼────────────────────┼────────────────────┤
│ Speed       │ ~1 cycle           │ ~100+ cycles       │
│ Size limit  │ ~1-8 MB            │ Virtual memory     │
│ Lifetime    │ Scope-bound        │ Explicit control   │
│ Fragmentation│ None              │ Possible           │
│ Thread-safe │ Per-thread         │ Needs sync         │
│ Cleanup     │ Automatic          │ Manual/RAII        │
│ Growth      │ Fixed at link time │ Dynamic            │
└─────────────┴────────────────────┴────────────────────┘
```

### When to Use Each

```
USE STACK:                          USE HEAP:
• Small objects (<few KB)           • Large objects (>KB)
• Short lifetime (function scope)   • Long lifetime (outlives scope)
• Fixed-size arrays                 • Dynamic-size containers
• Performance-critical              • Size unknown at compile time
• Temporary buffers                 • Shared ownership
```

**中文说明：**
- **栈**：局部变量、小缓冲区、编译时已知大小的对象
- **堆**：动态数组、大对象、跨作用域生命周期、运行时确定大小

---

## 3. Idiomatic C++ Techniques

### Small Buffer Optimization (SBO)

```cpp
// Many STL types use SBO to avoid heap for small data
std::string small = "hi";       // Likely on stack (SBO)
std::string large(1000, 'x');   // On heap

std::function<void()> f = []{}; // Small lambda: SBO
std::function<void()> g = [big = std::vector<int>(1000)]{}; // Heap

// Custom SBO
template<typename T, size_t N = 64>
class SmallVector {
    alignas(T) char buffer_[N];
    T* data_ = reinterpret_cast<T*>(buffer_);
    size_t size_ = 0;
    size_t capacity_ = N / sizeof(T);
    bool onHeap_ = false;
    
    // Use buffer_ for small, heap for large
};
```

### Avoiding Heap Allocation

```cpp
// Use array instead of vector when size known
std::array<int, 100> stackArray;  // Stack
std::vector<int> heapVector(100); // Heap

// Reserve to avoid reallocations
std::vector<int> v;
v.reserve(1000);  // One allocation, not many

// Use string_view instead of copying strings
void process(std::string_view s);  // No allocation

// Use span for array views
void process(std::span<int> data); // No allocation
```

### Stack-Based Allocation for Temporary Use

```cpp
// alloca (C-style, dangerous, non-portable)
void* buffer = alloca(size);  // On stack, no free needed

// Better: fixed-size array
void process(size_t n) {
    if (n <= 256) {
        int buffer[256];  // Stack
        doWork(buffer, n);
    } else {
        std::vector<int> buffer(n);  // Heap
        doWork(buffer.data(), n);
    }
}
```

---

## 4. Complete C++ Example

```cpp
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

// ============================================================
// Benchmark: Stack vs Heap allocation speed
// ============================================================
void benchmarkAllocation() {
    constexpr int N = 1'000'000;
    
    // Stack allocation
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        int x[16];  // Stack
        x[0] = i;
        volatile int y = x[0];  // Prevent optimization
        (void)y;
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // Heap allocation
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        auto p = std::make_unique<int[]>(16);  // Heap
        p[0] = i;
        volatile int y = p[0];
        (void)y;
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto us1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    auto us2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();
    
    std::cout << "Stack: " << us1 << " us\n";
    std::cout << "Heap:  " << us2 << " us\n";
    std::cout << "Ratio: " << (double)us2 / us1 << "x slower\n";
}

// ============================================================
// Hybrid stack/heap strategy
// ============================================================
template<typename T, size_t StackSize = 256>
class HybridBuffer {
    alignas(T) char stackBuffer_[StackSize * sizeof(T)];
    T* data_;
    size_t size_;
    bool onHeap_;
    
public:
    explicit HybridBuffer(size_t n) : size_(n), onHeap_(n > StackSize) {
        if (onHeap_) {
            data_ = new T[n];
        } else {
            data_ = reinterpret_cast<T*>(stackBuffer_);
        }
    }
    
    ~HybridBuffer() {
        if (onHeap_) {
            delete[] data_;
        }
    }
    
    HybridBuffer(const HybridBuffer&) = delete;
    HybridBuffer& operator=(const HybridBuffer&) = delete;
    
    T* data() { return data_; }
    size_t size() const { return size_; }
    bool isOnHeap() const { return onHeap_; }
};

// ============================================================
// Object pool to reduce allocations
// ============================================================
template<typename T, size_t PoolSize = 64>
class ObjectPool {
    union Slot {
        T object;
        Slot* next;
        Slot() {}
        ~Slot() {}
    };
    
    std::array<Slot, PoolSize> pool_;
    Slot* freeList_ = nullptr;
    
public:
    ObjectPool() {
        for (size_t i = 0; i < PoolSize - 1; ++i) {
            pool_[i].next = &pool_[i + 1];
        }
        pool_[PoolSize - 1].next = nullptr;
        freeList_ = &pool_[0];
    }
    
    template<typename... Args>
    T* allocate(Args&&... args) {
        if (!freeList_) return nullptr;  // Pool exhausted
        Slot* slot = freeList_;
        freeList_ = slot->next;
        return new (&slot->object) T(std::forward<Args>(args)...);
    }
    
    void deallocate(T* ptr) {
        ptr->~T();
        Slot* slot = reinterpret_cast<Slot*>(ptr);
        slot->next = freeList_;
        freeList_ = slot;
    }
};

// ============================================================
// Avoid allocations in hot path
// ============================================================
class EventProcessor {
    // Pre-allocated buffer for temp work
    std::array<char, 4096> workBuffer_;
    
public:
    void processEvent(const char* data, size_t len) {
        // Use stack buffer if possible
        if (len <= workBuffer_.size()) {
            std::memcpy(workBuffer_.data(), data, len);
            doWork(workBuffer_.data(), len);
        } else {
            // Fall back to heap for large events
            std::vector<char> heapBuffer(data, data + len);
            doWork(heapBuffer.data(), len);
        }
    }
    
private:
    void doWork(char* data, size_t len) {
        // Process data...
    }
};

int main() {
    std::cout << "=== Allocation Benchmark ===\n";
    benchmarkAllocation();
    
    std::cout << "\n=== Hybrid Buffer ===\n";
    HybridBuffer<int> small(100);
    HybridBuffer<int> large(1000);
    std::cout << "Small (100): on heap = " << small.isOnHeap() << "\n";
    std::cout << "Large (1000): on heap = " << large.isOnHeap() << "\n";
    
    std::cout << "\n=== Object Pool ===\n";
    ObjectPool<std::string, 8> pool;
    std::vector<std::string*> ptrs;
    for (int i = 0; i < 5; ++i) {
        auto* s = pool.allocate("String " + std::to_string(i));
        if (s) {
            std::cout << "Allocated: " << *s << "\n";
            ptrs.push_back(s);
        }
    }
    for (auto* p : ptrs) {
        pool.deallocate(p);
    }
    std::cout << "All deallocated\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Stack overflow

```cpp
void recursive(int depth) {
    char buffer[10000];  // 10KB per call
    if (depth > 0) recursive(depth - 1);
}
recursive(1000);  // 10MB stack usage - CRASH!

// FIX: Use heap for large buffers in recursive functions
void recursive(int depth) {
    auto buffer = std::make_unique<char[]>(10000);
    if (depth > 0) recursive(depth - 1);
}
```

### Mistake 2: Returning stack address

```cpp
int* bad() {
    int x = 42;
    return &x;  // Dangling pointer!
}

// FIX: Return by value or use heap
int good1() { return 42; }
std::unique_ptr<int> good2() { return std::make_unique<int>(42); }
```

### Mistake 3: VLA (Variable Length Array)

```cpp
void dangerous(size_t n) {
    int arr[n];  // VLA - non-standard in C++, stack overflow risk
}

// FIX: Use vector or hybrid approach
void safe(size_t n) {
    std::vector<int> arr(n);
}
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              STACK VS HEAP DECISION GUIDE                         |
+------------------------------------------------------------------+
|                                                                  |
|  USE STACK WHEN:                                                 |
|    □ Size known at compile time                                  |
|    □ Object fits in a few KB                                     |
|    □ Lifetime is function scope                                  |
|    □ Performance is critical                                     |
|                                                                  |
|  USE HEAP WHEN:                                                  |
|    □ Size determined at runtime                                  |
|    □ Object is large (>KB)                                       |
|    □ Lifetime exceeds scope                                      |
|    □ Need to return ownership                                    |
|                                                                  |
|  OPTIMIZATION TECHNIQUES:                                        |
|    □ Small buffer optimization (SBO)                             |
|    □ Object pools for frequent alloc/dealloc                     |
|    □ Reserve capacity in vectors                                 |
|    □ Reuse buffers across operations                             |
|                                                                  |
+------------------------------------------------------------------+
```

