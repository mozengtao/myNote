# Observer Pattern in Python

---

## 1. Pattern Name: Observer (Publish-Subscribe, Event)

**Purpose / Problem Solved:**
Define a one-to-many dependency between objects so that when one object changes state, all its dependents are notified and updated automatically. Enables loose coupling between subjects and observers.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         SUBJECT                                   |
|------------------------------------------------------------------|
| - observers: List[Observer]                                       |
| - state: State                                                    |
|------------------------------------------------------------------|
| + attach(observer: Observer)                                      |
| + detach(observer: Observer)                                      |
| + notify()                                                        |
|     for observer in observers:                                    |
|         observer.update(self)                                     |
+------------------------------------------------------------------+
                              |
                              | Notifies (one-to-many)
                              v
        +--------------------+--------------------+
        |                    |                    |
+---------------+    +---------------+    +---------------+
|  Observer A   |    |  Observer B   |    |  Observer C   |
|---------------|    |---------------|    |---------------|
| + update(subj)|    | + update(subj)|    | + update(subj)|
| # react to    |    | # react to    |    | # react to    |
| # state change|    | # state change|    | # state change|
+---------------+    +---------------+    +---------------+

EVENT FLOW:
+----------+     +----------+     +----------+
| Subject  | --> | Observer | --> |  Action  |
| changes  |     | notified |     | executed |
+----------+     +----------+     +----------+
     |                |                |
     v                v                v
  state=X         update()        [do something]
     |                |                |
     +----------------+----------------+
                      |
                 [Repeat for all observers]
```

**中文说明：**
观察者模式建立对象间的一对多依赖。当Subject状态改变时，所有注册的Observer自动收到通知并更新。Subject不需要知道Observer的具体类型，只需调用update方法。这实现了松耦合——Subject和Observer可以独立变化。Python中常用回调函数或信号(signals)实现。应用场景包括：GUI事件处理、数据绑定、消息队列、实时通知系统等。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Observer |
|----------------|--------------------------|
| **First-class functions** | Callbacks as observers |
| **`weakref`** | Prevent memory leaks with weak references |
| **`abc.ABC`** | Define Observer interface |
| **`*args` in callbacks** | Flexible update signatures |
| **`__call__`** | Make observer objects callable |
| **Decorators** | `@event.on("name")` subscription syntax |
| **Context managers** | Temporary subscriptions |
| **asyncio** | Async observers for I/O-bound reactions |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Django signals** | `post_save.connect(callback)` |
| **Blinker** | Flask's signal system |
| **RxPY** | Reactive Extensions for Python |
| **PyQt/PySide** | Qt's signal-slot mechanism |
| **Tkinter** | Event binding with `widget.bind()` |
| **asyncio** | Event loops and callbacks |

```python
# Django signals example
from django.db.models.signals import post_save
from django.dispatch import receiver
from .models import User

@receiver(post_save, sender=User)
def create_profile(sender, instance, created, **kwargs):
    if created:
        Profile.objects.create(user=instance)
```

---

## 5. Python Module Examples

### Example 1: Classic Observer with Classes

```python
#!/usr/bin/env python3
"""
Observer Pattern - Classic Implementation

Traditional OOP approach with Subject and Observer base classes.
"""

from abc import ABC, abstractmethod
from typing import List, Any
from dataclasses import dataclass, field


# ============== OBSERVER INTERFACE ==============

class Observer(ABC):
    """Abstract base class for observers."""
    
    @abstractmethod
    def update(self, subject: "Subject", *args, **kwargs) -> None:
        """Called when subject state changes."""
        pass


# ============== SUBJECT BASE ==============

class Subject(ABC):
    """
    Subject base class.
    
    Manages observer registration and notification.
    """
    
    def __init__(self):
        self._observers: List[Observer] = []
    
    def attach(self, observer: Observer) -> None:
        """Attach an observer."""
        if observer not in self._observers:
            self._observers.append(observer)
            print(f"✓ Attached {observer.__class__.__name__}")
    
    def detach(self, observer: Observer) -> None:
        """Detach an observer."""
        try:
            self._observers.remove(observer)
            print(f"✗ Detached {observer.__class__.__name__}")
        except ValueError:
            pass
    
    def notify(self, *args, **kwargs) -> None:
        """Notify all observers."""
        for observer in self._observers:
            observer.update(self, *args, **kwargs)


