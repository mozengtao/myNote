"""
Domain Service 层：VMCService

表达"VMC 能做什么"（业务动作），而不是"SSH 怎么发命令"（技术细节）。

VMCService 只依赖 InteractiveShell 暴露的 send / recv_until / drain 三个方法，
完全不知道 channel、socket、paramiko 的存在——业务代码非常干净。
"""

from __future__ import annotations

from .vmc import VMC


class VMCService:
    CONFIRM_PROMPT = "Are you sure?"

    def __init__(self, shell) -> None:
        self.shell = shell

    def reboot(self, vmc: VMC) -> bool:
        print("=" * 60)
        print(f"Reboot {vmc.name}")
        print("=" * 60)

        try:
            self.shell.send(
                f"vmc {vmc.name} reboot keep-current-version false"
            )
            self.shell.recv_until(self.CONFIRM_PROMPT)

            self.shell.send("yes")
            self.shell.drain()
            return True
        except TimeoutError as exc:
            print(f"[WARN] {vmc.name} 重启确认超时：{exc}")
            return False
