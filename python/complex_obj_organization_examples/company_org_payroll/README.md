# 示例五：公司组织架构与工资单

## 场景

用 `OrgChartBuilder` 逐步构建 `Company → Department → Employee` 的组织树，再由独立的 `PayrollService` 消费这棵对象树，计算每个员工的工资单。

## 对应的心智模型要点

对照 [`complex_obj_organization.md`](../../complex_obj_organization.md) 第十七节 Checklist：

| Checklist 原则 | 本示例中的体现 |
|---|---|
| Builder | `OrgChartBuilder` 通过链式调用 `add_department()` / `add_employee()` 逐步构建复杂组织树，区别于示例四中"一次性从原始数据转换"的 `OrderFactory` |
| 避免 God Object | `Company` / `Employee` 只描述组织结构本身，不包含发工资、算奖金的逻辑；这些逻辑独立在 `PayrollService` 中 |
| Composition | `Company` has `Department` has `Employee`，逐层组合而不是继承 |
| Service（无状态消费对象） | `PayrollService.run_payroll(company)` 只是"消费"传入的 `Company` 对象图产出工资单列表，自己不持有员工数据 |

## 运行方式

```bash
cd python/complex_obj_organization_examples/company_org_payroll
python3 main.py
```
