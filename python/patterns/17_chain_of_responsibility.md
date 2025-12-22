# Chain of Responsibility Pattern in Python

---

## 1. Pattern Name: Chain of Responsibility

**Purpose / Problem Solved:**
Pass requests along a chain of handlers. Each handler decides whether to process the request or pass it to the next handler.

---

## 2. ASCII Diagram

```
REQUEST
   |
   v
+--------+     +--------+     +--------+     +--------+
|Handler1| --> |Handler2| --> |Handler3| --> |Handler4|
+--------+     +--------+     +--------+     +--------+
   |              |              |              |
   v              v              v              v
[can't handle] [can't handle] [HANDLES IT!]   (not reached)
```

**中文说明：**
责任链模式让请求沿着处理器链传递。每个处理器决定是处理请求还是传递给下一个。解耦了请求发送者和接收者。常用于：中间件管道、日志级别过滤、事件冒泡、审批流程。

---

## 3. Python Module Example

```python
#!/usr/bin/env python3
"""Chain of Responsibility - Middleware & Logging Examples"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Optional, Callable, Any
from enum import IntEnum


# ============== CLASSIC CHAIN ==============

class Handler(ABC):
    """Abstract handler with successor chain."""
    
    def __init__(self):
        self._next: Optional[Handler] = None
    
    def set_next(self, handler: "Handler") -> "Handler":
        self._next = handler
        return handler  # Enable chaining
    
    def handle(self, request: Any) -> Optional[str]:
        result = self.process(request)
        if result is None and self._next:
            return self._next.handle(request)
        return result
    
    @abstractmethod
    def process(self, request: Any) -> Optional[str]:
        pass


# ============== SUPPORT TICKET HANDLERS ==============

@dataclass
class Ticket:
    severity: str  # "low", "medium", "high", "critical"
    message: str


class FrontDesk(Handler):
    def process(self, ticket: Ticket) -> Optional[str]:
        if ticket.severity == "low":
            return f"FrontDesk: Answered '{ticket.message}'"
        return None


class TechSupport(Handler):
    def process(self, ticket: Ticket) -> Optional[str]:
        if ticket.severity == "medium":
            return f"TechSupport: Resolved '{ticket.message}'"
        return None


class Engineering(Handler):
    def process(self, ticket: Ticket) -> Optional[str]:
        if ticket.severity == "high":
            return f"Engineering: Fixed '{ticket.message}'"
        return None


class CTO(Handler):
    def process(self, ticket: Ticket) -> Optional[str]:
        return f"CTO: Handled critical '{ticket.message}'"


# ============== MIDDLEWARE STYLE (Functional) ==============

def make_chain(*handlers: Callable) -> Callable:
    """Create chain from handler functions."""
    def chain(request, idx=0):
        if idx < len(handlers):
            return handlers[idx](request, lambda r: chain(r, idx + 1))
        return request
    return chain


def logging_middleware(request, next_handler):
    print(f"  LOG: Processing {request}")
    result = next_handler(request)
    print(f"  LOG: Completed {result}")
    return result


def auth_middleware(request, next_handler):
    if request.get("authenticated"):
        return next_handler(request)
    return {"error": "Not authenticated"}


def rate_limit_middleware(request, next_handler):
    if request.get("rate_ok", True):
        return next_handler(request)
    return {"error": "Rate limited"}


def handler(request, next_handler):
    return {"status": "success", "data": request.get("data")}


# ============== USAGE ==============
if __name__ == "__main__":
    print("=== Support Ticket Chain ===")
    
    # Build chain
    front = FrontDesk()
    tech = TechSupport()
    eng = Engineering()
    cto = CTO()
    
    front.set_next(tech).set_next(eng).set_next(cto)
    
    tickets = [
        Ticket("low", "Password reset"),
        Ticket("medium", "Can't upload file"),
        Ticket("high", "Database corruption"),
        Ticket("critical", "System down"),
    ]
    
    for ticket in tickets:
        print(front.handle(ticket))
    
    print("\n=== Middleware Chain ===")
    
    pipeline = make_chain(
        logging_middleware,
        auth_middleware,
        rate_limit_middleware,
        handler
    )
    
    print("\nAuthenticated request:")
    result = pipeline({"authenticated": True, "data": "payload"})
    print(f"Result: {result}")
    
    print("\nUnauthenticated request:")
    result = pipeline({"authenticated": False, "data": "payload"})
    print(f"Result: {result}")
```

---

## 4. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Command** | Commands can be passed along chain |
| **Composite** | Parent can be successor |
| **Decorator** | Similar structure but different purpose |

