# Adapter Pattern in Python

---

## 1. Pattern Name: Adapter (Wrapper)

**Purpose / Problem Solved:**
Convert the interface of a class into another interface that clients expect. Allows classes with incompatible interfaces to work together. Acts as a bridge between legacy and new code.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT CODE                               |
|------------------------------------------------------------------|
| Uses TargetInterface                                              |
| client.do_something(target)                                       |
+------------------------------------------------------------------+
                              |
                              | Expects TargetInterface
                              v
+------------------------------------------------------------------+
|                     TARGET INTERFACE                              |
|------------------------------------------------------------------|
| + request() -> Result                                             |
+------------------------------------------------------------------+
                              ^
                              |
              +---------------+----------------+
              |                                |
+-------------------------+      +-------------------------+
|    ConcreteTarget       |      |       ADAPTER           |
|-------------------------|      |-------------------------|
| + request() -> Result   |      | - adaptee: Adaptee      |
+-------------------------+      |-------------------------|
                                 | + request() -> Result   |
                                 |     adaptee.specific_request() |
                                 |     + transform result  |
                                 +-------------------------+
                                              |
                                              | Wraps
                                              v
                                 +-------------------------+
                                 |        ADAPTEE          |
                                 |-------------------------|
                                 | + specific_request()    |
                                 | + other_method()        |
                                 | (incompatible interface)|
                                 +-------------------------+

Object Adapter (Composition):
+-------------+         +-------------+
|   Adapter   |-------->|   Adaptee   |
+-------------+         +-------------+
| -adaptee    |         | +method_a() |
| +request()  |         | +method_b() |
+-------------+         +-------------+

Class Adapter (Multiple Inheritance):
+-------------+
|   Adapter   |
+------+------+
       |
   +---+---+
   |       |
+--+--+ +--+--+
|Target| |Adaptee|
+-----+ +------+
```

**中文说明：**
适配器模式将一个类的接口转换为客户端期望的另一种接口。当你想使用一个现有的类，但其接口与你需要的不兼容时，使用适配器。Python有两种实现方式：对象适配器（组合方式，持有被适配对象的引用）和类适配器（多重继承）。对象适配器更灵活，也是Python中更常用的方式。适配器常用于：集成第三方库、包装遗留代码、统一不同API。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Adapter |
|----------------|-------------------------|
| **Duck typing** | Adapter just needs same method names, no formal interface |
| **Multiple inheritance** | Class adapter via inheriting both Target and Adaptee |
| **Composition** | Object adapter by holding Adaptee reference |
| **`__getattr__`** | Dynamically delegate unknown attributes to adaptee |
| **Protocol classes** | Type-safe interface definition without inheritance |
| **`functools.wraps`** | Preserve metadata when wrapping functions |
| **`*args, **kwargs`** | Forward all arguments to adaptee |
| **Property** | Adapt getter/setter interfaces |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **requests** | Adapts various HTTP libraries to unified interface |
| **SQLAlchemy** | Database dialects adapt different DB APIs |
| **Django ORM** | Adapts database backends to common interface |
| **Python io module** | `TextIOWrapper` adapts bytes to text interface |
| **csv module** | Adapts file objects to row-based interface |
| **json module** | Adapts file-like objects for JSON operations |

```python
# io module - TextIOWrapper adapts binary to text
import io

binary_stream = io.BytesIO(b"Hello World")
text_stream = io.TextIOWrapper(binary_stream, encoding='utf-8')
print(text_stream.read())  # Works with text interface
```

---

## 5. Python Module Examples

### Example 1: Object Adapter (Composition)

```python
#!/usr/bin/env python3
"""
Adapter Pattern - Object Adapter (Composition)

Wraps an existing class to provide a new interface.
Uses composition to delegate calls to the adaptee.
"""

from abc import ABC, abstractmethod
from typing import Protocol


# ============== TARGET INTERFACE ==============

class PaymentProcessor(Protocol):
    """
    Target interface that client code expects.
    
    All payment processors should have this interface.
    """
    
    def pay(self, amount: float) -> dict:
        """Process a payment and return result."""
        ...
    
    def refund(self, transaction_id: str, amount: float) -> dict:
        """Process a refund and return result."""
        ...


# ============== ADAPTEE (Legacy/Third-Party) ==============

