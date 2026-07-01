from infrastructure import fake_database
from domain.order import Order, OrderItem
from repositories.customer_repository import CustomerRepository
from repositories.product_repository import ProductRepository


class OrderRepository:
    """
    组装型 Repository：把"订单原始数据"（只有 id 引用）
    与 CustomerRepository / ProductRepository 提供的对象拼装成完整的 Order。

    这一步之后，整条流水线里再也看不到 dict，只有 Order 对象在流动。
    """

    def __init__(self, customer_repo: CustomerRepository, product_repo: ProductRepository):
        self._customer_repo = customer_repo
        self._product_repo = product_repo

    def load_pending_orders(self) -> list[Order]:
        orders = []
        for raw in fake_database.RAW_PENDING_ORDERS:
            customer = self._customer_repo.get(raw["customer_id"])
            items = [
                OrderItem(self._product_repo.get(item["sku"]), item["qty"])
                for item in raw["items"]
            ]
            orders.append(Order(raw["order_id"], customer, items))
        return orders
