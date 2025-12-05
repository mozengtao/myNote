# Adapter Pattern (适配器模式)

## ASCII Diagram

```
Object Adapter (组合方式):
+-------------------+       +-------------------+
|      Client       |       |    Target         |<<interface>>
+-------------------+       +-------------------+
| + doWork()        |------>| + request()       |
+-------------------+       +-------------------+
                                    ^
                                    |
                            +-------+-------+
                            |    Adapter    |
                            +---------------+
                            | - adaptee     |------+
                            +---------------+      |
                            | + request()   |      |
                            +---------------+      |
                                                   |
                            +---------------+      |
                            |    Adaptee    |<-----+
                            +---------------+
                            | + specificReq()|
                            +---------------+

Class Adapter (继承方式):
+-------------------+       +-------------------+
|      Client       |       |    Target         |<<interface>>
+-------------------+       +-------------------+
| + doWork()        |------>| + request()       |
+-------------------+       +-------------------+
                                    ^
                                    |
                            +-------+-------+
                            |    Adapter    |
                            +---------------+
                            | + request()   |
                            +-------+-------+
                                    ^
                                    |
                            +-------+-------+
                            |    Adaptee    |
                            +---------------+
                            | + specificReq()|
                            +---------------+
```

**中文说明：**
- **Target（目标接口）**：客户端期望使用的接口
- **Adaptee（被适配者）**：现有的接口，与目标接口不兼容
- **Adapter（适配器）**：将 Adaptee 的接口转换为 Target 接口
- **Client（客户端）**：通过 Target 接口与适配器交互

---

## 核心思想

将一个类的接口**转换**成客户端期望的另一个接口。适配器模式让原本接口不兼容的类可以合作无间。就像电源适配器把 220V 转换成 5V 一样。

---

## 应用场景

1. **接口不兼容**：想使用现有类，但接口与需求不匹配
2. **遗留代码整合**：需要复用旧系统的功能但接口过时
3. **第三方库封装**：统一不同第三方库的接口
4. **实际应用**：
   - 支付网关适配（支付宝、微信、PayPal）
   - 数据格式转换（XML → JSON）
   - 日志库统一接口
   - 数据库 ORM 适配

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 解耦 | 客户端与被适配类解耦 |
| 复用 | 无需修改现有代码即可复用 |
| 灵活 | 可以适配多个不同的类 |
| 符合开闭原则 | 新增适配器无需修改现有代码 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 增加复杂度 | 引入额外的类 |
| 过度适配 | 过多适配器会使系统混乱 |
| 性能开销 | 多一层调用 |

---

## Python 代码示例

### 应用前：接口不兼容

```python
# 问题：不同支付服务的接口完全不同

class AlipaySDK:
    """支付宝 SDK - 有自己的接口"""
    
    def create_alipay_order(self, order_id, amount_yuan):
        return f"Alipay order {order_id} created: ¥{amount_yuan}"
    
    def alipay_pay(self, order_id):
        return f"Alipay payment for {order_id} successful"
    
    def query_alipay_status(self, order_id):
        return {"order_id": order_id, "status": "PAID", "platform": "alipay"}


class WeChatPaySDK:
    """微信支付 SDK - 完全不同的接口"""
    
    def unified_order(self, out_trade_no, total_fee_fen):
        # 微信用分为单位
        return {"prepay_id": f"wx_{out_trade_no}", "total": total_fee_fen}
    
    def wx_pay(self, prepay_id):
        return {"return_code": "SUCCESS", "prepay_id": prepay_id}
    
    def order_query(self, out_trade_no):
        return {"trade_state": "SUCCESS", "out_trade_no": out_trade_no}


class PayPalSDK:
    """PayPal SDK - 又是另一套接口"""
    
    def create_payment(self, invoice_number, usd_amount):
        return {"id": f"PAY-{invoice_number}", "state": "created"}
    
    def execute_payment(self, payment_id, payer_id):
        return {"id": payment_id, "state": "approved"}
    
    def get_payment(self, payment_id):
        return {"id": payment_id, "state": "approved"}


# 问题：业务代码需要分别处理每种支付方式
def checkout(payment_method, order_id, amount):
    """结账 - 代码混乱"""
    
    if payment_method == "alipay":
        sdk = AlipaySDK()
        sdk.create_alipay_order(order_id, amount)
        result = sdk.alipay_pay(order_id)
        status = sdk.query_alipay_status(order_id)
        return status["status"] == "PAID"
    
    elif payment_method == "wechat":
        sdk = WeChatPaySDK()
        amount_fen = int(amount * 100)  # 转换为分
        order_info = sdk.unified_order(order_id, amount_fen)
        sdk.wx_pay(order_info["prepay_id"])
        status = sdk.order_query(order_id)
        return status["trade_state"] == "SUCCESS"
    
    elif payment_method == "paypal":
        sdk = PayPalSDK()
        payment = sdk.create_payment(order_id, amount)
        sdk.execute_payment(payment["id"], "PAYER123")
        status = sdk.get_payment(payment["id"])
        return status["state"] == "approved"
    
    else:
        raise ValueError(f"Unknown payment method: {payment_method}")


# 问题：
# 1. 业务代码与多个 SDK 耦合
# 2. 每新增支付方式都要修改 checkout 函数
# 3. 单位转换逻辑散落各处
# 4. 难以测试和维护
```

