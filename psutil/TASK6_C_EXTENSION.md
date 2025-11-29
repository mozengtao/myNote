# Task 6: psutil Python C 扩展实现深入分析

## 概述

本文档深入分析 psutil 中 Python C 扩展的实现，包括模块初始化、类型转换、引用计数管理和内存管理。

---

## 1. 模块初始化

### 1.1 `PyMODINIT_FUNC` 的使用

```c
// psutil/_psutil_linux.c

// 模块定义结构
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,  // 必需的头部初始化
    "_psutil_linux",         // 模块名称
    NULL,                    // 模块文档字符串 (可为 NULL)
    -1,                      // 模块状态大小 (-1 表示不使用)
    mod_methods,             // 方法表
    NULL,                    // 用于多阶段初始化的槽
    NULL,                    // 遍历函数 (GC)
    NULL,                    // 清理函数 (GC)
    NULL                     // 释放函数
};

// 模块初始化函数
PyMODINIT_FUNC
PyInit__psutil_linux(void) {
    // 创建模块对象
    PyObject *mod = PyModule_Create(&moduledef);
    if (mod == NULL)
        return NULL;

    // Python 3.13+ 自由线程支持
#ifdef Py_GIL_DISABLED
    if (PyUnstable_Module_SetGIL(mod, Py_MOD_GIL_NOT_USED))
        return NULL;
#endif

    // 调用全平台初始化
    if (psutil_setup() != 0)
        return NULL;
    
    // 添加 POSIX 共享的常量和方法
    if (psutil_posix_add_constants(mod) != 0)
        return NULL;
    if (psutil_posix_add_methods(mod) != 0)
        return NULL;

    // 添加模块常量
    if (PyModule_AddIntConstant(mod, "version", PSUTIL_VERSION))
        return NULL;
    if (PyModule_AddIntConstant(mod, "DUPLEX_HALF", DUPLEX_HALF))
        return NULL;
    if (PyModule_AddIntConstant(mod, "DUPLEX_FULL", DUPLEX_FULL))
        return NULL;
    if (PyModule_AddIntConstant(mod, "DUPLEX_UNKNOWN", DUPLEX_UNKNOWN))
        return NULL;

    return mod;
}
```

### 1.2 模块方法表 (MethodTable) 定义

```c
// 方法表定义
static PyMethodDef mod_methods[] = {
    // --- 进程相关函数 ---
    
    // 条件编译：仅当有 ioprio 支持时
#ifdef PSUTIL_HAS_IOPRIO
    {"proc_ioprio_get", psutil_proc_ioprio_get, METH_VARARGS},
    {"proc_ioprio_set", psutil_proc_ioprio_set, METH_VARARGS},
#endif

    // 条件编译：仅当有 CPU 亲和性支持时
#ifdef PSUTIL_HAS_CPU_AFFINITY
    {"proc_cpu_affinity_get", psutil_proc_cpu_affinity_get, METH_VARARGS},
    {"proc_cpu_affinity_set", psutil_proc_cpu_affinity_set, METH_VARARGS},
#endif

    // --- 系统相关函数 ---
    {"disk_partitions", psutil_disk_partitions, METH_VARARGS},
    {"net_if_duplex_speed", psutil_net_if_duplex_speed, METH_VARARGS},
    
#ifdef PSUTIL_HAS_HEAP_INFO
    {"heap_info", psutil_heap_info, METH_VARARGS},
#endif
#ifdef PSUTIL_HAS_HEAP_TRIM
    {"heap_trim", psutil_heap_trim, METH_VARARGS},
#endif

    // Linux 特定
    {"linux_sysinfo", psutil_linux_sysinfo, METH_VARARGS},
    
    // 通用函数
    {"check_pid_range", psutil_check_pid_range, METH_VARARGS},
    {"set_debug", psutil_set_debug, METH_VARARGS},
    
    // 方法表结束标记 (必需)
    {NULL, NULL, 0, NULL}
};
```

### 1.3 异常类型的注册

