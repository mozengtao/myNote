"""
Application 层 / High-level Module：设备入网用例编排。

DeviceOnboardingService 的构造函数就是这个用例需要哪些抽象协作者的完整清单——
ConfigLoader、DeviceRepository、Notifier、Logger 全部是 domain/ 里定义的 Protocol，
本文件从头到尾没有 import 任何 infrastructure/ 下的具体类。
"""

from __future__ import annotations

from domain.config_loader import ConfigLoader
from domain.device_repository import DeviceRepository
from domain.logger import Logger
from domain.models import Device
from domain.notifier import Notifier


class DeviceOnboardingService:
    def __init__(
        self,
        config_loader: ConfigLoader,
        repo: DeviceRepository,
        notifier: Notifier,
        logger: Logger,
    ) -> None:
        self._config_loader = config_loader
        self._repo = repo
        self._notifier = notifier
        self._logger = logger

    def onboard(self, device_id: str, host: str) -> Device:
        config = self._config_loader.load(device_id)
        device = Device(
            device_id=device_id,
            host=host,
            model=config.get("model", "unknown"),
            firmware=config.get("firmware", "unknown"),
        )
        device.mark_onboarded()
        self._repo.save(device)
        self._logger.info(f"onboarded {device_id} ({device.model}, fw={device.firmware})")
        self._notifier.notify(f"Device {device_id} onboarded successfully")
        return device
