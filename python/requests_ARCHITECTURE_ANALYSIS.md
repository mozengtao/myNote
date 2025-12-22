# PSF/Requests Architecture & Design Patterns Analysis

## STEP 1: TOPOLOGY & LAYERING

### Module Dependency Diagram (ASCII)

```
+============================================================================+
|                           PUBLIC API LAYER                                  |
|----------------------------------------------------------------------------|
|  __init__.py                     api.py                                    |
|  [Package exports]               [Thin facade: get/post/put/delete/...]    |
|       |                               |                                    |
|       +---------------+---------------+                                    |
|                       |                                                    |
|                       v                                                    |
+============================================================================+
|                         COORDINATION LAYER                                  |
|----------------------------------------------------------------------------|
|                            sessions.py                                      |
|          [Session: cookies, auth, adapters, redirects, state]              |
|                       |                                                    |
|          +------------+------------+                                       |
|          |            |            |                                       |
|          v            v            v                                       |
+============================================================================+
|                          BACKEND LAYER                                      |
|----------------------------------------------------------------------------|
|      adapters.py          auth.py           cookies.py                     |
|  [HTTPAdapter:         [AuthBase:        [CookieJar                        |
|   urllib3 bridge]       callables]        management]                      |
|          |                                                                 |
|          v                                                                 |
|     [urllib3] <-- External Dependency                                      |
+============================================================================+
|                         DATA MODEL LAYER                                    |
|----------------------------------------------------------------------------|
|                            models.py                                        |
|       [Request -> PreparedRequest -> Response]                             |
|                       |                                                    |
|          +------------+------------+------------+                          |
|          |            |            |            |                          |
|          v            v            v            v                          |
+============================================================================+
|                        FOUNDATION LAYER                                     |
|----------------------------------------------------------------------------|
|  structures.py    utils.py     hooks.py    exceptions.py   compat.py      |
|  [CaseInsensitive [Helpers]    [Event      [Error          [Py version    |
|   Dict, LookupDict]            dispatch]    hierarchy]      compat]        |
+============================================================================+
```

---

### Module Responsibility Matrix

| Module | Responsibility | Public/Internal | Allowed Dependencies |
|--------|---------------|-----------------|---------------------|
| `__init__.py` | Package facade, re-exports public API | **Public** | api, models, sessions, exceptions |
| `api.py` | Stateless convenience functions | **Public** | sessions |
| `sessions.py` | Stateful request orchestration | **Public** | adapters, models, cookies, hooks, auth, utils, exceptions |
| `adapters.py` | Protocol-specific transport | **Public** (for extension) | models, utils, exceptions, urllib3 |
| `models.py` | Request/Response data containers | **Public** | utils, structures, hooks, cookies, exceptions, auth |
| `exceptions.py` | Exception hierarchy | **Public** | compat |
| `auth.py` | Authentication handlers | **Public** | utils, cookies, compat |
| `cookies.py` | Cookie jar management | Internal | compat |
| `hooks.py` | Event dispatch system | Internal | (none) |
| `structures.py` | Custom data structures | Internal | compat |
| `utils.py` | Shared utilities | Mixed (some public) | compat, structures, cookies, certs |
| `compat.py` | Python version compatibility | Internal | (stdlib only) |
| `_internal_utils.py` | Strictly internal helpers | **Internal** | (minimal) |
| `certs.py` | Certificate bundle location | Internal | (stdlib only) |
| `status_codes.py` | HTTP status code lookup | Internal | structures |

---

中文说明（Chinese Explanation）:

**模块分层设计原则：**

1. **公共API层（api.py, __init__.py）**
   - 提供用户直接调用的接口（如 `requests.get()`）
   - 保持极简，仅做参数传递，不包含业务逻辑
   - 隐藏底层复杂性，用户无需了解 Session、Adapter 等概念

2. **协调层（sessions.py）**
   - 核心调度器，管理请求生命周期
   - 持有状态：cookies、headers、auth、adapters
   - 负责合并配置、处理重定向、分发 hooks

3. **后端层（adapters.py, auth.py, cookies.py）**
   - 可插拔的协议实现
   - HTTPAdapter 作为 urllib3 的桥接器
   - 认证处理器通过 callable 协议实现策略模式

4. **数据模型层（models.py）**
   - Request：用户意图的表示
   - PreparedRequest：经过编码的实际请求
   - Response：服务器响应的封装

5. **基础层（structures.py, utils.py, hooks.py, exceptions.py）**
   - 无业务逻辑的工具类
   - 不依赖上层模块，避免循环依赖

