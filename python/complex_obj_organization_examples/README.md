# Python 复杂对象组织心智模式 —— 补充示例

本目录是 [`python/complex_obj_organization.md`](../complex_obj_organization.md)（Python 复杂 Object 组织以及 workflow 的心智模式）的**补充代码示例**，用与文档正文场景（网络设备 `Router`/`Interface`、电商 `User`/`Order`/`Item`）不同的场景，帮助巩固同一套心智模型。

前 5 个示例都是独立子目录，包含：

- `main.py` —— 完整、可直接运行的单文件示例（关键设计决策处有注释说明）
- `README.md` —— 场景介绍 + 该示例重点演示的心智模型原则

第 6 个示例（`vmc_reboot_automation`）不同：它演示的是**分层架构**（DDD + Object Flow + Infrastructure Layer），代码按层拆分成多个模块，而不是单个 `main.py`。

## 示例总览

| 示例 | 场景 | 重点演示的心智模型原则 |
|---|---|---|
| [`library_lending_system`](library_lending_system) | 图书馆借阅系统 | Object Graph、Encapsulation、Repository |
| [`smart_home_control`](smart_home_control) | 智能家居设备控制 | Composition（Has-A）、Hide Structure、Single Responsibility |
| [`bank_account_transfer`](bank_account_transfer) | 银行账户转账 | Encapsulation、Service 无状态、Repository |
| [`restaurant_order_kitchen`](restaurant_order_kitchen) | 餐厅点单与厨房工作流 | Factory、Object Flow、Workflow |
| [`company_org_payroll`](company_org_payroll) | 公司组织架构与工资单 | Builder、避免 God Object、Composition |
| [`vmc_reboot_automation`](vmc_reboot_automation) | 网络设备（VMC）批量重启自动化 | 分层架构（Infrastructure / Domain / Workflow / Application）、Object Flow、Infrastructure 可替换 |

前 5 个示例对应的原则均可在 `complex_obj_organization.md` 第十七节的 Checklist 表格中找到，第 6 个示例对应第二十节的分层架构说明。

## 运行方式

每个示例都可以独立运行：

```bash
cd python/complex_obj_organization_examples/<示例目录名>
python3 main.py
```

例如：

```bash
cd python/complex_obj_organization_examples/library_lending_system
python3 main.py
```

其中前 5 个示例仅使用 Python 标准库；`vmc_reboot_automation` 默认使用本地模拟的 SSH 连接，同样无需额外依赖即可运行（详见该目录下的 `README.md`）。
