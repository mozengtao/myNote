# 02 旋转门：非法转移必须拒绝

> **钉死的心智：** 表里没有的 `(State, Event.kind)` 一律 REJECT。投币金额是 Event payload，要作为参数交给 `coin_box_credit(cents)`，不能丢在枚举里。

`COIN` 在 `LOCKED` 表示投币开锁，并带上 `cents`；在 `UNLOCKED` 是重复投币，非法。`PUSH` 正好相反。合法投币一次判定产出两个带参动作：入币箱 + 开锁。

## 状态 / 事件 / 动作

```
State:      LOCKED, UNLOCKED
Event:      COIN(cents), PUSH
Action:     CREDIT_BOX(cents)      -> coin_box_credit(cents)
            SET_MECHANISM(locked)  -> mechanism_set(locked)   # 0=开, 1=锁
Ctx:        state
```

转移表（未列出的组合一律 REJECT）：

```
+----------+------+------------+----------------------------------+
| State    | kind | Next State | Worker calls                     |
+----------+------+------------+----------------------------------+
| LOCKED   | COIN | UNLOCKED   | coin_box_credit(cents)           |
|          |      |            | mechanism_set(0)                 |
| UNLOCKED | PUSH | LOCKED     | mechanism_set(1)                 |
| LOCKED   | PUSH | REJECT     | -                                |
| UNLOCKED | COIN | REJECT     | -                                |
+----------+------+------------+----------------------------------+
```

`cents` 来自 Event；`locked` 来自判定（下一状态是开还是锁）。

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

当前 `LOCKED`，收到 `COIN(cents=100)`：

```
[EVENT]    COIN(cents=100) | state=LOCKED
[DECISION] LOCKED + COIN -> UNLOCKED
[DISPATCH] CREDIT_BOX(cents=100)
[EXEC]     coin_box_credit(100)
[DISPATCH] SET_MECHANISM(locked=0)
[EXEC]     mechanism_set(0)
```

没投币就推门：

```
[EVENT]    PUSH | state=LOCKED
[DECISION] REJECT LOCKED + PUSH  (state unchanged)
```

没有 `[DISPATCH]`，Worker 不会被叫醒。

## 如果把执行写进 FSM

```
if event.kind == PUSH:
    motor_spin()     # 不管锁没锁，直接转门
```

未投币也能进站。金额若不从 Event 传到 `coin_box_credit`，账会对不上。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

脚本化事件：`COIN(100), PUSH, PUSH, COIN(100), COIN(50), PUSH`，初始 `LOCKED`。第 3、第 5 个事件应被 REJECT。
