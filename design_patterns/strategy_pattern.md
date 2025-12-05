# Strategy Pattern (策略模式)

## ASCII Diagram

```
+-------------------+          +-------------------+
|      Context      |          |     Strategy      |<<interface>>
+-------------------+          +-------------------+
| - strategy        |--------->| + execute()       |
+-------------------+          +-------------------+
| + setStrategy()   |                  ^
| + executeStrategy()|                  |
+-------------------+     +------------+------------+
                          |            |            |
                    +-----+----+ +-----+----+ +-----+----+
                    |StrategyA | |StrategyB | |StrategyC |
                    +----------+ +----------+ +----------+
                    | +execute()| | +execute()| | +execute()|
                    +----------+ +----------+ +----------+

Strategy Selection:
+-------------------+     setStrategy(A)     +-------------------+
|      Context      |----------------------->|    Strategy A     |
+-------------------+                        +-------------------+
         |
         | setStrategy(B)
         v
+-------------------+
|    Strategy B     |
+-------------------+

Runtime switching:
                    +-> Strategy A (e.g., CreditCard)
                   /
Context ----------+---> Strategy B (e.g., PayPal)
                   \
                    +-> Strategy C (e.g., Bitcoin)
```

**中文说明：**
- **Strategy（策略接口）**：定义算法的公共接口
- **ConcreteStrategy（具体策略）**：实现具体的算法
- **Context（上下文）**：维护对策略的引用，调用策略的方法
- **关键点**：将算法封装成独立的类，可以互相替换

---

## 核心思想

定义一系列**算法**，把它们一个个**封装**起来，并且使它们可以**互相替换**。策略模式让算法的变化独立于使用它的客户端。实现了"组合优于继承"的设计原则。

---

## 应用场景

1. **多种算法**：系统需要在多种算法中动态选择一种
2. **避免条件判断**：避免大量的 if-else 或 switch-case
3. **算法变化**：算法可能频繁变化或扩展
4. **实际应用**：
   - 支付方式选择
   - 排序算法选择
   - 压缩算法选择
   - 路由策略
   - 折扣计算

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 开闭原则 | 新增策略无需修改上下文 |
| 消除条件判断 | 避免多重条件判断 |
| 算法复用 | 策略可以在不同上下文中复用 |
| 运行时切换 | 可以动态改变对象行为 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 类数量增加 | 每个策略都需要一个类 |
| 客户端需了解策略 | 客户端需要知道不同策略的区别 |
| 通信开销 | 策略和上下文之间可能有通信开销 |

---

## Python 代码示例

### 应用前：条件判断选择算法

```python
# 问题：支付系统，用条件判断选择支付方式

class PaymentProcessor:
    """支付处理器 - 大量条件判断"""
    
    def process_payment(self, amount, method, **kwargs):
        if method == "credit_card":
            card_number = kwargs.get("card_number")
            expiry = kwargs.get("expiry")
            cvv = kwargs.get("cvv")
            # 信用卡支付逻辑
            fee = amount * 0.03  # 3% 手续费
            total = amount + fee
            print(f"Processing credit card payment: ${total:.2f}")
            return {"status": "success", "total": total, "fee": fee}
        
        elif method == "paypal":
            email = kwargs.get("email")
            # PayPal 支付逻辑
            fee = amount * 0.029 + 0.30  # 2.9% + $0.30
            total = amount + fee
            print(f"Processing PayPal payment: ${total:.2f}")
            return {"status": "success", "total": total, "fee": fee}
        
        elif method == "bank_transfer":
            account = kwargs.get("account")
            routing = kwargs.get("routing")
            # 银行转账逻辑
            fee = 5.00  # 固定手续费
            total = amount + fee
            print(f"Processing bank transfer: ${total:.2f}")
            return {"status": "success", "total": total, "fee": fee}
        
        elif method == "crypto":
            wallet = kwargs.get("wallet")
            currency = kwargs.get("currency", "BTC")
            # 加密货币支付逻辑
            fee = amount * 0.01  # 1% 手续费
            total = amount + fee
            print(f"Processing crypto payment: ${total:.2f} in {currency}")
            return {"status": "success", "total": total, "fee": fee}
        
        else:
            raise ValueError(f"Unknown payment method: {method}")


# 问题：
# 1. 大量的 if-else 判断
# 2. 新增支付方式需要修改这个类
# 3. 每个支付方式的逻辑混在一起
# 4. 难以测试单个支付方式
# 5. 违反开闭原则和单一职责原则
```

### 应用后：使用策略模式

