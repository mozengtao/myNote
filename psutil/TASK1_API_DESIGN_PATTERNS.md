# Task 1: psutil API 设计模式分析

## 概述

本文档深入分析 psutil 的 API 设计模式，重点对比与 C 系统编程的差异，包括函数命名规范、错误处理机制和资源管理策略。

---

## 1. 函数命名规范

### 1.1 C 系统调用 vs psutil 命名对比

| C 系统调用 | psutil 等价接口 | 说明 |
|-----------|----------------|------|
| `getpid()` | `os.getpid()` / `Process().pid` | PID 作为属性而非方法 |
| `getppid()` | `Process().ppid()` | 方法调用，一致性更好 |
| `kill(pid, sig)` | `Process(pid).kill()` | 面向对象，信号隐含 |
| `getpriority()` | `Process().nice()` | 更直观的名称 |
| `setpriority()` | `Process().nice(value)` | getter/setter 合一 |
| `sysinfo()` | `virtual_memory()` | 功能明确的名称 |
| `statvfs()` | `disk_usage(path)` | 意图导向命名 |
| `getrlimit()` | `Process().rlimit()` | 方法调用 |
| `sched_getaffinity()` | `Process().cpu_affinity()` | 简化命名 |
| `ioctl() + SIOCGIFFLAGS` | `net_if_stats()` | 高层抽象 |

### 1.2 Pythonic 命名约定分析

#### 1.2.1 `psutil/__init__.py` 中的命名示例

```python
# 系统级函数 - 使用下划线分隔的小写命名
def cpu_percent(interval=None, percpu=False):
    """Return a float representing the current system-wide CPU utilization."""
    ...

def virtual_memory():
    """Return statistics about system memory usage."""
    ...

def disk_partitions(all=False):
    """Return mounted partitions as a list."""
    ...

# 属性式访问 - Process 类
class Process:
    @property
    def pid(self):
        """The process PID."""
        return self._pid
    
    def name(self):
        """The process name. The return value is cached after first call."""
        ...
```

#### 1.2.2 命名模式对比

| 特征 | C 命名风格 | psutil Python 风格 |
|-----|-----------|-------------------|
| 大小写 | 全小写 (`getpid`) | 小写+下划线 (`cpu_percent`) |
| 前缀 | 系统特定 (`sys_`, `get_`) | 无前缀，意图明确 |
| 动词位置 | 开头 (`getpriority`) | 结尾或省略 (`nice()`) |
| 返回类型 | 隐含在文档中 | 函数名暗示 (`disk_usage`) |
| 命名空间 | 扁平 | 层次化 (`Process.cpu_times()`) |

#### 1.2.3 可读性提升示例

```python
# C 风格 (难以理解)
sysinfo(&info);  // 什么 info? 内存还是 CPU?
statvfs(path, &stat);  // statvfs 是什么意思?

# psutil 风格 (自描述)
memory = psutil.virtual_memory()  # 明确是虚拟内存
disk = psutil.disk_usage("/")     # 明确是磁盘使用情况
```

### 1.3 方法 vs 属性的设计决策

```python
class Process:
    # 属性: 不变的标识符
    @property
    def pid(self):
        """The process PID."""
        return self._pid  # 不可变，使用属性
    
    # 方法: 可能变化的值
    def name(self):
        """The process name."""
        # 名称可能改变 (POSIX)，使用方法
        return self._proc.name()
    
    # 方法: 带参数的操作
    def nice(self, value=None):
        """Get or set process niceness (priority)."""
        if value is None:
            return self._proc.nice_get()  # getter
        else:
            self._proc.nice_set(value)    # setter
```

---

## 2. 错误处理机制

### 2.1 C 错误码到 Python 异常的转换

#### 2.1.1 错误处理对比表

| C 错误机制 | Python 异常 | psutil 异常 |
|-----------|------------|-------------|
| `errno = ESRCH` | `ProcessLookupError` | `NoSuchProcess` |
| `errno = EPERM` | `PermissionError` | `AccessDenied` |
| `errno = EACCES` | `PermissionError` | `AccessDenied` |
| `errno = EINVAL` | `ValueError` | 视情况 |
| 超时/阻塞 | 无标准 | `TimeoutExpired` |
| 僵尸进程 | 无标准 | `ZombieProcess` |

