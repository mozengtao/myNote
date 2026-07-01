# 示例三：银行账户转账

## 场景

`Bank` 管理若干 `Account`，`TransferService` 负责在两个账户之间执行转账，包含余额不足时的拒绝与回滚逻辑。

## 对应的心智模型要点

对照 [`complex_obj_organization.md`](../../complex_obj_organization.md) 第十七节 Checklist：

| Checklist 原则 | 本示例中的体现 |
|---|---|
| Encapsulation | `Account.balance` 是只读 `property`，只能通过 `deposit()` / `withdraw()` 修改余额，外部直接赋值会抛 `AttributeError` |
| Service（无状态） | `TransferService` 没有 `__init__` 存任何账户状态，`transfer(from_account, to_account, amount)` 每次都是"消费"传入的对象 |
| Repository | `Bank` 承担账户的创建与查找职责（`open_account` / `find_account`），业务规则不下沉到 `Bank` 里 |
| Object Flow | 账户对象在 `Bank`（创建/查找） → `TransferService`（业务规则） 之间流动，状态变化始终落在账户对象自身，而不是某个全局字典 |

## 运行方式

```bash
cd python/complex_obj_organization_examples/bank_account_transfer
python3 main.py
```
