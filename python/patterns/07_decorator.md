# Decorator Pattern in Python

---

## 1. Pattern Name: Decorator (Wrapper)

**Purpose / Problem Solved:**
Attach additional responsibilities to an object dynamically. Decorators provide a flexible alternative to subclassing for extending functionality. Add behavior without modifying the original object.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT CODE                               |
|------------------------------------------------------------------|
| component = ConcreteComponent()                                   |
| decorated = Decorator1(Decorator2(component))                     |
| decorated.operation()                                             |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                      COMPONENT (Interface)                        |
|------------------------------------------------------------------|
| + operation() -> Result                                           |
+------------------------------------------------------------------+
         ^                                    ^
         |                                    |
+---------------------+             +---------------------+
| ConcreteComponent   |             |    DECORATOR        |
|---------------------|             |---------------------|
| + operation()       |             | - wrapped: Component|
+---------------------+             |---------------------|
                                    | + operation()       |
                                    |   wrapped.operation()|
                                    |   + extra behavior  |
                                    +---------------------+
                                              ^
                              +---------------+---------------+
                              |                               |
                    +------------------+            +------------------+
                    | ConcreteDecoratorA|            | ConcreteDecoratorB|
                    |------------------|            |------------------|
                    | + operation()    |            | + operation()    |
                    |   add_behavior_a |            |   add_behavior_b |
                    |   super.operation|            |   super.operation|
                    +------------------+            +------------------+

DECORATION CHAIN:
+--------+     +------------+     +------------+     +------------+
| Client | --> | DecoratorB | --> | DecoratorA | --> | Component  |
+--------+     +------------+     +------------+     +------------+
                    |                   |                  |
                    | operation()       | operation()      | operation()
                    |                   |                  |
                    v                   v                  v
              [pre_B + ...]      [pre_A + ...]      [core logic]
              [... + post_B]     [... + post_A]

Python's @ Decorator Syntax (Functions):
@decorator
def function():
    pass

# Is equivalent to:
function = decorator(function)
```

**中文说明：**
装饰器模式动态地向对象添加职责。装饰器包装原始对象，拦截对其方法的调用，在调用前后添加额外行为。可以无限嵌套装饰器形成调用链。Python有两层装饰器概念：(1) 设计模式中的装饰器类，(2) Python的@语法糖用于函数/类装饰。两者原理相同但应用层面不同。装饰器模式遵循开闭原则——扩展功能无需修改原有代码。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Decorator |
|----------------|---------------------------|
| **`@decorator` syntax** | Native language support for function decorators |
| **First-class functions** | Functions can be passed and returned |
| **Closures** | Decorator can access outer scope variables |
| **`functools.wraps`** | Preserve original function metadata |
| **`*args, **kwargs`** | Forward all arguments to wrapped function |
| **`__call__`** | Make decorator class callable |
| **`__getattr__`** | Delegate unknown attributes to wrapped object |
| **Class decorators** | `@decorator` on class definitions |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Flask** | `@app.route`, `@login_required` decorators |
| **Django** | `@login_required`, `@permission_required`, `@cache_page` |
| **pytest** | `@pytest.fixture`, `@pytest.mark.parametrize` |
| **functools** | `@lru_cache`, `@wraps`, `@total_ordering` |
| **dataclasses** | `@dataclass` decorator transforms class |
| **contextlib** | `@contextmanager` for generator-based context managers |

```python
# Flask route decorator example
from flask import Flask
app = Flask(__name__)

@app.route('/hello')  # Decorator registers route
def hello():
    return 'Hello, World!'

# functools.lru_cache decorator
from functools import lru_cache

@lru_cache(maxsize=100)
def fibonacci(n):
    if n < 2:
        return n
    return fibonacci(n-1) + fibonacci(n-2)
```

---

## 5. Python Module Examples

### Example 1: Function Decorators

```python
#!/usr/bin/env python3
"""
Decorator Pattern - Function Decorators

The most Pythonic implementation of the decorator pattern.
Uses @decorator syntax for clean, readable code.
"""

import functools
import time
from typing import Callable, Any


# ============== SIMPLE DECORATOR ==============

def timer(func: Callable) -> Callable:
    """
    Decorator that measures function execution time.
    
    Uses @functools.wraps to preserve function metadata.
    """
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        start = time.perf_counter()
        result = func(*args, **kwargs)
        elapsed = time.perf_counter() - start
        print(f"⏱️ {func.__name__} took {elapsed:.4f}s")
        return result
    return wrapper