# ============== CONCRETE SUBJECT ==============

class StockMarket(Subject):
    """
    Concrete subject: Stock market price tracker.
    """
    
    def __init__(self):
        super().__init__()
        self._stocks: dict[str, float] = {}
    
    def set_price(self, symbol: str, price: float) -> None:
        """Update stock price and notify observers."""
        old_price = self._stocks.get(symbol, 0)
        self._stocks[symbol] = price
        
        change = price - old_price
        change_pct = (change / old_price * 100) if old_price else 0
        
        print(f"\n📈 {symbol}: ${price:.2f} ({change:+.2f}, {change_pct:+.1f}%)")
        
        # Notify observers with event data
        self.notify(
            symbol=symbol, 
            price=price, 
            change=change,
            change_pct=change_pct
        )
    
    def get_price(self, symbol: str) -> float:
        return self._stocks.get(symbol, 0)


# ============== CONCRETE OBSERVERS ==============

class PriceDisplay(Observer):
    """Observer that displays price updates."""
    
    def __init__(self, name: str):
        self.name = name
    
    def update(self, subject: Subject, **kwargs) -> None:
        symbol = kwargs.get("symbol")
        price = kwargs.get("price")
        print(f"  [{self.name}] {symbol} is now ${price:.2f}")


class PriceAlert(Observer):
    """Observer that alerts on price thresholds."""
    
    def __init__(self, symbol: str, low: float, high: float):
        self.symbol = symbol
        self.low = low
        self.high = high
    
    def update(self, subject: Subject, **kwargs) -> None:
        symbol = kwargs.get("symbol")
        price = kwargs.get("price")
        
        if symbol != self.symbol:
            return
        
        if price <= self.low:
            print(f"  🔔 ALERT: {symbol} dropped to ${price:.2f} (below ${self.low:.2f})")
        elif price >= self.high:
            print(f"  🔔 ALERT: {symbol} rose to ${price:.2f} (above ${self.high:.2f})")


class TradeBot(Observer):
    """Observer that simulates automated trading."""
    
    def __init__(self, name: str, threshold_pct: float = 5.0):
        self.name = name
        self.threshold_pct = threshold_pct
        self.holdings: dict[str, int] = {}
    
    def update(self, subject: Subject, **kwargs) -> None:
        symbol = kwargs.get("symbol")
        change_pct = kwargs.get("change_pct", 0)
        
        if abs(change_pct) >= self.threshold_pct:
            action = "BUY" if change_pct < 0 else "SELL"
            print(f"  🤖 [{self.name}] {action} signal for {symbol} "
                  f"({change_pct:+.1f}% change)")


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Observer Pattern - Stock Market Example")
    print("=" * 60)
    
    # Create subject
    market = StockMarket()
    
    # Create and attach observers
    print("\n--- Setting up observers ---")
    
    display1 = PriceDisplay("Terminal 1")
    display2 = PriceDisplay("Terminal 2")
    
    alert_aapl = PriceAlert("AAPL", low=150, high=200)
    alert_googl = PriceAlert("GOOGL", low=100, high=150)
    
    bot = TradeBot("AlgoBot", threshold_pct=3.0)
    
    market.attach(display1)
    market.attach(display2)
    market.attach(alert_aapl)
    market.attach(alert_googl)
    market.attach(bot)
    
    # Simulate price updates
    print("\n--- Price Updates ---")
    market.set_price("AAPL", 175.00)
    market.set_price("GOOGL", 140.00)
    market.set_price("AAPL", 148.00)  # Triggers alert (below 150)
    market.set_price("GOOGL", 155.00)  # Triggers alert (above 150)
    market.set_price("AAPL", 140.00)  # Big drop, triggers bot
    
    # Detach an observer
    print("\n--- Detaching Terminal 2 ---")
    market.detach(display2)
    
    market.set_price("AAPL", 160.00)
