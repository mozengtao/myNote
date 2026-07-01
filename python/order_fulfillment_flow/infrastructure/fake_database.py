"""
模拟"数据库"。

真实项目中这里可能是 MySQL / PostgreSQL / MongoDB 查询结果，
本示例中用 Python 原生的 list/dict 代替 —— 它们是"未经加工的原始数据"，
只有 Repository 层才知道如何把它们变成领域对象（Customer / Product / Order）。

业务层 (Service / Workflow) 永远不会直接看到这里的 dict。
"""

RAW_CUSTOMERS = [
    {"customer_id": "C001", "name": "张三", "address": "上海市浦东新区世纪大道 100 号"},
    {"customer_id": "C002", "name": "李四", "address": "北京市朝阳区建国路 88 号"},
]

RAW_PRODUCTS = [
    {"sku": "P-KEYBOARD", "name": "机械键盘", "price": 299.0, "stock": 5},
    {"sku": "P-MOUSE", "name": "无线鼠标", "price": 129.0, "stock": 10},
    {"sku": "P-MONITOR", "name": "27寸显示器", "price": 1599.0, "stock": 1},
]

RAW_PENDING_ORDERS = [
    {
        "order_id": "O-20260701-001",
        "customer_id": "C001",
        "items": [
            {"sku": "P-KEYBOARD", "qty": 1},
            {"sku": "P-MOUSE", "qty": 2},
        ],
    },
    {
        "order_id": "O-20260701-002",
        "customer_id": "C002",
        "items": [
            {"sku": "P-MONITOR", "qty": 2},  # 库存只有 1，用于演示库存不足场景
        ],
    },
]
