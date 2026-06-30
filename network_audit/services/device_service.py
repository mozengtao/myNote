"""服务层：设备领域能力。

Service 开始有业务含义：它知道"采集版本"需要执行 `show version`，
"采集接口"需要执行 `show ip interface brief`。

它向下使用 SSHClient（基础设施），向上为 Workflow 提供领域方法。
注意：Service 返回的是原始 CLI 文本——文本到对象的转换是 Parser 的职责，
保持单一职责，便于各自独立测试。
"""

from __future__ import annotations

from config.loader import Config
from infra.ssh import SSHClient
from utils.logger import get_logger

CMD_VERSION = "show version"
CMD_INTERFACES = "show ip interface brief"


class DeviceService:
    """提供"采集设备信息"这一领域能力。"""

    def __init__(self, config: Config):
        self.config = config
        self.log = get_logger("services.device")

    def collect_version(self, host: str) -> str:
        """返回某台主机 `show version` 的原始输出。"""
        with self._client(host) as client:
            return client.execute(CMD_VERSION)

    def collect_interfaces(self, host: str) -> str:
        """返回某台主机接口表的原始输出。"""
        with self._client(host) as client:
            return client.execute(CMD_INTERFACES)

    def _client(self, host: str) -> SSHClient:
        return SSHClient(
            host=host,
            username=self.config.username,
            timeout=self.config.timeout,
        )
