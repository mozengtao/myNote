# 示例 1：任务分发 + 多 Worker 负载均衡

一个 Producer 派发 20 个"任务"（每个任务是一个耗时权重 1~5），多个 Worker 并行抢任务处理，验证 ZMQ 的自动 Round-Robin 负载均衡。

## 场景

模拟一个简单的计算任务队列：任务量固定，Worker 数量可以随时增减（水平扩展），Producer 完全不用关心具体是哪个 Worker 处理了任务。

## 心智模型

```
Producer (PUSH, bind)
     │
     │ 20个任务，自动轮流分发给"当前有空位"的Worker
 ┌───┼────────┬────────┐
 ▼   ▼        ▼        ▼
Worker1   Worker2   Worker3   (PULL, connect)
```

## 编译 & 运行（C）

```bash
make
# 先起若干个 worker（每个worker给一个编号方便区分日志）
./worker 1 &
./worker 2 &
./worker 3 &
# 再起 producer 开始派发任务
./producer
```

## 运行（Python）

```bash
python3 worker.py 1 &
python3 worker.py 2 &
python3 worker.py 3 &
python3 producer.py
```

## 预期输出（示例）

```
[Producer] Dispatching 20 tasks...
[Producer] Done dispatching.
[Worker 1] processing task 0 (cost=3 units)
[Worker 2] processing task 1 (cost=1 units)
[Worker 3] processing task 2 (cost=5 units)
[Worker 1] processing task 3 (cost=2 units)
...
```

具体哪个 Worker 拿到哪个任务不是固定顺序——ZMQ 会优先把任务发给"发送队列还有空位"的 Worker，天然实现了一种简单的负载均衡（处理快的 Worker 会拿到更多任务）。

## 关键点

- Worker 数量可以运行时增减：新起一个 `./worker 4` 会自动加入分发轮次，无需重启 Producer——这就是"水平扩展"的心智模型。
- 如果所有 Worker 都挂了，Producer 的 `zmq_send()` 会阻塞在队列写满之后，而不是丢弃任务——这是与 PUB/SUB 的核心区别。
