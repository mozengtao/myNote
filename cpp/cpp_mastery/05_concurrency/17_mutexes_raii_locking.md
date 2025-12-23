# Topic 17: Mutexes and RAII-based Locking

## 1. Problem Statement

### What real engineering problem does this solve?

Concurrent access to shared data causes data races:

```
THREAD 1                    THREAD 2                    RESULT
─────────────────────────────────────────────────────────────────
read counter (= 5)          read counter (= 5)          
counter = 5 + 1             counter = 5 + 1             
write counter (= 6)         write counter (= 6)         counter = 6
                                                        (should be 7!)
```

Mutexes solve this, but manual lock/unlock is error-prone:

```cpp
// DISASTER: Forgetting to unlock
void bad() {
    mutex.lock();
    if (error) return;  // BUG: mutex still locked!
    // ...
    mutex.unlock();
}

// DISASTER: Exception leaves mutex locked
void worse() {
    mutex.lock();
    riskyOperation();  // May throw - mutex stays locked!
    mutex.unlock();
}
```

**中文说明：**
多线程程序中，多个线程同时读写共享数据会导致数据竞争。互斥锁（mutex）可以保护共享数据，但手动 lock/unlock 在复杂控制流（early return、异常）中极易出错，导致死锁。RAII 锁定（如 lock_guard）通过对象生命周期自动管理锁的获取和释放。

---

## 2. Core Idea

### RAII Locking

```
MANUAL LOCKING                       RAII LOCKING
┌─────────────────────┐              ┌─────────────────────┐
│ mutex.lock();       │              │ lock_guard lk(mtx); │
│ // critical section │              │ // critical section │
│ // must remember... │              │                     │
│ mutex.unlock();     │              │ } // auto unlock    │
└─────────────────────┘              └─────────────────────┘
       ↓                                    ↓
  Error prone                         Exception safe
  Forget unlock = deadlock            Always unlocks
  Exception = deadlock                Even on exception
```

### Lock Types in C++

```cpp
// C++11
std::mutex                    // Basic mutual exclusion
std::recursive_mutex          // Same thread can lock multiple times
std::timed_mutex              // Lock with timeout
std::lock_guard<Mutex>        // Simple RAII wrapper, non-movable
std::unique_lock<Mutex>       // Flexible RAII wrapper, movable

// C++14
std::shared_mutex             // Reader-writer lock
std::shared_lock<Mutex>       // Shared (read) lock

// C++17
std::scoped_lock<Mutexes...>  // Lock multiple mutexes, deadlock-free
```

**中文说明：**
C++ 提供多种锁类型：
- **lock_guard**: 最简单，构造时锁定，析构时解锁，不可移动
- **unique_lock**: 更灵活，可移动，可手动解锁，支持条件变量
- **scoped_lock**: C++17，可同时锁定多个互斥锁，避免死锁
- **shared_lock**: 读写锁的读锁，允许多个读者同时访问

---

## 3. Idiomatic C++ Techniques

### Basic Locking Pattern

```cpp
class ThreadSafeCounter {
    mutable std::mutex mtx_;  // mutable for const methods
    int count_ = 0;
    
public:
    void increment() {
        std::lock_guard<std::mutex> lock(mtx_);
        ++count_;
    }  // Automatically unlocked here
    
    int get() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_;
    }
};
```

### Multiple Mutex Locking (Avoiding Deadlock)

```cpp
// WRONG: Can deadlock
void transfer(Account& from, Account& to, int amount) {
    std::lock_guard<std::mutex> lock1(from.mutex_);  // Lock A
    std::lock_guard<std::mutex> lock2(to.mutex_);    // Lock B
    // If another thread does transfer(to, from), DEADLOCK!
}

// CORRECT C++17: scoped_lock acquires in consistent order
void transfer(Account& from, Account& to, int amount) {
    std::scoped_lock lock(from.mutex_, to.mutex_);  // Deadlock-free
    from.balance_ -= amount;
    to.balance_ += amount;
}

// CORRECT C++11: std::lock + adopt_lock
void transfer(Account& from, Account& to, int amount) {
    std::lock(from.mutex_, to.mutex_);  // Lock both atomically
    std::lock_guard<std::mutex> lock1(from.mutex_, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(to.mutex_, std::adopt_lock);
    from.balance_ -= amount;
    to.balance_ += amount;
}
```

### Reader-Writer Lock

```cpp
class Config {
    mutable std::shared_mutex mtx_;
    std::map<std::string, std::string> data_;
    
public:
    // Multiple readers allowed
    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mtx_);  // Read lock
        auto it = data_.find(key);
        return it != data_.end() ? it->second : "";
    }
    
    // Exclusive writer
    void set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mtx_);  // Write lock
        data_[key] = value;
    }
};
```

