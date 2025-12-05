# Observer Pattern (观察者模式)

## ASCII Diagram

```
+-------------------+          +-------------------+
|     Subject       |          |     Observer      |<<interface>>
+-------------------+          +-------------------+
| - observers[]     |<-------->| + update()        |
+-------------------+          +-------------------+
| + attach(observer)|                  ^
| + detach(observer)|                  |
| + notify()        |     +------------+------------+
+-------------------+     |            |            |
         ^                |            |            |
         |          +-----+----+ +-----+----+ +-----+----+
         |          |Observer1 | |Observer2 | |Observer3 |
+--------+--------+ +----------+ +----------+ +----------+
|ConcreteSubject  | | +update()| | +update()| | +update()|
+-----------------+ +----------+ +----------+ +----------+
| - state         |
+-----------------+
| + getState()    |
| + setState()    |
+-----------------+

Notification Flow:
+----------------+     state change      +----------------+
| ConcreteSubject|-------------------->| notify()        |
+----------------+                      +-------+--------+
                                                |
                    +---------------------------+---------------------------+
                    |                           |                           |
                    v                           v                           v
            +--------------+            +--------------+            +--------------+
            | Observer 1   |            | Observer 2   |            | Observer 3   |
            +--------------+            +--------------+            +--------------+
            | update()     |            | update()     |            | update()     |
            +--------------+            +--------------+            +--------------+
```

**中文说明：**
- **Subject（主题/被观察者）**：维护观察者列表，状态变化时通知观察者
- **Observer（观察者）**：定义更新接口，接收主题的通知
- **ConcreteSubject（具体主题）**：存储状态，状态变化时发送通知
- **ConcreteObserver（具体观察者）**：实现更新接口，响应主题状态变化

---

## 核心思想

定义对象间的**一对多**依赖关系，当一个对象的状态改变时，所有依赖它的对象都会**自动收到通知**并更新。观察者模式实现了发布-订阅机制，实现松耦合。

---

## 应用场景

1. **一对多依赖**：一个对象状态改变需要通知多个对象
2. **发布-订阅**：实现事件系统
3. **解耦**：抽象耦合，主题不需要知道具体观察者
4. **实际应用**：
   - GUI 事件处理
   - 消息队列
   - 股票价格推送
   - 社交媒体关注/粉丝
   - MVC 中的 Model-View 通信

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 松耦合 | 主题和观察者相互独立 |
| 支持广播 | 一次通知多个观察者 |
| 开闭原则 | 可以新增观察者而不修改主题 |
| 动态关系 | 运行时添加/移除观察者 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 通知顺序不确定 | 观察者收到通知的顺序可能不同 |
| 内存泄漏 | 忘记移除观察者可能导致泄漏 |
| 级联更新 | 观察者更新可能触发更多更新 |
| 性能 | 大量观察者时通知开销大 |

---

## Python 代码示例

### 应用前：紧耦合

```python
# 问题：天气站直接调用各个显示设备，紧耦合

class WeatherStation:
    """天气站 - 直接依赖所有显示设备"""
    
    def __init__(self):
        self.temperature = 0
        self.humidity = 0
        self.pressure = 0
        
        # 直接依赖具体显示设备
        self.phone_display = PhoneDisplay()
        self.window_display = WindowDisplay()
        self.web_dashboard = WebDashboard()
    
    def set_measurements(self, temp, humidity, pressure):
        self.temperature = temp
        self.humidity = humidity
        self.pressure = pressure
        
        # 手动通知每个设备 - 需要修改才能添加/移除设备
        self.phone_display.update(temp, humidity, pressure)
        self.window_display.update(temp, humidity, pressure)
        self.web_dashboard.update(temp, humidity, pressure)


class PhoneDisplay:
    def update(self, temp, humidity, pressure):
        print(f"[Phone] Temp: {temp}°C, Humidity: {humidity}%")


class WindowDisplay:
    def update(self, temp, humidity, pressure):
        print(f"[Window] {temp}°C / {humidity}% / {pressure}hPa")


class WebDashboard:
    def update(self, temp, humidity, pressure):
        print(f"[Web] Weather: {temp}°C, {humidity}%, {pressure}hPa")


# 问题：
# 1. WeatherStation 直接依赖所有显示设备
# 2. 添加新设备需要修改 WeatherStation
# 3. 无法动态添加/移除设备
# 4. 不符合开闭原则
```

