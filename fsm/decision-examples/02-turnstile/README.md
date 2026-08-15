# 02 旋转门：非法转移必须拒绝

> **钉死的心智：** 表里没有的 `(State, Event)` 不是「偷偷忽略」，而是 **REJECT**：状态不变、不发 Action。

经典 CS 旋转门只有两态。`COIN` 在 `LOCKED` 表示投币开锁；在 `UNLOCKED` 表示重复投币，非法。`PUSH` 正好相反：解锁后推门通过，上锁时硬推则拒绝。

## 状态 / 事件 / 动作

```
State:   LOCKED, UNLOCKED
Event:   COIN, PUSH
Action:  UNLOCK, LOCK
```

转移表（未列出的组合一律 REJECT）：

```
+----------+-------+------------+---------+
| State    | Event | Next State | Actions |
+----------+-------+------------+---------+
| LOCKED   | COIN  | UNLOCKED   | UNLOCK  |
| UNLOCKED | PUSH  | LOCKED     | LOCK    |
| LOCKED   | PUSH  | REJECT     | -       |
| UNLOCKED | COIN  | REJECT     | -       |
+----------+-------+------------+---------+
```

状态图：

```
  +--------+  COIN   +----------+
  | LOCKED | ------> | UNLOCKED |
  +--------+         +----------+
      ^                    |
      +------- PUSH -------+

  REJECT: LOCKED+PUSH, UNLOCKED+COIN
```

## 一次事件的三层走查

当前 `LOCKED`，收到 `PUSH`（没投币就推门）：

```
[EVENT]    PUSH | state=LOCKED
[DECISION] REJECT LOCKED + PUSH  (state unchanged)
```

没有 `[DISPATCH]`，没有 `[EXEC]`。判定失败时执行层根本不会被叫醒。

合法路径：`LOCKED + COIN` → `UNLOCKED`，动作 `UNLOCK`，mechanism Worker 开锁。

## 如果把执行写进 FSM

```
if event == PUSH:
    motor_spin()     # 不管当前锁没锁，直接转门
```

未投币也能进站。拒绝非法转移是安全边界，不是「省一行代码」。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

脚本化事件：`COIN, PUSH, PUSH, COIN, COIN, PUSH`，初始状态 `LOCKED`。其中第 3、第 5 个事件应被 REJECT。
