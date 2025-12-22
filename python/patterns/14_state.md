# State Pattern in Python

---

## 1. Pattern Name: State

**Purpose / Problem Solved:**
Allow an object to alter its behavior when its internal state changes. The object will appear to change its class. Avoids large conditional statements based on state.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CONTEXT                                   |
|------------------------------------------------------------------|
| - state: State                                                    |
|------------------------------------------------------------------|
| + request()                                                       |
|     state.handle(self)                                            |
| + set_state(state: State)                                         |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                      STATE (Interface)                            |
|------------------------------------------------------------------|
| + handle(context: Context)                                        |
+------------------------------------------------------------------+
         ^                    ^                    ^
         |                    |                    |
+-----------------+  +-----------------+  +-----------------+
|  ConcreteStateA |  |  ConcreteStateB |  |  ConcreteStateC |
|-----------------|  |-----------------|  |-----------------|
| handle(ctx):    |  | handle(ctx):    |  | handle(ctx):    |
|   # behavior A  |  |   # behavior B  |  |   # behavior C  |
|   ctx.set_state |  |   ctx.set_state |  |   ctx.set_state |
|   (StateB())    |  |   (StateC())    |  |   (StateA())    |
+-----------------+  +-----------------+  +-----------------+
```

**中文说明：**
状态模式允许对象在内部状态改变时改变行为。每个状态封装为独立类，状态转换逻辑分散在各状态类中。这避免了大量if-else语句，符合开闭原则。常用于：工作流引擎、游戏角色状态、TCP连接状态、UI状态机。

---

## 3. Python Module Example

```python
#!/usr/bin/env python3
"""State Pattern - Document Workflow Example"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Optional


# ============== STATE INTERFACE ==============

class DocumentState(ABC):
    """Abstract state for document workflow."""
    
    @property
    @abstractmethod
    def name(self) -> str:
        pass
    
    @abstractmethod
    def edit(self, doc: "Document") -> str:
        pass
    
    @abstractmethod
    def review(self, doc: "Document") -> str:
        pass
    
    @abstractmethod
    def publish(self, doc: "Document") -> str:
        pass
    
    @abstractmethod
    def reject(self, doc: "Document") -> str:
        pass


# ============== CONCRETE STATES ==============

class DraftState(DocumentState):
    name = "Draft"
    
    def edit(self, doc):
        return "Editing draft..."
    
    def review(self, doc):
        doc.state = ReviewState()
        return "Submitted for review"
    
    def publish(self, doc):
        return "Cannot publish draft - submit for review first"
    
    def reject(self, doc):
        return "Cannot reject a draft"


class ReviewState(DocumentState):
    name = "In Review"
    
    def edit(self, doc):
        return "Cannot edit while in review"
    
    def review(self, doc):
        return "Already in review"
    
    def publish(self, doc):
        doc.state = PublishedState()
        return "Document published!"
    
    def reject(self, doc):
        doc.state = DraftState()
        return "Rejected - returned to draft"


class PublishedState(DocumentState):
    name = "Published"
    
    def edit(self, doc):
        doc.state = DraftState()
        return "Creating new draft from published"
    
    def review(self, doc):
        return "Published docs don't need review"
    
    def publish(self, doc):
        return "Already published"
    
    def reject(self, doc):
        doc.state = DraftState()
        return "Unpublished - returned to draft"


# ============== CONTEXT ==============

@dataclass
class Document:
    """Context that changes behavior based on state."""
    title: str
    state: DocumentState = None
    
    def __post_init__(self):
        if self.state is None:
            self.state = DraftState()
    
    def status(self) -> str:
        return f"'{self.title}' is in {self.state.name} state"
    
    def edit(self) -> str:
        result = self.state.edit(self)
        return f"[Edit] {result}"
    
    def submit_for_review(self) -> str:
        result = self.state.review(self)
        return f"[Review] {result}"
    
    def publish(self) -> str:
        result = self.state.publish(self)
        return f"[Publish] {result}"
    
    def reject(self) -> str:
        result = self.state.reject(self)
        return f"[Reject] {result}"


# ============== SIMPLE STATE MACHINE ==============

class StateMachine:
    """Simplified state machine for enumerable states."""
    
    def __init__(self, states: dict, initial: str):
        self.states = states  # {state_name: {event: next_state}}
        self.current = initial
    
    def trigger(self, event: str) -> bool:
        transitions = self.states.get(self.current, {})
        if event in transitions:
            old = self.current
            self.current = transitions[event]
            print(f"  {old} --[{event}]--> {self.current}")
            return True
        print(f"  Cannot {event} from {self.current}")
        return False


# ============== USAGE ==============
if __name__ == "__main__":
    print("=== Document Workflow ===")
    doc = Document("Report Q4")
    
    print(doc.status())
    print(doc.edit())
    print(doc.submit_for_review())
    print(doc.status())
    print(doc.edit())  # Can't edit in review
    print(doc.publish())
    print(doc.status())
    print(doc.edit())  # Creates new draft
    print(doc.status())
    
    print("\n=== Simple State Machine ===")
    traffic_light = StateMachine(
        states={
            "red": {"timer": "green"},
            "green": {"timer": "yellow"},
            "yellow": {"timer": "red"},
        },
        initial="red"
    )
    
    for _ in range(5):
        traffic_light.trigger("timer")
```

---

## 4. When to Use / Avoid

**Use When:**
- Object behavior depends on state
- Many conditionals based on state
- State transitions are well-defined

**Avoid When:**
- Only a few states with simple logic
- State changes rarely happen

---

## 5. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Strategy** | Similar structure; Strategy chosen by client, State chosen internally |
| **Singleton** | State objects often are singletons |

