"""Detail：结构化满足 DeviceConnector，走 NETCONF 协议。"""

from __future__ import annotations


class NetconfConnector:
    def __init__(self, port: int = 830) -> None:
        self._port = port

    def fetch_state(self, host: str) -> dict:
        print(f"[NETCONF] ncclient.manager.connect({host}:{self._port})")
        print("[NETCONF] <get><interfaces-state/></get>")
        return {"protocol": "netconf", "host": host, "interfaces_up": 24}
