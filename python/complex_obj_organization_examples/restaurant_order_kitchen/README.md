# 示例四：餐厅点单与厨房工作流

## 场景

服务员在 POS 机上录入原始点单（`sku` + 数量），`OrderFactory` 把它转成 `Order` 对象图，再经过 `Kitchen`（备餐）和 `Billing`（结账）两个 Service，由 `OrderFulfillmentWorkflow` 编排整个流程，最终产出一张 `Receipt`。

## 对应的心智模型要点

对照 [`complex_obj_organization.md`](../../complex_obj_organization.md) 第十七节 Checklist：

| Checklist 原则 | 本示例中的体现 |
|---|---|
| Factory | `OrderFactory.create_from_ticket(order_id, table_no, raw_ticket)` 把原始 `dict` 列表转成完整的 `Order` 对象图，业务代码不用关心拼装细节 |
| Object Flow | 同一个 `Order` 对象依次流过 `Kitchen.prepare()` → `Billing.charge()`，状态字段（`status`）逐步被推进，从未退化成 dict 或字符串 |
| Encapsulation | `Order` 的状态流转（`PLACED → PREPARING → READY → PAID`）只能通过 `mark_preparing()` / `mark_ready()` / `mark_paid()` 推进，且内部会校验顺序是否合法 |
| Workflow 驱动 Object Flow | `OrderFulfillmentWorkflow` 只负责编排 `Kitchen` 与 `Billing` 的调用顺序，本身不包含具体业务规则 |

## 运行方式

```bash
cd python/complex_obj_organization_examples/restaurant_order_kitchen
python3 main.py
```
