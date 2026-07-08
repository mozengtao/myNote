"""
Detail：结构化满足 domain.device_repository.DeviceRepository，用真实 SQLite 持久化
（这是 Part 5/8 里反复提到的"Database"角色的具体落地）。
"""

from __future__ import annotations

import sqlite3

from domain.models import Device


class SqliteDeviceRepository:
    def __init__(self, path: str = ":memory:") -> None:
        self._conn = sqlite3.connect(path)
        self._conn.execute(
            """
            CREATE TABLE IF NOT EXISTS devices (
                device_id TEXT PRIMARY KEY,
                host TEXT,
                model TEXT,
                firmware TEXT,
                status TEXT
            )
            """
        )
        self._conn.commit()

    def save(self, device: Device) -> None:
        self._conn.execute(
            """
            INSERT OR REPLACE INTO devices (device_id, host, model, firmware, status)
            VALUES (?, ?, ?, ?, ?)
            """,
            (device.device_id, device.host, device.model, device.firmware, device.status),
        )
        self._conn.commit()

    def get(self, device_id: str) -> Device | None:
        row = self._conn.execute(
            "SELECT device_id, host, model, firmware, status FROM devices WHERE device_id = ?",
            (device_id,),
        ).fetchone()
        return Device(*row) if row else None

    def list_all(self) -> list[Device]:
        rows = self._conn.execute(
            "SELECT device_id, host, model, firmware, status FROM devices"
        ).fetchall()
        return [Device(*row) for row in rows]