class LegacyPaymentGateway:
    """
    Legacy payment system with incompatible interface.
    
    We can't modify this (third-party or legacy code).
    """
    
    def make_payment(self, cents: int, currency: str = "USD") -> str:
        """Process payment in cents, returns transaction ID."""
        print(f"Legacy: Processing {cents} cents ({currency})")
        return f"TXN-{cents}-{id(self)}"
    
    def process_refund(self, txn_id: str, cents: int) -> bool:
        """Process refund in cents, returns success boolean."""
        print(f"Legacy: Refunding {cents} cents for {txn_id}")
        return True
    
    def get_transaction_status(self, txn_id: str) -> str:
        """Get transaction status."""
        return "COMPLETED"


# ============== ADAPTER ==============

class LegacyPaymentAdapter:
    """
    Adapter that wraps LegacyPaymentGateway.
    
    Converts the modern interface (dollars, dict) to 
    legacy interface (cents, string/bool).
    """
    
    def __init__(self, gateway: LegacyPaymentGateway):
        self._gateway = gateway
    
    def pay(self, amount: float) -> dict:
        """
        Adapt pay() to legacy make_payment().
        
        Converts dollars to cents and wraps result in dict.
        """
        cents = int(amount * 100)
        txn_id = self._gateway.make_payment(cents)
        
        return {
            "success": True,
            "transaction_id": txn_id,
            "amount": amount,
            "status": self._gateway.get_transaction_status(txn_id)
        }
    
    def refund(self, transaction_id: str, amount: float) -> dict:
        """
        Adapt refund() to legacy process_refund().
        
        Converts dollars to cents and wraps result in dict.
        """
        cents = int(amount * 100)
        success = self._gateway.process_refund(transaction_id, cents)
        
        return {
            "success": success,
            "transaction_id": transaction_id,
            "refunded_amount": amount if success else 0
        }


# ============== ANOTHER ADAPTEE ==============

class StripeAPI:
    """
    Modern Stripe-like API with different interface.
    
    Another third-party library to adapt.
    """
    
    def create_charge(self, amount_cents: int, currency: str, 
                     metadata: dict = None) -> dict:
        """Create a charge."""
        return {
            "id": f"ch_{id(self)}",
            "amount": amount_cents,
            "currency": currency,
            "status": "succeeded"
        }
    
    def create_refund(self, charge_id: str, amount_cents: int = None) -> dict:
        """Create a refund for a charge."""
        return {
            "id": f"re_{id(self)}",
            "charge": charge_id,
            "amount": amount_cents,
            "status": "succeeded"
        }


class StripeAdapter:
    """Adapter for Stripe API."""
    
    def __init__(self, api: StripeAPI):
        self._api = api
    
    def pay(self, amount: float) -> dict:
        cents = int(amount * 100)
        result = self._api.create_charge(cents, "USD")
        
        return {
            "success": result["status"] == "succeeded",
            "transaction_id": result["id"],
            "amount": amount,
            "status": result["status"]
        }
    
    def refund(self, transaction_id: str, amount: float) -> dict:
        cents = int(amount * 100)
        result = self._api.create_refund(transaction_id, cents)
        
        return {
            "success": result["status"] == "succeeded",
            "transaction_id": result["id"],
            "refunded_amount": amount
        }


# ============== CLIENT CODE ==============

def process_order(processor: PaymentProcessor, total: float):
    """
    Client code that works with any PaymentProcessor.
    
    Doesn't know or care about the underlying implementation.
    """
    print(f"\n--- Processing order for ${total:.2f} ---")
    
    # Process payment
    result = processor.pay(total)
    
    if result["success"]:
        print(f"Payment successful: {result['transaction_id']}")
        
        # Simulate partial refund
        refund_result = processor.refund(
            result["transaction_id"], 
            total * 0.1  # 10% refund
        )
        print(f"Refund processed: ${refund_result['refunded_amount']:.2f}")
    else:
        print("Payment failed!")
    
    return result


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Adapter Pattern - Object Adapter")
    print("=" * 60)
    
    # Use legacy gateway through adapter
    print("\n=== Using Legacy Payment Gateway ===")
    legacy = LegacyPaymentGateway()
    legacy_adapter = LegacyPaymentAdapter(legacy)
    process_order(legacy_adapter, 99.99)
    
    # Use Stripe API through adapter
    print("\n=== Using Stripe API ===")
    stripe = StripeAPI()
    stripe_adapter = StripeAdapter(stripe)
    process_order(stripe_adapter, 149.99)
    
    # Both adapters work interchangeably
    print("\n=== Switching Payment Processors ===")
    processors = [
        ("Legacy", LegacyPaymentAdapter(LegacyPaymentGateway())),
        ("Stripe", StripeAdapter(StripeAPI())),
    ]
    
    for name, processor in processors:
        print(f"\nUsing {name}:")
        result = processor.pay(50.00)
        print(f"  Result: {result}")
