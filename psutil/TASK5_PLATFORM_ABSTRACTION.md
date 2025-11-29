# Task 5: psutil 平台抽象层设计分析

## 概述

本文档分析 psutil 的平台抽象层设计，包括通用接口设计、平台检测机制、回退策略和测试策略。

---

## 1. 通用接口设计

### 1.1 `arch/all/` 中的通用函数

```c
// arch/all/init.c - 全平台共享的初始化

int PSUTIL_DEBUG = 0;
int PSUTIL_TESTING = 0;
int PSUTIL_CONN_NONE = 128;

// 启用/禁用调试输出
PyObject *
psutil_set_debug(PyObject *self, PyObject *args) {
    PyObject *value;
    int x;

    if (!PyArg_ParseTuple(args, "O", &value))
        return NULL;
    x = PyObject_IsTrue(value);
    if (x < 0) {
        return NULL;
    } else if (x == 0) {
        PSUTIL_DEBUG = 0;
    } else {
        PSUTIL_DEBUG = 1;
    }
    Py_RETURN_NONE;
}

// 所有平台的模块初始化调用此函数
int
psutil_setup(void) {
    if (getenv("PSUTIL_DEBUG") != NULL)
        PSUTIL_DEBUG = 1;
    if (getenv("PSUTIL_TESTING") != NULL)
        PSUTIL_TESTING = 1;
    return 0;
}
```

### 1.2 `arch/all/errors.c` - 统一错误处理

```c
// 跨平台错误处理

// 设置 OSError
PyObject *
psutil_oserror(void) {
#ifdef PSUTIL_WINDOWS
    PyErr_SetFromWindowsErr(GetLastError());
#else
    PyErr_SetFromErrno(PyExc_OSError);
#endif
    return NULL;
}

// 带系统调用名称的 OSError
PyObject *
psutil_oserror_wsyscall(const char *syscall) {
    char msg[512];

#ifdef PSUTIL_WINDOWS
    DWORD err = GetLastError();
    str_format(msg, sizeof(msg), "(originated from %s)", syscall);
    PyErr_SetFromWindowsErrWithFilename(err, msg);
#else
    PyObject *exc;
    str_format(msg, sizeof(msg), "%s (originated from %s)", 
               strerror(errno), syscall);
    exc = PyObject_CallFunction(PyExc_OSError, "(is)", errno, msg);
    PyErr_SetObject(PyExc_OSError, exc);
    Py_XDECREF(exc);
#endif
    return NULL;
}

// NoSuchProcess 错误
PyObject *
psutil_oserror_nsp(const char *syscall) {
    PyObject *exc;
    char msg[512];
    str_format(msg, sizeof(msg), 
               "force no such process (originated from %s)", syscall);
    exc = PyObject_CallFunction(PyExc_OSError, "(is)", ESRCH, msg);
    PyErr_SetObject(PyExc_OSError, exc);
    Py_XDECREF(exc);
    return NULL;
}

// AccessDenied 错误
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

### 1.3 `arch/all/init.h` - 统一头文件

```c
// arch/all/init.h - 所有 C 文件的唯一入口点

#include <Python.h>

// 根据平台宏自动包含对应的头文件
// clang-format off
#if defined(PSUTIL_POSIX)
    #include "../../arch/posix/init.h"
#endif
#if defined(PSUTIL_BSD)
    #include "../../arch/bsd/init.h"
#endif

#if defined(PSUTIL_LINUX)
    #include "../../arch/linux/init.h"
#elif defined(PSUTIL_WINDOWS)
    #include "../../arch/windows/init.h"
#elif defined(PSUTIL_OSX)
    #include "../../arch/osx/init.h"
#elif defined(PSUTIL_FREEBSD)
    #include "../../arch/freebsd/init.h"
#elif defined(PSUTIL_OPENBSD)
    #include "../../arch/openbsd/init.h"
#elif defined(PSUTIL_NETBSD)
    #include "../../arch/netbsd/init.h"
#elif defined(PSUTIL_SUNOS)
    #include "../../arch/sunos/init.h"
#endif
// clang-format on

// 全局变量声明
extern int PSUTIL_DEBUG;
extern int PSUTIL_CONN_NONE;
extern int PSUTIL_TESTING;

// GIL 相关
#ifdef Py_GIL_DISABLED
    extern PyMutex utxent_lock;
    #define UTXENT_MUTEX_LOCK() PyMutex_Lock(&utxent_lock)
    #define UTXENT_MUTEX_UNLOCK() PyMutex_Unlock(&utxent_lock)
