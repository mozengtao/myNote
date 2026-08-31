# 06. Shared Configuration（共享配置，Write Once Read Many）

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 7 节。

## 心智模型

- **问题**：一个配置进程更新配置，多个 worker 进程频繁读取；写少读多，
  绝不能让 worker 读到"改了一半"的撕裂配置。
- **核心技巧（seqlock）**：
  - 写者：`seq++`（变奇数，宣告"我正在写"）→ 更新各字段 → `seq++`（变偶数，
    宣告"写完了"）。
  - 读者：记录 `seq1` → 读所有字段 → 再读一次 `seq2`；
    如果 `seq1` 是奇数，或者 `seq1 != seq2`，说明读的中途被写者打断了，
    必须重试。
  - 这样读者永远不用等锁、写者也永远不用等读者——典型的
    "无锁读多写少"设计，Linux 内核的 `jiffies`/`xtime` 就是这么做的。
- **一个容易被忽视的坑（我们在压力测试中真实复现了）**：
  如果写者在"正在写"（`seq` 为奇数）期间被操作系统调度器换出
  （比如所有 CPU 核心都被忙等的 reader 占满），reader 会一直空转重试，
  直到写者被重新调度回来才能继续——这是 spinlock/seqlock 类设计通用的
  "忙等 + 调度延迟"问题，不是 bug，但在实时性要求高的系统里需要注意
  CPU 亲和性/优先级配置。
- **适用场景**：热更新限流阈值、特性开关、路由表版本号。

## 数据结构

```c
struct shared_config {
    _Atomic uint32_t seq;   /* seqlock: 偶=稳定 奇=写入中 */
    uint32_t magic;
    uint32_t version;
    int32_t  threshold;
    uint32_t retry_count;
    char     label[32];
    uint32_t checksum;       /* 覆盖 version/threshold/retry_count/label */
};
```

## 构建与运行

```bash
make
./run_demo.sh
```

`run_demo.sh` 会：

1. 启动 4 个 worker，各自轮询配置直到看到 `version == 20000`（写者最终版本）为止。
2. 启动 1 个 writer，背靠背发布 20000 次配置更新（不加任何延迟，故意
   高强度压测 seqlock）。
3. 每个 worker 每次成功读取后，用 FNV-1a checksum 校验读到的字段组合是否
   自洽——一旦出现 `checksum_mismatches > 0`，就说明 seqlock 没有生效，
   读到了撕裂数据（这是最关键的正确性指标）。

## 预期输出（节选）

```text
[writer] config created, will publish 20000 updates
[writer] done, final version=20000
[worker 0] reload_count=4965 last_version=20000 max_retries_per_read=2775 total_retries=2561259 checksum_mismatches=0 reached_final=yes
[worker 1] reload_count=5377 last_version=20000 max_retries_per_read=3952 total_retries=1558267 checksum_mismatches=0 reached_final=yes
```

- `checksum_mismatches=0`：证明所有 worker 读到的配置永远是一份完整、
  自洽的快照，从未读到"一半新一半旧"的撕裂数据。
- `reload_count`（每个 worker 观察到 version 变化的次数，通常远小于
  20000）：因为写更新速度远快于 worker 轮询速度，worker 会自然"跳过"
  一些中间版本——这正是"Write Once, Read Many"场景下的预期行为：
  读者只关心"当前最新配置"，不需要看到每一次中间更新。
- `max_retries_per_read` 有时会出现几千甚至几十万这种较大的值，正是上面
  提到的"写者被调度器换出导致读者空转"现象的直接证据。