---

## STEP 2: PUBLIC API VS INTERNAL CORE

### Why is `api.py` So Small? (62 lines of actual code)

```python
# api.py - The entire implementation pattern:
def request(method, url, **kwargs):
    with sessions.Session() as session:
        return session.request(method=method, url=url, **kwargs)

def get(url, params=None, **kwargs):
    return request("get", url, params=params, **kwargs)

def post(url, data=None, json=None, **kwargs):
    return request("post", url, data=data, json=json, **kwargs)
# ... similar for put, patch, delete, head, options
```

**Design Rationale:**

1. **Single Responsibility**: `api.py` only maps HTTP verbs to method calls
2. **Facade Pattern**: Hides the Session abstraction for one-off requests
3. **Resource Safety**: Uses context manager to ensure cleanup
4. **Delegation**: All logic delegated to `Session.request()`

### Tracing `requests.get()` End-to-End

```
User Code                   API Layer              Coordination         Backend
---------                   ---------              ------------         -------
requests.get(url)
    |
    v
api.get(url, **kwargs)
    |
    v
api.request("get", url)
    |
    +-- creates Session() with context manager
    |
    v
Session.request(method, url, **kwargs)
    |
    +-- 1. Create Request object
    +-- 2. Merge session settings
    +-- 3. Prepare: Request -> PreparedRequest
    +-- 4. Resolve proxies, environment
    |
    v
Session.send(prepared_request)
    |
    +-- 1. Get adapter for URL scheme
    +-- 2. Dispatch to adapter
    |
    v
HTTPAdapter.send(request)
    |
    +-- 1. Get/create urllib3 connection
    +-- 2. Execute HTTP call
    +-- 3. Build Response object
    |
    v
Response <-- returned through stack
```

### How Complexity is Hidden

| User Sees | Library Handles Internally |
|-----------|---------------------------|
| `requests.get(url)` | Session creation/cleanup, connection pooling |
| URL string | URL parsing, encoding, IDNA normalization |
| `auth=(user, pass)` | Basic/Digest authentication, 401 retry |
| Headers dict | Case-insensitive handling, encoding |
| `timeout=30` | Connect/read timeout separation, urllib3 integration |
| Redirect following | 301/302/307/308 handling, auth stripping, method changes |
| SSL verification | Certificate validation, bundle location, custom CA |

---

中文说明（Chinese Explanation）:

**API设计的简约之美：**

1. **api.py 为何如此精简？**
   - 仅暴露 HTTP 动词对应的函数
   - 每个函数只做一件事：调用 `request()`
   - 真正的工作在 Session 中完成

2. **一次 GET 请求的完整路径：**
   ```
   requests.get() -> api.request() -> Session.request() -> Session.send()
   -> HTTPAdapter.send() -> urllib3.urlopen() -> Response
   ```

3. **复杂度隐藏策略：**
   - Session 管理连接池、Cookie、认证状态
   - PreparedRequest 处理所有编码细节
   - Adapter 隔离底层协议实现
   - Response 提供便捷的 `.json()`, `.text` 属性

---

## STEP 3: SESSION AS THE CORE ABSTRACTION

### Session State Ownership

```python
class Session:
    __attrs__ = [
        "headers",      # Default headers for all requests
        "cookies",      # Persistent cookie jar
        "auth",         # Default authentication
        "proxies",      # Proxy configuration
        "hooks",        # Event callbacks
        "params",       # Default query parameters
        "verify",       # SSL verification setting
        "cert",         # Client certificate
        "adapters",     # Protocol adapters (OrderedDict)
        "stream",       # Stream response body
        "trust_env",    # Use environment for config
        "max_redirects" # Redirect limit
    ]
```

### Session as Facade Pattern

```
+------------------------------------------------------------------+
|                         Session                                   |
|------------------------------------------------------------------|
|  headers, cookies, auth, proxies, ...                            |
|                                                                  |
|  +------------------+  +------------------+  +------------------+|
|  | prepare_request  |  | merge_settings   |  | resolve_redirects||
|  +------------------+  +------------------+  +------------------+|
|            |                   |                     |           |
|            v                   v                     v           |
|  +---------+-------+  +--------+--------+  +--------+---------+ |
|  | models.py       |  | utils.py        |  | adapters.py      | |
|  | PreparedRequest |  | merge_setting() |  | HTTPAdapter      | |
|  +-----------------+  +-----------------+  +------------------+ |
+------------------------------------------------------------------+
```

### Session as Context Object

