# Topic 15: C++ Memory Model (happens-before)

## 1. Problem Statement

### What real engineering problem does this solve?

Compilers and CPUs reorder operations for performance:

```
WHAT YOU WRITE:                     WHAT MAY EXECUTE:
x = 1;                              y = 2;  // Reordered!
y = 2;                              x = 1;

Thread 1:          Thread 2:        POSSIBLE OUTCOME:
x = 1;             while (y != 1);  x may still be 0 when
y = 1;             print(x);        Thread 2 exits loop!
```

### What goes wrong without understanding memory model?

```cpp
// Classic data race
int data = 0;
bool ready = false;

// Thread 1
data = 42;
ready = true;

// Thread 2
while (!ready);
use(data);  // May see data == 0!
// Compiler may reorder, CPU may cache, etc.
```

**中文说明：**
编译器和 CPU 为了性能会重排指令、延迟写入、缓存数据。在多线程环境中，没有同步的共享数据可能产生不一致的视图。C++ 内存模型定义了"何时一个线程的写入对另一个线程可见"的规则，通过 happens-before 关系来推理多线程程序的正确性。

---

## 2. Core Idea

### Happens-Before Relationship

```
HAPPENS-BEFORE (→):
If A → B, then effects of A are visible to B

SOURCES OF HAPPENS-BEFORE:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Sequenced-before (same thread)                               │
│    x = 1;  →  y = 2;  (within same thread)                      │
├─────────────────────────────────────────────────────────────────┤
│ 2. Synchronizes-with (between threads)                          │
│    mutex.unlock()  →  mutex.lock()                              │
│    atomic store    →  atomic load (with acquire/release)        │
│    thread create   →  thread start                              │
│    thread end      →  thread join                               │
├─────────────────────────────────────────────────────────────────┤
│ 3. Transitive                                                   │
│    A → B  and  B → C  implies  A → C                            │
└─────────────────────────────────────────────────────────────────┘
```

### Data Race Definition

```
DATA RACE occurs when:
1. Two threads access same memory location
2. At least one is a write
3. No happens-before between them

DATA RACE = UNDEFINED BEHAVIOR
```

**中文说明：**
happens-before 是 C++ 内存模型的核心概念。如果操作 A happens-before 操作 B，那么 A 的效果对 B 可见。没有 happens-before 关系的并发读写是数据竞争，属于未定义行为。同步原语（mutex、atomic）建立线程间的 happens-before 关系。

---

## 3. Idiomatic C++ Techniques

### Mutex Establishes Happens-Before

```cpp
std::mutex mtx;
int data = 0;

// Thread 1
mtx.lock();
data = 42;      // A
mtx.unlock();   // B: Release

// Thread 2
mtx.lock();     // C: Acquire, synchronizes-with B
int x = data;   // D: Sees 42, because A → B → C → D
mtx.unlock();
```

### Atomic with Memory Ordering

```cpp
std::atomic<bool> ready{false};
int data = 0;

// Thread 1
data = 42;                          // A
ready.store(true, std::memory_order_release);  // B: Release

// Thread 2
while (!ready.load(std::memory_order_acquire));  // C: Acquire
int x = data;  // D: Sees 42, because A → B → C → D
```

### Thread Creation/Join

```cpp
int data = 0;

// Main thread
data = 42;                    // A
std::thread t([&]() {
    // A → thread start (happens-before)
    int x = data;             // Sees 42
    data = 100;               // B
});
t.join();                     // B → join (happens-before)
int y = data;                 // Sees 100
```

---

## 4. Complete C++ Example

