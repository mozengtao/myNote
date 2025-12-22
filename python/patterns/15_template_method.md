# Template Method Pattern in Python

---

## 1. Pattern Name: Template Method

**Purpose / Problem Solved:**
Define the skeleton of an algorithm in a base class, letting subclasses override specific steps without changing the algorithm's structure.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                    ABSTRACT CLASS                                 |
|------------------------------------------------------------------|
| + template_method()      # Final - defines algorithm skeleton     |
|     step1()                                                       |
|     step2()              # Can be abstract or have default        |
|     step3()                                                       |
|     hook()               # Optional hook with empty default       |
|------------------------------------------------------------------|
| # step1()                # Concrete - same for all subclasses     |
| + step2()                # Abstract - subclasses must implement   |
| + step3()                # Abstract                               |
| + hook()                 # Optional hook                          |
+------------------------------------------------------------------+
              ^                               ^
              |                               |
+-------------------------+     +-------------------------+
|    ConcreteClass A      |     |    ConcreteClass B      |
|-------------------------|     |-------------------------|
| + step2(): ...          |     | + step2(): ...          |
| + step3(): ...          |     | + step3(): ...          |
| + hook(): ...           |     | (uses default hook)     |
+-------------------------+     +-------------------------+
```

**中文说明：**
模板方法模式在基类中定义算法骨架，将某些步骤延迟到子类实现。父类控制整体流程，子类提供具体实现。"钩子"(hook)是可选的覆盖点，有默认实现。这实现了"好莱坞原则"——Don't call us, we'll call you（父类调用子类方法）。

---

## 3. Python Grammar & Tips

| Feature | Usage |
|---------|-------|
| **`abc.ABC`** | Define abstract base class |
| **`@abstractmethod`** | Force subclass implementation |
| **Default methods** | Hooks with optional override |
| **`super()`** | Call parent implementation if needed |
| **`@final` (3.8+)** | Prevent method override |

---

## 4. Python Module Example

```python
#!/usr/bin/env python3
"""Template Method Pattern - Data Processing Pipeline"""

from abc import ABC, abstractmethod
from typing import List, Any


# ============== ABSTRACT CLASS ==============

class DataProcessor(ABC):
    """
    Abstract class defining data processing template.
    
    Template method defines the algorithm skeleton.
    Subclasses implement the abstract steps.
    """
    
    def process(self, data: Any) -> Any:
        """
        Template method - defines the algorithm.
        
        This method should not be overridden.
        """
        print(f"\n--- Processing with {self.__class__.__name__} ---")
        
        # Step 1: Validate (concrete - same for all)
        if not self._validate(data):
            raise ValueError("Validation failed")
        
        # Step 2: Transform (abstract - subclasses implement)
        transformed = self.transform(data)
        
        # Step 3: Format (abstract)
        formatted = self.format(transformed)
        
        # Step 4: Hook - optional post-processing
        result = self.post_process(formatted)
        
        return result
    
    def _validate(self, data: Any) -> bool:
        """Concrete step - validation logic."""
        print("  Validating data...")
        return data is not None
    
    @abstractmethod
    def transform(self, data: Any) -> Any:
        """Abstract step - must be implemented."""
        pass
    
    @abstractmethod
    def format(self, data: Any) -> str:
        """Abstract step - must be implemented."""
        pass
    
    def post_process(self, data: str) -> str:
        """Hook - optional override with default impl."""
        print("  Post-processing (default: no-op)")
        return data


# ============== CONCRETE IMPLEMENTATIONS ==============

class JsonProcessor(DataProcessor):
    """Processes data as JSON."""
    
    def transform(self, data: Any) -> dict:
        print("  Transforming to dict...")
        if isinstance(data, dict):
            return data
        return {"value": data}
    
    def format(self, data: dict) -> str:
        print("  Formatting as JSON...")
        import json
        return json.dumps(data, indent=2)


class XmlProcessor(DataProcessor):
    """Processes data as XML."""
    
    def transform(self, data: Any) -> dict:
        print("  Transforming to dict...")
        if isinstance(data, dict):
            return data
        return {"value": data}
    
    def format(self, data: dict) -> str:
        print("  Formatting as XML...")
        lines = ["<data>"]
        for k, v in data.items():
            lines.append(f"  <{k}>{v}</{k}>")
        lines.append("</data>")
        return "\n".join(lines)
    
    def post_process(self, data: str) -> str:
        """Override hook to add XML declaration."""
        print("  Post-processing: adding XML header")
        return f'<?xml version="1.0"?>\n{data}'


class CsvProcessor(DataProcessor):
    """Processes data as CSV."""
    
    def transform(self, data: Any) -> List[dict]:
        print("  Transforming to list of dicts...")
        if isinstance(data, list):
            return data
        return [{"value": data}]
    
    def format(self, data: List[dict]) -> str:
        print("  Formatting as CSV...")
        if not data:
            return ""
        headers = list(data[0].keys())
        lines = [",".join(headers)]
        for row in data:
            lines.append(",".join(str(row.get(h, "")) for h in headers))
        return "\n".join(lines)


# ============== FUNCTIONAL TEMPLATE ==============

def make_processor(transform_fn, format_fn, post_fn=None):
    """Factory for simple processor functions."""
    def process(data):
        transformed = transform_fn(data)
        formatted = format_fn(transformed)
        if post_fn:
            return post_fn(formatted)
        return formatted
    return process


# ============== USAGE ==============
if __name__ == "__main__":
    data = {"name": "Alice", "age": 30, "city": "NYC"}
    
    print("=== Template Method Pattern ===")
    
    processors = [JsonProcessor(), XmlProcessor(), CsvProcessor()]
    
    for processor in processors:
        result = processor.process(data)
        print(f"Result:\n{result}")
    
    print("\n=== Functional Template ===")
    
    yaml_processor = make_processor(
        transform_fn=lambda d: d if isinstance(d, dict) else {"v": d},
        format_fn=lambda d: "\n".join(f"{k}: {v}" for k, v in d.items()),
        post_fn=lambda s: f"---\n{s}\n..."
    )
    
    print(yaml_processor(data))
```

---

## 5. When to Use / Avoid

**Use When:**
- Subclasses should extend only particular steps
- You want to control what subclasses can override
- Multiple classes share common algorithm structure

**Avoid When:**
- Algorithm doesn't need customization
- Inheritance hierarchy becomes too complex

---

## 6. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Strategy** | Composition over inheritance alternative |
| **Factory Method** | Template method that creates objects |

