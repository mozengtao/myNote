# Pattern 24: RAII (Resource Acquisition Is Initialization)

## 1. Problem the Pattern Solves

### Design Pressure
- Resources (memory, files, locks) must be released
- Exception-safe resource management
- Prevent resource leaks in all code paths

### What Goes Wrong Without It
```cpp
void process() {
    File* f = openFile("data.txt");
    // ... processing ...
    if (error) return;  // LEAK: file not closed!
    closeFile(f);
}
```

---

## 2. Core Idea (C++-Specific)

**RAII binds the life cycle of a resource to the lifetime of an object. The constructor acquires, the destructor releases.**

```
Object Creation  ────────────────────►  Object Destruction
       │                                        │
       ▼                                        ▼
   Constructor                             Destructor
   (acquire)                               (release)
```

RAII is **foundational to C++** and enables:
- Automatic cleanup
- Exception safety
- Scope-based resource management

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Destructor | `~Class()` | Auto-called on scope exit |
| `std::unique_ptr` | Memory RAII | Smart pointer |
| `std::lock_guard` | Mutex RAII | Scoped locking |
| `std::fstream` | File RAII | Auto-close |

---

## 4. Canonical C++ Implementation

### Custom RAII Wrapper

```cpp
#include <iostream>

class FileHandle {
public:
    explicit FileHandle(const char* path) {
        file_ = fopen(path, "r");
        if (!file_) throw std::runtime_error("Cannot open file");
        std::cout << "File opened\n";
    }
    
    ~FileHandle() {
        if (file_) {
            fclose(file_);
            std::cout << "File closed\n";
        }
    }
    
    // Non-copyable
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    
    // Movable
    FileHandle(FileHandle&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }
    
    FILE* get() { return file_; }
    
private:
    FILE* file_ = nullptr;
};

void process() {
    FileHandle f("data.txt");
    // Use f.get()...
    if (someCondition) return;  // File still closed!
    // Even if exception thrown, destructor runs
}
```

### Generic Scope Guard

```cpp
#include <functional>

class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> cleanup)
        : cleanup_(std::move(cleanup)) {}
    
    ~ScopeGuard() {
        if (cleanup_) cleanup_();
    }
    
    void dismiss() { cleanup_ = nullptr; }
    
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    
private:
    std::function<void()> cleanup_;
};

// Usage
void example() {
    void* ptr = malloc(100);
    ScopeGuard guard([ptr]() { free(ptr); });
    
    // If exception thrown, ptr still freed
    
    // On success:
    // guard.dismiss();  // Optional: don't cleanup
}
```

---

## 5. Typical Usage

| Resource | RAII Type |
|----------|-----------|
| Memory | `unique_ptr`, `shared_ptr` |
| Mutex | `lock_guard`, `unique_lock` |
| File | `fstream`, custom |
| Socket | Custom wrapper |
| Transaction | Rollback guard |

---

## 6. Common Mistakes

### ❌ Forgetting to Delete Copy Operations

```cpp
// BAD: Copyable RAII = double-free
class Handle {
    int* ptr_;
public:
    Handle() : ptr_(new int) {}
    ~Handle() { delete ptr_; }
    // No deleted copy = bug!
};
Handle a;
Handle b = a;  // Both delete same ptr!
```

---

## 7. Mental Model Summary

**When RAII "Clicks":**

RAII is the **fundamental C++ idiom**. Every resource should be managed by an RAII wrapper. If you're manually calling cleanup code, wrap it in RAII.

---

## 中文说明

### RAII 要点

1. **核心原则**：
   - 构造函数获取资源
   - 析构函数释放资源

2. **标准库 RAII 类型**：
   - `unique_ptr` / `shared_ptr`
   - `lock_guard` / `unique_lock`
   - `fstream`

3. **实现要点**：
   - 删除拷贝操作
   - 可选：实现移动操作

