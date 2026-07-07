"""
Infrastructure 层：InteractiveShell

只负责"人机交互式 CLI 会话"这一层通用能力：
    send()       —— 发一条命令
    recv_until() —— 一直读到出现某个 marker（比如 "Are you sure?" 提示符）
    drain()      —— 把 channel 里剩下的输出都读干净

完全不知道 VMC、不知道 reboot 是什么命令——那是 Domain / Service 层的事。
它只依赖 channel 对象的 send / recv_ready / recv 三个方法，
不关心 channel 到底是真实的 paramiko.Channel 还是 FakeVMCChannel。
"""

from __future__ import annotations

import time


class InteractiveShell:
    def __init__(self, channel) -> None:
        self.channel = channel

    def send(self, cmd: str) -> None:
        self.channel.send(cmd + "\n")

    def recv_until(self, marker: str, timeout: float = 30) -> str:
        buf = ""
        deadline = time.time() + timeout

        while time.time() < deadline:
            if self.channel.recv_ready():
                chunk = self.channel.recv(4096).decode()
                print(chunk, end="")
                buf += chunk
                if marker in buf:
                    return buf
            time.sleep(0.1)

        raise TimeoutError(f"等待 '{marker}' 超时（{timeout}s）")

    def drain(self, quiet_seconds: float = 0.3) -> None:
        time.sleep(quiet_seconds)
        while self.channel.recv_ready():
            print(self.channel.recv(4096).decode(), end="")
