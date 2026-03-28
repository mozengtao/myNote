# Python 装饰器与 *args / **kwargs 深度解析

## 目录

- [一、装饰器基础理论](#一装饰器基础理论)
- [二、*args / **kwargs 参数机制](#二args--kwargs-参数机制)
- [三、装饰器实际应用场景](#三装饰器实际应用场景)
- [四、高级装饰器模式](#四高级装饰器模式)
- [五、框架设计中的装饰器模式](#五框架设计中的装饰器模式)
- [六、生产环境最佳实践](#六生产环境最佳实践)
- [七、性能优化与注意事项](#七性能优化与注意事项)

---

## 一、装饰器基础理论

### 1.1 装饰器的本质

装饰器（Decorator）的本质是一个**高阶函数**：

- 接收一个函数作为参数
- 返回一个新的函数（wrapper）
- 用新函数替换原函数引用

### 1.2 语法等价关系

```python
@decorator
def f():
    pass

# 等价于：
f = decorator(f)
```

### 1.3 核心思想

> ❗**重要**：装饰器不是"修改函数"，而是"替换函数"

### 1.4 标准结构

```python
def decorator(func):
    def wrapper(*args, **kwargs):
        # before logic
        result = func(*args, **kwargs)
        # after logic
        return result
    return wrapper
```

### 1.5 设计原则

装饰器体现了：

✅ **开闭原则（Open-Closed Principle）**
- 对扩展开放
- 对修改封闭

---

## 二、*args / **kwargs 参数机制

### 2.1 Python 调用模型的根本原理

Python 函数调用统一抽象为：

```python
func(positional_args_tuple, keyword_args_dict)
```

即：
- 所有位置参数 → `tuple`
- 所有关键字参数 → `dict`

### 2.2 *args / **kwargs 的本质

| 写法 | 本质 |
|------|------|
| `*args` | 收集位置参数 → `tuple` |
| `**kwargs` | 收集关键字参数 → `dict` |

### 2.3 示例演示

```python
def f(*args, **kwargs):
    print(f"args: {args}")
    print(f"kwargs: {kwargs}")

f(1, 2, x=3, y=4)
# 输出：
# args: (1, 2)
# kwargs: {'x': 3, 'y': 4}
```

### 2.4 为什么可以适配任意函数？

> ❗**关键理解**：任何函数调用都可以被拆解为 `(args, kwargs)`

因此：`func(*args, **kwargs)` 可以"无损还原"原始调用

### 2.5 打包（Packing）与解包（Unpacking）

#### 2.5.1 打包（定义阶段）

```python
def f(*args, **kwargs):
    pass

# 1. 打包示例
def print_args(*args, **kwargs):
    print(f"位置参数元组: {args}")
    print(f"关键字参数字典: {kwargs}")

print_args(1, 2, 3, name='元宝', age=4)
# 输出：
# 位置参数元组: (1, 2, 3)
# 关键字参数字典: {'name': '元宝', 'age': 4}
```

作用：
- 多余位置参数 → `tuple`（args）
- 多余关键字参数 → `dict`（kwargs）

#### 2.5.2 解包（调用阶段）

```python
f(*args, **kwargs)

# 2. 解包示例
def normal_func(a, b, c):
    print(a, b, c)

my_list = [1, 2, 3]
my_dict = {'a': 10, 'b': 20, 'c': 30}

normal_func(*my_list)  # 等价于 normal_func(1, 2, 3)
normal_func(**my_dict) # 等价于 normal_func(a=10, b=20, c=30)
```

作用：
- `*args` → 展开为位置参数
- `**kwargs` → 展开为 key=value

#### 2.5.3 完整示例

**位置参数解包：**

```python
def f(a, b, c):
    print(a, b, c)

args = (1, 2, 3)
f(*args)  # 等价于：f(1, 2, 3)
```

**关键字参数解包：**

```python
def f(x, y):
    print(x, y)

kwargs = {'x': 10, 'y': 20}
f(**kwargs)  # 等价于：f(x=10, y=20)
```

### 2.6 函数参数顺序规则

**正确顺序：**
```
def func(standard_args, *args, keyword_only_args, **kwargs):
    ...

位置参数 → *args → keyword-only 参数 → **kwargs
```

**标准示例：**
```python
def f(a, b, *args, c=10, **kwargs):
    pass
```

**错误示例：**
```python
def f(**kwargs, *args):  # ❌ 语法错误
    pass
```

### 2.7 为什么装饰器必须使用 *args, **kwargs？

**错误写法（不可扩展）：**

```python
def wrapper():
    return func()
# 只能支持：func()
```

**正确写法（通用代理）：**

```python
def wrapper(*args, **kwargs):
    return func(*args, **kwargs)
```

**核心意义：**
✅ 实现对任意函数签名的"透明代理"

---

## 三、装饰器实际应用场景

### 3.1 日志记录装饰器

```python
import functools
import logging
from datetime import datetime

def log_calls(level=logging.INFO):
    """记录函数调用的装饰器"""
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            logger = logging.getLogger(func.__module__)
            
            # 记录调用开始
            start_time = datetime.now()
            logger.log(level, f"调用 {func.__name__} 开始，参数：args={args}, kwargs={kwargs}")
            
            try:
                result = func(*args, **kwargs)
                # 记录成功结果
                end_time = datetime.now()
                duration = (end_time - start_time).total_seconds()
                logger.log(level, f"调用 {func.__name__} 成功，耗时：{duration:.3f}s，返回值：{result}")
                return result
            except Exception as e:
                # 记录异常
                end_time = datetime.now()
                duration = (end_time - start_time).total_seconds()
                logger.error(f"调用 {func.__name__} 异常，耗时：{duration:.3f}s，错误：{e}")
                raise
        
        return wrapper
    return decorator

# 使用示例
@log_calls(level=logging.DEBUG)
def calculate_sum(a, b, multiplier=1):
    return (a + b) * multiplier

# 调用
result = calculate_sum(10, 20, multiplier=2)
```

### 3.2 性能监控装饰器

```python
import time
import functools
from collections import defaultdict

# 全局性能统计
performance_stats = defaultdict(list)

def performance_monitor(func):
    """性能监控装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        start_time = time.perf_counter()
        
        try:
            result = func(*args, **kwargs)
            return result
        finally:
            end_time = time.perf_counter()
            duration = end_time - start_time
            
            # 记录性能数据
            func_name = f"{func.__module__}.{func.__name__}"
            performance_stats[func_name].append(duration)
            
            # 实时警告慢查询
            if duration > 1.0:  # 超过1秒
                print(f"⚠️  慢函数警告：{func_name} 耗时 {duration:.3f}s")
    
    return wrapper

def get_performance_report():
    """获取性能报告"""
    report = {}
    for func_name, durations in performance_stats.items():
        report[func_name] = {
            'count': len(durations),
            'total_time': sum(durations),
            'avg_time': sum(durations) / len(durations),
            'max_time': max(durations),
            'min_time': min(durations)
        }
    return report

# 使用示例
@performance_monitor
def slow_function():
    time.sleep(0.5)  # 模拟耗时操作
    return "完成"

@performance_monitor
def fast_function():
    return sum(range(1000))

# 调用并查看报告
slow_function()
fast_function()
print(get_performance_report())
```

### 3.3 缓存装饰器

```python
import functools
import hashlib
import pickle
from datetime import datetime, timedelta

class CacheManager:
    def __init__(self):
        self._cache = {}
        self._expire_times = {}
    
    def _generate_key(self, func, args, kwargs):
        """生成缓存键"""
        key_data = {
            'func_name': f"{func.__module__}.{func.__name__}",
            'args': args,
            'kwargs': sorted(kwargs.items())
        }
        key_str = pickle.dumps(key_data)
        return hashlib.md5(key_str).hexdigest()
    
    def get(self, key):
        """获取缓存"""
        if key in self._cache:
            # 检查是否过期
            if key in self._expire_times:
                if datetime.now() > self._expire_times[key]:
                    del self._cache[key]
                    del self._expire_times[key]
                    return None
            return self._cache[key]
        return None
    
    def set(self, key, value, ttl=None):
        """设置缓存"""
        self._cache[key] = value
        if ttl:
            self._expire_times[key] = datetime.now() + timedelta(seconds=ttl)

# 全局缓存实例
cache_manager = CacheManager()

def cache(ttl=300):  # 默认5分钟过期
    """缓存装饰器"""
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            # 生成缓存键
            cache_key = cache_manager._generate_key(func, args, kwargs)
            
            # 尝试从缓存获取
            cached_result = cache_manager.get(cache_key)
            if cached_result is not None:
                print(f"🎯 缓存命中：{func.__name__}")
                return cached_result
            
            # 执行函数并缓存结果
            print(f"⚡ 执行函数：{func.__name__}")
            result = func(*args, **kwargs)
            cache_manager.set(cache_key, result, ttl)
            
            return result
        
        # 添加缓存清理方法
        def clear_cache():
            cache_manager._cache.clear()
            cache_manager._expire_times.clear()
        
        wrapper.clear_cache = clear_cache
        return wrapper
    return decorator

# 使用示例
@cache(ttl=10)  # 10秒过期
def expensive_calculation(n):
    """模拟耗时计算"""
    print(f"正在计算 {n} 的阶乘...")
    time.sleep(1)  # 模拟计算时间
    result = 1
    for i in range(1, n + 1):
        result *= i
    return result

# 测试缓存效果
print(expensive_calculation(5))  # 第一次调用，会计算
print(expensive_calculation(5))  # 第二次调用，使用缓存
```

### 3.4 重试机制装饰器

```python
import functools
import time
import random
from typing import Tuple, Type

def retry(
    max_attempts: int = 3,
    delay: float = 1.0,
    backoff: float = 2.0,
    exceptions: Tuple[Type[Exception], ...] = (Exception,)
):
    """重试装饰器
    
    Args:
        max_attempts: 最大重试次数
        delay: 初始延迟时间（秒）
        backoff: 延迟倍数
        exceptions: 需要重试的异常类型
    """
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            current_delay = delay
            
            for attempt in range(max_attempts):
                try:
                    return func(*args, **kwargs)
                except exceptions as e:
                    if attempt == max_attempts - 1:
                        # 最后一次尝试失败，抛出异常
                        raise
                    
                    print(f"🔄 {func.__name__} 第 {attempt + 1} 次尝试失败：{e}")
                    print(f"   等待 {current_delay:.1f} 秒后重试...")
                    
                    time.sleep(current_delay)
                    current_delay *= backoff
        
        return wrapper
    return decorator

# 使用示例
@retry(max_attempts=5, delay=0.5, backoff=1.5, exceptions=(ConnectionError, TimeoutError))
def unreliable_network_call():
    """模拟不可靠的网络调用"""
    if random.random() < 0.7:  # 70% 概率失败
        raise ConnectionError("网络连接失败")
    return "网络请求成功"

# 测试重试机制
try:
    result = unreliable_network_call()
    print(f"✅ {result}")
except Exception as e:
    print(f"❌ 最终失败：{e}")
```

### 3.5 权限验证装饰器

```python
import functools
from enum import Enum
from typing import Set, Optional

class Permission(Enum):
    READ = "read"
    WRITE = "write"
    DELETE = "delete"
    ADMIN = "admin"

class User:
    def __init__(self, username: str, permissions: Set[Permission]):
        self.username = username
        self.permissions = permissions

# 模拟当前用户上下文
current_user: Optional[User] = None

def set_current_user(user: User):
    global current_user
    current_user = user

def require_permission(*required_permissions: Permission):
    """权限验证装饰器"""
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            if current_user is None:
                raise PermissionError("用户未登录")
            
            # 检查是否有管理员权限
            if Permission.ADMIN in current_user.permissions:
                return func(*args, **kwargs)
            
            # 检查是否有所需权限
            for perm in required_permissions:
                if perm not in current_user.permissions:
                    raise PermissionError(f"用户 {current_user.username} 缺少权限：{perm.value}")
            
            return func(*args, **kwargs)
        
        return wrapper
    return decorator

# 使用示例
@require_permission(Permission.READ)
def read_data():
    return "读取数据成功"

@require_permission(Permission.WRITE)
def write_data(data):
    return f"写入数据成功：{data}"

@require_permission(Permission.DELETE)
def delete_data(data_id):
    return f"删除数据成功：{data_id}"

# 测试权限系统
# 普通用户
user1 = User("alice", {Permission.READ, Permission.WRITE})
set_current_user(user1)

print(read_data())    # ✅ 成功
print(write_data("test"))  # ✅ 成功

try:
    delete_data(123)   # ❌ 权限不足
except PermissionError as e:
    print(f"权限错误：{e}")

# 管理员用户
admin = User("admin", {Permission.ADMIN})
set_current_user(admin)
print(delete_data(123))  # ✅ 成功（管理员权限）
```

---

## 四、高级装饰器模式

### 4.1 带参数的装饰器

```python
def validate_types(**type_hints):
    """类型验证装饰器"""
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            # 验证位置参数
            func_args = list(func.__code__.co_varnames[:func.__code__.co_argcount])
            
            for i, (arg_name, arg_value) in enumerate(zip(func_args, args)):
                if arg_name in type_hints:
                    expected_type = type_hints[arg_name]
                    if not isinstance(arg_value, expected_type):
                        raise TypeError(
                            f"参数 {arg_name} 期望类型 {expected_type.__name__}，"
                            f"实际类型 {type(arg_value).__name__}"
                        )
            
            # 验证关键字参数
            for arg_name, arg_value in kwargs.items():
                if arg_name in type_hints:
                    expected_type = type_hints[arg_name]
                    if not isinstance(arg_value, expected_type):
                        raise TypeError(
                            f"参数 {arg_name} 期望类型 {expected_type.__name__}，"
                            f"实际类型 {type(arg_value).__name__}"
                        )
            
            return func(*args, **kwargs)
        return wrapper
    return decorator

# 使用示例
@validate_types(name=str, age=int, active=bool)
def create_user(name, age, active=True):
    return f"用户：{name}，年龄：{age}，激活：{active}"

# 测试
print(create_user("Alice", 25))  # ✅ 正确
try:
    create_user("Bob", "25")     # ❌ age 类型错误
except TypeError as e:
    print(f"类型错误：{e}")
```

### 4.2 类装饰器

```python
class CountCalls:
    """统计函数调用次数的类装饰器"""
    
    def __init__(self, func):
        self.func = func
        self.count = 0
        functools.update_wrapper(self, func)
    
    def __call__(self, *args, **kwargs):
        self.count += 1
        print(f"📊 {self.func.__name__} 被调用了 {self.count} 次")
        return self.func(*args, **kwargs)
    
    def reset_count(self):
        """重置计数"""
        self.count = 0

# 使用示例
@CountCalls
def greet(name):
    return f"Hello, {name}!"

print(greet("Alice"))
print(greet("Bob"))
print(f"总调用次数：{greet.count}")
greet.reset_count()
```

### 4.3 装饰器链

```python
def timer(func):
    """计时装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        start = time.perf_counter()
        result = func(*args, **kwargs)
        end = time.perf_counter()
        print(f"⏱️  {func.__name__} 耗时：{end - start:.4f}s")
        return result
    return wrapper

def debug(func):
    """调试装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        print(f"🐛 调用 {func.__name__}，参数：{args}, {kwargs}")
        result = func(*args, **kwargs)
        print(f"🐛 {func.__name__} 返回：{result}")
        return result
    return wrapper

# 装饰器链应用（注意顺序：从下往上）
@timer       # 最后执行
@debug       # 先执行
@cache(ttl=5)  # 最先执行
def complex_calculation(n):
    """复杂计算函数"""
    return sum(i * i for i in range(n))

# 测试装饰器链
result = complex_calculation(1000)
```

### 4.4 条件装饰器

```python
def conditional_decorator(condition):
    """条件装饰器"""
    def decorator(func):
        if condition():
            # 满足条件时应用装饰器
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                print(f"🎯 条件满足，执行增强逻辑")
                return func(*args, **kwargs)
            return wrapper
        else:
            # 不满足条件时返回原函数
            return func
    return decorator

# 使用示例
DEBUG_MODE = True

@conditional_decorator(lambda: DEBUG_MODE)
def process_data(data):
    return f"处理数据：{data}"

print(process_data("test"))  # 会打印调试信息

DEBUG_MODE = False

@conditional_decorator(lambda: DEBUG_MODE)
def process_data2(data):
    return f"处理数据：{data}"

print(process_data2("test"))  # 不会打印调试信息
```

---

## 五、框架设计中的装饰器模式

### 5.1 Web 框架路由系统

```python
from typing import Dict, Callable, Any
import re
import json

class SimpleWebFramework:
    """简单的 Web 框架示例"""
    
    def __init__(self):
        self.routes: Dict[str, Dict[str, Callable]] = {}
        self.middleware: list = []
    
    def route(self, path: str, methods: list = None):
        """路由装饰器"""
        if methods is None:
            methods = ['GET']
        
        def decorator(func):
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                # 应用中间件
                for middleware in self.middleware:
                    result = middleware(*args, **kwargs)
                    if result:  # 中间件可以中断请求
                        return result
                
                # 执行路由处理函数
                return func(*args, **kwargs)
            
            # 注册路由
            if path not in self.routes:
                self.routes[path] = {}
            
            for method in methods:
                self.routes[path][method] = wrapper
            
            return wrapper
        return decorator
    
    def middleware_decorator(self, func):
        """中间件装饰器"""
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            return func(*args, **kwargs)
        
        self.middleware.append(wrapper)
        return wrapper
    
    def handle_request(self, path: str, method: str, **kwargs):
        """处理请求"""
        if path in self.routes and method in self.routes[path]:
            handler = self.routes[path][method]
            return handler(**kwargs)
        else:
            return {"error": "404 Not Found"}

# 使用示例
app = SimpleWebFramework()

@app.middleware_decorator
def auth_middleware(**kwargs):
    """认证中间件"""
    if kwargs.get('user_id') is None:
        return {"error": "未认证"}
    return None  # 继续处理

@app.route('/users/<user_id>', methods=['GET', 'POST'])
def get_user(user_id=None, **kwargs):
    return {
        "user_id": user_id,
        "name": f"User {user_id}",
        "method": kwargs.get('method', 'GET')
    }

@app.route('/health')
def health_check(**kwargs):
    return {"status": "healthy"}

# 测试框架
print("测试认证成功：", app.handle_request('/users/123', 'GET', user_id='123', method='GET'))
print("测试认证失败：", app.handle_request('/users/123', 'GET', method='GET'))
print("测试健康检查：", app.handle_request('/health', 'GET'))
```

### 5.2 ORM 系统装饰器

```python
from typing import Dict, Any, Type, Optional
import sqlite3
import json
from datetime import datetime

class Model:
    """简单的 ORM 基类"""
    _table_name: str = ""
    _fields: Dict[str, Type] = {}
    _db_connection: sqlite3.Connection = None
    
    def __init__(self, **kwargs):
        for field, value in kwargs.items():
            setattr(self, field, value)
    
    @classmethod
    def set_connection(cls, connection):
        cls._db_connection = connection

def model(table_name: str):
    """ORM 模型装饰器"""
    def decorator(cls):
        # 设置表名
        cls._table_name = table_name
        
        # 分析字段类型
        cls._fields = {}
        for attr_name, attr_type in cls.__annotations__.items():
            cls._fields[attr_name] = attr_type
        
        # 添加查询方法
        @classmethod
        def find_by_id(model_cls, id_value):
            cursor = model_cls._db_connection.cursor()
            cursor.execute(f"SELECT * FROM {model_cls._table_name} WHERE id = ?", (id_value,))
            row = cursor.fetchone()
            if row:
                columns = [desc[0] for desc in cursor.description]
                data = dict(zip(columns, row))
                return model_cls(**data)
            return None
        
        @classmethod
        def find_all(model_cls):
            cursor = model_cls._db_connection.cursor()
            cursor.execute(f"SELECT * FROM {model_cls._table_name}")
            rows = cursor.fetchall()
            columns = [desc[0] for desc in cursor.description]
            return [model_cls(**dict(zip(columns, row))) for row in rows]
        
        def save(self):
            cursor = self._db_connection.cursor()
            fields = list(self._fields.keys())
            values = [getattr(self, field, None) for field in fields]
            
            placeholders = ','.join(['?' for _ in fields])
            field_names = ','.join(fields)
            
            cursor.execute(
                f"INSERT OR REPLACE INTO {self._table_name} ({field_names}) VALUES ({placeholders})",
                values
            )
            self._db_connection.commit()
            return self
        
        # 绑定方法到类
        cls.find_by_id = find_by_id
        cls.find_all = find_all
        cls.save = save
        
        return cls
    return decorator

def validate_field(field_name: str, validator: Callable):
    """字段验证装饰器"""
    def decorator(cls):
        original_init = cls.__init__
        
        @functools.wraps(original_init)
        def new_init(self, **kwargs):
            # 验证指定字段
            if field_name in kwargs:
                if not validator(kwargs[field_name]):
                    raise ValueError(f"字段 {field_name} 验证失败")
            original_init(self, **kwargs)
        
        cls.__init__ = new_init
        return cls
    return decorator

# 使用示例
@model("users")
@validate_field("email", lambda x: "@" in x if x else True)
class User(Model):
    id: int
    name: str
    email: str
    created_at: str

# 设置数据库连接
conn = sqlite3.connect(":memory:")
User.set_connection(conn)

# 创建表
conn.execute("""
    CREATE TABLE users (
        id INTEGER PRIMARY KEY,
        name TEXT,
        email TEXT,
        created_at TEXT
    )
""")

# 使用 ORM
user = User(
    id=1,
    name="Alice",
    email="alice@example.com",
    created_at=datetime.now().isoformat()
)
user.save()

# 查询
found_user = User.find_by_id(1)
print(f"找到用户：{found_user.name}, {found_user.email}")

all_users = User.find_all()
print(f"所有用户：{[u.name for u in all_users]}")
```

### 5.3 RPC 系统装饰器

```python
import json
import uuid
from typing import Callable, Dict, Any
from datetime import datetime

class RPCRegistry:
    """RPC 方法注册表"""
    def __init__(self):
        self.methods: Dict[str, Callable] = {}
        self.middleware: list = []
    
    def register(self, method_name: str = None):
        """注册 RPC 方法"""
        def decorator(func):
            name = method_name or func.__name__
            
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                # 创建请求上下文
                request_id = str(uuid.uuid4())
                context = {
                    'request_id': request_id,
                    'method': name,
                    'timestamp': datetime.now().isoformat(),
                    'args': args,
                    'kwargs': kwargs
                }
                
                try:
                    # 应用中间件（前置处理）
                    for middleware in self.middleware:
                        middleware_result = middleware(context, 'before')
                        if middleware_result:
                            context.update(middleware_result)
                    
                    # 执行方法
                    result = func(*args, **kwargs)
                    context['result'] = result
                    
                    # 应用中间件（后置处理）
                    for middleware in reversed(self.middleware):
                        middleware(context, 'after')
                    
                    return {
                        'jsonrpc': '2.0',
                        'result': result,
                        'id': request_id
                    }
                
                except Exception as e:
                    error_response = {
                        'jsonrpc': '2.0',
                        'error': {
                            'code': -32603,
                            'message': str(e),
                            'data': {
                                'type': type(e).__name__,
                                'request_id': request_id
                            }
                        },
                        'id': request_id
                    }
                    
                    # 通知中间件发生错误
                    context['error'] = e
                    for middleware in reversed(self.middleware):
                        middleware(context, 'error')
                    
                    return error_response
            
            self.methods[name] = wrapper
            return wrapper
        return decorator
    
    def add_middleware(self, middleware_func):
        """添加中间件"""
        self.middleware.append(middleware_func)
        return middleware_func
    
    def call_method(self, method_name: str, *args, **kwargs):
        """调用注册的方法"""
        if method_name not in self.methods:
            return {
                'jsonrpc': '2.0',
                'error': {
                    'code': -32601,
                    'message': f'Method {method_name} not found'
                },
                'id': None
            }
        
        return self.methods[method_name](*args, **kwargs)

# 创建 RPC 注册表
rpc = RPCRegistry()

# 添加日志中间件
@rpc.add_middleware
def logging_middleware(context, phase):
    if phase == 'before':
        print(f"📞 RPC 调用开始：{context['method']} [{context['request_id']}]")
    elif phase == 'after':
        print(f"✅ RPC 调用成功：{context['method']} [{context['request_id']}]")
    elif phase == 'error':
        print(f"❌ RPC 调用失败：{context['method']} [{context['request_id']}] - {context['error']}")

# 添加性能监控中间件
@rpc.add_middleware
def performance_middleware(context, phase):
    if phase == 'before':
        context['start_time'] = time.perf_counter()
    elif phase in ['after', 'error']:
        duration = time.perf_counter() - context.get('start_time', 0)
        print(f"⏱️  耗时：{duration:.4f}s")

# 注册 RPC 方法
@rpc.register()
def add_numbers(a: int, b: int) -> int:
    """加法计算"""
    return a + b

@rpc.register("user.create")
def create_user(name: str, email: str) -> dict:
    """创建用户"""
    if not email or "@" not in email:
        raise ValueError("无效的邮箱地址")
    
    return {
        "id": uuid.uuid4().hex,
        "name": name,
        "email": email,
        "created_at": datetime.now().isoformat()
    }

@rpc.register()
def divide_numbers(a: int, b: int) -> float:
    """除法计算（可能出错）"""
    if b == 0:
        raise ZeroDivisionError("不能除以零")
    return a / b

# 测试 RPC 系统
print("=== RPC 测试 ===")
print(json.dumps(rpc.call_method("add_numbers", 10, 5), indent=2))
print(json.dumps(rpc.call_method("user.create", "Alice", "alice@example.com"), indent=2))
print(json.dumps(rpc.call_method("divide_numbers", 10, 0), indent=2))  # 错误案例
print(json.dumps(rpc.call_method("unknown_method"), indent=2))  # 方法不存在
```

---

## 六、生产环境最佳实践

### 6.1 使用 functools.wraps 保持函数元数据

#### 6.1.1 functools.wraps 的作用和重要性

`functools.wraps` 是装饰器开发中的**必备工具**，它的核心作用是：

✅ **保持原函数的元数据**：函数名、文档字符串、注解、模块信息等  
✅ **维持调试信息**：让调试器和IDE正确识别被装饰的函数  
✅ **支持反射操作**：确保 `inspect` 模块能正确分析函数  
✅ **保证工具兼容性**：让各种Python工具（如文档生成器、测试框架）正常工作

#### 6.1.2 基础对比示例

```python
import functools
import inspect

def bad_decorator(func):
    """❌ 不好的装饰器：丢失原函数信息"""
    def wrapper(*args, **kwargs):
        print(f"调用函数...")
        return func(*args, **kwargs)
    return wrapper

def good_decorator(func):
    """✅ 好的装饰器：保持原函数信息"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        print(f"调用函数...")
        return func(*args, **kwargs)
    return wrapper

@bad_decorator
def example_function_bad(x: int, y: str = "default") -> str:
    """这是一个示例函数，计算某些值"""
    return f"计算结果：{x}, {y}"

@good_decorator
def example_function_good(x: int, y: str = "default") -> str:
    """这是一个示例函数，计算某些值"""
    return f"计算结果：{x}, {y}"

print("❌ 坏装饰器结果：")
print(f"  函数名：{example_function_bad.__name__}")
print(f"  文档：{example_function_bad.__doc__}")
print(f"  模块：{example_function_bad.__module__}")
print(f"  注解：{example_function_bad.__annotations__}")

print("\n✅ 好装饰器结果：")
print(f"  函数名：{example_function_good.__name__}")
print(f"  文档：{example_function_good.__doc__}")
print(f"  模块：{example_function_good.__module__}")
print(f"  注解：{example_function_good.__annotations__}")
```

**输出对比：**
```
❌ 坏装饰器结果：
  函数名：wrapper
  文档：None
  模块：__main__
  注解：{}

✅ 好装饰器结果：
  函数名：example_function_good
  文档：这是一个示例函数，计算某些值
  模块：__main__
  注解：{'x': <class 'int'>, 'y': <class 'str'>, 'return': <class 'str'>}
```

#### 6.1.3 functools.wraps 保持的完整元数据

`functools.wraps` 会复制以下所有重要属性：

```python
import functools

def demonstrate_preserved_attributes(func):
    """演示 functools.wraps 保持的属性"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    return wrapper

def original_function(a: int, b: str = "test", *, c: float = 1.0) -> bool:
    """
    这是原函数的详细文档。
    
    Args:
        a: 整数参数
        b: 字符串参数，默认为 "test"
        c: 浮点数关键字参数
        
    Returns:
        布尔值结果
        
    Raises:
        ValueError: 当参数无效时
    """
    return True

# 应用装饰器
decorated = demonstrate_preserved_attributes(original_function)

# 检查保持的属性
preserved_attrs = [
    '__name__',       # 函数名
    '__doc__',        # 文档字符串
    '__module__',     # 模块名
    '__qualname__',   # 限定名
    '__annotations__', # 类型注解
    '__dict__',       # 函数属性字典
]

print("functools.wraps 保持的属性：")
for attr in preserved_attrs:
    original_value = getattr(original_function, attr)
    decorated_value = getattr(decorated, attr)
    is_same = original_value == decorated_value
    print(f"  {attr}: {'✅' if is_same else '❌'} {is_same}")
    if not is_same:
        print(f"    原函数: {original_value}")
        print(f"    装饰后: {decorated_value}")

# 检查函数签名
print(f"\n原函数签名: {inspect.signature(original_function)}")
print(f"装饰后签名: {inspect.signature(decorated)}")
```

#### 6.1.4 不使用 functools.wraps 的实际问题

```python
import functools
import inspect

def problematic_decorator(func):
    """❌ 有问题的装饰器"""
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    return wrapper

@problematic_decorator
def calculate_tax(income: float, rate: float = 0.2) -> float:
    """计算税额
    
    Args:
        income: 收入金额
        rate: 税率（默认20%）
        
    Returns:
        应纳税额
    """
    return income * rate

# 问题1：IDE和工具无法正确识别
print("问题演示：")
print(f"1. 函数名错误: {calculate_tax.__name__}")  # 显示 'wrapper' 而非 'calculate_tax'

# 问题2：文档生成器无法获取正确信息
print(f"2. 文档丢失: {repr(calculate_tax.__doc__)}")  # None

# 问题3：类型检查器无法工作
print(f"3. 类型注解丢失: {calculate_tax.__annotations__}")  # {}

# 问题4：inspect 模块无法正确分析
try:
    sig = inspect.signature(calculate_tax)
    print(f"4. 签名信息: {sig}")
except Exception as e:
    print(f"4. 无法获取签名: {e}")

# 问题5：调试器显示错误信息
print(f"5. 调试信息: {calculate_tax.__qualname__}")

# 问题6：help() 函数显示错误
print(f"\n6. help() 输出:")
help(calculate_tax)  # 显示 wrapper 的信息而非原函数
```

#### 6.1.5 functools.wraps 的实现原理

```python
import functools

def manual_wraps_demo(wrapped_func):
    """手动实现 functools.wraps 的效果"""
    def decorator(wrapper_func):
        # 手动复制重要属性（这就是 functools.wraps 内部做的事）
        wrapper_func.__name__ = wrapped_func.__name__
        wrapper_func.__doc__ = wrapped_func.__doc__
        wrapper_func.__module__ = wrapped_func.__module__
        wrapper_func.__qualname__ = wrapped_func.__qualname__
        wrapper_func.__annotations__ = wrapped_func.__annotations__.copy()
        wrapper_func.__dict__.update(wrapped_func.__dict__)
        
        # 特殊属性：保持原函数引用
        wrapper_func.__wrapped__ = wrapped_func
        
        return wrapper_func
    return decorator

# 使用手动实现
def my_decorator(func):
    @manual_wraps_demo(func)  # 等价于 @functools.wraps(func)
    def wrapper(*args, **kwargs):
        print(f"手动wraps: 调用 {func.__name__}")
        return func(*args, **kwargs)
    return wrapper

@my_decorator
def test_function(x: int) -> str:
    """测试函数"""
    return str(x)

print(f"函数名保持: {test_function.__name__}")
print(f"原函数访问: {test_function.__wrapped__}")
```

#### 6.1.6 高级用法：自定义保持的属性

```python
import functools

def custom_wraps(*attrs_to_copy):
    """自定义版本的 wraps，只复制指定属性"""
    def decorator(wrapped):
        def wrapper_decorator(wrapper):
            for attr in attrs_to_copy:
                if hasattr(wrapped, attr):
                    setattr(wrapper, attr, getattr(wrapped, attr))
            wrapper.__wrapped__ = wrapped
            return wrapper
        return wrapper_decorator
    return decorator

# 只保持函数名和文档
def selective_decorator(func):
    @custom_wraps('__name__', '__doc__')
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    return wrapper

@selective_decorator
def selective_test(x: int) -> str:
    """选择性保持测试"""
    return str(x)

print(f"保持的函数名: {selective_test.__name__}")
print(f"保持的文档: {selective_test.__doc__}")
print(f"未保持的注解: {selective_test.__annotations__}")  # 应该为空
```

#### 6.1.7 functools.wraps 与调试和性能分析

```python
import functools
import time
import cProfile

def performance_decorator(func):
    """性能分析装饰器（正确使用 wraps）"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        start_time = time.perf_counter()
        result = func(*args, **kwargs)
        end_time = time.perf_counter()
        
        # 由于使用了 functools.wraps，性能报告会显示正确的函数名
        print(f"⏱️ {func.__name__} 耗时: {end_time - start_time:.4f}s")
        return result
    return wrapper

@performance_decorator
def fibonacci(n: int) -> int:
    """计算斐波那契数列"""
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

# 性能分析会显示正确的函数名
def profile_test():
    """性能分析测试"""
    profiler = cProfile.Profile()
    profiler.enable()
    
    result = fibonacci(10)
    
    profiler.disable()
    profiler.print_stats()
    
    return result

# 运行性能分析
print("斐波那契结果:", fibonacci(10))
# 在性能报告中会显示 'fibonacci' 而不是 'wrapper'
```

#### 6.1.8 最佳实践总结

✅ **始终使用 functools.wraps**：
```python
import functools

def correct_decorator(func):
    @functools.wraps(func)  # ← 永远不要忘记这一行
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    return wrapper
```

✅ **检查装饰器是否正确**：
```python
def verify_decorator(decorator_func):
    """验证装饰器是否正确保持了元数据"""
    @decorator_func
    def test_function(x: int, y: str = "test") -> bool:
        """测试函数文档"""
        return True
    
    # 检查关键属性
    checks = {
        '函数名正确': test_function.__name__ == 'test_function',
        '文档保持': test_function.__doc__ == '测试函数文档',
        '注解保持': 'x' in test_function.__annotations__,
        '原函数可访问': hasattr(test_function, '__wrapped__')
    }
    
    for check_name, passed in checks.items():
        print(f"{'✅' if passed else '❌'} {check_name}")
    
    return all(checks.values())

# 测试装饰器
print("测试正确的装饰器:")
verify_decorator(good_decorator)
```

❌ **常见错误**：
- 忘记使用 `@functools.wraps(func)`
- 在类装饰器中忘记使用 `functools.update_wrapper`
- 自定义装饰器时不保持必要的元数据

通过正确使用 `functools.wraps`，你的装饰器将：
- 在IDE中提供正确的代码补全和文档
- 支持调试器正确显示函数信息  
- 兼容各种Python工具和框架
- 保持完整的函数元信息用于反射和分析

### 6.2 装饰器工厂模式

```python
def create_decorator_factory(default_config: dict):
    """装饰器工厂：创建可配置的装饰器"""
    
    def decorator_factory(**config):
        # 合并默认配置和用户配置
        final_config = {**default_config, **config}
        
        def decorator(func):
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                # 使用配置执行装饰器逻辑
                if final_config.get('log_calls', False):
                    print(f"调用 {func.__name__}")
                
                if final_config.get('measure_time', False):
                    start = time.perf_counter()
                
                result = func(*args, **kwargs)
                
                if final_config.get('measure_time', False):
                    end = time.perf_counter()
                    print(f"{func.__name__} 耗时：{end - start:.4f}s")
                
                return result
            return wrapper
        return decorator
    
    return decorator_factory

# 创建装饰器工厂
monitor = create_decorator_factory({
    'log_calls': True,
    'measure_time': False
})

# 使用默认配置
@monitor()
def func1():
    return "结果1"

# 使用自定义配置
@monitor(measure_time=True, log_calls=False)
def func2():
    time.sleep(0.1)
    return "结果2"

func1()
func2()
```

### 6.3 异常安全的装饰器

```python
def safe_decorator(func):
    """异常安全的装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        try:
            # 前置处理
            print(f"开始执行 {func.__name__}")
            
            # 执行原函数
            result = func(*args, **kwargs)
            
            # 后置处理（正常情况）
            print(f"成功执行 {func.__name__}")
            return result
            
        except Exception as e:
            # 异常处理
            print(f"执行 {func.__name__} 时发生异常：{e}")
            
            # 可以选择：
            # 1. 重新抛出异常
            raise
            # 2. 返回默认值
            # return None
            # 3. 转换异常类型
            # raise RuntimeError(f"装饰器捕获异常：{e}") from e
        
        finally:
            # 清理工作（无论是否异常都会执行）
            print(f"清理 {func.__name__} 相关资源")
    
    return wrapper

@safe_decorator
def risky_function(should_fail=False):
    if should_fail:
        raise ValueError("故意失败")
    return "成功"

# 测试
print(risky_function(False))  # 正常执行
try:
    risky_function(True)      # 异常执行
except ValueError:
    print("捕获到异常")
```

### 6.4 装饰器组合与链式调用

```python
class DecoratorChain:
    """装饰器链管理器"""
    
    def __init__(self):
        self.decorators = []
    
    def add(self, decorator):
        """添加装饰器到链中"""
        self.decorators.append(decorator)
        return self
    
    def apply(self, func):
        """应用装饰器链到函数"""
        result = func
        # 从右到左应用装饰器（模拟 @装饰器 语法）
        for decorator in reversed(self.decorators):
            result = decorator(result)
        return result
    
    def __call__(self, func):
        """支持作为装饰器使用"""
        return self.apply(func)

# 创建装饰器链
chain = DecoratorChain()
chain.add(timer).add(debug).add(cache(ttl=5))

# 方式1：手动应用
def my_function(x):
    return x * x

decorated_func = chain.apply(my_function)

# 方式2：作为装饰器使用
@DecoratorChain().add(timer).add(debug)
def another_function(x):
    return x + 1

print(decorated_func(5))
print(another_function(5))
```

---

## 七、性能优化与注意事项

### 7.1 装饰器的性能影响

```python
import time
import functools

def measure_decorator_overhead():
    """测量装饰器开销"""
    
    def original_function(x):
        return x * 2
    
    def simple_decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            return func(*args, **kwargs)
        return wrapper
    
    def heavy_decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            # 模拟重型装饰器逻辑
            start = time.perf_counter()
            result = func(*args, **kwargs)
            end = time.perf_counter()
            # 存储性能数据等重型操作
            _ = end - start  # 模拟使用时间数据
            return result
        return wrapper
    
    # 准备测试函数
    original = original_function
    simple_decorated = simple_decorator(original_function)
    heavy_decorated = heavy_decorator(original_function)
    
    iterations = 1000000
    
    # 测试原始函数
    start = time.perf_counter()
    for _ in range(iterations):
        original(42)
    original_time = time.perf_counter() - start
    
    # 测试简单装饰器
    start = time.perf_counter()
    for _ in range(iterations):
        simple_decorated(42)
    simple_time = time.perf_counter() - start
    
    # 测试重型装饰器
    start = time.perf_counter()
    for _ in range(iterations):
        heavy_decorated(42)
    heavy_time = time.perf_counter() - start
    
    print(f"性能测试结果（{iterations} 次调用）：")
    print(f"  原始函数：     {original_time:.4f}s")
    print(f"  简单装饰器：   {simple_time:.4f}s  (开销: {((simple_time/original_time-1)*100):.1f}%)")
    print(f"  重型装饰器：   {heavy_time:.4f}s   (开销: {((heavy_time/original_time-1)*100):.1f}%)")

measure_decorator_overhead()
```

### 7.2 避免装饰器陷阱

```python
# ❌ 陷阱1：在装饰器中修改全局状态
global_counter = 0

def bad_counter_decorator(func):
    """❌ 不好的计数器装饰器"""
    def wrapper(*args, **kwargs):
        global global_counter
        global_counter += 1  # 全局状态污染
        return func(*args, **kwargs)
    return wrapper

# ✅ 改进：使用实例状态
class CounterDecorator:
    """✅ 好的计数器装饰器"""
    def __init__(self):
        self.count = 0
    
    def __call__(self, func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            self.count += 1
            return func(*args, **kwargs)
        wrapper.get_count = lambda: self.count
        return wrapper

# ❌ 陷阱2：装饰器中的可变默认参数
def bad_cache_decorator(func, cache={}):  # ❌ 可变默认参数
    """❌ 危险的缓存装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        key = str(args) + str(kwargs)
        if key not in cache:
            cache[key] = func(*args, **kwargs)
        return cache[key]
    return wrapper

# ✅ 改进：使用 None 作为默认值
def good_cache_decorator(func, cache=None):
    """✅ 安全的缓存装饰器"""
    if cache is None:
        cache = {}
    
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        key = str(args) + str(kwargs)
        if key not in cache:
            cache[key] = func(*args, **kwargs)
        return cache[key]
    return wrapper

# ❌ 陷阱3：忘记处理异常
def bad_logging_decorator(func):
    """❌ 没有异常处理的日志装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        print(f"调用 {func.__name__}")
        result = func(*args, **kwargs)
        print(f"{func.__name__} 完成")  # 如果异常，这行不会执行
        return result
    return wrapper

# ✅ 改进：正确处理异常
def good_logging_decorator(func):
    """✅ 正确处理异常的日志装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        print(f"调用 {func.__name__}")
        try:
            result = func(*args, **kwargs)
            print(f"{func.__name__} 完成")
            return result
        except Exception as e:
            print(f"{func.__name__} 异常：{e}")
            raise
    return wrapper
```

### 7.3 装饰器调试技巧

```python
def debuggable_decorator(func):
    """可调试的装饰器"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        # 调试信息
        wrapper._call_count = getattr(wrapper, '_call_count', 0) + 1
        wrapper._last_args = args
        wrapper._last_kwargs = kwargs
        
        # 执行函数
        try:
            result = func(*args, **kwargs)
            wrapper._last_result = result
            wrapper._last_exception = None
            return result
        except Exception as e:
            wrapper._last_exception = e
            wrapper._last_result = None
            raise
    
    # 添加调试方法
    def get_debug_info():
        return {
            'call_count': getattr(wrapper, '_call_count', 0),
            'last_args': getattr(wrapper, '_last_args', None),
            'last_kwargs': getattr(wrapper, '_last_kwargs', None),
            'last_result': getattr(wrapper, '_last_result', None),
            'last_exception': getattr(wrapper, '_last_exception', None),
        }
    
    wrapper.get_debug_info = get_debug_info
    return wrapper

# 使用示例
@debuggable_decorator
def test_function(x, y=10):
    if x < 0:
        raise ValueError("x 不能为负数")
    return x * y

# 测试和调试
print(test_function(5))
print("调试信息：", test_function.get_debug_info())

try:
    test_function(-1)
except ValueError:
    print("异常调试信息：", test_function.get_debug_info())
```

---

## 八、总结

### 8.1 装饰器的核心价值

1. **代码复用**：横切关注点（日志、缓存、权限等）可以通过装饰器复用
2. **关注点分离**：业务逻辑与基础设施代码分离
3. **开闭原则**：不修改原函数代码，通过装饰器扩展功能
4. **声明式编程**：通过 `@decorator` 语法清晰表达意图

### 8.2 最佳实践总结

✅ **推荐做法：**
- 始终使用 `functools.wraps` 保持元数据
- 使用 `*args, **kwargs` 实现透明代理
- 合理处理异常和资源清理
- 避免在装饰器中使用全局状态
- 为复杂装饰器提供配置选项

❌ **避免陷阱：**
- 不要忽略异常处理
- 避免可变默认参数
- 注意装饰器的性能开销
- 不要过度嵌套装饰器

### 8.3 应用场景总结

| 场景 | 典型装饰器 | 核心价值 |
|------|------------|----------|
| **Web 开发** | 路由、权限、CORS | 声明式配置，代码简洁 |
| **数据处理** | 缓存、验证、序列化 | 提高性能，保证数据质量 |
| **系统监控** | 日志、性能、健康检查 | 可观测性，问题诊断 |
| **测试框架** | Mock、断言、数据准备 | 测试隔离，提高可维护性 |
| **API 开发** | 限流、重试、熔断 | 系统稳定性，容错能力 |

### 8.4 一句话总结

**Python 装饰器 = 高阶函数 + wrapper 替换 + (args, kwargs) 透传机制**

通过装饰器，我们可以在不修改原函数的情况下，优雅地扩展功能，实现横切关注点的复用，让代码更加模块化和可维护。