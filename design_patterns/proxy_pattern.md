# Proxy Pattern (代理模式)

## ASCII Diagram

```
+-------------------+     +-------------------+
|      Client       |     |     Subject       |<<interface>>
+-------------------+     +-------------------+
| + doWork()        |---->| + request()       |
+-------------------+     +-------------------+
                                   ^
                                   |
                     +-------------+-------------+
                     |                           |
             +-------+-------+           +-------+-------+
             |     Proxy     |           |  RealSubject  |
             +---------------+           +---------------+
             | - realSubject |---------->| + request()   |
             +---------------+           +---------------+
             | + request()   |
             | + checkAccess()|
             | + logAccess() |
             +---------------+

Types of Proxies:
+------------------+     +------------------+     +------------------+
|  Virtual Proxy   |     | Protection Proxy |     |   Remote Proxy   |
+------------------+     +------------------+     +------------------+
| Lazy loading     |     | Access control   |     | Network access   |
| Resource saving  |     | Permission check |     | Location hiding  |
+------------------+     +------------------+     +------------------+

+------------------+     +------------------+
|  Caching Proxy   |     |  Logging Proxy   |
+------------------+     +------------------+
| Cache results    |     | Log requests     |
| Improve perf     |     | Audit trail      |
+------------------+     +------------------+
```

**中文说明：**
- **Subject（抽象主题）**：定义真实对象和代理的公共接口
- **RealSubject（真实主题）**：实现具体业务逻辑
- **Proxy（代理）**：持有真实对象的引用，控制对其的访问
- **关键点**：代理和真实对象实现相同接口，客户端无感知

---

## 核心思想

为另一个对象提供一个**代理**或**占位符**，以控制对它的访问。代理在客户端和目标对象之间起中介作用，可以在访问前后添加额外的处理。

**代理类型：**
- **虚拟代理**：延迟创建昂贵对象
- **保护代理**：控制访问权限
- **远程代理**：访问远程对象
- **缓存代理**：缓存请求结果
- **日志代理**：记录访问日志

---

## 应用场景

1. **延迟加载**：大对象的懒加载（图片、视频）
2. **访问控制**：权限检查
3. **远程访问**：封装网络通信细节
4. **缓存**：缓存昂贵操作的结果
5. **实际应用**：
   - 图片懒加载
   - ORM 延迟加载关联对象
   - API 网关
   - 智能引用（引用计数）

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 控制访问 | 可以在访问前后添加逻辑 |
| 延迟初始化 | 按需创建昂贵对象 |
| 透明性 | 客户端无需知道使用的是代理 |
| 职责分离 | 将访问控制与业务逻辑分离 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 响应延迟 | 增加一层间接调用 |
| 代码复杂 | 需要额外的代理类 |
| 可能过度使用 | 简单场景不需要代理 |

---

## Python 代码示例

### 应用前：直接访问对象

```python
import time


class LargeImage:
    """大图片 - 加载很慢"""
    
    def __init__(self, filename):
        self.filename = filename
        self._load_from_disk()  # 构造时就加载，即使不需要显示
    
    def _load_from_disk(self):
        print(f"Loading large image: {self.filename}")
        time.sleep(2)  # 模拟耗时加载
        self.data = f"[Image data of {self.filename}]"
        print(f"Image loaded: {len(self.data)} bytes")
    
    def display(self):
        print(f"Displaying: {self.filename}")
        return self.data


class SecretDocument:
    """机密文档 - 没有访问控制"""
    
    def __init__(self, content):
        self.content = content
    
    def read(self):
        return self.content
    
    def write(self, new_content):
        self.content = new_content
        return "Document updated"


class RemoteService:
    """远程服务 - 没有错误处理和重试"""
    
    def fetch_data(self, query):
        print(f"Fetching: {query}")
        # 可能失败，没有重试机制
        if "error" in query:
            raise ConnectionError("Network error")
        return f"Data for {query}"


# 问题演示
print("=== Problem 1: No lazy loading ===")
# 即使不显示图片，也会加载
images = [LargeImage(f"photo{i}.jpg") for i in range(3)]
# 只显示第一张
images[0].display()

print("\n=== Problem 2: No access control ===")
doc = SecretDocument("Top Secret: Launch codes...")
# 任何人都能读写
print(doc.read())
doc.write("Hacked!")

print("\n=== Problem 3: No error handling ===")
service = RemoteService()
try:
    service.fetch_data("error_query")
except ConnectionError as e:
    print(f"Failed: {e}")  # 没有重试
```

