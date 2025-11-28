# LevelDB Status 类设计分析

> **从 C 错误码到 C++ 面向对象错误处理的最佳实践**

---

## 目录

1. [类定义完整解析](#1-类定义完整解析)
2. [错误码设计对比](#2-错误码设计对比)
3. [典型使用场景](#3-典型使用场景)
4. [性能考量](#4-性能考量)
5. [从 C 迁移到 Status 的最佳实践](#5-从-c-迁移到-status-的最佳实践)

---

## 1. 类定义完整解析

### 1.1 头文件定义 (`include/leveldb/status.h`)

```cpp
class LEVELDB_EXPORT Status {
 public:
  // ═══════════════════════════════════════════════════════════════
  // 构造与析构
  // ═══════════════════════════════════════════════════════════════
  
  // 默认构造：成功状态，state_ 为 nullptr
  Status() noexcept : state_(nullptr) {}
  
  // 析构：释放动态分配的错误信息
  ~Status() { delete[] state_; }

  // ═══════════════════════════════════════════════════════════════
  // 拷贝语义
  // ═══════════════════════════════════════════════════════════════
  
  // 拷贝构造：深拷贝错误信息
  Status(const Status& rhs);
  Status& operator=(const Status& rhs);

  // ═══════════════════════════════════════════════════════════════
  // 移动语义 (C++11)
  // ═══════════════════════════════════════════════════════════════
  
  // 移动构造：转移所有权，源对象置空
  Status(Status&& rhs) noexcept : state_(rhs.state_) { 
    rhs.state_ = nullptr; 
  }
  Status& operator=(Status&& rhs) noexcept;

  // ═══════════════════════════════════════════════════════════════
  // 静态工厂方法 - 创建各种状态
  // ═══════════════════════════════════════════════════════════════
  
  static Status OK() { return Status(); }
  
  static Status NotFound(const Slice& msg, const Slice& msg2 = Slice());
  static Status Corruption(const Slice& msg, const Slice& msg2 = Slice());
  static Status NotSupported(const Slice& msg, const Slice& msg2 = Slice());
  static Status InvalidArgument(const Slice& msg, const Slice& msg2 = Slice());
  static Status IOError(const Slice& msg, const Slice& msg2 = Slice());

  // ═══════════════════════════════════════════════════════════════
  // 状态查询方法
  // ═══════════════════════════════════════════════════════════════
  
  bool ok() const { return (state_ == nullptr); }
  bool IsNotFound() const { return code() == kNotFound; }
  bool IsCorruption() const { return code() == kCorruption; }
  bool IsIOError() const { return code() == kIOError; }
  bool IsNotSupportedError() const { return code() == kNotSupported; }
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }

  // 转换为可读字符串
  std::string ToString() const;

 private:
  // ═══════════════════════════════════════════════════════════════
  // 错误码枚举
  // ═══════════════════════════════════════════════════════════════
  enum Code {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5
  };

  Code code() const {
    return (state_ == nullptr) ? kOk : static_cast<Code>(state_[4]);
  }

  Status(Code code, const Slice& msg, const Slice& msg2);
  static const char* CopyState(const char* s);

  // ═══════════════════════════════════════════════════════════════
  // 内存布局 - 紧凑设计
  // ═══════════════════════════════════════════════════════════════
  //
  // OK 状态: state_ = nullptr (零开销)
  //
  // 错误状态: state_ 指向动态分配的数组
  //   state_[0..3] = 消息长度 (uint32_t)
  //   state_[4]    = 错误码 (Code)
  //   state_[5..]  = 错误消息字符串
  //
  const char* state_;
};
```

### 1.2 内存布局图解

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Status 对象内存布局                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  成功状态 (OK):                                                         │
│  ┌─────────────────┐                                                   │
│  │  state_ = NULL  │  ← 仅 8 字节 (64位系统)                            │
│  └─────────────────┘                                                   │
│                                                                         │
│  错误状态:                                                              │
│  ┌─────────────────┐      ┌───────────────────────────────────────┐   │
│  │  state_ ────────┼─────►│ len(4) │ code(1) │ message (len bytes)│   │
│  └─────────────────┘      └───────────────────────────────────────┘   │
│                             ↑         ↑           ↑                   │
│                           [0..3]     [4]        [5..]                  │
│                                                                         │
│  示例: Status::IOError("read failed", "file.txt")                      │
│  ┌────────────────────────────────────────────────────────────────┐   │
│  │ 24 00 00 00 │ 05 │ read failed: file.txt                       │   │
│  └────────────────────────────────────────────────────────────────┘   │
│    len = 24      code = kIOError                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.3 实现细节 (`util/status.cc`)

```cpp
// 深拷贝 state_ 数组
const char* Status::CopyState(const char* state) {
  uint32_t size;
  std::memcpy(&size, state, sizeof(size));  // 读取消息长度
  char* result = new char[size + 5];        // 分配新内存
  std::memcpy(result, state, size + 5);     // 复制全部内容
  return result;
}

// 构造错误状态
Status::Status(Code code, const Slice& msg, const Slice& msg2) {
  assert(code != kOk);
  const uint32_t len1 = static_cast<uint32_t>(msg.size());
  const uint32_t len2 = static_cast<uint32_t>(msg2.size());
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);  // ": " 占 2 字节
  
  char* result = new char[size + 5];
  std::memcpy(result, &size, sizeof(size));  // [0..3] = 长度
  result[4] = static_cast<char>(code);       // [4] = 错误码
  std::memcpy(result + 5, msg.data(), len1); // [5..] = 消息1
  
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    std::memcpy(result + 7 + len1, msg2.data(), len2);  // 追加消息2
  }
  state_ = result;
}

// 转换为可读字符串
std::string Status::ToString() const {
  if (state_ == nullptr) {
    return "OK";
  }
  
  const char* type;
  switch (code()) {
    case kOk:             type = "OK"; break;
    case kNotFound:       type = "NotFound: "; break;
    case kCorruption:     type = "Corruption: "; break;
    case kNotSupported:   type = "Not implemented: "; break;
    case kInvalidArgument: type = "Invalid argument: "; break;
    case kIOError:        type = "IO error: "; break;
    default:              type = "Unknown code: "; break;
  }
  
  std::string result(type);
  uint32_t length;
  std::memcpy(&length, state_, sizeof(length));
  result.append(state_ + 5, length);
  return result;
}
```

---

## 2. 错误码设计对比

### 2.1 传统 C 错误处理方式

```c
// ═══════════════════════════════════════════════════════════════
// 方式1: 返回错误码，通过指针返回结果
// ═══════════════════════════════════════════════════════════════
typedef enum {
    LEVELDB_OK = 0,
    LEVELDB_NOT_FOUND = 1,
    LEVELDB_CORRUPTION = 2,
    LEVELDB_NOT_SUPPORTED = 3,
    LEVELDB_INVALID_ARG = 4,
    LEVELDB_IO_ERROR = 5
} leveldb_error_t;

// 全局错误消息
static __thread char g_error_msg[256];  // 线程局部存储

leveldb_error_t leveldb_get(db_t* db, const char* key, 
                            char** value, size_t* vlen) {
    if (db == NULL) {
        snprintf(g_error_msg, sizeof(g_error_msg), "db is NULL");
        return LEVELDB_INVALID_ARG;
    }
    
    // 查找逻辑...
    if (not_found) {
        snprintf(g_error_msg, sizeof(g_error_msg), "key not found: %s", key);
        return LEVELDB_NOT_FOUND;
    }
    
    *value = result;
    *vlen = result_len;
    return LEVELDB_OK;
}

// 获取错误消息
const char* leveldb_error_message() {
    return g_error_msg;
}

// 使用示例
void example_c_style() {
    char* value;
    size_t vlen;
    
    leveldb_error_t err = leveldb_get(db, "key1", &value, &vlen);
    if (err != LEVELDB_OK) {
        fprintf(stderr, "Error: %s\n", leveldb_error_message());
        // 问题: 全局错误消息可能被覆盖
        // 问题: 错误处理代码冗长
        return;
    }
    
    // 使用 value...
    free(value);  // 必须手动释放
}
```

```c
// ═══════════════════════════════════════════════════════════════
// 方式2: 返回结果，错误码通过参数返回
// ═══════════════════════════════════════════════════════════════
char* leveldb_get2(db_t* db, const char* key, size_t* vlen, 
                   leveldb_error_t* err, char* errmsg, size_t errmsg_size) {
    if (db == NULL) {
        *err = LEVELDB_INVALID_ARG;
        snprintf(errmsg, errmsg_size, "db is NULL");
        return NULL;
    }
    // ...
}

// 使用示例 - 参数更多，更繁琐
void example_c_style2() {
    leveldb_error_t err;
    char errmsg[256];
    size_t vlen;
    
    char* value = leveldb_get2(db, "key1", &vlen, &err, errmsg, sizeof(errmsg));
    if (err != LEVELDB_OK) {
        fprintf(stderr, "Error: %s\n", errmsg);
        return;
    }
    // ...
}
```

### 2.2 LevelDB C++ Status 方式

```cpp
// ═══════════════════════════════════════════════════════════════
// C++ Status 优雅处理
// ═══════════════════════════════════════════════════════════════

Status DB::Get(const ReadOptions& options, const Slice& key, 
               std::string* value) {
    // 参数验证
    if (key.empty()) {
        return Status::InvalidArgument("key is empty");
    }
    
    // 查找逻辑
    if (not_found) {
        return Status::NotFound(key, "not in database");
    }
    
    if (io_error) {
        return Status::IOError("read failed", filename);
    }
    
    *value = result;
    return Status::OK();  // 成功
}

// 使用示例 - 简洁优雅
void example_cpp_style() {
    std::string value;
    Status s = db->Get(ReadOptions(), "key1", &value);
    
    if (!s.ok()) {
        std::cerr << "Error: " << s.ToString() << std::endl;
        
        // 可以根据错误类型做不同处理
        if (s.IsNotFound()) {
            // 键不存在，可能是正常情况
        } else if (s.IsIOError()) {
            // I/O 错误，需要重试或报警
        }
        return;
    }
    
    // 使用 value...
}
```

### 2.3 详细对比表

| 维度 | C 错误码 | C++ Status |
|------|----------|------------|
| **返回信息** | 仅错误码数字 | 错误码 + 详细消息 |
| **消息存储** | 全局/TLS 变量 | 对象内部持有 |
| **线程安全** | 需要 TLS 或加锁 | 天然线程安全 |
| **组合消息** | 手动 snprintf | 构造时自动拼接 |
| **类型检查** | 弱 (int 比较) | 强 (专用方法) |
| **可扩展性** | 需要修改枚举 | 可继承扩展 |
| **性能 (成功)** | O(1) | O(1), 零分配 |
| **性能 (失败)** | O(1) | O(n) 堆分配 |

### 2.4 错误码定义对比

```cpp
// ═══════════════════════════════════════════════════════════════
// C 风格: 需要维护错误码和消息的映射
// ═══════════════════════════════════════════════════════════════

// error_codes.h
typedef enum {
    ERR_OK = 0,
    ERR_NOT_FOUND = 1,
    ERR_CORRUPTION = 2,
    // 添加新错误码时需要更新多处
} error_t;

// error_codes.c
const char* error_to_string(error_t err) {
    switch (err) {
        case ERR_OK: return "OK";
        case ERR_NOT_FOUND: return "Not Found";
        case ERR_CORRUPTION: return "Corruption";
        // 必须同步维护 switch-case
        default: return "Unknown error";
    }
}

// ═══════════════════════════════════════════════════════════════
// C++ Status: 错误类型和消息内聚在一起
// ═══════════════════════════════════════════════════════════════

// 工厂方法自包含错误类型信息
static Status NotFound(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotFound, msg, msg2);  // 类型和消息一起创建
}

// ToString() 集中处理格式化
std::string ToString() const {
    // 所有逻辑在一处维护
}
```

---

## 3. 典型使用场景

### 场景 1: 文件 I/O 操作 (`util/env_posix.cc:61-67`)

```cpp
// 将 POSIX errno 转换为 Status
Status PosixError(const std::string& context, int error_number) {
  if (error_number == ENOENT) {
    return Status::NotFound(context, std::strerror(error_number));
  } else {
    return Status::IOError(context, std::strerror(error_number));
  }
}

// 实际使用
Status PosixSequentialFile::Read(size_t n, Slice* result, char* scratch) {
  Status status;
  while (true) {
    ssize_t read_size = ::read(fd_, scratch, n);
    if (read_size < 0) {
      if (errno == EINTR) {
        continue;  // 被信号中断，重试
      }
      status = PosixError(filename_, errno);  // 转换为 Status
      break;
    }
    *result = Slice(scratch, read_size);
    break;
  }
  return status;  // 利用 RVO 避免拷贝
}
```

**最佳实践**:
- ✅ 将平台特定错误码统一转换为 Status
- ✅ 保留原始错误信息 (`strerror`)
- ✅ 添加上下文信息 (文件名)

---

### 场景 2: 数据完整性校验 (`db/write_batch.cc:43-79`)

```cpp
Status WriteBatch::Iterate(Handler* handler) const {
  Slice input(rep_);
  
  // 校验最小长度
  if (input.size() < kHeader) {
    return Status::Corruption("malformed WriteBatch (too small)");
  }
  
  // 解析记录
  while (!input.empty()) {
    char tag = input[0];
    input.remove_prefix(1);
    
    switch (tag) {
      case kTypeValue:
        if (GetLengthPrefixedSlice(&input, &key) &&
            GetLengthPrefixedSlice(&input, &value)) {
          handler->Put(key, value);
        } else {
          return Status::Corruption("bad WriteBatch Put");
        }
        break;
        
      case kTypeDeletion:
        if (GetLengthPrefixedSlice(&input, &key)) {
          handler->Delete(key);
        } else {
          return Status::Corruption("bad WriteBatch Delete");
        }
        break;
        
      default:
        return Status::Corruption("unknown WriteBatch tag");
    }
  }
  
  // 最终计数校验
  if (found != WriteBatchInternal::Count(this)) {
    return Status::Corruption("WriteBatch has wrong count");
  }
  
  return Status::OK();
}
```

**最佳实践**:
- ✅ 每个校验点返回具体的错误消息
- ✅ 早期返回 (early return) 减少嵌套
- ✅ 最后显式返回 `Status::OK()`

---

### 场景 3: 链式操作与错误传播 (`table/table.cc:38-80`)

```cpp
Status Table::Open(const Options& options, RandomAccessFile* file,
                   uint64_t size, Table** table) {
  *table = nullptr;  // 初始化输出参数
  
  // 校验文件大小
  if (size < Footer::kEncodedLength) {
    return Status::Corruption("file is too short to be an sstable");
  }

  // 读取 footer
  char footer_space[Footer::kEncodedLength];
  Slice footer_input;
  Status s = file->Read(size - Footer::kEncodedLength, 
                        Footer::kEncodedLength,
                        &footer_input, footer_space);
  if (!s.ok()) return s;  // ← 错误传播模式

  // 解码 footer
  Footer footer;
  s = footer.DecodeFrom(&footer_input);
  if (!s.ok()) return s;  // ← 错误传播模式

  // 读取 index block
  BlockContents index_block_contents;
  s = ReadBlock(file, opt, footer.index_handle(), &index_block_contents);
  
  // 只在成功时分配资源
  if (s.ok()) {
    Block* index_block = new Block(index_block_contents);
    Rep* rep = new Table::Rep;
    rep->options = options;
    rep->file = file;
    rep->index_block = index_block;
    *table = new Table(rep);
    (*table)->ReadMeta(footer);
  }

  return s;  // 返回最终状态
}
```

**最佳实践**:
- ✅ `if (!s.ok()) return s;` 模式简洁传播错误
- ✅ 输出参数先初始化为 nullptr
- ✅ 只在完全成功时分配资源

---

### 场景 4: 数据库恢复与错误分类 (`db/db_impl.cc:300-382`)

```cpp
Status DBImpl::Recover(VersionEdit* edit, bool* save_manifest) {
  // 获取文件锁
  Status s = env_->LockFile(LockFileName(dbname_), &db_lock_);
  if (!s.ok()) {
    return s;
  }

  // 检查数据库是否存在
  if (!env_->FileExists(CurrentFileName(dbname_))) {
    if (options_.create_if_missing) {
      Log(options_.info_log, "Creating DB %s since it was missing.", 
          dbname_.c_str());
      s = NewDB();
      if (!s.ok()) {
        return s;
      }
    } else {
      // 明确的错误：数据库不存在且不允许创建
      return Status::InvalidArgument(
          dbname_, "does not exist (create_if_missing is false)");
    }
  } else {
    if (options_.error_if_exists) {
      // 明确的错误：数据库已存在且设置了 error_if_exists
      return Status::InvalidArgument(
          dbname_, "exists (error_if_exists is true)");
    }
  }

  // 恢复版本信息
  s = versions_->Recover(save_manifest);
  if (!s.ok()) {
    return s;
  }

  // 恢复日志文件
  for (size_t i = 0; i < logs.size(); i++) {
    s = RecoverLogFile(logs[i], (i == logs.size() - 1), 
                       save_manifest, edit, &max_sequence);
    if (!s.ok()) {
      return s;
    }
  }

  return Status::OK();
}
```

**最佳实践**:
- ✅ 错误消息包含配置选项说明
- ✅ 多步骤操作逐步检查
- ✅ 日志记录与错误状态分离

---

### 场景 5: 根据错误类型分支处理 (`db/version_set.cc:882-886`)

```cpp
Status VersionSet::Recover(bool* save_manifest) {
  // 读取 CURRENT 文件
  std::string current;
  Status s = ReadFileToString(env_, CurrentFileName(dbname_), &current);
  if (!s.ok()) {
    return s;
  }

  // 打开 MANIFEST 文件
  SequentialFile* file;
  s = env_->NewSequentialFile(dscname, &file);
  if (!s.ok()) {
    // 根据错误类型做不同处理
    if (s.IsNotFound()) {
      // CURRENT 指向的文件不存在，转换为 Corruption 错误
      return Status::Corruption(
          "CURRENT points to a non-existent file",
          s.ToString());  // 保留原始错误信息
    }
    return s;  // 其他错误直接返回
  }
  
  // ...继续处理
}
```

**最佳实践**:
- ✅ 使用 `IsNotFound()` 等方法判断错误类型
- ✅ 可以转换错误类型 (NotFound → Corruption)
- ✅ 原始错误信息作为附加消息保留

---

## 4. 性能考量

### 4.1 返回值优化 (RVO/NRVO)

```cpp
// ═══════════════════════════════════════════════════════════════
// RVO (Return Value Optimization) 示例
// ═══════════════════════════════════════════════════════════════

// 直接返回临时对象 - 编译器会应用 RVO
Status GetStatus() {
    return Status::OK();           // RVO: 直接在调用者栈上构造
}

Status GetError() {
    return Status::IOError("read failed");  // RVO: 无拷贝
}

// NRVO (Named RVO) - 返回命名变量
Status DoWork() {
    Status s;
    
    s = Step1();
    if (!s.ok()) return s;  // NRVO 可能应用
    
    s = Step2();
    if (!s.ok()) return s;  // NRVO 可能应用
    
    return s;  // NRVO: 编译器优化掉拷贝
}
```

**编译器优化分析**:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      RVO/NRVO 优化效果                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  未优化时:                                                              │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐          │
│  │  构造 Status │ ───► │  拷贝到返回值 │ ───► │  拷贝到变量  │          │
│  │  (函数内)    │      │  (返回过程)   │      │  (调用者)    │          │
│  └──────────────┘      └──────────────┘      └──────────────┘          │
│                                                                         │
│  RVO 优化后:                                                            │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  直接在调用者栈上构造 Status (零拷贝)                             │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 移动语义 (`include/leveldb/status.h:33-34, 115-118`)

```cpp
// 移动构造函数
Status(Status&& rhs) noexcept : state_(rhs.state_) { 
    rhs.state_ = nullptr;  // 转移所有权
}

// 移动赋值运算符
Status& operator=(Status&& rhs) noexcept {
    std::swap(state_, rhs.state_);  // 交换指针
    return *this;
    // rhs 析构时会释放旧的 state_
}
```

**移动语义测试** (`util/status_test.cc`):

```cpp
TEST(Status, MoveConstructor) {
  // 测试 OK 状态的移动
  {
    Status ok = Status::OK();
    Status ok2 = std::move(ok);   // 移动构造
    ASSERT_TRUE(ok2.ok());
    // ok.state_ 现在是 nullptr
  }

  // 测试错误状态的移动
  {
    Status status = Status::NotFound("custom NotFound status message");
    Status status2 = std::move(status);  // 移动构造
    
    ASSERT_TRUE(status2.IsNotFound());
    ASSERT_EQ("NotFound: custom NotFound status message", status2.ToString());
    // 原 status 已无效，不应再使用
  }
}
```

### 4.3 内存分配策略

```cpp
// ═══════════════════════════════════════════════════════════════
// 成功路径: 零堆分配
// ═══════════════════════════════════════════════════════════════

Status OK() { 
    return Status();  // state_ = nullptr, 无堆分配
}

bool ok() const { 
    return (state_ == nullptr);  // O(1) 检查
}

// ═══════════════════════════════════════════════════════════════
// 错误路径: 一次堆分配
// ═══════════════════════════════════════════════════════════════

Status::Status(Code code, const Slice& msg, const Slice& msg2) {
    const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
    char* result = new char[size + 5];  // 唯一的堆分配
    // 所有数据打包在一次分配中
    state_ = result;
}
```

**性能数据对比**:

| 场景 | 堆分配次数 | 内存占用 |
|------|-----------|----------|
| `Status::OK()` | 0 | 8 bytes (仅指针) |
| `Status::IOError("msg")` | 1 | 8 + 5 + len(msg) bytes |
| 拷贝错误 Status | 1 | 同上 |
| 移动错误 Status | 0 | 同上 (指针转移) |

### 4.4 与异常处理的对比

```cpp
// ═══════════════════════════════════════════════════════════════
// 异常方式 (LevelDB 不使用)
// ═══════════════════════════════════════════════════════════════

std::string Get(const std::string& key) {
    if (not_found) {
        throw NotFoundException(key);  // 堆分配 + 栈展开
    }
    if (io_error) {
        throw IOError(filename);  // 堆分配 + 栈展开
    }
    return result;
}

// 调用者
try {
    auto value = db->Get("key");
} catch (const NotFoundException& e) {
    // 处理未找到
} catch (const IOError& e) {
    // 处理 IO 错误
}

// ═══════════════════════════════════════════════════════════════
// Status 方式 (LevelDB 实际使用)
// ═══════════════════════════════════════════════════════════════

Status Get(const std::string& key, std::string* value) {
    if (not_found) {
        return Status::NotFound(key);  // 一次堆分配，无栈展开
    }
    if (io_error) {
        return Status::IOError(filename);  // 一次堆分配，无栈展开
    }
    *value = result;
    return Status::OK();  // 零分配
}
```

**性能对比**:

| 维度 | 异常 | Status |
|------|------|--------|
| 正常路径开销 | 接近零 | 零 |
| 错误路径开销 | 高 (栈展开) | 低 (一次分配) |
| 代码大小 | 较大 (异常表) | 较小 |
| 编译器优化 | 受限 | 正常优化 |

---

## 5. 从 C 迁移到 Status 的最佳实践

### 5.1 迁移步骤

```c
// ═══════════════════════════════════════════════════════════════
// 步骤 1: 原始 C 代码
// ═══════════════════════════════════════════════════════════════

// 错误码定义
typedef enum {
    E_OK = 0,
    E_NOT_FOUND = -1,
    E_IO_ERROR = -2,
    E_CORRUPTION = -3
} error_code;

// 函数签名
error_code db_get(db_t* db, const char* key, 
                  char** value, size_t* vlen,
                  char* errmsg, size_t errmsg_size);

// 使用
void example() {
    char* value;
    size_t vlen;
    char errmsg[256];
    
    error_code err = db_get(db, "key", &value, &vlen, errmsg, sizeof(errmsg));
    if (err != E_OK) {
        fprintf(stderr, "Error %d: %s\n", err, errmsg);
        return;
    }
    // 使用 value
    free(value);
}
```

```cpp
// ═══════════════════════════════════════════════════════════════
// 步骤 2: 包装为 C++ Status
// ═══════════════════════════════════════════════════════════════

// 创建转换函数
Status CErrorToStatus(error_code err, const char* errmsg) {
    switch (err) {
        case E_OK:
            return Status::OK();
        case E_NOT_FOUND:
            return Status::NotFound(errmsg);
        case E_IO_ERROR:
            return Status::IOError(errmsg);
        case E_CORRUPTION:
            return Status::Corruption(errmsg);
        default:
            return Status::IOError("unknown error", std::to_string(err));
    }
}

// 包装函数
Status DBWrapper::Get(const Slice& key, std::string* value) {
    char* raw_value;
    size_t vlen;
    char errmsg[256];
    
    error_code err = db_get(db_, key.data(), &raw_value, &vlen, 
                            errmsg, sizeof(errmsg));
    if (err != E_OK) {
        return CErrorToStatus(err, errmsg);
    }
    
    value->assign(raw_value, vlen);
    free(raw_value);
    return Status::OK();
}
```

```cpp
// ═══════════════════════════════════════════════════════════════
// 步骤 3: 完全重构为原生 Status
// ═══════════════════════════════════════════════════════════════

class DB {
 public:
    Status Get(const ReadOptions& options, const Slice& key, 
               std::string* value) {
        // 参数校验
        if (key.empty()) {
            return Status::InvalidArgument("key cannot be empty");
        }
        
        // 获取锁
        MutexLock l(&mutex_);
        
        // 查找内存表
        if (mem_->Get(key, value)) {
            return Status::OK();
        }
        
        // 查找磁盘文件
        Status s = versions_->current()->Get(options, key, value);
        if (!s.ok()) {
            // 可以添加更多上下文
            if (s.IsCorruption()) {
                return Status::Corruption(
                    "database corruption detected",
                    s.ToString());
            }
            return s;
        }
        
        return Status::OK();
    }
};
```

### 5.2 迁移检查清单

| 检查项 | 说明 |
|--------|------|
| ☐ 识别所有错误码 | 列出 C 代码中所有可能的错误返回值 |
| ☐ 映射到 Status 类型 | 将每个 C 错误码对应到合适的 Status 类型 |
| ☐ 保留错误消息 | 确保原有的错误信息不丢失 |
| ☐ 处理输出参数 | 将 `out` 参数转换为指针参数或返回值 |
| ☐ 使用 RVO | 确保返回值可以被优化 |
| ☐ 添加移动语义 | 在需要的地方使用 `std::move` |
| ☐ 测试错误路径 | 验证所有错误场景正确传播 |

### 5.3 设计模式总结

```cpp
// ═══════════════════════════════════════════════════════════════
// 模式 1: 早期返回
// ═══════════════════════════════════════════════════════════════

Status DoMultipleSteps() {
    Status s = Step1();
    if (!s.ok()) return s;
    
    s = Step2();
    if (!s.ok()) return s;
    
    s = Step3();
    if (!s.ok()) return s;
    
    return Status::OK();
}

// ═══════════════════════════════════════════════════════════════
// 模式 2: 错误累积 (最后统一检查)
// ═══════════════════════════════════════════════════════════════

Status DoWithCleanup() {
    Resource* r = Acquire();
    
    Status s = DoWork(r);
    
    Status cleanup_status = Release(r);
    
    // 优先返回工作错误，其次是清理错误
    return s.ok() ? cleanup_status : s;
}

// ═══════════════════════════════════════════════════════════════
// 模式 3: 错误转换
// ═══════════════════════════════════════════════════════════════

Status ReadData() {
    Status s = file_->Read(&data);
    if (!s.ok()) {
        if (s.IsNotFound()) {
            // 文件不存在可能意味着数据库损坏
            return Status::Corruption(
                "expected file missing",
                s.ToString());
        }
        return s;
    }
    return Status::OK();
}

// ═══════════════════════════════════════════════════════════════
// 模式 4: 带上下文的错误
// ═══════════════════════════════════════════════════════════════

Status ProcessFile(const std::string& filename) {
    Status s = Open(filename);
    if (!s.ok()) {
        return Status::IOError(
            "failed to process",
            filename + ": " + s.ToString());
    }
    return Status::OK();
}
```

### 5.4 常见陷阱与解决方案

| 陷阱 | 问题 | 解决方案 |
|------|------|----------|
| 忽略返回值 | `file->Read(...);` 不检查返回值 | 编译器警告 `[[nodiscard]]` (C++17) |
| 丢失错误信息 | 只检查 `!s.ok()` 不记录具体错误 | 始终调用 `s.ToString()` 记录 |
| 过早优化 | 担心 Status 的堆分配开销 | 成功路径零开销，错误路径开销可忽略 |
| 错误类型选择 | 不确定用哪种 Status 类型 | 参考已有代码的使用模式 |

---

## 总结

LevelDB 的 Status 类是一个精心设计的错误处理机制：

1. **简洁的 API**: 静态工厂方法创建，`ok()` 快速检查
2. **零开销抽象**: 成功路径无堆分配
3. **完整的错误信息**: 类型 + 消息 + 上下文
4. **现代 C++ 特性**: 移动语义、RVO 优化
5. **线程安全**: 对象独立，无全局状态

相比 C 错误码，Status 提供了更好的类型安全、更丰富的错误信息和更优雅的错误传播机制，同时保持了良好的性能特性。

---

*文档生成时间: 2024年*