#### 2.1.2 C 扩展中的错误转换 (`arch/all/errors.c`)

```c
// C 层: 将系统错误转换为 Python 异常
PyObject *
psutil_oserror(void) {
#ifdef PSUTIL_WINDOWS
    // Windows: 使用 GetLastError()
    PyErr_SetFromWindowsErr(GetLastError());
#else
    // POSIX: 使用 errno
    PyErr_SetFromErrno(PyExc_OSError);
#endif
    return NULL;
}

// 强制设置 "进程不存在" 错误
PyObject *
psutil_oserror_nsp(const char *syscall) {
    PyObject *exc;
    char msg[512];
    // 创建 OSError(ESRCH, message)
    str_format(msg, sizeof(msg), 
        "force no such process (originated from %s)", syscall);
    exc = PyObject_CallFunction(PyExc_OSError, "(is)", ESRCH, msg);
    PyErr_SetObject(PyExc_OSError, exc);
    Py_XDECREF(exc);
    return NULL;
}

// 强制设置 "权限拒绝" 错误
PyObject *
psutil_oserror_ad(const char *syscall) {
    PyObject *exc;
    char msg[512];
    str_format(msg, sizeof(msg),
        "force permission denied (originated from %s)", syscall);
    exc = PyObject_CallFunction(PyExc_OSError, "(is)", EACCES, msg);
    PyErr_SetObject(PyExc_OSError, exc);
    Py_XDECREF(exc);
    return NULL;
}
```

#### 2.1.3 Python 层的异常转换 (`_common.py`)

```python
# psutil/_common.py 中的异常类层次

class Error(Exception):
    """Base exception class. All other psutil exceptions inherit from this."""
    __module__ = 'psutil'
    
    def __str__(self):
        # 友好的错误信息格式化
        info = self._infodict(("pid", "ppid", "name"))
        if info:
            details = "({})".format(
                ", ".join([f"{k}={v!r}" for k, v in info.items()])
            )
        else:
            details = None
        return " ".join([x for x in (getattr(self, "msg", ""), details) if x])


class NoSuchProcess(Error):
    """Exception raised when a process with a certain PID doesn't
    or no longer exists.
    """
    def __init__(self, pid, name=None, msg=None):
        Error.__init__(self)
        self.pid = pid
        self.name = name
        self.msg = msg or "process no longer exists"


class ZombieProcess(NoSuchProcess):
    """Exception raised when querying a zombie process."""
    def __init__(self, pid, name=None, ppid=None, msg=None):
        NoSuchProcess.__init__(self, pid, name, msg)
        self.ppid = ppid
        self.msg = msg or "PID still exists but it's a zombie"


class AccessDenied(Error):
    """Exception raised when permission to perform an action is denied."""
    def __init__(self, pid=None, name=None, msg=None):
        Error.__init__(self)
        self.pid = pid
        self.name = name
        self.msg = msg or ""


class TimeoutExpired(Error):
    """Raised on Process.wait(timeout) if timeout expires."""
    def __init__(self, seconds, pid=None, name=None):
        Error.__init__(self)
        self.seconds = seconds
        self.pid = pid
        self.name = name
        self.msg = f"timeout after {seconds} seconds"
```

### 2.2 平台特定异常处理

#### 2.2.1 Linux 特定处理 (`_pslinux.py`)

```python
# 读取 /proc 文件时的错误处理
def _read_proc_file(path, pid):
    try:
        with open(path, 'rb') as f:
            return f.read()
    except FileNotFoundError:
        # 进程在读取过程中消失
        raise NoSuchProcess(pid, msg="process no longer exists")
    except PermissionError:
        raise AccessDenied(pid)
```

#### 2.2.2 Windows 特定处理 (`_pswindows.py`)

