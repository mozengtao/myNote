"""
Infrastructure 层：SSHConnection（生产环境版本）

这一层只知道"如何建立一个 SSH 连接、如何打开一个交互式 shell"。
完全不知道 VMC 是什么、要发送什么命令 —— 那些是 Domain / Workflow 层的事。

注意：
    本文件依赖第三方库 paramiko（pip install paramiko），且需要连接真实设备才能工作。
    为了让本示例无需真实 VMC 设备、无需安装 paramiko 也能直接运行，
    main.py 实际使用的是 fake_ssh_client.py 里的 FakeSSHConnection。

    FakeSSHConnection 与本文件的 SSHConnection 对外接口完全一致
    （connect / open_shell / close），所以生产环境把 main.py 里的
    import 换成本文件的 SSHConnection 即可，domain/ 和 workflow/
    目录下的代码不需要做任何修改。
"""

from __future__ import annotations

import paramiko


class SSHConnection:
    """只负责 SSH 连接本身，不知道任何业务命令。"""

    def __init__(self, host: str, user: str, password: str):
        self.host = host
        self.user = user
        self.password = password
        self.client: paramiko.SSHClient | None = None

    def connect(self) -> None:
        self.client = paramiko.SSHClient()

        self.client.set_missing_host_key_policy(
            paramiko.AutoAddPolicy()
        )

        self.client.connect(
            hostname=self.host,
            username=self.user,
            password=self.password,
            allow_agent=False,
            look_for_keys=False,
        )

    def open_shell(self):
        return self.client.invoke_shell(width=220)

    def close(self) -> None:
        if self.client is not None:
            self.client.close()
