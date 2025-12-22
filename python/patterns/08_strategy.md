# Strategy Pattern in Python

---

## 1. Pattern Name: Strategy (Policy)

**Purpose / Problem Solved:**
Define a family of algorithms, encapsulate each one, and make them interchangeable. Strategy lets the algorithm vary independently from clients that use it. Avoids complex conditionals by delegating behavior to strategy objects.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                          CONTEXT                                  |
|------------------------------------------------------------------|
| - strategy: Strategy                                              |
|------------------------------------------------------------------|
| + set_strategy(strategy: Strategy)                                |
| + execute_strategy(data)                                          |
|     return strategy.algorithm(data)                               |
+------------------------------------------------------------------+
                              |
                              | Uses
                              v
+------------------------------------------------------------------+
|                    STRATEGY (Interface)                           |
|------------------------------------------------------------------|
| + algorithm(data) -> Result                                       |
+------------------------------------------------------------------+
         ^                    ^                    ^
         |                    |                    |
+-----------------+  +-----------------+  +-----------------+
| ConcreteStratA  |  | ConcreteStratB  |  | ConcreteStratC  |
|-----------------|  |-----------------|  |-----------------|
| + algorithm()   |  | + algorithm()   |  | + algorithm()   |
| # Implementation|  | # Implementation|  | # Implementation|
| # A             |  | # B             |  | # C             |
+-----------------+  +-----------------+  +-----------------+

RUNTIME STRATEGY SWITCHING:
+----------+          +----------+          +----------+
| Context  |          | Context  |          | Context  |
|----------|  switch  |----------|  switch  |----------|
| strategy |--------->| strategy |--------->| strategy |
|   = A    |          |   = B    |          |   = C    |
+----------+          +----------+          +----------+
     |                     |                     |
     v                     v                     v
[Algorithm A]        [Algorithm B]        [Algorithm C]
```

**中文说明：**
策略模式将算法封装成独立的类，使它们可以互换。Context类持有Strategy引用，将工作委托给Strategy。客户端可以在运行时切换策略，无需修改Context代码。这避免了大量if-else或switch语句，符合开闭原则。Python中由于支持一等函数，简单策略可以直接用函数实现，无需类层次结构。常用于：排序算法、压缩算法、验证规则、定价策略等需要多种可互换实现的场景。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Strategy |
|----------------|--------------------------|
| **First-class functions** | Functions can BE the strategy (no class needed) |
| **Duck typing** | Any object with right methods works as strategy |
| **Protocol classes** | Type-safe strategy interface without inheritance |
| **Lambda functions** | Inline simple strategies |
| **Dict dispatch** | Map names to strategy functions |
| **`operator` module** | Ready-made strategy functions |
| **`functools.partial`** | Create configured strategy functions |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Python sorted()** | `key=` parameter is a strategy for comparison |
| **Django validators** | Validation strategies for form fields |
| **pytest** | Plugin system uses strategy for test collection |
| **Pandas** | `agg()` takes strategy functions for aggregation |
| **Requests** | Auth handlers are strategy objects |

```python
# Python's built-in strategy pattern with sorted()
data = ["apple", "Banana", "cherry"]

# Strategy 1: Case-insensitive
sorted(data, key=str.lower)  # ['apple', 'Banana', 'cherry']

# Strategy 2: By length
sorted(data, key=len)  # ['apple', 'cherry', 'Banana']