#else
    #define UTXENT_MUTEX_LOCK()
    #define UTXENT_MUTEX_UNLOCK()
#endif

// 调试宏
#define psutil_debug(...)                                              \
    do {                                                               \
        if (!PSUTIL_DEBUG)                                             \
            break;                                                     \
        fprintf(stderr, "psutil-debug [%s:%d]> ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                                  \
        fprintf(stderr, "\n");                                         \
    } while (0)

// 通用函数声明
PyObject *psutil_oserror(void);
PyObject *psutil_oserror_ad(const char *msg);
PyObject *psutil_oserror_nsp(const char *msg);
PyObject *psutil_oserror_wsyscall(const char *syscall);
PyObject *psutil_runtime_error(const char *msg, ...);

int str_append(char *dst, size_t dst_size, const char *src);
int str_copy(char *dst, size_t dst_size, const char *src);
int str_format(char *buf, size_t size, const char *fmt, ...);

int psutil_badargs(const char *funcname);
int psutil_setup(void);

// 平台特定的导出函数
#if defined(PSUTIL_WINDOWS) || defined(PSUTIL_BSD) || defined(PSUTIL_OSX)
PyObject *psutil_pids(PyObject *self, PyObject *args);
#endif
PyObject *psutil_set_debug(PyObject *self, PyObject *args);
PyObject *psutil_check_pid_range(PyObject *self, PyObject *args);
```

### 1.4 条件编译技巧

```c
// arch/posix/init.c - POSIX 共享方法

static PyMethodDef posix_methods[] = {
    // 所有 POSIX 系统共享
    {"getpagesize", psutil_getpagesize_pywrapper, METH_VARARGS},
    {"net_if_addrs", psutil_net_if_addrs, METH_VARARGS},
    {"net_if_flags", psutil_net_if_flags, METH_VARARGS},
    {"proc_priority_get", psutil_proc_priority_get, METH_VARARGS},
    {"proc_priority_set", psutil_proc_priority_set, METH_VARARGS},
    
    // 仅 BSD/macOS
#if defined(PSUTIL_BSD) || defined(PSUTIL_OSX)
    {"net_if_duplex_speed", psutil_net_if_duplex_speed, METH_VARARGS},
#endif

    // 排除 OpenBSD 和 AIX
#if !defined(PSUTIL_OPENBSD) && !defined(PSUTIL_AIX)
    {"users", psutil_users, METH_VARARGS},
#endif

    // 仅 macOS/BSD 的僵尸进程检测
#if defined(PSUTIL_OSX) || defined(PSUTIL_BSD)
    {"proc_is_zombie", psutil_proc_is_zombie, METH_VARARGS},
#endif

    {NULL, NULL, 0, NULL}
};
```

---

## 2. 平台检测机制

### 2.1 Python 层的平台检测 (`_common.py`)

```python
# psutil/_common.py

import os
import sys

# 操作系统家族
POSIX = os.name == "posix"
WINDOWS = os.name == "nt"

# 具体操作系统
LINUX = sys.platform.startswith("linux")
MACOS = sys.platform.startswith("darwin")
OSX = MACOS  # 兼容别名
FREEBSD = sys.platform.startswith(("freebsd", "midnightbsd"))
OPENBSD = sys.platform.startswith("openbsd")
NETBSD = sys.platform.startswith("netbsd")
BSD = FREEBSD or OPENBSD or NETBSD
SUNOS = sys.platform.startswith(("sunos", "solaris"))
AIX = sys.platform.startswith("aix")
```

### 2.2 C 层的平台检测 (`setup.py`)

```python
# setup.py

# 预处理器宏定义
macros = []

if POSIX:
    macros.append(("PSUTIL_POSIX", 1))
if BSD:
    macros.append(("PSUTIL_BSD", 1))
if LINUX:
    macros.append(("PSUTIL_LINUX", 1))
if WINDOWS:
    macros.append(("PSUTIL_WINDOWS", 1))
if MACOS:
    macros.extend([("PSUTIL_OSX", 1), ("PSUTIL_MACOS", 1)])
if FREEBSD:
    macros.append(("PSUTIL_FREEBSD", 1))
if OPENBSD:
    macros.append(("PSUTIL_OPENBSD", 1))
if NETBSD:
    macros.append(("PSUTIL_NETBSD", 1))
if SUNOS:
    macros.append(("PSUTIL_SUNOS", 1))
if AIX:
    macros.append(("PSUTIL_AIX", 1))
```

### 2.3 运行时功能检测

```python
# psutil/__init__.py

# 动态导入平台模块
if LINUX:
    from . import _pslinux as _psplatform
elif WINDOWS:
    from . import _pswindows as _psplatform
elif MACOS:
    from . import _psosx as _psplatform
elif BSD:
    from . import _psbsd as _psplatform
elif SUNOS:
    from . import _pssunos as _psplatform
elif AIX:
    from . import _psaix as _psplatform
else:
    raise NotImplementedError(f"platform {sys.platform} is not supported")

# 功能可用性检测
if hasattr(_psplatform.Process, "rlimit"):
    # 导出 RLIM* 常量
    _globals = globals()
    for _name in dir(_psplatform.cext):
        if _name.startswith('RLIM') and _name.isupper():
            _globals[_name] = getattr(_psplatform.cext, _name)
            __all__.append(_name)

# 条件性添加功能
if hasattr(_psplatform, "cpu_freq"):
    def cpu_freq(percpu=False):
        """Return CPU frequency as a namedtuple."""
        return _psplatform.cpu_freq()
    __all__.append("cpu_freq")

if hasattr(_psplatform, "sensors_temperatures"):
    def sensors_temperatures(fahrenheit=False):
        """Return hardware temperatures."""
        # ...
    __all__.append("sensors_temperatures")
```

---

## 3. 回退机制设计

### 3.1 功能降级策略

```python
# 如果平台不支持某功能，提供回退

# 示例：ppid_map 回退
if hasattr(_psplatform, 'ppid_map'):
    # Linux/Windows 有快速实现
    _ppid_map = _psplatform.ppid_map
else:
    # 其他平台的通用回退实现
    def _ppid_map():
        """Return a {pid: ppid, ...} dict for all running processes."""
        ret = {}
        for pid in pids():
            try:
                ret[pid] = _psplatform.Process(pid).ppid()
            except (NoSuchProcess, ZombieProcess):
                pass
        return ret
```

### 3.2 Linux 内核版本兼容

```python
# psutil/_pslinux.py

@memoize
def _scputimes_ntuple(procfs_path):
    """根据内核版本动态创建 CPU 时间命名元组。"""
    with open_binary(f"{procfs_path}/stat") as f:
        values = f.readline().split()[1:]
    
    # 基础字段（所有版本）
    fields = ['user', 'nice', 'system', 'idle', 'iowait', 'irq', 'softirq']
    
    vlen = len(values)
    if vlen >= 8:
        fields.append('steal')       # Linux >= 2.6.11
    if vlen >= 9:
        fields.append('guest')       # Linux >= 2.6.24
    if vlen >= 10:
        fields.append('guest_nice')  # Linux >= 3.2.0
    
    return namedtuple('scputimes', fields)


def virtual_memory():
    """处理不同内核版本的 /proc/meminfo 差异。"""
    # MemAvailable 在 kernel 3.14+ 才有
    try:
        avail = mems[b'MemAvailable:']
    except KeyError:
        # 回退到手动计算
        avail = calculate_avail_vmem(mems)
    
    # 某些老内核可能缺少字段
    try:
        shared = mems[b'Shmem:']  # kernel 2.6.32+
    except KeyError:
        try:
            shared = mems[b'MemShared:']  # kernels 2.4
        except KeyError:
            shared = 0
```

### 3.3 编译时功能检测

```python
# setup.py

if LINUX:
    # 检测是否能编译 ethtool.h
    if not unix_can_compile("#include <linux/ethtool.h>"):
        macros.append(("PSUTIL_ETHTOOL_MISSING_TYPES", 1))

def unix_can_compile(c_code):
    """Test if the C code can be compiled."""
    from distutils.errors import CompileError
    from distutils.unixccompiler import UnixCCompiler

    with tempfile.NamedTemporaryFile(suffix='.c', delete=False, mode="wt") as f:
        f.write(c_code)

    tempdir = tempfile.mkdtemp()
    try:
        compiler = UnixCCompiler()
        if os.getenv('CC'):
            compiler.set_executable('compiler_so', os.getenv('CC'))
        compiler.compile([f.name], output_dir=tempdir)
    except CompileError:
        return False
    else:
        return True
    finally:
        os.remove(f.name)
        shutil.rmtree(tempdir)
```

---

## 4. 测试策略

### 4.1 平台特定测试组织

```
tests/
├── __init__.py          # 测试工具和常量
├── test_process.py      # 进程测试 (跨平台)
├── test_system.py       # 系统测试 (跨平台)
├── test_connections.py  # 网络连接 (跨平台)
├── test_contracts.py    # API 契约测试
├── test_posix.py        # POSIX 专有测试
├── test_linux.py        # Linux 专有测试
├── test_windows.py      # Windows 专有测试
├── test_osx.py          # macOS 专有测试
├── test_bsd.py          # BSD 专有测试
├── test_sunos.py        # Solaris 专有测试
└── test_aix.py          # AIX 专有测试
```

### 4.2 测试工具函数 (`tests/__init__.py`)

```python
# tests/__init__.py

# 平台特性检测
HAS_CPU_AFFINITY = hasattr(psutil.Process, "cpu_affinity")
HAS_ENVIRON = hasattr(psutil.Process, "environ")
HAS_IONICE = hasattr(psutil.Process, "ionice")
HAS_MEMORY_MAPS = hasattr(psutil.Process, "memory_maps")
HAS_PROC_IO_COUNTERS = hasattr(psutil.Process, "io_counters")
HAS_RLIMIT = hasattr(psutil.Process, "rlimit")
HAS_SENSORS_BATTERY = hasattr(psutil, "sensors_battery")
HAS_SENSORS_FANS = hasattr(psutil, "sensors_fans")
HAS_SENSORS_TEMPERATURES = hasattr(psutil, "sensors_temperatures")

# 跳过装饰器
def skip_on_access_denied(only_if=None):
    """Decorator to skip test if AccessDenied is raised."""
    def decorator(fun):
        @functools.wraps(fun)
        def wrapper(*args, **kwargs):
            try:
                return fun(*args, **kwargs)
            except psutil.AccessDenied:
                if only_if is None or only_if:
                    raise unittest.SkipTest("AccessDenied")
                raise
        return wrapper
    return decorator

def skip_on_not_implemented(only_if=None):
    """Decorator to skip test if NotImplementedError is raised."""
    def decorator(fun):
        @functools.wraps(fun)
        def wrapper(*args, **kwargs):
            try:
                return fun(*args, **kwargs)
            except NotImplementedError:
                if only_if is None or only_if:
                    raise unittest.SkipTest("NotImplementedError")
                raise
        return wrapper
    return decorator
```

### 4.3 平台特定测试示例

```python
# tests/test_linux.py

import pytest
import psutil
from psutil import LINUX

pytestmark = pytest.mark.skipif(not LINUX, reason="Linux only")


class TestLinuxProcess:
    """Linux-specific process tests."""
    
    def test_rlimit(self):
        """Test Process.rlimit()."""
        p = psutil.Process()
        soft, hard = p.rlimit(psutil.RLIMIT_NOFILE)
        assert soft > 0
        assert hard > 0
        assert soft <= hard
    
    def test_ionice(self):
        """Test Process.ionice()."""
        p = psutil.Process()
        ioclass, value = p.ionice()
        assert ioclass in (
            psutil.IOPRIO_CLASS_NONE,
            psutil.IOPRIO_CLASS_RT,
            psutil.IOPRIO_CLASS_BE,
            psutil.IOPRIO_CLASS_IDLE,
        )
    
    def test_cpu_affinity(self):
        """Test Process.cpu_affinity()."""
        p = psutil.Process()
        affinity = p.cpu_affinity()
        assert isinstance(affinity, list)
        assert len(affinity) > 0


# tests/test_windows.py

import pytest
import psutil
from psutil import WINDOWS

pytestmark = pytest.mark.skipif(not WINDOWS, reason="Windows only")


class TestWindowsProcess:
    """Windows-specific process tests."""
    
    def test_num_handles(self):
        """Test Process.num_handles()."""
        p = psutil.Process()
        handles = p.num_handles()
        assert handles > 0
    
    def test_ionice(self):
        """Test Process.ionice() on Windows."""
        p = psutil.Process()
        ionice = p.ionice()
        assert ionice in (
            psutil.IOPRIO_HIGH,
            psutil.IOPRIO_NORMAL,
            psutil.IOPRIO_LOW,
            psutil.IOPRIO_VERYLOW,
        )
```

---

## 5. 平台抽象层 UML 图

### 5.1 类图

```
┌─────────────────────────────────────────────────────────────────────┐
│                           psutil                                     │
│                        (公共 API)                                    │
├─────────────────────────────────────────────────────────────────────┤
│ + Process(pid)                                                       │
│ + cpu_percent()                                                      │
│ + virtual_memory()                                                   │
│ + disk_usage()                                                       │
│ + net_connections()                                                  │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ uses
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       _psplatform                                    │
│                    (平台适配器接口)                                  │
├─────────────────────────────────────────────────────────────────────┤
│ + Process(pid)                                                       │
│ + pids()                                                             │
│ + cpu_times()                                                        │
│ + virtual_memory()                                                   │
│ + disk_partitions()                                                  │
│ + net_connections()                                                  │
│ + cext (C 扩展模块)                                                  │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ implements
    ┌───────────────────────────┼───────────────────────────┐
    │                           │                           │
    ▼                           ▼                           ▼
┌───────────────┐       ┌───────────────┐       ┌───────────────┐
│ _pslinux.py   │       │ _pswindows.py │       │ _psosx.py     │
│               │       │               │       │               │
│ + cext =      │       │ + cext =      │       │ + cext =      │
│   _psutil_    │       │   _psutil_    │       │   _psutil_    │
│   linux       │       │   windows     │       │   osx         │
└───────┬───────┘       └───────┬───────┘       └───────┬───────┘
        │                       │                       │
        ▼                       ▼                       ▼
┌───────────────┐       ┌───────────────┐       ┌───────────────┐
│_psutil_linux.c│       │_psutil_       │       │_psutil_osx.c  │
│               │       │windows.c      │       │               │
│ + arch/all/   │       │ + arch/all/   │       │ + arch/all/   │
│ + arch/posix/ │       │ + arch/       │       │ + arch/posix/ │
│ + arch/linux/ │       │   windows/    │       │ + arch/osx/   │
└───────────────┘       └───────────────┘       └───────────────┘
```

### 5.2 模块依赖图

```
                    ┌──────────────────┐
                    │  用户应用程序     │
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │ psutil/__init__.py│
                    │   (Facade 模式)   │
                    └────────┬─────────┘
                             │
         ┌───────────────────┼───────────────────┐
         ▼                   ▼                   ▼
    ┌─────────┐        ┌──────────┐        ┌─────────────┐
    │_common.py│        │_ntuples.py│        │_psplatform  │
    │(常量/工具)│        │(数据结构) │        │(策略模式)   │
    └─────────┘        └──────────┘        └──────┬──────┘
                                                  │
    ┌──────────────┬──────────────┬───────────────┼───────────────┐
    │              │              │               │               │
    ▼              ▼              ▼               ▼               ▼
┌────────┐   ┌──────────┐   ┌────────┐   ┌──────────┐   ┌────────┐
│_pslinux│   │_pswindows│   │ _psosx │   │  _psbsd  │   │_pssunos│
│  .py   │   │   .py    │   │  .py   │   │   .py    │   │  .py   │
└───┬────┘   └────┬─────┘   └───┬────┘   └────┬─────┘   └───┬────┘
    │             │             │             │             │
    ▼             ▼             ▼             ▼             ▼
┌────────────────────────────────────────────────────────────────┐
│                         C 扩展层                               │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                    arch/all/                              │ │
│  │     init.c, errors.c, pids.c, str.c (全平台共享)         │ │
│  └──────────────────────────────────────────────────────────┘ │
│                              │                                  │
│  ┌──────────────────────────┴───────────────────────────────┐ │
│  │                    arch/posix/                            │ │
│  │       net.c, proc.c, users.c (POSIX 共享)                │ │
│  └──────────────────────────────────────────────────────────┘ │
│                              │                                  │
│  ┌───────────┬───────────┬───┴───────┬───────────┬─────────┐  │
│  │           │           │           │           │         │  │
│  │arch/linux │arch/windows│ arch/osx │ arch/bsd  │arch/sunos│  │
│  │           │           │           │           │         │  │
│  └───────────┴───────────┴───────────┴───────────┴─────────┘  │
└────────────────────────────────────────────────────────────────┘
```

---

## 6. 设计模式应用

### 6.1 使用的设计模式

| 模式 | 应用位置 | 说明 |
|-----|---------|------|
| **外观模式 (Facade)** | `psutil/__init__.py` | 统一接口，隐藏平台细节 |
| **策略模式 (Strategy)** | `_psplatform` 动态导入 | 运行时选择平台实现 |
| **模板方法 (Template)** | `Process` 类 | 定义算法骨架，子类实现细节 |
| **适配器模式 (Adapter)** | `_ps{platform}.py` | 适配不同平台 API |
| **装饰器模式 (Decorator)** | `@memoize`, `@memoize_when_activated` | 添加缓存功能 |
| **单例模式 (Singleton)** | 缓存的系统信息 | 避免重复计算 |

### 6.2 最佳实践建议

1. **分层抽象**：Python 层处理逻辑，C 层处理系统调用
2. **渐进增强**：优先实现核心功能，平台特性作为可选
3. **优雅降级**：功能不可用时提供合理的回退
4. **条件编译**：减少不必要的代码包含
5. **统一头文件**：`arch/all/init.h` 作为唯一入口

