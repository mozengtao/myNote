"""
智能家居设备控制 —— 完整可运行示例

本示例重点演示的心智模型原则（对应 complex_obj_organization.md 第十七节 Checklist）：
    1. Composition（Has-A 优先于 Is-A）—— SmartHome has Room has Device，
       各类设备（Light / Thermostat）共享同一个 Device 抽象，但不是靠一个塞满逻辑的巨型基类堆叠继承实现。
    2. Hide Structure               —— 通过 smart_home.turn_off_all("living_room") 操作，
       而不是外部直接写 home.rooms[0].devices[2].power = False。
    3. Single Responsibility        —— 每个 Device 子类只负责自己的开关/调节逻辑，不掺杂 Room/Home 的管理逻辑。

运行方式：
    python3 main.py
"""

from __future__ import annotations

from abc import ABC, abstractmethod


# ---------------------------------------------------------------------------
# 一、Device：每种设备只负责一种概念，而不是塞进一个 God Object
# ---------------------------------------------------------------------------

class Device(ABC):
    """设备的统一抽象：只约定"能开、能关、能描述自己"，具体行为交给子类。"""

    def __init__(self, name: str):
        self.name = name
        self._is_on = False

    @property
    def is_on(self) -> bool:
        return self._is_on

    def turn_on(self) -> None:
        self._is_on = True
        self._on_activated()

    def turn_off(self) -> None:
        self._is_on = False

    @abstractmethod
    def _on_activated(self) -> None:
        """设备被打开时，各自执行自己的逻辑（打印/上报状态等），不暴露给外部关心。"""

    @abstractmethod
    def status(self) -> str:
        """每个设备自己知道如何描述自己的状态，而不是让 Room/Home 去猜内部字段。"""


class Light(Device):
    """灯：只关心亮度，不知道 Room 是什么，也不知道 SmartHome 是什么。"""

    def __init__(self, name: str, brightness: int = 100):
        super().__init__(name)
        self.brightness = brightness

    def _on_activated(self) -> None:
        print(f"  [Light] {self.name} 已点亮，亮度 {self.brightness}%")

    def dim(self, brightness: int) -> None:
        if not self.is_on:
            raise ValueError(f"{self.name} 尚未开启，无法调节亮度")
        self.brightness = brightness

    def status(self) -> str:
        return f"{self.name}: {'开' if self.is_on else '关'}（亮度 {self.brightness}%）"


class Thermostat(Device):
    """温控器：只关心目标温度，逻辑与 Light 完全独立。"""

    def __init__(self, name: str, target_temp: float = 24.0):
        super().__init__(name)
        self.target_temp = target_temp

    def _on_activated(self) -> None:
        print(f"  [Thermostat] {self.name} 已启动，目标温度 {self.target_temp}°C")

    def set_target_temp(self, temp: float) -> None:
        self.target_temp = temp

    def status(self) -> str:
        return f"{self.name}: {'开' if self.is_on else '关'}（目标温度 {self.target_temp}°C）"


# ---------------------------------------------------------------------------
# 二、Room：Has-A Device 集合（组合），Room 本身不知道设备内部怎么实现
# ---------------------------------------------------------------------------

class Room:
    """一个房间拥有若干设备，房间只负责"管理这些设备"，不关心每种设备内部怎么工作。"""

    def __init__(self, name: str):
        self.name = name
        self.devices: list[Device] = []

    def add_device(self, device: Device) -> None:
        """对象自己管理自己的集合，而不是让外部到处 room.devices.append(...)。"""
        self.devices.append(device)

    def turn_off_all(self) -> None:
        for device in self.devices:
            device.turn_off()

    def find_device(self, name: str) -> Device:
        """Hide Structure：外部通过名字查找设备，而不用知道 devices 是个 list。"""
        for device in self.devices:
            if device.name == name:
                return device
        raise KeyError(f"房间 {self.name} 中找不到设备 {name}")

    def status_report(self) -> list[str]:
        return [device.status() for device in self.devices]


# ---------------------------------------------------------------------------
# 三、SmartHome：Has-A Room 集合，整栋房子的对象图入口
# ---------------------------------------------------------------------------

class SmartHome:
    """
    SmartHome -> Room -> Device 是一条清晰的组合链（Object Graph）。

    SmartHome 不知道每个设备具体怎么开灯、怎么调温度，
    它只知道"有哪些房间"，把具体动作转发给对应的 Room / Device。
    """

    def __init__(self, name: str):
        self.name = name
        self.rooms: dict[str, Room] = {}

    def add_room(self, room: Room) -> None:
        self.rooms[room.name] = room

    def get_room(self, name: str) -> Room:
        return self.rooms[name]

    def turn_off_all(self, room_name: str) -> None:
        """
        对外提供领域方法，隐藏内部结构：
        调用方写 smart_home.turn_off_all("living_room")，
        而不是 home.rooms["living_room"].devices[...].turn_off()。
        """
        self.get_room(room_name).turn_off_all()

    def whole_house_report(self) -> None:
        for room in self.rooms.values():
            print(f"[{room.name}]")
            for line in room.status_report():
                print(f"  - {line}")


# ---------------------------------------------------------------------------
# 四、演示
# ---------------------------------------------------------------------------

def main() -> None:
    home = SmartHome(name="我的家")

    living_room = Room(name="客厅")
    living_room.add_device(Light(name="客厅主灯"))
    living_room.add_device(Thermostat(name="客厅空调"))

    bedroom = Room(name="卧室")
    bedroom.add_device(Light(name="卧室台灯", brightness=40))

    home.add_room(living_room)
    home.add_room(bedroom)

    print("=== 打开客厅设备 ===")
    living_light = home.get_room("客厅").find_device("客厅主灯")
    living_light.turn_on()
    living_light.dim(60)

    living_ac = home.get_room("客厅").find_device("客厅空调")
    living_ac.turn_on()
    living_ac.set_target_temp(26)

    print("\n=== 全屋状态报告（打开客厅后）===")
    home.whole_house_report()

    # Hide Structure：一句话关掉整个客厅的设备，调用方不需要知道客厅里有几个设备
    print("\n=== 一键关闭客厅所有设备 ===")
    home.turn_off_all("客厅")

    print("\n=== 全屋状态报告（关闭客厅后）===")
    home.whole_house_report()


if __name__ == "__main__":
    main()
