# 示例 2：经典三级流水线（Producer → Workers → Sink）

这是 ZMQ 官方 Guide 里最经典的例子（Task Farm）的简化版：**Producer（发任务）→ Workers（做任务，可水平扩展）→ Sink（收结果、统计耗时）**，三级都用 PUSH/PULL 串联。

## 场景

批处理系统的典型形态：一批任务需要分发处理，处理完的结果需要汇总到一个地方统计（比如"处理了多少任务、总共花了多久"），且中间的 Worker 数量可以随意增减而不影响两端逻辑。

## 心智模型

```
Producer                Workers(N个，水平扩展)              Sink
   │                                                          │
   │──(控制消息:任务总数)──────────────────────────────────▶  │  (PULL :5565)
   │                                                          │
   │──(PUSH :5563)──▶ Worker1 ─┐                              │
   │                  Worker2 ─┼──(PUSH :5564)───────────────▶│  (PULL :5564)
   │                  Worker3 ─┘                               │
                                                         统计总耗时/任务数
```

三条独立的 PUSH/PULL 管道：

1. `tcp://*:5563`：Producer → Workers（任务分发）
2. `tcp://*:5564`：Workers → Sink（结果汇聚，多对一）
3. `tcp://*:5565`：Producer → Sink（控制通道，告诉 Sink 一共要等多少个结果）

## 编译 & 运行（C）

```bash
make
./sink               # 终端1：先起 Sink（bind控制通道+结果通道）
./worker 1 &         # 终端2起若干worker
./worker 2 &
./worker 3 &
./producer           # 终端3：最后起 Producer 开始派发任务
```

## 运行（Python）

```bash
python3 sink.py
python3 worker.py 1 &
python3 worker.py 2 &
python3 worker.py 3 &
python3 producer.py
```

## 预期输出（Sink，示例）

```
[Sink] Waiting for control message...
[Sink] Expecting 100 results
[Sink] Progress: 20/100
[Sink] Progress: 40/100
...
[Sink] All 100 results received. Total time: 1.82s
```

## 关键点

- 三段管道**互相独立**，各自都是标准的 PUSH/PULL 负载均衡：Producer 不知道有几个 Worker，Worker 不知道任务是谁发的，Sink 不知道结果是哪个 Worker 发的——每一段都只关心"上一级给我任务、我处理完交给下一级"。
- 这种"用简单 pattern 拼出复杂拓扑"正是 ZMQ 的设计哲学：**不需要一个大而全的框架，用几种基础 pattern 组合出任意管道**。
- 如果 Sink 需要知道结果具体属于哪个任务，应该在结果消息里带上任务 ID（本例简化为只统计数量和耗时）。
