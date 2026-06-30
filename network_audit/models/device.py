"""设备相关的数据模型。

整个程序传递的是 Device 对象，而不是 dict：
    device.hostname / device.version / device.interfaces
而不是：
    data["hostname"] / data["version"]
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class Interface:
    """单个网络接口。"""

    name: str
    status: str  # up / down

    @property
    def is_up(self) -> bool:
        return self.status.lower() == "up"


@dataclass
class Device:
    """一台被审计的网络设备。

    它是 Parser 的产物，也是 Analyzer / Reporter 的输入。
    """

    hostname: str
    version: str = ""
    interfaces: list[Interface] = field(default_factory=list)

    @property
    def up_count(self) -> int:
        return sum(1 for i in self.interfaces if i.is_up)

    @property
    def down_count(self) -> int:
        return sum(1 for i in self.interfaces if not i.is_up)