---

## 4. Complete C++ Example

```cpp
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

// ============================================================
// Example 1: Basic thread-safe queue
// ============================================================
template<typename T>
class ThreadSafeQueue {
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool closed_ = false;
    
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_) return;
            queue_.push(std::move(value));
        }  // Unlock before notify (optimization)
        cv_.notify_one();
    }
    
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { 
            return !queue_.empty() || closed_; 
        });
        
        if (queue_.empty()) return false;  // Closed and empty
        
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            closed_ = true;
        }
        cv_.notify_all();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }
};

// ============================================================
// Example 2: Reader-writer protected cache
// ============================================================
class Cache {
    mutable std::shared_mutex mtx_;
    std::unordered_map<int, std::string> data_;
    
public:
    // Multiple threads can read simultaneously
    std::optional<std::string> get(int key) const {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // Only one thread can write
    void put(int key, std::string value) {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        data_[key] = std::move(value);
    }
    
    // Upgrade pattern: read then write if needed
    std::string getOrCompute(int key) {
        // Try read lock first
        {
            std::shared_lock<std::shared_mutex> readLock(mtx_);
            auto it = data_.find(key);
            if (it != data_.end()) {
                return it->second;  // Fast path: already cached
            }
        }
        
        // Upgrade to write lock
        std::unique_lock<std::shared_mutex> writeLock(mtx_);
        // Double-check: another thread may have computed
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        
        // Compute and cache
        std::string result = "computed_" + std::to_string(key);
        data_[key] = result;
        return result;
    }
};

// ============================================================
// Example 3: Lock-free observation (minimizing lock scope)
// ============================================================
class Statistics {
    mutable std::mutex mtx_;
    int count_ = 0;
    double sum_ = 0;
    double sumSquares_ = 0;
    
public:
    void record(double value) {
        std::lock_guard<std::mutex> lock(mtx_);
        ++count_;
        sum_ += value;
        sumSquares_ += value * value;
    }
    
    // Return a snapshot - lock only for copy
    struct Snapshot {
        int count;
        double mean;
        double variance;
    };
    
    Snapshot getSnapshot() const {
        std::lock_guard<std::mutex> lock(mtx_);
        Snapshot snap;
        snap.count = count_;
        if (count_ > 0) {
            snap.mean = sum_ / count_;
            snap.variance = (sumSquares_ / count_) - (snap.mean * snap.mean);
        } else {
            snap.mean = 0;
            snap.variance = 0;
        }
        return snap;
    }  // Unlock, then caller can use snapshot freely
};

// ============================================================
// Example 4: Multiple mutex with scoped_lock
// ============================================================
class BankAccount {
public:
    std::mutex mtx;
    int balance = 0;
    std::string name;
    
    BankAccount(std::string n, int b) : balance(b), name(std::move(n)) {}
};

void transfer(BankAccount& from, BankAccount& to, int amount) {
    // C++17 scoped_lock handles deadlock avoidance
    std::scoped_lock lock(from.mtx, to.mtx);
    
    if (from.balance >= amount) {
        from.balance -= amount;
        to.balance += amount;
        std::cout << "Transferred " << amount << " from " 
                  << from.name << " to " << to.name << "\n";
    }
}

// ============================================================
// Example 5: Condition variable with unique_lock
// ============================================================
class Semaphore {
    std::mutex mtx_;
    std::condition_variable cv_;
    int count_;
    
public:
    explicit Semaphore(int initial = 0) : count_(initial) {}
    
    void acquire() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return count_ > 0; });
        --count_;
    }
    
    bool try_acquire_for(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!cv_.wait_for(lock, timeout, [this] { return count_ > 0; })) {
            return false;  // Timeout
        }
        --count_;
        return true;
    }
    
    void release() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            ++count_;
        }
        cv_.notify_one();
    }
};

// ============================================================
// Demonstration
// ============================================================
int main() {
    // Test 1: Thread-safe queue
    std::cout << "=== Thread-Safe Queue ===\n";
    ThreadSafeQueue<int> queue;
    
    std::thread producer([&queue] {
        for (int i = 0; i < 5; ++i) {
            queue.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        queue.close();
    });
    
    std::thread consumer([&queue] {
        int value;
        while (queue.pop(value)) {
            std::cout << "Consumed: " << value << "\n";
        }
    });
    
    producer.join();
    consumer.join();
    
    // Test 2: Bank transfer (deadlock-free)
    std::cout << "\n=== Bank Transfer ===\n";
    BankAccount alice("Alice", 1000);
    BankAccount bob("Bob", 500);
    
    std::thread t1([&] {
        for (int i = 0; i < 3; ++i) {
            transfer(alice, bob, 100);
        }
    });
    
    std::thread t2([&] {
        for (int i = 0; i < 3; ++i) {
            transfer(bob, alice, 50);
        }
    });
    
    t1.join();
    t2.join();
    
    std::cout << "Alice: " << alice.balance << ", Bob: " << bob.balance << "\n";
    
    // Test 3: Reader-writer cache
    std::cout << "\n=== Reader-Writer Cache ===\n";
    Cache cache;
    
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; ++i) {
        readers.emplace_back([&cache, i] {
            for (int j = 0; j < 5; ++j) {
                auto result = cache.getOrCompute(j);
                std::cout << "Reader " << i << " got: " << result << "\n";
            }
        });
    }
    
    for (auto& t : readers) t.join();
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Lock scope too large

```cpp
// BAD: Holds lock during slow I/O
void bad() {
    std::lock_guard<std::mutex> lock(mtx_);
    data_.push_back(value);
    sendToNetwork(value);  // Slow! Blocks other threads
}

