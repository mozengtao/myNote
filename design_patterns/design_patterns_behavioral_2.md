# Design Patterns - Behavioral Patterns Part 2 (行为型模式 第二部分)

A comprehensive guide to behavioral design patterns with English explanations,
Chinese details, ASCII diagrams, and Python code examples.

---

## Table of Contents

7. [State Pattern (状态模式)](#7-state-pattern-状态模式)
8. [Strategy Pattern (策略模式)](#8-strategy-pattern-策略模式)
9. [Template Method Pattern (模板方法模式)](#9-template-method-pattern-模板方法模式)
10. [Visitor Pattern (访问者模式)](#10-visitor-pattern-访问者模式)
11. [Interpreter Pattern (解释器模式)](#11-interpreter-pattern-解释器模式)

---

## 7. State Pattern (状态模式)

**Allow an object to alter its behavior when its internal state changes, appearing to change its class.**

### 中文详解

状态模式是一种行为型设计模式，它允许一个对象在其内部状态改变时改变它的行为，对象看起来似乎修改了它的类。

**适用场景：**
- 当对象的行为取决于它的状态，并且它必须在运行时根据状态改变它的行为时
- 当代码中包含大量与对象状态有关的条件语句时
- 例如：订单状态、文档审批流程、游戏角色状态、TCP 连接状态

**与策略模式的区别：**
- 状态模式：状态之间知道彼此存在，可以触发状态转换
- 策略模式：策略之间相互独立，不知道彼此存在

**优点：**
- 单一职责原则：将与特定状态相关的代码放在单独的类中
- 开闭原则：无需修改已有状态类和上下文就能引入新状态
- 消除庞大的条件分支语句

**缺点：**
- 如果状态机只有很少的状态或者很少改变，应用状态模式可能会显得小题大做

### Structure Diagram

```
+-------------------+             +-------------------+
|     Context       |             |      State        |
+-------------------+             |   <<interface>>   |
| - state: State    |------------>+-------------------+
+-------------------+             | + handle(context) |
| + set_state(state)|             +-------------------+
| + request()       |                      ^
+-------------------+                      |
                              +------------+------------+
                              |            |            |
                     +------------+  +------------+  +------------+
                     |  StateA    |  |  StateB    |  |  StateC    |
                     +------------+  +------------+  +------------+
                     | + handle() |  | + handle() |  | + handle() |
                     +------------+  +------------+  +------------+
                           |               ^              ^
                           |  transitions  |              |
                           +---------------+--------------+

State Transitions:
  StateA --[event1]--> StateB --[event2]--> StateC
    ^                                          |
    +----------------[event3]------------------+
```

**图解说明：**
- `Context` 上下文，维护当前状态的引用，将状态相关的工作委托给当前状态对象
- `State` 状态接口，定义与上下文的特定状态相关的行为
- `ConcreteState` 具体状态，实现与上下文的一个状态相关的行为
- 状态对象可以触发上下文的状态转换

### Python Code Example

```python
"""
State Pattern Implementation in Python
状态模式的 Python 实现

Example: Order processing workflow
示例：订单处理工作流
"""

from abc import ABC, abstractmethod
from datetime import datetime
from typing import Optional


# State interface
class OrderState(ABC):
    """
    State interface for order states.
    订单状态的状态接口。
    """
    @property
    @abstractmethod
    def name(self) -> str:
        pass

    @abstractmethod
    def next(self, order: 'Order') -> None:
        """Move to next state."""
        pass

    @abstractmethod
    def prev(self, order: 'Order') -> None:
        """Move to previous state (if possible)."""
        pass

    @abstractmethod
    def cancel(self, order: 'Order') -> None:
        """Cancel the order (if possible)."""
        pass

    def print_status(self, order: 'Order') -> str:
        """Print current status."""
        return f"Order #{order.order_id}: {self.name}"


# Concrete States
class PendingState(OrderState):
    """Order is pending, waiting for payment."""
    @property
    def name(self) -> str:
        return "Pending Payment"

    def next(self, order: 'Order') -> None:
        print(f"  💳 Payment received for Order #{order.order_id}")
        order.state = PaidState()

    def prev(self, order: 'Order') -> None:
        print("  ⚠️ Cannot go back - this is the initial state")

    def cancel(self, order: 'Order') -> None:
        print(f"  ❌ Order #{order.order_id} cancelled (was pending)")
        order.state = CancelledState()


class PaidState(OrderState):
    """Order is paid, preparing for shipment."""
    @property
    def name(self) -> str:
        return "Paid - Processing"

    def next(self, order: 'Order') -> None:
        print(f"  📦 Order #{order.order_id} shipped!")
        order.state = ShippedState()

    def prev(self, order: 'Order') -> None:
        print(f"  💸 Refund initiated for Order #{order.order_id}")
        order.state = PendingState()

    def cancel(self, order: 'Order') -> None:
        print(f"  ❌ Order #{order.order_id} cancelled, refund processing")
        order.state = CancelledState()


class ShippedState(OrderState):
    """Order is shipped, in transit."""
    @property
    def name(self) -> str:
        return "Shipped - In Transit"

    def next(self, order: 'Order') -> None:
        print(f"  ✅ Order #{order.order_id} delivered!")
        order.state = DeliveredState()

    def prev(self, order: 'Order') -> None:
        print("  ⚠️ Cannot unship - package already in transit")

    def cancel(self, order: 'Order') -> None:
        print("  ⚠️ Cannot cancel - package already shipped")


class DeliveredState(OrderState):
    """Order has been delivered."""
    @property
    def name(self) -> str:
        return "Delivered"

    def next(self, order: 'Order') -> None:
        print("  ⚠️ Order already delivered - no next state")

    def prev(self, order: 'Order') -> None:
        print(f"  📤 Return requested for Order #{order.order_id}")
        order.state = ReturnState()

    def cancel(self, order: 'Order') -> None:
        print("  ⚠️ Cannot cancel - already delivered. Request a return instead.")


class ReturnState(OrderState):
    """Order is being returned."""
    @property
    def name(self) -> str:
        return "Return in Progress"

    def next(self, order: 'Order') -> None:
        print(f"  💰 Refund completed for Order #{order.order_id}")
        order.state = RefundedState()

    def prev(self, order: 'Order') -> None:
        print(f"  🔙 Return cancelled, Order #{order.order_id} restored")
        order.state = DeliveredState()

    def cancel(self, order: 'Order') -> None:
        print("  ⚠️ Return already in progress")


class RefundedState(OrderState):
    """Order has been refunded."""
    @property
    def name(self) -> str:
        return "Refunded"

    def next(self, order: 'Order') -> None:
        print("  ⚠️ Order is complete (refunded)")

    def prev(self, order: 'Order') -> None:
        print("  ⚠️ Cannot undo refund")

    def cancel(self, order: 'Order') -> None:
        print("  ⚠️ Order already refunded")


class CancelledState(OrderState):
    """Order has been cancelled."""
    @property
    def name(self) -> str:
        return "Cancelled"

    def next(self, order: 'Order') -> None:
        print("  ⚠️ Cannot proceed - order is cancelled")

    def prev(self, order: 'Order') -> None:
        print("  ⚠️ Cannot restore - order is cancelled")

    def cancel(self, order: 'Order') -> None:
        print("  ⚠️ Order already cancelled")


# Context
class Order:
    """
    Context: Order that changes behavior based on state.
    上下文：根据状态改变行为的订单。
    """
    _order_counter = 0

    def __init__(self, items: list, total: float):
        Order._order_counter += 1
        self._order_id = Order._order_counter
        self._items = items
        self._total = total
        self._state: OrderState = PendingState()
        self._created_at = datetime.now()

    @property
    def order_id(self) -> int:
        return self._order_id

    @property
    def state(self) -> OrderState:
        return self._state

    @state.setter
    def state(self, state: OrderState) -> None:
        print(f"  [State Change] {self._state.name} → {state.name}")
        self._state = state

    def proceed(self) -> None:
        """Move order to next state."""
        print(f"\n→ Proceeding Order #{self._order_id}:")
        self._state.next(self)

    def go_back(self) -> None:
        """Move order to previous state (if possible)."""
        print(f"\n← Going back Order #{self._order_id}:")
        self._state.prev(self)

    def cancel(self) -> None:
        """Cancel the order."""
        print(f"\n✗ Cancelling Order #{self._order_id}:")
        self._state.cancel(self)

    def status(self) -> str:
        """Get current order status."""
        return self._state.print_status(self)


# Client code demonstration
if __name__ == "__main__":
    print("=== State Pattern Demo ===\n")

    # Scenario 1: Normal order flow
    print("Scenario 1: Normal order fulfillment")
    print("=" * 50)
    order1 = Order(["Laptop", "Mouse"], 1299.99)
    print(f"Created: {order1.status()}")

    order1.proceed()  # Pending -> Paid
    print(f"Status: {order1.status()}")

    order1.proceed()  # Paid -> Shipped
    print(f"Status: {order1.status()}")

    order1.proceed()  # Shipped -> Delivered
    print(f"Status: {order1.status()}")

    # Scenario 2: Order cancellation
    print("\n\nScenario 2: Order cancellation")
    print("=" * 50)
    order2 = Order(["Headphones"], 199.99)
    print(f"Created: {order2.status()}")

    order2.proceed()  # Pending -> Paid
    order2.cancel()   # Paid -> Cancelled
    print(f"Final: {order2.status()}")

    # Scenario 3: Return flow
    print("\n\nScenario 3: Return and refund")
    print("=" * 50)
    order3 = Order(["Keyboard"], 149.99)

    order3.proceed()  # Pending -> Paid
    order3.proceed()  # Paid -> Shipped
    order3.proceed()  # Shipped -> Delivered
    order3.go_back()  # Delivered -> Return
    order3.proceed()  # Return -> Refunded
    print(f"Final: {order3.status()}")

    # Scenario 4: Invalid transitions
    print("\n\nScenario 4: Invalid state transitions")
    print("=" * 50)
    order4 = Order(["Monitor"], 399.99)
    order4.go_back()  # Can't go back from initial state
    order4.proceed()  # Pending -> Paid
    order4.proceed()  # Paid -> Shipped
    order4.cancel()   # Can't cancel shipped order
```

---

## 8. Strategy Pattern (策略模式)

**Define a family of algorithms, encapsulate each one, and make them interchangeable, letting the algorithm vary independently from clients that use it.**

### 中文详解

策略模式是一种行为型设计模式，它定义一系列算法，将每一个算法封装起来，并使它们可以相互替换。策略模式让算法独立于使用它的客户而变化。

**适用场景：**
- 当需要使用对象中各种不同的算法变体，并希望能在运行时切换算法时
- 当有许多仅在执行某些行为时略有不同的相似类时
- 当类中使用了复杂条件运算符在同一算法的不同变体中切换时
- 例如：排序算法、支付方式、压缩算法、路由策略

**优点：**
- 可以在运行时切换对象内的算法
- 可以将算法的实现和使用算法的代码隔离开来
- 可以使用组合来代替继承
- 开闭原则：无需修改上下文即可引入新的策略

**缺点：**
- 如果算法极少改变，使用策略模式可能会使程序过于复杂
- 客户端必须知晓策略间的不同

### Structure Diagram

```
+-------------------+            +-------------------+
|     Context       |            |     Strategy      |
+-------------------+            |   <<interface>>   |
| - strategy        |----------->+-------------------+
+-------------------+            | + execute(data)   |
| + set_strategy()  |            +-------------------+
| + do_something()  |                     ^
+-------------------+                     |
                              +-----------+-----------+
                              |           |           |
                     +------------+ +------------+ +------------+
                     | StrategyA  | | StrategyB  | | StrategyC  |
                     +------------+ +------------+ +------------+
                     | + execute()| | + execute()| | + execute()|
                     +------------+ +------------+ +------------+

Runtime Strategy Selection:
  context.set_strategy(StrategyA)  // Use algorithm A
  context.set_strategy(StrategyB)  // Switch to algorithm B
```

**图解说明：**
- `Strategy` 策略接口，定义所有支持的算法的公共接口
- `ConcreteStrategy` 具体策略，实现具体的算法
- `Context` 上下文，维护策略引用，将客户请求委托给策略
- 可以在运行时动态切换策略

### Python Code Example

```python
"""
Strategy Pattern Implementation in Python
策略模式的 Python 实现

Example: Payment processing with different payment methods
示例：使用不同支付方式的支付处理
"""

from abc import ABC, abstractmethod
from typing import Dict, Any
from dataclasses import dataclass
from datetime import datetime


# Strategy interface
class PaymentStrategy(ABC):
    """
    Strategy interface for payment processing.
    支付处理的策略接口。
    """
    @abstractmethod
    def pay(self, amount: float) -> Dict[str, Any]:
        """Process payment and return result."""
        pass

    @abstractmethod
    def validate(self) -> bool:
        """Validate payment method details."""
        pass

    @property
    @abstractmethod
    def name(self) -> str:
        """Get payment method name."""
        pass


# Concrete Strategies
class CreditCardPayment(PaymentStrategy):
    """Strategy: Credit card payment."""
    def __init__(self, card_number: str, expiry: str, cvv: str, name: str):
        self._card_number = card_number
        self._expiry = expiry
        self._cvv = cvv
        self._name = name

    @property
    def name(self) -> str:
        return "Credit Card"

    def validate(self) -> bool:
        # Simplified validation
        if len(self._card_number.replace(" ", "")) != 16:
            print("  ❌ Invalid card number")
            return False
        if len(self._cvv) != 3:
            print("  ❌ Invalid CVV")
            return False
        return True

    def pay(self, amount: float) -> Dict[str, Any]:
        if not self.validate():
            return {"success": False, "error": "Validation failed"}

        # Simulate payment processing
        masked_card = f"**** **** **** {self._card_number[-4:]}"
        return {
            "success": True,
            "method": self.name,
            "amount": amount,
            "card": masked_card,
            "transaction_id": f"CC-{datetime.now().strftime('%Y%m%d%H%M%S')}",
            "message": f"Charged ${amount:.2f} to {masked_card}"
        }


class PayPalPayment(PaymentStrategy):
    """Strategy: PayPal payment."""
    def __init__(self, email: str):
        self._email = email

    @property
    def name(self) -> str:
        return "PayPal"

    def validate(self) -> bool:
        if "@" not in self._email:
            print("  ❌ Invalid email address")
            return False
        return True

    def pay(self, amount: float) -> Dict[str, Any]:
        if not self.validate():
            return {"success": False, "error": "Validation failed"}

        return {
            "success": True,
            "method": self.name,
            "amount": amount,
            "email": self._email,
            "transaction_id": f"PP-{datetime.now().strftime('%Y%m%d%H%M%S')}",
            "message": f"Charged ${amount:.2f} via PayPal ({self._email})"
        }


class CryptoPayment(PaymentStrategy):
    """Strategy: Cryptocurrency payment."""
    def __init__(self, wallet_address: str, currency: str = "BTC"):
        self._wallet = wallet_address
        self._currency = currency
        self._exchange_rates = {"BTC": 40000, "ETH": 2500, "USDT": 1}

    @property
    def name(self) -> str:
        return f"Crypto ({self._currency})"

    def validate(self) -> bool:
        if len(self._wallet) < 20:
            print("  ❌ Invalid wallet address")
            return False
        if self._currency not in self._exchange_rates:
            print(f"  ❌ Unsupported currency: {self._currency}")
            return False
        return True

    def pay(self, amount: float) -> Dict[str, Any]:
        if not self.validate():
            return {"success": False, "error": "Validation failed"}

        crypto_amount = amount / self._exchange_rates[self._currency]
        return {
            "success": True,
            "method": self.name,
            "amount_usd": amount,
            "amount_crypto": f"{crypto_amount:.8f} {self._currency}",
            "wallet": f"{self._wallet[:8]}...{self._wallet[-4:]}",
            "transaction_id": f"CRYPTO-{datetime.now().strftime('%Y%m%d%H%M%S')}",
            "message": f"Sent {crypto_amount:.8f} {self._currency} (${amount:.2f})"
        }


class BankTransferPayment(PaymentStrategy):
    """Strategy: Bank transfer payment."""
    def __init__(self, account_number: str, routing_number: str, account_name: str):
        self._account = account_number
        self._routing = routing_number
        self._name = account_name

    @property
    def name(self) -> str:
        return "Bank Transfer"

    def validate(self) -> bool:
        if len(self._account) < 8:
            print("  ❌ Invalid account number")
            return False
        if len(self._routing) != 9:
            print("  ❌ Invalid routing number")
            return False
        return True

    def pay(self, amount: float) -> Dict[str, Any]:
        if not self.validate():
            return {"success": False, "error": "Validation failed"}

        return {
            "success": True,
            "method": self.name,
            "amount": amount,
            "account": f"****{self._account[-4:]}",
            "transaction_id": f"ACH-{datetime.now().strftime('%Y%m%d%H%M%S')}",
            "message": f"Transfer of ${amount:.2f} initiated (2-3 business days)",
            "pending": True
        }


# Context
@dataclass
class CartItem:
    name: str
    price: float
    quantity: int


class ShoppingCart:
    """
    Context: Shopping cart that uses payment strategies.
    上下文：使用支付策略的购物车。
    """
    def __init__(self):
        self._items: list[CartItem] = []
        self._payment_strategy: PaymentStrategy = None

    def add_item(self, name: str, price: float, quantity: int = 1) -> None:
        self._items.append(CartItem(name, price, quantity))

    def get_total(self) -> float:
        return sum(item.price * item.quantity for item in self._items)

    def set_payment_method(self, strategy: PaymentStrategy) -> None:
        """Set the payment strategy."""
        self._payment_strategy = strategy
        print(f"  Payment method set to: {strategy.name}")

    def checkout(self) -> Dict[str, Any]:
        """Process checkout with the selected payment method."""
        if not self._payment_strategy:
            return {"success": False, "error": "No payment method selected"}

        if not self._items:
            return {"success": False, "error": "Cart is empty"}

        total = self.get_total()
        print(f"\n  Processing ${total:.2f} via {self._payment_strategy.name}...")

        result = self._payment_strategy.pay(total)

        if result["success"]:
            self._items.clear()  # Clear cart on successful payment

        return result

    def show_cart(self) -> None:
        """Display cart contents."""
        print("\n  🛒 Shopping Cart:")
        print("  " + "-" * 40)
        for item in self._items:
            subtotal = item.price * item.quantity
            print(f"  {item.name} x{item.quantity}: ${subtotal:.2f}")
        print("  " + "-" * 40)
        print(f"  Total: ${self.get_total():.2f}")


# Client code demonstration
if __name__ == "__main__":
    print("=== Strategy Pattern Demo ===\n")

    # Create shopping cart
    cart = ShoppingCart()
    cart.add_item("Laptop", 999.99)
    cart.add_item("Mouse", 29.99, 2)
    cart.add_item("USB Cable", 9.99, 3)
    cart.show_cart()

    # Strategy 1: Credit Card
    print("\n" + "=" * 50)
    print("Payment Method 1: Credit Card")
    credit_card = CreditCardPayment(
        card_number="4532 1234 5678 9012",
        expiry="12/25",
        cvv="123",
        name="John Doe"
    )
    cart.set_payment_method(credit_card)
    result = cart.checkout()
    print(f"  Result: {result['message']}")

    # Reset cart for next demo
    cart.add_item("Keyboard", 79.99)
    cart.add_item("Monitor", 299.99)
    cart.show_cart()

    # Strategy 2: PayPal
    print("\n" + "=" * 50)
    print("Payment Method 2: PayPal")
    paypal = PayPalPayment(email="john@example.com")
    cart.set_payment_method(paypal)
    result = cart.checkout()
    print(f"  Result: {result['message']}")

    # Reset cart
    cart.add_item("Webcam", 89.99)
    cart.show_cart()

    # Strategy 3: Cryptocurrency
    print("\n" + "=" * 50)
    print("Payment Method 3: Bitcoin")
    crypto = CryptoPayment(
        wallet_address="1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa",
        currency="BTC"
    )
    cart.set_payment_method(crypto)
    result = cart.checkout()
    print(f"  Result: {result['message']}")

    # Reset cart
    cart.add_item("Headphones", 149.99)
    cart.show_cart()

    # Strategy 4: Bank Transfer
    print("\n" + "=" * 50)
    print("Payment Method 4: Bank Transfer")
    bank = BankTransferPayment(
        account_number="123456789012",
        routing_number="021000021",
        account_name="John Doe"
    )
    cart.set_payment_method(bank)
    result = cart.checkout()
    print(f"  Result: {result['message']}")
```

---

## 9. Template Method Pattern (模板方法模式)

**Define the skeleton of an algorithm in an operation, deferring some steps to subclasses without changing the algorithm's structure.**

### 中文详解

模板方法模式是一种行为型设计模式，它在基类中定义一个算法的骨架，而将一些步骤的实现延迟到子类中。模板方法使得子类可以不改变一个算法的结构即可重定义该算法的某些特定步骤。

**适用场景：**
- 当只希望客户端扩展某个特定算法步骤，而不是整个算法或其结构时
- 当多个类的算法除一些细微不同之外几乎完全一样时
- 例如：数据处理流水线、文档解析器、测试框架、构建过程

**相关概念：**
- 钩子方法（Hook）：子类可以选择性重写的方法，有默认实现
- 抽象方法：子类必须重写的方法

**优点：**
- 可以让客户端重写算法的特定部分，使算法变化对其他部分的影响减小
- 可以将重复代码抽取到基类中

**缺点：**
- 部分客户端可能会受到算法骨架的限制
- 通过子类抑制默认步骤实现可能会违反里氏替换原则
- 模板方法中的步骤越多，维护难度越大

### Structure Diagram

```
+----------------------------------+
|        AbstractClass             |
+----------------------------------+
| + template_method()              |  // Final - defines skeleton
|   {                              |
|     step1()                      |
|     step2()                      |
|     if (hook1()) step3()         |
|     step4()                      |
|   }                              |
| # step1()         // abstract    |
| # step2()         // abstract    |
| # step3()         // concrete    |
| # step4()         // concrete    |
| # hook1(): bool   // hook        |
+----------------------------------+
              ^
              |
    +---------+---------+
    |                   |
+------------+    +------------+
| ConcreteA  |    | ConcreteB  |
+------------+    +------------+
| # step1()  |    | # step1()  |
| # step2()  |    | # step2()  |
| # hook1()  |    | # step3()  |
+------------+    +------------+
```

**图解说明：**
- `AbstractClass` 定义模板方法和算法步骤
- `template_method()` 定义算法骨架，调用各步骤
- 抽象步骤（abstract）：子类必须实现
- 具体步骤（concrete）：有默认实现
- 钩子（hook）：子类可选择性重写

### Python Code Example

```python
"""
Template Method Pattern Implementation in Python
模板方法模式的 Python 实现

Example: Data mining pipeline for different file formats
示例：不同文件格式的数据挖掘流水线
"""

from abc import ABC, abstractmethod
from typing import List, Dict, Any
import json


class DataMiner(ABC):
    """
    Abstract class with template method for data mining.
    数据挖掘的抽象类，包含模板方法。
    """
    def mine(self, path: str) -> Dict[str, Any]:
        """
        Template method: defines the algorithm skeleton.
        模板方法：定义算法骨架。
        """
        print(f"\n{'='*50}")
        print(f"Starting data mining for: {path}")
        print('='*50)

        # Step 1: Open/Read the file (abstract)
        raw_data = self.extract_data(path)
        print(f"  ✓ Extracted {len(raw_data)} raw records")

        # Step 2: Parse the data (abstract)
        parsed_data = self.parse_data(raw_data)
        print(f"  ✓ Parsed {len(parsed_data)} records")

        # Step 3: Clean the data (hook - optional override)
        if self.should_clean_data():
            cleaned_data = self.clean_data(parsed_data)
            print(f"  ✓ Cleaned data: {len(cleaned_data)} records remaining")
        else:
            cleaned_data = parsed_data
            print("  ○ Skipping data cleaning")

        # Step 4: Analyze the data (concrete)
        analysis = self.analyze_data(cleaned_data)
        print(f"  ✓ Analysis complete")

        # Step 5: Generate report (hook - optional override)
        report = self.generate_report(analysis)
        print(f"  ✓ Report generated")

        # Step 6: Send notification (hook - optional)
        if self.should_send_notification():
            self.send_notification(report)
            print("  ✓ Notification sent")

        return report

    # Abstract methods - must be implemented
    @abstractmethod
    def extract_data(self, path: str) -> str:
        """Extract raw data from file."""
        pass

    @abstractmethod
    def parse_data(self, raw_data: str) -> List[Dict]:
        """Parse raw data into structured format."""
        pass

    # Concrete methods - default implementation
    def clean_data(self, data: List[Dict]) -> List[Dict]:
        """Clean and filter data (default: remove entries with None values)."""
        return [
            record for record in data
            if all(v is not None for v in record.values())
        ]

    def analyze_data(self, data: List[Dict]) -> Dict[str, Any]:
        """Analyze the data (default: count and basic stats)."""
        if not data:
            return {"count": 0, "fields": []}

        fields = list(data[0].keys()) if data else []
        numeric_fields = {}

        for field in fields:
            values = [r.get(field) for r in data if isinstance(r.get(field), (int, float))]
            if values:
                numeric_fields[field] = {
                    "min": min(values),
                    "max": max(values),
                    "avg": sum(values) / len(values)
                }

        return {
            "count": len(data),
            "fields": fields,
            "numeric_analysis": numeric_fields
        }

    def generate_report(self, analysis: Dict) -> Dict[str, Any]:
        """Generate report (default: return analysis as-is)."""
        return {
            "status": "complete",
            "summary": analysis,
            "format": "standard"
        }

    # Hook methods - optional override
    def should_clean_data(self) -> bool:
        """Hook: whether to clean data (default: True)."""
        return True

    def should_send_notification(self) -> bool:
        """Hook: whether to send notification (default: False)."""
        return False

    def send_notification(self, report: Dict) -> None:
        """Hook: send notification (default: print message)."""
        print(f"    [Notification] Report ready: {report.get('status')}")


class CSVMiner(DataMiner):
    """Concrete class: CSV file data miner."""

    def extract_data(self, path: str) -> str:
        # Simulate reading CSV file
        return """name,age,salary,department
John,30,50000,Engineering
Jane,25,45000,Marketing
Bob,35,60000,Engineering
Alice,28,,Sales
Charlie,40,75000,Management"""

    def parse_data(self, raw_data: str) -> List[Dict]:
        lines = raw_data.strip().split('\n')
        headers = lines[0].split(',')
        result = []

        for line in lines[1:]:
            values = line.split(',')
            record = {}
            for i, header in enumerate(headers):
                value = values[i] if i < len(values) else None
                # Try to convert to number
                if value and value.isdigit():
                    value = int(value)
                elif value == '':
                    value = None
                record[header] = value
            result.append(record)

        return result


class JSONMiner(DataMiner):
    """Concrete class: JSON file data miner."""

    def extract_data(self, path: str) -> str:
        # Simulate reading JSON file
        return json.dumps([
            {"name": "Product A", "price": 29.99, "stock": 100, "category": "Electronics"},
            {"name": "Product B", "price": 49.99, "stock": 50, "category": "Electronics"},
            {"name": "Product C", "price": 19.99, "stock": None, "category": "Books"},
            {"name": "Product D", "price": 99.99, "stock": 25, "category": "Electronics"},
        ])

    def parse_data(self, raw_data: str) -> List[Dict]:
        return json.loads(raw_data)

    # Override hook - JSON data is usually clean
    def should_clean_data(self) -> bool:
        return True  # Still clean to remove None stock values

    # Override to send notification for JSON reports
    def should_send_notification(self) -> bool:
        return True


class XMLMiner(DataMiner):
    """Concrete class: XML file data miner (simplified)."""

    def extract_data(self, path: str) -> str:
        # Simulate reading XML file
        return """<records>
            <record><id>1</id><value>100</value><status>active</status></record>
            <record><id>2</id><value>200</value><status>inactive</status></record>
            <record><id>3</id><value>150</value><status>active</status></record>
        </records>"""

    def parse_data(self, raw_data: str) -> List[Dict]:
        # Simplified XML parsing (in real code, use xml.etree)
        import re
        records = []
        record_pattern = r'<record>(.*?)</record>'
        field_pattern = r'<(\w+)>(.*?)</\1>'

        for match in re.finditer(record_pattern, raw_data, re.DOTALL):
            record_xml = match.group(1)
            record = {}
            for field_match in re.finditer(field_pattern, record_xml):
                key = field_match.group(1)
                value = field_match.group(2)
                if value.isdigit():
                    value = int(value)
                record[key] = value
            records.append(record)

        return records

    # Override: Don't clean XML data
    def should_clean_data(self) -> bool:
        return False

    # Override report generation
    def generate_report(self, analysis: Dict) -> Dict[str, Any]:
        report = super().generate_report(analysis)
        report["format"] = "xml_enhanced"
        report["xml_specific"] = "Additional XML metadata"
        return report


# Client code demonstration
if __name__ == "__main__":
    print("=== Template Method Pattern Demo ===")

    # Process CSV data
    csv_miner = CSVMiner()
    csv_report = csv_miner.mine("employees.csv")
    print(f"\nCSV Report Summary: {csv_report['summary']['count']} records")

    # Process JSON data
    json_miner = JSONMiner()
    json_report = json_miner.mine("products.json")
    print(f"\nJSON Report Summary: {json_report['summary']['count']} records")

    # Process XML data
    xml_miner = XMLMiner()
    xml_report = xml_miner.mine("data.xml")
    print(f"\nXML Report Summary: {xml_report['summary']['count']} records")
    print(f"XML Format: {xml_report['format']}")
```

---

## 10. Visitor Pattern (访问者模式)

**Represent an operation to be performed on elements of an object structure, allowing new operations to be defined without changing the classes of the elements.**

### 中文详解

访问者模式是一种行为型设计模式，它允许你在不改变各元素的类的前提下定义作用于这些元素的新操作。

**适用场景：**
- 当需要对一个复杂对象结构（如对象树）中的所有元素执行某些操作时
- 当需要为不同类型的元素提供多种不同的操作时
- 当算法逻辑需要与元素类分离时
- 例如：编译器的语法树处理、文档导出为不同格式

**优点：**
- 开闭原则：可以引入新的访问者操作而无需修改现有代码
- 单一职责原则：将相关操作集中到一个访问者中
- 访问者可以在遍历时累积信息

**缺点：**
- 每增加新的元素类都需要更新所有访问者
- 访问者可能难以访问元素的私有成员

### Structure Diagram

```
+------------------+           +------------------+
|     Visitor      |           |     Element      |
|   <<interface>>  |           |   <<interface>>  |
+------------------+           +------------------+
| + visitA(ElementA)           | + accept(Visitor)|
| + visitB(ElementB)           +------------------+
+------------------+                    ^
         ^                              |
         |                    +---------+---------+
+--------+--------+           |                   |
|                 |     +------------+      +------------+
+-------------+  +-------------+    | ElementA   |      | ElementB   |
| ConcreteVis1|  | ConcreteVis2|    +------------+      +------------+
+-------------+  +-------------+    | + accept(v)|      | + accept(v)|
| + visitA()  |  | + visitA()  |    |   v.visitA |      |   v.visitB |
| + visitB()  |  | + visitB()  |    +------------+      +------------+
+-------------+  +-------------+

Double Dispatch:
  element.accept(visitor) --> visitor.visitX(element)
```

**图解说明：**
- `Visitor` 为每种元素类型声明一个访问方法
- `ConcreteVisitor` 实现对各元素的具体操作
- `Element` 声明接受访问者的方法
- `ConcreteElement` 实现 accept 方法，调用对应的访问方法
- 双重分派机制确保调用正确的访问方法

### Python Code Example

```python
"""
Visitor Pattern Implementation in Python
访问者模式的 Python 实现

Example: Document elements with multiple export formats
示例：具有多种导出格式的文档元素
"""

from abc import ABC, abstractmethod
from typing import List


# Visitor interface
class DocumentVisitor(ABC):
    """
    Visitor interface for document elements.
    文档元素的访问者接口。
    """
    @abstractmethod
    def visit_heading(self, heading: 'Heading') -> str:
        pass

    @abstractmethod
    def visit_paragraph(self, paragraph: 'Paragraph') -> str:
        pass

    @abstractmethod
    def visit_image(self, image: 'Image') -> str:
        pass

    @abstractmethod
    def visit_code_block(self, code: 'CodeBlock') -> str:
        pass

    @abstractmethod
    def visit_list(self, list_elem: 'ListElement') -> str:
        pass


# Element interface
class DocumentElement(ABC):
    """
    Element interface for document components.
    文档组件的元素接口。
    """
    @abstractmethod
    def accept(self, visitor: DocumentVisitor) -> str:
        pass


# Concrete Elements
class Heading(DocumentElement):
    """Concrete element: Heading."""
    def __init__(self, text: str, level: int = 1):
        self.text = text
        self.level = level

    def accept(self, visitor: DocumentVisitor) -> str:
        return visitor.visit_heading(self)


class Paragraph(DocumentElement):
    """Concrete element: Paragraph."""
    def __init__(self, text: str):
        self.text = text

    def accept(self, visitor: DocumentVisitor) -> str:
        return visitor.visit_paragraph(self)


class Image(DocumentElement):
    """Concrete element: Image."""
    def __init__(self, url: str, alt_text: str = ""):
        self.url = url
        self.alt_text = alt_text

    def accept(self, visitor: DocumentVisitor) -> str:
        return visitor.visit_image(self)


class CodeBlock(DocumentElement):
    """Concrete element: Code block."""
    def __init__(self, code: str, language: str = ""):
        self.code = code
        self.language = language

    def accept(self, visitor: DocumentVisitor) -> str:
        return visitor.visit_code_block(self)


class ListElement(DocumentElement):
    """Concrete element: List."""
    def __init__(self, items: List[str], ordered: bool = False):
        self.items = items
        self.ordered = ordered

    def accept(self, visitor: DocumentVisitor) -> str:
        return visitor.visit_list(self)


# Concrete Visitors
class HTMLExporter(DocumentVisitor):
    """Visitor: Export to HTML format."""

    def visit_heading(self, heading: Heading) -> str:
        return f"<h{heading.level}>{heading.text}</h{heading.level}>"

    def visit_paragraph(self, paragraph: Paragraph) -> str:
        return f"<p>{paragraph.text}</p>"

    def visit_image(self, image: Image) -> str:
        return f'<img src="{image.url}" alt="{image.alt_text}" />'

    def visit_code_block(self, code: CodeBlock) -> str:
        lang_class = f' class="language-{code.language}"' if code.language else ""
        return f"<pre><code{lang_class}>{code.code}</code></pre>"

    def visit_list(self, list_elem: ListElement) -> str:
        tag = "ol" if list_elem.ordered else "ul"
        items = "\n".join(f"  <li>{item}</li>" for item in list_elem.items)
        return f"<{tag}>\n{items}\n</{tag}>"


class MarkdownExporter(DocumentVisitor):
    """Visitor: Export to Markdown format."""

    def visit_heading(self, heading: Heading) -> str:
        return f"{'#' * heading.level} {heading.text}"

    def visit_paragraph(self, paragraph: Paragraph) -> str:
        return paragraph.text

    def visit_image(self, image: Image) -> str:
        return f"![{image.alt_text}]({image.url})"

    def visit_code_block(self, code: CodeBlock) -> str:
        return f"```{code.language}\n{code.code}\n```"

    def visit_list(self, list_elem: ListElement) -> str:
        if list_elem.ordered:
            return "\n".join(f"{i+1}. {item}" for i, item in enumerate(list_elem.items))
        return "\n".join(f"- {item}" for item in list_elem.items)


class PlainTextExporter(DocumentVisitor):
    """Visitor: Export to plain text format."""

    def visit_heading(self, heading: Heading) -> str:
        text = heading.text.upper()
        underline = "=" * len(text) if heading.level == 1 else "-" * len(text)
        return f"{text}\n{underline}"

    def visit_paragraph(self, paragraph: Paragraph) -> str:
        return paragraph.text

    def visit_image(self, image: Image) -> str:
        return f"[Image: {image.alt_text or image.url}]"

    def visit_code_block(self, code: CodeBlock) -> str:
        lines = code.code.split('\n')
        indented = '\n'.join(f"    {line}" for line in lines)
        return f"Code ({code.language or 'unknown'}):\n{indented}"

    def visit_list(self, list_elem: ListElement) -> str:
        if list_elem.ordered:
            return "\n".join(f"  {i+1}) {item}" for i, item in enumerate(list_elem.items))
        return "\n".join(f"  * {item}" for item in list_elem.items)


class WordCountVisitor(DocumentVisitor):
    """Visitor: Count words in document."""

    def __init__(self):
        self.total_words = 0

    def _count_words(self, text: str) -> int:
        words = len(text.split())
        self.total_words += words
        return words

    def visit_heading(self, heading: Heading) -> str:
        count = self._count_words(heading.text)
        return f"Heading: {count} words"

    def visit_paragraph(self, paragraph: Paragraph) -> str:
        count = self._count_words(paragraph.text)
        return f"Paragraph: {count} words"

    def visit_image(self, image: Image) -> str:
        count = self._count_words(image.alt_text)
        return f"Image alt: {count} words"

    def visit_code_block(self, code: CodeBlock) -> str:
        # Don't count code as words
        return "Code block: (not counted)"

    def visit_list(self, list_elem: ListElement) -> str:
        count = sum(self._count_words(item) for item in list_elem.items)
        return f"List: {count} words"


# Document class (Object Structure)
class Document:
    """Object structure that holds elements."""

    def __init__(self, title: str):
        self.title = title
        self._elements: List[DocumentElement] = []

    def add(self, element: DocumentElement) -> None:
        self._elements.append(element)

    def export(self, visitor: DocumentVisitor) -> str:
        """Export document using the given visitor."""
        results = [element.accept(visitor) for element in self._elements]
        return "\n\n".join(results)


# Client code demonstration
if __name__ == "__main__":
    print("=== Visitor Pattern Demo ===\n")

    # Create a document with various elements
    doc = Document("Sample Document")
    doc.add(Heading("Introduction", level=1))
    doc.add(Paragraph("This is a sample document demonstrating the Visitor pattern."))
    doc.add(Heading("Features", level=2))
    doc.add(ListElement(["Easy to extend", "Clean separation", "Multiple formats"], ordered=False))
    doc.add(CodeBlock("def hello():\n    print('Hello, World!')", language="python"))
    doc.add(Image("https://example.com/image.png", "Example image"))
    doc.add(Paragraph("Thank you for reading!"))

    # Export to different formats using different visitors
    print("1. HTML Export:")
    print("-" * 50)
    html_exporter = HTMLExporter()
    print(doc.export(html_exporter))

    print("\n\n2. Markdown Export:")
    print("-" * 50)
    md_exporter = MarkdownExporter()
    print(doc.export(md_exporter))

    print("\n\n3. Plain Text Export:")
    print("-" * 50)
    text_exporter = PlainTextExporter()
    print(doc.export(text_exporter))

    print("\n\n4. Word Count Analysis:")
    print("-" * 50)
    word_counter = WordCountVisitor()
    analysis = doc.export(word_counter)
    print(analysis)
    print(f"\nTotal word count: {word_counter.total_words}")
```

---

## 11. Interpreter Pattern (解释器模式)

**Given a language, define a representation for its grammar along with an interpreter that uses the representation to interpret sentences in the language.**

### 中文详解

解释器模式是一种行为型设计模式，它定义一个语言的文法，并建立一个解释器来解释该语言中的句子。

**适用场景：**
- 当有一个语言需要解释执行，并且可以将该语言中的句子表示为一个抽象语法树时
- 当语法比较简单时
- 当效率不是关键问题时
- 例如：SQL 解析、正则表达式、简单计算器、配置文件解析

**优点：**
- 可以轻松改变和扩展语法
- 每个语法规则作为一个类，容易实现
- 易于添加新的表达式

**缺点：**
- 对于复杂语法，类的数量会急剧增加
- 效率可能较低

### Structure Diagram

```
+--------------------+
| AbstractExpression |
|    <<interface>>   |
+--------------------+
| + interpret(ctx)   |
+--------------------+
          ^
          |
    +-----+-----+
    |           |
+----------+ +-------------+
| Terminal | | NonTerminal |
| Expression | Expression  |
+----------+ +-------------+
| + interpret| | - children|
+----------+ +-------------+
             | + interpret |
             +-------------+

Context holds variable values and global information

Abstract Syntax Tree Example (a + b * c):
            [+]
           /   \
         [a]   [*]
              /   \
            [b]   [c]
```

**图解说明：**
- `AbstractExpression` 声明解释操作
- `TerminalExpression` 终结符表达式，语法中的基本元素
- `NonTerminalExpression` 非终结符表达式，组合其他表达式
- `Context` 包含解释器的全局信息
- 语法树递归解释求值

### Python Code Example

```python
"""
Interpreter Pattern Implementation in Python
解释器模式的 Python 实现

Example: Simple arithmetic expression interpreter
示例：简单算术表达式解释器
"""

from abc import ABC, abstractmethod
from typing import Dict, Any, List
import re


# Context
class Context:
    """
    Context for the interpreter - holds variables and state.
    解释器的上下文 - 保存变量和状态。
    """
    def __init__(self):
        self._variables: Dict[str, float] = {}

    def set_variable(self, name: str, value: float) -> None:
        self._variables[name] = value

    def get_variable(self, name: str) -> float:
        if name not in self._variables:
            raise ValueError(f"Undefined variable: {name}")
        return self._variables[name]

    def has_variable(self, name: str) -> bool:
        return name in self._variables


# Abstract Expression
class Expression(ABC):
    """
    Abstract expression interface.
    抽象表达式接口。
    """
    @abstractmethod
    def interpret(self, context: Context) -> float:
        pass

    @abstractmethod
    def __str__(self) -> str:
        pass


# Terminal Expressions
class NumberExpression(Expression):
    """Terminal expression: A numeric literal."""
    def __init__(self, value: float):
        self._value = value

    def interpret(self, context: Context) -> float:
        return self._value

    def __str__(self) -> str:
        return str(self._value)


class VariableExpression(Expression):
    """Terminal expression: A variable reference."""
    def __init__(self, name: str):
        self._name = name

    def interpret(self, context: Context) -> float:
        return context.get_variable(self._name)

    def __str__(self) -> str:
        return self._name


# Non-terminal Expressions (Binary Operations)
class AddExpression(Expression):
    """Non-terminal expression: Addition."""
    def __init__(self, left: Expression, right: Expression):
        self._left = left
        self._right = right

    def interpret(self, context: Context) -> float:
        return self._left.interpret(context) + self._right.interpret(context)

    def __str__(self) -> str:
        return f"({self._left} + {self._right})"


class SubtractExpression(Expression):
    """Non-terminal expression: Subtraction."""
    def __init__(self, left: Expression, right: Expression):
        self._left = left
        self._right = right

    def interpret(self, context: Context) -> float:
        return self._left.interpret(context) - self._right.interpret(context)

    def __str__(self) -> str:
        return f"({self._left} - {self._right})"


class MultiplyExpression(Expression):
    """Non-terminal expression: Multiplication."""
    def __init__(self, left: Expression, right: Expression):
        self._left = left
        self._right = right

    def interpret(self, context: Context) -> float:
        return self._left.interpret(context) * self._right.interpret(context)

    def __str__(self) -> str:
        return f"({self._left} * {self._right})"


class DivideExpression(Expression):
    """Non-terminal expression: Division."""
    def __init__(self, left: Expression, right: Expression):
        self._left = left
        self._right = right

    def interpret(self, context: Context) -> float:
        right_val = self._right.interpret(context)
        if right_val == 0:
            raise ValueError("Division by zero")
        return self._left.interpret(context) / right_val

    def __str__(self) -> str:
        return f"({self._left} / {self._right})"


class PowerExpression(Expression):
    """Non-terminal expression: Exponentiation."""
    def __init__(self, base: Expression, exponent: Expression):
        self._base = base
        self._exponent = exponent

    def interpret(self, context: Context) -> float:
        return self._base.interpret(context) ** self._exponent.interpret(context)

    def __str__(self) -> str:
        return f"({self._base} ^ {self._exponent})"


class NegateExpression(Expression):
    """Non-terminal expression: Unary negation."""
    def __init__(self, operand: Expression):
        self._operand = operand

    def interpret(self, context: Context) -> float:
        return -self._operand.interpret(context)

    def __str__(self) -> str:
        return f"(-{self._operand})"


# Parser (builds the AST from string)
class ExpressionParser:
    """
    Parser that converts string expressions to AST.
    将字符串表达式转换为抽象语法树的解析器。
    """
    def __init__(self, expression: str):
        self._tokens = self._tokenize(expression)
        self._pos = 0

    def _tokenize(self, expression: str) -> List[str]:
        """Tokenize the expression string."""
        pattern = r'(\d+\.?\d*|[a-zA-Z_]\w*|[+\-*/^()])'
        tokens = re.findall(pattern, expression)
        return tokens

    def parse(self) -> Expression:
        """Parse and return the root expression."""
        result = self._parse_expression()
        if self._pos < len(self._tokens):
            raise ValueError(f"Unexpected token: {self._tokens[self._pos]}")
        return result

    def _current_token(self) -> str:
        if self._pos < len(self._tokens):
            return self._tokens[self._pos]
        return ""

    def _consume(self, expected: str = None) -> str:
        token = self._current_token()
        if expected and token != expected:
            raise ValueError(f"Expected '{expected}', got '{token}'")
        self._pos += 1
        return token

    def _parse_expression(self) -> Expression:
        """Parse addition and subtraction (lowest precedence)."""
        left = self._parse_term()

        while self._current_token() in ('+', '-'):
            op = self._consume()
            right = self._parse_term()
            if op == '+':
                left = AddExpression(left, right)
            else:
                left = SubtractExpression(left, right)

        return left

    def _parse_term(self) -> Expression:
        """Parse multiplication and division."""
        left = self._parse_power()

        while self._current_token() in ('*', '/'):
            op = self._consume()
            right = self._parse_power()
            if op == '*':
                left = MultiplyExpression(left, right)
            else:
                left = DivideExpression(left, right)

        return left

    def _parse_power(self) -> Expression:
        """Parse exponentiation (right associative)."""
        left = self._parse_unary()

        if self._current_token() == '^':
            self._consume('^')
            right = self._parse_power()  # Right associative
            return PowerExpression(left, right)

        return left

    def _parse_unary(self) -> Expression:
        """Parse unary operators."""
        if self._current_token() == '-':
            self._consume('-')
            return NegateExpression(self._parse_unary())
        return self._parse_primary()

    def _parse_primary(self) -> Expression:
        """Parse numbers, variables, and parenthesized expressions."""
        token = self._current_token()

        if token == '(':
            self._consume('(')
            expr = self._parse_expression()
            self._consume(')')
            return expr

        if re.match(r'\d', token):
            self._consume()
            return NumberExpression(float(token))

        if re.match(r'[a-zA-Z_]', token):
            self._consume()
            return VariableExpression(token)

        raise ValueError(f"Unexpected token: {token}")


# Calculator class (client)
class Calculator:
    """
    Calculator that uses the interpreter pattern.
    使用解释器模式的计算器。
    """
    def __init__(self):
        self._context = Context()

    def set_variable(self, name: str, value: float) -> None:
        """Set a variable value."""
        self._context.set_variable(name, value)
        print(f"  Set {name} = {value}")

    def evaluate(self, expression: str) -> float:
        """Evaluate an expression string."""
        try:
            parser = ExpressionParser(expression)
            ast = parser.parse()
            result = ast.interpret(self._context)
            print(f"  {expression}")
            print(f"  AST: {ast}")
            print(f"  = {result}")
            return result
        except Exception as e:
            print(f"  Error: {e}")
            raise


# Client code demonstration
if __name__ == "__main__":
    print("=== Interpreter Pattern Demo ===\n")

    calc = Calculator()

    print("1. Simple arithmetic:")
    print("-" * 40)
    calc.evaluate("2 + 3 * 4")
    print()
    calc.evaluate("(2 + 3) * 4")
    print()

    print("\n2. Using variables:")
    print("-" * 40)
    calc.set_variable("x", 10)
    calc.set_variable("y", 5)
    print()
    calc.evaluate("x + y")
    print()
    calc.evaluate("x * y - 3")
    print()

    print("\n3. Complex expressions:")
    print("-" * 40)
    calc.evaluate("2 ^ 3 ^ 2")  # Right associative: 2^(3^2) = 2^9 = 512
    print()
    calc.set_variable("a", 2)
    calc.set_variable("b", 3)
    calc.evaluate("a ^ b + b ^ a")  # 2^3 + 3^2 = 8 + 9 = 17
    print()

    print("\n4. Negation:")
    print("-" * 40)
    calc.evaluate("-5 + 3")
    print()
    calc.evaluate("-(2 + 3)")
    print()

    print("\n5. Division:")
    print("-" * 40)
    calc.evaluate("10 / 2 / 5")  # Left associative: (10/2)/5 = 1
```

---

## Complete Pattern Summary

### Behavioral Patterns Overview

| Pattern | Intent | When to Use |
|---------|--------|-------------|
| **Chain of Responsibility** | Pass request along chain of handlers | Multiple handlers for same request |
| **Command** | Encapsulate request as object | Undo/redo, queuing, logging |
| **Iterator** | Sequential access to collection | Hide collection internals |
| **Mediator** | Centralize complex communications | Many-to-many relationships |
| **Memento** | Capture and restore object state | Undo functionality, snapshots |
| **Observer** | Notify dependents of state changes | Event systems, pub/sub |
| **State** | Alter behavior when state changes | State machines, workflows |
| **Strategy** | Interchangeable algorithms | Multiple algorithm variants |
| **Template Method** | Define algorithm skeleton | Reusable algorithm structure |
| **Visitor** | Operations on object structures | Multiple operations on elements |
| **Interpreter** | Interpret grammar/language | DSLs, expression evaluation |

---

## All Design Patterns Reference

### Creational Patterns (创建型模式)
- Singleton (单例) - One instance
- Factory Method (工厂方法) - Subclass creates
- Abstract Factory (抽象工厂) - Create families
- Builder (生成器) - Step-by-step construction
- Prototype (原型) - Clone existing

### Structural Patterns (结构型模式)
- Adapter (适配器) - Interface conversion
- Bridge (桥接) - Separate abstraction/implementation
- Composite (组合) - Tree structures
- Decorator (装饰器) - Add responsibilities
- Facade (外观) - Simplified interface
- Flyweight (享元) - Share fine-grained objects
- Proxy (代理) - Control access

### Behavioral Patterns (行为型模式)
- Chain of Responsibility (责任链) - Pass along chain
- Command (命令) - Encapsulate request
- Iterator (迭代器) - Sequential access
- Mediator (中介者) - Centralize communication
- Memento (备忘录) - Save/restore state
- Observer (观察者) - State change notification
- State (状态) - Behavior varies with state
- Strategy (策略) - Interchangeable algorithms
- Template Method (模板方法) - Algorithm skeleton
- Visitor (访问者) - Operations on structure
- Interpreter (解释器) - Language interpretation

---

*Document complete. See also:*
- `design_patterns_creational.md`
- `design_patterns_structural.md`
- `design_patterns_behavioral_1.md`

