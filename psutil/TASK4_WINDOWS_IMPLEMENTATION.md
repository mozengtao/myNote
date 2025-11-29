# Task 4: psutil Windows 平台实现分析

## 概述

本文档分析 psutil 在 Windows 平台的实现差异，包括 Windows API 封装、进程快照机制和特权处理。

---

## 1. Windows API 封装

### 1.1 使用的 Windows 动态库

| 动态库 | 用途 | psutil 使用场景 |
|-------|------|----------------|
| `kernel32.dll` | 核心系统功能 | 进程/线程、内存、文件 |
| `psapi.dll` | 进程状态 API | 进程内存信息、模块枚举 |
| `advapi32.dll` | 高级 API | 安全、注册表、服务 |
| `netapi32.dll` | 网络管理 | 用户信息 |
| `pdh.dll` | 性能数据助手 | CPU 使用率、计数器 |
| `PowrProf.dll` | 电源管理 | 电池状态 |
| `ws2_32.dll` | Winsock 2 | 网络连接 |
| `iphlpapi.dll` | IP 辅助 | 网络接口、路由表 |

### 1.2 `setup.py` 中的库链接配置

```python
# setup.py
if WINDOWS:
    ext = Extension(
        'psutil._psutil_windows',
        sources=(
            sources
            + ["psutil/_psutil_windows.c"]
            + glob.glob("psutil/arch/windows/*.c")
        ),
        define_macros=macros,
        libraries=[
            "advapi32",
            "kernel32",
            "netapi32",
            "pdh",
            "PowrProf",
            "psapi",
            "shell32",
            "ws2_32",
        ],
    )
```

### 1.3 C 扩展中的 Windows API 调用

#### 1.3.1 进程时间获取 (`arch/windows/proc.c`)

```c
PyObject *
psutil_proc_times(PyObject *self, PyObject *args) {
    DWORD pid;
    HANDLE hProcess;
    FILETIME ftCreate, ftExit, ftKernel, ftUser;

    if (!PyArg_ParseTuple(args, _Py_PARSE_PID, &pid))
        return NULL;

    // 打开进程句柄
    hProcess = psutil_handle_from_pid(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    if (hProcess == NULL)
        return NULL;
    
    // 调用 Windows API
    if (!GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            // 进程可能已死亡
            psutil_oserror_nsp("GetProcessTimes -> ERROR_ACCESS_DENIED");
        } else {
            psutil_oserror();
        }
        CloseHandle(hProcess);
        return NULL;
    }

    CloseHandle(hProcess);

    // FILETIME 是 100 纳秒间隔的 64 位值
    // 转换为秒
    return Py_BuildValue(
        "(ddd)",
        (double)(IsWindowsXPOrLater() ? 
            *((unsigned long long*)&ftUser) : 0) * 1e-7,
        (double)(IsWindowsXPOrLater() ? 
            *((unsigned long long*)&ftKernel) : 0) * 1e-7,
        // 创建时间转换...
    );
}
```

#### 1.3.2 内存信息获取 (`arch/windows/proc.c`)

```c
PyObject *
psutil_proc_memory_info(PyObject *self, PyObject *args) {
    DWORD pid;
    HANDLE hProcess;
    PROCESS_MEMORY_COUNTERS_EX pmc;

    if (!PyArg_ParseTuple(args, _Py_PARSE_PID, &pid))
        return NULL;

    hProcess = psutil_handle_from_pid(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    if (hProcess == NULL)
        return NULL;

    // 获取内存计数器
    if (!GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS)&pmc, 
                              sizeof(pmc))) {
        psutil_oserror();
        CloseHandle(hProcess);
        return NULL;
    }

    CloseHandle(hProcess);

    // 返回 Windows 特有的内存指标
    return Py_BuildValue(
        "(kKKKKKKKKKKK)",
        pmc.PageFaultCount,        // 页面错误数
        pmc.PeakWorkingSetSize,    // 峰值工作集
        pmc.WorkingSetSize,        // 当前工作集 (rss)
        pmc.QuotaPeakPagedPoolUsage,
        pmc.QuotaPagedPoolUsage,
        pmc.QuotaPeakNonPagedPoolUsage,
        pmc.QuotaNonPagedPoolUsage,
        pmc.PagefileUsage,         // 页面文件使用 (vms)
        pmc.PeakPagefileUsage,
        pmc.PrivateUsage           // 私有字节
    );
}
```

