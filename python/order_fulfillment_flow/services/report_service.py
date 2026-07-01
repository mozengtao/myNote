from domain.order import Order


class ReportService:
    """
    报告服务：流水线的终点。

    输入依然是 Order 对象列表（不是 dict/JSON），
    在这一步才把对象"翻译"成人类可读的 Markdown 文本 —— 这是
    整条流水线中唯一允许把对象"降级"为文本的地方。
    """

    def generate(self, orders: list[Order]) -> str:
        lines = ["# 订单履约报告", ""]

        for order in orders:
            lines.append(f"## 订单 {order.order_id}")
            lines.append(f"- 客户：{order.customer.name}（{order.customer.address}）")
            lines.append(f"- 状态：**{order.status}**")
            lines.append(f"- 支付状态：{order.payment_status}")
            lines.append(f"- 运单号：{order.tracking_number or '无'}")
            lines.append("- 商品明细：")
            for item in order.items:
                lines.append(
                    f"  - {item.product.name} x{item.quantity}"
                    f"（单价 ¥{item.product.price:.2f}，小计 ¥{item.subtotal:.2f}）"
                )
            lines.append(f"- 订单总额：¥{order.total_amount:.2f}")
            lines.append("")

        report = "\n".join(lines)
        print(report)
        return report