# Strategy 3: Custom
sorted(data, key=lambda x: x[-1])  # By last character
```

---

## 5. Python Module Examples

### Example 1: Classic Strategy with Classes

```python
#!/usr/bin/env python3
"""
Strategy Pattern - Classic Implementation with Classes

Demonstrates the traditional OOP approach to Strategy.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List


# ============== STRATEGY INTERFACE ==============

class PaymentStrategy(ABC):
    """Abstract strategy for payment processing."""
    
    @abstractmethod
    def pay(self, amount: float) -> str:
        """Process a payment."""
        pass
    
    @abstractmethod
    def validate(self) -> bool:
        """Validate payment details."""
        pass


# ============== CONCRETE STRATEGIES ==============

@dataclass
class CreditCardPayment(PaymentStrategy):
    """Credit card payment strategy."""
    card_number: str
    cvv: str
    expiry: str
    
    def pay(self, amount: float) -> str:
        masked = f"****{self.card_number[-4:]}"
        return f"💳 Paid ${amount:.2f} with Credit Card {masked}"
    
    def validate(self) -> bool:
        return (
            len(self.card_number) == 16 and
            len(self.cvv) == 3 and
            len(self.expiry) == 5
        )


@dataclass
class PayPalPayment(PaymentStrategy):
    """PayPal payment strategy."""
    email: str
    
    def pay(self, amount: float) -> str:
        return f"📧 Paid ${amount:.2f} via PayPal ({self.email})"
    
    def validate(self) -> bool:
        return "@" in self.email


@dataclass
class CryptoPayment(PaymentStrategy):
    """Cryptocurrency payment strategy."""
    wallet_address: str
    currency: str = "BTC"
    
    def pay(self, amount: float) -> str:
        short_addr = f"{self.wallet_address[:6]}...{self.wallet_address[-4:]}"
        return f"₿ Paid ${amount:.2f} in {self.currency} to {short_addr}"
    
    def validate(self) -> bool:
        return len(self.wallet_address) >= 26


@dataclass
class BankTransferPayment(PaymentStrategy):
    """Bank transfer payment strategy."""
    account_number: str
    routing_number: str
    
    def pay(self, amount: float) -> str:
        masked = f"****{self.account_number[-4:]}"
        return f"🏦 Transferred ${amount:.2f} to account {masked}"
    
    def validate(self) -> bool:
        return len(self.account_number) >= 8 and len(self.routing_number) == 9


# ============== CONTEXT ==============

class ShoppingCart:
    """
    Context class that uses payment strategies.
    
    The cart doesn't know HOW payment works - it delegates
    to the strategy object.
    """
    
    def __init__(self):
        self.items: List[tuple[str, float]] = []
        self._payment_strategy: PaymentStrategy = None
    
    def add_item(self, name: str, price: float):
        """Add item to cart."""
        self.items.append((name, price))
    
    def get_total(self) -> float:
        """Calculate total price."""
        return sum(price for _, price in self.items)
    
    def set_payment_strategy(self, strategy: PaymentStrategy):
        """Set the payment strategy."""
        self._payment_strategy = strategy
    
    def checkout(self) -> str:
        """Complete checkout using current strategy."""
        if not self._payment_strategy:
            raise ValueError("No payment strategy set!")
        
        if not self._payment_strategy.validate():
            raise ValueError("Invalid payment details!")
        
        total = self.get_total()
        result = self._payment_strategy.pay(total)
        
        # Clear cart after successful payment
        self.items.clear()
        return result


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Strategy Pattern - Payment Processing")
    print("=" * 60)
    
    # Create shopping cart
    cart = ShoppingCart()
    cart.add_item("Laptop", 999.99)
    cart.add_item("Mouse", 29.99)
    cart.add_item("Keyboard", 79.99)
    
    print(f"\nCart total: ${cart.get_total():.2f}")
    
    # Different payment strategies
    strategies = [
        ("Credit Card", CreditCardPayment(
            card_number="4111111111111111",
            cvv="123",
            expiry="12/25"
        )),
        ("PayPal", PayPalPayment(email="user@example.com")),
        ("Crypto", CryptoPayment(
            wallet_address="bc1qar0srrr7xfkvy5l643lydnw9re59gtzzwf5mdq",
            currency="BTC"
        )),
        ("Bank Transfer", BankTransferPayment(
            account_number="123456789",
            routing_number="021000021"
        )),
    ]
    
    for name, strategy in strategies:
        # Re-add items for each demo
        cart.add_item("Item", 100.00)
        
        print(f"\n--- Paying with {name} ---")
        cart.set_payment_strategy(strategy)
        result = cart.checkout()
        print(result)
```

---

### Example 2: Pythonic Strategy with Functions

```python
#!/usr/bin/env python3
"""
Strategy Pattern - Pythonic Implementation with Functions