### 1.4 未文档化 NT API 的使用

```c
// arch/windows/ntextapi.h - 未文档化的 NT 原生 API

// NtQuerySystemInformation 用于获取系统级进程信息
typedef NTSTATUS (NTAPI *_NtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

// NtQueryInformationProcess 用于获取进程详情
typedef NTSTATUS (NTAPI *_NtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

// NtSuspendProcess / NtResumeProcess 用于进程挂起/恢复
typedef NTSTATUS (NTAPI *_NtSuspendProcess)(HANDLE ProcessHandle);
typedef NTSTATUS (NTAPI *_NtResumeProcess)(HANDLE ProcessHandle);
```

### 1.5 Win32 编程与 Python 封装的差异

| 方面 | Win32 C 编程 | psutil Python 封装 |
|-----|-------------|-------------------|
| 句柄管理 | 手动 `OpenProcess`/`CloseHandle` | 自动封装在函数内 |
| 错误处理 | `GetLastError()` + switch | 自动转换为 Python 异常 |
| 数据类型 | DWORD, HANDLE, FILETIME | Python int, float |
| 字符串 | WCHAR (UTF-16) | Python str (自动转换) |
| 权限 | 手动请求权限 | 自动尝试降级权限 |
| 缓冲区 | 手动分配/扩展 | 自动处理 |

---

## 2. 进程快照机制

### 2.1 `CreateToolhelp32Snapshot` 的使用

```c
// arch/windows/pids.c

int
_psutil_pids(DWORD **pids_array, int *pids_count) {
    DWORD *proc_array = NULL;
    DWORD proc_array_bytes;
    int proc_array_sz = 0;
    DWORD enum_return_bytes = 0;

    *pids_array = NULL;
    *pids_count = 0;

    // 动态扩展缓冲区
    do {
        proc_array_sz += 1024;
        if (proc_array != NULL)
            free(proc_array);

        proc_array_bytes = proc_array_sz * sizeof(DWORD);
        proc_array = malloc(proc_array_bytes);
        if (proc_array == NULL) {
            PyErr_NoMemory();
            return -1;
        }

        // EnumProcesses 返回所有进程 ID
        if (!EnumProcesses(proc_array, proc_array_bytes, &enum_return_bytes)) {
            free(proc_array);
            psutil_oserror();
            return -1;
        }

        // 如果返回的字节数等于缓冲区大小，可能不完整
    } while (enum_return_bytes == proc_array_bytes);

    *pids_count = (int)(enum_return_bytes / sizeof(DWORD));
    *pids_array = proc_array;
    return 0;
}
```

### 2.2 进程枚举的详细实现

```c
// 使用 Toolhelp32 API 进行进程枚举
PyObject *
psutil_pids(PyObject *self, PyObject *args) {
    DWORD *pids;
    int pids_count;
    PyObject *py_retlist = NULL;
    PyObject *py_pid = NULL;

    // 获取 PID 数组
    if (_psutil_pids(&pids, &pids_count) != 0)
        return NULL;

    // 创建 Python 列表
    py_retlist = PyList_New(pids_count);
    if (py_retlist == NULL) {
        free(pids);
        return NULL;
    }

    // 填充列表
    for (int i = 0; i < pids_count; i++) {
        py_pid = PyLong_FromUnsignedLong(pids[i]);
        if (py_pid == NULL) {
            Py_DECREF(py_retlist);
            free(pids);
            return NULL;
        }
        // PyList_SET_ITEM 偷取引用
        PyList_SET_ITEM(py_retlist, i, py_pid);
    }

    free(pids);
    return py_retlist;
}
```

### 2.3 线程枚举