```

---

### Example 2: Class Adapter (Multiple Inheritance)

```python
#!/usr/bin/env python3
"""
Adapter Pattern - Class Adapter (Multiple Inheritance)

Uses multiple inheritance to adapt interfaces.
The adapter IS-A Target and IS-A Adaptee.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass


# ============== TARGET INTERFACE ==============

class MediaPlayer(ABC):
    """Target interface for media playback."""
    
    @abstractmethod
    def play(self, filename: str) -> str:
        pass
    
    @abstractmethod
    def stop(self) -> str:
        pass
    
    @abstractmethod
    def get_duration(self) -> float:
        pass


# ============== ADAPTEES ==============

class VLCLibrary:
    """
    VLC media library with its own interface.
    
    Uses different method names and parameters.
    """
    
    def __init__(self):
        self._current_file = None
        self._playing = False
    
    def vlc_load(self, path: str):
        """Load a media file."""
        self._current_file = path
        print(f"VLC: Loaded {path}")
    
    def vlc_play(self):
        """Start playback."""
        self._playing = True
        return f"VLC: Playing {self._current_file}"
    
    def vlc_halt(self):
        """Stop playback."""
        self._playing = False
        return "VLC: Stopped"
    
    def vlc_get_length(self) -> int:
        """Get length in milliseconds."""
        return 180000  # 3 minutes


class FFmpegWrapper:
    """
    FFmpeg wrapper with command-based interface.
    
    Very different interface from what we need.
    """
    
    def __init__(self):
        self._current = None
    
    def ffmpeg_open(self, input_file: str) -> bool:
        """Open an input file."""
        self._current = input_file
        print(f"FFmpeg: Opened {input_file}")
        return True
    
    def ffmpeg_decode_and_play(self) -> str:
        """Decode and start playback."""
        return f"FFmpeg: Decoding and playing {self._current}"
    
    def ffmpeg_close(self):
        """Close the current file."""
        self._current = None
        return "FFmpeg: Closed"
    
    def ffmpeg_probe_duration(self) -> dict:
        """Get media information."""
        return {"duration": "00:03:45", "seconds": 225.0}


# ============== CLASS ADAPTERS ==============

class VLCAdapter(MediaPlayer, VLCLibrary):
    """
    Class adapter using multiple inheritance.
    
    Inherits interface from MediaPlayer and 
    implementation from VLCLibrary.
    """
    
    def __init__(self):
        VLCLibrary.__init__(self)
    
    def play(self, filename: str) -> str:
        """Adapt play() to VLC methods."""
        self.vlc_load(filename)
        return self.vlc_play()
    
    def stop(self) -> str:
        """Adapt stop() to vlc_halt()."""
        return self.vlc_halt()
    
    def get_duration(self) -> float:
        """Adapt get_duration() to vlc_get_length()."""
        ms = self.vlc_get_length()
        return ms / 1000.0  # Convert to seconds


class FFmpegAdapter(MediaPlayer, FFmpegWrapper):
    """
    Class adapter for FFmpeg.
    
    Inherits both interfaces and provides translation.
    """
    
    def __init__(self):
        FFmpegWrapper.__init__(self)
    
    def play(self, filename: str) -> str:
        """Adapt play() to FFmpeg methods."""
        self.ffmpeg_open(filename)
        return self.ffmpeg_decode_and_play()
    
    def stop(self) -> str:
        """Adapt stop() to ffmpeg_close()."""
        return self.ffmpeg_close()
    
    def get_duration(self) -> float:
        """Adapt get_duration() to ffmpeg_probe_duration()."""
        info = self.ffmpeg_probe_duration()
        return info["seconds"]


# ============== CLIENT CODE ==============

def play_media(player: MediaPlayer, filename: str):
    """Client that uses MediaPlayer interface."""
    print(f"\n--- Playing: {filename} ---")
    
    duration = player.get_duration()
    print(f"Duration: {duration:.1f} seconds")
    
    result = player.play(filename)
    print(result)
    
    # Simulate some playback time...
    
    stop_result = player.stop()
    print(stop_result)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Adapter Pattern - Class Adapter (Multiple Inheritance)")
    print("=" * 60)
    
    # Use VLC through adapter
    print("\n=== VLC Adapter ===")
    vlc_player = VLCAdapter()
    play_media(vlc_player, "song.mp3")
    
    # Use FFmpeg through adapter
    print("\n=== FFmpeg Adapter ===")
    ffmpeg_player = FFmpegAdapter()
    play_media(ffmpeg_player, "video.mkv")
    
    # Both work identically from client's perspective
    print("\n=== Polymorphic Usage ===")
    players = [VLCAdapter(), FFmpegAdapter()]
    for player in players:
        print(f"Player type: {type(player).__name__}")
        print(f"  Duration: {player.get_duration()} seconds")
```

---

### Example 3: Dynamic Adapter with __getattr__

```python
#!/usr/bin/env python3
"""
Adapter Pattern - Dynamic Adapter using __getattr__

