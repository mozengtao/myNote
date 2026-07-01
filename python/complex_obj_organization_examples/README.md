# Python 复杂对象组织心智模式 —— 5 个补充示例

本目录是 [`python/complex_obj_organization.md`](../complex_obj_organization.md)（Python 复杂 Object 组织以及 workflow 的心智模式）的**补充代码示例**，用 5 个与文档正文场景（网络设备 `Router`/`Interface`、电商 `User`/`Order`/`Item`）不同的场景，帮助巩固同一套心智模型。

每个示例都是一个独立的子目录，包含：

- `main.py` —— 完整、可直接运行的单文件示例（关键设计决策处有注释说明）
- `README.md` —— 场景介绍 + 该示例重点演示的心智模型原则

## 示例总览

| 示例 | 场景 | 重点演示的心智模型原则 |
|---|---|---|
| [`library_lending_system`](library_lending_system) | 图书馆借阅系统 | Object Graph、Encapsulation、Repository |
| [`smart_home_control`](smart_home_control) | 智能家居设备控制 | Composition（Has-A）、Hide Structure、Single Responsibility |
| [`bank_account_transfer`](bank_account_transfer) | 银行账户转账 | Encapsulation、Service 无状态、Repository |
| [`restaurant_order_kitchen`](restaurant_order_kitchen) | 餐厅点单与厨房工作流 | Factory、Object Flow、Workflow |
| [`company_org_payroll`](company_org_payroll) | 公司组织架构与工资单 | Builder、避免 God Object、Composition |

以上原则均对应 `complex_obj_organization.md` 第十七节的 Checklist 表格。

## 运行方式

每个示例都可以独立运行，无需额外依赖（仅使用 Python 标准库）：

```bash
cd python/complex_obj_organization_examples/<示例目录名>
python3 main.py
```

例如：

```bash
cd python/complex_obj_organization_examples/library_lending_system
python3 main.py
```
