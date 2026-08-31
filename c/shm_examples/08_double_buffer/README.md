# 08. Double Buffer（双缓冲，SPSC 特化版 Snapshot）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 9 节。

## 心智模型

- **问题**：单个生产者持续产出"下一帧完整数据"，单个消费者只关心"当前
  这一帧"；图形渲染的前后台缓冲区（front/back buffer）、仿真系统的状态
  切换都是这个模型。
- **核心技巧**：只用 2 个槽位。生产者永远独占写"背面（back）"槽位，
  写完后原子翻转 `front_index`，让消费者下次读到新数据；消费者永远只读
  `front_index` 指向的槽位。角色随着每次翻转互换，而不是像 07_snapshot
  那样"轮转 N 个槽位"。
- **本质**：这是 07_snapshot 里 Epoch-Based Reclamation 协议的一个特例——
  把"最多 `MAX_READERS` 个并发广播者、`NUM_SNAPSHOTS` 个槽位"收窄成
  "恰好 1 个读者、恰好 2 个槽位"。读者广播自己在读哪个槽位（用
  `front_index` 的取值 0/1 直接当作"版本号"，不需要单独的版本计数器），
  写者写入 back 槽位前必须确认读者没有正在广播这个槽位，逻辑与
  07 的 EBR 完全同构，只是槽位数从 N 降到 2。

## Double Buffer vs Snapshot：差异到底在哪

| | 07 Snapshot | 08 Double Buffer |
|---|---|---|
| 槽位数 | N（本例 6） | 2（固定） |
| 读者数量 | 多个（`MAX_READERS`） | 1 个（SPSC） |
| 写者可接受的"读者落后"窗口 | 最多落后 N 个版本 | 几乎为 0（读者必须紧跟）|
| 写者阻塞概率 | 较低（多槽位吸收突发）| 较高（槽位少，稍慢就等）|
| 典型场景 | 多读者共享的行情快照、配置缓存 | 单一消费者的渲染帧、控制回路状态 |

**槛少带来的直接代价**：本示例的压测数据显示，即便只有 1 个消费者，
`writer_wait_events` 也高达千万级，且消费者能观察到约 95%~99% 的帧
（对比 07_snapshot 里 4 个读者只观察到约 20% 的版本）——这是因为
只有 2 个槽位，生产者几乎每一帧都要等消费者"腾出"上一帧的槽位，
天然形成了背压（backpressure）；而 Snapshot 用更多槽位换来了生产者
可以超前发布多个版本而不阻塞的自由度。**没有免费的午餐：槽位越少，
内存越省、协议越简单，但生产者被消费者拖慢的概率越高。**

## 数据结构

```c
struct db_slot {
    uint64_t frame_id;
    uint64_t payload[8];
    uint32_t checksum;
};

struct double_buffer {
    _Atomic uint32_t front_index;     /* 0 或 1：Reader 该读哪个槽位 */
    _Atomic uint32_t reader_epoch;    /* Reader 正在读的槽位号，NO_EPOCH=空闲 */
    _Atomic uint32_t writer_wait_events;
    struct db_slot slots[2];
};
```

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 会先在后台启动 consumer（等待共享内存出现后开始轮询），
再运行 producer 背靠背发布 20000 帧（每帧模拟一点"生成耗时"，否则
producer 会在 consumer 进程完成 `fork`/`exec` 之前就把全部帧发完，
根本看不出双缓冲的等待行为）。

## 预期输出（节选）

```text
[producer] double buffer created, publishing 20000 frames
[producer] done, final frame=20000, writer_wait_events=65590016
[consumer] observed_frames=19753 last_frame=20000 checksum_mismatches=0 reached_final=yes
```

- `checksum_mismatches=0`（多次压测重复验证）：证明消费者从未读到"正在
  被生产者覆写"的槽位。
- `writer_wait_events` 高达千万级：正是"只有 2 个槽位"带来的强背压，
  生产者绝大多数时间都在等消费者用完上一帧。
- `observed_frames` 接近 `frame_count`：因为只有 2 个槽位，生产者跳不过
  太多帧（跳一帧就必须等消费者），消费者几乎能看到每一帧——这与
  07_snapshot 里"读者大量跳过中间版本"形成鲜明对比。
