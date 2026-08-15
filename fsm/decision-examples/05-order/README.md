# 05 订单履约：一次判定，多个 Worker

> **钉死的心智：** 一次 Decision 可以产出 **Action List**。FSM 不扣库存、不发物流、不加积分；它只决定「这些事现在该做」，Dispatcher 再分发给各个 Worker。

支付成功不是「订单对象自己把全世界都办了」。FSM 从 `CREATED + PAY_SUCCESS` 判定下一状态是 `PAID`，并列出四件事：扣库存、通知物流、加积分、发短信。已发货后再取消被 REJECT——不是 Worker 拒绝，是判定层根本不发动作。

## 状态 / 事件 / 动作

```
State:   CREATED, PAID, SHIPPED, CANCELLED
Event:   PAY_SUCCESS, SHIP, CANCEL
Action:  DEDUCT_STOCK, NOTIFY_LOGISTICS, ADD_POINTS, SEND_SMS, RELEASE_STOCK
```

转移表（未列出的组合一律 REJECT）：

```
+----------+-------------+------------+-------------------------------------------+
| State    | Event       | Next State | Actions                                   |
+----------+-------------+------------+-------------------------------------------+
| CREATED  | PAY_SUCCESS | PAID       | DEDUCT_STOCK, NOTIFY_LOGISTICS,           |
|          |             |            | ADD_POINTS, SEND_SMS                      |
| PAID     | SHIP        | SHIPPED    | SEND_SMS                                  |
| CREATED  | CANCEL      | CANCELLED  | SEND_SMS                                  |
| PAID     | CANCEL      | CANCELLED  | RELEASE_STOCK, SEND_SMS                   |
| CREATED  | SHIP        | REJECT     | -                                         |
| SHIPPED  | CANCEL      | REJECT     | -                                         |
+----------+-------------+------------+-------------------------------------------+
```

状态图：

```
  CREATED --PAY_SUCCESS--> PAID --SHIP--> SHIPPED
     |                      |
     +--CANCEL--> CANCELLED <+
```

## 一次事件的三层走查

当前 `CREATED`，收到 `PAY_SUCCESS`：

```
[EVENT]    PAY_SUCCESS | state=CREATED
[DECISION] CREATED + PAY_SUCCESS -> PAID  actions=[DEDUCT_STOCK, NOTIFY_LOGISTICS, ADD_POINTS, SEND_SMS]
[DISPATCH] DEDUCT_STOCK
[EXEC]     inventory -> DEDUCT
[DISPATCH] NOTIFY_LOGISTICS
[EXEC]     logistics -> CREATE_SHIPMENT
[DISPATCH] ADD_POINTS
[EXEC]     points -> ADD
[DISPATCH] SEND_SMS
[EXEC]     sms -> SEND
```

四个 Worker 各自干活。FSM 不知道库存怎么扣、短信怎么发。

已发货再取消：

```
[EVENT]    CANCEL | state=SHIPPED
[DECISION] REJECT SHIPPED + CANCEL  (state unchanged)
```

没有回滚库存，因为判定层没有发出任何 Action。

## 如果把执行写进 FSM

```
if state == CREATED and event == PAY_SUCCESS:
    db.exec("UPDATE stock ...")
    http.post(logistics)
    redis.incr(points)
    smtp.send(...)
    state = PAID
```

一次支付把四个系统的失败、重试、超时全部塞进判定函数。正确拆法：FSM 只产出 Action List；每个 Worker 独立重试。库存失败不应让「判定下一状态」本身变得不可测试。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

三个脚本化场景，都从 `CREATED` 开始：

- `happy-path`: `SHIP, PAY_SUCCESS, SHIP, CANCEL`（首尾两次 REJECT）
- `cancel-created`: `CANCEL`（未支付取消，只发短信）
- `cancel-paid`: `PAY_SUCCESS, CANCEL`（支付后取消，释放库存 + 短信）
