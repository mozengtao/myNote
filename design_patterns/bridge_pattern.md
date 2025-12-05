# Bridge Pattern (桥接模式)

## ASCII Diagram

```
                    Abstraction                          Implementation
                    (What to do)                         (How to do it)
                         |                                     |
+------------------------+------------------------+            |
|                                                 |            |
|  +-------------------+     +-------------------+|            |
|  |   Abstraction     |     |  Implementor     ||<-----------+
|  +-------------------+     +-------------------+|
|  | - impl: Implementor|---->| + operationImpl()||
|  +-------------------+     +-------------------+|
|  | + operation()     |            ^            |
|  +--------+----------+            |            |
|           ^               +-------+-------+    |
|           |               |               |    |
|  +--------+--------+  +---+---+     +---+---+  |
|  |RefinedAbstraction| |ImplA  |     |ImplB  |  |
|  +-----------------+  +-------+     +-------+  |
|  | + operation()   |  |+opImpl|     |+opImpl|  |
|  +-----------------+  +-------+     +-------+  |
+------------------------------------------------+

Bridge separates:
+------------------+          +------------------+
|  Shape           |  bridge  |  Color           |
|  (Circle, Square)|<-------->|  (Red, Blue)     |
+------------------+          +------------------+
     Abstraction                 Implementation

Without Bridge (N x M classes):
RedCircle, BlueCircle, GreenCircle
RedSquare, BlueSquare, GreenSquare
...

With Bridge (N + M classes):
Circle + Red
Circle + Blue
Square + Red
...
```

**中文说明：**
- **Abstraction（抽象）**：定义高层接口，维护对实现的引用
- **RefinedAbstraction（精化抽象）**：扩展抽象部分
- **Implementor（实现接口）**：定义实现类的接口
- **ConcreteImplementor（具体实现）**：实现 Implementor 接口
- **关键点**：将抽象与实现分离，使它们可以独立变化

---

## 核心思想

将**抽象部分**与**实现部分**分离，使它们可以**独立变化**。通过组合而非继承来实现功能的扩展，避免了类爆炸问题。

**与继承的区别：**
- 继承：静态绑定，编译时确定
- 桥接：动态绑定，运行时可切换

---

## 应用场景

1. **多维度变化**：系统需要在多个维度上扩展（如形状×颜色、设备×平台）
2. **避免类爆炸**：避免因多维度组合导致的类数量激增
3. **运行时切换**：需要在运行时切换实现
4. **实际应用**：
   - 跨平台 GUI（按钮×操作系统）
   - 消息发送（消息类型×发送渠道）
   - 设备驱动（设备×操作系统）
   - 图形渲染（形状×渲染引擎）

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 分离关注点 | 抽象与实现各自独立变化 |
| 避免类爆炸 | N+M 个类代替 N×M 个类 |
| 运行时灵活 | 可动态切换实现 |
| 符合开闭原则 | 新增抽象或实现无需修改现有代码 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 增加复杂度 | 需要理解抽象和实现的分离 |
| 设计难度 | 正确识别变化维度需要经验 |

---

## Python 代码示例

### 应用前：类爆炸问题

```python
# 问题：通知系统，消息类型 × 发送渠道 = 类爆炸

# 普通消息
class EmailNormalMessage:
    def send(self, content):
        return f"[Email] Normal: {content}"

class SMSNormalMessage:
    def send(self, content):
        return f"[SMS] Normal: {content}"

class SlackNormalMessage:
    def send(self, content):
        return f"[Slack] Normal: {content}"

class WeChatNormalMessage:
    def send(self, content):
        return f"[WeChat] Normal: {content}"

# 紧急消息
class EmailUrgentMessage:
    def send(self, content):
        return f"[Email] URGENT!!! {content}"

class SMSUrgentMessage:
    def send(self, content):
        return f"[SMS] URGENT!!! {content}"

class SlackUrgentMessage:
    def send(self, content):
        return f"[Slack] URGENT!!! {content}"

class WeChatUrgentMessage:
    def send(self, content):
        return f"[WeChat] URGENT!!! {content}"

# 定时消息
class EmailScheduledMessage:
    def send(self, content, schedule):
        return f"[Email] Scheduled({schedule}): {content}"

class SMSScheduledMessage:
    def send(self, content, schedule):
        return f"[SMS] Scheduled({schedule}): {content}"

# ... 还要继续写下去

# 问题：
# - 4 种渠道 × 3 种类型 = 12 个类
# - 新增一种渠道，需要新增 3 个类
# - 新增一种类型，需要新增 4 个类
# - 代码重复严重
```