```python
# Session carries context through the request lifecycle:

def send(self, request, **kwargs):
    # 1. Apply session-level defaults
    kwargs.setdefault("stream", self.stream)
    kwargs.setdefault("verify", self.verify)
    kwargs.setdefault("cert", self.cert)
    
    # 2. Get appropriate adapter based on URL
    adapter = self.get_adapter(url=request.url)
    
    # 3. Delegate to adapter
    r = adapter.send(request, **kwargs)
    
    # 4. Dispatch hooks with context
    r = dispatch_hook("response", request.hooks, r, **kwargs)
    
    # 5. Persist cookies back to session
    extract_cookies_to_jar(self.cookies, request, r.raw)
    
    # 6. Handle redirects (loops back to send)
    if allow_redirects:
        gen = self.resolve_redirects(r, request, **kwargs)
        history = [resp for resp in gen]
```

### Session as Dependency Aggregator

```
                    +-------------+
                    |   Session   |
                    +------+------+
                           |
       +-------------------+-------------------+
       |                   |                   |
       v                   v                   v
+------+------+    +-------+-------+   +-------+-------+
| HTTPAdapter |    | RequestsCookieJar|   | default_hooks() |
| (https://)  |    | (persistence)   |   | (callbacks)     |
+-------------+    +-----------------+   +-----------------+
       |
       v
+------+------+
| HTTPAdapter |
| (http://)   |
+-------------+
```

### Patterns in Session

| Pattern | Implementation | Purpose |
|---------|---------------|---------|
| **Facade** | `Session.request()` wraps Request, PreparedRequest, Adapter | Hide multi-step process |
| **Context Object** | Session carries state across method calls | Maintain consistency |
| **Dependency Aggregation** | Adapters mounted by prefix | Pluggable transports |
| **Template Method** | `prepare_request()` → `send()` → `resolve_redirects()` | Standardized flow |
| **Mixin** | `SessionRedirectMixin` | Separate redirect logic |

---

中文说明（Chinese Explanation）:

**Session 为何是核心抽象？**

1. **状态持有者**
   - cookies：跨请求保持登录状态
   - headers：设置全局头部（如 User-Agent）
   - adapters：按协议选择后端

2. **门面模式（Facade）**
   - 用户只需调用 `session.get()`
   - Session 内部协调多个组件完成请求

3. **上下文对象（Context Object）**
   - Session 贯穿请求生命周期
   - 传递配置、收集状态、分发事件

4. **依赖聚合**
   - Session 持有多个 Adapter
   - 通过 `mount()` 方法注册新的协议处理器
   - `get_adapter()` 按 URL 前缀匹配

---

## STEP 4: ADAPTERS AS PLUGGABLE BACKENDS

### Adapter Interface (Implicit Protocol)

```python
class BaseAdapter:
    """The Base Transport Adapter"""
    
    def send(self, request, stream=False, timeout=None, 
             verify=True, cert=None, proxies=None):
        """Sends PreparedRequest object. Returns Response object."""
        raise NotImplementedError
    
    def close(self):
        """Cleans up adapter specific items."""
        raise NotImplementedError
```

### HTTPAdapter as Bridge to urllib3

```
+------------------------------------------------------------------+
|                        HTTPAdapter                                |
|------------------------------------------------------------------|
|  poolmanager: urllib3.PoolManager                                |
|  proxy_manager: {proxy_url: ProxyManager}                        |
|  max_retries: urllib3.Retry                                      |
|                                                                  |
|  Methods:                                                        |
|  +------------------+----------------------------------------+   |
|  | send()           | Execute request, return Response       |   |
|  | build_response() | urllib3.Response -> requests.Response  |   |
|  | get_connection() | Get pooled connection for URL          |   |
|  | cert_verify()    | Configure SSL settings                 |   |
|  | proxy_manager_for()| Get/create proxy manager             |   |
|  +------------------+----------------------------------------+   |
+------------------------------------------------------------------+
              |
              | Delegates to
              v
+------------------------------------------------------------------+
|                        urllib3                                    |
|------------------------------------------------------------------|
|  PoolManager       | Connection pooling                          |
|  HTTPConnectionPool| Per-host connection management              |
|  ProxyManager      | Tunnel through proxies                      |
|  Retry             | Retry configuration                         |
+------------------------------------------------------------------+
```

### How Adapters Decouple Protocol Handling

