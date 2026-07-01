from domain.order import Order
from infrastructure import fake_courier_api


class ShippingService:
    """物流服务：输入 Order，输出 Order（补充 tracking_number）。"""

    def arrange(self, order: Order) -> Order:
        if order.status != "PAID":
            print(f"[ShippingService] 订单 {order.order_id} 未完成支付，跳过发货。")
            return order

        tracking_number = fake_courier_api.request_pickup(
            order.order_id, order.customer.address
        )

        order.tracking_number = tracking_number
        order.status = "SHIPPED"
        return order