```c
// psutil/_psutil_windows.c

// 全局异常对象
PyObject *TimeoutExpired;
PyObject *TimeoutAbandoned;

PyMODINIT_FUNC
PyInit__psutil_windows(void) {
    PyObject *mod = PyModule_Create(&moduledef);
    if (mod == NULL)
        return NULL;

    // ... 初始化代码 ...

    // 创建自定义异常类
    TimeoutExpired = PyErr_NewException(
        "_psutil_windows.TimeoutExpired",  // 完整名称
        NULL,                               // 基类 (NULL = Exception)
        NULL                                // 字典 (额外属性)
    );
    if (TimeoutExpired == NULL)
        return NULL;
    
    // 添加到模块
    if (PyModule_AddObject(mod, "TimeoutExpired", TimeoutExpired))
        return NULL;

    TimeoutAbandoned = PyErr_NewException(
        "_psutil_windows.TimeoutAbandoned", NULL, NULL
    );
    if (TimeoutAbandoned == NULL)
        return NULL;
    if (PyModule_AddObject(mod, "TimeoutAbandoned", TimeoutAbandoned))
        return NULL;

    return mod;
}
```

```c
// arch/posix/init.c

// POSIX 系统的 ZombieProcessError
PyObject *ZombieProcessError = NULL;

int
psutil_posix_add_methods(PyObject *mod) {
    // 添加方法...
    
    // 创建并注册 ZombieProcessError 异常
    ZombieProcessError = PyErr_NewException(
        "_psutil_posix.ZombieProcessError", NULL, NULL
    );
    if (ZombieProcessError == NULL)
        return -1;
    if (PyModule_AddObject(mod, "ZombieProcessError", ZombieProcessError))
        return -1;

    return 0;
}
```

---

## 2. 类型转换

### 2.1 C 数据类型与 Python 对象的转换

#### 2.1.1 基本类型转换

```c
// 参数解析：Python -> C
PyObject *
psutil_proc_ioprio_get(PyObject *self, PyObject *args) {
    pid_t pid;
    
    // _Py_PARSE_PID 是平台相关的格式字符串
    // 可能是 "i" (int), "l" (long), 或 "L" (long long)
    if (!PyArg_ParseTuple(args, _Py_PARSE_PID, &pid))
        return NULL;
    
    // ... 操作 ...
}

// 返回值构建：C -> Python
PyObject *
psutil_proc_times(PyObject *self, PyObject *args) {
    // ... 获取数据 ...
    
    // Py_BuildValue 格式字符串：
    // "d" = double (Python float)
    // "i" = int (Python int)
    // "k" = unsigned long (Python int)
    // "K" = unsigned long long (Python int)
    // "s" = char* (Python str)
    // "u" = wchar_t* (Python str)
    // "O" = PyObject* (任意 Python 对象)
    
    return Py_BuildValue(
        "(ddd)",  // 返回 3 个 double 的元组
        user_time,
        system_time,
        create_time
    );
}
```

#### 2.1.2 常用格式字符串

| 格式 | C 类型 | Python 类型 |
|-----|-------|-------------|
| `i` | int | int |
| `I` | unsigned int | int |
| `l` | long | int |
| `k` | unsigned long | int |
| `L` | long long | int |
| `K` | unsigned long long | int |
| `d` | double | float |
| `f` | float | float |
| `s` | char* | str |
| `s#` | char*, Py_ssize_t | bytes |
| `u` | wchar_t* | str |
| `O` | PyObject* | object |
| `O!` | type, PyObject* | 类型检查的 object |

#### 2.1.3 复杂对象构建

```c
// 构建列表
PyObject *
psutil_proc_cpu_affinity_get(PyObject *self, PyObject *args) {
    PyObject *py_list = PyList_New(0);
    if (py_list == NULL)
        return NULL;

    for (int cpu = 0; cpu < num_cpus; cpu++) {
        if (CPU_ISSET(cpu, &cpu_set)) {
            // 创建 Python 整数
            PyObject *cpu_num = PyLong_FromLong(cpu);
            if (cpu_num == NULL) {
                Py_DECREF(py_list);
                return NULL;
            }
            // 添加到列表
            if (PyList_Append(py_list, cpu_num)) {
                Py_DECREF(cpu_num);
                Py_DECREF(py_list);
                return NULL;
            }
            Py_DECREF(cpu_num);  // Append 会增加引用计数
        }
    }
    
    return py_list;
}

// 构建字典
PyObject *
psutil_ppid_map(PyObject *self, PyObject *args) {
    PyObject *py_dict = PyDict_New();
    if (py_dict == NULL)
        return NULL;

    // 遍历进程
    while (/* ... */) {
        PyObject *py_pid = PyLong_FromUnsignedLong(pid);
        PyObject *py_ppid = PyLong_FromUnsignedLong(ppid);
        
        if (py_pid == NULL || py_ppid == NULL) {
            Py_XDECREF(py_pid);
            Py_XDECREF(py_ppid);
            Py_DECREF(py_dict);
            return NULL;
        }
        
        // 设置字典项
        if (PyDict_SetItem(py_dict, py_pid, py_ppid)) {
            Py_DECREF(py_pid);
            Py_DECREF(py_ppid);
            Py_DECREF(py_dict);
            return NULL;
        }
        
        // SetItem 不会偷取引用，需要手动释放
        Py_DECREF(py_pid);
        Py_DECREF(py_ppid);
    }
    
    return py_dict;
}
```

