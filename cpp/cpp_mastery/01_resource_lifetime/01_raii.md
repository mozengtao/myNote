# Topic 1: RAII (Resource Acquisition Is Initialization)

## 1. Problem Statement

### What real engineering problem does this solve?

In systems programming, resources must be acquired (memory, files, locks, sockets, database connections) and later released. The fundamental problem is:

```
+------------------+     +------------------+     +------------------+
| Acquire Resource |---->| Use Resource     |---->| Release Resource |
+------------------+     +------------------+     +------------------+
        ✓                       ?                       ✗ often forgotten!
```

**Without RAII:**
- Resources leak when control flow is complex (early returns, exceptions)
- Manual cleanup code is duplicated and error-prone
- Exception paths often miss cleanup entirely

### What goes wrong if ignored?

```cpp
// DISASTER: Resource leaks everywhere
void processFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;  // OK, nothing to clean
    
    char* buffer = (char*)malloc(1024);
    if (!buffer) {
        // BUG: f is leaked!
        return;
    }
    
    if (someCondition()) {
        // BUG: both f and buffer leaked!
        throw std::runtime_error("oops");
    }
    
    // Must remember ALL cleanup paths
    free(buffer);
    fclose(f);
}
```

**中文说明：**
在 C 语言中，资源管理完全依赖程序员记住在每个退出点释放资源。这在复杂控制流（多个 return、异常、嵌套条件）中极易出错。RAII 通过将资源生命周期绑定到对象生命周期，利用 C++ 的确定性析构来自动管理资源。

---

## 2. Core Idea

### The Principle

**RAII binds resource lifetime to object lifetime:**

```
Object Construction  ←→  Resource Acquisition
Object Destruction   ←→  Resource Release
```

C++ guarantees:
1. **Constructors run when objects are created**
2. **Destructors run when objects go out of scope**
3. **Stack unwinding calls destructors in reverse order**

```
STACK FRAME                          RESOURCE STATE
+------------------------+
| FileHandle f1(path1);  |  ───────► f1 opens file
|   +------------------+ |
|   | Lock lk(mutex);  | |  ───────► lk acquires mutex
|   |   +------------+ | |
|   |   | Buffer buf;| | |  ───────► buf allocates memory
|   |   +------------+ | |
|   |   // work here   | |
|   |   // exception!  | |  ───────► UNWIND STARTS
|   |   +------------+ | |
|   |   | ~Buffer()  | | |  ◄─────── buf frees memory
|   |   +------------+ | |
|   +------------------+ |
|   | ~Lock()          | |  ◄─────── lk releases mutex
|   +------------------+ |
+------------------------+
| ~FileHandle()          |  ◄─────── f1 closes file
+------------------------+
```

**中文说明：**
RAII 的核心是利用 C++ 的作用域规则和确定性析构。当对象离开作用域时（无论是正常退出还是异常退出），析构函数一定会被调用。这意味着只要在构造函数中获取资源、在析构函数中释放资源，资源就永远不会泄漏。

---

## 3. Idiomatic C++ Techniques

### C++ Language Features That Enable RAII

| Feature | Role in RAII |
|---------|--------------|
| Deterministic destructors | Guaranteed cleanup on scope exit |
| Stack allocation | Automatic lifetime management |
| Exception stack unwinding | Cleanup even on exceptions |
| Move semantics | Transfer ownership without copy |

### STL Facilities Supporting RAII

```cpp
// Smart pointers - memory RAII
std::unique_ptr<T>   // Exclusive ownership
std::shared_ptr<T>   // Shared ownership

// Locking - mutex RAII
std::lock_guard<std::mutex>
std::unique_lock<std::mutex>
std::scoped_lock<Mutexes...>  // C++17

// File streams - file handle RAII
std::fstream, std::ifstream, std::ofstream

// Containers - dynamic memory RAII
std::vector, std::string, std::map, ...
```

---

## 4. Complete C++ Example