### 应用后：使用桥接模式

```python
from abc import ABC, abstractmethod
from datetime import datetime
from typing import List, Optional


# ========== 实现接口（发送渠道）==========
class MessageSender(ABC):
    """消息发送器接口 - 实现维度"""
    
    @abstractmethod
    def send(self, title: str, content: str) -> bool:
        pass
    
    @abstractmethod
    def get_channel_name(self) -> str:
        pass


# ========== 具体实现（各种发送渠道）==========
class EmailSender(MessageSender):
    """邮件发送器"""
    
    def __init__(self, smtp_server: str = "smtp.example.com"):
        self.smtp_server = smtp_server
    
    def send(self, title: str, content: str) -> bool:
        print(f"[Email via {self.smtp_server}]")
        print(f"  Subject: {title}")
        print(f"  Body: {content}")
        return True
    
    def get_channel_name(self) -> str:
        return "Email"


class SMSSender(MessageSender):
    """短信发送器"""
    
    def __init__(self, api_key: str = "sms_api_key"):
        self.api_key = api_key
    
    def send(self, title: str, content: str) -> bool:
        # 短信通常没有标题，合并内容
        message = f"{title}: {content}" if title else content
        print(f"[SMS] {message[:160]}")  # 短信限制
        return True
    
    def get_channel_name(self) -> str:
        return "SMS"


class SlackSender(MessageSender):
    """Slack 发送器"""
    
    def __init__(self, webhook_url: str = "https://hooks.slack.com/..."):
        self.webhook_url = webhook_url
    
    def send(self, title: str, content: str) -> bool:
        print(f"[Slack Webhook]")
        print(f"  *{title}*")
        print(f"  {content}")
        return True
    
    def get_channel_name(self) -> str:
        return "Slack"


class WeChatSender(MessageSender):
    """微信发送器"""
    
    def __init__(self, app_id: str = "wx_app_id"):
        self.app_id = app_id
    
    def send(self, title: str, content: str) -> bool:
        print(f"[WeChat Template Message]")
        print(f"  标题: {title}")
        print(f"  内容: {content}")
        return True
    
    def get_channel_name(self) -> str:
        return "WeChat"


class PushNotificationSender(MessageSender):
    """推送通知发送器（新增渠道很容易）"""
    
    def send(self, title: str, content: str) -> bool:
        print(f"[Push Notification]")
        print(f"  Title: {title}")
        print(f"  Body: {content}")
        return True
    
    def get_channel_name(self) -> str:
        return "Push"


# ========== 抽象（消息类型）==========
class Message(ABC):
    """消息抽象类 - 抽象维度"""
    
    def __init__(self, sender: MessageSender):
        # 桥接：持有实现的引用
        self._sender = sender
    
    @abstractmethod
    def send(self, content: str, **kwargs) -> bool:
        pass
    
    def set_sender(self, sender: MessageSender):
        """运行时切换发送渠道"""
        self._sender = sender


# ========== 精化抽象（具体消息类型）==========
class NormalMessage(Message):
    """普通消息"""
    
    def send(self, content: str, **kwargs) -> bool:
        title = kwargs.get("title", "Notification")
        return self._sender.send(title, content)


class UrgentMessage(Message):
    """紧急消息"""
    
    def send(self, content: str, **kwargs) -> bool:
        title = f"🚨 URGENT: {kwargs.get('title', 'Alert')}"
        urgent_content = f"[IMMEDIATE ACTION REQUIRED]\n{content}"
        return self._sender.send(title, urgent_content)


class ScheduledMessage(Message):
    """定时消息"""
    
    def __init__(self, sender: MessageSender, schedule_time: datetime = None):
        super().__init__(sender)
        self.schedule_time = schedule_time or datetime.now()
    
    def send(self, content: str, **kwargs) -> bool:
        title = kwargs.get("title", "Scheduled Notification")
        scheduled_content = f"[Scheduled: {self.schedule_time}]\n{content}"
        return self._sender.send(title, scheduled_content)


class BatchMessage(Message):
    """批量消息 - 可以使用多个发送器"""
    
    def __init__(self, sender: MessageSender, additional_senders: List[MessageSender] = None):
        super().__init__(sender)
        self._additional_senders = additional_senders or []
    
    def add_sender(self, sender: MessageSender):
        self._additional_senders.append(sender)
    
    def send(self, content: str, **kwargs) -> bool:
        title = kwargs.get("title", "Batch Notification")
        all_senders = [self._sender] + self._additional_senders
        
        print(f"\n--- Batch sending to {len(all_senders)} channels ---")
        results = []
        for sender in all_senders:
            print(f"\nSending via {sender.get_channel_name()}:")
            results.append(sender.send(title, content))
        
        return all(results)


class TemplateMessage(Message):
    """模板消息"""
    
    def __init__(self, sender: MessageSender, template: str):
        super().__init__(sender)
        self.template = template
    
    def send(self, content: str, **kwargs) -> bool:
        # 用 kwargs 填充模板
        filled_content = self.template.format(content=content, **kwargs)
        title = kwargs.get("title", "Template Message")
        return self._sender.send(title, filled_content)


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    # 创建发送器（实现）
    email = EmailSender("smtp.gmail.com")
    sms = SMSSender("twilio_api_key")
    slack = SlackSender()
    wechat = WeChatSender()
    
    print("=" * 60)
    print("1. Normal Message via Email")
    print("=" * 60)
    normal_email = NormalMessage(email)
    normal_email.send("Your order has been shipped.", title="Order Update")
    
    print("\n" + "=" * 60)
    print("2. Urgent Message via SMS")
    print("=" * 60)
    urgent_sms = UrgentMessage(sms)
    urgent_sms.send("Server CPU at 95%!", title="Server Alert")
    
    print("\n" + "=" * 60)
    print("3. Same Urgent Message, switch to Slack (runtime)")
    print("=" * 60)
    urgent_sms.set_sender(slack)  # 运行时切换渠道！
    urgent_sms.send("Server CPU at 95%!", title="Server Alert")
    
    print("\n" + "=" * 60)
    print("4. Scheduled Message via WeChat")
    print("=" * 60)
    scheduled = ScheduledMessage(wechat, datetime(2024, 12, 25, 9, 0))
    scheduled.send("Merry Christmas!", title="Holiday Greeting")
    
    print("\n" + "=" * 60)
    print("5. Batch Message to multiple channels")
    print("=" * 60)
    batch = BatchMessage(email)
    batch.add_sender(slack)
    batch.add_sender(wechat)
    batch.send("System maintenance tonight at 10 PM", title="Maintenance Notice")
    
    print("\n" + "=" * 60)
    print("6. Template Message")
    print("=" * 60)
    template = """
    Dear {name},
    
    {content}
    
    Best regards,
    {sender_name}
    """
    template_msg = TemplateMessage(email, template)
    template_msg.send(
        "Your subscription has been renewed.",
        title="Subscription Update",
        name="John",
        sender_name="Support Team"
    )
    
    # ========== 展示灵活性 ==========
    print("\n" + "=" * 60)
    print("7. Flexibility Demo: Same message type, different channels")
    print("=" * 60)
    
    senders = [email, sms, slack, wechat]
    for sender in senders:
        msg = UrgentMessage(sender)
        print(f"\n--- Via {sender.get_channel_name()} ---")
        msg.send("Critical security update available", title="Security")


# ========== 类数量对比 ==========
"""
Without Bridge Pattern:
- NormalEmailMessage
- NormalSMSMessage
- NormalSlackMessage
- NormalWeChatMessage
- UrgentEmailMessage
- UrgentSMSMessage
- UrgentSlackMessage
- UrgentWeChatMessage
- ScheduledEmailMessage
- ... (4 channels × 4 types = 16 classes)

With Bridge Pattern:
- Senders: EmailSender, SMSSender, SlackSender, WeChatSender (4 classes)
- Messages: NormalMessage, UrgentMessage, ScheduledMessage, BatchMessage (4 classes)
- Total: 8 classes (instead of 16)

Add new channel? Just 1 new Sender class.
Add new message type? Just 1 new Message class.
"""
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **类数量** | N × M 个类 | N + M 个类 |
| **扩展渠道** | 需要新增 M 个类 | 只需新增 1 个 Sender 类 |
| **扩展类型** | 需要新增 N 个类 | 只需新增 1 个 Message 类 |
| **运行时切换** | 不可能 | 可以动态切换 Sender |
| **代码复用** | 大量重复代码 | 逻辑集中，无重复 |
| **测试** | 需要测试所有组合 | 可以分别测试抽象和实现 |

---

## 与其他模式的关系

```
+-------------------+     +-------------------+     +-------------------+
|     Strategy      |     |      Bridge       |     |   Adapter         |
+-------------------+     +-------------------+     +-------------------+
| 行为可替换        |     | 结构分离          |     | 接口转换          |
| 单一维度变化      |     | 多维度变化        |     | 兼容不同接口      |
| 客户端选择算法    |     | 抽象和实现独立    |     | 事后补救          |
+-------------------+     +-------------------+     +-------------------+
```

---

## 实际应用：跨平台 UI 组件

```python
from abc import ABC, abstractmethod


