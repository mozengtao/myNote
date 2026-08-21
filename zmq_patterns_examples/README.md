# ZMQ Pattern 示例代码库

配合 [zmq_mental_model.md](../zmq_mental_model.md) 第七节「通信模式（Pattern）」使用。

本文件夹为 ZMQ 的 5 种核心 Pattern 各设计了 3 个典型场景示例（共 15 个），每个示例都提供**可独立编译/运行**的完整 C 代码和 Python 代码，帮助你：

1. 理解每种 pattern **内部是怎么工作的**（消息流向、阻塞点、负载均衡方式）。
2. 理解**什么场景该用什么 pattern**（而不是死记 API）。
3. 通过 C 和 Python 两版对照，看清"业务逻辑"和"语言语法"的边界——ZMQ 的心智模型在两种语言里是完全一样的。

---

## 目录导航

| 目录 | Pattern | 一句话心智模型 |
|---|---|---|
| [01_req_rep/](01_req_rep/README.md) | REQ / REP | 严格交替的 RPC，一问一答 |
| [02_pub_sub/](02_pub_sub/README.md) | PUB / SUB | 广播，发布者不关心谁在听 |
| [03_push_pull/](03_push_pull/README.md) | PUSH / PULL | 工厂流水线，自动负载均衡 |
| [04_dealer_router/](04_dealer_router/README.md) | DEALER / ROUTER | 交换机，知道每个连接是谁 |
| [05_pair/](05_pair/README.md) | PAIR | 一对一专线，没有角色区分 |

最后看 [SUMMARY.md](SUMMARY.md)，里面有 5 种 pattern 的横向对比表格、拓扑图和"如何选择 pattern"的决策思路。

---

## 环境依赖

### C 语言（libzmq）

Ubuntu / Debian：

```bash
sudo apt-get update
sudo apt-get install -y libzmq3-dev build-essential
```

验证安装：

```bash
pkg-config --modversion libzmq
```

每个示例目录下都有一个极简 `Makefile`，写法统一为：

```makefile
CC = gcc
CFLAGS = -Wall -O2
LDLIBS = -lzmq

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)
```

编译方式统一为 `make`，产物是与 `.c` 同名的可执行文件（例如 `server.c` -> `./server`）。

### Python（pyzmq）

```bash
pip install pyzmq
```

运行方式统一为 `python3 xxx.py`。

---

## 通用运行方式

几乎每个示例都是"先起服务端/上游，再起客户端/下游"，且**C 和 Python 两端可以互相混用**（比如 C 写的 server 配 Python 写的 client），因为 ZMQ 的 wire protocol 是语言无关的。建议：

1. 打开两个（或多个）终端。
2. 每个示例目录的 `README.md` 里都写明了先启动谁、后启动谁、预期输出。
3. 想验证"跨语言互通"，可以第一个终端跑 `./server`（C），第二个终端跑 `python3 client.py`（Python）。

---

## 目录结构总览

```
zmq_patterns_examples/
├── README.md                          本文件
├── SUMMARY.md                         总结：5种pattern的场景对比 + 心智模型
├── 01_req_rep/
│   ├── README.md
│   ├── example1_basic_rpc/
│   ├── example2_calculator_service/
│   └── example3_lazy_pirate_reliable/
├── 02_pub_sub/
│   ├── README.md
│   ├── example1_topic_filter/
│   ├── example2_multi_pub_multi_sub/
│   └── example3_slow_subscriber_hwm/
├── 03_push_pull/
│   ├── README.md
│   ├── example1_task_distribution/
│   ├── example2_three_stage_pipeline/
│   └── example3_log_aggregation/
├── 04_dealer_router/
│   ├── README.md
│   ├── example1_async_rpc/
│   ├── example2_broker_load_balance/
│   └── example3_identity_push/
└── 05_pair/
    ├── README.md
    ├── example1_thread_sync_inproc/
    ├── example2_heartbeat_tcp/
    └── example3_bidirectional_chat/
```
