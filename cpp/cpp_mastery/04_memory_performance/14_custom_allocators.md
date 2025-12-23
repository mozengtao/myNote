# Topic 14: Custom Allocators and std::pmr

## 1. Problem Statement

### What real engineering problem does this solve?

Default heap allocation (malloc/new) has limitations:

```
DEFAULT ALLOCATOR ISSUES:
┌─────────────────────────────────────────────────────────────────┐
│ • Global lock contention in multithreaded code                  │
│ • Fragmentation over time                                       │
│ • Unpredictable latency (system calls)                          │
│ • No memory locality control                                    │
│ • Cannot use special memory (GPU, shared, etc.)                 │
└─────────────────────────────────────────────────────────────────┘
```

### What goes wrong without custom allocators?

```cpp
// Real-time audio processing - can't have unpredictable allocs
void audioCallback() {
    std::vector<float> buffer(1024);  // malloc might block!
}

// Game engine - frame-local allocations add up
void renderFrame() {
    for (auto& obj : objects) {
        auto temp = std::make_unique<TempData>();  // Thousands of allocs per frame!
    }
}
```

**中文说明：**
默认分配器使用全局堆，有锁竞争、不可预测延迟、碎片化等问题。自定义分配器允许：使用预分配内存池（实时系统）、帧/区域分配器（游戏引擎）、线程本地分配（避免锁）、特殊内存（GPU/共享内存）。

---

## 2. Core Idea

### Allocator Types

```
┌─────────────────────────────────────────────────────────────────┐
│ POOL ALLOCATOR                                                  │
│ • Fixed-size blocks                                             │
│ • O(1) alloc/dealloc                                            │
│ • No fragmentation                                              │
│ • Good for: frequent same-size allocs                           │
├─────────────────────────────────────────────────────────────────┤
│ ARENA / REGION ALLOCATOR                                        │
│ • Bump pointer allocation                                       │
│ • O(1) alloc, bulk dealloc only                                 │
│ • Fast, no fragmentation                                        │
│ • Good for: temporary/frame-local data                          │
├─────────────────────────────────────────────────────────────────┤
│ STACK ALLOCATOR                                                 │
│ • LIFO allocation                                               │
│ • O(1) alloc/dealloc                                            │
│ • Must deallocate in reverse order                              │
│ • Good for: nested scopes                                       │
└─────────────────────────────────────────────────────────────────┘
```

### std::pmr (Polymorphic Memory Resources)

```cpp
#include <memory_resource>

// Memory resource (allocator backend)
std::pmr::monotonic_buffer_resource pool{1024};

// Containers use the resource
std::pmr::vector<int> vec{&pool};
std::pmr::string str{&pool};

// All allocations come from pool
vec.push_back(1);
vec.push_back(2);
// When pool is destroyed, all memory is freed at once
```

**中文说明：**
std::pmr（多态内存资源）是 C++17 引入的标准化分配器接口。memory_resource 是分配策略的抽象，pmr 容器可以使用任何 memory_resource。优点是运行时多态（无需模板）和标准化的分配器链。

---

## 3. Idiomatic C++ Techniques

### Using std::pmr

```cpp
#include <memory_resource>
#include <vector>
#include <string>

void example() {
    // Stack buffer for small allocations
    char buffer[1024];
    std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};
    
    // All containers share the pool
    std::pmr::vector<std::pmr::string> names{&pool};
    names.push_back("Alice");
    names.push_back("Bob");
    
    // When function exits, everything is freed
}

// Unsynchronized pool for single-threaded use
std::pmr::unsynchronized_pool_resource singleThreadPool;

// Synchronized pool for multi-threaded use
std::pmr::synchronized_pool_resource multiThreadPool;
```

### Traditional Allocator

```cpp
template<typename T>
class PoolAllocator {
    using value_type = T;
    
    MemoryPool* pool_;
    
public:
    PoolAllocator(MemoryPool* pool) : pool_(pool) {}
    
    T* allocate(size_t n) {
        return static_cast<T*>(pool_->allocate(n * sizeof(T), alignof(T)));
    }
    
    void deallocate(T* p, size_t n) {
        pool_->deallocate(p, n * sizeof(T));
    }
    
    // Required for rebind
    template<typename U>
    struct rebind { using other = PoolAllocator<U>; };
};

// Usage
MemoryPool pool;
std::vector<int, PoolAllocator<int>> vec(PoolAllocator<int>{&pool});
```

---

## 4. Complete C++ Example