# ========== 实现：平台渲染器 ==========
class PlatformRenderer(ABC):
    @abstractmethod
    def render_button(self, text: str) -> str:
        pass
    
    @abstractmethod
    def render_text_field(self, placeholder: str) -> str:
        pass
    
    @abstractmethod
    def render_checkbox(self, label: str, checked: bool) -> str:
        pass


class WindowsRenderer(PlatformRenderer):
    def render_button(self, text: str) -> str:
        return f"[Windows Button: {text}]"
    
    def render_text_field(self, placeholder: str) -> str:
        return f"[Windows TextBox: {placeholder}]"
    
    def render_checkbox(self, label: str, checked: bool) -> str:
        mark = "X" if checked else " "
        return f"[{mark}] {label}"


class MacRenderer(PlatformRenderer):
    def render_button(self, text: str) -> str:
        return f"(Mac Button: {text})"
    
    def render_text_field(self, placeholder: str) -> str:
        return f"(Mac TextField: {placeholder})"
    
    def render_checkbox(self, label: str, checked: bool) -> str:
        mark = "✓" if checked else " "
        return f"({mark}) {label}"


class WebRenderer(PlatformRenderer):
    def render_button(self, text: str) -> str:
        return f"<button>{text}</button>"
    
    def render_text_field(self, placeholder: str) -> str:
        return f'<input placeholder="{placeholder}"/>'
    
    def render_checkbox(self, label: str, checked: bool) -> str:
        checked_attr = " checked" if checked else ""
        return f'<input type="checkbox"{checked_attr}/> {label}'


