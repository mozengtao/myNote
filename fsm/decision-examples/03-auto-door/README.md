# 03 自动门：FSM 发出 `motor_run(dir, duty)`，电机才转

> **钉死的心智：** `OPENING` / `CLOSING` 是过程态。FSM 只构造带参命令 `motor_run(OPEN, 80)`；真正转门的是电机函数。

有人靠近时，关闭的门不会瞬间变成打开。`PERSON` 带着 `sensor_id`，`TIMEOUT` 带着 `elapsed_ms`。判定层用 kind 决定下一状态和电机方向；Worker 只看见方向和占空比，不看见 FSM 状态。

## 状态 / 事件 / 动作

```
State:      CLOSED, OPENING, OPEN, CLOSING
Event:      PERSON(sensor_id), TIMEOUT(elapsed_ms)
Action:     MOTOR_RUN(dir, duty) -> motor_run(dir, duty)
            LATCH_SET(open)      -> latch_set(open)      # 1=开闩, 0=关闩
Ctx:        state
```

转移表（未列出的组合一律 REJECT）：

```
+---------+---------+------------+------------------------+
| State   | kind    | Next State | Worker call            |
+---------+---------+------------+------------------------+
| CLOSED  | PERSON  | OPENING    | motor_run(OPEN, 80)    |
| OPENING | TIMEOUT | OPEN       | latch_set(1)           |
| OPEN    | PERSON  | OPEN       | (none)                 |
| OPEN    | TIMEOUT | CLOSING    | motor_run(CLOSE, 80)   |
| CLOSING | PERSON  | OPENING    | motor_run(OPEN, 80)    |
| CLOSING | TIMEOUT | CLOSED     | latch_set(0)           |
| CLOSED  | TIMEOUT | REJECT     | -                      |
| OPENING | PERSON  | REJECT     | -                      |
+---------+---------+------------+------------------------+
```

`dir` / `open` 来自判定（下一状态）；`sensor_id` / `elapsed_ms` 留在 Event 上，供日志和真实驱动使用，不进入转移表。

状态图：

```
  CLOSED --PERSON--> OPENING --TIMEOUT--> OPEN --TIMEOUT--> CLOSING
     ^                  ^                  |                  |
     |                  |                  | PERSON           | PERSON
     |                  +------------------+                  |
     |                  +-------------------------------------+
     +------------------------------ TIMEOUT -----------------+
```

## 一次事件的三层走查

当前 `CLOSED`，收到 `PERSON(sensor_id=1)`：

```
[EVENT]    PERSON(sensor_id=1) | state=CLOSED
[DECISION] CLOSED + PERSON -> OPENING
[DISPATCH] MOTOR_RUN(dir=OPEN, duty=80)
[EXEC]     motor_run(OPEN, 80)
```

关门途中另一传感器触发：

```
[EVENT]    PERSON(sensor_id=2) | state=CLOSING
[DECISION] CLOSING + PERSON -> OPENING
[DISPATCH] MOTOR_RUN(dir=OPEN, duty=80)
[EXEC]     motor_run(OPEN, 80)
```

同一 kind `PERSON`：关闭时表示「开始开门」，已开时表示「保持」（空 Action），关闭途中表示「反向」。

## 如果把执行写进 FSM

```
if state == CLOSED and event.kind == PERSON:
    pwm_set_duty(80)
    sleep(2)
    state = OPEN
```

判定被阻塞在 IO 和延时上。正确拆法：FSM 瞬间返回 `motor_run(OPEN, 80)`，行程结束作为 `TIMEOUT(elapsed_ms=...)` 再进来判定。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

脚本化事件：`TIMEOUT(500), PERSON(1), TIMEOUT(2000), PERSON(1), TIMEOUT(5000), PERSON(2), TIMEOUT(2000), TIMEOUT(5000), TIMEOUT(2000)`，初始 `CLOSED`。
