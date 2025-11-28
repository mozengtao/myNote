# LevelDB LSM-Tree 实现分析

> **核心文件**: `db/db_impl.cc`, `db/memtable.cc`, `table/table.cc`, `db/version_set.cc`

---

## 目录

1. [MemTable 内存表结构](#1-memtable-内存表结构)
2. [SSTable 磁盘文件格式](#2-sstable-磁盘文件格式)
3. [Compaction 合并机制](#3-compaction-合并机制)
4. [VersionSet 版本控制](#4-versionset-版本控制)
5. [读写流程图解](#5-读写流程图解)

---

## 1. MemTable 内存表结构

### 1.1 MemTable 类定义

**文件**: `db/memtable.h`

```cpp
class MemTable {
 public:
  explicit MemTable(const InternalKeyComparator& comparator);
  
  void Ref();     // 增加引用计数
  void Unref();   // 减少引用计数
  
  // 添加键值对
  void Add(SequenceNumber seq, ValueType type, 
           const Slice& key, const Slice& value);
  
  // 查找键值
  bool Get(const LookupKey& key, std::string* value, Status* s);
  
  Iterator* NewIterator();
  size_t ApproximateMemoryUsage();

 private:
  typedef SkipList<const char*, KeyComparator> Table;
  
  KeyComparator comparator_;
  int refs_;
  Arena arena_;    // 内存分配器
  Table table_;    // SkipList 存储
};
```

### 1.2 键值编码格式

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      MemTable 键值编码格式                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Internal Key 结构:                                                     │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ user_key (变长) │ sequence (7 bytes) │ type (1 byte) │           │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  MemTable Entry 编码:                                                   │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ key_size  │ internal_key  │ value_size │ value                   │  │
│  │ (varint)  │ (key_size)    │ (varint)   │ (value_size)            │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  示例: Put("user", "John") with seq=100                                 │
│                                                                         │
│  ┌────┬────────────────────────────┬────┬──────┐                       │
│  │ 12 │ "user" + seq=100 + kValue  │ 4  │ John │                       │
│  └────┴────────────────────────────┴────┴──────┘                       │
│    ↑       ↑                          ↑     ↑                          │
│  varint  internal_key              varint  value                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.3 刷新机制 (Minor Compaction)

**文件**: `db/db_impl.cc`

```cpp
void DBImpl::CompactMemTable() {
  // 1. 保存当前 immutable memtable 到 SSTable
  VersionEdit edit;
  Version* base = versions_->current();
  base->Ref();
  
  Status s = WriteLevel0Table(imm_, &edit, base);
  base->Unref();
  
  if (s.ok()) {
    // 2. 更新版本信息
    s = versions_->LogAndApply(&edit, &mutex_);
  }
  
  if (s.ok()) {
    // 3. 释放 immutable memtable
    imm_->Unref();
    imm_ = nullptr;
    has_imm_.store(false, std::memory_order_release);
    RemoveObsoleteFiles();
  }
}
```

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      MemTable 刷新流程                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. 写入达到阈值 (4MB)                                                  │
│     ┌──────────────┐                                                   │
│     │   MemTable   │  ← 写入中                                         │
│     │  (Active)    │                                                   │
│     └──────┬───────┘                                                   │
│            │ 达到阈值                                                   │
│            ▼                                                            │
│  2. 切换为 Immutable                                                    │
│     ┌──────────────┐      ┌──────────────┐                             │
│     │   MemTable   │      │  Immutable   │                             │
│     │  (新 Active) │      │   MemTable   │  ← 只读                     │
│     └──────────────┘      └──────┬───────┘                             │
│                                  │ 后台刷盘                             │
│                                  ▼                                      │
│  3. 写入 Level-0 SSTable                                                │
│     ┌──────────────┐      ┌──────────────┐                             │
│     │   MemTable   │      │ Level-0 SST  │                             │
│     │  (Active)    │      │  (.ldb 文件) │                             │
│     └──────────────┘      └──────────────┘                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. SSTable 磁盘文件格式

### 2.1 文件整体结构

**文件**: `doc/table_format.md`, `table/format.h`

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      SSTable 文件格式                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                      Data Block 1                                 │  │
│  │                    (有序键值对)                                   │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                      Data Block 2                                 │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                          ...                                      │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                      Data Block N                                 │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                     Meta Block (Filter)                          │  │
│  │                    (Bloom Filter 数据)                            │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                    MetaIndex Block                               │  │
│  │              ("filter.xxx" -> BlockHandle)                       │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                      Index Block                                 │  │
│  │              (last_key -> BlockHandle)                           │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                        Footer                                    │  │
│  │  (metaindex_handle, index_handle, magic=0xdb4775248b80fb57)      │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  Footer 固定大小: 48 bytes (2×BlockHandle + padding + magic)            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Block 格式

**文件**: `table/block_builder.cc`

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Data Block 内部格式                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Entry 格式 (前缀压缩):                                                 │
│  ┌──────────────┬───────────────┬──────────────┬─────────┬───────┐     │
│  │ shared_bytes │ unshared_bytes│ value_length │key_delta│ value │     │
│  │  (varint32)  │  (varint32)   │  (varint32)  │ (变长)  │(变长) │     │
│  └──────────────┴───────────────┴──────────────┴─────────┴───────┘     │
│                                                                         │
│  Restart Points (每 16 个 key 一个重启点):                              │
│                                                                         │
│  Entry 1: shared=0  "abc"        value1    ← restart point             │
│  Entry 2: shared=2  "d"   (="abd")  value2                              │
│  Entry 3: shared=2  "e"   (="abe")  value3                              │
│  ...                                                                    │
│  Entry 17: shared=0 "xyz"        value17   ← restart point             │
│                                                                         │
│  Block 尾部:                                                            │
│  ┌─────────────────────────────────────────────┬──────────────────┐    │
│  │ restart[0], restart[1], ..., restart[n-1]   │  num_restarts    │    │
│  │            (各 4 bytes)                     │   (4 bytes)      │    │
│  └─────────────────────────────────────────────┴──────────────────┘    │
│                                                                         │
│  Block 后还有 trailer:                                                  │
│  ┌─────────────────┬────────────────┐                                  │
│  │ type (1 byte)   │ crc32 (4 bytes)│                                  │
│  │ 0=none,1=snappy │                │                                  │
│  └─────────────────┴────────────────┘                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.3 读取优化

**文件**: `table/table.cc`

```cpp
Status Table::InternalGet(const ReadOptions& options, const Slice& k,
                          void* arg,
                          void (*handle_result)(void*, const Slice&, 
                                                const Slice&)) {
  // 1. 在 index block 中二分查找
  Iterator* iiter = rep_->index_block->NewIterator(rep_->options.comparator);
  iiter->Seek(k);
  
  if (iiter->Valid()) {
    Slice handle_value = iiter->value();
    FilterBlockReader* filter = rep_->filter;
    BlockHandle handle;
    
    // 2. Bloom Filter 快速判断
    if (filter != nullptr && 
        handle.DecodeFrom(&handle_value).ok() &&
        !filter->KeyMayMatch(handle.offset(), k)) {
      // Key 一定不存在，跳过磁盘读取
    } else {
      // 3. 读取 data block
      Iterator* block_iter = BlockReader(this, options, iiter->value());
      block_iter->Seek(k);
      
      if (block_iter->Valid()) {
        (*handle_result)(arg, block_iter->key(), block_iter->value());
      }
      delete block_iter;
    }
  }
  delete iiter;
  return s;
}
```

---

## 3. Compaction 合并机制

### 3.1 Level 层级结构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      LSM-Tree Level 结构                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Level-0: 最多 4 个文件，文件间 KEY 可重叠                              │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                                   │
│  │ SST1 │ │ SST2 │ │ SST3 │ │ SST4 │  ← 直接从 MemTable 刷入           │
│  │[a-z] │ │[b-x] │ │[c-y] │ │[a-w] │  ← 可能有重叠                     │
│  └──────┘ └──────┘ └──────┘ └──────┘                                   │
│                     │                                                   │
│                     ▼ Compaction                                        │
│  Level-1: 最大 10 MB，文件间 KEY 不重叠                                 │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                          │
│  │ SST  │ │ SST  │ │ SST  │ │ SST  │ │ SST  │                          │
│  │[a-d] │ │[e-h] │ │[i-l] │ │[m-p] │ │[q-z] │  ← 有序不重叠            │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘                          │
│                     │                                                   │
│                     ▼ Compaction                                        │
│  Level-2: 最大 100 MB，文件间 KEY 不重叠                                │
│  ┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐         │
│  │SST ││SST ││SST ││SST ││SST ││SST ││SST ││SST ││SST ││SST │         │
│  └────┘└────┘└────┘└────┘└────┘└────┘└────┘└────┘└────┘└────┘         │
│                     │                                                   │
│                     ▼                                                   │
│  Level-N: 最大 10^N MB                                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Compaction 触发条件

**文件**: `db/version_set.cc`

```cpp
bool VersionSet::NeedsCompaction() const {
  Version* v = current_;
  return (v->compaction_score_ >= 1) ||    // 大小超过阈值
         (v->file_to_compact_ != nullptr);  // seek 触发
}

void VersionSet::Finalize(Version* v) {
  int best_level = -1;
  double best_score = -1;

  for (int level = 0; level < config::kNumLevels - 1; level++) {
    double score;
    if (level == 0) {
      // Level-0 按文件数评分
      score = v->files_[level].size() / 
              static_cast<double>(config::kL0_CompactionTrigger);
    } else {
      // 其他层按大小评分
      const uint64_t level_bytes = TotalFileSize(v->files_[level]);
      score = static_cast<double>(level_bytes) / MaxBytesForLevel(level);
    }

    if (score > best_score) {
      best_level = level;
      best_score = score;
    }
  }

  v->compaction_level_ = best_level;
  v->compaction_score_ = best_score;
}
```

### 3.3 Compaction 执行过程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Major Compaction 过程                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  输入:                                                                  │
│    Level-L:   ┌──────┐                                                 │
│               │ SST1 │  ← 选中的文件                                   │
│               └──────┘                                                 │
│    Level-L+1: ┌──────┐ ┌──────┐ ┌──────┐                              │
│               │ SST2 │ │ SST3 │ │ SST4 │  ← 与 SST1 有重叠的文件       │
│               └──────┘ └──────┘ └──────┘                              │
│                                                                         │
│  处理:                                                                  │
│    1. 多路归并排序所有输入文件                                          │
│    2. 删除过期的键值对:                                                 │
│       - 被更新版本覆盖的旧值                                            │
│       - 已删除且无更高层引用的删除标记                                  │
│    3. 输出新的 Level-L+1 文件 (每个最大 2MB)                            │
│                                                                         │
│  输出:                                                                  │
│    Level-L+1: ┌──────┐ ┌──────┐                                        │
│               │ NEW1 │ │ NEW2 │  ← 合并后的新文件                      │
│               └──────┘ └──────┘                                        │
│                                                                         │
│  清理:                                                                  │
│    - 删除原输入文件 (SST1, SST2, SST3, SST4)                           │
│    - 更新 MANIFEST                                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 4. VersionSet 版本控制

### 4.1 版本管理结构

**文件**: `db/version_set.h`

```cpp
class Version {
 private:
  VersionSet* vset_;
  Version* next_;              // 双向链表
  Version* prev_;
  int refs_;                   // 引用计数
  
  // 每层的文件列表
  std::vector<FileMetaData*> files_[config::kNumLevels];
  
  // Compaction 相关
  FileMetaData* file_to_compact_;
  int file_to_compact_level_;
  double compaction_score_;
  int compaction_level_;
};

class VersionSet {
 private:
  Version dummy_versions_;     // 版本链表头
  Version* current_;           // 当前版本
  
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
  
  WritableFile* descriptor_file_;
  log::Writer* descriptor_log_;  // MANIFEST 日志
};
```

### 4.2 版本链表结构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Version 链表示意图                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│                      ┌─────────────────┐                               │
│             ┌───────►│  dummy_versions_ │◄───────┐                     │
│             │        └────────┬────────┘        │                      │
│             │                 │                  │                      │
│             │                 ▼                  │                      │
│             │        ┌─────────────────┐        │                      │
│             │        │    Version 1    │        │                      │
│             │        │   refs_ = 1     │        │                      │
│             │        │  (被迭代器持有) │        │                      │
│             │        └────────┬────────┘        │                      │
│             │                 │                  │                      │
│             │                 ▼                  │                      │
│             │        ┌─────────────────┐        │                      │
│             │        │    Version 2    │        │                      │
│             │        │   refs_ = 2     │        │                      │
│             └────────┤  (current_)     ├────────┘                      │
│                      └─────────────────┘                               │
│                                                                         │
│  当 refs_ 降为 0 时，版本从链表中移除并删除                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.3 MANIFEST 文件

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      MANIFEST 文件格式                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  MANIFEST 记录每次版本变更:                                             │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ VersionEdit 1: comparator="leveldb.BytewiseComparator"           │  │
│  │                log_number=3, next_file_number=5                  │  │
│  │                new_files: [(L0, file4, [a-z])]                   │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │ VersionEdit 2: deleted_files: [(L0, file4)]                      │  │
│  │                new_files: [(L1, file6, [a-m]),                   │  │
│  │                            (L1, file7, [n-z])]                   │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │ VersionEdit 3: ...                                               │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  CURRENT 文件内容: "MANIFEST-000002\n"                                  │
│  指向当前使用的 MANIFEST                                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. 读写流程图解

### 5.1 写入流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      写入流程 (Write Path)                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Client: db->Put("key", "value")                                        │
│           │                                                             │
│           ▼                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 1. 写入 WAL (Write-Ahead Log)                                   │   │
│  │    文件: xxxxxx.log                                              │   │
│  │    db/log_writer.cc                                              │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│           │                                                             │
│           ▼                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 2. 写入 MemTable                                                │   │
│  │    db/memtable.cc                                                │   │
│  │    db/write_batch.cc                                             │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│           │                                                             │
│           │ (MemTable 达到 4MB)                                         │
│           ▼                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 3. 切换 MemTable → Immutable MemTable                           │   │
│  │    创建新的 MemTable 和 WAL                                      │   │
│  │    db/db_impl.cc: MakeRoomForWrite()                            │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│           │                                                             │
│           │ (后台线程)                                                  │
│           ▼                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 4. Minor Compaction: Immutable → Level-0 SSTable                │   │
│  │    db/db_impl.cc: CompactMemTable()                             │   │
│  │    db/builder.cc: BuildTable()                                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│           │                                                             │
│           │ (Level-0 文件数 ≥ 4)                                        │
│           ▼                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 5. Major Compaction: Level-N → Level-N+1                        │   │
│  │    db/db_impl.cc: DoCompactionWork()                            │   │
│  │    table/merger.cc: NewMergingIterator()                        │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 读取流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      读取流程 (Read Path)                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Client: db->Get("key", &value)                                         │
│           │                                                             │
│           ▼                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 1. 查找 MemTable                                                │   │
│  │    db/db_impl.cc: Get()                                         │   │
│  │    db/memtable.cc: Get()                                        │   │
│  │                                                                  │   │
│  │    找到? ───Yes──→ 返回 value                                   │   │
│  │      │                                                           │   │
│  │      No                                                          │   │
│  └──────┼──────────────────────────────────────────────────────────┘   │
│         ▼                                                               │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 2. 查找 Immutable MemTable (如果存在)                           │   │
│  │                                                                  │   │
│  │    找到? ───Yes──→ 返回 value                                   │   │
│  │      │                                                           │   │
│  │      No                                                          │   │
│  └──────┼──────────────────────────────────────────────────────────┘   │
│         ▼                                                               │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 3. 查找 Level-0 SSTable (可能多个，从新到旧)                    │   │
│  │    db/version_set.cc: Get()                                     │   │
│  │    table/table.cc: InternalGet()                                │   │
│  │                                                                  │   │
│  │    Bloom Filter 检查 → 跳过不匹配的文件                          │   │
│  │                                                                  │   │
│  │    找到? ───Yes──→ 返回 value                                   │   │
│  │      │                                                           │   │
│  │      No                                                          │   │
│  └──────┼──────────────────────────────────────────────────────────┘   │
│         ▼                                                               │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 4. 查找 Level-1...N SSTable (每层二分查找一个文件)              │   │
│  │                                                                  │   │
│  │    for level in 1..N:                                           │   │
│  │      file = 二分查找 key 可能所在的文件                          │   │
│  │      if Bloom Filter 可能匹配:                                   │   │
│  │        if file 中找到 key:                                       │   │
│  │          return value                                            │   │
│  │                                                                  │   │
│  │    return NotFound                                               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.3 关键文件索引

| 功能 | 文件 |
|------|------|
| 数据库主实现 | `db/db_impl.cc` |
| MemTable | `db/memtable.cc`, `db/skiplist.h` |
| WAL 日志 | `db/log_writer.cc`, `db/log_reader.cc` |
| SSTable 构建 | `table/table_builder.cc`, `db/builder.cc` |
| SSTable 读取 | `table/table.cc`, `table/block.cc` |
| 版本管理 | `db/version_set.cc`, `db/version_edit.cc` |
| Compaction | `db/db_impl.cc` (DoCompactionWork) |
| 多路归并 | `table/merger.cc` |
| Bloom Filter | `util/bloom.cc`, `table/filter_block.cc` |

---

## 总结

LevelDB 的 LSM-Tree 实现具有以下特点：

1. **写入优化**：所有写入先进入内存，顺序刷盘
2. **读取优化**：Bloom Filter + 多级缓存
3. **空间回收**：通过 Compaction 合并和清理数据
4. **并发安全**：MVCC 通过 Version 实现一致性读取
5. **崩溃恢复**：WAL 保证数据持久性

---

*文档生成时间: 2024年*

