# Task 8: psutil 测试体系分析

## 概述

本文档分析 psutil 的测试体系，包括单元测试设计、集成测试、持续集成配置和测试用例编写示例。

---

## 1. 单元测试设计

### 1.1 测试目录组织

```
tests/
├── __init__.py           # 测试工具、常量、辅助函数
├── README.md             # 测试说明
│
├── test_process.py       # Process 类测试 (跨平台)
├── test_process_all.py   # 遍历所有进程的测试
├── test_system.py        # 系统级函数测试 (跨平台)
├── test_connections.py   # 网络连接测试
├── test_contracts.py     # API 契约测试
├── test_misc.py          # 杂项测试
├── test_unicode.py       # Unicode 处理测试
├── test_memleaks.py      # 内存泄漏测试
├── test_heap.py          # 堆内存测试
├── test_testutils.py     # 测试工具自身的测试
│
├── test_posix.py         # POSIX 平台测试
├── test_linux.py         # Linux 平台测试
├── test_windows.py       # Windows 平台测试
├── test_osx.py           # macOS 平台测试
├── test_bsd.py           # BSD 平台测试
├── test_sunos.py         # Solaris 平台测试
├── test_aix.py           # AIX 平台测试
│
├── test_sudo.py          # 需要 root 权限的测试
└── test_scripts.py       # scripts/ 目录脚本测试
```

### 1.2 测试基类和工具 (`tests/__init__.py`)

```python
# tests/__init__.py

import contextlib
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest

import psutil
from psutil import LINUX, WINDOWS, MACOS, POSIX

# ===================================================================
# --- 常量
# ===================================================================

# CI 环境检测
CI_TESTING = 'GITHUB_ACTIONS' in os.environ
GITHUB_ACTIONS = CI_TESTING

# 超时设置（CI 环境更宽松）
GLOBAL_TIMEOUT = 5 if not CI_TESTING else 15
NO_RETRIES = 10 if not CI_TESTING else 30

# 容差设置
TOLERANCE_SYS_MEM = 5 * 1024 * 1024  # 5MB
TOLERANCE_DISK_USAGE = 10 * 1024 * 1024  # 10MB

# Python 解释器路径
PYTHON_EXE = sys.executable
PYTHON_EXE_ENV = os.environ.copy()

# 功能可用性检测
HAS_CPU_AFFINITY = hasattr(psutil.Process, "cpu_affinity")
HAS_IONICE = hasattr(psutil.Process, "ionice")
HAS_MEMORY_MAPS = hasattr(psutil.Process, "memory_maps")
HAS_RLIMIT = hasattr(psutil.Process, "rlimit")
HAS_SENSORS_BATTERY = hasattr(psutil, "sensors_battery")

# ===================================================================
# --- 装饰器
# ===================================================================

def skip_on_access_denied(only_if=None):
    """跳过因权限不足而失败的测试。"""
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
    """跳过因功能未实现而失败的测试。"""
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

def retry_on_failure(ntimes=NO_RETRIES):
    """失败时重试（处理竞态条件）。"""
    def decorator(fun):
        @functools.wraps(fun)
        def wrapper(*args, **kwargs):
            for i in range(ntimes):
                try:
                    return fun(*args, **kwargs)
                except AssertionError:
                    if i == ntimes - 1:
                        raise
                    time.sleep(0.1)
        return wrapper
    return decorator

# ===================================================================
# --- 测试基类
# ===================================================================

class PsutilTestCase(unittest.TestCase):
    """psutil 测试的基类。"""
    
    def setUp(self):
        self._subprocesses = []
    
    def tearDown(self):
        # 清理所有子进程
        reap_children()
        for proc in self._subprocesses:
            try:
                proc.terminate()
                proc.wait(timeout=3)
            except Exception:
                pass
    
    def spawn_psproc(self, cmd=None, **kwargs):
        """创建并跟踪一个子进程。"""
        if cmd is None:
            cmd = [PYTHON_EXE, "-c", "import time; time.sleep(60)"]
        p = psutil.Popen(cmd, **kwargs)
        self._subprocesses.append(p)
        return p
    
    def assert_proc_gone(self, proc):
        """断言进程已终止。"""
        assert not proc.is_running()
        with self.assertRaises(psutil.NoSuchProcess):
            proc.status()

# ===================================================================
# --- 辅助函数
# ===================================================================

def reap_children(timeout=3):
    """终止并回收所有子进程。"""
    procs = psutil.Process().children(recursive=True)
    for p in procs:
        try:
            p.terminate()
        except psutil.NoSuchProcess:
            pass
    gone, alive = psutil.wait_procs(procs, timeout=timeout)
    for p in alive:
        try:
            p.kill()
        except psutil.NoSuchProcess:
            pass

def wait_for_pid(pid, timeout=GLOBAL_TIMEOUT):
    """等待 PID 出现。"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if psutil.pid_exists(pid):
            return
        time.sleep(0.01)
    raise RuntimeError(f"PID {pid} not found")

def get_free_port():
    """获取一个空闲端口。"""
    import socket
    with socket.socket() as sock:
        sock.bind(('', 0))
        return sock.getsockname()[1]
```

