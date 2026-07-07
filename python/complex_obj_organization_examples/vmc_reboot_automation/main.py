"""
Application 层：程序入口（对应文档里的 app.py）

本层只负责"组装对象"，不包含任何业务逻辑：

    FakeSSHConnection -> InteractiveShell -> VMCService -> RebootWorkflow

对应 complex_obj_organization.md 第二十节的对象流（Object Flow）：

    main()
      -> SSHConnection.connect()
      -> InteractiveShell
      -> RebootWorkflow
      -> create VMC(name)
      -> VMCService.reboot(vmc)
      -> shell.send(command_string)
      -> channel.send() / channel.recv()
      -> InteractiveShell.recv_until()
      -> VMCService（业务决策：是否重试、是否算失败）

生产环境只需要把下面这一行：
    from infrastructure.fake_ssh_client import FakeSSHConnection
换成：
    from infrastructure.ssh_client import SSHConnection
domain/ 和 workflow/ 目录下的代码完全不用修改——这正是分层架构的核心价值。

运行方式：
    python3 main.py
"""

from infrastructure.fake_ssh_client import FakeSSHConnection
from infrastructure.interactive_shell import InteractiveShell

from domain.vmc_service import VMCService

from workflow.reboot_workflow import RebootWorkflow

HOST = "192.168.244.43"
USER = "admin"
PASSWORD = "admin"

VMC_NAMES = [
    "astatine0",
    "barium0",
    "bohrium",
]


def main() -> None:
    conn = FakeSSHConnection(HOST, USER, PASSWORD)
    conn.connect()

    shell = InteractiveShell(conn.open_shell())
    shell.drain()

    service = VMCService(shell)
    workflow = RebootWorkflow(service)

    results = workflow.execute(VMC_NAMES)

    conn.close()

    print("\n=== 重启结果汇总 ===")
    for vmc, ok in results:
        print(f"- {vmc.name}: {'成功' if ok else '失败'}")

    print(
        "\n提示：本示例用 infrastructure/fake_ssh_client.py 模拟 SSH 交互，"
        "无需真实 VMC 设备、无需安装 paramiko 即可运行。\n"
        "生产环境换成 infrastructure/ssh_client.py 中真实的 SSHConnection，"
        "domain/ 和 workflow/ 目录下的代码完全不用改——这正是分层架构的价值所在。"
    )


if __name__ == "__main__":
    main()