```cpp
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// ============================================================
// Example 1: Race condition (UNDEFINED BEHAVIOR)
// ============================================================
void demonstrateRace() {
    int counter = 0;  // Non-atomic, no synchronization
    
    auto increment = [&]() {
        for (int i = 0; i < 100000; ++i) {
            ++counter;  // DATA RACE!
        }
    };
    
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
    
    std::cout << "Race result: " << counter 
              << " (expected 200000, undefined behavior)\n";
}

// ============================================================
// Example 2: Fixed with mutex
// ============================================================
void demonstrateMutex() {
    int counter = 0;
    std::mutex mtx;
    
    auto increment = [&]() {
        for (int i = 0; i < 100000; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            ++counter;  // Protected by mutex
        }
    };
    
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
    
    std::cout << "Mutex result: " << counter << " (correct)\n";
}

// ============================================================
// Example 3: Fixed with atomic
// ============================================================
void demonstrateAtomic() {
    std::atomic<int> counter{0};
    
    auto increment = [&]() {
        for (int i = 0; i < 100000; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
    
    std::cout << "Atomic result: " << counter << " (correct)\n";
}

// ============================================================
// Example 4: Release-Acquire pattern
// ============================================================
void demonstrateReleaseAcquire() {
    std::atomic<bool> ready{false};
    int data = 0;
    
    std::thread producer([&]() {
        data = 42;  // Non-atomic write
        ready.store(true, std::memory_order_release);  // Release
    });
    
    std::thread consumer([&]() {
        while (!ready.load(std::memory_order_acquire));  // Acquire
        // Guaranteed to see data = 42
        std::cout << "Consumer sees data: " << data << "\n";
    });
    
    producer.join();
    consumer.join();
}

// ============================================================
// Example 5: Sequential consistency (default)
// ============================================================
void demonstrateSeqCst() {
    std::atomic<int> x{0}, y{0};
    int r1 = 0, r2 = 0;
    
    std::thread t1([&]() {
        x.store(1);  // memory_order_seq_cst by default
        r1 = y.load();
    });
    
    std::thread t2([&]() {
        y.store(1);
        r2 = x.load();
    });
    
    t1.join();
    t2.join();
    
    // With seq_cst, r1 == 0 && r2 == 0 is IMPOSSIBLE
    // (Would require both stores to appear after both loads)
    std::cout << "Seq_cst: r1=" << r1 << ", r2=" << r2 << "\n";
}

// ============================================================
// Example 6: Happens-before chain
// ============================================================
void demonstrateHappensBeforeChain() {
    std::atomic<int> sync{0};
    int a = 0, b = 0, c = 0;
    
    std::thread t1([&]() {
        a = 1;
        sync.store(1, std::memory_order_release);  // Release a
    });
    
    std::thread t2([&]() {
        while (sync.load(std::memory_order_acquire) < 1);  // Acquire a
        b = a + 1;  // Sees a = 1
        sync.store(2, std::memory_order_release);  // Release b
    });
    
    std::thread t3([&]() {
        while (sync.load(std::memory_order_acquire) < 2);  // Acquire b
        c = b + 1;  // Sees b = 2
    });
    
    t1.join();
    t2.join();
    t3.join();
    
    std::cout << "Chain: a=" << a << ", b=" << b << ", c=" << c << "\n";
}

int main() {
    std::cout << "=== Data Race (UB) ===\n";
    demonstrateRace();
    
    std::cout << "\n=== Mutex Synchronization ===\n";
    demonstrateMutex();
    
    std::cout << "\n=== Atomic Operations ===\n";
    demonstrateAtomic();
    
    std::cout << "\n=== Release-Acquire ===\n";
    demonstrateReleaseAcquire();
    
    std::cout << "\n=== Sequential Consistency ===\n";
    demonstrateSeqCst();
    
    std::cout << "\n=== Happens-Before Chain ===\n";
    demonstrateHappensBeforeChain();
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Assuming order without synchronization

```cpp
bool flag = false;  // Not atomic!
int data = 0;

// Thread 1
data = 42;
flag = true;  // May be reordered before data = 42!

// Thread 2
while (!flag);  // May never see flag = true (optimized to infinite loop)
use(data);      // May see data = 0
```

### Mistake 2: Relaxed ordering when stronger needed

```cpp
std::atomic<bool> ready{false};
int data = 0;

// Thread 1
data = 42;
ready.store(true, std::memory_order_relaxed);  // No ordering guarantee!

// Thread 2
while (!ready.load(std::memory_order_relaxed));
use(data);  // May see data = 0!
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              MEMORY MODEL QUICK REFERENCE                         |
+------------------------------------------------------------------+
|                                                                  |
|  HAPPENS-BEFORE SOURCES:                                         |
|    • Same thread: statement order                                |
|    • Mutex: unlock → lock                                        |
|    • Atomic: release store → acquire load                        |
|    • Thread: create → start, end → join                          |
|                                                                  |
|  MEMORY ORDERS:                                                  |
|    • relaxed: atomicity only, no ordering                        |
|    • acquire: prevents reads moving before                       |
|    • release: prevents writes moving after                       |
|    • acq_rel: both acquire and release                           |
|    • seq_cst: total order (default, safest)                      |
|                                                                  |
|  RULES:                                                          |
|    • Shared non-atomic data needs synchronization                |
|    • Data race = undefined behavior                              |
|    • When in doubt, use mutex or seq_cst atomics                 |
|                                                                  |
+------------------------------------------------------------------+
```