In Python, functions are first-class citizens.
A function can BE the strategy - no class needed!
"""

from typing import Callable, List
from dataclasses import dataclass
from functools import partial


# ============== STRATEGY FUNCTIONS ==============

def bubble_sort(data: List[int]) -> List[int]:
    """Bubble sort strategy - O(n²)."""
    result = data.copy()
    n = len(result)
    for i in range(n):
        for j in range(0, n - i - 1):
            if result[j] > result[j + 1]:
                result[j], result[j + 1] = result[j + 1], result[j]
    return result


def quick_sort(data: List[int]) -> List[int]:
    """Quick sort strategy - O(n log n) average."""
    if len(data) <= 1:
        return data.copy()
    
    pivot = data[len(data) // 2]
    left = [x for x in data if x < pivot]
    middle = [x for x in data if x == pivot]
    right = [x for x in data if x > pivot]
    
    return quick_sort(left) + middle + quick_sort(right)


def insertion_sort(data: List[int]) -> List[int]:
    """Insertion sort strategy - O(n²) but fast for small arrays."""
    result = data.copy()
    for i in range(1, len(result)):
        key = result[i]
        j = i - 1
        while j >= 0 and result[j] > key:
            result[j + 1] = result[j]
            j -= 1
        result[j + 1] = key
    return result


def python_sort(data: List[int]) -> List[int]:
    """Use Python's built-in Timsort."""
    return sorted(data)


# ============== CONTEXT ==============

# Type alias for sort strategy
SortStrategy = Callable[[List[int]], List[int]]


class DataProcessor:
    """
    Context that uses sorting strategies.
    
    Strategy is just a function!
    """
    
    def __init__(self, sort_strategy: SortStrategy = python_sort):
        self._sort = sort_strategy
        self._data: List[int] = []
    
    @property
    def sort_strategy(self) -> SortStrategy:
        return self._sort
    
    @sort_strategy.setter
    def sort_strategy(self, strategy: SortStrategy):
        self._sort = strategy
    
    def set_data(self, data: List[int]):
        self._data = data
    
    def process(self) -> List[int]:
        """Process data using current strategy."""
        return self._sort(self._data)


# ============== STRATEGY SELECTION ==============

def get_best_strategy(data_size: int) -> SortStrategy:
    """
    Dynamically select best strategy based on data size.
    
    This is a form of Strategy pattern too!
    """
    if data_size < 10:
        return insertion_sort  # Fast for tiny arrays
    elif data_size < 1000:
        return quick_sort
    else:
        return python_sort  # Timsort is optimized


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Strategy Pattern - Pythonic with Functions")
    print("=" * 60)
    
    import random
    import time
    
    # Test data
    test_data = random.sample(range(1000), 100)
    
    # All strategies
    strategies = {
        "Bubble Sort": bubble_sort,
        "Quick Sort": quick_sort,
        "Insertion Sort": insertion_sort,
        "Python's Timsort": python_sort,
    }
    
    processor = DataProcessor()
    processor.set_data(test_data)
    
    print(f"\nSorting {len(test_data)} elements:\n")
    
    for name, strategy in strategies.items():
        processor.sort_strategy = strategy
        
        start = time.perf_counter()
        result = processor.process()
        elapsed = time.perf_counter() - start
        
        # Verify correctness
        is_sorted = result == sorted(test_data)
        status = "✓" if is_sorted else "✗"
        
        print(f"{status} {name:20} took {elapsed*1000:.3f}ms")
    
    # Dynamic strategy selection
    print("\n--- Dynamic Strategy Selection ---")
    for size in [5, 50, 500, 5000]:
        strategy = get_best_strategy(size)
        print(f"Size {size:5}: use {strategy.__name__}")
    
    # Lambda as strategy
    print("\n--- Lambda Strategies ---")
    
    # Strategy via lambda
    reverse_sort = lambda data: sorted(data, reverse=True)
    abs_sort = lambda data: sorted(data, key=abs)
    
    processor.set_data([-5, 3, -1, 4, -2])
    
    processor.sort_strategy = reverse_sort
    print(f"Reverse: {processor.process()}")
    
    processor.sort_strategy = abs_sort
    print(f"By absolute: {processor.process()}")
