"""
模拟"物流公司 API"（例如顺丰/DHL 的下单接口，或者需要 SSH 登录到仓库
终端系统触发出库指令的场景）。

同样地，这里只处理原始字符串参数，用 print 代替真实的网络/SSH 调用，
并返回一个模拟的运单号。
"""


def request_pickup(order_id: str, address: str) -> str:
    print(f"[CourierAPI] 正在调用物流公司接口，为订单 {order_id} 在「{address}」安排揽收 ...")
    tracking_number = f"TRACK-{order_id}"
    print(f"[CourierAPI] （模拟）揽收成功，运单号：{tracking_number}")
    return tracking_number
