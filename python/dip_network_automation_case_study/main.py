"""
Composition Root（组合根）：整个项目里唯一允许同时认识所有具体实现（Detail）的地方。

Object Flow（对象创建 -> 注入 -> 调用）：

    main()
      -> create SqliteDeviceRepository / FileConfigLoader / EmailNotifier / ConsoleLogger
      -> create SshConnector / NetconfConnector
      -> inject into DeviceOnboardingService / DeviceInspectionService
      -> inject into DeviceUpgradeWorkflow
      -> api.cli.run_upgrade_command(workflow, device_id, host)
      -> workflow.run() -> onboarding.onboard() -> inspection.inspect()
      -> infrastructure 层真正执行 SSH/NETCONF/SQLite/Email/Console

运行方式：
    cd python/dip_network_automation_case_study
    python3 main.py

想把 SSH 换成 NETCONF、把 SQLite 换成内存仓储，只需要修改本文件里"选择具体实现"的
那几行——domain/、application/、workflows/、api/ 目录下的代码完全不需要改动。
"""

from __future__ import annotations

from pathlib import Path

from application.device_inspection_service import DeviceInspectionService
from application.device_onboarding_service import DeviceOnboardingService
from infrastructure.console_logger import ConsoleLogger
from infrastructure.email_notifier import EmailNotifier
from infrastructure.file_config_loader import FileConfigLoader
from infrastructure.netconf_connector import NetconfConnector
from infrastructure.sqlite_repository import SqliteDeviceRepository
from infrastructure.ssh_connector import SshConnector
from workflows.device_upgrade_workflow import DeviceUpgradeWorkflow
from api.cli import run_upgrade_command

CONFIG_DIR = Path(__file__).parent / "configs"


def build_workflow(connector) -> DeviceUpgradeWorkflow:
    """本函数就是"组装对象图"这件事的具体代码——只有这里知道所有 Detail 的存在。"""
    repo = SqliteDeviceRepository()  # 换成 InMemoryDeviceRepository() 即可切换存储后端
    logger = ConsoleLogger()
    config_loader = FileConfigLoader(str(CONFIG_DIR))
    notifier = EmailNotifier(smtp_host="smtp.corp.com", to_address="noc@corp.com")

    onboarding = DeviceOnboardingService(config_loader, repo, notifier, logger)
    inspection = DeviceInspectionService(connector, repo, logger)
    return DeviceUpgradeWorkflow(onboarding, inspection, logger)


def main() -> None:
    print("\n----- 设备 1：走 SSH 协议 -----")
    ssh_workflow = build_workflow(SshConnector(username="admin"))
    run_upgrade_command(ssh_workflow, device_id="RPD-01", host="10.0.0.1")

    print("\n----- 设备 2：走 NETCONF 协议（连接器替换，其余代码零改动） -----")
    netconf_workflow = build_workflow(NetconfConnector())
    run_upgrade_command(netconf_workflow, device_id="RPD-02", host="10.0.0.2")


if __name__ == "__main__":
    main()