```c
PyObject *
psutil_proc_threads(PyObject *self, PyObject *args) {
    DWORD pid;
    HANDLE hThread;
    THREADENTRY32 te32;
    HANDLE hSnapshot;
    PyObject *py_retlist = NULL;
    PyObject *py_tuple = NULL;
    FILETIME ftCreate, ftExit, ftKernel, ftUser;

    if (!PyArg_ParseTuple(args, _Py_PARSE_PID, &pid))
        return NULL;

    // 创建线程快照
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        psutil_oserror();
        return NULL;
    }

    py_retlist = PyList_New(0);
    if (py_retlist == NULL) {
        CloseHandle(hSnapshot);
        return NULL;
    }

    te32.dwSize = sizeof(THREADENTRY32);

    // 遍历线程
    if (!Thread32First(hSnapshot, &te32)) {
        // 空快照是合法的
        if (GetLastError() != ERROR_NO_MORE_FILES) {
            psutil_oserror();
            goto error;
        }
        goto done;
    }

    do {
        if (te32.th32OwnerProcessID != pid)
            continue;

        // 打开线程获取时间信息
        hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
        if (hThread == NULL) {
            // 权限不足，跳过此线程
            continue;
        }

        if (GetThreadTimes(hThread, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
            py_tuple = Py_BuildValue(
                "(Idd)",
                te32.th32ThreadID,
                FILETIME_TO_DOUBLE(ftUser),
                FILETIME_TO_DOUBLE(ftKernel)
            );
            if (py_tuple == NULL) {
                CloseHandle(hThread);
                goto error;
            }
            if (PyList_Append(py_retlist, py_tuple)) {
                Py_DECREF(py_tuple);
                CloseHandle(hThread);
                goto error;
            }
            Py_DECREF(py_tuple);
        }
        CloseHandle(hThread);
    } while (Thread32Next(hSnapshot, &te32));

done:
    CloseHandle(hSnapshot);
    return py_retlist;

error:
    Py_XDECREF(py_retlist);
    CloseHandle(hSnapshot);
    return NULL;
}
```

### 2.4 性能优化考虑

```c
// 使用 ppid_map 一次性获取所有进程的父进程 ID
PyObject *
psutil_ppid_map(PyObject *self, PyObject *args) {
    HANDLE hSnapshot;
    PROCESSENTRY32W pe32 = {0};
    PyObject *py_retdict = NULL;
    PyObject *py_pid = NULL;
    PyObject *py_ppid = NULL;

    // 创建进程快照
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        psutil_oserror();
        return NULL;
    }

    py_retdict = PyDict_New();
    if (py_retdict == NULL) {
        CloseHandle(hSnapshot);
        return NULL;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &pe32)) {
        if (GetLastError() != ERROR_NO_MORE_FILES) {
            psutil_oserror();
            goto error;
        }
        goto done;
    }

    // 一次遍历获取所有 PID -> PPID 映射
    do {
        py_pid = PyLong_FromUnsignedLong(pe32.th32ProcessID);
        py_ppid = PyLong_FromUnsignedLong(pe32.th32ParentProcessID);
        if (py_pid == NULL || py_ppid == NULL) {
            Py_XDECREF(py_pid);
            Py_XDECREF(py_ppid);
            goto error;
        }
        if (PyDict_SetItem(py_retdict, py_pid, py_ppid)) {
            Py_DECREF(py_pid);
            Py_DECREF(py_ppid);
            goto error;
        }
        Py_DECREF(py_pid);
        Py_DECREF(py_ppid);
    } while (Process32NextW(hSnapshot, &pe32));

done:
    CloseHandle(hSnapshot);
    return py_retdict;

error:
    Py_XDECREF(py_retdict);
    CloseHandle(hSnapshot);
    return NULL;
}
```

---

## 3. 特权处理

### 3.1 管理员权限处理

#### 3.1.1 权限设置 (`arch/windows/security.c`)