```

---

### Example 2: Event System with Callbacks

```python
#!/usr/bin/env python3
"""
Observer Pattern - Event System with Callbacks

Pythonic approach using functions as observers.
More flexible than class-based observers.
"""

from typing import Callable, Dict, List, Any
from dataclasses import dataclass, field
from functools import wraps
import weakref


# ============== EVENT EMITTER ==============

class EventEmitter:
    """
    Event emitter that supports callback-based observers.
    
    Similar to Node.js EventEmitter or Python's blinker.
    """
    
    def __init__(self):
        # Dict of event_name -> list of callbacks
        self._listeners: Dict[str, List[Callable]] = {}
        # For one-time listeners
        self._once: Dict[str, List[Callable]] = {}
    
    def on(self, event: str, callback: Callable = None):
        """
        Register an event listener.
        
        Can be used as decorator:
            @emitter.on("event_name")
            def handler():
                pass
        """
        def decorator(fn: Callable) -> Callable:
            if event not in self._listeners:
                self._listeners[event] = []
            self._listeners[event].append(fn)
            return fn
        
        if callback:
            return decorator(callback)
        return decorator
    
    def once(self, event: str, callback: Callable = None):
        """Register a one-time listener."""
        def decorator(fn: Callable) -> Callable:
            if event not in self._once:
                self._once[event] = []
            self._once[event].append(fn)
            return fn
        
        if callback:
            return decorator(callback)
        return decorator
    
    def off(self, event: str, callback: Callable = None):
        """Remove an event listener."""
        if callback and event in self._listeners:
            try:
                self._listeners[event].remove(callback)
            except ValueError:
                pass
        elif event in self._listeners:
            self._listeners[event] = []
    
    def emit(self, event: str, *args, **kwargs):
        """Emit an event, calling all listeners."""
        # Regular listeners
        for callback in self._listeners.get(event, []):
            callback(*args, **kwargs)
        
        # One-time listeners (remove after calling)
        once_listeners = self._once.pop(event, [])
        for callback in once_listeners:
            callback(*args, **kwargs)
    
    def listener_count(self, event: str) -> int:
        """Count listeners for an event."""
        regular = len(self._listeners.get(event, []))
        once = len(self._once.get(event, []))
        return regular + once


# ============== APPLICATION EXAMPLE ==============

class UserService(EventEmitter):
    """
    User service that emits events for various actions.
    """
    
    def __init__(self):
        super().__init__()
        self.users: Dict[str, dict] = {}
    
    def create_user(self, username: str, email: str):
        """Create a user and emit event."""
        user = {"username": username, "email": email}
        self.users[username] = user
        self.emit("user:created", user)
        return user
    
    def update_user(self, username: str, **updates):
        """Update a user and emit event."""
        if username in self.users:
            old_data = self.users[username].copy()
            self.users[username].update(updates)
            self.emit("user:updated", self.users[username], old_data)
            return self.users[username]
        return None
    
    def delete_user(self, username: str):
        """Delete a user and emit event."""
        if username in self.users:
            user = self.users.pop(username)
            self.emit("user:deleted", user)
            return user
        return None


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Observer Pattern - Event Emitter")
    print("=" * 60)
    
    service = UserService()
    
    # Register listeners using decorator syntax
    @service.on("user:created")
    def log_creation(user):
        print(f"📝 LOG: User created - {user['username']}")
    
    @service.on("user:created")
    def send_welcome_email(user):
        print(f"📧 EMAIL: Sending welcome to {user['email']}")
    
    @service.on("user:updated")
    def log_update(user, old_data):
        changes = {k: v for k, v in user.items() if old_data.get(k) != v}
        print(f"📝 LOG: User updated - {user['username']}, changes: {changes}")
    
    @service.on("user:deleted")
    def cleanup_user(user):
        print(f"🗑️ CLEANUP: Removing data for {user['username']}")
    
    # One-time listener
    @service.once("user:created")
    def first_user_bonus(user):
        print(f"🎁 BONUS: First user {user['username']} gets special offer!")
    
    # Perform actions
    print("\n--- Creating users ---")
    service.create_user("alice", "alice@example.com")
    service.create_user("bob", "bob@example.com")
    
    print("\n--- Updating user ---")
    service.update_user("alice", email="alice.new@example.com")
    
    print("\n--- Deleting user ---")
    service.delete_user("bob")
    
    # Check listener counts
    print(f"\n--- Listener counts ---")
    print(f"user:created listeners: {service.listener_count('user:created')}")
    
    # Remove a listener
    print("\n--- Removing email listener ---")
    service.off("user:created", send_welcome_email)
    print(f"user:created listeners: {service.listener_count('user:created')}")
    
    print("\n--- Creating another user ---")
    service.create_user("carol", "carol@example.com")
