"""
Detail：结构化满足 domain.device_connector.DeviceConnector，走 SSH 协议。

这里刻意不显式继承 DeviceConnector（它是一个 typing.Protocol），只要方法签名
匹配即可"隐式"满足契约——domain 层从未、也不需要 import 这个类，依赖方向只能是
"细节单向依赖抽象"，而不是抽象依赖细节。
"""

from __future__ import annotations


class SshConnector:
    def __init__(self, username: str, timeout: int = 10) -> None:
        self._username = username
        self._timeout = timeout

    def fetch_state(self, host: str) -> dict:
        print(f"[SSH user={self._username} timeout={self._timeout}] connecting to {host} ...")
        print("[SSH] running 'show interface status'")
        return {"protocol": "ssh", "host": host, "interfaces_up": 24}