### 2.2 引用计数管理

```c
// 引用计数规则：
// 
// 1. Py_INCREF(obj): 增加引用计数
// 2. Py_DECREF(obj): 减少引用计数，可能释放对象
// 3. Py_XDECREF(obj): 安全版本，obj 可以为 NULL
// 4. Py_XINCREF(obj): 安全版本，obj 可以为 NULL

// 偷取引用 vs 借用引用
// - PyList_SET_ITEM: 偷取引用（不增加计数）
// - PyList_Append: 借用引用（会增加计数）
// - PyTuple_SET_ITEM: 偷取引用
// - PyDict_SetItem: 借用引用

// 正确的模式
PyObject *
example_build_list(void) {
    PyObject *list = PyList_New(3);  // 新引用
    if (list == NULL)
        return NULL;
    
    for (int i = 0; i < 3; i++) {
        PyObject *item = PyLong_FromLong(i);  // 新引用
        if (item == NULL) {
            Py_DECREF(list);
            return NULL;
        }
        // SET_ITEM 偷取引用，不需要 DECREF
        PyList_SET_ITEM(list, i, item);
    }
    
    return list;  // 调用者拥有引用
}

// 错误的模式 - 内存泄漏
PyObject *
bad_example(void) {
    PyObject *list = PyList_New(0);
    PyObject *item = PyLong_FromLong(42);
    
    PyList_Append(list, item);
    // 泄漏! Append 增加了 item 的引用计数
    // 但我们没有释放原始引用
    
    return list;
}

// 正确的模式
PyObject *
good_example(void) {
    PyObject *list = PyList_New(0);
    PyObject *item = PyLong_FromLong(42);
    
    if (PyList_Append(list, item)) {
        Py_XDECREF(item);
        Py_XDECREF(list);
        return NULL;
    }
    Py_DECREF(item);  // 释放我们的引用
    
    return list;
}
```

### 2.3 字符串编码处理

```c
// Linux: 通常是 UTF-8
PyObject *
get_process_name(const char *name) {
    // 从 C 字符串创建 Python str
    return PyUnicode_DecodeFSDefault(name);
    // 或者，如果确定是 UTF-8:
    // return PyUnicode_FromString(name);
}

// Windows: 使用宽字符 (UTF-16)
PyObject *
get_process_name_windows(const wchar_t *name) {
    // 从宽字符串创建 Python str
    return PyUnicode_FromWideChar(name, -1);
    // -1 表示使用 wcslen 计算长度
}

// 处理可能的编码错误
PyObject *
read_proc_file(const char *path) {
    char buffer[4096];
    
    // 读取文件内容...
    
    // 尝试作为 UTF-8 解码，失败则使用替换字符
    PyObject *str = PyUnicode_DecodeUTF8(
        buffer, 
        strlen(buffer), 
        "surrogateescape"  // 错误处理方式
    );
    
    return str;
}
```

---

## 3. 内存管理

### 3.1 C 扩展中的内存分配策略

```c
// 策略 1: 使用栈分配（小型固定大小数据）
PyObject *
small_fixed_data(void) {
    char buffer[256];  // 栈分配
    // 使用 buffer...
    return PyUnicode_FromString(buffer);
}

// 策略 2: 使用 malloc/free（动态大小数据）
PyObject *
dynamic_data(void) {
    size_t size = calculate_size();
    char *buffer = malloc(size);
    
    if (buffer == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    
    // 使用 buffer...
    
    PyObject *result = PyUnicode_FromString(buffer);
    free(buffer);  // 立即释放
    return result;
}

// 策略 3: 使用 Python 内存分配器
PyObject *
python_allocated_data(void) {
    char *buffer = PyMem_Malloc(size);
    if (buffer == NULL) {
        return PyErr_NoMemory();
    }
    
    // 使用 buffer...
    
    PyObject *result = PyUnicode_FromString(buffer);
    PyMem_Free(buffer);
    return result;
}
```

