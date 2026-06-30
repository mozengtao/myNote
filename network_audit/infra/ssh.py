"""基础设施层：SSH 客户端。

这一层只负责"发命令、回文本"，它不知道什么是 Device / Report / Workflow。

真实项目里这里会用 paramiko / netmiko 建立连接：
    self._conn = paramiko.SSHClient(); self._conn.connect(...)
    stdin, stdout, stderr = self._conn.exec_command(command)
    return stdout.read().decode()

本 demo 用"打印命令 + 返回内置假 CLI 文本"代替真实连接，
这样既能演示分层结构，又无需任何真实设备即可运行。
"""

from __future__ import annotations

from utils.logger import get_logger

# 内置假 CLI 输出：模拟不同设备对不同命令的响应。
# 真实场景中这些文本来自设备，本 demo 直接写死以替代 SSH。
_FAKE_OUTPUT: dict[str, dict[str, str]] = {
    "sw1": {
        "show version": "Cisco IOS Software, Version 15.2(4)E7",
        "show ip interface brief": (
            "Interface              IP-Address      Status     Protocol\n"
            "GigabitEthernet0/0     10.0.0.1        up         up\n"
            "GigabitEthernet0/1     10.0.0.5        up         up\n"
            "GigabitEthernet0/2     unassigned      down       down\n"
        ),
    },
    "sw2": {
        "show version": "Cisco IOS Software, Version 15.2(4)E7",
        "show ip interface brief": (
            "Interface              IP-Address      Status     Protocol\n"
            "GigabitEthernet0/0     10.0.1.1        up         up\n"
            "GigabitEthernet0/1     unassigned      down       down\n"
        ),
    },
    "sw3": {
        "show version": "Cisco IOS Software, Version 16.9(3)",
        "show ip interface brief": (
            "Interface              IP-Address      Status     Protocol\n"
            "GigabitEthernet0/0     10.0.2.1        up         up\n"
            "GigabitEthernet0/1     10.0.2.5        up         up\n"
            "GigabitEthernet0/2     10.0.2.9        up         up\n"
        ),
    },
}


class SSHClient:
    """模拟的 SSH 客户端。每个实例对应一台主机的一次会话。"""

    def __init__(self, host: str, username: str = "admin", timeout: int = 10):
        self.host = host
        self.username = username
        self.timeout = timeout
        self.log = get_logger("infra.ssh")

    def __enter__(self) -> "SSHClient":
        self.connect()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def connect(self) -> None:
        # 真实实现：建立 TCP/SSH 连接、认证。这里用打印代替。
        self.log.info("[SSH] connect %s@%s (timeout=%ss)", self.username, self.host, self.timeout)

    def execute(self, command: str) -> str:
        """发送一条命令，返回原始文本输出。"""
        self.log.info("[SSH %s] $ %s", self.host, command)
        host_table = _FAKE_OUTPUT.get(self.host, {})
        if command not in host_table:
            # 模拟设备返回未知命令错误，让上层能感知失败。
            return f"% Unknown command or unreachable host: {command}"
        return host_table[command]

    def close(self) -> None:
        self.log.info("[SSH] close %s", self.host)
