# Topic 16: Atomics and Memory Ordering

## 1. Problem Statement

### What real engineering problem does this solve?

Atomic operations provide thread-safe access without locks:

```
REGULAR VARIABLE                     ATOMIC VARIABLE
┌─────────────────┐                  ┌─────────────────┐
│ int x = 0;      │                  │ atomic<int> x{0}│
│                 │                  │                 │
│ Thread 1: x++   │                  │ Thread 1: x++   │
│ Thread 2: x++   │                  │ Thread 2: x++   │
│                 │                  │                 │
│ Result: 0,1,2?  │                  │ Result: always 2│
│ (DATA RACE!)    │                  │ (well-defined)  │
└─────────────────┘                  └─────────────────┘
```

### When atomics vs mutexes?

- **Atomics**: Simple operations (counter, flag, pointer swap)
- **Mutexes**: Complex operations (multiple values, invariants)

**中文说明：**
原子操作是不可分割的操作——执行过程中不会被其他线程打断。std::atomic 提供线程安全的读写，无需锁。适用于简单的计数器、标志位、指针交换等场景。对于需要同时修改多个相关值的操作，仍需使用互斥锁。

---

## 2. Core Idea

### Memory Orderings

```
┌─────────────────────────────────────────────────────────────────┐
│ ORDERING          │ GUARANTEES                                  │
├─────────────────────────────────────────────────────────────────┤
│ relaxed           │ Atomicity only, no ordering                 │
│                   │ Use for: counters where order doesn't matter│
├─────────────────────────────────────────────────────────────────┤
│ acquire           │ Reads after cannot move before this load    │
│                   │ Use for: acquiring lock, reading flag       │
├─────────────────────────────────────────────────────────────────┤
│ release           │ Writes before cannot move after this store  │
│                   │ Use for: releasing lock, setting flag       │
├─────────────────────────────────────────────────────────────────┤
│ acq_rel           │ Both acquire and release                    │
│                   │ Use for: read-modify-write operations       │
├─────────────────────────────────────────────────────────────────┤
│ seq_cst           │ Total ordering across all threads (default) │
│                   │ Use for: when unsure, safest choice         │
└─────────────────────────────────────────────────────────────────┘
```

### Common Patterns

```cpp
// Pattern 1: Simple counter (relaxed is fine)
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);

// Pattern 2: Flag signaling (release-acquire)
std::atomic<bool> ready{false};
// Producer: ready.store(true, std::memory_order_release);
// Consumer: while (!ready.load(std::memory_order_acquire));

// Pattern 3: Lock-free stack (acq_rel for CAS)
head.compare_exchange_weak(old, new_node, 
    std::memory_order_acq_rel);
```

**中文说明：**
内存序控制编译器和 CPU 如何重排指令：
- **relaxed**：只保证原子性，适合独立的计数器
- **acquire**：阻止后续读取移动到此之前（"获取"数据）
- **release**：阻止之前写入移动到此之后（"发布"数据）
- **seq_cst**：最强保证，全局顺序一致（默认，最安全）

---

## 3. Idiomatic C++ Techniques

### Basic Atomic Operations

```cpp
#include <atomic>

std::atomic<int> x{0};

// Load and store
int val = x.load();           // Read
x.store(42);                  // Write

// Read-modify-write
x.fetch_add(1);               // Returns old value
x.fetch_sub(1);
x.fetch_and(mask);
x.fetch_or(mask);

// Compare-and-swap (CAS)
int expected = 0;
bool success = x.compare_exchange_strong(expected, 42);
// If x == expected: x = 42, return true
// Else: expected = x, return false
```

### Atomic Flag (Simplest Atomic)

```cpp
std::atomic_flag flag = ATOMIC_FLAG_INIT;

// Spinlock
while (flag.test_and_set(std::memory_order_acquire)) {
    // Spin
}
// Critical section
flag.clear(std::memory_order_release);
```

### Atomic Pointer

```cpp
std::atomic<Node*> head{nullptr};

// Lock-free push
void push(Node* new_node) {
    new_node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(
            new_node->next, new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
}
```

---

## 4. Complete C++ Example