```python
# Windows 错误码映射
def _check_windows_error(err_code, pid):
    if err_code == ERROR_ACCESS_DENIED:
        raise AccessDenied(pid)
    elif err_code == ERROR_INVALID_PARAMETER:
        raise NoSuchProcess(pid)
    # ... 其他错误码
```

### 2.3 C 错误处理 vs Python 异常优劣对比

| 方面 | C 错误处理 | Python 异常 | 优劣分析 |
|-----|-----------|------------|---------|
| **传播机制** | 手动检查返回值 | 自动向上传播 | Python 更简洁，避免遗漏 |
| **信息携带** | errno + strerror | 异常对象含完整上下文 | Python 信息更丰富 |
| **性能** | 零开销（正常路径） | 异常创建有开销 | C 在正常路径更快 |
| **代码量** | 每次调用都要检查 | 集中 try/except | Python 代码更简洁 |
| **类型安全** | 依赖约定 | 类型层次结构 | Python 更安全 |
| **调试** | 需要 strace/调试器 | 完整堆栈跟踪 | Python 调试更容易 |

#### 示例对比

```c
// C 风格: 繁琐的错误检查
int pid = fork();
if (pid < 0) {
    perror("fork failed");
    return -1;
}
if (kill(target_pid, 0) < 0) {
    if (errno == ESRCH) {
        fprintf(stderr, "Process not found\n");
    } else if (errno == EPERM) {
        fprintf(stderr, "Permission denied\n");
    }
    return -1;
}
```

```python
# Python/psutil 风格: 简洁的异常处理
try:
    p = psutil.Process(target_pid)
    p.kill()
except psutil.NoSuchProcess:
    print("Process not found")
except psutil.AccessDenied:
    print("Permission denied")
```

---

## 3. 资源管理

### 3.1 上下文管理器的使用

#### 3.1.1 `oneshot()` 上下文管理器

```python
# psutil/__init__.py

class Process:
    @contextlib.contextmanager
    def oneshot(self):
        """Utility context manager which considerably speeds up the
        retrieval of multiple process information at the same time.
        
        内部不同进程信息可能使用同一个系统调用获取，但每次只返回一个值。
        使用此上下文管理器时，首次调用会缓存所有数据，后续调用返回缓存值。
        """
        with self._lock:
            if hasattr(self, "_cache"):
                # 已在 oneshot 上下文中，直接返回
                yield
            else:
                try:
                    # 激活各方法的缓存
                    self.cpu_times.cache_activate(self)
                    self.memory_info.cache_activate(self)
                    self.ppid.cache_activate(self)
                    if POSIX:
                        self.uids.cache_activate(self)
                    # 平台特定的缓存
                    self._proc.oneshot_enter()
                    yield
                finally:
                    # 清理缓存
                    self.cpu_times.cache_deactivate(self)
                    self.memory_info.cache_deactivate(self)
                    self.ppid.cache_deactivate(self)
                    if POSIX:
                        self.uids.cache_deactivate(self)
                    self._proc.oneshot_exit()
```

#### 3.1.2 使用示例

```python
import psutil

p = psutil.Process()

# 不使用 oneshot: 多次系统调用
name = p.name()        # 系统调用 1
cpu = p.cpu_times()    # 系统调用 2
mem = p.memory_info()  # 系统调用 3

# 使用 oneshot: 批量获取，减少系统调用
with p.oneshot():
    name = p.name()        # 收集多个信息
    cpu = p.cpu_times()    # 返回缓存值
    mem = p.memory_info()  # 返回缓存值
```

### 3.2 避免资源泄漏的策略

#### 3.2.1 C 扩展中的句柄管理

```c
// arch/windows/proc.c - Windows 句柄管理示例

PyObject *
psutil_proc_times(PyObject *self, PyObject *args) {
    DWORD pid;
    HANDLE hProcess;
    FILETIME ftCreate, ftExit, ftKernel, ftUser;

    if (!PyArg_ParseTuple(args, _Py_PARSE_PID, &pid))
        return NULL;

    // 获取进程句柄
    hProcess = psutil_handle_from_pid(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    if (hProcess == NULL)
        return NULL;
    
    // 执行操作
    if (!GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        // 错误处理 - 注意要关闭句柄!
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            psutil_oserror_nsp("GetProcessTimes -> ERROR_ACCESS_DENIED");
        } else {
            psutil_oserror();
        }
        CloseHandle(hProcess);  // 关键: 错误路径也要关闭
        return NULL;
    }

    CloseHandle(hProcess);  // 正常路径关闭句柄
    return Py_BuildValue("(ddd)", ...);
}
```