# ========== 抽象：UI 组件 ==========
class UIComponent(ABC):
    def __init__(self, renderer: PlatformRenderer):
        self._renderer = renderer
    
    @abstractmethod
    def draw(self) -> str:
        pass


class LoginForm(UIComponent):
    def draw(self) -> str:
        lines = [
            "=== Login Form ===",
            self._renderer.render_text_field("Username"),
            self._renderer.render_text_field("Password"),
            self._renderer.render_checkbox("Remember me", False),
            self._renderer.render_button("Login"),
        ]
        return "\n".join(lines)


class SettingsPanel(UIComponent):
    def __init__(self, renderer: PlatformRenderer, settings: dict):
        super().__init__(renderer)
        self.settings = settings
    
    def draw(self) -> str:
        lines = ["=== Settings ==="]
        for key, value in self.settings.items():
            if isinstance(value, bool):
                lines.append(self._renderer.render_checkbox(key, value))
            else:
                lines.append(self._renderer.render_text_field(f"{key}: {value}"))
        lines.append(self._renderer.render_button("Save"))
        return "\n".join(lines)


# 使用
for renderer in [WindowsRenderer(), MacRenderer(), WebRenderer()]:
    print(f"\n--- {renderer.__class__.__name__} ---")
    form = LoginForm(renderer)
    print(form.draw())
```