def debug(func: Callable) -> Callable:
    """
    Decorator that prints function call details.
    """
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        args_repr = [repr(a) for a in args]
        kwargs_repr = [f"{k}={v!r}" for k, v in kwargs.items()]
        signature = ", ".join(args_repr + kwargs_repr)
        print(f"📞 Calling {func.__name__}({signature})")
        result = func(*args, **kwargs)
        print(f"📤 {func.__name__} returned {result!r}")
        return result
    return wrapper


def retry(max_attempts: int = 3, delay: float = 1.0):
    """
    Decorator factory that retries failed function calls.
    
    This is a decorator with parameters - returns a decorator.
    """
    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            last_exception = None
            for attempt in range(1, max_attempts + 1):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    last_exception = e
                    print(f"⚠️ Attempt {attempt}/{max_attempts} failed: {e}")
                    if attempt < max_attempts:
                        time.sleep(delay)
            raise last_exception
        return wrapper
    return decorator


def cache(func: Callable) -> Callable:
    """
    Simple caching decorator (memoization).
    
    Uses a dictionary to store results.
    """
    memo = {}
    
    @functools.wraps(func)
    def wrapper(*args):
        if args not in memo:
            memo[args] = func(*args)
        return memo[args]
    
    wrapper.cache = memo  # Expose cache for debugging
    wrapper.cache_clear = memo.clear
    return wrapper


# ============== DECORATED FUNCTIONS ==============

@timer
@debug
def slow_function(n: int) -> int:
    """A slow function to demonstrate decorators."""
    time.sleep(0.1)
    return n * 2


@retry(max_attempts=3, delay=0.5)
def unreliable_function(fail_count: list) -> str:
    """Function that fails a few times before succeeding."""
    if fail_count[0] > 0:
        fail_count[0] -= 1
        raise ConnectionError("Network error")
    return "Success!"


@cache
def fibonacci(n: int) -> int:
    """Fibonacci with caching."""
    if n < 2:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)


# ============== STACKING DECORATORS ==============

@timer
@cache
def expensive_computation(x: int, y: int) -> int:
    """Cached and timed computation."""
    time.sleep(0.2)  # Simulate expensive work
    return x ** y


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Decorator Pattern - Function Decorators")
    print("=" * 60)
    
    # Timer and debug
    print("\n--- Timer & Debug Decorators ---")
    result = slow_function(5)
    print(f"Result: {result}")
    
    # Retry decorator
    print("\n--- Retry Decorator ---")
    fail_counter = [2]  # Will fail twice then succeed
    try:
        result = unreliable_function(fail_counter)
        print(f"Final result: {result}")
    except Exception as e:
        print(f"All retries failed: {e}")
    
    # Cache decorator
    print("\n--- Cache Decorator ---")
    print("Computing fibonacci(30)...")
    start = time.perf_counter()
    result = fibonacci(30)
    elapsed = time.perf_counter() - start
    print(f"fib(30) = {result} (took {elapsed:.4f}s)")
    
    print("\nComputing fibonacci(30) again (cached)...")
    start = time.perf_counter()
    result = fibonacci(30)
    elapsed = time.perf_counter() - start
    print(f"fib(30) = {result} (took {elapsed:.6f}s)")
    
    # Stacked decorators
    print("\n--- Stacked Decorators ---")
    print("First call (not cached):")
    expensive_computation(2, 10)
    
    print("\nSecond call (cached):")
    expensive_computation(2, 10)
