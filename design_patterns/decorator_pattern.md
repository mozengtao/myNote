# Decorator Pattern (装饰器模式)

## ASCII Diagram

```
+-------------------+
|     Component     |<<interface>>
+-------------------+
| + operation()     |
+--------+----------+
         ^
         |
    +----+----+
    |         |
+---+------+  +--------+--------+
|Concrete  |  |    Decorator    |<<abstract>>
|Component |  +-----------------+
+----------+  | - component     |-----> Component
| +operation| +-----------------+
+----------+  | + operation()   |
              +--------+--------+
                       ^
                       |
              +--------+--------+
              |                 |
      +-------+------+  +-------+------+
      |DecoratorA    |  |DecoratorB    |
      +--------------+  +--------------+
      | + operation()|  | + operation()|
      | + addedBehavA|  | + addedBehavB|
      +--------------+  +--------------+

Wrapping Process:
+-------------+     +--------------+     +--------------+
| Component   |<----| DecoratorA   |<----| DecoratorB   |
+-------------+     +--------------+     +--------------+
| operation() |     | operation()  |     | operation()  |
+-------------+     | + extra A    |     | + extra B    |
                    +--------------+     +--------------+

Call Flow: DecoratorB.operation()
          -> DecoratorA.operation()
          -> Component.operation()
          -> return + extra A
          -> return + extra B
```

**中文说明：**
- **Component（组件接口）**：定义对象的接口
- **ConcreteComponent（具体组件）**：被装饰的原始对象
- **Decorator（装饰器抽象类）**：持有组件引用，实现组件接口
- **ConcreteDecorator（具体装饰器）**：添加额外的功能
- **关键点**：通过包装而非继承来扩展功能，可以多层嵌套

---

## 核心思想

动态地给对象添加一些额外的职责。装饰器模式相比继承更加灵活，可以在运行时按需组合功能。通过递归组合，可以实现功能的任意叠加。

**与继承的区别：**
- 继承：静态扩展，编译时确定
- 装饰器：动态扩展，运行时组合

---

## 应用场景

1. **动态添加功能**：需要在运行时为对象添加功能
2. **功能组合**：需要灵活组合多个功能
3. **保持接口不变**：扩展功能但不改变原有接口
4. **实际应用**：
   - I/O 流（BufferedReader 装饰 FileReader）
   - Web 中间件（日志、认证、压缩）
   - 缓存层
   - 数据验证层
   - Python 的 @decorator 语法

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 灵活组合 | 可以任意组合装饰器 |
| 符合开闭原则 | 新增功能无需修改现有代码 |
| 单一职责 | 每个装饰器只负责一个功能 |
| 运行时扩展 | 可以动态添加/移除功能 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 对象数量增加 | 可能产生很多小对象 |
| 调试困难 | 多层装饰时调试复杂 |
| 顺序敏感 | 装饰器顺序可能影响结果 |

---

## Python 代码示例

### 应用前：功能耦合或继承爆炸

```python
# 问题 1：功能耦合在一个类中

class DataProcessor:
    """数据处理器 - 所有功能耦合在一起"""
    
    def __init__(self, enable_logging=True, enable_validation=True, 
                 enable_caching=True, enable_encryption=False):
        self.enable_logging = enable_logging
        self.enable_validation = enable_validation
        self.enable_caching = enable_caching
        self.enable_encryption = enable_encryption
        self._cache = {}
    
    def process(self, data):
        # 日志
        if self.enable_logging:
            print(f"[LOG] Processing: {data}")
        
        # 验证
        if self.enable_validation:
            if not data:
                raise ValueError("Data cannot be empty")
        
        # 缓存检查
        if self.enable_caching:
            if data in self._cache:
                print("[CACHE] Hit!")
                return self._cache[data]
        
        # 实际处理
        result = f"Processed: {data}"
        
        # 加密
        if self.enable_encryption:
            result = f"ENCRYPTED({result})"
        
        # 缓存存储
        if self.enable_caching:
            self._cache[data] = result
        
        # 日志
        if self.enable_logging:
            print(f"[LOG] Result: {result}")
        
        return result


# 问题 2：继承爆炸

class BasicProcessor:
    def process(self, data):
        return f"Processed: {data}"

class LoggingProcessor(BasicProcessor):
    def process(self, data):
        print(f"[LOG] Processing: {data}")
        result = super().process(data)
        print(f"[LOG] Result: {result}")
        return result

class ValidatingProcessor(BasicProcessor):
    def process(self, data):
        if not data:
            raise ValueError("Data cannot be empty")
        return super().process(data)

class LoggingValidatingProcessor(LoggingProcessor, ValidatingProcessor):
    pass

class CachingLoggingProcessor(LoggingProcessor):
    def __init__(self):
        self._cache = {}
    
    def process(self, data):
        if data in self._cache:
            return self._cache[data]
        result = super().process(data)
        self._cache[data] = result
        return result

# 还需要更多组合...
# CachingValidatingProcessor, LoggingCachingValidatingProcessor, ...
```

