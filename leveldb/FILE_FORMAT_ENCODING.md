# LevelDB 文件格式与编码分析

> **核心目录**: `table/`, `util/coding.cc`, `util/crc32c.cc`

---

## 目录

1. [Block 格式设计](#1-block-格式设计)
2. [Bloom Filter 实现](#2-bloom-filter-实现)
3. [变长编码技术](#3-变长编码技术)
4. [CRC 校验实现](#4-crc-校验实现)
5. [二进制格式详解](#5-二进制格式详解)

---

## 1. Block 格式设计

### 1.1 Data Block 结构

**文件**: `table/block_builder.cc`, `table/block.cc`

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Data Block 完整格式                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                        Entry 序列                                 │  │
│  │  ┌─────────────────────────────────────────────────────────────┐ │  │
│  │  │ Entry 1: shared=0, unshared=3, val_len=5, "abc", "value1"   │ │  │
│  │  ├─────────────────────────────────────────────────────────────┤ │  │
│  │  │ Entry 2: shared=2, unshared=1, val_len=5, "d",   "value2"   │ │  │
│  │  │          (完整 key = "ab" + "d" = "abd")                    │ │  │
│  │  ├─────────────────────────────────────────────────────────────┤ │  │
│  │  │ Entry 3: shared=2, unshared=1, val_len=5, "e",   "value3"   │ │  │
│  │  │          (完整 key = "ab" + "e" = "abe")                    │ │  │
│  │  └─────────────────────────────────────────────────────────────┘ │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                     Restart Points                               │  │
│  │  ┌───────────┬───────────┬───────────┬──────────────────────┐   │  │
│  │  │restart[0] │restart[1] │restart[2] │   num_restarts=3     │   │  │
│  │  │  =0       │  =48      │  =96      │   (uint32, LE)       │   │  │
│  │  │ (uint32)  │ (uint32)  │ (uint32)  │                      │   │  │
│  │  └───────────┴───────────┴───────────┴──────────────────────┘   │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                        Block Trailer                             │  │
│  │  ┌─────────────────────────┬────────────────────────────────┐   │  │
│  │  │  type (1 byte)          │  crc32 (4 bytes, LE)           │   │  │
│  │  │  0x00=none, 0x01=snappy │  (包含 type 在内的校验)        │   │  │
│  │  │  0x02=zstd              │                                │   │  │
│  │  └─────────────────────────┴────────────────────────────────┘   │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 单个 Entry 格式

```cpp
// table/block_builder.cc
void BlockBuilder::Add(const Slice& key, const Slice& value) {
  // Entry 编码:
  // ┌─────────────┬───────────────┬──────────────┬───────────┬───────┐
  // │shared_bytes │ unshared_bytes│ value_length │ key_delta │ value │
  // │ (varint32)  │  (varint32)   │  (varint32)  │ (变长)    │ (变长)│
  // └─────────────┴───────────────┴──────────────┴───────────┴───────┘
  
  PutVarint32(&buffer_, shared);           // 共享前缀长度
  PutVarint32(&buffer_, non_shared);       // 非共享部分长度
  PutVarint32(&buffer_, value.size());     // value 长度
  buffer_.append(key.data() + shared, non_shared);  // 非共享的 key 部分
  buffer_.append(value.data(), value.size());       // value 数据
}
```

### 1.3 Index Block 结构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Index Block 格式                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Index Block 存储每个 Data Block 的索引信息:                            │
│                                                                         │
│  ┌────────────────────────────────────────────────────────────────────┐│
│  │ Entry 1: key="abc\xFF" (Data Block 1 的最大 key 的分隔符)          ││
│  │          value=BlockHandle(offset=0, size=4096)                    ││
│  ├────────────────────────────────────────────────────────────────────┤│
│  │ Entry 2: key="ghi\xFF" (Data Block 2 的最大 key 的分隔符)          ││
│  │          value=BlockHandle(offset=4101, size=4096)                 ││
│  ├────────────────────────────────────────────────────────────────────┤│
│  │ Entry 3: key="xyz\xFF" (Data Block 3 的最大 key 的分隔符)          ││
│  │          value=BlockHandle(offset=8202, size=4096)                 ││
│  └────────────────────────────────────────────────────────────────────┘│
│                                                                         │
│  BlockHandle 编码: varint64(offset) + varint64(size)                   │
│  最大 20 bytes                                                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.4 Block 读取过程

**文件**: `table/block.cc`

```cpp
// Block 迭代器使用二分查找定位 restart point
void Block::Iter::Seek(const Slice& target) {
  // 1. 在 restart points 上二分查找
  uint32_t left = 0;
  uint32_t right = num_restarts_ - 1;
  
  while (left < right) {
    uint32_t mid = (left + right + 1) / 2;
    uint32_t region_offset = GetRestartPoint(mid);
    // 解析 mid 位置的 key
    Slice mid_key = GetKeyAtRestartPoint(region_offset);
    
    if (Compare(mid_key, target) < 0) {
      left = mid;
    } else {
      right = mid - 1;
    }
  }
  
  // 2. 从 restart point 开始线性扫描
  SeekToRestartPoint(left);
  while (Valid()) {
    if (Compare(key_, target) >= 0) {
      return;
    }
    Next();
  }
}
```

---

## 2. Bloom Filter 实现

### 2.1 算法设计

**文件**: `util/bloom.cc`

```cpp
class BloomFilterPolicy : public FilterPolicy {
 public:
  explicit BloomFilterPolicy(int bits_per_key) 
      : bits_per_key_(bits_per_key) {
    // k = ln(2) * (bits/key) ≈ 0.69 * bits_per_key
    k_ = static_cast<size_t>(bits_per_key * 0.69);
    if (k_ < 1) k_ = 1;
    if (k_ > 30) k_ = 30;  // 限制探测次数
  }
};
```

### 2.2 创建 Filter

```cpp
void CreateFilter(const Slice* keys, int n, std::string* dst) const override {
  // 1. 计算位数组大小
  size_t bits = n * bits_per_key_;
  if (bits < 64) bits = 64;  // 最小 64 bits
  
  size_t bytes = (bits + 7) / 8;
  bits = bytes * 8;
  
  // 2. 分配空间
  const size_t init_size = dst->size();
  dst->resize(init_size + bytes, 0);
  dst->push_back(static_cast<char>(k_));  // 存储 k 值
  
  char* array = &(*dst)[init_size];
  
  // 3. 对每个 key 设置 k 个位
  for (int i = 0; i < n; i++) {
    uint32_t h = BloomHash(keys[i]);
    const uint32_t delta = (h >> 17) | (h << 15);  // 旋转得到增量
    
    for (size_t j = 0; j < k_; j++) {
      const uint32_t bitpos = h % bits;
      array[bitpos / 8] |= (1 << (bitpos % 8));  // 设置位
      h += delta;  // Double hashing
    }
  }
}
```

### 2.3 Filter 内存布局

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Bloom Filter 数据格式                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌────────────────────────────────────────────────────────────┬──────┐ │
│  │                    Bit Array                               │  k   │ │
│  │                   (bytes 个字节)                           │(1字节)│ │
│  └────────────────────────────────────────────────────────────┴──────┘ │
│                                                                         │
│  示例: 10 个 key, bits_per_key=10                                       │
│        bits = 10 * 10 = 100, bytes = 13, k = 6                         │
│                                                                         │
│  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬───┐     │
│  │byte0│byte1│byte2│byte3│byte4│byte5│byte6│byte7│byte8│...  │ k │     │
│  │     │     │     │     │     │     │     │     │     │     │=6 │     │
│  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴───┘     │
│   ↑                                                                     │
│   每个字节存储 8 个 bit                                                 │
│                                                                         │
│  查找 key 时:                                                           │
│  - 计算 hash(key) 得到 h                                               │
│  - 检查 k 个位置: h%bits, (h+delta)%bits, (h+2*delta)%bits, ...        │
│  - 如果任何一位为 0，key 一定不存在                                     │
│  - 如果全部为 1，key 可能存在 (有假阳性)                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.4 Filter Block 整体结构

**文件**: `table/filter_block.cc`

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Filter Block 完整格式                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  Filter 0 (对应 Data Block 偏移 0 - 2KB)                         │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │  Filter 1 (对应 Data Block 偏移 2KB - 4KB)                       │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │  Filter 2 (对应 Data Block 偏移 4KB - 6KB)                       │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │                          ...                                      │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │  Filter N-1                                                       │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │  offset[0] (uint32)  - Filter 0 起始偏移                         │  │
│  │  offset[1] (uint32)  - Filter 1 起始偏移                         │  │
│  │  ...                                                              │  │
│  │  offset[N] (uint32)  - 末尾偏移 (用于计算最后一个 filter 大小)   │  │
│  ├──────────────────────────────────────────────────────────────────┤  │
│  │  array_offset (uint32) - offset[] 数组起始位置                   │  │
│  │  kFilterBaseLg (1 byte) = 11 (每 2KB 一个 filter)                │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  给定 Block 偏移 block_offset:                                          │
│    index = block_offset >> kFilterBaseLg  (即 block_offset / 2048)     │
│    filter = data[offset[index] : offset[index+1]]                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 变长编码技术

### 3.1 Varint 编码原理

**文件**: `util/coding.cc`, `util/coding.h`

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Varint 编码原理                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  核心思想: 使用 MSB (最高位) 作为续位标志                               │
│  - MSB = 1: 后面还有更多字节                                            │
│  - MSB = 0: 这是最后一个字节                                            │
│                                                                         │
│  编码规则 (小端序):                                                      │
│                                                                         │
│  值: 300 = 0b100101100                                                  │
│                                                                         │
│  步骤:                                                                   │
│  1. 取低 7 位: 0101100 = 44                                             │
│  2. 设置 MSB=1: 10101100 = 0xAC                                         │
│  3. 右移 7 位: 10 = 2                                                   │
│  4. MSB=0 (最后): 00000010 = 0x02                                       │
│                                                                         │
│  结果: [0xAC, 0x02]                                                     │
│                                                                         │
│  解码: (0xAC & 0x7F) | ((0x02 & 0x7F) << 7)                             │
│      = 44 | (2 << 7) = 44 + 256 = 300 ✓                                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Varint32 编码实现

```cpp
char* EncodeVarint32(char* dst, uint32_t v) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(dst);
  static const int B = 128;  // 0x80
  
  if (v < (1 << 7)) {
    // 1 字节: [0, 127]
    *(ptr++) = v;
  } else if (v < (1 << 14)) {
    // 2 字节: [128, 16383]
    *(ptr++) = v | B;
    *(ptr++) = v >> 7;
  } else if (v < (1 << 21)) {
    // 3 字节: [16384, 2097151]
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = v >> 14;
  } else if (v < (1 << 28)) {
    // 4 字节: [2097152, 268435455]
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = v >> 21;
  } else {
    // 5 字节: [268435456, 4294967295]
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = (v >> 21) | B;
    *(ptr++) = v >> 28;
  }
  return reinterpret_cast<char*>(ptr);
}
```

### 3.3 编码示例

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Varint 编码示例                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  数值          二进制                    Varint 编码        字节数      │
│  ────────────────────────────────────────────────────────────────────   │
│  1             00000001                  0x01               1           │
│  127           01111111                  0x7F               1           │
│  128           10000000                  0x80 0x01          2           │
│  300           100101100                 0xAC 0x02          2           │
│  16383         11111111111111            0xFF 0x7F          2           │
│  16384         100000000000000           0x80 0x80 0x01     3           │
│  2^32-1        全1                       0xFF 0xFF 0xFF     5           │
│                                          0xFF 0x0F                      │
│                                                                         │
│  空间节省分析:                                                           │
│  - 小于 128 的值: 1 字节 (vs 4 字节定长) = 75% 节省                     │
│  - 小于 16384 的值: 2 字节 (vs 4 字节) = 50% 节省                       │
│  - 在 Block 中，大多数长度字段 < 10000，平均 2 字节                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.4 Fixed 编码

```cpp
// 小端序固定长度编码
inline void EncodeFixed32(char* dst, uint32_t value) {
  uint8_t* const buffer = reinterpret_cast<uint8_t*>(dst);
  buffer[0] = static_cast<uint8_t>(value);
  buffer[1] = static_cast<uint8_t>(value >> 8);
  buffer[2] = static_cast<uint8_t>(value >> 16);
  buffer[3] = static_cast<uint8_t>(value >> 24);
}

inline uint32_t DecodeFixed32(const char* ptr) {
  const uint8_t* const buffer = reinterpret_cast<const uint8_t*>(ptr);
  return (static_cast<uint32_t>(buffer[0])) |
         (static_cast<uint32_t>(buffer[1]) << 8) |
         (static_cast<uint32_t>(buffer[2]) << 16) |
         (static_cast<uint32_t>(buffer[3]) << 24);
}
```

---

## 4. CRC 校验实现

### 4.1 CRC32C 算法

**文件**: `util/crc32c.cc`, `util/crc32c.h`

```cpp
// CRC32C (Castagnoli polynomial)
// 多项式: 0x1EDC6F41
// 被 SSE4.2 指令集硬件加速

uint32_t Extend(uint32_t crc, const char* data, size_t n) {
  static bool accelerate = CanAccelerateCRC32C();
  if (accelerate) {
    return port::AcceleratedCRC32C(crc, data, n);
  }
  
  // 软件实现使用查表法
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  const uint8_t* e = p + n;
  uint32_t l = crc ^ kCRC32Xor;  // 预处理

  // 按字节查表处理
  while (p != e) {
    int c = (l & 0xff) ^ *p++;
    l = kByteExtensionTable[c] ^ (l >> 8);
  }
  
  return l ^ kCRC32Xor;  // 后处理
}
```

### 4.2 CRC 校验格式

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      CRC 校验在 Block 中的应用                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Block 存储格式:                                                        │
│  ┌─────────────────────────────────────────────────┬────────┬────────┐ │
│  │              Block Data                         │  Type  │  CRC   │ │
│  │            (变长)                               │(1 byte)│(4 bytes)│ │
│  └─────────────────────────────────────────────────┴────────┴────────┘ │
│                                                                         │
│  CRC 计算范围: Block Data + Type                                        │
│                                                                         │
│  CRC 值变换 (防止全 0/全 1 数据):                                       │
│  masked_crc = ((crc >> 15) | (crc << 17)) + 0xa282ead8                 │
│                                                                         │
│  验证过程:                                                              │
│  1. 读取 Block Data + Type + CRC                                        │
│  2. 计算 CRC(Block Data + Type)                                         │
│  3. Mask 计算结果                                                       │
│  4. 与存储的 CRC 比较                                                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.3 CRC 验证代码

```cpp
// table/format.cc
Status ReadBlock(RandomAccessFile* file, const ReadOptions& options,
                 const BlockHandle& handle, BlockContents* result) {
  // 1. 读取 Block 数据
  size_t n = static_cast<size_t>(handle.size());
  char* buf = new char[n + kBlockTrailerSize];
  
  Slice contents;
  Status s = file->Read(handle.offset(), n + kBlockTrailerSize, 
                        &contents, buf);
  
  // 2. 验证 CRC (如果开启)
  if (options.verify_checksums) {
    const uint32_t crc = crc32c::Unmask(DecodeFixed32(data + n + 1));
    const uint32_t actual = crc32c::Value(data, n + 1);
    if (actual != crc) {
      delete[] buf;
      return Status::Corruption("block checksum mismatch");
    }
  }
  
  // 3. 解压缩 (如果需要)
  switch (data[n]) {  // compression type
    case kNoCompression:
      // 直接使用
      break;
    case kSnappyCompression:
      // Snappy 解压
      break;
  }
  
  return s;
}
```

---

## 5. 二进制格式详解

### 5.1 完整 SSTable 文件解析

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      SSTable 二进制格式示例                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  偏移量     内容                   说明                                 │
│  ────────────────────────────────────────────────────────────────────   │
│  0x0000     [Data Block 1]        第一个数据块                          │
│             0x02 0x03 0x05        Entry: shared=2, unshared=3, val=5   │
│             "abc" "value"         key_delta + value                     │
│             ...                                                         │
│             0x00 0x00 0x00 0x00   restart[0] = 0                        │
│             0x30 0x00 0x00 0x00   restart[1] = 48                       │
│             0x02 0x00 0x00 0x00   num_restarts = 2                      │
│  0x00A0     0x00                  type = no compression                 │
│  0x00A1     0xXX 0xXX 0xXX 0xXX   crc32                                │
│                                                                         │
│  0x00A5     [Data Block 2]        第二个数据块                          │
│             ...                                                         │
│                                                                         │
│  0x0200     [Filter Block]        Bloom Filter 数据                     │
│             [bit array...]        位数组                                │
│             0x06                  k = 6                                 │
│             0x00 0x00 0x00 0x00   offset[0] = 0                         │
│             0x10 0x00 0x00 0x00   offset[1] = 16                        │
│             0x00 0x00 0x00 0x00   array_offset                          │
│             0x0B                  base_lg = 11                          │
│                                                                         │
│  0x0300     [MetaIndex Block]     元信息索引                            │
│             "filter.leveldb..."   filter 名称                           │
│             [BlockHandle]         Filter Block 位置                     │
│                                                                         │
│  0x0350     [Index Block]         数据索引                              │
│             "abc\xFF"             Block 1 分隔 key                      │
│             [BlockHandle]         Block 1 位置和大小                    │
│             "xyz\xFF"             Block 2 分隔 key                      │
│             [BlockHandle]         Block 2 位置和大小                    │
│                                                                         │
│  0x03D0     [Footer]              文件尾 (固定 48 字节)                 │
│             [MetaIndex Handle]    varint64 offset + varint64 size      │
│             [Index Handle]        varint64 offset + varint64 size      │
│             [padding]             填充至 40 字节                        │
│             0x57 0xFB 0x80 0x8B   Magic Number (低 32 位)              │
│             0x24 0x75 0x47 0xDB   Magic Number (高 32 位)              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 使用 hexdump 分析示例

```bash
# 查看 SSTable 文件
hexdump -C 000005.ldb | head -50

# 示例输出:
# 00000000  02 03 05 61 62 63 76 61  6c 75 65 02 01 05 64 76  |...abcvalue...dv|
# 00000010  61 6c 32 00 00 00 00 10  00 00 00 02 00 00 00 00  |al2.............|
# ...
```

### 5.3 Footer 结构详解

**文件**: `table/format.h`

```cpp
class Footer {
 public:
  // Footer 编码格式:
  // metaindex_handle: BlockHandle (最多 10 字节 varint64 × 2 = 20 字节)
  // index_handle:     BlockHandle (最多 20 字节)  
  // padding:          填充至 40 字节
  // magic:            8 字节固定值 (0xdb4775248b80fb57)
  // 总计:             48 字节
  
  static const size_t kEncodedLength = 48;
  static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;
  
  void EncodeTo(std::string* dst) const;
  Status DecodeFrom(Slice* input);
  
 private:
  BlockHandle metaindex_handle_;
  BlockHandle index_handle_;
};
```

---

## 总结

LevelDB 的文件格式设计体现了以下原则：

1. **空间效率**：Varint 编码、前缀压缩、Snappy/Zstd 压缩
2. **读取效率**：Index Block 支持二分查找，Bloom Filter 快速过滤
3. **数据完整性**：每个 Block 都有 CRC32C 校验
4. **扩展性**：Filter Block 支持自定义策略
5. **可移植性**：小端序编码，跨平台兼容

---

*文档生成时间: 2024年*

