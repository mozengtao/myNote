"""
Workflow 层：编排多个 Application 服务，组成"设备入网并巡检"这一条完整业务流水线。

DeviceUpgradeWorkflow 只依赖两个 Application Service 和 Logger 三个抽象协作者，
不知道它们内部各自又依赖了哪些更底层的 infrastructure 实现——这是分层架构里
"每一层只认识自己下面那一层暴露出来的抽象"的直接体现。
"""

from __future__ import annotations

from application.device_inspection_service import DeviceInspectionService
from application.device_onboarding_service import DeviceOnboardingService
from domain.logger import Logger


class DeviceUpgradeWorkflow:
    def __init__(
        self,
        onboarding: DeviceOnboardingService,
        inspection: DeviceInspectionService,
        logger: Logger,
    ) -> None:
        self._onboarding = onboarding
        self._inspection = inspection
        self._logger = logger

    def run(self, device_id: str, host: str) -> dict:
        self._logger.info(f"=== starting workflow for {device_id} ===")
        self._onboarding.onboard(device_id, host)
        state = self._inspection.inspect(device_id)
        self._logger.info(f"=== workflow completed for {device_id} ===")
        return state