```

---

### Example 3: Property Change Observer

```python
#!/usr/bin/env python3
"""
Observer Pattern - Property Change Observer

Automatically notify observers when object properties change.
Similar to data binding in UI frameworks.
"""

from typing import Callable, Dict, List, Any


# ============== OBSERVABLE PROPERTY ==============

class ObservableProperty:
    """
    Descriptor that notifies on property changes.
    """
    
    def __init__(self, name: str):
        self.name = name
        self.private_name = f"_observable_{name}"
    
    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        return getattr(obj, self.private_name, None)
    
    def __set__(self, obj, value):
        old_value = getattr(obj, self.private_name, None)
        if old_value != value:
            setattr(obj, self.private_name, value)
            obj._notify_change(self.name, old_value, value)


class Observable:
    """
    Base class for observable objects.
    
    Subclasses can use ObservableProperty for auto-notification.
    """
    
    def __init__(self):
        self._observers: Dict[str, List[Callable]] = {}
    
    def observe(self, property_name: str, callback: Callable):
        """Observe changes to a property."""
        if property_name not in self._observers:
            self._observers[property_name] = []
        self._observers[property_name].append(callback)
    
    def unobserve(self, property_name: str, callback: Callable = None):
        """Stop observing a property."""
        if callback and property_name in self._observers:
            try:
                self._observers[property_name].remove(callback)
            except ValueError:
                pass
        elif property_name in self._observers:
            self._observers[property_name] = []
    
    def _notify_change(self, property_name: str, old_value: Any, new_value: Any):
        """Notify observers of property change."""
        for callback in self._observers.get(property_name, []):
            callback(property_name, old_value, new_value)
        
        # Also notify catch-all observers
        for callback in self._observers.get("*", []):
            callback(property_name, old_value, new_value)


# ============== EXAMPLE MODEL ==============

class Person(Observable):
    """Person with observable properties."""
    
    name = ObservableProperty("name")
    age = ObservableProperty("age")
    email = ObservableProperty("email")
    
    def __init__(self, name: str = "", age: int = 0, email: str = ""):
        super().__init__()
        self.name = name
        self.age = age
        self.email = email


class Product(Observable):
    """Product with observable properties."""
    
    name = ObservableProperty("name")
    price = ObservableProperty("price")
    stock = ObservableProperty("stock")
    
    def __init__(self, name: str = "", price: float = 0, stock: int = 0):
        super().__init__()
        self.name = name
        self.price = price
        self.stock = stock


# ============== UI COMPONENTS (OBSERVERS) ==============

class Label:
    """Simulated UI label that displays a value."""
    
    def __init__(self, prefix: str = ""):
        self.prefix = prefix
        self.text = ""
    
    def bind(self, observable: Observable, property_name: str):
        """Bind to an observable property."""
        def update(prop, old, new):
            if prop == property_name:
                self.text = f"{self.prefix}{new}"
                print(f"[Label] {self.text}")
        
        # Set initial value
        initial = getattr(observable, property_name)
        self.text = f"{self.prefix}{initial}"
        
        # Subscribe to changes
        observable.observe(property_name, update)


