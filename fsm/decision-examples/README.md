# FSM Decision 可运行对照示例

> **FSM 的本质不是执行，而是基于当前状态（State）和输入事件（Event）进行判定（Decision），然后决定下一状态（Next State）以及需要触发的动作（Action）。**

这五个示例用同一套骨架把这句话写成可运行代码。每种语言的源文件分区顺序相同，方便对照：

```
1. Types     State / EventKind / ActionKind / Event / Action / Ctx
2. Table     (state, event_kind) -> (next_state, action_kinds[])
3. Engine    fsm_decide(ctx, event) -> Decision   # 查表 + 填 Action 参数，零副作用
4. Workers   带参函数（lamp_set(color), nic_send(flags, seq, ack), ...）
5. Dispatch  按 Action.kind 解包参数，调用对应 Worker
6. Apply     先改 Ctx（state 与扩展数据），再 dispatch
7. Main      固定事件序列 + 打印 trace
```

## kind 与数据

判定用的是 **kind**（枚举）。干活用的是 **数据**（payload / 函数参数）。

| 概念 | kind（判定） | 数据（执行） |
|------|--------------|--------------|
| Event | `TICK` / `SYN` / `PAY_SUCCESS` | `now_ms` / `seq` / `sku, qty` |
| Action | `SET_LAMP` / `NIC_SEND` / `DEDUCT_STOCK` | `color` / `flags, seq, ack` / `sku, qty` |
| State | `RED` / `LISTEN` / `PAID` | 扩展状态放在 **Ctx**（序号、订单字段），不塞进 Worker |

转移表只存 kind。参数不进表：

```
查表键： (State, Event.kind)
填参：   Action.args 来自 Event.payload 和/或 Ctx
执行：   worker_fn(args...)     # Worker 不知道 FSM State
```

Action 参数有两种来源：

- **来自判定本身：** `TICK` 没有颜色；红灯到期 → Decision 选出 `lamp_set(GREEN)`。
- **来自 Event / Ctx：** `PAY_SUCCESS` 的 sku/qty 填进 `inventory_deduct(sku, qty)`；`SYN.seq` 填进 `nic_send(..., ack=seq+1)`。

## 统一流水线

```
Event (kind + payload) + Ctx (state + extended data)
      |
      v
+-----------------+
|  fsm_decide()   |   查表用 kind；构造带参 Action
|  无 IO、无 Worker|
+-----------------+
      |           |
      v           v
 Next State   Action List (kind + args)
                  |
                  v
           Dispatcher
                  |
                  v
           worker_fn(args...)
```

不变量（三种语言都遵守）：

- `fsm_decide()` 禁止 IO、禁止改全局、禁止调用 Worker。
- 非法 `(state, event.kind)` 返回 `accepted=false`：状态不变、不发 Action。
- Worker 只接收函数参数，不知道当前 FSM State。
- 同一条脚本化事件序列，三种语言的状态、Action.kind 和关键参数必须同构。

Trace 格式：

```
[INIT] state=RED

[EVENT] TICK(now_ms=1000) | state=RED
[DECISION] RED + TICK -> GREEN
[DISPATCH] SET_LAMP(color=GREEN)
[EXEC] lamp_set(GREEN)
```

非法事件：

```
[DECISION] REJECT LOCKED + PUSH  (state unchanged)
```

## 五例对照

| 示例 | 钉死的心智 | 目录 |
|------|------------|------|
| 1. 交通灯 | 同一事件在不同状态下意义不同；颜色来自判定 | [01-traffic-light](01-traffic-light/README.md) |
| 2. 旋转门 | 非法转移必须拒绝；投币金额进入 `coin_box_credit(cents)` | [02-turnstile](02-turnstile/README.md) |
| 3. 自动门 | FSM 只发出 `motor_run(dir, duty)`，电机才转 | [03-auto-door](03-auto-door/README.md) |
| 4. TCP 握手 | `nic_send(flags, seq, ack)`；序号活在 Ctx | [04-tcp-handshake](04-tcp-handshake/README.md) |
| 5. 订单履约 | 一次判定把订单数据填进多个带参 Worker | [05-order](05-order/README.md) |

## 怎么跑

每个示例目录下：

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

建议先看 Python，再对照 C / Go：三份代码做同一件事，差别只在语法。