```python
from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Dict, Any, Optional, Callable
from decimal import Decimal
from enum import Enum


# ========== 策略接口 ==========
class PaymentStrategy(ABC):
    """支付策略接口"""
    
    @abstractmethod
    def pay(self, amount: Decimal) -> Dict[str, Any]:
        """处理支付"""
        pass
    
    @abstractmethod
    def validate(self) -> bool:
        """验证支付信息"""
        pass
    
    @property
    @abstractmethod
    def name(self) -> str:
        """策略名称"""
        pass
    
    @abstractmethod
    def calculate_fee(self, amount: Decimal) -> Decimal:
        """计算手续费"""
        pass


# ========== 具体策略 ==========
class CreditCardStrategy(PaymentStrategy):
    """信用卡支付策略"""
    
    def __init__(self, card_number: str, expiry: str, cvv: str):
        self.card_number = card_number
        self.expiry = expiry
        self.cvv = cvv
    
    def validate(self) -> bool:
        # 简化的验证逻辑
        if len(self.card_number) != 16:
            return False
        if len(self.cvv) != 3:
            return False
        return True
    
    def calculate_fee(self, amount: Decimal) -> Decimal:
        return amount * Decimal("0.03")  # 3%
    
    def pay(self, amount: Decimal) -> Dict[str, Any]:
        if not self.validate():
            return {"status": "failed", "error": "Invalid card details"}
        
        fee = self.calculate_fee(amount)
        total = amount + fee
        
        # 模拟支付处理
        masked_card = f"****-****-****-{self.card_number[-4:]}"
        print(f"  [CreditCard] Charging {masked_card}: ${total:.2f}")
        
        return {
            "status": "success",
            "method": self.name,
            "amount": float(amount),
            "fee": float(fee),
            "total": float(total),
            "card": masked_card
        }
    
    @property
    def name(self) -> str:
        return "Credit Card"


class PayPalStrategy(PaymentStrategy):
    """PayPal 支付策略"""
    
    def __init__(self, email: str):
        self.email = email
    
    def validate(self) -> bool:
        return "@" in self.email
    
    def calculate_fee(self, amount: Decimal) -> Decimal:
        return amount * Decimal("0.029") + Decimal("0.30")  # 2.9% + $0.30
    
    def pay(self, amount: Decimal) -> Dict[str, Any]:
        if not self.validate():
            return {"status": "failed", "error": "Invalid email"}
        
        fee = self.calculate_fee(amount)
        total = amount + fee
        
        print(f"  [PayPal] Charging {self.email}: ${total:.2f}")
        
        return {
            "status": "success",
            "method": self.name,
            "amount": float(amount),
            "fee": float(fee),
            "total": float(total),
            "email": self.email
        }
    
    @property
    def name(self) -> str:
        return "PayPal"


class BankTransferStrategy(PaymentStrategy):
    """银行转账策略"""
    
    def __init__(self, account: str, routing: str):
        self.account = account
        self.routing = routing
    
    def validate(self) -> bool:
        return len(self.account) > 0 and len(self.routing) == 9
    
    def calculate_fee(self, amount: Decimal) -> Decimal:
        return Decimal("5.00")  # 固定手续费
    
    def pay(self, amount: Decimal) -> Dict[str, Any]:
        if not self.validate():
            return {"status": "failed", "error": "Invalid bank details"}
        
        fee = self.calculate_fee(amount)
        total = amount + fee
        
        masked_account = f"****{self.account[-4:]}"
        print(f"  [BankTransfer] Transferring to {masked_account}: ${total:.2f}")
        
        return {
            "status": "success",
            "method": self.name,
            "amount": float(amount),
            "fee": float(fee),
            "total": float(total),
            "account": masked_account
        }
    
    @property
    def name(self) -> str:
        return "Bank Transfer"


class CryptoStrategy(PaymentStrategy):
    """加密货币支付策略"""
    
    def __init__(self, wallet_address: str, currency: str = "BTC"):
        self.wallet_address = wallet_address
        self.currency = currency
    
    def validate(self) -> bool:
        return len(self.wallet_address) >= 26
    
    def calculate_fee(self, amount: Decimal) -> Decimal:
        return amount * Decimal("0.01")  # 1%
    
    def pay(self, amount: Decimal) -> Dict[str, Any]:
        if not self.validate():
            return {"status": "failed", "error": "Invalid wallet address"}
        
        fee = self.calculate_fee(amount)
        total = amount + fee
        
        short_wallet = f"{self.wallet_address[:8]}...{self.wallet_address[-4:]}"
        print(f"  [Crypto] Sending {self.currency} to {short_wallet}: ${total:.2f}")
        
        return {
            "status": "success",
            "method": self.name,
            "amount": float(amount),
            "fee": float(fee),
            "total": float(total),
            "currency": self.currency,
            "wallet": short_wallet
        }
    
    @property
    def name(self) -> str:
        return f"Crypto ({self.currency})"


# ========== 上下文 ==========
class PaymentProcessor:
    """
    支付处理器 - 上下文
    
    维护对支付策略的引用
    """
    
    def __init__(self, strategy: Optional[PaymentStrategy] = None):
        self._strategy = strategy
    
    def set_strategy(self, strategy: PaymentStrategy) -> None:
        """设置支付策略"""
        self._strategy = strategy
        print(f"  [Processor] Strategy set to: {strategy.name}")
    
    def process_payment(self, amount: Decimal) -> Dict[str, Any]:
        """处理支付"""
        if self._strategy is None:
            raise ValueError("No payment strategy set")
        
        print(f"\n  [Processor] Processing ${amount:.2f} via {self._strategy.name}")
        return self._strategy.pay(amount)
    
    def get_fee_estimate(self, amount: Decimal) -> Decimal:
        """获取手续费估算"""
        if self._strategy is None:
            raise ValueError("No payment strategy set")
        return self._strategy.calculate_fee(amount)


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    print("=" * 60)
    print("Payment System with Strategy Pattern")
    print("=" * 60)
    
    processor = PaymentProcessor()
    amount = Decimal("100.00")
    
    # 信用卡支付
    print("\n--- Credit Card Payment ---")
    credit_card = CreditCardStrategy("1234567890123456", "12/25", "123")
    processor.set_strategy(credit_card)
    result = processor.process_payment(amount)
    print(f"  Result: {result}")
    
    # PayPal 支付
    print("\n--- PayPal Payment ---")
    paypal = PayPalStrategy("user@example.com")
    processor.set_strategy(paypal)
    result = processor.process_payment(amount)
    print(f"  Result: {result}")
    
    # 银行转账
    print("\n--- Bank Transfer ---")
    bank = BankTransferStrategy("123456789012", "123456789")
    processor.set_strategy(bank)
    result = processor.process_payment(amount)
    print(f"  Result: {result}")
    
    # 加密货币
    print("\n--- Crypto Payment ---")
    crypto = CryptoStrategy("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa", "BTC")
    processor.set_strategy(crypto)
    result = processor.process_payment(amount)
    print(f"  Result: {result}")
    
    # 比较手续费
    print("\n--- Fee Comparison ---")
    strategies = [
        CreditCardStrategy("1234567890123456", "12/25", "123"),
        PayPalStrategy("user@example.com"),
        BankTransferStrategy("123456789012", "123456789"),
        CryptoStrategy("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa", "ETH"),
    ]
    
    for strategy in strategies:
        fee = strategy.calculate_fee(amount)
        print(f"  {strategy.name:20} fee: ${fee:.2f}")


# ========== 函数式策略（Python 风格）==========
print("\n" + "=" * 60)
print("Functional Strategy Pattern (Pythonic)")
print("=" * 60)


class DiscountStrategy:
    """折扣策略 - 函数式风格"""
    
    @staticmethod
    def no_discount(price: Decimal) -> Decimal:
        return price
    
    @staticmethod
    def percentage_discount(percent: int):
        def discount(price: Decimal) -> Decimal:
            return price * (1 - Decimal(percent) / 100)
        return discount
    
    @staticmethod
    def fixed_discount(amount: Decimal):
        def discount(price: Decimal) -> Decimal:
            return max(Decimal("0"), price - amount)
        return discount
    
    @staticmethod
    def buy_one_get_one():
        def discount(price: Decimal) -> Decimal:
            return price / 2
        return discount


class ShoppingCart:
    """购物车 - 使用函数作为策略"""
    
    def __init__(self):
        self._items: list = []
        self._discount_strategy: Callable = DiscountStrategy.no_discount
    
    def add_item(self, name: str, price: Decimal):
        self._items.append({"name": name, "price": price})
    
    def set_discount(self, strategy: Callable):
        self._discount_strategy = strategy
    
    def calculate_total(self) -> Decimal:
        subtotal = sum(item["price"] for item in self._items)
        return self._discount_strategy(subtotal)
    
    def checkout(self):
        subtotal = sum(item["price"] for item in self._items)
        total = self._discount_strategy(subtotal)
        discount = subtotal - total
        
        print(f"  Subtotal: ${subtotal:.2f}")
        print(f"  Discount: -${discount:.2f}")
        print(f"  Total:    ${total:.2f}")


# 使用函数式策略
cart = ShoppingCart()
cart.add_item("Book", Decimal("30.00"))
cart.add_item("Pen", Decimal("5.00"))
cart.add_item("Notebook", Decimal("15.00"))

print("\n--- No discount ---")
cart.set_discount(DiscountStrategy.no_discount)
cart.checkout()

print("\n--- 20% off ---")
cart.set_discount(DiscountStrategy.percentage_discount(20))
cart.checkout()

print("\n--- $10 off ---")
cart.set_discount(DiscountStrategy.fixed_discount(Decimal("10")))
cart.checkout()

print("\n--- BOGO ---")
cart.set_discount(DiscountStrategy.buy_one_get_one())
cart.checkout()


# ========== 排序策略示例 ==========
print("\n" + "=" * 60)
print("Sorting Strategy Example")
print("=" * 60)


class SortStrategy(ABC):
    @abstractmethod
    def sort(self, data: list) -> list:
        pass


class BubbleSort(SortStrategy):
    def sort(self, data: list) -> list:
        arr = data.copy()
        n = len(arr)
        for i in range(n):
            for j in range(0, n-i-1):
                if arr[j] > arr[j+1]:
                    arr[j], arr[j+1] = arr[j+1], arr[j]
        return arr


class QuickSort(SortStrategy):
    def sort(self, data: list) -> list:
        if len(data) <= 1:
            return data
        pivot = data[len(data) // 2]
        left = [x for x in data if x < pivot]
        middle = [x for x in data if x == pivot]
        right = [x for x in data if x > pivot]
        return self.sort(left) + middle + self.sort(right)


class MergeSort(SortStrategy):
    def sort(self, data: list) -> list:
        if len(data) <= 1:
            return data
        mid = len(data) // 2
        left = self.sort(data[:mid])
        right = self.sort(data[mid:])
        return self._merge(left, right)
    
    def _merge(self, left, right):
        result = []
        i = j = 0
        while i < len(left) and j < len(right):
            if left[i] <= right[j]:
                result.append(left[i])
                i += 1
            else:
                result.append(right[j])
                j += 1
        result.extend(left[i:])
        result.extend(right[j:])
        return result


class Sorter:
    def __init__(self, strategy: SortStrategy):
        self._strategy = strategy
    
    def set_strategy(self, strategy: SortStrategy):
        self._strategy = strategy
    
    def sort(self, data: list) -> list:
        return self._strategy.sort(data)


# 使用排序策略
data = [64, 34, 25, 12, 22, 11, 90]
sorter = Sorter(BubbleSort())

print(f"\nOriginal: {data}")
print(f"BubbleSort: {sorter.sort(data)}")

sorter.set_strategy(QuickSort())
print(f"QuickSort: {sorter.sort(data)}")

sorter.set_strategy(MergeSort())
print(f"MergeSort: {sorter.sort(data)}")
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **条件判断** | 大量 if-else | 无条件判断 |
| **扩展性** | 修改类添加新算法 | 添加新策略类 |
| **测试** | 难以单独测试算法 | 策略可独立测试 |
| **复用** | 算法难以复用 | 策略可在不同上下文复用 |
| **维护** | 所有算法在一起 | 每个策略独立维护 |

---

## 策略选择

```python
# 策略工厂
class PaymentStrategyFactory:
    _strategies = {
        "credit_card": lambda **k: CreditCardStrategy(k["card"], k["expiry"], k["cvv"]),
        "paypal": lambda **k: PayPalStrategy(k["email"]),
        "bank": lambda **k: BankTransferStrategy(k["account"], k["routing"]),
    }
    
    @classmethod
    def create(cls, method: str, **kwargs) -> PaymentStrategy:
        if method not in cls._strategies:
            raise ValueError(f"Unknown method: {method}")
        return cls._strategies[method](**kwargs)

# 使用
strategy = PaymentStrategyFactory.create(
    "credit_card",
    card="1234567890123456",
    expiry="12/25",
    cvv="123"
)
```

---

## 与其他模式的关系

| 模式 | 关系 |
|------|------|
| **State** | 状态模式是策略模式的扩展，状态可以改变自己 |
| **Factory** | 工厂可以创建策略对象 |
| **Decorator** | 装饰器增强功能，策略替换算法 |
| **Template Method** | 模板方法在父类定义骨架，策略完全替换算法 |