class ValidationBadge:
    """Badge that shows validation status."""
    
    def __init__(self, validator: Callable[[Any], bool], message: str):
        self.validator = validator
        self.message = message
        self.valid = True
    
    def bind(self, observable: Observable, property_name: str):
        """Bind validation to a property."""
        def update(prop, old, new):
            if prop == property_name:
                self.valid = self.validator(new)
                status = "✓" if self.valid else "✗"
                print(f"[Validation] {status} {self.message}")
        
        observable.observe(property_name, update)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Observer Pattern - Property Change Observer")
    print("=" * 60)
    
    # Create person
    person = Person("Alice", 25, "alice@example.com")
    
    # Bind UI components
    print("\n--- Setting up bindings ---")
    
    name_label = Label("Name: ")
    name_label.bind(person, "name")
    
    age_label = Label("Age: ")
    age_label.bind(person, "age")
    
    age_validator = ValidationBadge(
        lambda age: 18 <= age <= 120,
        "Age must be 18-120"
    )
    age_validator.bind(person, "age")
    
    email_validator = ValidationBadge(
        lambda email: "@" in email,
        "Email must contain @"
    )
    email_validator.bind(person, "email")
    
    # Log all changes
    def log_all_changes(prop, old, new):
        print(f"[LOG] {prop}: {old} -> {new}")
    
    person.observe("*", log_all_changes)
    
    # Make changes
    print("\n--- Changing properties ---")
    person.name = "Bob"
    person.age = 17  # Invalid
    person.age = 30  # Valid
    person.email = "invalid"  # Invalid
    person.email = "bob@example.com"  # Valid
    
    # Product example
    print("\n--- Product example ---")
    
    product = Product("Widget", 9.99, 100)
    
    # Stock alert
    def stock_alert(prop, old, new):
        if prop == "stock" and new < 10:
            print(f"⚠️ LOW STOCK: Only {new} units remaining!")
    
    product.observe("stock", stock_alert)
    
    product.stock = 50
    product.stock = 5  # Triggers alert
```

---

### Example 4: Async Observer

```python
#!/usr/bin/env python3
"""
Observer Pattern - Async Observers

Observers that work with asyncio for non-blocking operations.
"""

import asyncio
from typing import Callable, Dict, List, Any, Coroutine
from dataclasses import dataclass
from datetime import datetime


# ============== ASYNC EVENT EMITTER ==============

class AsyncEventEmitter:
    """
    Event emitter that supports both sync and async callbacks.
    """
    
    def __init__(self):
        self._sync_listeners: Dict[str, List[Callable]] = {}
        self._async_listeners: Dict[str, List[Callable]] = {}
    
    def on(self, event: str):
        """Decorator to register sync listener."""
        def decorator(fn: Callable) -> Callable:
            if event not in self._sync_listeners:
                self._sync_listeners[event] = []
            self._sync_listeners[event].append(fn)
            return fn
        return decorator
    
    def on_async(self, event: str):
        """Decorator to register async listener."""
        def decorator(fn: Callable) -> Callable:
            if event not in self._async_listeners:
                self._async_listeners[event] = []
            self._async_listeners[event].append(fn)
            return fn
        return decorator
    
    async def emit(self, event: str, *args, **kwargs):
        """Emit event and await all listeners."""
        # Call sync listeners
        for callback in self._sync_listeners.get(event, []):
            callback(*args, **kwargs)
        
        # Gather and await async listeners
        async_callbacks = self._async_listeners.get(event, [])
        if async_callbacks:
            tasks = [callback(*args, **kwargs) for callback in async_callbacks]
            await asyncio.gather(*tasks)
    
    def emit_sync(self, event: str, *args, **kwargs):
        """Emit event synchronously (only calls sync listeners)."""
        for callback in self._sync_listeners.get(event, []):
            callback(*args, **kwargs)


# ============== NOTIFICATION SERVICE ==============