```python
# Session mounts adapters by URL prefix:
def __init__(self):
    self.adapters = OrderedDict()
    self.mount("https://", HTTPAdapter())
    self.mount("http://", HTTPAdapter())

def get_adapter(self, url):
    """Returns the appropriate adapter for the given URL."""
    for prefix, adapter in self.adapters.items():
        if url.lower().startswith(prefix.lower()):
            return adapter
    raise InvalidSchema(f"No connection adapters were found for {url!r}")

# Users can mount custom adapters:
from requests_ftp import FTPAdapter
session.mount("ftp://", FTPAdapter())
```

### Patterns in Adapters

| Pattern | Implementation | Benefit |
|---------|---------------|---------|
| **Strategy** | Different adapters for different protocols | Runtime protocol selection |
| **Adapter** | HTTPAdapter wraps urllib3's interface | Decouples from urllib3 API |
| **Inversion of Control** | Session calls adapter; adapter returns Response | Adapter controls protocol details |
| **Object Pool** | `poolmanager` caches connections | Performance, resource efficiency |
| **Factory Method** | `proxy_manager_for()` creates managers lazily | On-demand resource creation |

### Key Design Insight: Why Adapters?

```
Without Adapters:                    With Adapters:
-----------------                    ---------------
Session                              Session
  |                                    |
  +-- urllib3 code                     +-- mount("https://", HTTPAdapter())
  +-- proxy code                       +-- mount("ftp://", FTPAdapter())
  +-- retry code                       +-- mount("mock://", MockAdapter())
  +-- SSL code                         |
  |                                    v
  v                                  adapter.send()  <-- All protocol details here
(tightly coupled)                    (loosely coupled)
```

---

中文说明（Chinese Explanation）:

**为何需要 Adapter 抽象？**

1. **策略模式（Strategy）**
   - 不同协议有不同的 Adapter
   - HTTP/HTTPS 使用 HTTPAdapter
   - 社区提供 FTP、Mock 等扩展

2. **适配器模式（Adapter）**
   - HTTPAdapter 将 urllib3 的接口适配为 requests 的接口
   - 隔离第三方库的 API 变化
   - `build_response()` 将 urllib3.Response 转为 requests.Response

3. **控制反转（IoC）**
   - Session 不直接调用 urllib3
   - 通过 adapter 间接调用
   - 方便测试（可注入 MockAdapter）

4. **连接池复用**
   - HTTPAdapter 持有 PoolManager
   - 同一 host 的请求复用连接
   - 显著提升性能

---

## STEP 5: DATA OBJECTS & IMMUTABILITY BOUNDARIES

### The Request Lifecycle

```
User Input                    Mutable                      Immutable(ish)
----------                    -------                      -------------
Request(                      PreparedRequest(             Response(
  method,                       method,                      status_code,
  url,                          url,          send()         headers,
  headers,      prepare()       headers,    -------->        content,
  data,        --------->       body,                        cookies,
  params,                       _cookies,                    ...
  ...                           ...                        )
)                             )
```

### Request vs PreparedRequest

| Aspect | Request | PreparedRequest |
|--------|---------|-----------------|
| **Role** | User's intent | Wire-ready bytes |
| **URL** | String, may be invalid | Normalized, encoded |
| **Headers** | Dict | CaseInsensitiveDict |
| **Body** | dict/list/bytes/file | bytes (encoded) |
| **Cookies** | dict or CookieJar | Merged, serialized to header |
| **Auth** | tuple or AuthBase | Applied to headers |
| **Mutability** | Designed for user modification | Should not be modified after prepare |

### Response Immutability

```python
class Response:
    # Effectively immutable after creation:
    status_code = None        # Set once by adapter
    headers = CaseInsensitiveDict()  # Set once
    url = None                # Set once
    reason = None             # Set once
    
    # Lazily computed, cached:
    _content = False          # Cached after first access
    _content_consumed = False
    
    @property
    def content(self):
        if self._content is False:
            # Read and cache
            self._content = b"".join(self.iter_content(CHUNK_SIZE))
        self._content_consumed = True
        return self._content
    
    @property
    def text(self):
        # Decodes self.content using encoding
        ...
    
    def json(self, **kwargs):
        # Parses self.text as JSON
        ...
```

### Patterns in Data Objects

| Pattern | Where | Implementation |
|---------|-------|---------------|
| **Builder** | `PreparedRequest.prepare()` | Step-by-step construction |
| **DTO (Data Transfer Object)** | Request, Response | Carry data between layers |
| **Fluent Interface** | `Request.prepare()` returns PreparedRequest | Method chaining possible |
| **Lazy Initialization** | `Response.content`, `Response.text` | Compute on demand |
| **Copy-on-Modify** | `PreparedRequest.copy()` | Safe redirect handling |
| **Controlled Mutability** | `PreparedRequest` mutable during prepare only | Prevent accidental changes |

