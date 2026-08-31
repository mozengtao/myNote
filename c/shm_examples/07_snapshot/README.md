# 07. Snapshot（快照，Epoch-Based Reclamation）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 8 节。

## 心智模型

- **问题**：Writer 持续构建"下一代完整数据"，Reader 只想读"当前这一份完整、
  一致的数据"，且读写双方都不希望互相阻塞。数据库的 MVCC、Linux 内核的
  RCU，本质上都是这个思路的变体。
- **表面上的核心技巧**：共享内存里放 N 份快照槽位，一个只增不减的
  版本号 `current_version`，`version % N` 决定物理槽位。Writer 写完一份新
  快照后原子推进版本号完成"发布"；Reader 只需读一次版本号，找到对应槽位。
- **但"槽位什么时候可以被 Writer 复用"才是这个模式真正的难点**，
  也是本示例里最有价值的一课（见下）。

## 一次真实的"想当然的方案被压测打穿"的经历

最早的实现给每个槽位配一个引用计数（refcount）：

1. Reader 读 `current_index` → 对目标槽位 `refcount++` → 读数据 → `refcount--`。
2. Writer 只复用 `refcount == 0` 的槽位。

这个方案"讲道理"是通的，写起来也很直观，是大多数人第一次设计 Snapshot
时会写出的版本。但连续压测（4 个 reader + 1 个 writer，背靠背发布 20000
个版本）后，会以大约几万次里 1 次的概率复现：

```text
[reader 0] CHECKSUM MISMATCH idx=1 version=15819 (snapshot got overwritten while pinned!)
```

根因是**"读到 index"和"refcount+1"这两步之间存在一个时间窗口**：如果
Reader 在这个窗口里被操作系统调度器换出足够长时间，Writer 会把它认为"没人
用"的这个槽位快速轮换复用很多次，等 Reader 被唤醒继续执行 `refcount++`
时，它保护的已经不是它原本想读的那份数据了——refcount 变成了"事后才生效
的锁"，而不是"生效前就能阻止别人动手的锁"。

排查过程中还尝试过："给 Writer 的槽位选择加一个 CAS 抢占标记，防止
TOCTOU"，结果引入了新的更明显的 bug（Writer 无条件把 refcount 清零，
会把合法 Reader 的占用直接清掉）。这条路被放弃。

**这是本示例最重要的心智模型结论：无锁数据结构的正确性不能只靠"讲得通
的推理"，必须靠高强度压力测试验证；一旦压测发现问题，往往说明设计本身
有理论漏洞，修补细节不如换一个可证明正确的模型。**

## 最终方案：Epoch-Based Reclamation（EBR）

这也是 Linux 内核 RCU、Rust `crossbeam-epoch` 等无锁库回收内存的标准做法：

- Writer 维护全局版本号 `current_version`（只增不减），物理槽位 =
  `version % NUM_SNAPSHOTS`。
- 每个 Reader 有专属的"广播位"`reader_epoch[reader_id]`：
  1. 先读一次 `current_version` 得到 `v`；
  2. 把 `v` 写入自己的 `reader_epoch[reader_id]`（相当于向全世界宣告
     "我正在读版本 v，在我改口之前，谁都别回收它"）；
  3. 再读一次 `current_version` 确认 `v` 对应的槽位还没被回收
     （距离最新版本不超过 `NUM_SNAPSHOTS`）；确认通过才算真正"认领"成功，
     否则撤销广播、重试。
- Writer 发布版本 `v` 之前，如果 `v` 会导致复用槽位 `v - NUM_SNAPSHOTS`，
  必须先扫一遍所有 Reader 的广播，确认没有人还停留在这个即将被回收的
  版本上，否则原地自旋等待。
- 只要 Reader 的广播没有撤销，Writer **在数学上不可能**推进到需要回收那个
  槽位的版本——这不是"概率上大概率安全"，是可以逐条推理证明的安全。
- 读写双方对 `current_version` / `reader_epoch[]` 的访问全部使用
  `memory_order_seq_cst`：这类"各自发布意图、再互相确认对方状态"的协议
  需要一个全局总序，纯粹的 acquire/release 只保证单变量的偏序，不足以
  排除 IRIW / store-buffering 类型的异象。

## 数据结构

```c
struct snapshot_slot {
    uint64_t version;
    uint64_t payload[8];
    uint32_t checksum;
};

struct snapshot_store {
    _Atomic uint64_t current_version;
    _Atomic uint32_t writer_wait_events;
    _Atomic uint64_t reader_epoch[MAX_READERS];   /* NO_EPOCH 表示当前未在读 */
    struct snapshot_slot slots[NUM_SNAPSHOTS];
};
```

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 会启动 4 个 reader（各自反复 acquire/校验/release）与 1 个
writer（背靠背发布 20000 个版本），最后检查所有 reader 的
`checksum_mismatches` 是否均为 0。

## 预期输出（节选）

```text
[writer] snapshot store created (6 buffers), publishing 20000 versions
[writer] done, final version=20000, writer_wait_events=12224202
[reader 0] observed_versions=3866 last_version=20000 checksum_mismatches=0 reached_final=yes
[reader 1] observed_versions=3860 last_version=20000 checksum_mismatches=0 reached_final=yes
```

- `checksum_mismatches=0`（多次压测重复验证）：证明 EBR 协议下 Reader
  永远不会读到"正在被回收/覆盖"的槽位。
- `writer_wait_events` 高达千万级：因为只有 6 个槽位却要发布 20000 个
  版本，Writer 绝大多数时间都在等待某个 Reader 释放即将被复用的槽位——
  这也说明 `NUM_SNAPSHOTS` 太小会让 EBR 退化成"几乎处处等待"，实践中应
  根据 Reader 的最大持有时长与发布频率设置足够大的槽位数。
- `observed_versions` 远小于 20000：Reader 轮询速度慢于 Writer 发布速度，
  自然"跳过"了大部分中间版本，这与 Snapshot 模式"只关心当前最新完整数据"
  的语义完全吻合。

## 与 08_double_buffer 的区别

Snapshot 强调"任意多个历史版本可能同时被不同 Reader 持有，因此需要 N 个
槽位 + 回收协议"；08 的双缓冲更简单粗暴，只有 2 个槽位，且明确规定
"生产者独占写 A、消费者独占读 B"，通过一次原子 swap 切换角色，不需要
处理"多个版本同时存活"的复杂性。两者的取舍在各自 README 中对比说明。
