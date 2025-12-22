# Proxy Pattern in Python

---

## 1. Pattern Name: Proxy (Surrogate)

**Purpose / Problem Solved:**
Provide a surrogate or placeholder for another object to control access to it. Types: Virtual (lazy loading), Protection (access control), Remote (network proxy), Caching, Logging.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT                                    |
|------------------------------------------------------------------|
|  Uses Subject interface                                           |
|  client.request()                                                 |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                    SUBJECT (Interface)                            |
|------------------------------------------------------------------|
| + request()                                                       |
+------------------------------------------------------------------+
         ^                                    ^
         |                                    |
+---------------------+             +---------------------+
|    RealSubject      |             |       PROXY         |
|---------------------|             |---------------------|
| + request()         |             | - real_subject      |
| # Actual logic      |             |---------------------|
+---------------------+             | + request()         |
                                    |   # Pre-processing  |
         ^                          |   real.request()    |
         |                          |   # Post-processing |
         +----------------<---------+---------------------+
                   Proxy holds reference
```

**中文说明：**
代理模式提供一个占位符来控制对另一个对象的访问。根据用途分为：虚拟代理（延迟加载昂贵对象）、保护代理（访问控制）、远程代理（网络调用）、缓存代理、日志代理。代理与真实对象实现相同接口，客户端无法区分。Python中可用`__getattr__`动态转发方法调用。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works |
|----------------|--------------|
| **`__getattr__`** | Automatic delegation to real object |
| **`@property`** | Lazy loading on first access |
| **Descriptors** | Control attribute access |
| **Context managers** | Resource proxies |
| **`functools.cached_property`** | Built-in lazy loading |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Django ORM** | QuerySet is a proxy that delays DB queries |
| **SQLAlchemy** | Lazy loading relationships |
| **Werkzeug** | LocalProxy for thread-local objects |
| **requests** | Session proxies for connection pooling |

---

## 5. Python Module Example

```python
#!/usr/bin/env python3
"""Proxy Pattern - Virtual, Protection, and Caching Proxies"""

from abc import ABC, abstractmethod
from functools import wraps
from typing import Optional
import time


# ============== INTERFACE ==============

class Image(ABC):
    @abstractmethod
    def display(self) -> str:
        pass


# ============== REAL SUBJECT ==============

class HighResImage(Image):
    """Expensive object that takes time to load."""
    
    def __init__(self, filename: str):
        self.filename = filename
        self._load_from_disk()
    
    def _load_from_disk(self):
        print(f"  Loading {self.filename} from disk...")
        time.sleep(0.5)  # Simulate slow loading
    
    def display(self) -> str:
        return f"Displaying {self.filename}"


# ============== VIRTUAL PROXY (Lazy Loading) ==============

class ImageProxy(Image):
    """Lazy loading proxy - only loads image when needed."""
    
    def __init__(self, filename: str):
        self.filename = filename
        self._image: Optional[HighResImage] = None
    
    def display(self) -> str:
        if self._image is None:
            self._image = HighResImage(self.filename)
        return self._image.display()


# ============== PROTECTION PROXY ==============

class ProtectedImage(Image):
    """Access control proxy - checks permissions."""
    
    def __init__(self, image: Image, allowed_users: list):
        self._image = image
        self._allowed = set(allowed_users)
        self._current_user: Optional[str] = None
    
    def set_user(self, user: str):
        self._current_user = user
    
    def display(self) -> str:
        if self._current_user not in self._allowed:
            return f"Access denied for {self._current_user}"
        return self._image.display()


# ============== CACHING PROXY ==============

class CachingProxy(Image):
    """Caching proxy - caches display results."""
    
    def __init__(self, image: Image):
        self._image = image
        self._cache: Optional[str] = None
    
    def display(self) -> str:
        if self._cache is None:
            print("  Cache miss - calling real object")
            self._cache = self._image.display()
        else:
            print("  Cache hit")
        return self._cache
    
    def invalidate(self):
        self._cache = None


# ============== DYNAMIC PROXY WITH __getattr__ ==============

class LoggingProxy:
    """Generic logging proxy using __getattr__."""
    
    def __init__(self, obj, name: str = "object"):
        self._obj = obj
        self._name = name
    
    def __getattr__(self, name):
        attr = getattr(self._obj, name)
        if callable(attr):
            @wraps(attr)
            def wrapper(*args, **kwargs):
                print(f"[LOG] {self._name}.{name}() called")
                result = attr(*args, **kwargs)
                print(f"[LOG] {self._name}.{name}() returned")
                return result
            return wrapper
        return attr


# ============== USAGE ==============
if __name__ == "__main__":
    print("=== Virtual Proxy (Lazy Loading) ===")
    proxy = ImageProxy("photo.jpg")
    print("Proxy created - image not loaded yet")
    print(proxy.display())  # Now loads
    print(proxy.display())  # Uses cached
    
    print("\n=== Protection Proxy ===")
    real = HighResImage("secret.jpg")
    protected = ProtectedImage(real, ["admin", "editor"])
    protected.set_user("guest")
    print(protected.display())
    protected.set_user("admin")
    print(protected.display())
    
    print("\n=== Caching Proxy ===")
    cached = CachingProxy(HighResImage("large.jpg"))
    print(cached.display())
    print(cached.display())
    
    print("\n=== Logging Proxy ===")
    data = [1, 2, 3]
    logged_list = LoggingProxy(data, "mylist")
    logged_list.append(4)
    print(f"Length: {len(data)}")
```

---

## 6. When to Use / Avoid

**Use When:**
- Lazy loading of expensive objects
- Access control needed
- Logging/monitoring method calls
- Caching results

**Avoid When:**
- Direct access is simpler and sufficient

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Decorator** | Adds behavior; Proxy controls access |
| **Adapter** | Changes interface; Proxy keeps same interface |