### 应用后：使用代理模式

```python
from abc import ABC, abstractmethod
import time
from functools import wraps
from typing import Optional, Any, Callable
import hashlib


# ========== 抽象主题 ==========
class Image(ABC):
    """图片接口"""
    
    @abstractmethod
    def display(self) -> str:
        pass
    
    @abstractmethod
    def get_info(self) -> dict:
        pass


class Document(ABC):
    """文档接口"""
    
    @abstractmethod
    def read(self) -> str:
        pass
    
    @abstractmethod
    def write(self, content: str) -> str:
        pass


class DataService(ABC):
    """数据服务接口"""
    
    @abstractmethod
    def fetch_data(self, query: str) -> str:
        pass


# ========== 真实主题 ==========
class RealImage(Image):
    """真实图片对象 - 加载耗时"""
    
    def __init__(self, filename: str):
        self.filename = filename
        self.data = None
        self._load()
    
    def _load(self):
        print(f"  [RealImage] Loading {self.filename} from disk...")
        time.sleep(1)  # 模拟耗时
        self.data = f"[Pixel data of {self.filename}]"
        self.size = len(self.data)
        print(f"  [RealImage] Loaded: {self.size} bytes")
    
    def display(self) -> str:
        print(f"  [RealImage] Displaying {self.filename}")
        return self.data
    
    def get_info(self) -> dict:
        return {
            "filename": self.filename,
            "size": self.size,
            "loaded": True
        }


class RealDocument(Document):
    """真实文档对象"""
    
    def __init__(self, content: str):
        self.content = content
    
    def read(self) -> str:
        return self.content
    
    def write(self, content: str) -> str:
        self.content = content
        return "Document updated successfully"


class RealDataService(DataService):
    """真实数据服务"""
    
    def fetch_data(self, query: str) -> str:
        print(f"  [RealService] Fetching: {query}")
        time.sleep(0.5)  # 模拟网络延迟
        if "error" in query:
            raise ConnectionError("Network error")
        return f"Result for '{query}'"


# ========== 代理类 ==========

# 1. 虚拟代理 - 延迟加载
class ImageProxy(Image):
    """
    图片代理 - 延迟加载
    
    只在真正需要显示时才加载图片
    """
    
    def __init__(self, filename: str):
        self.filename = filename
        self._real_image: Optional[RealImage] = None
    
    def _get_real_image(self) -> RealImage:
        """延迟创建真实对象"""
        if self._real_image is None:
            print(f"[ImageProxy] Creating real image for {self.filename}")
            self._real_image = RealImage(self.filename)
        return self._real_image
    
    def display(self) -> str:
        # 只在显示时才加载
        return self._get_real_image().display()
    
    def get_info(self) -> dict:
        if self._real_image is None:
            # 不加载也能返回基本信息
            return {
                "filename": self.filename,
                "size": "unknown",
                "loaded": False
            }
        return self._real_image.get_info()


# 2. 保护代理 - 访问控制
class User:
    def __init__(self, name: str, role: str):
        self.name = name
        self.role = role


class ProtectedDocumentProxy(Document):
    """
    保护代理 - 访问控制
    
    根据用户角色控制读写权限
    """
    
    def __init__(self, document: Document, current_user: User):
        self._document = document
        self._user = current_user
        self._access_log = []
    
    def _log_access(self, action: str, success: bool):
        entry = {
            "user": self._user.name,
            "role": self._user.role,
            "action": action,
            "success": success,
            "time": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        self._access_log.append(entry)
        status = "✓" if success else "✗"
        print(f"[AccessLog] {status} {self._user.name} ({self._user.role}) -> {action}")
    
    def read(self) -> str:
        # 所有人都能读
        self._log_access("READ", True)
        return self._document.read()
    
    def write(self, content: str) -> str:
        # 只有 admin 能写
        if self._user.role != "admin":
            self._log_access("WRITE", False)
            raise PermissionError(
                f"User '{self._user.name}' does not have write permission"
            )
        self._log_access("WRITE", True)
        return self._document.write(content)
    
    def get_access_log(self) -> list:
        return self._access_log.copy()


# 3. 缓存代理
class CachingServiceProxy(DataService):
    """
    缓存代理
    
    缓存请求结果，避免重复的网络调用
    """
    
    def __init__(self, service: DataService, cache_ttl: float = 60.0):
        self._service = service
        self._cache = {}  # query_hash -> (result, timestamp)
        self._ttl = cache_ttl
        self._stats = {"hits": 0, "misses": 0}
    
    def _get_cache_key(self, query: str) -> str:
        return hashlib.md5(query.encode()).hexdigest()
    
    def fetch_data(self, query: str) -> str:
        key = self._get_cache_key(query)
        current_time = time.time()
        
        # 检查缓存
        if key in self._cache:
            result, timestamp = self._cache[key]
            if current_time - timestamp < self._ttl:
                self._stats["hits"] += 1
                print(f"[CacheProxy] Cache HIT for '{query}'")
                return result
        
        # 缓存未命中
        self._stats["misses"] += 1
        print(f"[CacheProxy] Cache MISS for '{query}'")
        result = self._service.fetch_data(query)
        self._cache[key] = (result, current_time)
        return result
    
    def get_stats(self) -> dict:
        total = self._stats["hits"] + self._stats["misses"]
        hit_rate = self._stats["hits"] / total if total > 0 else 0
        return {**self._stats, "hit_rate": f"{hit_rate:.2%}"}
    
    def clear_cache(self):
        self._cache.clear()
        print("[CacheProxy] Cache cleared")


# 4. 重试代理
class RetryServiceProxy(DataService):
    """
    重试代理
    
    失败时自动重试
    """
    
    def __init__(self, service: DataService, 
                 max_retries: int = 3,
                 delay: float = 1.0,
                 backoff: float = 2.0):
        self._service = service
        self._max_retries = max_retries
        self._delay = delay
        self._backoff = backoff
    
    def fetch_data(self, query: str) -> str:
        last_error = None
        delay = self._delay
        
        for attempt in range(self._max_retries):
            try:
                print(f"[RetryProxy] Attempt {attempt + 1}/{self._max_retries}")
                return self._service.fetch_data(query)
            except Exception as e:
                last_error = e
                print(f"[RetryProxy] Failed: {e}")
                if attempt < self._max_retries - 1:
                    print(f"[RetryProxy] Waiting {delay:.1f}s before retry...")
                    time.sleep(delay)
                    delay *= self._backoff  # 指数退避
        
        raise last_error


# 5. 日志代理
class LoggingServiceProxy(DataService):
    """
    日志代理
    
    记录所有请求和响应
    """
    
    def __init__(self, service: DataService, logger_name: str = "SERVICE"):
        self._service = service
        self._logger_name = logger_name
    
    def fetch_data(self, query: str) -> str:
        print(f"[{self._logger_name}] >>> Request: {query}")
        start = time.perf_counter()
        
        try:
            result = self._service.fetch_data(query)
            elapsed = time.perf_counter() - start
            print(f"[{self._logger_name}] <<< Response ({elapsed:.3f}s): {result[:50]}...")
            return result
        except Exception as e:
            elapsed = time.perf_counter() - start
            print(f"[{self._logger_name}] !!! Error ({elapsed:.3f}s): {e}")
            raise


# ========== 组合代理 ==========
def create_robust_service(base_service: DataService) -> DataService:
    """创建一个健壮的服务代理链"""
    # 从内到外：重试 -> 缓存 -> 日志
    service = base_service
    service = RetryServiceProxy(service, max_retries=2, delay=0.5)
    service = CachingServiceProxy(service, cache_ttl=300)
    service = LoggingServiceProxy(service, "API")
    return service


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    print("=" * 60)
    print("1. Virtual Proxy - Lazy Loading")
    print("=" * 60)
    
    # 创建多个图片代理，不会立即加载
    print("\nCreating image proxies (no loading yet):")
    images = [ImageProxy(f"photo{i}.jpg") for i in range(5)]
    
    print(f"\nImage info (still not loaded):")
    for img in images[:2]:
        print(f"  {img.get_info()}")
    
    print(f"\nDisplaying only first image (now it loads):")
    images[0].display()
    
    print(f"\nImage info after display:")
    print(f"  Image 0: {images[0].get_info()}")
    print(f"  Image 1: {images[1].get_info()}")  # Still not loaded
    
    print("\n" + "=" * 60)
    print("2. Protection Proxy - Access Control")
    print("=" * 60)
    
    # 创建文档
    secret_doc = RealDocument("Classified: Project Alpha details...")
    
    # 普通用户
    normal_user = User("alice", "viewer")
    proxy_for_alice = ProtectedDocumentProxy(secret_doc, normal_user)
    
    print("\nAlice (viewer) trying to read:")
    print(f"  Content: {proxy_for_alice.read()}")
    
    print("\nAlice (viewer) trying to write:")
    try:
        proxy_for_alice.write("Hacked!")
    except PermissionError as e:
        print(f"  Error: {e}")
    
    # 管理员
    admin_user = User("bob", "admin")
    proxy_for_bob = ProtectedDocumentProxy(secret_doc, admin_user)
    
    print("\nBob (admin) trying to write:")
    result = proxy_for_bob.write("Updated classified info")
    print(f"  Result: {result}")
    
    print("\nAccess log:")
    for entry in proxy_for_alice.get_access_log():
        print(f"  {entry}")
    
    print("\n" + "=" * 60)
    print("3. Caching Proxy")
    print("=" * 60)
    
    real_service = RealDataService()
    cached_service = CachingServiceProxy(real_service, cache_ttl=60)
    
    print("\nFirst request (cache miss):")
    cached_service.fetch_data("SELECT * FROM users")
    
    print("\nSame request (cache hit):")
    cached_service.fetch_data("SELECT * FROM users")
    
    print("\nDifferent request (cache miss):")
    cached_service.fetch_data("SELECT * FROM products")
    
    print(f"\nCache stats: {cached_service.get_stats()}")
    
    print("\n" + "=" * 60)
    print("4. Composite Proxy Chain")
    print("=" * 60)
    
    robust_service = create_robust_service(RealDataService())
    
    print("\nUsing robust service:")
    result = robust_service.fetch_data("important_query")
    print(f"Result: {result}")
    
    print("\nSame query (should be cached):")
    result = robust_service.fetch_data("important_query")


# ========== Python 动态代理 ==========
print("\n" + "=" * 60)
print("5. Python Dynamic Proxy")
print("=" * 60)


class DynamicProxy:
    """
    动态代理 - 拦截所有方法调用
    """
    
    def __init__(self, target: Any, handler: Callable):
        self._target = target
        self._handler = handler
    
    def __getattr__(self, name: str):
        attr = getattr(self._target, name)
        if callable(attr):
            @wraps(attr)
            def wrapper(*args, **kwargs):
                return self._handler(self._target, name, attr, args, kwargs)
            return wrapper
        return attr


def logging_handler(target, method_name, method, args, kwargs):
    """日志处理器"""
    print(f"[DynamicProxy] Calling {type(target).__name__}.{method_name}")
    print(f"  args: {args}, kwargs: {kwargs}")
    result = method(*args, **kwargs)
    print(f"  result: {result}")
    return result


# 使用动态代理
class Calculator:
    def add(self, a, b):
        return a + b
    
    def multiply(self, a, b):
        return a * b


calc = Calculator()
proxy_calc = DynamicProxy(calc, logging_handler)

proxy_calc.add(2, 3)
proxy_calc.multiply(4, 5)
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **资源使用** | 立即加载所有对象 | 按需延迟加载 |
| **访问控制** | 无法控制访问权限 | 可以精细控制 |
| **错误处理** | 客户端处理重试逻辑 | 代理封装重试 |
| **性能优化** | 每次都请求后端 | 代理缓存结果 |
| **日志审计** | 分散在各处 | 代理集中记录 |
| **透明性** | - | 客户端无需修改 |

---

## 代理 vs 装饰器 vs 适配器

| 模式 | 目的 | 接口 |
|------|------|------|
| **Proxy** | 控制访问 | 相同 |
| **Decorator** | 增强功能 | 相同 |
| **Adapter** | 转换接口 | 不同 |

```python
# Proxy: 控制对 RealSubject 的访问
proxy = Proxy(real_subject)

# Decorator: 给 Component 添加功能
decorated = Decorator(component)

# Adapter: 转换 Adaptee 的接口
adapter = Adapter(adaptee)
```