```c
// 启用 SE_DEBUG_NAME 特权以访问其他进程
int
psutil_set_se_debug() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    LUID luid;

    // 打开当前进程令牌
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &hToken)) {
        // 权限不足不是致命错误
        psutil_debug("OpenProcessToken() failed");
        return 0;
    }

    // 查找 SE_DEBUG_NAME 特权的 LUID
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        psutil_debug("LookupPrivilegeValue() failed");
        CloseHandle(hToken);
        return 0;
    }

    // 设置特权
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, 
                                sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        psutil_debug("AdjustTokenPrivileges() failed");
        CloseHandle(hToken);
        return 0;
    }

    CloseHandle(hToken);
    
    // 检查是否真的成功（可能被策略阻止）
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        psutil_debug("SE_DEBUG_NAME privilege not assigned");
        return 0;
    }

    return 0;  // 成功或优雅失败
}
```

#### 3.1.2 进程句柄获取（带降级策略）

```c
// arch/windows/proc_utils.c

HANDLE
psutil_handle_from_pid(DWORD pid, DWORD access) {
    HANDLE hProcess;

    if (pid == 0) {
        // PID 0 是特殊的 System Idle Process
        psutil_oserror_ad("PID 0 is not a valid target");
        return NULL;
    }

    // 首先尝试完整权限
    hProcess = OpenProcess(access, FALSE, pid);
    if (hProcess != NULL) {
        return hProcess;
    }

    // 如果失败，尝试使用更少的权限
    if (GetLastError() == ERROR_ACCESS_DENIED) {
        // 尝试 PROCESS_QUERY_LIMITED_INFORMATION
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess != NULL) {
            return hProcess;
        }
    }

    // 检查进程是否存在
    if (GetLastError() == ERROR_INVALID_PARAMETER) {
        // 进程不存在
        psutil_oserror_nsp("OpenProcess");
        return NULL;
    }

    // 其他错误
    psutil_oserror();
    return NULL;
}
```

### 3.2 错误处理和降级策略

```c
// 进程信息获取的错误处理
PyObject *
psutil_proc_info(PyObject *self, PyObject *args) {
    DWORD pid;
    HANDLE hProcess;
    DWORD access;

    if (!PyArg_ParseTuple(args, _Py_PARSE_PID, &pid))
        return NULL;

    // 尝试不同的访问级别
    static DWORD access_levels[] = {
        PROCESS_QUERY_INFORMATION,
        PROCESS_QUERY_LIMITED_INFORMATION,
        0
    };

    for (int i = 0; access_levels[i] != 0; i++) {
        access = access_levels[i];
        hProcess = OpenProcess(access, FALSE, pid);
        if (hProcess != NULL) {
            break;
        }
        if (GetLastError() != ERROR_ACCESS_DENIED) {
            break;  // 不是权限问题
        }
    }

    if (hProcess == NULL) {
        if (GetLastError() == ERROR_INVALID_PARAMETER) {
            return psutil_oserror_nsp("OpenProcess");
        }
        return psutil_oserror();
    }

    // 获取能获取的信息
    PyObject *result = get_available_info(hProcess, access);
    CloseHandle(hProcess);
    return result;
}
```

### 3.3 Windows 特有的错误码处理

```python
# psutil/_pswindows.py

# Windows 错误码映射
def handle_windows_error(err_code, pid, name=None):
    """Convert Windows error code to psutil exception."""
    if err_code == cext.ERROR_ACCESS_DENIED:
        raise AccessDenied(pid, name)
    elif err_code == cext.ERROR_INVALID_PARAMETER:
        raise NoSuchProcess(pid, name)
    elif err_code == cext.ERROR_PRIVILEGE_NOT_HELD:
        raise AccessDenied(pid, name, msg="privilege not held")
    else:
        raise OSError(err_code, os.strerror(err_code))
```

---

## 4. Windows 特有功能

### 4.1 Windows 服务管理

