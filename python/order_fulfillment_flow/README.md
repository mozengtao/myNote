# 电商订单履约流程示例（Object Flow Driven 实战）

本目录是 [Python 对象流驱动（Object Flow Driven）心智模型](../object_flow_driven.md)的一个**可运行示例项目**，用另一个经典场景（电商订单履约），而不是文档中"网络设备巡检（NetBox + SSH）"的场景，帮助巩固同一套心智模型。

## 场景

```
客户下单
    │
    ▼
库存校验 / 锁定
    │
    ▼
支付扣款
    │
    ▼
物流揽收
    │
    ▼
生成履约报告
```

这是一个几乎每个人都熟悉的"电商下单"流程，天然可以映射到 Object Flow Driven 的每一层。

## 目录结构

```
order_fulfillment_flow/
├── README.md                          本说明文档
├── main.py                            入口：组装依赖并运行 workflow
│
├── domain/                            领域对象层
│   ├── customer.py                    Customer
│   ├── product.py                     Product
│   └── order.py                       OrderItem / Order
│
├── repositories/                      仓储层：原始数据 -> 领域对象
│   ├── customer_repository.py
│   ├── product_repository.py
│   └── order_repository.py
│
├── services/                          业务服务层：Object -> Object
│   ├── inventory_service.py           库存校验/锁定
│   ├── payment_service.py             支付扣款
│   ├── shipping_service.py            物流揽收
│   └── report_service.py              生成 Markdown 报告
│
├── workflows/                         编排层：对象流水线
│   └── order_fulfillment_workflow.py
│
└── infrastructure/                    基础设施层：外部系统交互
    ├── fake_database.py               模拟数据库原始数据
    ├── fake_payment_gateway.py        模拟支付网关（print 代替真实 HTTP/SSH 调用）
    └── fake_courier_api.py            模拟物流公司 API（print 代替真实 HTTP/SSH 调用）
```

## 心智模型映射

`object_flow_driven.md` 的核心结论是：

```
Object
    │
    ▼
Service
    │
    ▼
Workflow
    │
    ▼
Repository
    │
    ▼
Infrastructure
```

在本示例中，这套结构对应关系如下：

| 心智模型层级 | 本示例对应角色 | 职责 |
|---|---|---|
| Domain Object | `Customer` / `Product` / `Order` / `OrderItem` | 携带状态与行为，是全流程中真正流动的东西 |
| Repository | `CustomerRepository` / `ProductRepository` / `OrderRepository` | 把"原始数据"（本示例中是内存 dict，真实项目可能是数据库/REST）转换为领域对象 |
| Service | `InventoryService` / `PaymentService` / `ShippingService` / `ReportService` | 输入 `Order`，输出 `Order`，只关心业务规则本身 |
| Workflow | `OrderFulfillmentWorkflow` | 编排 Service 的调用顺序，不关心每一步内部怎么实现 |
| Infrastructure | `fake_database.py` / `fake_payment_gateway.py` / `fake_courier_api.py` | 与"外部世界"打交道，只认识原始参数（字符串/数字），不认识 `Order` 等领域对象 |

## 对象流动图

```
fake_database (原始 dict)
        │
        ▼
   OrderRepository
        │
        ▼
       Order  ──────────────────────────────┐
        │                                   │
        ▼                                   │
 InventoryService.reserve(order)            │  始终是同一个
        │                                   │  Order 对象，
        ▼                                   │  只是状态字段
       Order                                │  被逐步补充：
        │                                   │
        ▼                                   │  status
 PaymentService.charge(order)               │  payment_status
        │                                   │  tracking_number
        ▼                                   │
       Order                                │
        │                                   │
        ▼                                   │
 ShippingService.arrange(order)             │
        │                                   │
        ▼                                   │
       Order  ──────────────────────────────┘
        │
        ▼
 ReportService.generate(orders)
        │
        ▼
   Markdown 报告（唯一允许把对象"降级"为文本的地方）
```

从 `OrderRepository` 产生开始，一直到 `ReportService` 打印报告之前，流动的始终是 `Order` 对象本身，而不是 dict 或 JSON。这正是 Object Flow Driven 想强调的核心心智模型：

> **领域对象（Object）是系统中的第一公民，而不是数据（dict/JSON）。**

## "SSH / 外部调用"如何被替代

文档中的网络自动化案例涉及真实的 SSH 登录设备；本示例中的对应物是"调用支付网关"和"调用物流公司 API"——同样属于**基础设施层与外部世界交互**的场景。

为了让示例可以离线直接运行，`infrastructure/fake_payment_gateway.py` 和 `infrastructure/fake_courier_api.py` 都用 `print(...)` 模拟了真实的网络请求过程，并返回一个模拟结果（例如运单号）。业务层（Service）完全不知道这背后是真实的 HTTP 调用、SSH 命令，还是这里的打印语句——这也正是"业务与基础设施隔离"的意义所在。

## 运行示例

```bash
cd python/order_fulfillment_flow
python3 main.py
```

运行后会看到两笔订单的完整处理日志（其中一笔因库存不足在中途被业务规则短路），以及最终生成的 Markdown 格式履约报告，直观展示对象是如何在各层之间"流动"并被逐步"补充状态"的。

## 与文档中网络自动化案例的对比

| 对比项 | 文档：网络设备巡检 | 本示例：电商订单履约 |
|---|---|---|
| 核心领域对象 | `Device` / `Interface` / `Vlan` | `Customer` / `Product` / `Order` |
| Repository 数据来源 | NetBox REST API | 模拟数据库（`fake_database.py`） |
| 外部系统交互 | SSH 登录设备采集接口状态 | 调用支付网关 / 物流公司 API |
| Service 职责 | 健康检查、部署、备份 | 库存锁定、支付、发货 |
| Workflow 输出 | 打印巡检结果 | 生成 Markdown 履约报告 |
| 共同点 | 全程都是同一个领域对象在流动，从未退化为 dict/JSON | 同上 |

两个场景表面上毫不相关，但底层遵循的是同一套 Object Flow Driven 心智模型——这也说明这套心智模型具有跨领域的普适性。