### 3.2 避免内存泄漏

```c
// 模式 1: 使用 goto 进行清理
PyObject *
function_with_cleanup(PyObject *self, PyObject *args) {
    HANDLE handle = NULL;
    char *buffer = NULL;
    PyObject *result = NULL;
    PyObject *temp = NULL;
    
    // 分配资源
    handle = OpenSomething();
    if (handle == NULL)
        goto error;
    
    buffer = malloc(1024);
    if (buffer == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    
    // 构建结果
    temp = PyLong_FromLong(42);
    if (temp == NULL)
        goto error;
    
    result = Py_BuildValue("(Os)", temp, buffer);
    if (result == NULL)
        goto error;
    
    // 成功路径清理
    Py_DECREF(temp);
    free(buffer);
    CloseHandle(handle);
    return result;

error:
    // 错误路径清理
    Py_XDECREF(temp);
    Py_XDECREF(result);
    if (buffer)
        free(buffer);
    if (handle)
        CloseHandle(handle);
    return NULL;
}

// 模式 2: 结构化清理（推荐用于简单情况）
PyObject *
simple_function(PyObject *self, PyObject *args) {
    char buffer[256];  // 栈分配，自动清理
    
    int result = some_syscall(buffer, sizeof(buffer));
    if (result < 0)
        return psutil_oserror();
    
    return PyUnicode_FromString(buffer);
}
```

### 3.3 与 Python 垃圾收集器协同

```c
// 对于包含 Python 对象的 C 结构，需要支持 GC

// 模块状态结构（支持 GC）
struct module_state {
    PyObject *error;
};

// 遍历函数：告诉 GC 我们持有哪些对象
static int
psutil_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

// 清理函数：释放我们持有的对象
static int
psutil_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

// 模块定义中注册这些函数
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "psutil_windows",
    NULL,
    sizeof(struct module_state),
    PsutilMethods,
    NULL,
    psutil_traverse,  // 遍历函数
    psutil_clear,     // 清理函数
    NULL
};
```

---

## 4. 具体函数实现详解

### 4.1 进程信息获取

```c
// arch/windows/proc.c

/*
 * 获取进程 CPU 时间
 * 
 * @param self: 模块对象 (未使用)
 * @param args: Python 参数元组，包含 PID
 * @return: (user_time, system_time, create_time) 元组，单位为秒
 *          失败返回 NULL 并设置异常
 */
PyObject *
psutil_proc_times(PyObject *self, PyObject *args) {
    DWORD pid;
    HANDLE hProcess;
    FILETIME ftCreate, ftExit, ftKernel, ftUser;

    // Step 1: 解析参数
    // _Py_PARSE_PID 根据平台定义为 "i", "l", 或 "L"
    if (!PyArg_ParseTuple(args, _Py_PARSE_PID, &pid))
        return NULL;

    // Step 2: 打开进程句柄
    // PROCESS_QUERY_LIMITED_INFORMATION 是最低权限要求
    hProcess = psutil_handle_from_pid(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    if (hProcess == NULL)
        return NULL;  // 异常已在 handle_from_pid 中设置

    // Step 3: 获取进程时间
    if (!GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            // 进程可能已终止，报告为 NoSuchProcess
            psutil_oserror_nsp("GetProcessTimes -> ERROR_ACCESS_DENIED");
        } else {
            psutil_oserror();
        }
        CloseHandle(hProcess);
        return NULL;
    }

    // Step 4: 清理句柄
    CloseHandle(hProcess);

    // Step 5: 转换时间格式并返回
    // FILETIME 是 100 纳秒间隔的 64 位值
    // 需要转换为秒（double）
    double user = filetime_to_double(ftUser);
    double kernel = filetime_to_double(ftKernel);
    double create = filetime_to_unix_time(ftCreate);
    
    return Py_BuildValue("(ddd)", user, kernel, create);
}
```

### 4.2 系统信息获取

