"""
Abstraction (Port)：设备连接契约。

application/ 层只依赖这一个 Protocol，完全不知道底层走的是 SSH、NETCONF 还是 gNMI。
这是本项目里"依赖倒置"发生的第一个地方：Low-level 的 infrastructure/*_connector.py
必须满足这个契约，而不是反过来由这个契约去迁就某一种具体协议的形状。
"""

from __future__ import annotations

from typing import Protocol


class DeviceConnector(Protocol):
    def fetch_state(self, host: str) -> dict: ...