@dataclass
class Notification:
    """Notification data."""
    type: str
    message: str
    timestamp: datetime = None
    
    def __post_init__(self):
        if self.timestamp is None:
            self.timestamp = datetime.now()


class NotificationService(AsyncEventEmitter):
    """
    Service that sends notifications through various channels.
    
    Channels are async observers.
    """
    
    async def send(self, notification: Notification):
        """Send a notification to all channels."""
        print(f"\n📤 Sending notification: {notification.message}")
        await self.emit("notification", notification)
        print("✓ All channels notified")


# ============== ASYNC OBSERVERS (CHANNELS) ==============

async def email_channel(notification: Notification):
    """Simulate sending email (slow operation)."""
    await asyncio.sleep(0.5)  # Simulate network delay
    print(f"  📧 Email sent: {notification.message}")


async def sms_channel(notification: Notification):
    """Simulate sending SMS."""
    await asyncio.sleep(0.3)
    print(f"  📱 SMS sent: {notification.message}")


async def push_channel(notification: Notification):
    """Simulate sending push notification."""
    await asyncio.sleep(0.1)
    print(f"  🔔 Push sent: {notification.message}")


async def webhook_channel(notification: Notification):
    """Simulate calling webhook."""
    await asyncio.sleep(0.4)
    print(f"  🌐 Webhook called: {notification.message}")


def sync_logger(notification: Notification):
    """Sync observer for logging."""
    print(f"  📝 Logged: [{notification.type}] {notification.message}")


# ============== USAGE DEMO ==============
async def main():
    print("=" * 60)
    print("Observer Pattern - Async Observers")
    print("=" * 60)
    
    # Create service
    service = NotificationService()
    
    # Register async observers
    @service.on_async("notification")
    async def email_handler(n):
        await email_channel(n)
    
    @service.on_async("notification")
    async def sms_handler(n):
        await sms_channel(n)
    
    @service.on_async("notification")
    async def push_handler(n):
        await push_channel(n)
    
    @service.on_async("notification")
    async def webhook_handler(n):
        await webhook_channel(n)
    
    # Register sync observer
    @service.on("notification")
    def log_handler(n):
        sync_logger(n)
    
    # Send notifications
    import time
    
    start = time.perf_counter()
    
    await service.send(Notification(
        type="alert",
        message="Server is down!"
    ))
    
    await service.send(Notification(
        type="info",
        message="New user registered"
    ))
    
    elapsed = time.perf_counter() - start
    
    print(f"\n⏱️ Total time: {elapsed:.2f}s")
    print("(All async channels run concurrently!)")
    
    # Demonstrate concurrent notifications
    print("\n--- Sending 3 notifications concurrently ---")
    
    notifications = [
        Notification("warning", "CPU usage high"),
        Notification("error", "Database connection lost"),
        Notification("info", "Backup completed"),
    ]
    
    start = time.perf_counter()
    await asyncio.gather(*[service.send(n) for n in notifications])
    elapsed = time.perf_counter() - start
    
    print(f"\n⏱️ Total time for 3 notifications: {elapsed:.2f}s")


if __name__ == "__main__":
    asyncio.run(main())
```

---

## 6. When to Use / When to Avoid

### Use When:
- **One-to-many** relationship between objects
- Changes in one object should **trigger updates** in others
- You don't know **how many** observers will exist
- Observers should be **loosely coupled** to the subject

### Avoid When:
- Simple one-to-one relationship (direct call is fine)
- Order of notification matters (Observer doesn't guarantee order)
- Observers might cause complex cascading updates

### Python Idiom:
Use **callback functions** for simple cases:

```python
# Simple callback-based observation
class Subject:
    def __init__(self):
        self.on_change = []
    
    def set_value(self, value):
        self.value = value
        for callback in self.on_change:
            callback(value)

subject = Subject()
subject.on_change.append(lambda v: print(f"Value: {v}"))
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Mediator** | Centralizes communication; Observer is direct notification |
| **Singleton** | Subjects often are singletons (e.g., event bus) |
| **Strategy** | Observers can use different strategies to handle events |
| **Command** | Events can be Command objects |