```

---

### Example 3: Compression Strategies

```python
#!/usr/bin/env python3
"""
Strategy Pattern - Compression Strategies

Practical example with different compression algorithms.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Protocol
import zlib
import bz2
import lzma


# ============== STRATEGY INTERFACE ==============

class CompressionStrategy(Protocol):
    """Protocol for compression strategies."""
    
    name: str
    
    def compress(self, data: bytes) -> bytes:
        ...
    
    def decompress(self, data: bytes) -> bytes:
        ...


# ============== CONCRETE STRATEGIES ==============

@dataclass
class ZlibCompression:
    """Zlib (deflate) compression strategy."""
    name: str = "zlib"
    level: int = 6  # 0-9, 9 = max compression
    
    def compress(self, data: bytes) -> bytes:
        return zlib.compress(data, level=self.level)
    
    def decompress(self, data: bytes) -> bytes:
        return zlib.decompress(data)


@dataclass
class Bz2Compression:
    """BZ2 compression strategy."""
    name: str = "bz2"
    level: int = 9
    
    def compress(self, data: bytes) -> bytes:
        return bz2.compress(data, compresslevel=self.level)
    
    def decompress(self, data: bytes) -> bytes:
        return bz2.decompress(data)


@dataclass
class LzmaCompression:
    """LZMA compression strategy (best ratio, slowest)."""
    name: str = "lzma"
    
    def compress(self, data: bytes) -> bytes:
        return lzma.compress(data)
    
    def decompress(self, data: bytes) -> bytes:
        return lzma.decompress(data)


@dataclass
class NoCompression:
    """No compression (pass-through) strategy."""
    name: str = "none"
    
    def compress(self, data: bytes) -> bytes:
        return data
    
    def decompress(self, data: bytes) -> bytes:
        return data


# ============== CONTEXT ==============

class FileCompressor:
    """
    Context that compresses files using configurable strategy.
    """
    
    def __init__(self, strategy: CompressionStrategy = None):
        self._strategy = strategy or ZlibCompression()
    
    @property
    def strategy(self) -> CompressionStrategy:
        return self._strategy
    
    @strategy.setter
    def strategy(self, strategy: CompressionStrategy):
        self._strategy = strategy
    
    def compress_data(self, data: bytes) -> tuple[bytes, dict]:
        """
        Compress data and return result with stats.
        """
        compressed = self._strategy.compress(data)
        
        original_size = len(data)
        compressed_size = len(compressed)
        ratio = (1 - compressed_size / original_size) * 100 if original_size > 0 else 0
        
        stats = {
            "algorithm": self._strategy.name,
            "original_size": original_size,
            "compressed_size": compressed_size,
            "compression_ratio": f"{ratio:.1f}%",
        }
        
        return compressed, stats
    
    def decompress_data(self, data: bytes) -> bytes:
        """Decompress data."""
        return self._strategy.decompress(data)
    
    def verify_roundtrip(self, data: bytes) -> bool:
        """Verify that compress->decompress returns original."""
        compressed = self._strategy.compress(data)
        decompressed = self._strategy.decompress(compressed)
        return data == decompressed


# ============== STRATEGY FACTORY ==============

def get_compression_strategy(
    name: str, 
    prefer_speed: bool = False,
    prefer_ratio: bool = False
) -> CompressionStrategy:
    """
    Factory function to get compression strategy.
    """
    strategies = {
        "none": NoCompression(),
        "zlib": ZlibCompression(level=6 if not prefer_ratio else 9),
        "bz2": Bz2Compression(level=9 if prefer_ratio else 5),
        "lzma": LzmaCompression(),
    }
    
    if name not in strategies:
        raise ValueError(f"Unknown compression: {name}")
    
    return strategies[name]


def select_best_strategy(data: bytes, prefer_speed: bool = True) -> CompressionStrategy:
    """
    Auto-select best strategy based on data characteristics.
    """
    size = len(data)
    
    if size < 100:
        return NoCompression()  # Not worth compressing
    elif size < 10_000:
        return ZlibCompression(level=6)  # Fast, decent
    elif prefer_speed:
        return ZlibCompression(level=1)  # Fastest zlib
    else:
        return LzmaCompression()  # Best ratio


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Strategy Pattern - Compression Strategies")
    print("=" * 60)
    
    import time
    
    # Test data: repeated text compresses well
    test_data = (b"Hello, World! " * 1000 + 
                 b"Strategy pattern is useful. " * 500)
    
    print(f"\nOriginal size: {len(test_data):,} bytes\n")
    
    compressor = FileCompressor()
    
    # Compare all strategies
    strategies = [
        NoCompression(),
        ZlibCompression(level=1),
        ZlibCompression(level=6),
        ZlibCompression(level=9),
        Bz2Compression(),
        LzmaCompression(),
    ]
    
    print(f"{'Strategy':<20} {'Size':>10} {'Ratio':>10} {'Time':>10}")
    print("-" * 52)
    
    for strategy in strategies:
        compressor.strategy = strategy
        
        start = time.perf_counter()
        compressed, stats = compressor.compress_data(test_data)
        elapsed = time.perf_counter() - start
        
        # Verify roundtrip
        assert compressor.verify_roundtrip(test_data)
        
        name = f"{strategy.name}"
        if hasattr(strategy, 'level'):
            name += f" (L{strategy.level})"
        
        print(f"{name:<20} {stats['compressed_size']:>10,} "
              f"{stats['compression_ratio']:>10} {elapsed*1000:>8.2f}ms")
    
    # Dynamic strategy selection
    print("\n--- Auto-selecting Strategy ---")
    for size in [50, 5000, 50000]:
        data = b"x" * size
        strategy = select_best_strategy(data)
        print(f"Size {size:>6}: {strategy.name}")
```

---

### Example 4: Validation Strategies

```python
#!/usr/bin/env python3
"""
Strategy Pattern - Validation Strategies