```cpp
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory_resource>
#include <vector>

// ============================================================
// Simple Arena Allocator
// ============================================================
class ArenaAllocator : public std::pmr::memory_resource {
    char* buffer_;
    size_t size_;
    size_t offset_ = 0;
    
protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        // Align offset
        size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned + bytes > size_) {
            throw std::bad_alloc();
        }
        void* ptr = buffer_ + aligned;
        offset_ = aligned + bytes;
        return ptr;
    }
    
    void do_deallocate(void*, size_t, size_t) override {
        // Arena doesn't deallocate individual objects
    }
    
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
    
public:
    ArenaAllocator(char* buffer, size_t size) 
        : buffer_(buffer), size_(size) {}
    
    void reset() { offset_ = 0; }
    size_t used() const { return offset_; }
};

// ============================================================
// Fixed-Size Pool Allocator
// ============================================================
template<size_t BlockSize, size_t NumBlocks>
class FixedPool : public std::pmr::memory_resource {
    union Block {
        char data[BlockSize];
        Block* next;
    };
    
    std::array<Block, NumBlocks> blocks_;
    Block* freeList_ = nullptr;
    
protected:
    void* do_allocate(size_t bytes, size_t) override {
        if (bytes > BlockSize || !freeList_) {
            throw std::bad_alloc();
        }
        Block* block = freeList_;
        freeList_ = block->next;
        return block->data;
    }
    
    void do_deallocate(void* p, size_t, size_t) override {
        Block* block = reinterpret_cast<Block*>(p);
        block->next = freeList_;
        freeList_ = block;
    }
    
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
    
public:
    FixedPool() {
        for (size_t i = 0; i < NumBlocks - 1; ++i) {
            blocks_[i].next = &blocks_[i + 1];
        }
        blocks_[NumBlocks - 1].next = nullptr;
        freeList_ = &blocks_[0];
    }
};

// ============================================================
// Benchmark: Default vs Custom Allocator
// ============================================================
void benchmarkAllocators() {
    constexpr int N = 100000;
    
    // Default allocator
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        std::vector<int> v;
        for (int j = 0; j < 100; ++j) {
            v.push_back(j);
        }
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // PMR with monotonic buffer
    auto start2 = std::chrono::high_resolution_clock::now();
    char buffer[1024 * 1024];  // 1MB buffer
    for (int i = 0; i < N; ++i) {
        std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};
        std::pmr::vector<int> v{&pool};
        for (int j = 0; j < 100; ++j) {
            v.push_back(j);
        }
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto us1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    auto us2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();
    
    std::cout << "Default allocator: " << us1 << " us\n";
    std::cout << "Monotonic buffer:  " << us2 << " us\n";
    std::cout << "Speedup: " << (double)us1 / us2 << "x\n";
}

// ============================================================
// Real-world pattern: Frame allocator for games
// ============================================================
class FrameAllocator {
    std::array<char, 1024 * 1024> buffer_;  // 1MB per frame
    ArenaAllocator arena_;
    
public:
    FrameAllocator() : arena_(buffer_.data(), buffer_.size()) {}
    
    std::pmr::memory_resource* resource() { return &arena_; }
    
    void beginFrame() {
        arena_.reset();  // Free all previous frame allocations
    }
    
    size_t usedThisFrame() const { return arena_.used(); }
};

void gameLoop() {
    FrameAllocator frameAlloc;
    
    for (int frame = 0; frame < 3; ++frame) {
        frameAlloc.beginFrame();
        
        // All allocations this frame use arena
        std::pmr::vector<int> tempData{frameAlloc.resource()};
        for (int i = 0; i < 1000; ++i) {
            tempData.push_back(i);
        }
        
        std::cout << "Frame " << frame << ": used " 
                  << frameAlloc.usedThisFrame() << " bytes\n";
        
        // End of frame - all memory automatically reused
    }
}

int main() {
    std::cout << "=== Allocator Benchmark ===\n";
    benchmarkAllocators();
    
    std::cout << "\n=== Arena Allocator ===\n";
    char buffer[4096];
    ArenaAllocator arena(buffer, sizeof(buffer));
    
    std::pmr::vector<int> v1{&arena};
    std::pmr::vector<double> v2{&arena};
    
    v1.resize(100);
    v2.resize(50);
    
    std::cout << "Used: " << arena.used() << " bytes\n";
    
    arena.reset();
    std::cout << "After reset: " << arena.used() << " bytes\n";
    
    std::cout << "\n=== Frame Allocator Pattern ===\n";
    gameLoop();
    
    std::cout << "\n=== Fixed Pool ===\n";
    FixedPool<64, 100> pool;
    std::pmr::vector<std::pmr::string> strings{&pool};
    strings.push_back("Hello");
    strings.push_back("World");
    std::cout << "Strings from pool: " << strings[0] << " " << strings[1] << "\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Using deallocated memory resource

```cpp
std::pmr::vector<int>* createVector() {
    char buffer[1024];
    std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};
    auto* v = new std::pmr::vector<int>{&pool};
    v->push_back(1);
    return v;  // BUG: pool dies, vector uses dangling memory!
}
```

### Mistake 2: Memory resource lifetime

```cpp
std::pmr::vector<int> v;
{
    std::pmr::monotonic_buffer_resource pool{1024};
    v = std::pmr::vector<int>{&pool};
    v.push_back(1);
}  // pool destroyed!
v.push_back(2);  // Undefined behavior!
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              CUSTOM ALLOCATOR DECISION GUIDE                      |
+------------------------------------------------------------------+
|                                                                  |
|  USE std::pmr::monotonic_buffer_resource WHEN:                   |
|    □ Allocations are temporary (frame, request, etc.)            |
|    □ Bulk deallocation is acceptable                             |
|    □ Want simplest custom allocation                             |
|                                                                  |
|  USE std::pmr::pool_resource WHEN:                               |
|    □ Need individual deallocation                                |
|    □ Objects have varying sizes                                  |
|    □ Want pooling benefits                                       |
|                                                                  |
|  USE CUSTOM ALLOCATOR WHEN:                                      |
|    □ Need special memory (GPU, shared, mmap)                     |
|    □ Need tracking/debugging                                     |
|    □ Real-time constraints (no syscalls)                         |
|                                                                  |
+------------------------------------------------------------------+
```

