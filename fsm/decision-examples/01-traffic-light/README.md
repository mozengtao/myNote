# 01 交通灯：同一事件，不同状态，不同意义

> **钉死的心智：** `TICK` 本身没有意义。真正有意义的是 `State + Event`。

交通灯只有一个外部事件：定时器到期 `TICK`。红灯时它表示「该变绿」，绿灯时它表示「该变黄」，黄灯时它表示「该变红」。FSM 判定下一盏灯该亮哪一色；灯具 Worker 才去改物理灯光。

## 状态 / 事件 / 动作

```
State:   RED, GREEN, YELLOW
Event:   TICK
Action:  LIGHT_RED, LIGHT_GREEN, LIGHT_YELLOW
```

转移表：

```
+--------+-------+------------+----------------+
| State  | Event | Next State | Actions        |
+--------+-------+------------+----------------+
| RED    | TICK  | GREEN      | LIGHT_GREEN    |
| GREEN  | TICK  | YELLOW     | LIGHT_YELLOW   |
| YELLOW | TICK  | RED        | LIGHT_RED      |
+--------+-------+------------+----------------+
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

当前 `RED`，收到 `TICK`：

```
[EVENT]    TICK | state=RED
[DECISION] RED + TICK -> GREEN  actions=[LIGHT_GREEN]
[DISPATCH] LIGHT_GREEN
[EXEC]     lamp -> GREEN
```

- Decision：查表，得到下一状态 `GREEN` 和动作 `LIGHT_GREEN`。
- Dispatch：把动作交给灯具 Worker。
- Execution：Worker 把灯改成绿色。FSM 没有碰灯。

## 如果把执行写进 FSM

```
if state == RED and event == TICK:
    set_gpio_green()     # FSM 里直接改硬件
    state = GREEN
```

判定和 IO 缠在一起：单测要接 GPIO，换灯具协议要改状态机。正确拆法是 FSM 只返回 `LIGHT_GREEN`，GPIO 留在 Worker。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

脚本化事件：`TICK, TICK, TICK, TICK`，初始状态 `RED`。三种语言输出应完全一致。