Flexible validation rules that can be combined.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List, Any, Callable
import re


# ============== VALIDATION STRATEGY ==============

class ValidationRule(ABC):
    """Abstract validation rule."""
    
    @abstractmethod
    def validate(self, value: Any) -> tuple[bool, str]:
        """
        Validate a value.
        
        Returns:
            (is_valid, error_message)
        """
        pass


# ============== CONCRETE STRATEGIES ==============

@dataclass
class Required(ValidationRule):
    """Value must not be empty."""
    field_name: str = "Value"
    
    def validate(self, value: Any) -> tuple[bool, str]:
        if value is None or value == "" or value == []:
            return False, f"{self.field_name} is required"
        return True, ""


@dataclass
class MinLength(ValidationRule):
    """String must have minimum length."""
    min_len: int
    field_name: str = "Value"
    
    def validate(self, value: Any) -> tuple[bool, str]:
        if len(str(value)) < self.min_len:
            return False, f"{self.field_name} must be at least {self.min_len} characters"
        return True, ""


@dataclass
class MaxLength(ValidationRule):
    """String must not exceed maximum length."""
    max_len: int
    field_name: str = "Value"
    
    def validate(self, value: Any) -> tuple[bool, str]:
        if len(str(value)) > self.max_len:
            return False, f"{self.field_name} must be at most {self.max_len} characters"
        return True, ""


@dataclass
class Pattern(ValidationRule):
    """Value must match regex pattern."""
    pattern: str
    message: str = "Invalid format"
    
    def validate(self, value: Any) -> tuple[bool, str]:
        if not re.match(self.pattern, str(value)):
            return False, self.message
        return True, ""


@dataclass
class Range(ValidationRule):
    """Numeric value must be in range."""
    min_val: float
    max_val: float
    field_name: str = "Value"
    
    def validate(self, value: Any) -> tuple[bool, str]:
        try:
            num = float(value)
            if not (self.min_val <= num <= self.max_val):
                return False, f"{self.field_name} must be between {self.min_val} and {self.max_val}"
            return True, ""
        except (ValueError, TypeError):
            return False, f"{self.field_name} must be a number"


@dataclass
class InList(ValidationRule):
    """Value must be in allowed list."""
    allowed: List[Any]
    field_name: str = "Value"
    
    def validate(self, value: Any) -> tuple[bool, str]:
        if value not in self.allowed:
            return False, f"{self.field_name} must be one of: {self.allowed}"
        return True, ""


@dataclass
class Custom(ValidationRule):
    """Custom validation function."""
    validator: Callable[[Any], bool]
    message: str
    
    def validate(self, value: Any) -> tuple[bool, str]:
        if not self.validator(value):
            return False, self.message
        return True, ""


# ============== VALIDATOR (CONTEXT) ==============