```

---

### Example 2: Class Decorator Pattern

```python
#!/usr/bin/env python3
"""
Decorator Pattern - Class-based Implementation

Uses classes to implement decorators for more complex scenarios.
Decorators wrap objects and add behavior.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Protocol


# ============== COMPONENT INTERFACE ==============

class Coffee(Protocol):
    """Interface for coffee beverages."""
    
    def get_description(self) -> str:
        ...
    
    def get_cost(self) -> float:
        ...


# ============== CONCRETE COMPONENTS ==============

@dataclass
class Espresso:
    """Base espresso coffee."""
    
    def get_description(self) -> str:
        return "Espresso"
    
    def get_cost(self) -> float:
        return 2.00


@dataclass
class HouseBlend:
    """House blend coffee."""
    
    def get_description(self) -> str:
        return "House Blend Coffee"
    
    def get_cost(self) -> float:
        return 1.50


@dataclass
class DarkRoast:
    """Dark roast coffee."""
    
    def get_description(self) -> str:
        return "Dark Roast Coffee"
    
    def get_cost(self) -> float:
        return 1.75


# ============== DECORATOR BASE ==============

class CoffeeDecorator(ABC):
    """
    Abstract base for coffee decorators.
    
    Wraps a coffee and delegates to it.
    """
    
    def __init__(self, coffee: Coffee):
        self._coffee = coffee
    
    @abstractmethod
    def get_description(self) -> str:
        pass
    
    @abstractmethod
    def get_cost(self) -> float:
        pass


# ============== CONCRETE DECORATORS ==============

class Milk(CoffeeDecorator):
    """Add milk to coffee."""
    
    def get_description(self) -> str:
        return f"{self._coffee.get_description()}, Milk"
    
    def get_cost(self) -> float:
        return self._coffee.get_cost() + 0.30


class Mocha(CoffeeDecorator):
    """Add mocha (chocolate) to coffee."""
    
    def get_description(self) -> str:
        return f"{self._coffee.get_description()}, Mocha"
    
    def get_cost(self) -> float:
        return self._coffee.get_cost() + 0.50


class Whip(CoffeeDecorator):
    """Add whipped cream to coffee."""
    
    def get_description(self) -> str:
        return f"{self._coffee.get_description()}, Whip"
    
    def get_cost(self) -> float:
        return self._coffee.get_cost() + 0.40


class Soy(CoffeeDecorator):
    """Add soy milk to coffee."""
    
    def get_description(self) -> str:
        return f"{self._coffee.get_description()}, Soy"
    
    def get_cost(self) -> float:
        return self._coffee.get_cost() + 0.45


class ExtraShot(CoffeeDecorator):
    """Add extra espresso shot."""
    
    def get_description(self) -> str:
        return f"{self._coffee.get_description()}, Extra Shot"
    
    def get_cost(self) -> float:
        return self._coffee.get_cost() + 0.75


# ============== ORDER HELPER ==============

def print_order(coffee: Coffee):
    """Print a coffee order."""
    print(f"☕ {coffee.get_description()}")
    print(f"   ${coffee.get_cost():.2f}")


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Decorator Pattern - Coffee Shop Example")
    print("=" * 60)
    
    # Simple espresso
    print("\n--- Order 1: Simple Espresso ---")
    coffee1 = Espresso()
    print_order(coffee1)
    
    # Dark roast with double mocha and whip
    print("\n--- Order 2: Dark Roast with Double Mocha and Whip ---")
    coffee2 = DarkRoast()
    coffee2 = Mocha(coffee2)
    coffee2 = Mocha(coffee2)  # Double mocha!
    coffee2 = Whip(coffee2)
    print_order(coffee2)
    
    # House blend with soy, mocha, and whip
    print("\n--- Order 3: House Blend Soy Mocha with Whip ---")
    coffee3 = HouseBlend()
    coffee3 = Soy(coffee3)
    coffee3 = Mocha(coffee3)
    coffee3 = Whip(coffee3)
    print_order(coffee3)
    
    # Espresso with extra shot and milk
    print("\n--- Order 4: Espresso with Extra Shot and Milk ---")
    coffee4 = Milk(ExtraShot(Espresso()))  # Inline decoration
    print_order(coffee4)
    
    # The ultimate coffee
    print("\n--- Order 5: The Ultimate Coffee ---")
    ultimate = Espresso()
    for decorator in [ExtraShot, ExtraShot, Mocha, Mocha, Soy, Whip]:
        ultimate = decorator(ultimate)
    print_order(ultimate)
```

---

### Example 3: Class Decorators (@decorator on classes)

```python
#!/usr/bin/env python3
"""
Decorator Pattern - Class Decorators