```c
// arch/linux/mem.c

/*
 * 获取系统内存信息 (通过 sysinfo 系统调用)
 * 
 * @return: (total, free, buffers, shared, available) 元组
 *          单位为字节
 */
PyObject *
psutil_linux_sysinfo(PyObject *self, PyObject *args) {
    struct sysinfo info;
    
    // 调用系统调用
    if (sysinfo(&info) != 0)
        return psutil_oserror();
    
    // 注意：mem_unit 是内存单位乘数
    // 在大多数系统上是 1，但某些系统可能不同
    unsigned long long total = (unsigned long long)info.totalram * info.mem_unit;
    unsigned long long free = (unsigned long long)info.freeram * info.mem_unit;
    unsigned long long buffers = (unsigned long long)info.bufferram * info.mem_unit;
    unsigned long long shared = (unsigned long long)info.sharedram * info.mem_unit;
    
    // 返回元组
    return Py_BuildValue("(KKKK)", total, free, buffers, shared);
}
```

### 4.3 网络连接枚举

```c
// arch/windows/socks.c (简化版)

/*
 * 获取系统网络连接
 * 
 * @param kind: 连接类型 ("tcp4", "tcp6", "udp4", "udp6", "all")
 * @return: 连接信息列表
 */
PyObject *
psutil_net_connections(PyObject *self, PyObject *args) {
    PyObject *py_retlist = NULL;
    PyObject *py_conn_tuple = NULL;
    MIB_TCPTABLE_OWNER_PID *table = NULL;
    DWORD size = 0;
    DWORD ret;
    
    // Step 1: 获取所需缓冲区大小
    ret = GetExtendedTcpTable(NULL, &size, FALSE, AF_INET,
                               TCP_TABLE_OWNER_PID_ALL, 0);
    if (ret != ERROR_INSUFFICIENT_BUFFER) {
        psutil_oserror();
        return NULL;
    }
    
    // Step 2: 分配缓冲区
    table = (MIB_TCPTABLE_OWNER_PID *)malloc(size);
    if (table == NULL)
        return PyErr_NoMemory();
    
    // Step 3: 获取实际数据
    ret = GetExtendedTcpTable(table, &size, FALSE, AF_INET,
                               TCP_TABLE_OWNER_PID_ALL, 0);
    if (ret != NO_ERROR) {
        free(table);
        psutil_oserror();
        return NULL;
    }
    
    // Step 4: 构建结果列表
    py_retlist = PyList_New(0);
    if (py_retlist == NULL) {
        free(table);
        return NULL;
    }
    
    for (DWORD i = 0; i < table->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID row = table->table[i];
        
        // 转换 IP 地址
        char local_addr[16], remote_addr[16];
        inet_ntop(AF_INET, &row.dwLocalAddr, local_addr, sizeof(local_addr));
        inet_ntop(AF_INET, &row.dwRemoteAddr, remote_addr, sizeof(remote_addr));
        
        // 构建连接元组
        py_conn_tuple = Py_BuildValue(
            "(iiisHsHiI)",
            -1,  // fd (Windows 不适用)
            AF_INET,
            SOCK_STREAM,
            local_addr,
            ntohs((u_short)row.dwLocalPort),
            remote_addr,
            ntohs((u_short)row.dwRemotePort),
            row.dwState,
            row.dwOwningPid
        );
        
        if (py_conn_tuple == NULL)
            goto error;
        
        if (PyList_Append(py_retlist, py_conn_tuple)) {
            Py_DECREF(py_conn_tuple);
            goto error;
        }
        Py_DECREF(py_conn_tuple);
    }
    
    free(table);
    return py_retlist;

error:
    free(table);
    Py_XDECREF(py_retlist);
    return NULL;
}
```

---

## 5. 最佳实践总结

### 5.1 代码组织

| 实践 | 说明 |
|-----|------|
| 统一头文件 | 所有 .c 文件只包含 `arch/all/init.h` |
| 平台分离 | 每个平台有自己的目录和 init.h |
| 条件编译 | 使用 `#ifdef` 处理可选功能 |
| 错误处理函数 | 使用 `psutil_oserror()` 等统一函数 |

### 5.2 引用计数规则

| 规则 | 说明 |
|-----|------|
| 返回新引用 | 函数返回的对象，调用者负责释放 |
| 借用引用 | 从容器获取的对象，不需要释放 |
| 偷取引用 | `PyList_SET_ITEM` 等，不需要释放 |
| 错误路径 | 必须释放所有分配的资源 |

### 5.3 内存管理规则

| 规则 | 说明 |
|-----|------|
| 栈分配优先 | 固定大小的小数据使用栈 |
| 及时释放 | malloc 后尽快 free |
| goto 清理 | 复杂函数使用 goto 统一清理 |
| Python 分配器 | Python 对象使用 PyMem_* |