### 1.3 模拟（Mock）技术的使用

```python
# tests/test_process.py

from unittest import mock
import psutil

class TestProcessMock(PsutilTestCase):
    """使用 mock 的进程测试。"""
    
    @pytest.mark.skipif(not POSIX, reason="POSIX only")
    def test_send_signal_mocked(self):
        """模拟 os.kill 的异常情况。"""
        p = self.spawn_psproc()
        
        # 模拟进程不存在
        with mock.patch('psutil.os.kill', side_effect=ProcessLookupError):
            with pytest.raises(psutil.NoSuchProcess):
                p.send_signal(signal.SIGTERM)
        
        # 模拟权限不足
        p = self.spawn_psproc()
        with mock.patch('psutil.os.kill', side_effect=PermissionError):
            with pytest.raises(psutil.AccessDenied):
                p.send_signal(signal.SIGTERM)
    
    def test_name_mocked(self):
        """模拟 name() 方法的返回值。"""
        p = psutil.Process()
        original_name = p.name()
        
        with mock.patch.object(p._proc, 'name', return_value='mocked_name'):
            # 清除缓存
            p._name = None
            assert p.name() == 'mocked_name'
        
        # 恢复后应该正常
        p._name = None
        assert p.name() == original_name
```

### 1.4 平台特定测试处理

```python
# tests/test_linux.py

import pytest
import psutil
from psutil import LINUX

# 整个模块仅在 Linux 上运行
pytestmark = pytest.mark.skipif(not LINUX, reason="Linux only")


class TestLinuxSpecific:
    """Linux 特有功能测试。"""
    
    def test_rlimit(self):
        """测试资源限制。"""
        p = psutil.Process()
        
        # 获取当前限制
        soft, hard = p.rlimit(psutil.RLIMIT_NOFILE)
        assert soft > 0
        assert hard > 0
        assert soft <= hard
        
        # 尝试设置限制
        try:
            p.rlimit(psutil.RLIMIT_NOFILE, (soft, hard))
        except psutil.AccessDenied:
            pass  # 某些环境可能没有权限
    
    def test_ionice(self):
        """测试 I/O 优先级。"""
        p = psutil.Process()
        initial = p.ionice()
        
        assert initial.ioclass in (
            psutil.IOPRIO_CLASS_NONE,
            psutil.IOPRIO_CLASS_RT,
            psutil.IOPRIO_CLASS_BE,
            psutil.IOPRIO_CLASS_IDLE,
        )
        
        # 尝试设置 I/O 优先级
        try:
            p.ionice(psutil.IOPRIO_CLASS_BE, value=4)
            new = p.ionice()
            assert new.ioclass == psutil.IOPRIO_CLASS_BE
            assert new.value == 4
        finally:
            # 恢复原始值
            try:
                p.ionice(initial.ioclass, initial.value)
            except Exception:
                pass
    
    def test_cpu_affinity(self):
        """测试 CPU 亲和性。"""
        p = psutil.Process()
        initial = p.cpu_affinity()
        
        assert isinstance(initial, list)
        assert len(initial) > 0
        assert all(isinstance(cpu, int) for cpu in initial)
        
        # 尝试设置亲和性
        if len(initial) > 1:
            try:
                p.cpu_affinity([initial[0]])
                new = p.cpu_affinity()
                assert new == [initial[0]]
            finally:
                p.cpu_affinity(initial)
```

---

## 2. 集成测试

### 2.1 系统级测试实现

