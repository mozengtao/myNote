# FSM Decision 可运行对照示例

> **FSM 的本质不是执行，而是基于当前状态（State）和输入事件（Event）进行判定（Decision），然后决定下一状态（Next State）以及需要触发的动作（Action）。**

这五个示例用同一套骨架把这句话写成可运行代码。每种语言的源文件分区顺序相同，方便对照：

```
1. Types     State / Event / Action
2. Table     (state, event) -> (next_state, actions[], accepted)
3. Engine    fsm_decide(state, event) -> Decision   # 只查表，零副作用
4. Workers   模拟真实工作（灯、电机、网卡、库存…）
5. Dispatch  按 Action 路由到 Worker
6. Apply     先改状态，再 dispatch
7. Main      固定事件序列 + 打印 trace
```

## 统一流水线

```
(State, Event)
      |
      v
+-----------------+
|  fsm_decide()   |   Decision Engine
|  只查表，无 IO   |
+-----------------+
      |           |
      v           v
 Next State   Action List
                  |
                  v
           Dispatcher
                  |
                  v
           Worker / Service
                  |
                  v
           Real Execution
```

不变量（三种语言都遵守）：

- `fsm_decide()` 禁止 IO、禁止改全局、禁止调用 Worker。
- 非法 `(state, event)` 返回 `accepted=false`：状态不变、不发 Action。
- Worker 只接收 Action，不知道当前状态。
- 同一条脚本化事件序列，三种语言的状态序列和 Action 序列必须同构。

Trace 格式：

```
[INIT] state=RED

[EVENT] TICK | state=RED
[DECISION] RED + TICK -> GREEN  actions=[LIGHT_GREEN]
[DISPATCH] LIGHT_GREEN
[EXEC] lamp -> GREEN
```

非法事件：

```
[DECISION] REJECT LOCKED + PUSH  (state unchanged)
```

## 五例对照

| 示例 | 钉死的心智 | 目录 |
|------|------------|------|
| 1. 交通灯 | 同一事件在不同状态下意义不同 | [01-traffic-light](01-traffic-light/README.md) |
| 2. 旋转门 | 非法转移必须拒绝，不能偷偷执行 | [02-turnstile](02-turnstile/README.md) |
| 3. 自动门 | FSM 只发 `MOTOR_*`，电机才开门 | [03-auto-door](03-auto-door/README.md) |
| 4. TCP 握手 | `SEND_*` 是 Action，NIC Worker 才发包 | [04-tcp-handshake](04-tcp-handshake/README.md) |
| 5. 订单履约 | 一次判定产出 Action List，分发给多个 Worker | [05-order](05-order/README.md) |

## 怎么跑

每个示例目录下：

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

建议先看 Python，再对照 C / Go：三份代码做同一件事，差别只在语法。
