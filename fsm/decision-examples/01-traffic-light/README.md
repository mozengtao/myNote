# 01 交通灯：同一事件，不同状态，不同意义

> **钉死的心智：** `TICK` 本身没有颜色。颜色是 Decision 选出来的，再作为参数交给 `lamp_set(color)`。

交通灯只有一种事件 kind：`TICK`。事件带着时刻 `now_ms`，但灯该亮哪一色不在 TICK 里——红灯到期变绿，绿灯到期变黄。FSM 查表判定下一状态，并把对应颜色填进 Action；灯具 Worker 只调用 `lamp_set(color)`，不知道当前是红灯还是绿灯。

## 状态 / 事件 / 动作

```
State:      RED, GREEN, YELLOW
Event:      TICK(now_ms)
Action:     SET_LAMP(color)  ->  lamp_set(color)
Ctx:        state
```

转移表只存 kind；颜色在 `fsm_decide` 里按 **下一状态** 绑定：

```
+--------+------+------------+----------------------+
| State  | kind | Next State | Worker call          |
+--------+------+------------+----------------------+
| RED    | TICK | GREEN      | lamp_set(GREEN)      |
| GREEN  | TICK | YELLOW     | lamp_set(YELLOW)     |
| YELLOW | TICK | RED        | lamp_set(RED)        |
+--------+------+------------+----------------------+
```

状态图：

```
  +-----+  TICK   +-------+  TICK   +--------+
  | RED | ------> | GREEN | ------> | YELLOW |
  +-----+         +-------+         +--------+
     ^                                   |
     +--------------- TICK --------------+
```

## 一次事件的三层走查

当前 `RED`，收到 `TICK(now_ms=1000)`：

```
[EVENT]    TICK(now_ms=1000) | state=RED
[DECISION] RED + TICK -> GREEN
[DISPATCH] SET_LAMP(color=GREEN)
[EXEC]     lamp_set(GREEN)
```

- Decision：查表得到 `GREEN`，参数 `color=GREEN` **来自判定**，不是来自 TICK。
- Dispatch：解包后调用 `lamp_set(GREEN)`。
- Execution：Worker 改灯。FSM 没有碰 GPIO。

## 如果把执行写进 FSM

```
if state == RED and event.kind == TICK:
    gpio_write(GREEN_PIN)     # FSM 里直接改硬件
    state = GREEN
```

判定和 IO 缠在一起。正确拆法：FSM 构造 `SET_LAMP(color=GREEN)`，GPIO 留在 `lamp_set`。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

脚本化事件：`TICK(1000), TICK(2000), TICK(3000), TICK(4000)`，初始状态 `RED`。三种语言输出应完全一致。