class FieldValidator:
    """
    Validates a field using multiple rules.
    """
    
    def __init__(self, field_name: str, *rules: ValidationRule):
        self.field_name = field_name
        self.rules: List[ValidationRule] = list(rules)
    
    def add_rule(self, rule: ValidationRule) -> "FieldValidator":
        """Add a validation rule (fluent interface)."""
        self.rules.append(rule)
        return self
    
    def validate(self, value: Any) -> tuple[bool, List[str]]:
        """
        Validate value against all rules.
        
        Returns:
            (is_valid, list_of_errors)
        """
        errors = []
        
        for rule in self.rules:
            is_valid, error = rule.validate(value)
            if not is_valid:
                errors.append(error)
        
        return len(errors) == 0, errors


class FormValidator:
    """
    Validates a form with multiple fields.
    """
    
    def __init__(self):
        self.fields: dict[str, FieldValidator] = {}
    
    def add_field(self, validator: FieldValidator) -> "FormValidator":
        """Add a field validator."""
        self.fields[validator.field_name] = validator
        return self
    
    def validate(self, data: dict) -> tuple[bool, dict]:
        """
        Validate all fields.
        
        Returns:
            (is_valid, {field: [errors]})
        """
        all_errors = {}
        
        for field_name, validator in self.fields.items():
            value = data.get(field_name)
            is_valid, errors = validator.validate(value)
            if not is_valid:
                all_errors[field_name] = errors
        
        return len(all_errors) == 0, all_errors


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Strategy Pattern - Form Validation")
    print("=" * 60)
    
    # Create form validator
    form = FormValidator()
    
    # Username: required, 3-20 chars, alphanumeric
    form.add_field(
        FieldValidator("username",
            Required(field_name="Username"),
            MinLength(3, field_name="Username"),
            MaxLength(20, field_name="Username"),
            Pattern(r"^[a-zA-Z0-9_]+$", "Username must be alphanumeric"),
        )
    )
    
    # Email: required, valid format
    form.add_field(
        FieldValidator("email",
            Required(field_name="Email"),
            Pattern(r"^[\w.-]+@[\w.-]+\.\w+$", "Invalid email format"),
        )
    )
    
    # Age: optional, 18-120
    form.add_field(
        FieldValidator("age",
            Range(18, 120, field_name="Age"),
        )
    )
    
    # Role: must be in list
    form.add_field(
        FieldValidator("role",
            Required(field_name="Role"),
            InList(["admin", "user", "guest"], field_name="Role"),
        )
    )
    
    # Test cases
    test_forms = [
        {
            "username": "john_doe",
            "email": "john@example.com",
            "age": 25,
            "role": "user",
        },
        {
            "username": "ab",  # too short
            "email": "invalid-email",  # bad format
            "age": 15,  # too young
            "role": "superuser",  # not allowed
        },
        {
            "username": "",  # required
            "email": "valid@email.com",
            "age": 150,  # too old
            "role": "admin",
        },
    ]
    
    for i, data in enumerate(test_forms, 1):
        print(f"\n--- Test Case {i} ---")
        print(f"Data: {data}")
        
        is_valid, errors = form.validate(data)
        
        if is_valid:
            print("✓ Valid!")
        else:
            print("✗ Validation errors:")
            for field, field_errors in errors.items():
                for error in field_errors:
                    print(f"  - {error}")
```

---

## 6. When to Use / When to Avoid

### Use When:
- You have **multiple algorithms** for a task
- Algorithms need to be **interchangeable** at runtime
- You want to avoid complex **conditional statements**
- Different clients need **different behaviors**

### Avoid When:
- Only one algorithm exists (no need for abstraction)
- Algorithms rarely change
- Simple conditionals suffice

### Python Idiom:
Use **functions** instead of classes for simple strategies:

```python
# Pythonic: function as strategy
def process(data, strategy=str.upper):
    return strategy(data)

process("hello", str.lower)  # 'hello'
process("hello", str.title)  # 'Hello'
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **State** | Similar structure but different intent: State changes behavior based on object state |
| **Template Method** | Fixed algorithm skeleton vs interchangeable algorithms |
| **Command** | Commands can use different strategies for execution |
| **Decorator** | Enhances object; Strategy replaces algorithm |