---

中文说明（Chinese Explanation）:

**数据对象的设计思想：**

1. **Request vs PreparedRequest**
   - Request：用户友好，接受多种输入格式
   - PreparedRequest：协议友好，已编码为字节流
   - 分离关注点：用户界面 vs 网络协议

2. **Response 的惰性计算**
   - `content` 属性首次访问时读取并缓存
   - `text` 属性基于 `content` 解码
   - `json()` 方法基于 `text` 解析
   - 避免重复读取和计算

3. **不可变性边界**
   - Response 创建后不应修改
   - 但 `history` 列表会被追加
   - 设计上是"有效不可变"（effectively immutable）

4. **构建者模式（Builder）**
   - `PreparedRequest.prepare()` 分步构建
   - `prepare_method()`, `prepare_url()`, `prepare_headers()`...
   - 顺序有意义（如 auth 必须在最后）

---

## STEP 6: ERROR HANDLING & EXTENSION POINTS

### Exception Hierarchy

```
IOError (stdlib)
    |
    v
RequestException              <-- Base for all requests errors
    |
    +-- InvalidJSONError
    |       +-- JSONDecodeError (also inherits json.JSONDecodeError)
    |
    +-- HTTPError             <-- Server returned 4xx/5xx
    |
    +-- ConnectionError       <-- Network-level failures
    |       +-- ProxyError
    |       +-- SSLError
    |
    +-- Timeout
    |       +-- ConnectTimeout (also inherits ConnectionError)
    |       +-- ReadTimeout
    |
    +-- URLRequired
    +-- TooManyRedirects
    +-- MissingSchema (also inherits ValueError)
    +-- InvalidSchema (also inherits ValueError)
    +-- InvalidURL (also inherits ValueError)
    |       +-- InvalidProxyURL
    +-- InvalidHeader (also inherits ValueError)
    +-- ChunkedEncodingError
    +-- ContentDecodingError (also inherits urllib3.HTTPError)
    +-- StreamConsumedError (also inherits TypeError)
    +-- RetryError
    +-- UnrewindableBodyError

RequestsWarning (Warning)
    +-- FileModeWarning (also DeprecationWarning)
    +-- RequestsDependencyWarning
```

### Why Exceptions are Part of Public API

```python
# Users catch specific exception types:
try:
    r = requests.get(url, timeout=5)
    r.raise_for_status()
except requests.ConnectionError:
    print("Network unreachable")
except requests.Timeout:
    print("Request timed out")
except requests.HTTPError as e:
    print(f"Server error: {e.response.status_code}")
except requests.RequestException:
    print("Something went wrong")
```

**Design Principles:**

1. **Single root**: All inherit from `RequestException`
2. **Semantic grouping**: `Timeout` catches both Connect and Read timeouts
3. **Multiple inheritance**: Some inherit stdlib types for compatibility
4. **Attached context**: Exceptions carry `request` and `response` objects

### Extension Points

#### 1. Hooks System

```python
# Hook signature:
def my_hook(response, **kwargs):
    print(f"Got response: {response.status_code}")
    return response  # Must return response or None

# Registration:
session.hooks['response'].append(my_hook)
# Or per-request:
requests.get(url, hooks={'response': [my_hook]})
```

#### 2. Custom Adapters

```python
class MyAdapter(HTTPAdapter):
    def send(self, request, **kwargs):
        # Add custom header
        request.headers['X-Custom'] = 'value'
        # Maybe log, retry, etc.
        return super().send(request, **kwargs)

session.mount("https://api.example.com/", MyAdapter())
```

#### 3. Custom Authentication

```python
class TokenAuth(AuthBase):
    def __init__(self, token):
        self.token = token
    
    def __call__(self, r):
        r.headers['Authorization'] = f'Bearer {self.token}'
        return r

requests.get(url, auth=TokenAuth('my-token'))
```

#### 4. Session Configuration

```python
session = requests.Session()
session.headers.update({'User-Agent': 'MyApp/1.0'})
session.verify = '/path/to/ca-bundle.crt'
session.proxies = {'https': 'http://proxy:8080'}
session.max_redirects = 5
```

---

中文说明（Chinese Explanation）:

**异常设计的考量：**

1. **层次结构清晰**
   - `RequestException` 是所有异常的基类
   - 可以用基类捕获所有请求相关异常
   - 也可以精确捕获特定类型

