from .customer import Customer
from .product import Product


class OrderItem:
    """订单中的一行：某个商品 + 购买数量。"""

    def __init__(self, product: Product, quantity: int):
        self.product = product
        self.quantity = quantity

    @property
    def subtotal(self) -> float:
        return self.product.price * self.quantity

    def __repr__(self):
        return f"OrderItem({self.product.name!r} x{self.quantity})"


class Order:
    """
    订单对象——整条流水线中真正流动的东西。

    从 Repository 产生开始，一路流经
    InventoryService -> PaymentService -> ShippingService -> ReportService，
    始终是同一个 Order 对象，只是状态字段被逐步补充，
    从未被降级为 dict/JSON 再传递。
    """

    def __init__(self, order_id: str, customer: Customer, items: list[OrderItem]):
        self.order_id = order_id
        self.customer = customer
        self.items = items

        # 状态字段：随着对象流经各个 Service 被逐步填充
        self.status = "PENDING"
        self.payment_status = None
        self.tracking_number = None

    @property
    def total_amount(self) -> float:
        return sum(item.subtotal for item in self.items)

    def __repr__(self):
        return (
            f"Order({self.order_id}, customer={self.customer.name!r}, "
            f"status={self.status}, items={len(self.items)})"
        )