```c
// arch/windows/services.c

PyObject *
psutil_winservice_enumerate(PyObject *self, PyObject *args) {
    ENUM_SERVICE_STATUS_PROCESSW *services = NULL;
    SC_HANDLE sc = NULL;
    DWORD bytes_needed = 0;
    DWORD num_services = 0;
    PyObject *py_retlist = NULL;
    PyObject *py_tuple = NULL;

    // 打开服务控制管理器
    sc = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (sc == NULL) {
        psutil_oserror();
        return NULL;
    }

    // 第一次调用获取所需缓冲区大小
    EnumServicesStatusExW(
        sc,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        NULL,
        0,
        &bytes_needed,
        &num_services,
        NULL,
        NULL
    );

    if (GetLastError() != ERROR_MORE_DATA) {
        psutil_oserror();
        goto error;
    }

    // 分配缓冲区
    services = (ENUM_SERVICE_STATUS_PROCESSW*)malloc(bytes_needed);
    if (services == NULL) {
        PyErr_NoMemory();
        goto error;
    }

    // 实际枚举
    if (!EnumServicesStatusExW(
            sc,
            SC_ENUM_PROCESS_INFO,
            SERVICE_WIN32,
            SERVICE_STATE_ALL,
            (LPBYTE)services,
            bytes_needed,
            &bytes_needed,
            &num_services,
            NULL,
            NULL)) {
        psutil_oserror();
        goto error;
    }

    // 构建返回列表
    py_retlist = PyList_New(num_services);
    if (py_retlist == NULL)
        goto error;

    for (DWORD i = 0; i < num_services; i++) {
        py_tuple = Py_BuildValue(
            "(uuI)",
            services[i].lpServiceName,
            services[i].lpDisplayName,
            services[i].ServiceStatusProcess.dwCurrentState
        );
        if (py_tuple == NULL)
            goto error;
        PyList_SET_ITEM(py_retlist, i, py_tuple);
    }

    free(services);
    CloseServiceHandle(sc);
    return py_retlist;

error:
    if (services)
        free(services);
    if (sc)
        CloseServiceHandle(sc);
    Py_XDECREF(py_retlist);
    return NULL;
}
```

### 4.2 性能计数器 (PDH)

```c
// arch/windows/cpu.c - 使用 PDH 获取 CPU 使用率

static PDH_HQUERY cpu_query = NULL;
static PDH_HCOUNTER *cpu_counters = NULL;
static int num_cpus = 0;

int
psutil_init_cpu_counters() {
    PDH_STATUS status;
    wchar_t counter_path[256];

    // 创建查询
    status = PdhOpenQueryW(NULL, 0, &cpu_query);
    if (status != ERROR_SUCCESS)
        return -1;

    num_cpus = psutil_get_num_cpus();
    cpu_counters = malloc(sizeof(PDH_HCOUNTER) * num_cpus);

    // 为每个 CPU 添加计数器
    for (int i = 0; i < num_cpus; i++) {
        swprintf(counter_path, 256, 
                 L"\\Processor(%d)\\%% Processor Time", i);
        status = PdhAddEnglishCounterW(cpu_query, counter_path, 0, 
                                       &cpu_counters[i]);
        if (status != ERROR_SUCCESS) {
            // 清理并返回错误
            return -1;
        }
    }

    return 0;
}
```

---

## 5. 关键差异总结

### 5.1 与 Linux 实现的对比

| 方面 | Linux 实现 | Windows 实现 |
|-----|-----------|-------------|
| 进程信息来源 | `/proc` 文件系统 | Windows API |
| 进程枚举 | 读取 `/proc` 目录 | `EnumProcesses()` |
| 内存信息 | `/proc/[pid]/statm` | `GetProcessMemoryInfo()` |
| CPU 时间 | `/proc/[pid]/stat` | `GetProcessTimes()` |
| 线程枚举 | `/proc/[pid]/task` | `CreateToolhelp32Snapshot()` |
| 网络连接 | `/proc/net/tcp` | `GetExtendedTcpTable()` |
| 权限处理 | `errno == EACCES` | `GetLastError() == ERROR_ACCESS_DENIED` |
| 特权提升 | `setuid` / capabilities | `AdjustTokenPrivileges()` |

### 5.2 Windows 特有挑战

1. **句柄管理**：必须手动关闭，容易泄漏
2. **权限模型**：更复杂，需要多级降级策略
3. **Unicode 处理**：Windows 使用 UTF-16 (WCHAR)
4. **未文档化 API**：某些功能需要使用 NT 原生 API
5. **版本差异**：不同 Windows 版本 API 行为不同

