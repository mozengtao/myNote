# 示例一：图书馆借阅系统

## 场景

一个最小的图书馆借阅系统：`Library` 管理 `Book` 与 `Member`，会员可以借书、还书，图书馆可以查询逾期未还的借阅记录。

## 对应的心智模型要点

对照 [`complex_obj_organization.md`](../../complex_obj_organization.md) 第十七节 Checklist：

| Checklist 原则 | 本示例中的体现 |
|---|---|
| Object Graph | `Library` 组合 `Book` 集合与 `Member` 集合，`Loan` 把两者关联起来，形成一张对象图，而不是三个互不相关的列表 |
| Encapsulation（对象管理自己的状态） | `Book.borrow()` / `Book.return_book()` 内部校验是否已被借出，外部不能直接改 `book._borrowed_by` |
| Hide Structure | `Library.borrow_book(isbn, member_id, today)` 隐藏了内部用 `dict` 存书和会员的事实 |
| Repository | `LibraryRepository` 负责按 ISBN / 会员号查找对象，以及封装"逾期未还"这类业务查询，调用方不用自己遍历底层集合 |

## 运行方式

```bash
cd python/complex_obj_organization_examples/library_lending_system
python3 main.py
```