2. **语义分组**
   - `Timeout` 同时覆盖连接超时和读取超时
   - `ConnectionError` 覆盖所有网络级错误

3. **附带上下文**
   - 异常对象包含 `request` 和 `response`
   - 方便调试和重试逻辑

**扩展点设计：**

1. **Hooks（钩子）**
   - 事件驱动的扩展机制
   - 目前仅支持 `response` 事件
   - 可用于日志、监控、修改响应

2. **Adapter（适配器）**
   - 继承 HTTPAdapter 添加自定义逻辑
   - 挂载到特定 URL 前缀

3. **Auth（认证）**
   - 实现 `__call__` 方法的 callable
   - 接收 PreparedRequest，返回修改后的请求

---

## STEP 7: DEPENDENCY DIRECTION & RULES

### Forbidden Dependencies

```
NEVER import upward in the layer stack:

+------------------+
|     api.py       |  <-- NEVER import from sessions or below
+------------------+
        |
        v (may import)
+------------------+
|   sessions.py    |  <-- NEVER import from api.py
+------------------+
        |
        v (may import)
+------------------+
|   adapters.py    |  <-- NEVER import from sessions or api
+------------------+
        |
        v (may import)
+------------------+
|    models.py     |  <-- NEVER import from adapters, sessions, or api
+------------------+
        |
        v (may import)
+------------------+
| utils, structures|  <-- NEVER import from any layer above
|   hooks, compat  |
+------------------+
```

### Dependency-Free Modules

These modules MUST remain dependency-free (or stdlib only):

| Module | Why |
|--------|-----|
| `compat.py` | Python version abstraction; imported everywhere |
| `hooks.py` | Simple event dispatch; no external dependencies |
| `exceptions.py` | Exception definitions; imported by all modules |
| `_internal_utils.py` | Low-level helpers; no circular risk |

### How Circular Dependencies are Avoided

**Strategy 1: Layered Architecture**
```
Upper layers import lower layers, never reverse.
```

**Strategy 2: Interface Segregation**
```python
# adapters.py does NOT import Session
# Instead, Session calls adapter.send()
# Adapter only knows about PreparedRequest and Response
```

**Strategy 3: Late Binding**
```python
# In models.py, Response stores connection:
response.connection = self  # Set by adapter

# Later, auth.py can resend:
_r = r.connection.send(prep, **kwargs)
# Without importing adapters.py directly
```

**Strategy 4: Re-exports for Backward Compatibility**
```python
# sessions.py re-exports from models.py:
from .models import (  # noqa: F401
    DEFAULT_REDIRECT_LIMIT,
    REDIRECT_STATI,
    PreparedRequest,
    Request,
)
# This is for users who import from sessions, not internal use
```

### Potential Violation Points

| Location | Risk | Current Mitigation |
|----------|------|-------------------|
| `models.py` importing `auth.py` | Could create cycle | auth is leaf module |
| `adapters.py` importing `models.py` | OK (lower imports lower) | Correct direction |
| `sessions.py` importing many modules | Complex but OK | All imports are downward |
| Type hints in `adapters.py` | Could import from models | Uses `TYPE_CHECKING` guard |

```python
# Safe type hint import pattern (adapters.py):
if typing.TYPE_CHECKING:
    from .models import PreparedRequest
```

---

中文说明（Chinese Explanation）:

**依赖方向规则：**

1. **上层依赖下层，禁止反向**
   - api.py 可以导入 sessions.py
   - sessions.py 不能导入 api.py

2. **基础模块保持独立**
   - compat.py 只依赖标准库
   - exceptions.py 只依赖 compat.py
   - hooks.py 无任何依赖

3. **避免循环依赖的技巧**
   - 后期绑定：通过属性传递引用
   - 类型检查守卫：`if TYPE_CHECKING:`
   - 重导出仅用于公共 API 兼容

4. **潜在风险点**
   - models.py 导入 auth.py（目前安全）
   - 未来添加新功能时需注意依赖方向

---

## STEP 8: PATTERNS SUMMARY

