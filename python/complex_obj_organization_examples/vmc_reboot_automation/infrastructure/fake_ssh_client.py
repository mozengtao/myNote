"""
Infrastructure 层的"假实现"：FakeSSHConnection + FakeVMCChannel

目的：让本示例不需要真实 VMC 设备、也不需要安装 paramiko 就能完整运行，
同时演示分层架构真正的好处——Domain / Service / Workflow 层完全不知道
自己面对的是真实 SSH 还是这个本地模拟实现，因为它们依赖的只是"接口"
（connect / open_shell / close，以及 channel 的 send / recv_ready / recv），
而不是"paramiko 这个具体类"。

这就是本示例要额外演示的一条心智模型原则：

    Infrastructure 层可以被整体替换（真实 SSH <-> 本地模拟），
    只要接口不变，Domain / Service / Workflow 层代码一行都不用改。

生产环境把 main.py 里的 FakeSSHConnection 换成 ssh_client.py 中的
真实 SSHConnection 即可。
"""

from __future__ import annotations

from collections import deque


class FakeVMCChannel:
    """
    模拟一个 VMC CLI 的交互式会话。

    只实现 InteractiveShell 真正用到的三个方法：
        send(cmd)
        recv_ready()
        recv(buffer_size)
    这三者构成了 InteractiveShell 依赖的"channel 协议"（鸭子类型）。
    真实的 paramiko.Channel 和这里的 FakeVMCChannel 都满足这个协议，
    InteractiveShell 完全不关心具体是谁实现的。
    """

    PROMPT = "Are you sure?"

    def __init__(self) -> None:
        self._outbox: deque[str] = deque()
        self._awaiting_confirmation: str | None = None
        self._queue("vmc> ")

    def send(self, data: str) -> None:
        line = data.strip()

        if line.startswith("vmc ") and "reboot" in line:
            name = line.split()[1]
            self._awaiting_confirmation = name
            self._queue(f"{line}\n")
            self._queue(f"{self.PROMPT} [yes/no]: ")
            return

        if line == "yes" and self._awaiting_confirmation:
            name = self._awaiting_confirmation
            self._awaiting_confirmation = None
            self._queue("yes\n")
            self._queue(f"Rebooting {name} ...\n")
            self._queue(f"{name}: reboot request accepted.\n")
            self._queue("vmc> ")
            return

    def recv_ready(self) -> bool:
        return len(self._outbox) > 0

    def recv(self, buffer_size: int = 4096) -> bytes:
        chunk = self._outbox.popleft() if self._outbox else ""
        return chunk.encode()

    def _queue(self, text: str) -> None:
        self._outbox.append(text)


class FakeSSHConnection:
    """与 SSHConnection 接口完全一致的假实现，供本示例演示用。"""

    def __init__(self, host: str, user: str, password: str):
        self.host = host
        self.user = user
        self.password = password

    def connect(self) -> None:
        print(f"[fake-ssh] 模拟连接 {self.host}（用户：{self.user}），未发起真实网络连接")

    def open_shell(self):
        return FakeVMCChannel()

    def close(self) -> None:
        print(f"[fake-ssh] 已关闭到 {self.host} 的模拟连接")