Decorators that modify class definitions.
Similar to @dataclass but custom behavior.
"""

import functools
import time
from typing import Type, TypeVar
from dataclasses import dataclass


T = TypeVar("T")


# ============== CLASS DECORATORS ==============

def singleton(cls: Type[T]) -> Type[T]:
    """
    Class decorator that makes a class a singleton.
    
    Only one instance of the class will ever exist.
    """
    instances = {}
    
    @functools.wraps(cls)
    def get_instance(*args, **kwargs):
        if cls not in instances:
            instances[cls] = cls(*args, **kwargs)
        return instances[cls]
    
    return get_instance


def auto_repr(cls: Type[T]) -> Type[T]:
    """
    Class decorator that adds automatic __repr__.
    
    Similar to what @dataclass provides.
    """
    original_init = cls.__init__
    
    @functools.wraps(original_init)
    def new_init(self, *args, **kwargs):
        original_init(self, *args, **kwargs)
        # Store init args for repr
        self._init_args = args
        self._init_kwargs = kwargs
    
    def new_repr(self):
        class_name = self.__class__.__name__
        args_str = ", ".join(repr(a) for a in self._init_args)
        kwargs_str = ", ".join(f"{k}={v!r}" for k, v in self._init_kwargs.items())
        all_args = ", ".join(filter(None, [args_str, kwargs_str]))
        return f"{class_name}({all_args})"
    
    cls.__init__ = new_init
    cls.__repr__ = new_repr
    return cls


def timer_all_methods(cls: Type[T]) -> Type[T]:
    """
    Class decorator that times all public methods.
    """
    for name, method in vars(cls).items():
        if callable(method) and not name.startswith('_'):
            # Wrap the method
            @functools.wraps(method)
            def timed_method(self, *args, _method=method, _name=name, **kwargs):
                start = time.perf_counter()
                result = _method(self, *args, **kwargs)
                elapsed = time.perf_counter() - start
                print(f"⏱️ {cls.__name__}.{_name} took {elapsed:.4f}s")
                return result
            
            setattr(cls, name, timed_method)
    
    return cls


def immutable(cls: Type[T]) -> Type[T]:
    """
    Class decorator that makes instances immutable after __init__.
    """
    original_init = cls.__init__
    
    @functools.wraps(original_init)
    def new_init(self, *args, **kwargs):
        object.__setattr__(self, '_initialized', False)
        original_init(self, *args, **kwargs)
        object.__setattr__(self, '_initialized', True)
    
    def new_setattr(self, name, value):
        if getattr(self, '_initialized', False):
            raise AttributeError(f"Cannot modify immutable object: {name}")
        object.__setattr__(self, name, value)
    
    def new_delattr(self, name):
        if getattr(self, '_initialized', False):
            raise AttributeError(f"Cannot delete from immutable object: {name}")
        object.__delattr__(self, name)
    
    cls.__init__ = new_init
    cls.__setattr__ = new_setattr
    cls.__delattr__ = new_delattr
    return cls


# ============== DECORATED CLASSES ==============

@singleton
class Configuration:
    """Singleton configuration class."""
    
    def __init__(self):
        self.settings = {"debug": False}
        print("Configuration created!")


@auto_repr
class Point:
    """Point with auto-generated repr."""
    
    def __init__(self, x, y, label=None):
        self.x = x
        self.y = y
        self.label = label


@timer_all_methods
class Calculator:
    """Calculator with timed methods."""
    
    def add(self, a, b):
        time.sleep(0.01)
        return a + b
    
    def multiply(self, a, b):
        time.sleep(0.02)
        return a * b
    
    def power(self, base, exp):
        time.sleep(0.03)
        return base ** exp


@immutable
class ImmutablePoint:
    """Immutable point class."""
    
    def __init__(self, x, y):
        self.x = x
        self.y = y


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Decorator Pattern - Class Decorators")
    print("=" * 60)
    
    # Singleton
    print("\n--- Singleton Decorator ---")
    config1 = Configuration()
    config2 = Configuration()  # No "created!" message
    print(f"config1 is config2: {config1 is config2}")
    
    # Auto repr
    print("\n--- Auto Repr Decorator ---")
    p1 = Point(10, 20)
    p2 = Point(5, 15, label="origin")
    print(f"p1: {p1}")
    print(f"p2: {p2}")
    
    # Timer all methods
    print("\n--- Timer All Methods Decorator ---")
    calc = Calculator()
    calc.add(1, 2)
    calc.multiply(3, 4)
    calc.power(2, 10)
    
    # Immutable
    print("\n--- Immutable Decorator ---")
    point = ImmutablePoint(100, 200)
    print(f"point.x = {point.x}")
    try:
        point.x = 999  # Should raise
    except AttributeError as e:
        print(f"Error: {e}")
```

---

### Example 4: Stream Processing Decorators

```python
#!/usr/bin/env python3
"""
Decorator Pattern - Stream Processing

