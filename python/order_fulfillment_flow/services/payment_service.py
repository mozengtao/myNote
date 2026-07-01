from domain.order import Order
from infrastructure import fake_payment_gateway


class PaymentService:
    """
    支付服务：输入 Order，输出 Order。

    业务层（本类）只认识 Order 对象；
    "怎么调用支付网关"这件事被完全下放到 infrastructure 层，
    Service 甚至不知道对方是 HTTP 请求还是别的协议。
    """

    def charge(self, order: Order) -> Order:
        if order.status != "RESERVED":
            print(f"[PaymentService] 订单 {order.order_id} 未完成库存锁定，跳过支付。")
            return order

        success = fake_payment_gateway.charge(order.customer.name, order.total_amount)

        order.payment_status = "PAID" if success else "FAILED"
        order.status = "PAID" if success else "PAYMENT_FAILED"
        return order
