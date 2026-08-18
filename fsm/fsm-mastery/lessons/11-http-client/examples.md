# 第 11 课：五个代表例（应用请求生命周期 + 取消）

主例「HTTP 请求 FSM」的完整实现在 `code/`。其余四例只建立直觉：一次调用从发到结束，中途可以放弃；连接复用和流式是同一台机上的数据或自环，不是新状态层。

本课继续使用表、事件、超时想法。新增加的是上下文里的取消标志。

---

## 例 1：HTTP 请求生命周期（主例）

**问题**：一次 GET 要经过发送、收头、收体；用户可能在中途取消；302 要在同一次调用里再发，但不能无限跟。

**状态图**：见 `LESSON.md`。

**状态表**：`IDLE` / `SENDING` / `RECEIVING` / `DONE` / `ERROR`；`CANCEL` 置 `cancel_requested` 后 `ERROR` 再 `IDLE`。

**实现**：`code/c/http_client.c`、`code/cpp/http_client.cpp`、`code/python/http_client.py`。

**复杂度**：5 个扁平状态；上下文 4 个整数。取消不增加状态名。

**工业用途**：典型 HTTP 客户端库里的一次 transfer。

**常见缺陷**：取消后仍把 `BODY_DONE` 当成功；或增加 `CANCELLING` 却不忽略迟到事件。

**重构**：不要把「读头 / 读体」画成 `RECEIVING` 里的套圈。`HEADER_OK` 与 `CHUNK` 自环即可。

---

## 例 2：重定向（代码路径）

**问题**：响应是 302 时，同一次用户请求还没结束，客户端换 URI 再发 GET。次数有上限，防止环。

```
RECEIVING --REDIRECT [count < 3] / send GET--> SENDING
RECEIVING --REDIRECT [count >= 3] / too many--> ERROR
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| RECEIVING | REDIRECT | count < 3 | count++，send GET | SENDING |
| RECEIVING | REDIRECT | count >= 3 | print too many | ERROR |
| SENDING | SENT | — | print sent | RECEIVING |

跟重定向时 **不要** 回 `IDLE`：调用者并没有结束这次 `START`。`redirect_count` 是上下文，不要做成 `REDIRECT1` `REDIRECT2` 状态。

`main` 里「one redirect then 200」和「too many redirects」两条轨迹覆盖本例。

**复杂度**：0 个新状态；1 个计数器。

**工业用途**：浏览器、API 客户端自动跟 3xx。

**常见缺陷**：无限跟；或把每一次 302 当成一次新的用户请求（取消令牌绑错对象）。

**重构**：307/308 保持原方法是监护，不是新状态。本课一律 GET。

---

## 例 3：连接复用（讲义，不实现连接池）

**问题**：TCP（以及 TLS）握手贵。一次请求 `DONE` 之后，连接往往还活着，下一次 GET 应写在同一条连接上，而不是再走一遍建连。

本例 **不实现池**。请求机上下文里一个 `connection_open` 就够教学：

```
DONE --START [conn open] / reuse connection, send GET--> SENDING
ERROR --START [conn closed] / new connection, send GET--> SENDING
```

| 请求机 | 连接（另一台机，本课不写代码） |
|--------|--------------------------------|
| IDLE / DONE / ERROR | 可能仍是 established |
| SENDING / RECEIVING | 正在被这条请求占用 |
| CANCEL / FAIL | 教学里把 conn 关掉，避免半截流污染下次 |

第 10 课的多机思想：连接寿命 ≠ 请求寿命。池的内部（按主机分组、最大闲置、健康检查）全部省略，只在讲义里点名。

`main` 主路径结束后不 `fsm_init`，直接第二次 `START`，日志应出现 `reuse connection`。

**复杂度**：请求机仍 5 个状态；池若真写，是第二台 FSM。

**工业用途**：典型 HTTP 库的 keep-alive / 连接复用。

**常见缺陷**：把已取消或失败的半截连接放回池；或在请求机枚举里加入 `POOL_WAIT` `POOLED`，把两台机焊死。

**重构**：需要池时另画一台连接机，用事件向它要连接、还连接。不要把那些名字加进本课状态表。

---

## 例 4：取消（代码路径）

**问题**：用户在发送途中或收体途中说「不要了」。这一次请求必须结束，并且迟到的成功事件不能再交付。

```
SENDING  --CANCEL / cancel=1, abort--> ERROR --> IDLE
RECEIVING --CANCEL / cancel=1, abort--> ERROR --> IDLE
IDLE --SENT / ignore (cancel_requested)--> IDLE
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| SENDING | CANCEL | — | 置标志，abort | ERROR，然后 IDLE |
| RECEIVING | CANCEL | — | 置标志，abort | ERROR，然后 IDLE |
| * | SENT 等进度 | cancel==1 | ignore | 保持 |

取消是 **事件 + 标志**。标志解决「包还在路上」；回到 `IDLE` 解决「客户端空闲」。不要用 `CANCELLING` 代替标志——除非中止本身要和对方握手，本课没有这种握手。

`main` 第 5、6 段：取消发送后喂 `SENT`，取消接收后喂 `BODY_DONE`，都应打印 `ignore`。

**复杂度**：0 个新状态；1 个布尔。

**工业用途**：取消令牌、abort、GUI 取消下载。

**常见缺陷**：只置标志不改状态，UI 以为还在转圈；或改了状态不置标志，迟到 `BODY_DONE` 写成成功文件。

**重构**：若必须 drain 套接字，可以加一个扁平状态 `DRAINING`，仍然不要套圈。本课不要求。

---

## 例 5：流式 body（讲义 + 可选事件 `CHUNK`）

**问题**：体很大或不想等收完再交给调用者。每到一块数据就吐一块，收完再 `BODY_DONE`。

不要为每一块发明状态。`CHUNK` 是 `RECEIVING` 上的自环：

```
RECEIVING --HEADER_OK / got 200--> RECEIVING
RECEIVING --CHUNK / chunk--> RECEIVING
RECEIVING --BODY_DONE / body done--> DONE
RECEIVING --CANCEL--> ERROR --> IDLE
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| RECEIVING | CHUNK | cancel==0 | print chunk | RECEIVING |
| RECEIVING | CHUNK | cancel==1 | ignore | RECEIVING（实际保持，分发开头就 return） |
| RECEIVING | BODY_DONE | cancel==0 | complete | DONE |

代码里 `CHUNK` 已接进表。`main` 第 7 段打两个 chunk 再结束。这仍是教学打印，不是真分块解码。

**复杂度**：0 个新状态；1 个自环事件。

**工业用途**：流式 API、下载进度、大对象 GET。

**常见缺陷**：为 chunk 编号做成 `RECV1` `RECV2` … 状态爆炸；或流式回调在取消之后仍触发。

**重构**：进度百分比是上下文里的整数，不是状态。解析分块编码属于解析器，不属于本课 FSM。