Decorators for data streams/pipelines.
Similar to how io.TextIOWrapper decorates BytesIO.
"""

from abc import ABC, abstractmethod
from typing import Iterator, Any
import gzip
import base64


# ============== DATA STREAM INTERFACE ==============

class DataStream(ABC):
    """Abstract data stream interface."""
    
    @abstractmethod
    def read(self) -> bytes:
        """Read all data."""
        pass
    
    @abstractmethod
    def write(self, data: bytes) -> int:
        """Write data, return bytes written."""
        pass


# ============== CONCRETE STREAM ==============

class MemoryStream(DataStream):
    """In-memory data stream."""
    
    def __init__(self, initial_data: bytes = b""):
        self._buffer = bytearray(initial_data)
    
    def read(self) -> bytes:
        return bytes(self._buffer)
    
    def write(self, data: bytes) -> int:
        self._buffer.extend(data)
        return len(data)


# ============== STREAM DECORATORS ==============

class StreamDecorator(DataStream):
    """Base decorator for streams."""
    
    def __init__(self, wrapped: DataStream):
        self._wrapped = wrapped
    
    def read(self) -> bytes:
        return self._wrapped.read()
    
    def write(self, data: bytes) -> int:
        return self._wrapped.write(data)


class Base64Stream(StreamDecorator):
    """Base64 encoding/decoding decorator."""
    
    def read(self) -> bytes:
        """Read and decode from base64."""
        encoded = self._wrapped.read()
        if encoded:
            return base64.b64decode(encoded)
        return b""
    
    def write(self, data: bytes) -> int:
        """Encode to base64 and write."""
        encoded = base64.b64encode(data)
        return self._wrapped.write(encoded)


class CompressionStream(StreamDecorator):
    """Gzip compression decorator."""
    
    def read(self) -> bytes:
        """Read and decompress."""
        compressed = self._wrapped.read()
        if compressed:
            return gzip.decompress(compressed)
        return b""
    
    def write(self, data: bytes) -> int:
        """Compress and write."""
        compressed = gzip.compress(data)
        return self._wrapped.write(compressed)


class EncryptionStream(StreamDecorator):
    """Simple XOR encryption decorator (for demo only!)."""
    
    def __init__(self, wrapped: DataStream, key: int = 42):
        super().__init__(wrapped)
        self._key = key
    
    def _xor(self, data: bytes) -> bytes:
        """XOR each byte with key."""
        return bytes(b ^ self._key for b in data)
    
    def read(self) -> bytes:
        """Read and decrypt."""
        encrypted = self._wrapped.read()
        return self._xor(encrypted)
    
    def write(self, data: bytes) -> int:
        """Encrypt and write."""
        encrypted = self._xor(data)
        return self._wrapped.write(encrypted)


class LoggingStream(StreamDecorator):
    """Logging decorator for debugging."""
    
    def __init__(self, wrapped: DataStream, name: str = "stream"):
        super().__init__(wrapped)
        self._name = name
    
    def read(self) -> bytes:
        data = self._wrapped.read()
        print(f"[{self._name}] READ: {len(data)} bytes")
        return data
    
    def write(self, data: bytes) -> int:
        print(f"[{self._name}] WRITE: {len(data)} bytes")
        return self._wrapped.write(data)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Decorator Pattern - Stream Processing")
    print("=" * 60)
    
    original_data = b"Hello, World! This is a test of the decorator pattern."
    
    # Simple memory stream
    print("\n--- Simple Memory Stream ---")
    stream = MemoryStream()
    stream.write(original_data)
    print(f"Written: {stream.read()}")
    
    # Base64 encoded stream
    print("\n--- Base64 Encoded Stream ---")
    stream = Base64Stream(MemoryStream())
    stream.write(original_data)
    raw = stream._wrapped.read()  # Get underlying data
    print(f"Raw (base64): {raw[:50]}...")
    print(f"Decoded: {stream.read()}")
    
    # Compressed stream
    print("\n--- Compressed Stream ---")
    stream = CompressionStream(MemoryStream())
    stream.write(original_data * 10)  # More data for better compression
    raw = stream._wrapped.read()
    print(f"Original size: {len(original_data) * 10}")
    print(f"Compressed size: {len(raw)}")
    
    # Stacked decorators: Encrypt -> Compress -> Base64 -> Memory
    print("\n--- Stacked Decorators ---")
    stream = EncryptionStream(
        CompressionStream(
            Base64Stream(
                LoggingStream(
                    MemoryStream(),
                    name="base"
                )
            )
        ),
        key=123
    )
    
    print("Writing data through stack...")
    stream.write(original_data)
    
    print("\nReading data through stack...")
    result = stream.read()
    print(f"Result: {result}")
    print(f"Matches original: {result == original_data}")
```

---

## 6. When to Use / When to Avoid

### Use When:
- Add responsibilities to objects **dynamically**
- Extension by **subclassing is impractical**
- Need to add/remove features at **runtime**
- Want to follow **Open/Closed Principle**

### Avoid When:
- Simple behavior change (just modify the class)
- Order of decorators creates confusion
- Too many small decorators make code hard to debug

### Python Idiom:
Prefer `@decorator` syntax for function decoration:

```python
# Pythonic
@log
@timer
@cache
def my_function():
    pass

# Rather than
my_function = log(timer(cache(my_function)))
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Adapter** | Changes interface; Decorator enhances without changing |
| **Proxy** | Controls access; Decorator adds behavior |
| **Composite** | Decorator is a degenerate Composite with one component |
| **Strategy** | Changes guts; Decorator changes skin |
| **Chain of Responsibility** | Similar chaining but different purpose |