### 应用后：使用适配器模式

```python
from abc import ABC, abstractmethod
from decimal import Decimal
from typing import Optional
from functools import lru_cache


# ========== 目标接口 ==========
class PaymentGateway(ABC):
    """统一的支付网关接口"""
    
    @abstractmethod
    def create_order(self, order_id: str, amount: Decimal) -> str:
        """创建订单，返回支付凭证"""
        pass
    
    @abstractmethod
    def pay(self, payment_token: str) -> bool:
        """执行支付"""
        pass
    
    @abstractmethod
    def query_status(self, order_id: str) -> dict:
        """查询订单状态"""
        pass
    
    @abstractmethod
    def refund(self, order_id: str, amount: Optional[Decimal] = None) -> bool:
        """退款"""
        pass


# ========== 被适配的第三方 SDK（保持原样）==========
class AlipaySDK:
    def create_alipay_order(self, order_id, amount_yuan):
        print(f"[Alipay] Creating order {order_id}: ¥{amount_yuan}")
        return f"alipay_token_{order_id}"
    
    def alipay_pay(self, order_id):
        print(f"[Alipay] Processing payment for {order_id}")
        return True
    
    def query_alipay_status(self, order_id):
        return {"order_id": order_id, "status": "TRADE_SUCCESS", "platform": "alipay"}
    
    def alipay_refund(self, order_id, refund_amount):
        print(f"[Alipay] Refunding ¥{refund_amount} for {order_id}")
        return True


class WeChatPaySDK:
    def unified_order(self, out_trade_no, total_fee_fen):
        print(f"[WeChat] Creating order {out_trade_no}: {total_fee_fen} fen")
        return {"prepay_id": f"wx_{out_trade_no}", "total": total_fee_fen}
    
    def wx_pay(self, prepay_id):
        print(f"[WeChat] Processing payment {prepay_id}")
        return {"return_code": "SUCCESS"}
    
    def order_query(self, out_trade_no):
        return {"trade_state": "SUCCESS", "out_trade_no": out_trade_no}
    
    def wx_refund(self, out_trade_no, refund_fee_fen):
        print(f"[WeChat] Refunding {refund_fee_fen} fen for {out_trade_no}")
        return {"return_code": "SUCCESS"}


class PayPalSDK:
    def create_payment(self, invoice_number, usd_amount):
        print(f"[PayPal] Creating payment {invoice_number}: ${usd_amount}")
        return {"id": f"PAY-{invoice_number}", "state": "created"}
    
    def execute_payment(self, payment_id, payer_id):
        print(f"[PayPal] Executing payment {payment_id}")
        return {"id": payment_id, "state": "approved"}
    
    def get_payment(self, payment_id):
        return {"id": payment_id, "state": "approved"}
    
    def refund_payment(self, payment_id, amount):
        print(f"[PayPal] Refunding ${amount} for {payment_id}")
        return {"state": "completed"}


# ========== 适配器（无缓存版本）==========
class AlipayAdapter(PaymentGateway):
    """支付宝适配器"""
    
    def __init__(self):
        self._sdk = AlipaySDK()
        self._tokens = {}  # order_id -> token
    
    def create_order(self, order_id: str, amount: Decimal) -> str:
        token = self._sdk.create_alipay_order(order_id, float(amount))
        self._tokens[order_id] = token
        return token
    
    def pay(self, payment_token: str) -> bool:
        # 从 token 提取 order_id
        order_id = payment_token.replace("alipay_token_", "")
        return self._sdk.alipay_pay(order_id)
    
    def query_status(self, order_id: str) -> dict:
        result = self._sdk.query_alipay_status(order_id)
        # 转换为统一格式
        return {
            "order_id": order_id,
            "status": "SUCCESS" if result["status"] == "TRADE_SUCCESS" else "FAILED",
            "raw": result
        }
    
    def refund(self, order_id: str, amount: Optional[Decimal] = None) -> bool:
        return self._sdk.alipay_refund(order_id, float(amount) if amount else 0)


class WeChatPayAdapter(PaymentGateway):
    """微信支付适配器"""
    
    def __init__(self):
        self._sdk = WeChatPaySDK()
        self._prepay_ids = {}  # order_id -> prepay_id
    
    def create_order(self, order_id: str, amount: Decimal) -> str:
        # 元转分
        amount_fen = int(amount * 100)
        result = self._sdk.unified_order(order_id, amount_fen)
        prepay_id = result["prepay_id"]
        self._prepay_ids[order_id] = prepay_id
        return prepay_id
    
    def pay(self, payment_token: str) -> bool:
        result = self._sdk.wx_pay(payment_token)
        return result.get("return_code") == "SUCCESS"
    
    def query_status(self, order_id: str) -> dict:
        result = self._sdk.order_query(order_id)
        return {
            "order_id": order_id,
            "status": "SUCCESS" if result["trade_state"] == "SUCCESS" else "FAILED",
            "raw": result
        }
    
    def refund(self, order_id: str, amount: Optional[Decimal] = None) -> bool:
        amount_fen = int(amount * 100) if amount else 0
        result = self._sdk.wx_refund(order_id, amount_fen)
        return result.get("return_code") == "SUCCESS"


class PayPalAdapter(PaymentGateway):
    """PayPal 适配器"""
    
    def __init__(self, default_payer_id: str = "DEFAULT_PAYER"):
        self._sdk = PayPalSDK()
        self._payment_ids = {}  # order_id -> payment_id
        self._payer_id = default_payer_id
    
    def create_order(self, order_id: str, amount: Decimal) -> str:
        result = self._sdk.create_payment(order_id, float(amount))
        payment_id = result["id"]
        self._payment_ids[order_id] = payment_id
        return payment_id
    
    def pay(self, payment_token: str) -> bool:
        result = self._sdk.execute_payment(payment_token, self._payer_id)
        return result.get("state") == "approved"
    
    def query_status(self, order_id: str) -> dict:
        payment_id = self._payment_ids.get(order_id, f"PAY-{order_id}")
        result = self._sdk.get_payment(payment_id)
        return {
            "order_id": order_id,
            "status": "SUCCESS" if result["state"] == "approved" else "FAILED",
            "raw": result
        }
    
    def refund(self, order_id: str, amount: Optional[Decimal] = None) -> bool:
        payment_id = self._payment_ids.get(order_id, f"PAY-{order_id}")
        result = self._sdk.refund_payment(payment_id, float(amount) if amount else 0)
        return result.get("state") == "completed"


# ========== 适配器（有缓存版本）==========
class CachedAlipayAdapter(PaymentGateway):
    """带缓存的支付宝适配器"""
    
    def __init__(self, cache_ttl: int = 60):
        self._sdk = AlipaySDK()
        self._tokens = {}
        self._status_cache = {}  # order_id -> (timestamp, status)
        self._cache_ttl = cache_ttl
    
    def create_order(self, order_id: str, amount: Decimal) -> str:
        token = self._sdk.create_alipay_order(order_id, float(amount))
        self._tokens[order_id] = token
        return token
    
    def pay(self, payment_token: str) -> bool:
        order_id = payment_token.replace("alipay_token_", "")
        result = self._sdk.alipay_pay(order_id)
        # 支付后清除缓存
        self._status_cache.pop(order_id, None)
        return result
    
    @lru_cache(maxsize=100)
    def _cached_query(self, order_id: str, cache_key: int) -> dict:
        """带缓存的查询（cache_key 用于控制缓存失效）"""
        return self._sdk.query_alipay_status(order_id)
    
    def query_status(self, order_id: str) -> dict:
        import time
        # 使用时间戳作为缓存 key，实现 TTL
        cache_key = int(time.time()) // self._cache_ttl
        result = self._cached_query(order_id, cache_key)
        return {
            "order_id": order_id,
            "status": "SUCCESS" if result["status"] == "TRADE_SUCCESS" else "FAILED",
            "raw": result,
            "cached": True
        }
    
    def refund(self, order_id: str, amount: Optional[Decimal] = None) -> bool:
        result = self._sdk.alipay_refund(order_id, float(amount) if amount else 0)
        # 退款后清除缓存
        self._cached_query.cache_clear()
        return result
    
    def clear_cache(self):
        """手动清除缓存"""
        self._cached_query.cache_clear()
        self._status_cache.clear()


# ========== 工厂方法获取适配器 ==========
class PaymentGatewayFactory:
    """支付网关工厂"""
    
    _adapters = {
        "alipay": AlipayAdapter,
        "wechat": WeChatPayAdapter,
        "paypal": PayPalAdapter,
        "alipay_cached": CachedAlipayAdapter,
    }
    
    @classmethod
    def create(cls, gateway_type: str, **kwargs) -> PaymentGateway:
        adapter_class = cls._adapters.get(gateway_type)
        if not adapter_class:
            raise ValueError(f"Unknown gateway: {gateway_type}")
        return adapter_class(**kwargs)
    
    @classmethod
    def register(cls, name: str, adapter_class: type):
        cls._adapters[name] = adapter_class


# ========== 客户端代码（简洁统一）==========
class CheckoutService:
    """结账服务 - 只依赖 PaymentGateway 接口"""
    
    def __init__(self, gateway: PaymentGateway):
        self._gateway = gateway
    
    def process_payment(self, order_id: str, amount: Decimal) -> bool:
        """处理支付"""
        print(f"\n{'='*50}")
        print(f"Processing order {order_id}: ¥{amount}")
        print('='*50)
        
        # 1. 创建订单
        token = self._gateway.create_order(order_id, amount)
        print(f"Payment token: {token}")
        
        # 2. 执行支付
        success = self._gateway.pay(token)
        if not success:
            print("Payment failed!")
            return False
        
        # 3. 查询状态
        status = self._gateway.query_status(order_id)
        print(f"Status: {status}")
        
        return status["status"] == "SUCCESS"


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    # 使用不同的支付方式，代码完全一致
    for gateway_type in ["alipay", "wechat", "paypal"]:
        gateway = PaymentGatewayFactory.create(gateway_type)
        service = CheckoutService(gateway)
        
        order_id = f"ORD-{gateway_type.upper()}-001"
        success = service.process_payment(order_id, Decimal("99.99"))
        print(f"Result: {'SUCCESS' if success else 'FAILED'}")
    
    # 使用带缓存的适配器
    print("\n" + "="*50)
    print("Testing Cached Adapter")
    print("="*50)
    
    cached_gateway = PaymentGatewayFactory.create("alipay_cached", cache_ttl=30)
    
    # 多次查询，后续查询使用缓存
    for i in range(3):
        status = cached_gateway.query_status("ORD-001")
        print(f"Query {i+1}: {status}")
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **耦合度** | 业务代码直接依赖多个 SDK | 只依赖统一的 Gateway 接口 |
| **可扩展性** | 新增支付方式需修改业务代码 | 新增适配器即可，业务代码不变 |
| **可测试性** | 难以 mock 各种 SDK | 可以轻松 mock Gateway 接口 |
| **代码复用** | 单位转换、格式转换散落各处 | 转换逻辑封装在适配器中 |
| **维护性** | SDK 升级影响多处代码 | 只需修改对应适配器 |

---

## 适配器 vs 其他模式

```
+----------------+     +----------------+     +----------------+
|    Adapter     |     |    Decorator   |     |     Proxy      |
+----------------+     +----------------+     +----------------+
| 转换接口       |     | 增强功能       |     | 控制访问       |
| 兼容不同系统   |     | 保持接口不变   |     | 保持接口不变   |
| A 接口 -> B 接口|     | 包装后增加行为 |     | 包装后控制调用 |
+----------------+     +----------------+     +----------------+
```

---

## 双向适配器

```python
class TwoWayAdapter:
    """双向适配器：可以在两种接口间互相转换"""
    
    def __init__(self, adaptee_a=None, adaptee_b=None):
        self._adaptee_a = adaptee_a or SystemA()
        self._adaptee_b = adaptee_b or SystemB()
    
    # 作为 B 接口使用（适配 A）
    def interface_b_method(self, data):
        # 转换数据格式
        converted = self._convert_b_to_a(data)
        result = self._adaptee_a.interface_a_method(converted)
        return self._convert_a_to_b(result)
    
    # 作为 A 接口使用（适配 B）
    def interface_a_method(self, data):
        converted = self._convert_a_to_b(data)
        result = self._adaptee_b.interface_b_method(converted)
        return self._convert_b_to_a(result)
    
    def _convert_a_to_b(self, data):
        # A 格式 -> B 格式
        return data
    
    def _convert_b_to_a(self, data):
        # B 格式 -> A 格式
        return data
```