### 应用后：使用观察者模式

```python
from abc import ABC, abstractmethod
from typing import List, Dict, Any, Set, Optional
from dataclasses import dataclass
from enum import Enum
import weakref


# ========== 观察者接口 ==========
class Observer(ABC):
    """观察者抽象基类"""
    
    @abstractmethod
    def update(self, subject: 'Subject', *args, **kwargs) -> None:
        """接收更新通知"""
        pass


# ========== 主题接口 ==========
class Subject(ABC):
    """主题抽象基类"""
    
    def __init__(self):
        self._observers: List[Observer] = []
    
    def attach(self, observer: Observer) -> None:
        """添加观察者"""
        if observer not in self._observers:
            self._observers.append(observer)
            print(f"  [Subject] Attached: {observer.__class__.__name__}")
    
    def detach(self, observer: Observer) -> None:
        """移除观察者"""
        if observer in self._observers:
            self._observers.remove(observer)
            print(f"  [Subject] Detached: {observer.__class__.__name__}")
    
    def notify(self, *args, **kwargs) -> None:
        """通知所有观察者"""
        print(f"  [Subject] Notifying {len(self._observers)} observers...")
        for observer in self._observers:
            observer.update(self, *args, **kwargs)


# ========== 具体主题 ==========
@dataclass
class WeatherData:
    """天气数据"""
    temperature: float
    humidity: float
    pressure: float


class WeatherStation(Subject):
    """天气站 - 具体主题"""
    
    def __init__(self):
        super().__init__()
        self._data = WeatherData(0.0, 0.0, 0.0)
    
    @property
    def data(self) -> WeatherData:
        return self._data
    
    def set_measurements(self, temp: float, humidity: float, pressure: float):
        """设置测量数据并通知观察者"""
        print(f"\n[WeatherStation] New measurements: {temp}°C, {humidity}%, {pressure}hPa")
        self._data = WeatherData(temp, humidity, pressure)
        self.notify()


# ========== 具体观察者 ==========
class CurrentConditionsDisplay(Observer):
    """当前天气显示"""
    
    def update(self, subject: Subject, *args, **kwargs) -> None:
        if isinstance(subject, WeatherStation):
            data = subject.data
            print(f"  [CurrentConditions] Temperature: {data.temperature}°C, "
                  f"Humidity: {data.humidity}%")


class StatisticsDisplay(Observer):
    """统计显示"""
    
    def __init__(self):
        self._temperatures: List[float] = []
    
    def update(self, subject: Subject, *args, **kwargs) -> None:
        if isinstance(subject, WeatherStation):
            temp = subject.data.temperature
            self._temperatures.append(temp)
            
            avg = sum(self._temperatures) / len(self._temperatures)
            min_t = min(self._temperatures)
            max_t = max(self._temperatures)
            
            print(f"  [Statistics] Avg: {avg:.1f}°C, Min: {min_t}°C, Max: {max_t}°C")


class ForecastDisplay(Observer):
    """天气预报显示"""
    
    def __init__(self):
        self._last_pressure = 0.0
    
    def update(self, subject: Subject, *args, **kwargs) -> None:
        if isinstance(subject, WeatherStation):
            current_pressure = subject.data.pressure
            
            if current_pressure > self._last_pressure:
                forecast = "Improving weather!"
            elif current_pressure < self._last_pressure:
                forecast = "Watch out for rain!"
            else:
                forecast = "More of the same"
            
            print(f"  [Forecast] {forecast}")
            self._last_pressure = current_pressure


class AlertDisplay(Observer):
    """警报显示"""
    
    def __init__(self, temp_threshold: float = 35.0):
        self._threshold = temp_threshold
    
    def update(self, subject: Subject, *args, **kwargs) -> None:
        if isinstance(subject, WeatherStation):
            temp = subject.data.temperature
            if temp > self._threshold:
                print(f"  [ALERT] ⚠️ High temperature warning: {temp}°C > {self._threshold}°C!")


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    print("=" * 60)
    print("Weather Station with Observer Pattern")
    print("=" * 60)
    
    # 创建天气站（主题）
    weather_station = WeatherStation()
    
    # 创建显示设备（观察者）
    current_display = CurrentConditionsDisplay()
    stats_display = StatisticsDisplay()
    forecast_display = ForecastDisplay()
    alert_display = AlertDisplay(temp_threshold=30.0)
    
    # 注册观察者
    print("\nAttaching observers:")
    weather_station.attach(current_display)
    weather_station.attach(stats_display)
    weather_station.attach(forecast_display)
    weather_station.attach(alert_display)
    
    # 模拟天气变化
    print("\n" + "=" * 60)
    weather_station.set_measurements(25.0, 65.0, 1013.0)
    
    print("\n" + "=" * 60)
    weather_station.set_measurements(28.0, 70.0, 1010.0)
    
    print("\n" + "=" * 60)
    weather_station.set_measurements(32.0, 80.0, 1005.0)  # 触发警报
    
    # 动态移除观察者
    print("\n" + "=" * 60)
    print("Detaching alert display:")
    weather_station.detach(alert_display)
    
    weather_station.set_measurements(35.0, 85.0, 1000.0)  # 不再有警报


# ========== 高级实现：事件系统 ==========
print("\n" + "=" * 60)
print("Advanced: Event System")
print("=" * 60)


class EventType(Enum):
    """事件类型"""
    USER_LOGIN = "user_login"
    USER_LOGOUT = "user_logout"
    ORDER_CREATED = "order_created"
    ORDER_PAID = "order_paid"
    ORDER_SHIPPED = "order_shipped"


@dataclass
class Event:
    """事件数据"""
    type: EventType
    data: Dict[str, Any]
    timestamp: str = ""
    
    def __post_init__(self):
        from datetime import datetime
        if not self.timestamp:
            self.timestamp = datetime.now().isoformat()


class EventHandler(ABC):
    """事件处理器接口"""
    
    @abstractmethod
    def handle(self, event: Event) -> None:
        pass


class EventBus:
    """事件总线 - 发布订阅系统"""
    
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._handlers: Dict[EventType, List[EventHandler]] = {}
        return cls._instance
    
    def subscribe(self, event_type: EventType, handler: EventHandler) -> None:
        """订阅事件"""
        if event_type not in self._handlers:
            self._handlers[event_type] = []
        self._handlers[event_type].append(handler)
        print(f"  [EventBus] {handler.__class__.__name__} subscribed to {event_type.value}")
    
    def unsubscribe(self, event_type: EventType, handler: EventHandler) -> None:
        """取消订阅"""
        if event_type in self._handlers:
            self._handlers[event_type].remove(handler)
    
    def publish(self, event: Event) -> None:
        """发布事件"""
        print(f"\n  [EventBus] Publishing: {event.type.value}")
        handlers = self._handlers.get(event.type, [])
        for handler in handlers:
            try:
                handler.handle(event)
            except Exception as e:
                print(f"  [EventBus] Error in handler: {e}")


# 具体事件处理器
class EmailNotificationHandler(EventHandler):
    """邮件通知处理器"""
    
    def handle(self, event: Event) -> None:
        if event.type == EventType.ORDER_CREATED:
            print(f"    [Email] Sending order confirmation to {event.data.get('email')}")
        elif event.type == EventType.ORDER_SHIPPED:
            print(f"    [Email] Sending shipping notification to {event.data.get('email')}")


class SMSNotificationHandler(EventHandler):
    """短信通知处理器"""
    
    def handle(self, event: Event) -> None:
        if event.type in [EventType.ORDER_PAID, EventType.ORDER_SHIPPED]:
            print(f"    [SMS] Sending notification to {event.data.get('phone')}")


class AnalyticsHandler(EventHandler):
    """分析处理器"""
    
    def handle(self, event: Event) -> None:
        print(f"    [Analytics] Tracking event: {event.type.value}, data: {event.data}")


class InventoryHandler(EventHandler):
    """库存处理器"""
    
    def handle(self, event: Event) -> None:
        if event.type == EventType.ORDER_CREATED:
            print(f"    [Inventory] Reserving items for order {event.data.get('order_id')}")
        elif event.type == EventType.ORDER_PAID:
            print(f"    [Inventory] Confirming reservation for order {event.data.get('order_id')}")


# 使用事件系统
event_bus = EventBus()

# 注册处理器
email_handler = EmailNotificationHandler()
sms_handler = SMSNotificationHandler()
analytics_handler = AnalyticsHandler()
inventory_handler = InventoryHandler()

print("\nSubscribing handlers:")
event_bus.subscribe(EventType.ORDER_CREATED, email_handler)
event_bus.subscribe(EventType.ORDER_CREATED, analytics_handler)
event_bus.subscribe(EventType.ORDER_CREATED, inventory_handler)
event_bus.subscribe(EventType.ORDER_PAID, sms_handler)
event_bus.subscribe(EventType.ORDER_PAID, analytics_handler)
event_bus.subscribe(EventType.ORDER_PAID, inventory_handler)
event_bus.subscribe(EventType.ORDER_SHIPPED, email_handler)
event_bus.subscribe(EventType.ORDER_SHIPPED, sms_handler)

# 发布事件
print("\n" + "=" * 40)
event_bus.publish(Event(
    EventType.ORDER_CREATED,
    {"order_id": "ORD-001", "email": "user@example.com", "phone": "123-456-7890"}
))

print("\n" + "=" * 40)
event_bus.publish(Event(
    EventType.ORDER_PAID,
    {"order_id": "ORD-001", "amount": 99.99, "phone": "123-456-7890"}
))

print("\n" + "=" * 40)
event_bus.publish(Event(
    EventType.ORDER_SHIPPED,
    {"order_id": "ORD-001", "email": "user@example.com", "phone": "123-456-7890", "tracking": "TRK123"}
))


# ========== 使用弱引用避免内存泄漏 ==========
print("\n" + "=" * 60)
print("Weak Reference Observer (Memory Safe)")
print("=" * 60)


class WeakSubject:
    """使用弱引用的主题，自动清理已销毁的观察者"""
    
    def __init__(self):
        self._observers: Set[weakref.ref] = set()
    
    def attach(self, observer: Observer) -> None:
        self._observers.add(weakref.ref(observer))
    
    def detach(self, observer: Observer) -> None:
        self._observers.discard(weakref.ref(observer))
    
    def notify(self, *args, **kwargs) -> None:
        # 清理已销毁的观察者并通知存活的
        dead = set()
        for ref in self._observers:
            observer = ref()
            if observer is None:
                dead.add(ref)
            else:
                observer.update(self, *args, **kwargs)
        self._observers -= dead
        if dead:
            print(f"  [WeakSubject] Cleaned up {len(dead)} dead observers")
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **耦合度** | 主题直接依赖具体观察者 | 主题只依赖抽象观察者接口 |
| **扩展性** | 添加观察者需修改主题 | 动态添加/移除观察者 |
| **复用性** | 观察者难以复用 | 观察者可在不同主题间复用 |
| **维护性** | 代码集中，难以维护 | 关注点分离，易于维护 |

---

## 推模型 vs 拉模型

```python
# 推模型：主题推送所有数据
class PushSubject:
    def notify(self, temperature, humidity, pressure):
        for observer in self._observers:
            observer.update(temperature, humidity, pressure)

# 拉模型：观察者主动获取需要的数据
class PullSubject:
    def notify(self):
        for observer in self._observers:
            observer.update(self)  # 传递主题引用

class PullObserver:
    def update(self, subject):
        # 只获取需要的数据
        temp = subject.temperature
```

---

## 与其他模式的关系

| 模式 | 关系 |
|------|------|
| **Mediator** | 观察者分布式通信，中介者集中式通信 |
| **Singleton** | 事件总线通常是单例 |
| **Command** | 可以将命令作为事件传递 |