```python
# tests/test_system.py

class TestSystemAPIs(PsutilTestCase):
    """系统级 API 测试。"""
    
    def test_cpu_count(self):
        """测试 CPU 计数。"""
        logical = psutil.cpu_count()
        physical = psutil.cpu_count(logical=False)
        
        assert logical >= 1
        assert physical is None or physical >= 1
        if physical is not None:
            assert logical >= physical
    
    def test_cpu_times(self):
        """测试 CPU 时间统计。"""
        times = psutil.cpu_times()
        
        # 基本字段应该存在
        assert hasattr(times, 'user')
        assert hasattr(times, 'system')
        assert hasattr(times, 'idle')
        
        # 所有值应该非负
        for field in times._fields:
            value = getattr(times, field)
            assert value >= 0, f"{field}={value}"
    
    def test_virtual_memory(self):
        """测试虚拟内存统计。"""
        mem = psutil.virtual_memory()
        
        assert mem.total > 0
        assert mem.available >= 0
        assert mem.available <= mem.total
        assert 0 <= mem.percent <= 100
        assert mem.used >= 0
        assert mem.free >= 0
    
    def test_disk_usage(self):
        """测试磁盘使用统计。"""
        usage = psutil.disk_usage('/')
        
        assert usage.total > 0
        assert usage.used >= 0
        assert usage.free >= 0
        assert usage.used + usage.free <= usage.total + TOLERANCE_DISK_USAGE
        assert 0 <= usage.percent <= 100
    
    def test_net_io_counters(self):
        """测试网络 I/O 计数器。"""
        counters = psutil.net_io_counters()
        
        if counters is None:
            pytest.skip("no network interfaces")
        
        assert counters.bytes_sent >= 0
        assert counters.bytes_recv >= 0
        assert counters.packets_sent >= 0
        assert counters.packets_recv >= 0
```

### 2.2 特权测试处理策略

```python
# tests/test_sudo.py

"""需要 root/admin 权限的测试。

运行方式: sudo python -m pytest tests/test_sudo.py
"""

import os
import pytest
import psutil

# 跳过非 root 环境
pytestmark = pytest.mark.skipif(
    os.geteuid() != 0 if hasattr(os, 'geteuid') else True,
    reason="requires root"
)


class TestRootRequired:
    """需要 root 权限的测试。"""
    
    def test_other_process_name(self):
        """以 root 身份访问其他进程的名称。"""
        for p in psutil.process_iter(['name']):
            # 应该能访问所有进程
            assert p.info['name'] is not None
    
    def test_rlimit_set(self):
        """设置资源限制。"""
        p = psutil.Process()
        soft, hard = p.rlimit(psutil.RLIMIT_NOFILE)
        
        # 只有 root 可以提高硬限制
        new_hard = hard + 1
        p.rlimit(psutil.RLIMIT_NOFILE, (soft, new_hard))
        
        soft2, hard2 = p.rlimit(psutil.RLIMIT_NOFILE)
        assert hard2 == new_hard
        
        # 恢复
        p.rlimit(psutil.RLIMIT_NOFILE, (soft, hard))
```

### 2.3 性能测试设计

```python
# tests/test_misc.py

import time
import psutil

class TestPerformance:
    """性能相关测试。"""
    
    def test_process_iter_performance(self):
        """测试 process_iter 的性能。"""
        start = time.time()
        count = 0
        for p in psutil.process_iter(['name', 'pid']):
            count += 1
        elapsed = time.time() - start
        
        # 应该在合理时间内完成
        assert elapsed < 10, f"took {elapsed}s for {count} processes"
    
    def test_oneshot_performance(self):
        """测试 oneshot 的性能提升。"""
        p = psutil.Process()
        
        # 不使用 oneshot
        start = time.time()
        for _ in range(100):
            p.name()
            p.cpu_times()
            p.memory_info()
        without_oneshot = time.time() - start
        
        # 使用 oneshot
        start = time.time()
        for _ in range(100):
            with p.oneshot():
                p.name()
                p.cpu_times()
                p.memory_info()
        with_oneshot = time.time() - start
        
        # oneshot 应该更快
        # 由于缓存，至少应该快 20%
        assert with_oneshot < without_oneshot * 0.9
```

---

## 3. 持续集成

### 3.1 GitHub Actions 配置 (`.github/workflows/build.yml`)