| Pattern | Location | Why Chosen | Problem Solved |
|---------|----------|------------|----------------|
| **Facade** | `api.py`, `Session` | Hide complexity | Users don't need to understand internals |
| **Strategy** | Adapters, Auth handlers | Runtime behavior selection | Support multiple protocols/auth schemes |
| **Adapter** | `HTTPAdapter` | Interface translation | Decouple from urllib3 API |
| **Builder** | `PreparedRequest.prepare()` | Step-by-step construction | Complex object creation |
| **Template Method** | `Session.request()` flow | Standardized algorithm | Consistent request lifecycle |
| **Context Object** | `Session` | Carry state across calls | Cookie/connection persistence |
| **Inversion of Control** | Session → Adapter | Decouple policy from mechanism | Pluggable transports |
| **Object Pool** | `HTTPAdapter.poolmanager` | Resource reuse | Connection pooling |
| **Factory Method** | `proxy_manager_for()` | Lazy creation | On-demand resource allocation |
| **Hook/Callback** | `hooks.py` | Event-driven extension | User customization without subclassing |
| **DTO** | `Request`, `Response` | Data carrier | Separate data from behavior |
| **Lazy Initialization** | `Response.content` | Defer computation | Memory efficiency, optional features |
| **Mixin** | `SessionRedirectMixin` | Compose behavior | Separate redirect logic |
| **Exception Hierarchy** | `exceptions.py` | Semantic error handling | Precise error catching |
| **Singleton-ish** | Default `Session` in `api.py` | Convenience | One-off requests without setup |
| **Copy-on-Modify** | `PreparedRequest.copy()` | Immutability | Safe redirect handling |

---

中文说明（Chinese Explanation）:

**设计模式选择的理由：**

1. **门面模式（Facade）**
   - 问题：用户只想发请求，不想了解 Session/Adapter
   - 方案：api.py 提供简单函数

2. **策略模式（Strategy）**
   - 问题：不同协议有不同实现
   - 方案：Adapter 接口 + 多个实现类

3. **适配器模式（Adapter）**
   - 问题：urllib3 API 可能变化
   - 方案：HTTPAdapter 封装，隔离变化

4. **模板方法（Template Method）**
   - 问题：请求流程固定，但步骤可变
   - 方案：Session.request() 定义骨架

5. **惰性初始化（Lazy Initialization）**
   - 问题：Response.content 可能很大
   - 方案：首次访问时才读取

6. **钩子/回调（Hook/Callback）**
   - 问题：用户想在请求完成时执行自定义逻辑
   - 方案：hooks 系统允许注册回调

---

## STEP 9: USER-SPACE TRANSFER

### Abstract Architecture Template

```
+============================================================================+
|                              API LAYER                                      |
|----------------------------------------------------------------------------|
|  - Stateless convenience functions                                         |
|  - Minimal code, maximum usability                                         |
|  - Hides all internal complexity                                           |
|  - Entry point for simple use cases                                        |
+============================================================================+
                                    |
                                    v
+============================================================================+
|                         COORDINATION LAYER                                  |
|----------------------------------------------------------------------------|
|  - Stateful orchestrator (Session/Client/Context)                          |
|  - Owns configuration, credentials, connection state                       |
|  - Merges defaults with per-request options                                |
|  - Handles retries, redirects, error recovery                              |
|  - Dispatches to appropriate backend                                       |
+============================================================================+
                                    |
                                    v
+============================================================================+
|                           BACKEND LAYER                                     |
|----------------------------------------------------------------------------|
|  - Protocol-specific implementations (Adapters)                            |
|  - Translates abstract requests to wire format                             |
|  - Manages connections/pools                                               |
|  - Pluggable via interface/protocol                                        |
+============================================================================+
                                    |
                                    v
+============================================================================+
|                         DATA MODEL LAYER                                    |
|----------------------------------------------------------------------------|
|  - Request/Response objects (DTOs)                                         |
|  - User-friendly input → Protocol-ready output                             |
|  - Lazy evaluation for expensive operations                                |
|  - Immutable after creation (where possible)                               |
+============================================================================+
                                    |
                                    v
+============================================================================+
|                         FOUNDATION LAYER                                    |
|----------------------------------------------------------------------------|
|  - Utilities, structures, compatibility                                    |
|  - Exception hierarchy                                                     |
|  - No dependencies on upper layers                                         |
+============================================================================+
```

### Applying to Other Domains

#### Database Client (e.g., psycopg, redis-py)

```
API Layer:        redis.get(key), redis.set(key, value)
Coordination:     Connection pool manager, pipeline support
Backend:          TCP adapter, Unix socket adapter
Data Model:       Command objects, Response objects
Foundation:       Serializers, connection string parser
```

#### RPC Client (e.g., gRPC, Thrift)

```
API Layer:        client.call_method(args)
Coordination:     Channel manager, load balancer, retry logic
Backend:          HTTP/2 transport, custom protocol transport
Data Model:       Request message, Response message, Protobuf
Foundation:       Serialization, service discovery
```

#### Plugin-Based Tool (e.g., pytest, flake8)

