"""
模拟"支付网关"（例如支付宝/Stripe 的 HTTP API）。

真实实现中这里应该是一次真正的 HTTP 请求（requests.post(...)）。
本示例用 print 语句代替真实网络调用 —— 这与文档中提到的
"具体的 SSH 等步骤可以用打印代替"是同一个思路：

    基础设施层（Infrastructure）只跟"原始数据/协议"打交道（这里是简单的
    字符串和数字参数），完全不知道 Order/Customer 等领域对象的存在。
"""


def charge(customer_name: str, amount: float) -> bool:
    print(f"[PaymentGateway] 正在为 {customer_name} 发起扣款请求，金额 ¥{amount:.2f} ...")
    print("[PaymentGateway] （模拟）扣款成功。")
    return True