#### 3.2.2 Python 层的文件句柄管理

```python
# psutil/_pslinux.py - 安全的文件读取

def virtual_memory():
    """Report virtual memory stats."""
    mems = {}
    # 使用 with 语句确保文件关闭
    with open_binary(f"{get_procfs_path()}/meminfo") as f:
        for line in f:
            fields = line.split()
            mems[fields[0]] = int(fields[1]) * 1024
    
    # 文件已自动关闭，继续处理数据
    total = mems[b'MemTotal:']
    ...
```

### 3.3 C 手动资源管理 vs Python 自动管理对比

#### 3.3.1 内存管理对比

```c
// C: 手动 malloc/free
PyObject *
psutil_proc_cpu_affinity_get(PyObject *self, PyObject *args) {
    cpu_set_t *mask = NULL;
    PyObject *py_list = NULL;
    
    // 分配内存
    mask = CPU_ALLOC(ncpus);
    if (mask == NULL) {
        return PyErr_NoMemory();
    }
    
    py_list = PyList_New(0);
    if (py_list == NULL)
        goto error;  // 必须跳转到清理代码
    
    // ... 操作 ...
    
    CPU_FREE(mask);  // 正常路径释放
    return py_list;

error:
    // 错误路径释放
    if (mask)
        CPU_FREE(mask);
    Py_XDECREF(py_list);
    return NULL;
}
```

```python
# Python: 自动内存管理
def cpu_affinity():
    # 无需手动分配/释放
    affinity_list = []
    for cpu in range(cpu_count()):
        if is_cpu_in_affinity(cpu):
            affinity_list.append(cpu)
    return affinity_list  # 垃圾回收器处理
```

#### 3.3.2 资源管理总结表

| 资源类型 | C 管理方式 | Python/psutil 管理方式 |
|---------|-----------|----------------------|
| 内存 | `malloc()`/`free()` | 引用计数 + GC |
| 文件描述符 | `open()`/`close()` | `with` 语句 |
| 进程句柄 | `OpenProcess()`/`CloseHandle()` | 自动包装 |
| 锁 | `pthread_mutex_lock()`/`unlock()` | `with self._lock:` |
| 临时文件 | `mkstemp()`/`unlink()` | `tempfile` 模块 |

### 3.4 线程安全的资源访问

```python
# psutil/__init__.py

class Process:
    def __init__(self, pid=None):
        # 每个 Process 实例有自己的锁
        self._lock = threading.RLock()
        ...
    
    @contextlib.contextmanager
    def oneshot(self):
        # 使用锁保护缓存操作
        with self._lock:
            ...
```

---

## 4. 设计模式总结

### 4.1 psutil 使用的主要设计模式

| 设计模式 | 应用场景 | 示例 |
|---------|---------|------|
| **外观模式 (Facade)** | 统一 API 隐藏平台差异 | `psutil.cpu_percent()` |
| **策略模式 (Strategy)** | 平台特定实现 | `_psplatform` 模块 |
| **工厂模式 (Factory)** | 创建平台适配器 | `Process()` 类 |
| **单例模式 (Singleton)** | 缓存系统信息 | `@memoize` 装饰器 |
| **代理模式 (Proxy)** | `Process` 包装底层 `_proc` | `self._proc.name()` |
| **模板方法 (Template)** | `oneshot()` 上下文 | 缓存激活/停用流程 |

### 4.2 最佳实践建议

1. **命名**：使用描述性名称，优先意图而非实现
2. **错误处理**：使用语义化异常，携带足够上下文
3. **资源管理**：优先使用上下文管理器
4. **文档**：为每个公共 API 提供清晰的文档字符串
5. **一致性**：保持 API 风格在整个库中统一

