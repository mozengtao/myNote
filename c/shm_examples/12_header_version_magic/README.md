# 12. Header + Version + Magic + CRC（工业界必备的可靠性层）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 13 节。

## 心智模型

前面 01~11 的每个示例都在回答"数据怎么高效地被多个进程读写"，但没有
一个回答"我怎么知道对面这段共享内存是不是我期望的那种格式、版本，有
没有被打乱"。任何工业级共享内存协议几乎都会在数据区前面加一个固定的
Header，本示例实现并验证三层递进的防御：

| 字段 | 检测什么 | 典型触发场景 |
|---|---|---|
| Magic | "这根本不是我认识的格式" | 附加到了被其他程序复用的 `/dev/shm` 对象、名字冲突 |
| Version（major） | "格式认识，但协议不兼容" | 老 reader 遇到做了不兼容改动的新 writer（或反过来） |
| CRC | "格式和版本都对，但内容被改动过" | 内存 bit flip、写入过程中进程崩溃、越界写入 |

三层检测**必须按顺序**：先查 magic，再查 version，最后才查 CRC——如果
magic 都不对，说明这段内存跟"这份协议"毫无关系，再校验 CRC 没有意义
（甚至 `header.length` 字段本身可能就是随机垃圾，用它去算 CRC 反而可能
越界读)。

## 数据结构

```c
struct shm_header {
    uint32_t magic;
    uint16_t major_version;
    uint16_t minor_version;
    uint64_t sequence;
    uint64_t timestamp;
    uint32_t flags;
    uint32_t length;   /* 数据区有效字节数 */
    uint32_t crc;      /* 对 data[0..length) 的 CRC32 */
};

struct shm_region {
    struct shm_header header;
    uint8_t data[256];
};
```

约定：**major 版本变化 = 不兼容**（字段布局变了，老 reader 必须拒绝）；
**minor 版本变化 = 向后兼容**（只是新增了字段，老 reader 忽略新增部分
依然能正确工作）——本示例的 `shm_header_verify()` 只检查 major，
不检查 minor，正是体现这条约定。

## 与 06_shared_config 的关系

06 演示的是"高频热更新场景下用 seqlock 保证读者不会读到撕裂数据"；
本示例更关注"启动时/附加时的一次性合法性校验"（写者发布一次，读者
之后才附加进来检查）。两者可以叠加：先做 Header 校验确认版本兼容、
数据没损坏，再决定要不要用 seqlock/EBR 之类的协议去读后续的热更新。

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 依次跑四个场景，并断言每个场景观察到的 reader 输出与
预期一致：

1. **正常写入 + 正常读取**：`writer` 发布数据，`reader` 校验通过并打印
   payload。
2. **篡改 magic**（`corrupt_tool magic`）：`reader` 报
   `MAGIC_MISMATCH` 并拒绝读取。
3. **翻转数据字节但不动 crc 字段**（`corrupt_tool data`，模拟内存被
   意外改动/损坏）：`reader` 报 `CRC_MISMATCH` 并拒绝读取——这一步
   专门证明"只有真正重新计算并比较 CRC 才能发现这类损坏"，光看 magic
   和 version 是看不出来的。
4. **用不兼容的新 major 版本写入**（`./writer 2 0`）：期望 major=1 的
   `reader` 报 `VERSION_INCOMPATIBLE` 并拒绝——即使数据本身完好、CRC
   也正确，仍然因为版本不兼容而拒绝，体现"版本检查必须先于内容校验"
   的心智模型。

## 预期输出（节选）

```text
=== 场景 1: 正常写入 + 正常读取 ===
[PASS] reader 应该成功读取
       [reader] check result: OK
       [reader] magic=0x314D4853 major=1 minor=0 sequence=1 ... payload="hello from writer, major=1 minor=0"

=== 场景 2: 篡改 magic ===
[PASS] reader 应该拒绝（MAGIC_MISMATCH）
       [reader] check result: MAGIC_MISMATCH（不是本程序认识的共享内存格式）

=== 场景 3: 翻转数据字节（模拟内存损坏） ===
[PASS] reader 应该拒绝（CRC_MISMATCH）
       [reader] check result: CRC_MISMATCH（数据已损坏）

=== 场景 4: 用不兼容的新 major 版本写入 ===
[PASS] reader（期望 major=1）应该拒绝（VERSION_INCOMPATIBLE）
       [reader] check result: VERSION_INCOMPATIBLE（主版本号不兼容）

全部场景行为符合预期
```