```cpp
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

// ============================================================
// Example 1: Thread-safe counter
// ============================================================
class AtomicCounter {
    std::atomic<int> count_{0};
    
public:
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    int get() const {
        return count_.load(std::memory_order_relaxed);
    }
};

// ============================================================
// Example 2: Producer-consumer flag
// ============================================================
class DataChannel {
    std::atomic<bool> ready_{false};
    int data_ = 0;
    
public:
    void produce(int value) {
        data_ = value;  // Non-atomic write
        ready_.store(true, std::memory_order_release);  // Release
    }
    
    int consume() {
        while (!ready_.load(std::memory_order_acquire));  // Acquire
        return data_;  // Guaranteed to see updated data_
    }
};

// ============================================================
// Example 3: Spinlock
// ============================================================
class Spinlock {
    std::atomic_flag locked_ = ATOMIC_FLAG_INIT;
    
public:
    void lock() {
        while (locked_.test_and_set(std::memory_order_acquire)) {
            // Spin - could add pause instruction for efficiency
        }
    }
    
    void unlock() {
        locked_.clear(std::memory_order_release);
    }
};

// ============================================================
// Example 4: Lock-free stack (simplified)
// ============================================================
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
        Node(T d) : data(std::move(d)), next(nullptr) {}
    };
    
    std::atomic<Node*> head_{nullptr};
    
public:
    void push(T data) {
        Node* new_node = new Node(std::move(data));
        new_node->next = head_.load(std::memory_order_relaxed);
        
        // CAS loop
        while (!head_.compare_exchange_weak(
                new_node->next, new_node,
                std::memory_order_release,
                std::memory_order_relaxed)) {
            // new_node->next updated by CAS on failure
        }
    }
    
    bool pop(T& result) {
        Node* old_head = head_.load(std::memory_order_acquire);
        
        while (old_head && !head_.compare_exchange_weak(
                old_head, old_head->next,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            // old_head updated by CAS on failure
        }
        
        if (!old_head) return false;
        
        result = std::move(old_head->data);
        delete old_head;  // Note: ABA problem not handled here
        return true;
    }
    
    ~LockFreeStack() {
        T dummy;
        while (pop(dummy));
    }
};

// ============================================================
// Example 5: Compare memory orderings
// ============================================================
void compareOrderings() {
    constexpr int N = 10'000'000;
    
    // Relaxed
    std::atomic<int> relaxed{0};
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        relaxed.fetch_add(1, std::memory_order_relaxed);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // Seq_cst
    std::atomic<int> seqcst{0};
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        seqcst.fetch_add(1, std::memory_order_seq_cst);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto ns1 = std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - start1).count();
    auto ns2 = std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
    
    std::cout << "Relaxed: " << ns1 / N << " ns/op\n";
    std::cout << "Seq_cst: " << ns2 / N << " ns/op\n";
}

int main() {
    std::cout << "=== Atomic Counter ===\n";
    AtomicCounter counter;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100000; ++j) {
                counter.increment();
            }
        });
    }
    
    for (auto& t : threads) t.join();
    std::cout << "Counter: " << counter.get() << " (expected 400000)\n";
    
    std::cout << "\n=== Producer-Consumer ===\n";
    DataChannel channel;
    
    std::thread producer([&]() {
        channel.produce(42);
    });
    
    std::thread consumer([&]() {
        int value = channel.consume();
        std::cout << "Received: " << value << "\n";
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "\n=== Lock-Free Stack ===\n";
    LockFreeStack<int> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);
    
    int val;
    while (stack.pop(val)) {
        std::cout << "Popped: " << val << "\n";
    }
    
    std::cout << "\n=== Memory Ordering Comparison ===\n";
    compareOrderings();
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Non-atomic access to shared data

```cpp
int data = 0;  // Not atomic!
std::atomic<bool> ready{false};

// Thread 1
data = 42;
ready.store(true, std::memory_order_release);

// Thread 2 - WRONG
while (!ready.load(std::memory_order_acquire));
data = data + 1;  // Data race! data is not atomic
```

### Mistake 2: Weak CAS retry bug

```cpp
// WRONG: Not handling spurious failure correctly
if (x.compare_exchange_weak(expected, desired)) {
    // Success
}
// What if it was spurious failure? expected is modified!

// CORRECT: Loop or use strong
while (!x.compare_exchange_weak(expected, desired)) {
    // expected is updated, decide whether to retry
}
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              ATOMIC OPERATIONS CHEAT SHEET                        |
+------------------------------------------------------------------+
|                                                                  |
|  OPERATION              │ TYPICAL ORDERING                       |
|  ───────────────────────│────────────────────────────────────────|
|  Counter increment      │ relaxed                                |
|  Publish flag           │ release (store)                        |
|  Check flag             │ acquire (load)                         |
|  CAS for data structure │ acq_rel (success), acquire (failure)   |
|  When unsure            │ seq_cst (default)                      |
|                                                                  |
|  GUIDELINES:                                                     |
|  ─────────────────────────────────────────────────────────────── |
|  • Start with seq_cst, optimize only if needed                   |
|  • Pair release stores with acquire loads                        |
|  • Use relaxed only when order truly doesn't matter              |
|  • Protect non-atomic data with atomic flag + proper ordering    |
|                                                                  |
+------------------------------------------------------------------+
```

