"""Detail：结构化满足 ConfigLoader，从本地 JSON 文件读取设备出厂配置。"""

from __future__ import annotations

import json
from pathlib import Path


class FileConfigLoader:
    def __init__(self, base_dir: str) -> None:
        self._base_dir = Path(base_dir)

    def load(self, device_id: str) -> dict:
        path = self._base_dir / f"{device_id}.json"
        if not path.exists():
            print(f"[FileConfigLoader] no config file for {device_id}, using defaults")
            return {}
        with path.open("r", encoding="utf-8") as fh:
            return json.load(fh)
