"""
Workflow 层：RebootWorkflow

表达"整个重启流程"：创建 VMC 对象 -> 调用 Service -> 收集结果。
完全不知道 SSH、不知道 paramiko、不知道 recv()——那些都在更下面的层。

Workflow 只描述"业务步骤的顺序"，不描述"每一步具体怎么实现"。
"""

from __future__ import annotations

from domain.vmc import VMC
from domain.vmc_service import VMCService


class RebootWorkflow:
    def __init__(self, vmc_service: VMCService) -> None:
        self.service = vmc_service

    def execute(self, vmc_names: list[str]) -> list[tuple[VMC, bool]]:
        results: list[tuple[VMC, bool]] = []

        for name in vmc_names:
            vmc = VMC(name)
            ok = self.service.reboot(vmc)
            results.append((vmc, ok))

        return results