```
API Layer:        pytest.main(), command-line interface
Coordination:     Plugin manager, hook dispatcher
Backend:          Test runners, collectors, reporters
Data Model:       Test items, Test results
Foundation:       Config parser, path utilities
```

### The "Requests Pattern" Checklist

When designing a client library, ask:

1. **API Layer**
   - [ ] Can users accomplish 80% of tasks with simple function calls?
   - [ ] Is the API consistent and discoverable?
   - [ ] Are advanced features opt-in, not required?

2. **Coordination Layer**
   - [ ] Is there a Session/Context object for stateful usage?
   - [ ] Are defaults sensible but overridable?
   - [ ] Is configuration merged predictably?

3. **Backend Layer**
   - [ ] Can the transport be swapped without changing user code?
   - [ ] Is the external dependency isolated?
   - [ ] Is connection pooling handled transparently?

4. **Data Model Layer**
   - [ ] Are input types forgiving (accept dict, list, object)?
   - [ ] Are output types convenient (`.json()`, `.text`)?
   - [ ] Is expensive work deferred (lazy properties)?

5. **Foundation Layer**
   - [ ] Are utilities dependency-free?
   - [ ] Is the exception hierarchy clear?
   - [ ] Can any module be imported without side effects?

---

中文说明（Chinese Explanation）:

**架构迁移到其他领域：**

1. **数据库客户端**
   ```
   API层：         db.query(sql), db.execute(sql)
   协调层：        连接池管理、事务管理
   后端层：        TCP/Unix Socket 适配器
   数据模型层：    Query对象、ResultSet对象
   基础层：        SQL解析、连接字符串解析
   ```

2. **RPC客户端**
   ```
   API层：         client.call(method, args)
   协调层：        负载均衡、重试、超时
   后端层：        HTTP/2、gRPC、Thrift协议
   数据模型层：    Protobuf消息
   基础层：        序列化、服务发现
   ```

3. **插件系统**
   ```
   API层：         tool.run()
   协调层：        插件管理器、钩子分发
   后端层：        各插件实现
   数据模型层：    插件配置、执行结果
   基础层：        配置解析、路径工具
   ```

**设计检查清单：**

- API是否足够简单？
- Session是否管理好状态？
- 后端是否可插拔？
- 数据模型是否清晰？
- 基础模块是否独立？

---

## Summary Diagram

```
+------------------------------------------------------------------------+
|                    PSF/REQUESTS ARCHITECTURE                            |
|                                                                        |
|   "Human-friendly HTTP" achieved through layered abstraction:          |
|                                                                        |
|   requests.get(url)                                                    |
|        |                                                               |
|        v                                                               |
|   +----------+    +---------+    +---------+    +----------+           |
|   |  api.py  | -> | Session | -> | Adapter | -> | urllib3  |           |
|   | (facade) |    | (state) |    | (bridge)|    | (engine) |           |
|   +----------+    +---------+    +---------+    +----------+           |
|        |              |              |               |                 |
|        v              v              v               v                 |
|   +----+----+    +----+----+    +----+----+    +----+----+             |
|   | Request |    |Prepared |    | Response|    |HTTP/1.1 |             |
|   |  (user) |    | Request |    |  (rich) |    |  bytes  |             |
|   +---------+    +---------+    +---------+    +---------+             |
|                                                                        |
|   Key Patterns:                                                        |
|   - Facade: Hide complexity behind simple functions                    |
|   - Strategy: Pluggable adapters for different protocols              |
|   - Builder: Prepare requests in controlled steps                      |
|   - Lazy Init: Defer expensive operations                             |
|                                                                        |
|   Design Principles:                                                   |
|   - Dependency flows downward only                                     |
|   - Public API is minimal and stable                                  |
|   - Extension via composition (adapters, hooks, auth)                  |
|   - Sensible defaults, everything overridable                          |
+------------------------------------------------------------------------+
```

---

## 总结（Chinese Summary）

`requests` 库的架构是 Python 库设计的典范：

1. **分层清晰**：api → sessions → adapters → models → utils
2. **职责单一**：每个模块只做一件事
3. **依赖向下**：上层依赖下层，杜绝循环
4. **扩展友好**：Adapter、Hook、Auth 三种扩展机制
5. **用户优先**：简单接口，智能默认值，丰富的错误信息

学习 `requests` 的架构，可以应用到：
- 数据库客户端设计
- RPC框架设计
- 任何需要"简单接口 + 复杂内部"的库

核心思想：**让用户的代码尽可能简单，把复杂度藏在库里。**

