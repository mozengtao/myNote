# LevelDB 并发编程模式分析

> **核心文件**: `port/port_stdcxx.h`, `util/mutexlock.h`, `db/db_impl.cc`, `db/skiplist.h`

---

## 目录

1. [锁设计与实现](#1-锁设计与实现)
2. [无锁编程技术](#2-无锁编程技术)
3. [后台线程模型](#3-后台线程模型)
4. [热点路径优化](#4-热点路径优化)
5. [与 C 语言实现对比](#5-与-c-语言实现对比)

---

## 1. 锁设计与实现

### 1.1 Mutex 封装

**文件**: `port/port_stdcxx.h`

```cpp
// 对 std::mutex 的薄封装，添加线程安全注解
class LOCKABLE Mutex {
 public:
  Mutex() = default;
  ~Mutex() = default;
  
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  
  void Lock() EXCLUSIVE_LOCK_FUNCTION() { 
    mu_.lock(); 
  }
  
  void Unlock() UNLOCK_FUNCTION() { 
    mu_.unlock(); 
  }
  
  void AssertHeld() ASSERT_EXCLUSIVE_LOCK() {}
  
 private:
  friend class CondVar;
  std::mutex mu_;
};
```

### 1.2 条件变量封装

```cpp
class CondVar {
 public:
  explicit CondVar(Mutex* mu) : mu_(mu) { 
    assert(mu != nullptr); 
  }
  ~CondVar() = default;
  
  CondVar(const CondVar&) = delete;
  CondVar& operator=(const CondVar&) = delete;
  
  void Wait() {
    std::unique_lock<std::mutex> lock(mu_->mu_, std::adopt_lock);
    cv_.wait(lock);
    lock.release();
  }
  
  void Signal() { cv_.notify_one(); }
  void SignalAll() { cv_.notify_all(); }
  
 private:
  std::condition_variable cv_;
  Mutex* const mu_;
};
```

### 1.3 RAII 锁守卫

**文件**: `util/mutexlock.h`

```cpp
// 标准 RAII 锁守卫
class SCOPED_LOCKABLE MutexLock {
 public:
  explicit MutexLock(port::Mutex* mu) EXCLUSIVE_LOCK_FUNCTION(mu)
      : mu_(mu) {
    this->mu_->Lock();
  }
  
  ~MutexLock() UNLOCK_FUNCTION() { 
    this->mu_->Unlock(); 
  }
  
  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;
  
 private:
  port::Mutex* const mu_;
};
```

### 1.4 线程安全注解

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      线程安全注解 (Thread Safety Annotations)           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  注解宏                          含义                                   │
│  ──────────────────────────────  ──────────────────────────────────────│
│  LOCKABLE                        该类型可被锁定                         │
│  SCOPED_LOCKABLE                 RAII 风格的锁守卫                      │
│  GUARDED_BY(mutex)               该成员受 mutex 保护                   │
│  EXCLUSIVE_LOCK_FUNCTION(mutex)  函数会锁定 mutex                      │
│  UNLOCK_FUNCTION()               函数会解锁                             │
│  ASSERT_EXCLUSIVE_LOCK()         断言当前持有锁                         │
│                                                                         │
│  示例 (db/db_impl.h):                                                   │
│  ──────────────────────────────────────────────────────────────────────│
│  port::Mutex mutex_;                                                    │
│  MemTable* imm_ GUARDED_BY(mutex_);  // 受 mutex_ 保护                 │
│  std::atomic<bool> shutting_down_;   // 原子变量，无需注解              │
│                                                                         │
│  编译器 (Clang) 可静态分析是否正确使用锁:                               │
│  - 访问 GUARDED_BY 成员时必须持有对应锁                                 │
│  - 锁的获取和释放必须配对                                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 无锁编程技术

### 2.1 原子操作基础

**文件**: `db/skiplist.h`

```cpp
// 原子高度变量
std::atomic<int> max_height_;

// relaxed 读取 - 不需要同步
inline int GetMaxHeight() const {
  return max_height_.load(std::memory_order_relaxed);
}

// relaxed 写入 - 配合其他屏障使用
max_height_.store(height, std::memory_order_relaxed);
```

### 2.2 内存序详解

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      C++ 内存序 (Memory Order)                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  memory_order_relaxed                                                   │
│  ────────────────────                                                   │
│  - 仅保证原子性，不保证顺序                                             │
│  - 最快，适合计数器等无依赖场景                                         │
│                                                                         │
│  memory_order_acquire (读操作)                                          │
│  ────────────────────────────                                           │
│  - 本次读取后的所有读写不能重排到之前                                   │
│  - "获取"另一线程的 release 发布的数据                                  │
│                                                                         │
│  memory_order_release (写操作)                                          │
│  ────────────────────────────                                           │
│  - 本次写入前的所有读写不能重排到之后                                   │
│  - "发布"数据供其他线程 acquire                                         │
│                                                                         │
│  memory_order_seq_cst (默认)                                            │
│  ──────────────────────────                                             │
│  - 全局顺序一致，最强保证                                               │
│  - 性能开销最大                                                         │
│                                                                         │
│  LevelDB SkipList 中的使用:                                              │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  写线程                           读线程                        │    │
│  │  ──────────                       ──────────                    │    │
│  │  1. 初始化 Node                                                 │    │
│  │  2. x->NoBarrier_SetNext(...)     3. node = x->Next(level)     │    │
│  │     [relaxed]                        [acquire]                  │    │
│  │  3. prev->SetNext(x)                 看到完整初始化的节点        │    │
│  │     [release]                                                   │    │
│  └────────────────────────────────────────────────────────────────┘    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.3 SkipList 无锁读实现

**文件**: `db/skiplist.h`

```cpp
// Node 指针访问封装
struct Node {
  // acquire 语义读取 - 确保看到完整初始化的节点
  Node* Next(int n) {
    assert(n >= 0);
    return next_[n].load(std::memory_order_acquire);
  }
  
  // release 语义写入 - 发布新节点
  void SetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].store(x, std::memory_order_release);
  }
  
  // 无屏障版本 - 在已同步的上下文中使用
  Node* NoBarrier_Next(int n) {
    return next_[n].load(std::memory_order_relaxed);
  }
  void NoBarrier_SetNext(int n, Node* x) {
    next_[n].store(x, std::memory_order_relaxed);
  }
  
 private:
  std::atomic<Node*> next_[1];
};
```

### 2.4 单写多读模型

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      SkipList 并发模型                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                        SkipList                                   │  │
│  │                                                                   │  │
│  │      写线程 (需要外部锁)           读线程 (无锁)                  │  │
│  │      ─────────────────             ─────────────                  │  │
│  │            │                          │  │  │                     │  │
│  │            │                          ▼  ▼  ▼                     │  │
│  │            │                                                       │  │
│  │      ┌─────▼─────┐               ┌───────────────┐               │  │
│  │      │  Insert() │               │   Contains()  │               │  │
│  │      │           │               │   Iterator    │               │  │
│  │      └───────────┘               └───────────────┘               │  │
│  │                                                                   │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  保证:                                                                  │
│  1. 节点一旦插入，永不删除 (直到整个 SkipList 销毁)                     │
│  2. 节点内容 (key) 是 immutable 的                                      │
│  3. 只有 next 指针会被修改，且使用原子操作                              │
│                                                                         │
│  这些保证使得读操作完全无锁！                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 后台线程模型

### 3.1 DBImpl 线程模型

**文件**: `db/db_impl.cc`

```cpp
class DBImpl : public DB {
 private:
  port::Mutex mutex_;                    // 主锁
  std::atomic<bool> shutting_down_;      // 关闭标志
  port::CondVar background_work_finished_signal_;  // 后台完成信号
  bool background_compaction_scheduled_; // 是否已调度压缩
  
  // 后台线程入口
  static void BGWork(void* db) {
    reinterpret_cast<DBImpl*>(db)->BackgroundCall();
  }
  
  void BackgroundCall() {
    MutexLock l(&mutex_);
    
    assert(background_compaction_scheduled_);
    if (shutting_down_.load(std::memory_order_acquire)) {
      // 正在关闭，不执行压缩
    } else if (!imm_ && !manual_compaction_ && !versions_->NeedsCompaction()) {
      // 无需压缩
    } else {
      BackgroundCompaction();
    }
    
    background_compaction_scheduled_ = false;
    
    // 检查是否需要再次调度
    MaybeScheduleCompaction();
    
    background_work_finished_signal_.SignalAll();
  }
};
```

### 3.2 Compaction 调度

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Compaction 调度流程                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   用户线程                          后台线程                            │
│   ─────────                         ─────────                           │
│      │                                 │                                │
│      │ Write()                         │                                │
│      ├───────────────────────►         │                                │
│      │ MakeRoomForWrite()              │                                │
│      │   │                             │                                │
│      │   ├─ MemTable 满?               │                                │
│      │   │   │                         │                                │
│      │   │   Yes                       │                                │
│      │   │   │                         │                                │
│      │   │   ├─ 切换 MemTable          │                                │
│      │   │   │                         │                                │
│      │   │   ├─ MaybeScheduleCompaction()                               │
│      │   │   │         │               │                                │
│      │   │   │         ▼               │                                │
│      │   │   │  ┌─────────────────┐   │                                │
│      │   │   │  │ Schedule BGWork │   │                                │
│      │   │   │  │ (env_->Schedule)│   │                                │
│      │   │   │  └────────┬────────┘   │                                │
│      │   │   │           │             │                                │
│      │   │   │           └─────────────► BackgroundCall()               │
│      │   │   │                         │   │                            │
│      │   │   │                         │   ├─ BackgroundCompaction()    │
│      │   │   │                         │   │   ├─ CompactMemTable()    │
│      │   │   │                         │   │   ├─ PickCompaction()     │
│      │   │   │                         │   │   ├─ DoCompactionWork()   │
│      │   │   │                         │   │                            │
│      │   │   │                         │   ├─ MaybeScheduleCompaction() │
│      │   │   │                         │   │   (检查是否需要再次压缩)   │
│      │   │   │                         │   │                            │
│      │   │   │                         │   ├─ SignalAll()               │
│      │   │   │                         │                                │
│      │   │   └─ Wait (如果 L0 文件过多)                                 │
│      │                                 │                                │
│      │◄────────────────────────────────│                                │
│      │ 继续写入                        │                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.3 写入限速

```cpp
Status DBImpl::MakeRoomForWrite(bool force) {
  mutex_.AssertHeld();
  
  while (true) {
    if (!bg_error_.ok()) {
      // 后台错误
      return bg_error_;
    } else if (!force && 
               (mem_->ApproximateMemoryUsage() <= options_.write_buffer_size)) {
      // MemTable 还有空间
      break;
    } else if (imm_ != nullptr) {
      // 等待 immutable memtable 压缩完成
      background_work_finished_signal_.Wait();
    } else if (versions_->NumLevelFiles(0) >= kL0_StopWritesTrigger) {
      // Level-0 文件过多，等待压缩
      Log(options_.info_log, "waiting...\n");
      background_work_finished_signal_.Wait();
    } else {
      // 切换到新 memtable
      imm_ = mem_;
      has_imm_.store(true, std::memory_order_release);
      mem_ = new MemTable(internal_comparator_);
      mem_->Ref();
      
      force = false;
      MaybeScheduleCompaction();
    }
  }
  return Status::OK();
}
```

---

## 4. 热点路径优化

### 4.1 读路径优化

**文件**: `db/db_impl.cc`

```cpp
Status DBImpl::Get(const ReadOptions& options, const Slice& key,
                   std::string* value) {
  Status s;
  MutexLock l(&mutex_);
  
  SequenceNumber snapshot;
  if (options.snapshot != nullptr) {
    snapshot = static_cast<const SnapshotImpl*>(options.snapshot)->sequence_number();
  } else {
    snapshot = versions_->LastSequence();
  }
  
  // 获取当前状态的引用
  MemTable* mem = mem_;
  MemTable* imm = imm_;
  Version* current = versions_->current();
  mem->Ref();
  if (imm != nullptr) imm->Ref();
  current->Ref();
  
  // 释放锁后查找 - 这是关键优化！
  {
    mutex_.Unlock();
    
    LookupKey lkey(key, snapshot);
    // 首先查找 MemTable
    if (mem->Get(lkey, value, &s)) {
      // 找到了
    } else if (imm != nullptr && imm->Get(lkey, value, &s)) {
      // 在 immutable memtable 中找到
    } else {
      // 查找 SSTable
      s = current->Get(options, lkey, value, &stats);
    }
    
    mutex_.Lock();
  }
  
  // 释放引用
  mem->Unref();
  if (imm != nullptr) imm->Unref();
  current->Unref();
  return s;
}
```

### 4.2 优化策略总结

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      热点路径优化策略                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. 最小化锁持有时间                                                    │
│     ─────────────────────                                               │
│     - 只在必要时持有锁                                                  │
│     - 获取引用后立即释放锁                                              │
│     - 实际查找操作在锁外进行                                            │
│                                                                         │
│  2. 引用计数替代锁                                                      │
│     ─────────────────                                                   │
│     class MemTable {                                                    │
│       void Ref() { ++refs_; }                                          │
│       void Unref() { if (--refs_ == 0) delete this; }                  │
│     };                                                                  │
│                                                                         │
│     // 持有引用期间，对象不会被销毁                                     │
│     mem->Ref();                                                         │
│     mutex_.Unlock();                                                    │
│     // 安全访问 mem                                                     │
│     mutex_.Lock();                                                      │
│     mem->Unref();                                                       │
│                                                                         │
│  3. 避免写-写竞争                                                       │
│     ─────────────────                                                   │
│     - 写操作批量化 (WriteBatch)                                         │
│     - 单个 MemTable 只有一个写者                                        │
│     - 压缩在后台单线程执行                                              │
│                                                                         │
│  4. 原子变量用于简单状态                                                │
│     ─────────────────────────                                           │
│     std::atomic<bool> shutting_down_;   // 无需锁                       │
│     std::atomic<bool> has_imm_;         // 快速检查                     │
│                                                                         │
│  5. SkipList 无锁读取                                                   │
│     ─────────────────────                                               │
│     - MemTable 查找不需要锁                                             │
│     - 只需确保 MemTable 对象存活                                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. 与 C 语言实现对比

### 5.1 互斥锁对比

```c
// ═══════════════════════════════════════════════════════════════
// C 语言实现
// ═══════════════════════════════════════════════════════════════

#include <pthread.h>

typedef struct {
    pthread_mutex_t mu;
} leveldb_mutex_t;

void leveldb_mutex_init(leveldb_mutex_t* m) {
    pthread_mutex_init(&m->mu, NULL);
}

void leveldb_mutex_destroy(leveldb_mutex_t* m) {
    pthread_mutex_destroy(&m->mu);
}

void leveldb_mutex_lock(leveldb_mutex_t* m) {
    pthread_mutex_lock(&m->mu);
}

void leveldb_mutex_unlock(leveldb_mutex_t* m) {
    pthread_mutex_unlock(&m->mu);
}

// 使用示例
void c_function(leveldb_mutex_t* mu, int* data) {
    leveldb_mutex_lock(mu);
    *data = 42;
    // 如果这里 return 或抛出异常，锁不会释放！
    if (*data < 0) {
        leveldb_mutex_unlock(mu);  // 必须手动解锁
        return;
    }
    leveldb_mutex_unlock(mu);  // 容易忘记
}

// ═══════════════════════════════════════════════════════════════
// C++ 实现
// ═══════════════════════════════════════════════════════════════

class Mutex {
 public:
  Mutex() = default;
  ~Mutex() = default;  // 自动调用 std::mutex 析构
  
  void Lock() { mu_.lock(); }
  void Unlock() { mu_.unlock(); }
  
 private:
  std::mutex mu_;
};

class MutexLock {
 public:
  explicit MutexLock(Mutex* mu) : mu_(mu) { mu_->Lock(); }
  ~MutexLock() { mu_->Unlock(); }  // 自动解锁
 private:
  Mutex* const mu_;
};

// 使用示例
void cpp_function(Mutex* mu, int* data) {
    MutexLock lock(mu);  // 自动加锁
    *data = 42;
    if (*data < 0) {
        return;  // 自动解锁！
    }
    // 作用域结束自动解锁
}
```

### 5.2 原子操作对比

```c
// ═══════════════════════════════════════════════════════════════
// C 语言实现 (C11 原子)
// ═══════════════════════════════════════════════════════════════

#include <stdatomic.h>

typedef struct {
    _Atomic(struct node*) next[12];
} skiplist_node_t;

struct node* skiplist_node_next(skiplist_node_t* node, int level) {
    return atomic_load_explicit(&node->next[level], 
                                memory_order_acquire);
}

void skiplist_node_set_next(skiplist_node_t* node, int level, 
                            struct node* x) {
    atomic_store_explicit(&node->next[level], x, 
                          memory_order_release);
}

// ═══════════════════════════════════════════════════════════════
// C++ 实现
// ═══════════════════════════════════════════════════════════════

struct Node {
    Node* Next(int n) {
        return next_[n].load(std::memory_order_acquire);
    }
    
    void SetNext(int n, Node* x) {
        next_[n].store(x, std::memory_order_release);
    }
    
 private:
    std::atomic<Node*> next_[1];
};
```

### 5.3 条件变量对比

```c
// ═══════════════════════════════════════════════════════════════
// C 语言实现
// ═══════════════════════════════════════════════════════════════

typedef struct {
    pthread_cond_t cv;
    pthread_mutex_t* mu;
} leveldb_condvar_t;

void leveldb_condvar_init(leveldb_condvar_t* cv, pthread_mutex_t* mu) {
    pthread_cond_init(&cv->cv, NULL);
    cv->mu = mu;
}

void leveldb_condvar_wait(leveldb_condvar_t* cv) {
    pthread_cond_wait(&cv->cv, cv->mu);
}

void leveldb_condvar_signal(leveldb_condvar_t* cv) {
    pthread_cond_signal(&cv->cv);
}

// ═══════════════════════════════════════════════════════════════
// C++ 实现
// ═══════════════════════════════════════════════════════════════

class CondVar {
 public:
  explicit CondVar(Mutex* mu) : mu_(mu) {}
  
  void Wait() {
    std::unique_lock<std::mutex> lock(mu_->mu_, std::adopt_lock);
    cv_.wait(lock);
    lock.release();
  }
  
  void Signal() { cv_.notify_one(); }
  void SignalAll() { cv_.notify_all(); }
  
 private:
  std::condition_variable cv_;
  Mutex* const mu_;
};
```

### 5.4 对比总结

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      C vs C++ 并发编程对比                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  特性              C 实现                     C++ 实现                  │
│  ─────────────────────────────────────────────────────────────────────  │
│                                                                         │
│  互斥锁            pthread_mutex_t            std::mutex               │
│                    需手动 init/destroy        RAII 自动管理             │
│                                                                         │
│  锁守卫            无内置支持                 std::lock_guard          │
│                    需手动解锁                 自动解锁                  │
│                                                                         │
│  原子操作          _Atomic (C11)              std::atomic              │
│                    atomic_load_explicit       .load()                   │
│                                                                         │
│  内存序            memory_order_*             std::memory_order_*       │
│                    (相同)                     (相同)                    │
│                                                                         │
│  条件变量          pthread_cond_t             std::condition_variable   │
│                                                                         │
│  异常安全          不适用                     ✅ RAII 保证              │
│                                                                         │
│  代码简洁度        需要更多样板代码           更简洁                    │
│                                                                         │
│  类型安全          较弱                       ✅ 模板保证               │
│                                                                         │
│  静态分析          有限                       ✅ 线程安全注解           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 总结

LevelDB 的并发设计特点：

1. **分层锁策略**
   - 全局锁保护元数据
   - 引用计数保护数据对象
   - 无锁访问只读数据

2. **精确的内存序**
   - 仅在必要时使用屏障
   - acquire-release 配对
   - relaxed 用于无依赖操作

3. **后台线程简化**
   - 单一后台 compaction 线程
   - 避免复杂的并发控制

4. **RAII 保证安全**
   - MutexLock 确保解锁
   - 引用计数确保对象存活

5. **静态分析支持**
   - 线程安全注解
   - 编译期检查锁使用

---

*文档生成时间: 2024年*

