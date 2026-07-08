"""
API 层：对外暴露的最外层入口（这里用一个简单的 CLI 风格函数代表 HTTP/CLI/gRPC 皆可）。

run_upgrade_command 只依赖 DeviceUpgradeWorkflow 这一个抽象概念上的"用例"，
不知道 workflow 内部是怎么组装 onboarding/inspection/infrastructure 的。
"""

from __future__ import annotations

from workflows.device_upgrade_workflow import DeviceUpgradeWorkflow


def run_upgrade_command(workflow: DeviceUpgradeWorkflow, device_id: str, host: str) -> None:
    result = workflow.run(device_id, host)
    print(f"[CLI] final state for {device_id}: {result}")