```yaml
# .github/workflows/build.yml

name: build
on: [push, pull_request]

concurrency:
  group: ${{ github.ref }}-${{ github.workflow }}
  cancel-in-progress: true

jobs:
  # 多平台测试
  tests:
    name: "${{ matrix.os }}, ${{ matrix.arch }}"
    runs-on: ${{ matrix.os }}
    timeout-minutes: 15
    strategy:
      matrix:
        include:
          - { os: ubuntu-latest, arch: x86_64 }
          - { os: ubuntu-24.04-arm, arch: aarch64 }
          - { os: macos-15, arch: x86_64 }
          - { os: macos-15, arch: arm64 }
          - { os: windows-2025, arch: AMD64 }
          - { os: windows-11-arm, arch: ARM64 }
    steps:
      - uses: actions/checkout@v5
      
      - name: Install Python
        uses: actions/setup-python@v6
        with:
          python-version: '3.11'
      
      - name: Build wheels + run tests
        uses: pypa/cibuildwheel@v3.2.1
        env:
          CIBW_ARCHS: "${{ matrix.arch }}"
      
      - name: Upload wheels
        uses: actions/upload-artifact@v4
        with:
          name: wheels-${{ matrix.os }}-${{ matrix.arch }}
          path: wheelhouse

  # 代码检查
  linters:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5
      - uses: actions/setup-python@v6
        with:
          python-version: 3.x
      - run: make ci-lint
```

### 3.2 BSD 平台测试 (`.github/workflows/bsd.yml`)

```yaml
# .github/workflows/bsd.yml

name: BSD
on: [push, pull_request]

jobs:
  freebsd:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Test on FreeBSD
        uses: vmactions/freebsd-vm@v1
        with:
          usesh: true
          run: |
            pkg install -y python3 py39-pip
            pip install pytest
            python3 setup.py build_ext -i
            python3 -m pytest tests/
  
  openbsd:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Test on OpenBSD
        uses: vmactions/openbsd-vm@v1
        with:
          usesh: true
          run: |
            pkg_add python3 py3-pip
            pip install pytest
            python3 setup.py build_ext -i
            python3 -m pytest tests/
```

### 3.3 测试覆盖率收集

```yaml
# 在 CI 中收集覆盖率
steps:
  - name: Run tests with coverage
    run: |
      pip install pytest-cov
      pytest --cov=psutil --cov-report=xml tests/
  
  - name: Upload coverage
    uses: codecov/codecov-action@v3
    with:
      files: coverage.xml
```

---

## 4. 关键模块测试用例示例

### 4.1 Process 类测试

```python
# tests/test_process.py

class TestProcess(PsutilTestCase):
    """Process 类核心测试。"""
    
    def test_pid(self):
        """测试 pid 属性。"""
        p = psutil.Process()
        assert p.pid == os.getpid()
        
        # pid 应该是只读的
        with pytest.raises(AttributeError):
            p.pid = 123
    
    def test_name(self):
        """测试 name() 方法。"""
        p = psutil.Process()
        name = p.name()
        
        assert isinstance(name, str)
        assert len(name) > 0
        # Python 进程名应该包含 'python'
        assert 'python' in name.lower()
    
    def test_exe(self):
        """测试 exe() 方法。"""
        p = psutil.Process()
        exe = p.exe()
        
        assert isinstance(exe, str)
        if exe:  # 某些情况可能返回空字符串
            assert os.path.exists(exe)
            assert os.path.isabs(exe)
    
    def test_cmdline(self):
        """测试 cmdline() 方法。"""
        p = psutil.Process()
        cmdline = p.cmdline()
        
        assert isinstance(cmdline, list)
        assert len(cmdline) > 0
        assert all(isinstance(arg, str) for arg in cmdline)
    
    def test_create_time(self):
        """测试 create_time() 方法。"""
        p = psutil.Process()
        ctime = p.create_time()
        
        assert isinstance(ctime, float)
        assert ctime > 0
        assert ctime <= time.time()
    
    def test_is_running(self):
        """测试 is_running() 方法。"""
        p = psutil.Process()
        assert p.is_running()
        
        # 创建并终止一个进程
        child = self.spawn_psproc()
        assert child.is_running()
        child.terminate()
        child.wait()
        assert not child.is_running()
    
    def test_parent(self):
        """测试 parent() 方法。"""
        p = psutil.Process()
        parent = p.parent()
        
        if parent is not None:
            assert isinstance(parent, psutil.Process)
            assert parent.pid != p.pid
    
    def test_children(self):
        """测试 children() 方法。"""
        p = psutil.Process()
        
        # 创建子进程
        child = self.spawn_psproc()
        time.sleep(0.1)
        
        children = p.children()
        assert child.pid in [c.pid for c in children]
    
    def test_cpu_times(self):
        """测试 cpu_times() 方法。"""
        p = psutil.Process()
        times = p.cpu_times()
        
        assert hasattr(times, 'user')
        assert hasattr(times, 'system')
        assert times.user >= 0
        assert times.system >= 0
    
    def test_memory_info(self):
        """测试 memory_info() 方法。"""
        p = psutil.Process()
        mem = p.memory_info()
        
        assert hasattr(mem, 'rss')
        assert hasattr(mem, 'vms')
        assert mem.rss > 0
        assert mem.vms > 0
    
    def test_oneshot(self):
        """测试 oneshot() 上下文管理器。"""
        p = psutil.Process()
        
        with p.oneshot():
            name = p.name()
            cpu = p.cpu_times()
            mem = p.memory_info()
        
        assert name
        assert cpu
        assert mem
    
    def test_as_dict(self):
        """测试 as_dict() 方法。"""
        p = psutil.Process()
        d = p.as_dict(attrs=['name', 'pid', 'cpu_times'])
        
        assert 'name' in d
        assert 'pid' in d
        assert 'cpu_times' in d
        assert d['pid'] == p.pid
```

