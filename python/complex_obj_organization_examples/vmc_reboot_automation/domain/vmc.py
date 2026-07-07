"""
Domain 层：VMC

到这一层才第一次出现"VMC 是什么"这个业务概念。
VMC 只是一个纯粹的数据对象，不知道 SSH、不知道 channel、不知道 recv()，
只描述"一个 VMC 叫什么名字"。
"""

from dataclasses import dataclass


@dataclass(frozen=True)
class VMC:
    name: str
