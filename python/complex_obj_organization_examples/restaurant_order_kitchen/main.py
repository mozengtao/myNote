"""
餐厅点单与厨房工作流 —— 完整可运行示例

本示例重点演示的心智模型原则（对应 complex_obj_organization.md 第十七节 Checklist）：
    1. Factory        —— OrderFactory.create_from_ticket() 把原始点单（dict/字符串）转成完整的 Order 对象图，
       业务代码不需要关心"怎么从原始数据拼出 Order"这件事。
    2. Object Flow     —— 同一个 Order 对象依次流过 Kitchen（备餐）→ Billing（结账），
       每一步只在对象上补充状态，而不是转成 dict 再转回来。

运行方式：
    python3 main.py
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum


class OrderStatus(Enum):
    PLACED = "已下单"
    PREPARING = "备餐中"
    READY = "已出餐"
    PAID = "已结账"


# ---------------------------------------------------------------------------
# 一、Domain Object
# ---------------------------------------------------------------------------

@dataclass
class MenuItem:
    """菜单里的一道菜，只描述"这道菜是什么、多少钱"。"""

    sku: str
    name: str
    price: float
    prep_minutes: int


@dataclass
class OrderLine:
    """订单中的一行：某道菜 + 数量，把 MenuItem 和数量组合起来。"""

    item: MenuItem
    quantity: int

    @property
    def subtotal(self) -> float:
        return self.item.price * self.quantity


class Order:
    """
    一张订单。Order 拥有若干 OrderLine（Composition），
    并自己管理状态流转（PLACED -> PREPARING -> READY -> PAID），
    外部不能跳过流程直接把 status 设成 PAID。
    """

    def __init__(self, order_id: str, table_no: int):
        self.order_id = order_id
        self.table_no = table_no
        self.lines: list[OrderLine] = []
        self.status = OrderStatus.PLACED

    def add_line(self, item: MenuItem, quantity: int) -> None:
        self.lines.append(OrderLine(item=item, quantity=quantity))

    @property
    def total(self) -> float:
        return sum(line.subtotal for line in self.lines)

    @property
    def total_prep_minutes(self) -> int:
        """一张订单的备餐时间取所有菜品中最长的那个（并行备餐），而不是简单相加。"""
        return max((line.item.prep_minutes for line in self.lines), default=0)

    def mark_preparing(self) -> None:
        if self.status != OrderStatus.PLACED:
            raise ValueError(f"订单 {self.order_id} 当前状态为 {self.status}，无法开始备餐")
        self.status = OrderStatus.PREPARING

    def mark_ready(self) -> None:
        if self.status != OrderStatus.PREPARING:
            raise ValueError(f"订单 {self.order_id} 当前状态为 {self.status}，无法标记为已出餐")
        self.status = OrderStatus.READY

    def mark_paid(self) -> None:
        if self.status != OrderStatus.READY:
            raise ValueError(f"订单 {self.order_id} 当前状态为 {self.status}，还未出餐，不能结账")
        self.status = OrderStatus.PAID

    def __repr__(self) -> str:
        return f"<Order {self.order_id} 桌号={self.table_no} 状态={self.status.value} 总价={self.total:.2f}>"


# ---------------------------------------------------------------------------
# 二、OrderFactory：把原始点单数据转成完整对象图
# ---------------------------------------------------------------------------

class OrderFactory:
    """
    OrderFactory 负责：
        解析原始点单（比如服务员在 POS 机上敲的 dict）
        查菜单、创建 OrderLine
        拼装成完整的 Order 对象图
        校验基本合法性（比如点了菜单里不存在的菜）

    业务代码只需要调用 OrderFactory.create_from_ticket(...)，
    完全不需要知道"怎么把一堆字符串变成 Order"这件事的细节。
    """

    def __init__(self, menu: dict[str, MenuItem]):
        self._menu = menu

    def create_from_ticket(self, order_id: str, table_no: int, raw_ticket: list[dict]) -> Order:
        order = Order(order_id=order_id, table_no=table_no)
        for entry in raw_ticket:
            sku = entry["sku"]
            quantity = entry["quantity"]
            if sku not in self._menu:
                raise ValueError(f"菜单中不存在编号为 {sku} 的菜品")
            order.add_line(self._menu[sku], quantity)
        return order


# ---------------------------------------------------------------------------
# 三、Kitchen / Billing：Service，只消费 Order，不持有 Order 状态
# ---------------------------------------------------------------------------

class Kitchen:
    """厨房：负责把订单从"已下单"推进到"已出餐"，不关心结账逻辑。"""

    def prepare(self, order: Order) -> None:
        order.mark_preparing()
        print(f"  [Kitchen] 开始备餐订单 {order.order_id}，预计 {order.total_prep_minutes} 分钟")
        for line in order.lines:
            print(f"    - {line.item.name} x{line.quantity}")
        order.mark_ready()
        print(f"  [Kitchen] 订单 {order.order_id} 已出餐")


@dataclass
class Receipt:
    """一张小票，是 Order 在"结账"这一步产出的衍生对象，而不是把 Order 本身降级成字符串。"""

    order_id: str
    table_no: int
    total: float

    def render(self) -> str:
        return f"订单 {self.order_id}（{self.table_no} 号桌）应付：{self.total:.2f} 元"


class Billing:
    """收银台：负责把"已出餐"的订单标记为"已结账"，并生成小票。"""

    def charge(self, order: Order) -> Receipt:
        order.mark_paid()
        return Receipt(order_id=order.order_id, table_no=order.table_no, total=order.total)


# ---------------------------------------------------------------------------
# 四、Workflow：编排 Order 依次流过 Kitchen -> Billing
# ---------------------------------------------------------------------------

class OrderFulfillmentWorkflow:
    """
    Workflow 只负责"按什么顺序调用哪些 Service"，
    自己不包含具体业务规则（备餐规则在 Kitchen，结账规则在 Billing）。
    """

    def __init__(self, kitchen: Kitchen, billing: Billing):
        self._kitchen = kitchen
        self._billing = billing

    def run(self, order: Order) -> Receipt:
        self._kitchen.prepare(order)
        receipt = self._billing.charge(order)
        return receipt


# ---------------------------------------------------------------------------
# 五、演示
# ---------------------------------------------------------------------------

def main() -> None:
    menu = {
        "N001": MenuItem(sku="N001", name="番茄炒蛋", price=28.0, prep_minutes=8),
        "N002": MenuItem(sku="N002", name="红烧肉", price=48.0, prep_minutes=20),
        "N003": MenuItem(sku="N003", name="米饭", price=3.0, prep_minutes=2),
    }

    factory = OrderFactory(menu=menu)

    # 原始点单数据（可以来自 POS 机 JSON、小程序表单等，本质就是一堆 dict）
    raw_ticket = [
        {"sku": "N001", "quantity": 1},
        {"sku": "N002", "quantity": 2},
        {"sku": "N003", "quantity": 3},
    ]

    order = factory.create_from_ticket(order_id="ORD-0001", table_no=7, raw_ticket=raw_ticket)
    print("=== 下单后的订单对象 ===")
    print(order)

    workflow = OrderFulfillmentWorkflow(kitchen=Kitchen(), billing=Billing())

    print("\n=== 工作流执行：备餐 -> 结账 ===")
    receipt = workflow.run(order)

    print("\n=== 结账后的订单对象 ===")
    print(order)

    print("\n=== 小票 ===")
    print(receipt.render())


if __name__ == "__main__":
    main()
