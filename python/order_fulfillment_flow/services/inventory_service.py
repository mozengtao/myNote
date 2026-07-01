from domain.order import Order


class InventoryService:
    """
    库存服务：输入 Order，输出还是同一个 Order（只是状态被更新）。

    Service 只关心业务规则本身（库存是否足够），
    完全不关心 Order 从哪里来、库存数据存在哪里。
    """

    def reserve(self, order: Order) -> Order:
        for item in order.items:
            reserved = item.product.reserve(item.quantity)
            if not reserved:
                order.status = "OUT_OF_STOCK"
                print(
                    f"[InventoryService] 订单 {order.order_id} 库存不足："
                    f"{item.product.name} 库存 {item.product.stock}，需要 {item.quantity}"
                )
                return order

        order.status = "RESERVED"
        print(f"[InventoryService] 订单 {order.order_id} 库存锁定成功。")
        return order