// GOOD: Minimize critical section
void good() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        data_.push_back(value);
    }  // Unlock before I/O
    sendToNetwork(value);  // Other threads can proceed
}
```

### Mistake 2: Deadlock from lock ordering

```cpp
// Thread 1                 Thread 2
lock(mutexA);              lock(mutexB);
lock(mutexB);  // Wait     lock(mutexA);  // Wait
// DEADLOCK!

// FIX: Always lock in consistent order, or use std::scoped_lock
```

### Mistake 3: Forgetting mutable for const methods

```cpp
class Counter {
    std::mutex mtx_;  // NOT mutable
    int count_ = 0;
    
public:
    int get() const {
        std::lock_guard<std::mutex> lock(mtx_);  // ERROR: mtx_ is const!
        return count_;
    }
};

// FIX: mutable std::mutex mtx_;
```

### Mistake 4: Calling unknown code while holding lock

```cpp
void dangerous() {
    std::lock_guard<std::mutex> lock(mtx_);
    callback_();  // What if callback tries to acquire same lock?
                  // Or calls back into our code?
}

// FIX: Copy data, release lock, then call
void safe() {
    Data copy;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        copy = data_;
    }
    callback_(copy);  // No lock held
}
```

---

## 6. When NOT to Use Mutexes

### Alternatives to Consider

| Scenario | Better Alternative |
|----------|-------------------|
| Single writer, many readers | `shared_mutex` or RCU |
| Simple counters | `std::atomic<int>` |
| Lock-free needed | Atomic operations |
| No shared state | Thread-local storage |
| Async processing | Message queues |

### Cost Awareness

```
MUTEX COST:
┌─────────────────────────────────────────────────────────────────┐
│ • Uncontended lock: ~25-50 ns (fast)                            │
│ • Contended lock: ~1000+ ns + context switch (slow)             │
│ • Memory barrier effects on all cores                           │
│ • Priority inversion possible                                   │
└─────────────────────────────────────────────────────────────────┘

CONSIDER ALTERNATIVES:
┌─────────────────────────────────────────────────────────────────┐
│ • std::atomic for simple types (often lock-free)                │
│ • Thread-local storage if sharing not needed                    │
│ • Immutable data structures (no locks needed)                   │
│ • Actor model / message passing                                 │
└─────────────────────────────────────────────────────────────────┘
```

**中文说明：**
互斥锁不是唯一的同步手段：
- **无竞争时**锁很快（~25ns），但有竞争时会导致上下文切换和等待
- **简单计数器**用 std::atomic 更高效
- **读多写少**用 shared_mutex 允许并发读
- **无共享状态**时用线程局部存储完全避免同步

---

## Summary

```
+------------------------------------------------------------------+
|                    MUTEX BEST PRACTICES                           |
+------------------------------------------------------------------+
|                                                                  |
|  1. ALWAYS use RAII locks (lock_guard, scoped_lock)              |
|     Never call lock()/unlock() directly                          |
|                                                                  |
|  2. MINIMIZE lock scope                                          |
|     Release lock before slow operations                          |
|                                                                  |
|  3. AVOID deadlock                                               |
|     Use scoped_lock for multiple mutexes                         |
|     Or always lock in consistent order                           |
|                                                                  |
|  4. Mark mutex mutable for const methods                         |
|                                                                  |
|  5. DON'T call unknown code while holding lock                   |
|                                                                  |
|  6. Consider alternatives: atomic, shared_mutex, lock-free       |
|                                                                  |
+------------------------------------------------------------------+
```