### 4.2 系统函数测试

```python
# tests/test_system.py

class TestCPU(PsutilTestCase):
    """CPU 相关函数测试。"""
    
    def test_cpu_percent(self):
        """测试 cpu_percent()。"""
        # 阻塞模式
        percent = psutil.cpu_percent(interval=0.1)
        assert 0 <= percent <= 100
        
        # 非阻塞模式
        psutil.cpu_percent(interval=None)  # 初始化
        time.sleep(0.1)
        percent = psutil.cpu_percent(interval=None)
        assert 0 <= percent <= 100
    
    def test_cpu_percent_percpu(self):
        """测试按 CPU 的百分比。"""
        percents = psutil.cpu_percent(interval=0.1, percpu=True)
        
        assert isinstance(percents, list)
        assert len(percents) == psutil.cpu_count()
        assert all(0 <= p <= 100 for p in percents)


class TestMemory(PsutilTestCase):
    """内存相关函数测试。"""
    
    def test_virtual_memory_consistency(self):
        """测试虚拟内存数据一致性。"""
        mem = psutil.virtual_memory()
        
        # available <= total
        assert mem.available <= mem.total
        
        # used + free 应该接近 total
        # 注意：由于内核缓存等原因，可能不完全相等
        assert mem.used >= 0
        assert mem.free >= 0
    
    def test_swap_memory(self):
        """测试交换内存。"""
        swap = psutil.swap_memory()
        
        assert swap.total >= 0
        assert swap.used >= 0
        assert swap.free >= 0
        if swap.total > 0:
            assert 0 <= swap.percent <= 100
```

### 4.3 测试思路说明

```python
"""
测试设计原则：

1. 独立性
   - 每个测试应该独立运行
   - 使用 setUp/tearDown 清理状态
   - 不依赖测试执行顺序

2. 确定性
   - 避免时间相关的不稳定测试
   - 使用 retry_on_failure 处理竞态条件
   - 设置合理的容差值

3. 平台兼容
   - 使用 pytest.mark.skipif 跳过不支持的测试
   - 检测功能可用性 (HAS_*)
   - 平台特定测试放在单独文件

4. 权限处理
   - 使用 skip_on_access_denied 处理权限错误
   - 分离需要 root 权限的测试

5. 资源清理
   - 测试结束后终止所有子进程
   - 关闭打开的文件和套接字
   - 恢复修改的系统状态
"""
```

---

## 5. 运行测试

### 5.1 Makefile 命令

```makefile
# Makefile

test:  ## 运行所有测试（排除内存泄漏测试）
	$(PYTHON) -m pytest --ignore=tests/test_memleaks.py $(ARGS)

test-parallel:  ## 并行运行测试
	$(PYTHON) -m pytest --ignore=tests/test_memleaks.py -p xdist -n auto

test-coverage:  ## 运行测试并收集覆盖率
	$(PYTHON) -m pytest --cov=psutil --cov-report=html tests/

test-memleaks:  ## 运行内存泄漏测试
	$(PYTHON) -m pytest tests/test_memleaks.py -v
```

### 5.2 常用测试命令

```bash
# 运行所有测试
make test

# 运行特定测试文件
pytest tests/test_process.py

# 运行特定测试类
pytest tests/test_process.py::TestProcess

# 运行特定测试方法
pytest tests/test_process.py::TestProcess::test_name

# 并行运行
make test-parallel

# 带覆盖率
make test-coverage

# 详细输出
pytest -v tests/

# 失败时停止
pytest -x tests/

# 显示慢测试
pytest --durations=10 tests/
```