### 应用后：使用装饰器模式

```python
from abc import ABC, abstractmethod
from functools import wraps
from typing import Any, Optional
import time
import hashlib


# ========== 组件接口 ==========
class DataProcessor(ABC):
    """数据处理器接口"""
    
    @abstractmethod
    def process(self, data: Any) -> Any:
        pass


# ========== 具体组件 ==========
class BasicDataProcessor(DataProcessor):
    """基本数据处理器"""
    
    def process(self, data: Any) -> Any:
        # 核心处理逻辑
        return f"Processed: {data}"


class JSONDataProcessor(DataProcessor):
    """JSON 数据处理器"""
    
    def process(self, data: Any) -> Any:
        import json
        if isinstance(data, str):
            return json.loads(data)
        return json.dumps(data)


# ========== 装饰器抽象类 ==========
class ProcessorDecorator(DataProcessor):
    """处理器装饰器基类"""
    
    def __init__(self, processor: DataProcessor):
        self._processor = processor
    
    def process(self, data: Any) -> Any:
        return self._processor.process(data)


# ========== 经典装饰器 ==========
class LoggingDecorator(ProcessorDecorator):
    """日志装饰器"""
    
    def __init__(self, processor: DataProcessor, logger_name: str = "DEFAULT"):
        super().__init__(processor)
        self.logger_name = logger_name
    
    def process(self, data: Any) -> Any:
        print(f"[{self.logger_name}] Input: {data}")
        result = self._processor.process(data)
        print(f"[{self.logger_name}] Output: {result}")
        return result


class ValidationDecorator(ProcessorDecorator):
    """验证装饰器"""
    
    def __init__(self, processor: DataProcessor, 
                 allow_empty: bool = False, 
                 max_length: Optional[int] = None):
        super().__init__(processor)
        self.allow_empty = allow_empty
        self.max_length = max_length
    
    def process(self, data: Any) -> Any:
        # 空值检查
        if not self.allow_empty and not data:
            raise ValueError("Data cannot be empty")
        
        # 长度检查
        if self.max_length and len(str(data)) > self.max_length:
            raise ValueError(f"Data exceeds max length of {self.max_length}")
        
        return self._processor.process(data)


class TimingDecorator(ProcessorDecorator):
    """计时装饰器"""
    
    def process(self, data: Any) -> Any:
        start = time.perf_counter()
        result = self._processor.process(data)
        elapsed = time.perf_counter() - start
        print(f"[TIMING] Processing took {elapsed:.4f}s")
        return result


class RetryDecorator(ProcessorDecorator):
    """重试装饰器"""
    
    def __init__(self, processor: DataProcessor, 
                 max_retries: int = 3, 
                 delay: float = 1.0):
        super().__init__(processor)
        self.max_retries = max_retries
        self.delay = delay
    
    def process(self, data: Any) -> Any:
        last_error = None
        for attempt in range(self.max_retries):
            try:
                return self._processor.process(data)
            except Exception as e:
                last_error = e
                print(f"[RETRY] Attempt {attempt + 1} failed: {e}")
                if attempt < self.max_retries - 1:
                    time.sleep(self.delay)
        raise last_error


# ========== 有状态装饰器 ==========
class CachingDecorator(ProcessorDecorator):
    """缓存装饰器（有状态）"""
    
    def __init__(self, processor: DataProcessor, ttl: float = 60.0):
        super().__init__(processor)
        self._cache = {}  # key -> (value, timestamp)
        self._ttl = ttl
        self._hits = 0
        self._misses = 0
    
    def _get_cache_key(self, data: Any) -> str:
        return hashlib.md5(str(data).encode()).hexdigest()
    
    def process(self, data: Any) -> Any:
        key = self._get_cache_key(data)
        current_time = time.time()
        
        # 检查缓存
        if key in self._cache:
            value, timestamp = self._cache[key]
            if current_time - timestamp < self._ttl:
                self._hits += 1
                print(f"[CACHE] Hit! (hits: {self._hits})")
                return value
        
        # 缓存未命中
        self._misses += 1
        print(f"[CACHE] Miss (misses: {self._misses})")
        result = self._processor.process(data)
        self._cache[key] = (result, current_time)
        return result
    
    def get_stats(self) -> dict:
        total = self._hits + self._misses
        hit_rate = self._hits / total if total > 0 else 0
        return {
            "hits": self._hits,
            "misses": self._misses,
            "hit_rate": f"{hit_rate:.2%}",
            "cache_size": len(self._cache)
        }
    
    def clear_cache(self):
        self._cache.clear()
        print("[CACHE] Cleared")


class RateLimitDecorator(ProcessorDecorator):
    """限流装饰器（有状态）"""
    
    def __init__(self, processor: DataProcessor, 
                 max_calls: int = 10, 
                 window_seconds: float = 60.0):
        super().__init__(processor)
        self._max_calls = max_calls
        self._window = window_seconds
        self._calls = []  # 调用时间戳列表
    
    def process(self, data: Any) -> Any:
        current_time = time.time()
        
        # 清理过期记录
        self._calls = [t for t in self._calls 
                      if current_time - t < self._window]
        
        # 检查限制
        if len(self._calls) >= self._max_calls:
            wait_time = self._window - (current_time - self._calls[0])
            raise RuntimeError(
                f"Rate limit exceeded. Try again in {wait_time:.1f}s"
            )
        
        self._calls.append(current_time)
        return self._processor.process(data)


class MetricsDecorator(ProcessorDecorator):
    """指标收集装饰器（有状态）"""
    
    def __init__(self, processor: DataProcessor):
        super().__init__(processor)
        self._total_calls = 0
        self._successful_calls = 0
        self._failed_calls = 0
        self._total_time = 0.0
    
    def process(self, data: Any) -> Any:
        self._total_calls += 1
        start = time.perf_counter()
        
        try:
            result = self._processor.process(data)
            self._successful_calls += 1
            return result
        except Exception as e:
            self._failed_calls += 1
            raise
        finally:
            self._total_time += time.perf_counter() - start
    
    def get_metrics(self) -> dict:
        avg_time = (self._total_time / self._total_calls 
                   if self._total_calls > 0 else 0)
        success_rate = (self._successful_calls / self._total_calls 
                       if self._total_calls > 0 else 0)
        return {
            "total_calls": self._total_calls,
            "successful": self._successful_calls,
            "failed": self._failed_calls,
            "success_rate": f"{success_rate:.2%}",
            "avg_time": f"{avg_time:.4f}s",
            "total_time": f"{self._total_time:.4f}s"
        }


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    print("=" * 60)
    print("1. Basic Usage - Single Decorator")
    print("=" * 60)
    
    processor = BasicDataProcessor()
    logged = LoggingDecorator(processor, "MYAPP")
    result = logged.process("Hello World")
    print(f"Result: {result}\n")
    
    print("=" * 60)
    print("2. Chained Decorators")
    print("=" * 60)
    
    # 装饰器链：验证 -> 日志 -> 计时 -> 基础处理
    processor = BasicDataProcessor()
    processor = TimingDecorator(processor)
    processor = LoggingDecorator(processor)
    processor = ValidationDecorator(processor, max_length=100)
    
    result = processor.process("Test Data")
    print(f"Result: {result}\n")
    
    print("=" * 60)
    print("3. Caching Decorator (Stateful)")
    print("=" * 60)
    
    base = BasicDataProcessor()
    cached = CachingDecorator(base, ttl=300)
    
    # 第一次调用 - 缓存未命中
    result1 = cached.process("same data")
    
    # 第二次调用 - 缓存命中
    result2 = cached.process("same data")
    
    # 第三次调用 - 不同数据
    result3 = cached.process("different data")
    
    print(f"Cache stats: {cached.get_stats()}\n")
    
    print("=" * 60)
    print("4. Metrics Collection")
    print("=" * 60)
    
    base = BasicDataProcessor()
    metrics = MetricsDecorator(base)
    timed = TimingDecorator(metrics)
    
    for i in range(5):
        timed.process(f"data_{i}")
    
    print(f"Metrics: {metrics.get_metrics()}\n")
    
    print("=" * 60)
    print("5. Flexible Composition")
    print("=" * 60)
    
    def create_production_processor():
        """生产环境处理器"""
        processor = BasicDataProcessor()
        processor = CachingDecorator(processor, ttl=3600)
        processor = RetryDecorator(processor, max_retries=3)
        processor = MetricsDecorator(processor)
        processor = LoggingDecorator(processor, "PROD")
        return processor
    
    def create_debug_processor():
        """调试环境处理器"""
        processor = BasicDataProcessor()
        processor = TimingDecorator(processor)
        processor = LoggingDecorator(processor, "DEBUG")
        return processor
    
    prod = create_production_processor()
    debug = create_debug_processor()
    
    print("Production:")
    prod.process("production data")
    
    print("\nDebug:")
    debug.process("debug data")


# ========== Python 函数装饰器风格 ==========
print("\n" + "=" * 60)
print("6. Python Function Decorator Style")
print("=" * 60)


def logging_decorator(func):
    """函数日志装饰器"""
    @wraps(func)
    def wrapper(*args, **kwargs):
        print(f"[LOG] Calling {func.__name__} with {args}, {kwargs}")
        result = func(*args, **kwargs)
        print(f"[LOG] {func.__name__} returned {result}")
        return result
    return wrapper


def timing_decorator(func):
    """函数计时装饰器"""
    @wraps(func)
    def wrapper(*args, **kwargs):
        start = time.perf_counter()
        result = func(*args, **kwargs)
        print(f"[TIMING] {func.__name__} took {time.perf_counter()-start:.4f}s")
        return result
    return wrapper


def retry_decorator(max_retries=3, delay=1.0):
    """函数重试装饰器（带参数）"""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            for attempt in range(max_retries):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    print(f"[RETRY] Attempt {attempt+1} failed: {e}")
                    if attempt < max_retries - 1:
                        time.sleep(delay)
            raise
        return wrapper
    return decorator


@logging_decorator
@timing_decorator
@retry_decorator(max_retries=2, delay=0.5)
def process_data(data):
    return f"Processed: {data}"


result = process_data("test")
print(f"Final result: {result}")
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **功能耦合** | 所有功能在一个类中 | 每个装饰器一个职责 |
| **组合方式** | 静态，通过参数控制 | 动态，任意组合装饰器 |
| **扩展性** | 新增功能需修改现有类 | 新增装饰器类即可 |
| **测试** | 难以单独测试某功能 | 每个装饰器可独立测试 |
| **复用** | 功能难以复用 | 装饰器可在不同组件间复用 |

---

## 装饰器顺序

```python
# 顺序很重要！

# 从外到内执行：Validation -> Logging -> Caching -> Process
# 从内到外返回：Process -> Caching -> Logging -> Validation

processor = BasicDataProcessor()
processor = CachingDecorator(processor)   # 最内层
processor = LoggingDecorator(processor)   # 中间
processor = ValidationDecorator(processor) # 最外层

# 调用流程：
# 1. ValidationDecorator.process() 验证
# 2. LoggingDecorator.process() 记录输入
# 3. CachingDecorator.process() 检查缓存
# 4. BasicDataProcessor.process() 实际处理
# 5. CachingDecorator 存储结果
# 6. LoggingDecorator 记录输出
# 7. ValidationDecorator 返回结果
```

---

## 与其他模式的关系

| 模式 | 目的 | 区别 |
|------|------|------|
| **Decorator** | 动态添加功能 | 增强功能，保持接口 |
| **Proxy** | 控制访问 | 访问控制，保持接口 |
| **Adapter** | 转换接口 | 改变接口 |
| **Strategy** | 替换算法 | 替换整个算法 |