Dynamically delegates unknown methods to the adaptee.
Useful when you want to expose most of the adaptee's interface.
"""

from typing import Any


class OldAPI:
    """Legacy API with many methods."""
    
    def get_user_data(self, user_id: int) -> dict:
        return {"id": user_id, "name": f"User{user_id}", "active": True}
    
    def update_user_data(self, user_id: int, data: dict) -> bool:
        print(f"Updating user {user_id} with {data}")
        return True
    
    def delete_user_data(self, user_id: int) -> bool:
        print(f"Deleting user {user_id}")
        return True
    
    def list_all_users(self) -> list:
        return [{"id": i, "name": f"User{i}"} for i in range(1, 4)]
    
    def get_user_permissions(self, user_id: int) -> list:
        return ["read", "write"]


class ModernAPIAdapter:
    """
    Adapter that provides modern interface while delegating 
    to legacy API for unknown methods.
    
    Uses __getattr__ for dynamic delegation.
    """
    
    def __init__(self, old_api: OldAPI):
        self._api = old_api
    
    # New, modern interface
    def get_user(self, user_id: int) -> dict:
        """Modern interface with camelCase-style naming."""
        data = self._api.get_user_data(user_id)
        # Add extra processing/transformation
        return {
            "userId": data["id"],
            "username": data["name"],
            "isActive": data["active"],
            "permissions": self._api.get_user_permissions(user_id)
        }
    
    def update_user(self, user_id: int, **updates) -> bool:
        """Modern update with kwargs."""
        return self._api.update_user_data(user_id, updates)
    
    def delete_user(self, user_id: int) -> bool:
        """Renamed method."""
        return self._api.delete_user_data(user_id)
    
    def __getattr__(self, name: str) -> Any:
        """
        Delegate unknown methods to the old API.
        
        This allows using any method from OldAPI that 
        we haven't explicitly overridden.
        """
        return getattr(self._api, name)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Adapter Pattern - Dynamic __getattr__ Delegation")
    print("=" * 60)
    
    old = OldAPI()
    adapter = ModernAPIAdapter(old)
    
    # Use new modern interface
    print("\n--- Modern Interface ---")
    user = adapter.get_user(1)
    print(f"Get user: {user}")
    
    adapter.update_user(1, name="Updated", email="new@example.com")
    
    # Use delegated legacy methods (no adapter method defined)
    print("\n--- Delegated Legacy Methods ---")
    users = adapter.list_all_users()  # Delegated to old API
    print(f"All users: {users}")
    
    perms = adapter.get_user_permissions(1)  # Delegated
    print(f"Permissions: {perms}")
```

---

### Example 4: Function Adapter

```python
#!/usr/bin/env python3
"""
Adapter Pattern - Function Adapters

