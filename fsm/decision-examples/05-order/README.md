# 05 订单履约：一次判定，把订单数据填进多个带参 Worker

> **钉死的心智：** Action 是对数据的函数调用。FSM 不扣库存；它构造 `inventory_deduct("SKU-7", 2)`。订单字段活在 Ctx 里，供后续不带 payload 的 `SHIP` / `CANCEL` 使用。

支付成功一次判定拆成四个带参调用。已发货再取消被 REJECT——不是 Worker 拒绝，是判定层根本不发动作。

## 状态 / 事件 / 动作

```
State:      CREATED, PAID, SHIPPED, CANCELLED
Event:      PAY_SUCCESS(order_id, sku, qty, amount, phone)
            SHIP, CANCEL
Action:     DEDUCT_STOCK(sku, qty)              -> inventory_deduct(sku, qty)
            NOTIFY_LOGISTICS(order_id, sku)     -> logistics_create(order_id, sku)
            ADD_POINTS(order_id, amount)        -> points_add(order_id, amount)
            SEND_SMS(phone, msg)                -> sms_send(phone, msg)
            RELEASE_STOCK(sku, qty)             -> inventory_release(sku, qty)
Ctx:        state, order_id, sku, qty, amount, phone
```

转移表（未列出的组合一律 REJECT）：

```
+----------+-------------+------------+---------------------------------------+
| State    | kind        | Next State | Worker calls                          |
+----------+-------------+------------+---------------------------------------+
| CREATED  | PAY_SUCCESS | PAID       | inventory_deduct(sku, qty)            |
|          |             |            | logistics_create(order_id, sku)       |
|          |             |            | points_add(order_id, amount)          |
|          |             |            | sms_send(phone, "paid")               |
| PAID     | SHIP        | SHIPPED    | sms_send(phone, "shipped")            |
| CREATED  | CANCEL      | CANCELLED  | sms_send(phone, "cancelled")          |
| PAID     | CANCEL      | CANCELLED  | inventory_release(sku, qty)           |
|          |             |            | sms_send(phone, "cancelled")          |
| CREATED  | SHIP        | REJECT     | -                                     |
| SHIPPED  | CANCEL      | REJECT     | -                                     |
+----------+-------------+------------+---------------------------------------+
```

`PAY_SUCCESS` 的参数来自 Event；`SHIP` / `CANCEL` 的参数来自 Ctx（订单创建时就已经有了）。

状态图：

```
  CREATED --PAY_SUCCESS--> PAID --SHIP--> SHIPPED
     |                      |
     +--CANCEL--> CANCELLED <+
```

## 一次事件的三层走查

当前 `CREATED`，收到支付回调：

```
[EVENT]    PAY_SUCCESS(order_id=1001, sku=SKU-7, qty=2, amount=99, phone=13800001111) | state=CREATED
[DECISION] CREATED + PAY_SUCCESS -> PAID
[DISPATCH] DEDUCT_STOCK(sku=SKU-7, qty=2)
[EXEC]     inventory_deduct("SKU-7", 2)
[DISPATCH] NOTIFY_LOGISTICS(order_id=1001, sku=SKU-7)
[EXEC]     logistics_create(1001, "SKU-7")
[DISPATCH] ADD_POINTS(order_id=1001, amount=99)
[EXEC]     points_add(1001, 99)
[DISPATCH] SEND_SMS(phone=13800001111, msg=paid)
[EXEC]     sms_send("13800001111", "paid")
```

四个 Worker 各自拿到自己的参数。FSM 不知道库存怎么扣、短信怎么发。

已发货再取消：

```
[EVENT]    CANCEL | state=SHIPPED
[DECISION] REJECT SHIPPED + CANCEL  (state unchanged)
```

没有回滚库存，因为判定层没有发出任何 Action。

## 如果把执行写进 FSM

```
if state == CREATED and event.kind == PAY_SUCCESS:
    db.exec("UPDATE stock SET qty=qty-? WHERE sku=?", event.qty, event.sku)
    http.post(logistics)
    redis.incr(points)
    smtp.send(...)
    state = PAID
```

一次支付把四个系统的失败、重试、超时全部塞进判定函数。正确拆法：FSM 只产出带参 Action List；每个 Worker 独立重试。

## 怎么跑

```
python3 python/main.py
gcc -o /tmp/fsm-demo c/main.c && /tmp/fsm-demo
(cd go && go run main.go)
```

三个脚本化场景，订单在 INIT 时已经有 `order_id=1001 sku=SKU-7 qty=2 amount=99 phone=13800001111`：

- `happy-path`: `SHIP, PAY_SUCCESS, SHIP, CANCEL`（首尾两次 REJECT）
- `cancel-created`: `CANCEL`（未支付取消，只发短信）
- `cancel-paid`: `PAY_SUCCESS, CANCEL`（支付后取消，释放库存 + 短信）
