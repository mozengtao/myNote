# 03 自动门：FSM 决定「该开门」，电机才开门

> **钉死的心智：** `OPENING` / `CLOSING` 是过程态。FSM 只发出 `MOTOR_*`；真正移动门扇的是电机 Worker。

有人靠近时，关闭的门不会瞬间变成打开。FSM 先判定「现在应该开门」，发 `MOTOR_OPEN` 进入 `OPENING`。电机跑完、行程开关超时，才进入 `OPEN` 并闩住。关门同理。关门途中又来人，FSM 判定「反向开门」，再次发 `MOTOR_OPEN`。

## 状态 / 事件 / 动作

```
State:   CLOSED, OPENING, OPEN, CLOSING
Event:   PERSON, TIMEOUT
Action:  MOTOR_OPEN, MOTOR_CLOSE, LATCH_OPEN, LATCH_CLOSED
```

转移表（未列出的组合一律 REJECT）：

```
+---------+---------+------------+--------------+
| State   | Event   | Next State | Actions      |
+---------+---------+------------+--------------+
| CLOSED  | PERSON  | OPENING    | MOTOR_OPEN   |
| OPENING | TIMEOUT | OPEN       | LATCH_OPEN   |
| OPEN    | PERSON  | OPEN       | (none)       |
| OPEN    | TIMEOUT | CLOSING    | MOTOR_CLOSE  |
| CLOSING | PERSON  | OPENING    | MOTOR_OPEN   |
| CLOSING | TIMEOUT | CLOSED     | LATCH_CLOSED |
| CLOSED  | TIMEOUT | REJECT     | -            |
| OPENING | PERSON  | REJECT     | -            |
+---------+---------+------------+--------------+
```

状态图：

```
  CLOSED --PERSON--> OPENING --TIMEOUT--> OPEN --TIMEOUT--> CLOSING
     ^                  ^                  |                  |
     |                  |                  | PERSON           | PERSON
     |                  +------------------+                  |
     |                  +-------------------------------------+
     +------------------------------ TIMEOUT -----------------+
```

`OPEN + PERSON` 是自环：门已开，只需保持，不发电机指令。

## 一次事件的三层走查

当前 `CLOSED`，收到 `PERSON`：

```
[EVENT]    PERSON | state=CLOSED
[DECISION] CLOSED + PERSON -> OPENING  actions=[MOTOR_OPEN]
[DISPATCH] MOTOR_OPEN
[EXEC]     motor -> OPEN
```

FSM 没有驱动电机。它只判定：现在应该开门。电机 Worker 才执行。

关门途中来人：

```
[EVENT]    PERSON | state=CLOSING
[DECISION] CLOSING + PERSON -> OPENING  actions=[MOTOR_OPEN]
[DISPATCH] MOTOR_OPEN
[EXEC]     motor -> OPEN
```

同一事件 `PERSON`：关闭时表示「开始开门」，开门保持时表示「继续开着」（空 Action），关闭途中表示「反向」。

## 如果把执行写进 FSM

```
if state == CLOSED and event == PERSON:
    pwm_set_duty(80)     # FSM 里直接拧电机
    sleep(2)
    state = OPEN
```

判定被阻塞在 IO 和延时上，障碍物、反向、超时全搅进一张 if。正确拆法：FSM 瞬间返回 `MOTOR_OPEN`，行程结束作为 `TIMEOUT` 事件再进来判定。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

脚本化事件：`TIMEOUT, PERSON, TIMEOUT, PERSON, TIMEOUT, PERSON, TIMEOUT, TIMEOUT, TIMEOUT`，初始状态 `CLOSED`。第一个 `TIMEOUT` 应被 REJECT；中间的 `PERSON` 会在开门保持和关门反向两条路上出现。