Adapts functions with different signatures.
Common in callback-based APIs and event handlers.
"""

from typing import Callable, Any
from functools import wraps


# ============== FUNCTION ADAPTERS ==============

def adapt_callback(
    old_style: Callable[[str, int], Any]
) -> Callable[[dict], Any]:
    """
    Adapt old callback (str, int) to new callback (dict).
    
    Args:
        old_style: Function taking (message: str, code: int)
    
    Returns:
        Function taking (result: dict)
    """
    @wraps(old_style)
    def wrapper(result: dict) -> Any:
        message = result.get("message", "")
        code = result.get("code", 0)
        return old_style(message, code)
    
    return wrapper


def adapt_args(
    *transformers: Callable
) -> Callable:
    """
    Decorator to transform function arguments.
    
    Each transformer is applied to the corresponding argument.
    """
    def decorator(func: Callable) -> Callable:
        @wraps(func)
        def wrapper(*args, **kwargs):
            adapted_args = [
                t(a) if t else a 
                for a, t in zip(args, transformers + (None,) * len(args))
            ]
            return func(*adapted_args, **kwargs)
        return wrapper
    return decorator


def adapt_return(
    transformer: Callable
) -> Callable:
    """
    Decorator to transform function return value.
    """
    def decorator(func: Callable) -> Callable:
        @wraps(func)
        def wrapper(*args, **kwargs):
            result = func(*args, **kwargs)
            return transformer(result)
        return wrapper
    return decorator


# ============== EXAMPLE FUNCTIONS ==============

def old_handler(message: str, error_code: int):
    """Old-style handler with positional args."""
    print(f"Old handler: {message} (code: {error_code})")


def new_system_callback(result: dict):
    """New system expects dict-based callbacks."""
    pass  # This is the expected signature


# Adapt old handler to new signature
adapted_handler = adapt_callback(old_handler)


# ============== DECORATOR ADAPTERS ==============

@adapt_args(str.upper, int)
def process_data(name: str, count: int):
    """Function with adapted arguments."""
    print(f"Processing {name} x{count}")
    return f"Processed {name}"


@adapt_return(str.upper)
def get_status() -> str:
    """Function with adapted return value."""
    return "active"


# ============== CLASS FOR SIGNATURE ADAPTATION ==============

class CallbackAdapter:
    """
    Adapter for callback functions with different signatures.
    
    Stores the original callback and adapts calls to it.
    """
    
    def __init__(self, callback: Callable, adapter: Callable = None):
        self._callback = callback
        self._adapter = adapter or (lambda x: x)
    
    def __call__(self, *args, **kwargs):
        """Call the adapted callback."""
        adapted = self._adapter(*args, **kwargs)
        if isinstance(adapted, tuple):
            return self._callback(*adapted)
        return self._callback(adapted)
    
    @classmethod
    def from_positional_to_dict(cls, callback: Callable, keys: list):
        """Create adapter from positional args to dict."""
        def adapter(*args):
            return ({k: v for k, v in zip(keys, args)},)
        return cls(callback, adapter)
    
    @classmethod
    def from_dict_to_positional(cls, callback: Callable, keys: list):
        """Create adapter from dict to positional args."""
        def adapter(d: dict):
            return tuple(d.get(k) for k in keys)
        return cls(callback, adapter)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Adapter Pattern - Function Adapters")
    print("=" * 60)
    
    # Callback adaptation
    print("\n--- Callback Adaptation ---")
    print("New system calling old handler via adapter:")
    adapted_handler({"message": "Error occurred", "code": 500})
    
    # Argument adaptation
    print("\n--- Argument Adaptation ---")
    process_data("hello", "42")  # String args adapted
    
    # Return value adaptation  
    print("\n--- Return Value Adaptation ---")
    status = get_status()
    print(f"Status: {status}")
    
    # CallbackAdapter class
    print("\n--- CallbackAdapter Class ---")
    
    def positional_callback(a, b, c):
        print(f"Positional: a={a}, b={b}, c={c}")
    
    def dict_callback(data: dict):
        print(f"Dict: {data}")
    
    # Adapt positional -> dict
    adapted = CallbackAdapter.from_positional_to_dict(
        dict_callback, 
        ["x", "y", "z"]
    )
    adapted(1, 2, 3)  # Calls dict_callback({"x": 1, "y": 2, "z": 3})
    
    # Adapt dict -> positional
    adapted2 = CallbackAdapter.from_dict_to_positional(
        positional_callback,
        ["x", "y", "z"]
    )
    adapted2({"x": 10, "y": 20, "z": 30})
```

---

## 6. When to Use / When to Avoid

### Use When:
- You want to use an existing class with **incompatible interface**
- You need to **integrate third-party libraries** with your code
- You're working with **legacy code** that can't be modified
- You want to create **reusable class** that works with unrelated classes

### Avoid When:
- Interfaces are already compatible (don't over-engineer)
- You can modify the original class directly
- A simpler solution (like a wrapper function) suffices

### Python Idiom:
For simple cases, use a **wrapper function** instead of a class:

```python
# Instead of Adapter class, sometimes this is enough:
def adapted_function(new_args):
    old_result = legacy_function(transform(new_args))
    return modernize(old_result)
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Bridge** | Similar structure but different intent: Bridge separates abstraction from implementation |
| **Decorator** | Adds behavior without changing interface; Adapter changes interface |
| **Proxy** | Same interface, controls access; Adapter different interface |
| **Facade** | Defines new interface for subsystem; Adapter makes existing interface usable |