```cpp
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================
// Example 1: Custom RAII wrapper for C-style FILE*
// ============================================================
class FileHandle {
    FILE* file_;
    
public:
    // Constructor acquires resource
    explicit FileHandle(const char* path, const char* mode)
        : file_(std::fopen(path, mode))
    {
        if (!file_) {
            throw std::runtime_error(
                std::string("Failed to open: ") + path);
        }
    }
    
    // Destructor releases resource
    ~FileHandle() {
        if (file_) {
            std::fclose(file_);
        }
    }
    
    // Non-copyable (resource has single owner)
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    
    // Movable (transfer ownership)
    FileHandle(FileHandle&& other) noexcept 
        : file_(other.file_) 
    {
        other.file_ = nullptr;
    }
    
    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (file_) std::fclose(file_);
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }
    
    // Access the underlying resource
    FILE* get() const noexcept { return file_; }
    
    // Convenience methods
    size_t read(void* buf, size_t size) {
        return std::fread(buf, 1, size, file_);
    }
    
    size_t write(const void* buf, size_t size) {
        return std::fwrite(buf, 1, size, file_);
    }
};

// ============================================================
// Example 2: RAII for database transaction
// ============================================================
class Database {
public:
    void beginTransaction() { /* ... */ }
    void commit() { /* ... */ }
    void rollback() { /* ... */ }
    void execute(const std::string& sql) { /* ... */ }
};

class Transaction {
    Database& db_;
    bool committed_ = false;
    
public:
    explicit Transaction(Database& db) : db_(db) {
        db_.beginTransaction();
    }
    
    ~Transaction() {
        if (!committed_) {
            db_.rollback();  // Automatic rollback on exception
        }
    }
    
    void commit() {
        db_.commit();
        committed_ = true;
    }
    
    // Non-copyable, non-movable (transaction is tied to scope)
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
};

// ============================================================
// Example 3: Generic RAII with custom deleter
// ============================================================
template<typename T, typename Deleter>
class ScopedResource {
    T resource_;
    Deleter deleter_;
    bool active_ = true;
    
public:
    ScopedResource(T resource, Deleter deleter)
        : resource_(resource), deleter_(std::move(deleter)) {}
    
    ~ScopedResource() {
        if (active_) {
            deleter_(resource_);
        }
    }
    
    T get() const noexcept { return resource_; }
    T release() noexcept { active_ = false; return resource_; }
    
    ScopedResource(const ScopedResource&) = delete;
    ScopedResource& operator=(const ScopedResource&) = delete;
};

// Helper function for type deduction
template<typename T, typename Deleter>
ScopedResource<T, Deleter> makeScopedResource(T resource, Deleter deleter) {
    return ScopedResource<T, Deleter>(resource, std::move(deleter));
}

// ============================================================
// Usage demonstration
// ============================================================
void processData(const char* inputPath, const char* outputPath) {
    // RAII: Files automatically closed on any exit
    FileHandle input(inputPath, "rb");
    FileHandle output(outputPath, "wb");
    
    // RAII: Buffer automatically freed
    std::vector<char> buffer(4096);
    
    size_t bytesRead;
    while ((bytesRead = input.read(buffer.data(), buffer.size())) > 0) {
        output.write(buffer.data(), bytesRead);
        
        // Even if this throws, all resources are cleaned up
        if (bytesRead < 100) {
            throw std::runtime_error("Incomplete read");
        }
    }
    // No explicit cleanup needed - destructors handle everything
}

void databaseOperation(Database& db) {
    Transaction txn(db);  // Begin transaction
    
    db.execute("INSERT INTO users ...");
    db.execute("UPDATE accounts ...");
    
    // If anything throws above, transaction rolls back automatically
    
    txn.commit();  // Only if we reach here
}

// Using standard library RAII
void modernStyle() {
    // unique_ptr with custom deleter for C-style resource
    auto file = std::unique_ptr<FILE, decltype(&std::fclose)>(
        std::fopen("data.txt", "r"),
        &std::fclose
    );
    
    if (!file) {
        throw std::runtime_error("Cannot open file");
    }
    
    // Use file...
    // Automatically closed when unique_ptr is destroyed
}

int main() {
    try {
        processData("input.txt", "output.txt");
    } catch (const std::exception& e) {
        // All resources already cleaned up!
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Forgetting to disable copy

```cpp
// WRONG: Copyable RAII leads to double-free
class BadFileHandle {
    FILE* file_;
public:
    BadFileHandle(const char* path) : file_(fopen(path, "r")) {}
    ~BadFileHandle() { if (file_) fclose(file_); }
    // Implicit copy constructor/assignment - DISASTER!
};

void bug() {
    BadFileHandle f1("test.txt");
    BadFileHandle f2 = f1;  // f2.file_ == f1.file_
}   // Both destructors close the same file - DOUBLE FREE!
```

### Mistake 2: Releasing in wrong order

```cpp
// WRONG: Manual release can get order wrong
class BadTransaction {
public:
    void complete() {
        // BUG: If commit() throws, lock is still held
        commit();
        releaseLock();  // Never reached on exception
    }
};

// CORRECT: Use nested RAII
class GoodTransaction {
public:
    void complete() {
        std::lock_guard<std::mutex> lock(mutex_);
        commit();  // If this throws, lock is released by guard
    }
};
```

### Mistake 3: Resource escape

```cpp
// WRONG: Raw pointer escapes RAII control
FILE* leaky() {
    FileHandle fh("test.txt", "r");
    return fh.get();  // BUG: File closed when fh dies, caller has dangling pointer
}

// CORRECT: Transfer ownership
FileHandle correct() {
    FileHandle fh("test.txt", "r");
    return fh;  // Move returns ownership to caller
}
```

---

## 6. When NOT to Use RAII

### When RAII May Not Fit

| Situation | Alternative |
|-----------|-------------|
| Resources with complex lifetime | `shared_ptr` or reference counting |
| Resources shared across threads | `shared_ptr` with careful synchronization |
| Deferred initialization | `optional<T>` or factory functions |
| Resources from C callbacks | Store raw pointer, wrap at boundary |

### Tradeoffs

```
RAII Costs:
┌─────────────────────────────────────────────────────────────┐
│ • Wrapper class boilerplate (mitigated by templates)        │
│ • Exception safety requires noexcept move operations        │
│ • Not suitable for resources with unclear ownership         │
│ • Object lifetime may not match desired resource lifetime   │
└─────────────────────────────────────────────────────────────┘
```

**中文说明：**
RAII 不适用于以下情况：
1. **复杂生命周期**：资源需要在多个不相关的作用域间共享
2. **延迟初始化**：资源需要在对象构造后才获取
3. **跨线程共享**：需要显式的引用计数（使用 shared_ptr）
4. **C 回调**：资源由 C 库管理，只能在边界处包装

---

## Summary

```
+------------------------------------------------------------------+
|                     RAII MENTAL MODEL                             |
+------------------------------------------------------------------+
|                                                                  |
|   Constructor = Acquire     Destructor = Release                 |
|                                                                  |
|   Scope Entry = Birth       Scope Exit = Death                   |
|                                                                  |
|   Stack Unwinding = GUARANTEED Cleanup                           |
|                                                                  |
|   "If it compiles, it cleans up."                                |
|                                                                  |
+------------------------------------------------------------------+
```

**Key Takeaways:**
1. Every resource should have an owner object
2. Acquisition happens in constructor, release in destructor
3. Disable copy for exclusive ownership, enable move for transfer
4. Use `unique_ptr` with custom deleter for C-style resources
5. Never let raw resources escape RAII wrappers

