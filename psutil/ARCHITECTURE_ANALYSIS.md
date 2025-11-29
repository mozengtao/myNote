# psutil 项目架构分析文档

## 目录
1. [项目概述](#1-项目概述)
2. [项目结构分析](#2-项目结构分析)
3. [跨平台架构设计](#3-跨平台架构设计)
4. [构建系统分析](#4-构建系统分析)
5. [模块交互图](#5-模块交互图)

---

## 1. 项目概述

psutil（process and system utilities）是一个跨平台的 Python 库，用于检索进程和系统利用率信息（CPU、内存、磁盘、网络、传感器）。它支持以下平台：

- **Linux**
- **Windows**
- **macOS**
- **FreeBSD / OpenBSD / NetBSD**
- **Sun Solaris**
- **AIX**

psutil 的核心价值在于其混合架构：**Python 提供高层抽象 API**，而 **C 扩展模块处理底层系统调用**。

---

## 2. 项目结构分析

### 2.1 顶层目录结构

```
psutil/
├── psutil/                 # 核心源代码目录
│   ├── __init__.py        # 公共 API 入口点
│   ├── _common.py         # 跨平台共享的 Python 工具
│   ├── _ntuples.py        # namedtuple 定义
│   ├── _psposix.py        # POSIX 系统共享代码
│   │
│   ├── _pslinux.py        # Linux 平台 Python 实现
│   ├── _pswindows.py      # Windows 平台 Python 实现
│   ├── _psosx.py          # macOS 平台 Python 实现
│   ├── _psbsd.py          # BSD 系列平台 Python 实现
│   ├── _pssunos.py        # Solaris 平台 Python 实现
│   ├── _psaix.py          # AIX 平台 Python 实现
│   │
│   ├── _psutil_linux.c    # Linux C 扩展入口
│   ├── _psutil_windows.c  # Windows C 扩展入口
│   ├── _psutil_osx.c      # macOS C 扩展入口
│   ├── _psutil_bsd.c      # BSD C 扩展入口
│   ├── _psutil_sunos.c    # Solaris C 扩展入口
│   ├── _psutil_aix.c      # AIX C 扩展入口
│   │
│   ├── arch/              # 平台特定的 C 实现
│   └── test/              # 测试工具
│
├── tests/                  # 测试套件
│   ├── test_linux.py
│   ├── test_windows.py
│   ├── test_osx.py
│   ├── test_bsd.py
│   ├── test_process.py
│   ├── test_system.py
│   └── ...
│
├── scripts/                # 示例脚本和工具
│   ├── top.py             # 类 top 命令实现
│   ├── ps.py              # 类 ps 命令实现
│   ├── netstat.py         # 网络状态工具
│   ├── iotop.py           # I/O 监控工具
│   └── ...
│
├── docs/                   # 文档
├── setup.py               # 构建配置（关键文件）
├── pyproject.toml         # 现代 Python 项目配置
├── Makefile               # 开发便捷命令
├── README.rst             # 项目说明
├── HISTORY.rst            # 变更日志
├── INSTALL.rst            # 安装指南
└── LICENSE                # BSD-3-Clause 许可证
```

### 2.2 核心模块职责划分

| 模块 | 职责 |
|------|------|
| `__init__.py` | 公共 API 暴露，统一接口，平台检测与模块加载 |
| `_common.py` | 跨平台常量、异常类、工具函数（不导入平台特定代码） |
| `_ntuples.py` | 所有返回值的 namedtuple 定义 |
| `_psposix.py` | POSIX 系统共享的 Python 实现 |
| `_ps{platform}.py` | 平台特定的 Python 实现层 |
| `_psutil_{platform}.c` | 平台特定的 C 扩展模块入口 |
| `arch/` | 按平台组织的底层 C 实现 |

### 2.3 `arch/` 目录详细结构

```
psutil/arch/
├── all/                    # 全平台共享的 C 代码
│   ├── init.c             # 全局初始化
│   ├── init.h             # 统一头文件（包含其他平台头文件）
│   ├── errors.c           # 错误处理
│   ├── pids.c             # PID 相关工具
│   └── str.c              # 字符串工具
│
├── posix/                  # POSIX 系统共享代码
│   ├── init.c             # POSIX 初始化和方法注册
│   ├── init.h
│   ├── net.c              # 网络接口
│   ├── pids.c             # 进程 ID 操作
│   ├── proc.c             # 进程操作
│   ├── sysctl.c           # sysctl 封装
│   └── users.c            # 用户信息
│
├── linux/                  # Linux 专用
│   ├── init.h
│   ├── disk.c             # 磁盘分区
│   ├── heap.c             # 堆内存统计
│   ├── mem.c              # 内存信息
│   ├── net.c              # 网络接口
│   └── proc.c             # /proc 文件系统解析
│
├── windows/                # Windows 专用
│   ├── init.c
│   ├── init.h
│   ├── ntextapi.h         # NT API 未文档化接口
│   ├── cpu.c              # CPU 信息
│   ├── disk.c             # 磁盘
│   ├── heap.c             # 堆内存
│   ├── mem.c              # 内存
│   ├── net.c              # 网络
│   ├── pids.c             # 进程 ID
│   ├── proc.c             # 进程信息
│   ├── proc_handles.c     # 进程句柄
│   ├── proc_info.c        # 进程详情
│   ├── proc_utils.c       # 进程工具
│   ├── security.c         # 安全/权限
│   ├── sensors.c          # 传感器（电池）
│   ├── services.c         # Windows 服务
│   ├── socks.c            # 套接字
│   ├── sys.c              # 系统信息
│   └── wmi.c              # WMI 接口
│
├── osx/                    # macOS 专用
│   ├── init.c/h
│   ├── cpu.c, disk.c, mem.c, net.c
│   ├── proc.c, proc_utils.c
│   ├── sensors.c, sys.c
│   ├── heap.c
│   └── pids.c
│
├── bsd/                    # BSD 共享代码
├── freebsd/                # FreeBSD 专用
├── openbsd/                # OpenBSD 专用
├── netbsd/                 # NetBSD 专用
├── sunos/                  # Solaris/SunOS 专用
└── aix/                    # AIX 专用
```

### 2.4 与传统 C 系统编程项目的对比

| 特性 | psutil 项目 | 传统 C 系统项目 |
|------|-------------|-----------------|
| **入口点** | Python `__init__.py` + C 扩展 | `main.c` |
| **构建系统** | `setup.py` (setuptools/distutils) | Makefile / CMake / Autotools |
| **头文件组织** | `arch/{platform}/init.h` 分层包含 | 扁平或按功能分组 |
| **平台抽象** | Python 层 + C 条件编译 | 纯条件编译 (#ifdef) |
| **接口定义** | Python API + C PyMethodDef | 头文件函数声明 |
| **错误处理** | Python 异常 (OSError 等) | errno + 返回值 |
| **内存管理** | Python 引用计数 + C malloc | 手动 malloc/free |
| **测试** | pytest (Python) | 独立测试框架或自定义 |

---

## 3. 跨平台架构设计

### 3.1 分层架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                     用户 Python 代码                             │
│                  import psutil; psutil.cpu_percent()            │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                  psutil/__init__.py                             │
│              (公共 API，平台检测，统一接口)                       │
│                                                                 │
│   if LINUX:                                                     │
│       from . import _pslinux as _psplatform                     │
│   elif WINDOWS:                                                 │
│       from . import _pswindows as _psplatform                   │
│   elif MACOS:                                                   │
│       from . import _psosx as _psplatform                       │
│   ...                                                           │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│               平台特定 Python 模块                               │
│     _pslinux.py / _pswindows.py / _psosx.py / _psbsd.py         │
│                                                                 │
│   - 导入对应的 C 扩展模块 (cext)                                  │
│   - 实现平台特定的 Python 逻辑                                    │
│   - 解析 /proc、/sys 等文件系统（Linux）                          │
│   - 提供 Process 类的平台实现                                    │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                 C 扩展模块 (.so/.pyd)                            │
│    _psutil_linux.so / _psutil_windows.pyd / _psutil_osx.so      │
│                                                                 │
│   - Python/C API 接口                                           │
│   - 调用底层 C 实现函数                                          │
│   - 类型转换和错误处理                                           │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                    arch/{platform}/*.c                          │
│                   底层 C 系统调用实现                             │
│                                                                 │
│   - 直接调用操作系统 API                                         │
│   - Linux: procfs/sysfs 解析, sysinfo(), ioctl()                │
│   - Windows: NtQuerySystemInformation, Win32 API               │
│   - macOS: mach_*, sysctl, IOKit                               │
│   - BSD: kvm_*, sysctl                                         │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      操作系统内核                                │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 条件编译与平台抽象层

#### 3.2.1 预处理器宏定义 (`setup.py`)

```python
# setup.py 中的平台检测与宏定义
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
    macros.append(("PSUTIL_OSX", 1))
    macros.append(("PSUTIL_MACOS", 1))
# ... 其他平台
```

#### 3.2.2 统一头文件机制 (`arch/all/init.h`)

```c
// arch/all/init.h - 统一入口点
// 根据平台宏自动包含对应的头文件

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
// ... 其他平台
#endif
```

#### 3.2.3 错误处理抽象 (`arch/all/errors.c`)

```c
// 跨平台错误处理
PyObject *
psutil_oserror(void) {
#ifdef PSUTIL_WINDOWS
    PyErr_SetFromWindowsErr(GetLastError());
#else
    PyErr_SetFromErrno(PyExc_OSError);
#endif
    return NULL;
}
```

#### 3.2.4 POSIX 共享层 (`arch/posix/init.c`)

```c
// POSIX 系统共享的方法
static PyMethodDef posix_methods[] = {
    {"getpagesize", psutil_getpagesize_pywrapper, METH_VARARGS},
    {"net_if_addrs", psutil_net_if_addrs, METH_VARARGS},
    {"net_if_flags", psutil_net_if_flags, METH_VARARGS},
    {"proc_priority_get", psutil_proc_priority_get, METH_VARARGS},
    {"proc_priority_set", psutil_proc_priority_set, METH_VARARGS},
    // 条件编译：仅 BSD/macOS
#if defined(PSUTIL_BSD) || defined(PSUTIL_OSX)
    {"net_if_duplex_speed", psutil_net_if_duplex_speed, METH_VARARGS},
#endif
    // ...
    {NULL, NULL, 0, NULL}
};

// 在平台模块初始化时注册 POSIX 方法
int
psutil_posix_add_methods(PyObject *mod) {
    for (int i = 0; posix_methods[i].ml_name != NULL; i++) {
        PyObject *f = PyCFunction_NewEx(&posix_methods[i], NULL, mod);
        if (PyModule_AddObject(mod, posix_methods[i].ml_name, f))
            return -1;
    }
    return 0;
}
```

### 3.3 Python 层的平台抽象

#### 3.3.1 `__init__.py` 中的平台检测

```python
# psutil/__init__.py
from ._common import LINUX, WINDOWS, MACOS, BSD, SUNOS, AIX

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
```

#### 3.3.2 `_common.py` 中的平台常量

```python
# psutil/_common.py
import os
import sys

POSIX = os.name == "posix"
WINDOWS = os.name == "nt"
LINUX = sys.platform.startswith("linux")
MACOS = sys.platform.startswith("darwin")
FREEBSD = sys.platform.startswith(("freebsd", "midnightbsd"))
OPENBSD = sys.platform.startswith("openbsd")
NETBSD = sys.platform.startswith("netbsd")
BSD = FREEBSD or OPENBSD or NETBSD
SUNOS = sys.platform.startswith(("sunos", "solaris"))
AIX = sys.platform.startswith("aix")
```

### 3.4 模块依赖关系图

```
                          ┌────────────────────┐
                          │    用户代码         │
                          │  import psutil     │
                          └─────────┬──────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────────────┐
│                        psutil/__init__.py                             │
│                                                                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                   │
│  │  _common.py │  │ _ntuples.py │  │ _psplatform │◄─── 动态导入      │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                   │
│         │                │                │                           │
└─────────┼────────────────┼────────────────┼───────────────────────────┘
          │                │                │
          ▼                ▼                ▼
    ┌───────────────────────────────────────────────────┐
    │            平台特定 Python 模块选择                │
    │                                                   │
    │  ┌──────────────┐ ┌────────────────┐             │
    │  │ _pslinux.py  │ │ _pswindows.py  │ ...         │
    │  └───────┬──────┘ └───────┬────────┘             │
    └──────────┼────────────────┼───────────────────────┘
               │                │
               ▼                ▼
    ┌───────────────────────────────────────────────────┐
    │              C 扩展模块 (cext)                    │
    │                                                   │
    │  ┌───────────────────┐ ┌─────────────────────┐   │
    │  │ _psutil_linux.so  │ │ _psutil_windows.pyd │   │
    │  │ (PyInit 入口)      │ │ (PyInit 入口)        │   │
    │  └─────────┬─────────┘ └──────────┬──────────┘   │
    └────────────┼──────────────────────┼───────────────┘
                 │                      │
                 ▼                      ▼
    ┌───────────────────────────────────────────────────┐
    │                  arch/ 目录                       │
    │                                                   │
    │  ┌─────────────────────────────────────────────┐ │
    │  │               arch/all/                     │ │
    │  │   init.c, init.h, errors.c, pids.c, str.c   │ │
    │  │           (全平台共享)                       │ │
    │  └──────────────────┬──────────────────────────┘ │
    │                     │                             │
    │  ┌──────────────────┴──────────────────────────┐ │
    │  │                                             │ │
    │  │  ┌───────────┐  ┌───────────┐               │ │
    │  │  │arch/posix/│  │arch/bsd/  │  (共享层)     │ │
    │  │  └─────┬─────┘  └─────┬─────┘               │ │
    │  │        │              │                     │ │
    │  │  ┌─────┴─────────┬────┴────┬──────────┐    │ │
    │  │  ▼               ▼         ▼          ▼    │ │
    │  │ linux/      windows/    osx/      freebsd/ │ │
    │  │ netbsd/     openbsd/    sunos/    aix/     │ │
    │  │           (平台特定实现)                    │ │
    │  └─────────────────────────────────────────────┘ │
    └───────────────────────────────────────────────────┘
```

### 3.5 代码共享层次

```
层次 4: 平台特定 (arch/{platform}/*.c)
    └── FreeBSD: kvm_getprocs()
    └── Linux: /proc 解析
    └── Windows: NtQuerySystemInformation
    └── macOS: mach_* APIs

层次 3: 系统家族共享 (arch/posix/, arch/bsd/)
    └── POSIX: nice(), getpagesize(), net_if_addrs()
    └── BSD: kvm 基础设施, sysctl 封装

层次 2: 全平台 C 共享 (arch/all/)
    └── 错误处理、调试、字符串工具

层次 1: Python 平台抽象
    └── _common.py: 常量、工具函数
    └── _psposix.py: POSIX Python 工具
```

---

## 4. 构建系统分析

### 4.1 `setup.py` 核心结构

```python
# setup.py 关键配置

# 1. 平台检测 (从 _common.py 导入)
from _common import LINUX, WINDOWS, MACOS, FREEBSD, ...

# 2. 预处理器宏定义
macros = []
if POSIX:
    macros.append(("PSUTIL_POSIX", 1))
if BSD:
    macros.append(("PSUTIL_BSD", 1))
# ... 平台特定宏

# 3. 版本宏
macros.append(('PSUTIL_VERSION', int(VERSION.replace('.', ''))))

# 4. 稳定 ABI 支持 (Py_LIMITED_API)
if CP36_PLUS and (MACOS or LINUX):
    macros.append(('Py_LIMITED_API', '0x03060000'))
```

### 4.2 平台特定编译配置

#### Linux
```python
if LINUX:
    macros.append(("PSUTIL_LINUX", 1))
    ext = Extension(
        'psutil._psutil_linux',
        sources=(
            sources                              # arch/all/*.c
            + ["psutil/_psutil_linux.c"]         # 模块入口
            + glob.glob("psutil/arch/linux/*.c") # Linux 特定
        ),
        define_macros=macros,
        **py_limited_api
    )
```

#### Windows
```python
if WINDOWS:
    macros.append(("PSUTIL_WINDOWS", 1))
    macros.extend([
        ('_WIN32_WINNT', get_winver()),
        ('_CRT_SECURE_NO_WARNINGS', None),
        ('PSAPI_VERSION', 1),
    ])
    ext = Extension(
        'psutil._psutil_windows',
        sources=(
            sources
            + ["psutil/_psutil_windows.c"]
            + glob.glob("psutil/arch/windows/*.c")
        ),
        define_macros=macros,
        libraries=[
            "advapi32", "kernel32", "netapi32",
            "pdh", "PowrProf", "psapi", "shell32", "ws2_32",
        ],
        **py_limited_api
    )
```

#### macOS
```python
if MACOS:
    macros.extend([("PSUTIL_OSX", 1), ("PSUTIL_MACOS", 1)])
    ext = Extension(
        'psutil._psutil_osx',
        sources=(
            sources
            + ["psutil/_psutil_osx.c"]
            + glob.glob("psutil/arch/osx/*.c")
        ),
        define_macros=macros,
        extra_link_args=[
            '-framework', 'CoreFoundation',
            '-framework', 'IOKit',
        ],
        **py_limited_api
    )
```

#### BSD 系列
```python
# FreeBSD
if FREEBSD:
    macros.append(("PSUTIL_FREEBSD", 1))
    sources += glob.glob("psutil/arch/bsd/*.c")
    sources += glob.glob("psutil/arch/freebsd/*.c")
    libraries = ["devstat"]

# OpenBSD
if OPENBSD:
    macros.append(("PSUTIL_OPENBSD", 1))
    sources += glob.glob("psutil/arch/bsd/*.c")
    sources += glob.glob("psutil/arch/openbsd/*.c")
    libraries = ["kvm"]

# NetBSD
if NETBSD:
    macros.append(("PSUTIL_NETBSD", 1))
    sources += glob.glob("psutil/arch/bsd/*.c")
    sources += glob.glob("psutil/arch/netbsd/*.c")
    libraries = ["kvm", "jemalloc"]
```

### 4.3 源文件组装逻辑

```python
# 基础源文件（所有平台）
sources = glob.glob("psutil/arch/all/*.c")

# POSIX 系统添加 posix 共享代码
if POSIX:
    sources.extend(glob.glob("psutil/arch/posix/*.c"))

# 平台特定源文件
# Linux:   sources + _psutil_linux.c + arch/linux/*.c
# Windows: sources + _psutil_windows.c + arch/windows/*.c
# macOS:   sources + _psutil_osx.c + arch/osx/*.c
# BSD:     sources + _psutil_bsd.c + arch/bsd/*.c + arch/{bsd_variant}/*.c
```

### 4.4 编译流程图

```
                    setup.py 执行
                         │
                         ▼
            ┌────────────────────────┐
            │    平台检测            │
            │  (导入 _common.py)     │
            └───────────┬────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
   ┌─────────┐    ┌──────────┐    ┌──────────┐
   │ LINUX   │    │ WINDOWS  │    │  MACOS   │  ...
   └────┬────┘    └────┬─────┘    └────┬─────┘
        │              │               │
        ▼              ▼               ▼
┌──────────────┐ ┌───────────────┐ ┌───────────────┐
│ 设置宏:       │ │ 设置宏:        │ │ 设置宏:        │
│ PSUTIL_LINUX │ │ PSUTIL_WINDOWS│ │ PSUTIL_OSX    │
│ PSUTIL_POSIX │ │ _WIN32_WINNT  │ │ PSUTIL_MACOS  │
└──────┬───────┘ └───────┬───────┘ └───────┬───────┘
       │                 │                 │
       ▼                 ▼                 ▼
┌──────────────┐ ┌───────────────┐ ┌───────────────┐
│ 源文件:       │ │ 源文件:        │ │ 源文件:        │
│ arch/all/*   │ │ arch/all/*    │ │ arch/all/*    │
│ arch/posix/* │ │ arch/windows/*│ │ arch/posix/*  │
│ arch/linux/* │ │               │ │ arch/osx/*    │
└──────┬───────┘ └───────┬───────┘ └───────┬───────┘
       │                 │                 │
       ▼                 ▼                 ▼
┌──────────────┐ ┌───────────────┐ ┌───────────────┐
│ 链接库:       │ │ 链接库:        │ │ 链接框架:      │
│ (无额外库)    │ │ advapi32      │ │ CoreFoundation│
│              │ │ kernel32...   │ │ IOKit         │
└──────┬───────┘ └───────┬───────┘ └───────┬───────┘
       │                 │                 │
       └────────────┬────┴─────────────────┘
                    │
                    ▼
          ┌─────────────────┐
          │  编译 C 扩展     │
          │ gcc/clang/MSVC  │
          └────────┬────────┘
                   │
                   ▼
          ┌─────────────────┐
          │ 生成 .so/.pyd   │
          │ _psutil_{os}.so │
          └─────────────────┘
```

### 4.5 稳定 ABI 支持

```python
# Py_LIMITED_API 允许单个 wheel 支持多个 Python 版本
if setuptools and CP36_PLUS and (MACOS or LINUX) and not Py_GIL_DISABLED:
    py_limited_api = {"py_limited_api": True}
    options = {"bdist_wheel": {"py_limited_api": "cp36"}}
    macros.append(('Py_LIMITED_API', '0x03060000'))
elif setuptools and CP37_PLUS and WINDOWS and not Py_GIL_DISABLED:
    # Windows 需要 3.7+ (PyErr_SetFromWindowsErr)
    py_limited_api = {"py_limited_api": True}
    options = {"bdist_wheel": {"py_limited_api": "cp37"}}
    macros.append(('Py_LIMITED_API', '0x03070000'))
```

---

## 5. 模块交互图

### 5.1 Python 与 C 协同工作方式

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Python 层                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │                   psutil/__init__.py                        │    │
│  │                                                             │    │
│  │  class Process:                                             │    │
│  │      def cpu_times(self):                                   │    │
│  │          return self._proc.cpu_times()  ──┐                 │    │
│  │                                           │                 │    │
│  │  def virtual_memory():                    │                 │    │
│  │      return _psplatform.virtual_memory() ─┼─┐               │    │
│  │                                           │ │               │    │
│  └───────────────────────────────────────────┼─┼───────────────┘    │
│                                              │ │                    │
│  ┌───────────────────────────────────────────┼─┼───────────────┐    │
│  │           _pslinux.py (Linux 示例)        │ │               │    │
│  │                                           │ │               │    │
│  │  import _psutil_linux as cext  ◄──────────┼─┘               │    │
│  │                                           │                 │    │
│  │  class Process:                           │                 │    │
│  │      def cpu_times(self):  ◄──────────────┘                 │    │
│  │          # 读取 /proc/{pid}/stat                            │    │
│  │          data = cat(f"/proc/{self.pid}/stat")               │    │
│  │          return pcputimes(...)                              │    │
│  │                                                             │    │
│  │  def virtual_memory():                                      │    │
│  │      # 读取 /proc/meminfo                                   │    │
│  │      data = cat("/proc/meminfo")                            │    │
│  │      # 调用 C 扩展获取 sysinfo                               │    │
│  │      info = cext.linux_sysinfo()  ──────────────┐           │    │
│  │      return svmem(...)                          │           │    │
│  │                                                 │           │    │
│  └─────────────────────────────────────────────────┼───────────┘    │
│                                                    │                │
└────────────────────────────────────────────────────┼────────────────┘
                                                     │
                    ┌────────────────────────────────┘
                    │ Python/C API 边界
                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          C 层                                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │              _psutil_linux.c (模块入口)                     │    │
│  │                                                             │    │
│  │  static PyMethodDef mod_methods[] = {                       │    │
│  │      {"linux_sysinfo", psutil_linux_sysinfo, METH_VARARGS}, │    │
│  │      {"proc_ioprio_get", psutil_proc_ioprio_get, ...},      │    │
│  │      // ...                                                 │    │
│  │  };                                                         │    │
│  │                                                             │    │
│  │  PyMODINIT_FUNC PyInit__psutil_linux(void) {                │    │
│  │      mod = PyModule_Create(&moduledef);                     │    │
│  │      psutil_setup();           // arch/all/init.c           │    │
│  │      psutil_posix_add_methods(mod); // arch/posix/init.c    │    │
│  │      // 添加常量...                                          │    │
│  │      return mod;                                            │    │
│  │  }                                                          │    │
│  └────────────────────────────────────────────────────────────┘    │
│                              │                                      │
│                              ▼                                      │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │              arch/linux/mem.c                               │    │
│  │                                                             │    │
│  │  PyObject *psutil_linux_sysinfo(PyObject *self, ...) {      │    │
│  │      struct sysinfo info;                                   │    │
│  │      if (sysinfo(&info) != 0)                               │    │
│  │          return psutil_oserror();  // arch/all/errors.c     │    │
│  │      return Py_BuildValue("KKKK",                           │    │
│  │          info.totalram,                                     │    │
│  │          info.freeram,                                      │    │
│  │          // ...                                             │    │
│  │      );                                                     │    │
│  │  }                                                          │    │
│  └────────────────────────────────────────────────────────────┘    │
│                              │                                      │
│                              ▼                                      │
│                     ┌─────────────────┐                            │
│                     │  Linux Kernel   │                            │
│                     │   sysinfo()     │                            │
│                     └─────────────────┘                            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 典型 API 调用链示例

#### 示例 1: `psutil.cpu_percent()` (Linux)

```
用户代码: psutil.cpu_percent()
         │
         ▼
__init__.py: cpu_percent()
         │ 调用 _psplatform.cpu_times()
         ▼
_pslinux.py: cpu_times()
         │ 读取 /proc/stat
         │ 返回 scputimes namedtuple
         ▼
__init__.py: 计算百分比
         │
         ▼
返回 float (CPU 使用百分比)
```

#### 示例 2: `psutil.Process(pid).memory_info()` (Windows)

```
用户代码: psutil.Process(1234).memory_info()
         │
         ▼
__init__.py: Process.memory_info()
         │ 调用 self._proc.memory_info()
         ▼
_pswindows.py: Process.memory_info()
         │ 调用 cext.proc_memory_info(self.pid)
         ▼
_psutil_windows.c: psutil_proc_memory_info()
         │ 调用 arch/windows/proc.c 函数
         ▼
arch/windows/proc.c:
         │ 调用 GetProcessMemoryInfo()
         │ 返回 Py_BuildValue(...)
         ▼
返回 pmem namedtuple (rss, vms, ...)
```

### 5.3 完整架构交互图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              用户应用程序                                    │
│                         import psutil                                       │
│                         psutil.cpu_percent()                                │
│                         psutil.Process(pid).name()                          │
└─────────────────────────────────────────────────┬───────────────────────────┘
                                                  │
                                                  ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         psutil/__init__.py                                  │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ 公共 API:                                                          │    │
│  │   - Process 类                                                     │    │
│  │   - cpu_percent(), virtual_memory(), disk_usage() 等              │    │
│  │   - 统一的异常类 (NoSuchProcess, AccessDenied, ...)               │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                   │                                         │
│                    ┌──────────────┼──────────────┐                         │
│                    │              │              │                         │
│                    ▼              ▼              ▼                         │
│  ┌─────────────────────┐ ┌─────────────┐ ┌─────────────────────┐          │
│  │    _common.py       │ │ _ntuples.py │ │   _psplatform       │          │
│  │  (常量/工具/异常)    │ │ (数据结构)   │ │ (动态导入的模块)    │          │
│  └─────────────────────┘ └─────────────┘ └──────────┬──────────┘          │
└─────────────────────────────────────────────────────┼───────────────────────┘
                                                      │
            ┌─────────────┬─────────────┬─────────────┼─────────────┬─────────┐
            │             │             │             │             │         │
            ▼             ▼             ▼             ▼             ▼         │
┌───────────────┐ ┌───────────────┐ ┌───────────┐ ┌───────────┐ ┌─────────┐  │
│ _pslinux.py   │ │ _pswindows.py │ │ _psosx.py │ │ _psbsd.py │ │_psaix.py│  │
│               │ │               │ │           │ │           │ │         │  │
│ import cext   │ │ import cext   │ │ import    │ │ import    │ │ import  │  │
│ (_psutil_     │ │ (_psutil_     │ │ cext      │ │ cext      │ │ cext    │  │
│  linux)       │ │  windows)     │ │           │ │           │ │         │  │
│               │ │               │ │           │ │           │ │         │  │
│ class Process │ │ class Process │ │ class     │ │ class     │ │ class   │  │
│ def pids()    │ │ def pids()    │ │ Process   │ │ Process   │ │ Process │  │
│ def virtual_  │ │ def virtual_  │ │ ...       │ │ ...       │ │ ...     │  │
│   memory()    │ │   memory()    │ │           │ │           │ │         │  │
└───────┬───────┘ └───────┬───────┘ └─────┬─────┘ └─────┬─────┘ └────┬────┘  │
        │                 │               │             │            │       │
        │                 │               │             │            │       │
        ▼                 ▼               ▼             ▼            ▼       │
┌─────────────────────────────────────────────────────────────────────────┐  │
│                        C 扩展模块 (.so / .pyd)                          │  │
│                                                                         │  │
│  ┌────────────────┐ ┌──────────────────┐ ┌─────────────┐ ┌───────────┐ │  │
│  │_psutil_linux.so│ │_psutil_windows.so│ │_psutil_osx  │ │_psutil_bsd│ │  │
│  │                │ │                  │ │             │ │           │ │  │
│  │ PyMethodDef    │ │ PyMethodDef      │ │ PyMethodDef │ │PyMethodDef│ │  │
│  │ linux_sysinfo  │ │ virtual_mem      │ │ vm_stats    │ │ cpu_stats │ │  │
│  │ proc_ioprio    │ │ proc_memory_info │ │ proc_threads│ │ proc_cwd  │ │  │
│  └────────┬───────┘ └────────┬─────────┘ └──────┬──────┘ └─────┬─────┘ │  │
└───────────┼──────────────────┼──────────────────┼──────────────┼───────┘  │
            │                  │                  │              │          │
            ▼                  ▼                  ▼              ▼          │
┌─────────────────────────────────────────────────────────────────────────┐  │
│                         arch/ 目录 (C 实现)                             │  │
│                                                                         │  │
│  ┌───────────────────────────────────────────────────────────────────┐ │  │
│  │                         arch/all/                                  │ │  │
│  │              (全平台共享: init, errors, pids, str)                 │ │  │
│  └───────────────────────────────────────────────────────────────────┘ │  │
│                                  │                                      │  │
│  ┌───────────────────────────────┴───────────────────────────────────┐ │  │
│  │                        arch/posix/                                 │ │  │
│  │            (POSIX 共享: net, proc, sysctl, users)                  │ │  │
│  └───────────────────────────────────────────────────────────────────┘ │  │
│                                  │                                      │  │
│  ┌──────────┬───────────┬────────┴───────┬───────────┬─────────────┐  │  │
│  │          │           │                │           │             │  │  │
│  ▼          ▼           ▼                ▼           ▼             ▼  │  │
│ linux/   windows/     osx/             bsd/      freebsd/      sunos/ │  │
│ mem.c    cpu.c       cpu.c            init.c    cpu_freq.c     mem.c  │  │
│ net.c    mem.c       disk.c                     disk.c         net.c  │  │
│ proc.c   net.c       mem.c                      proc.c         proc.c │  │
│ disk.c   proc.c      net.c                                            │  │
│ heap.c   services.c  sensors.c                                        │  │
│          security.c  heap.c                                           │  │
│          ...         ...                                              │  │
└─────────────────────────────────────────────────────────────────────────┘  │
            │                  │                  │              │          │
            ▼                  ▼                  ▼              ▼          │
┌─────────────────────────────────────────────────────────────────────────┐  │
│                           操作系统内核 API                              │  │
│                                                                         │  │
│  Linux:            Windows:           macOS:           BSD:             │  │
│  ├─ /proc          ├─ NtQuery*        ├─ mach_*        ├─ sysctl()     │  │
│  ├─ /sys           ├─ GetProcess*     ├─ IOKit         ├─ kvm_*        │  │
│  ├─ sysinfo()      ├─ WMI             ├─ sysctl()      ├─ kinfo_proc   │  │
│  ├─ ioctl()        ├─ PDH             ├─ host_*        └─ ...          │  │
│  └─ syscall()      └─ Registry        └─ proc_*                         │  │
│                                                                         │  │
└─────────────────────────────────────────────────────────────────────────┘  │
```

---

## 总结

### 关键架构特点

1. **双层抽象**：Python 提供高层 API，C 处理系统调用
2. **渐进式共享**：`arch/all` → `arch/posix` → `arch/{platform}` 层次化复用
3. **统一头文件**：`arch/all/init.h` 作为唯一入口，自动包含平台头文件
4. **条件编译**：`setup.py` 动态配置宏和源文件
5. **平台检测**：Python 和 C 两层都有检测机制
6. **稳定 ABI**：`Py_LIMITED_API` 支持单个 wheel 跨 Python 版本

### 维护建议

1. 添加新平台：创建 `arch/{newplatform}/`，更新 `setup.py` 条件
2. 添加新功能：先在 `arch/all/` 定义通用接口，再平台实现
3. 测试：每个平台有对应的 `test_{platform}.py`
4. 错误处理：使用 `arch/all/errors.c` 中的统一函数

