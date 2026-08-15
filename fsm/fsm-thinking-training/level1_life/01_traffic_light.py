"""Lesson 01: 交通灯 (Level 1 - 生活场景).

对应 01_traffic_light.md。本文件包含同一个"固定周期交通灯"问题的四版实现:

    v1: 最简单版本 —— Enum + match-case 硬编码转移逻辑
    v2: Transition Table 版 —— 数据驱动的转移表
    v3: State Pattern 版 —— 每个状态是一个类，自己处理事件
    v4: 通用 FSM 框架片段 —— 可被其他案例复用的 State/Event/Transition/FSM 抽象

四版都实现同一套业务规则: RED -> GREEN -> YELLOW -> RED 循环，
仅由 TIMER_EXPIRED 事件驱动，非法/未知输入被忽略并保持当前状态。
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable


# ============ v1: 简单版 (Enum + match-case) ============
#
# 设计思路: 用 Enum 表示状态，转移规则和执行动作全部写在一个方法里，
# 用 match 语句根据"当前状态"分支。这是最直觉的写法，优点是一眼能看到
# "发生了什么就做什么"；缺点是转移规则和动作代码耦合在一起，状态和事件
# 一多，match 分支会迅速膨胀，也无法脱离代码单独查看"全部允许的转移"。


class ColorV1(Enum):
    RED = auto()
    GREEN = auto()
    YELLOW = auto()


class TrafficLightV1:
    """最简单版本: 用 match-case 硬编码状态转移。"""

    # 每种颜色对应的持续时长（秒），仅用于演示打印，不做真实计时。
    DURATIONS = {ColorV1.RED: 30, ColorV1.GREEN: 25, ColorV1.YELLOW: 3}

    def __init__(self) -> None:
        self.color: ColorV1 = ColorV1.RED

    def on_timer_expired(self) -> None:
        """处理"当前颜色计时结束"事件，硬编码在 match 分支里。"""
        match self.color:
            case ColorV1.RED:
                self.color = ColorV1.GREEN
            case ColorV1.GREEN:
                self.color = ColorV1.YELLOW
            case ColorV1.YELLOW:
                self.color = ColorV1.RED
            case _:
                # 理论上不会到达，Enum 已穷举所有取值。
                raise AssertionError(f"未知颜色: {self.color}")

    def on_pedestrian_button(self) -> None:
        """行人按钮在本课基础模型中被忽略，不改变状态。"""
        # 显式写出"忽略"这个决定，而不是什么都不写，
        # 这样阅读代码的人知道这是"故意忽略"而不是"漏写了处理逻辑"。
        pass

    def __repr__(self) -> str:
        return f"TrafficLightV1(color={self.color.name})"


def demo_v1() -> None:
    print("\n--- v1: 简单版 (match-case 硬编码) ---")
    light = TrafficLightV1()
    print(f"初始状态: {light.color.name}")
    for _ in range(4):
        light.on_timer_expired()
        print(f"计时结束 -> {light.color.name}")
    light.on_pedestrian_button()
    print(f"行人按钮(应被忽略) -> {light.color.name}")


# ============ v2: Transition Table 版 (数据驱动) ============
#
# 设计思路: 把"允许的转移"从代码逻辑里抽出来，变成一个字典:
#   (当前状态, 事件) -> (下一状态, 动作函数)
# 相比 v1 的改进:
#   1. 转移规则变成了数据，可以被打印、被单独测试，不需要改代码逻辑本身;
#   2. 新增一条转移，只需要往字典里加一行，不需要改动分支结构;
#   3. "非法输入"统一处理为"字典查不到对应的 key"，不用在每个分支末尾
#      重复写一遍 else 忽略逻辑。


class ColorV2(Enum):
    RED = auto()
    GREEN = auto()
    YELLOW = auto()


class EventV2(Enum):
    TIMER_EXPIRED = auto()


class TrafficLightV2:
    """Transition Table 版: 用字典描述全部合法转移。"""

    def __init__(self) -> None:
        self.color: ColorV2 = ColorV2.RED
        # 转移表: (当前状态, 事件) -> (下一状态, 进入新状态时执行的动作)
        self._table: dict[
            tuple[ColorV2, EventV2], tuple[ColorV2, Callable[[], None]]
        ] = {
            (ColorV2.RED, EventV2.TIMER_EXPIRED): (
                ColorV2.GREEN,
                lambda: print("  action: 关红灯，点亮绿灯，启动绿灯计时器"),
            ),
            (ColorV2.GREEN, EventV2.TIMER_EXPIRED): (
                ColorV2.YELLOW,
                lambda: print("  action: 关绿灯，点亮黄灯，启动黄灯计时器"),
            ),
            (ColorV2.YELLOW, EventV2.TIMER_EXPIRED): (
                ColorV2.RED,
                lambda: print("  action: 关黄灯，点亮红灯，启动红灯计时器"),
            ),
        }

    def handle(self, event: EventV2) -> None:
        """统一入口: 查表，找不到就忽略并保持当前状态。"""
        key = (self.color, event)
        if key not in self._table:
            print(f"  忽略未定义转移: state={self.color.name}, event={event.name}")
            return
        next_state, action = self._table[key]
        action()
        self.color = next_state

    def __repr__(self) -> str:
        return f"TrafficLightV2(color={self.color.name})"


def demo_v2() -> None:
    print("\n--- v2: Transition Table 版 (数据驱动) ---")
    light = TrafficLightV2()
    print(f"初始状态: {light.color.name}")
    for _ in range(4):
        light.handle(EventV2.TIMER_EXPIRED)
        print(f"-> {light.color.name}")


# ============ v3: State Pattern 版 (每个状态是一个类) ============
#
# 设计思路: 把每个状态实现成一个类，自己知道"收到某事件该转到哪个状态"。
# 适用场景: 每个状态的行为差异很大、每个状态内部逻辑复杂(比如某状态要
# 额外读取传感器、做异常处理)时，State Pattern 能让每个状态类只关心
# 自己的逻辑，新增/修改一个状态不会牵动别的状态类。
# 对本课这种"三个状态逻辑几乎相同、只是切换目标不同"的简单场景来说，
# 这么做略显"过度设计"，但值得实现一遍用来对比感受。


class TrafficLightStateV3:
    """状态基类，定义每个具体状态必须实现的接口。"""

    name: str = "AbstractState"

    def on_timer_expired(self, context: "TrafficLightContextV3") -> None:
        raise NotImplementedError

    def on_pedestrian_button(self, context: "TrafficLightContextV3") -> None:
        # 默认忽略；具体状态如需特殊处理可以覆写。
        print(f"  [{self.name}] 忽略行人按钮")


class RedStateV3(TrafficLightStateV3):
    name = "RED"

    def on_timer_expired(self, context: "TrafficLightContextV3") -> None:
        print("  action: 关红灯，点亮绿灯")
        context.transition_to(GreenStateV3())


class GreenStateV3(TrafficLightStateV3):
    name = "GREEN"

    def on_timer_expired(self, context: "TrafficLightContextV3") -> None:
        print("  action: 关绿灯，点亮黄灯")
        context.transition_to(YellowStateV3())


class YellowStateV3(TrafficLightStateV3):
    name = "YELLOW"

    def on_timer_expired(self, context: "TrafficLightContextV3") -> None:
        print("  action: 关黄灯，点亮红灯")
        context.transition_to(RedStateV3())


class TrafficLightContextV3:
    """持有"当前状态对象"的上下文，把事件转发给当前状态对象处理。"""

    def __init__(self) -> None:
        self._state: TrafficLightStateV3 = RedStateV3()

    def transition_to(self, new_state: TrafficLightStateV3) -> None:
        self._state = new_state

    def on_timer_expired(self) -> None:
        self._state.on_timer_expired(self)

    def on_pedestrian_button(self) -> None:
        self._state.on_pedestrian_button(self)

    @property
    def color_name(self) -> str:
        return self._state.name

    def __repr__(self) -> str:
        return f"TrafficLightContextV3(color={self.color_name})"


def demo_v3() -> None:
    print("\n--- v3: State Pattern 版 (每个状态是一个类) ---")
    light = TrafficLightContextV3()
    print(f"初始状态: {light.color_name}")
    for _ in range(4):
        light.on_timer_expired()
        print(f"-> {light.color_name}")
    light.on_pedestrian_button()


# ============ v4: 通用 FSM 框架片段 (可复用) ============
#
# 设计思路: 抽象出 State / Event / Transition / FSM 四个基础构件。
# 任何案例只要定义好自己的状态、事件、转移表，就可以直接复用这个引擎，
# 不需要重复实现"根据 (state, event) 查表 -> 执行动作 -> 切换状态"这套样板逻辑。
# 什么时候值得引入框架，而不是每次手写(如 v1/v2)?
#   - 当你发现自己在第二、第三个案例里又要重写一遍同样的"查表+切换"逻辑;
#   - 当转移表需要被外部工具消化(打印状态图、自动生成测试用例、做转移覆盖率统计);
#   - 当团队里有多个人协作维护多个 FSM，需要统一的调试/日志/校验方式。
# 如果只有一次性、几个状态的简单场景，直接用 v1 或 v2 通常更划算。


class State(Enum):
    """占位基类语义: 具体案例应定义自己的 State Enum。"""


class Event(Enum):
    """占位基类语义: 具体案例应定义自己的 Event Enum。"""


@dataclass(frozen=True)
class Transition:
    """一条转移规则: 在 from_state 状态下收到 event，应转移到 to_state。"""

    from_state: Enum
    event: Enum
    to_state: Enum
    action: Callable[[], None] | None = None


@dataclass
class FSM:
    """通用有限状态机引擎，与具体业务领域无关。"""

    initial_state: Enum
    transitions: list[Transition]
    on_illegal: Callable[[Enum, Enum], None] | None = None
    _current: Enum = field(init=False)
    _table: dict[tuple[Enum, Enum], Transition] = field(init=False)

    def __post_init__(self) -> None:
        self._current = self.initial_state
        self._table = {(t.from_state, t.event): t for t in self.transitions}

    @property
    def current_state(self) -> Enum:
        return self._current

    def fire(self, event: Enum) -> None:
        """触发一个事件；查不到对应转移时调用 on_illegal 回调并保持原状态。"""
        key = (self._current, event)
        transition = self._table.get(key)
        if transition is None:
            if self.on_illegal is not None:
                self.on_illegal(self._current, event)
            return
        if transition.action is not None:
            transition.action()
        self._current = transition.to_state


class TrafficColor(Enum):
    RED = auto()
    GREEN = auto()
    YELLOW = auto()


class TrafficEvent(Enum):
    TIMER_EXPIRED = auto()


def build_traffic_light_fsm() -> FSM:
    """用通用 FSM 引擎组装出交通灯这一个具体案例。"""

    def log_illegal(state: Enum, event: Enum) -> None:
        print(f"  忽略未定义转移: state={state.name}, event={event.name}")

    transitions = [
        Transition(
            TrafficColor.RED,
            TrafficEvent.TIMER_EXPIRED,
            TrafficColor.GREEN,
            action=lambda: print("  action: 关红灯，点亮绿灯"),
        ),
        Transition(
            TrafficColor.GREEN,
            TrafficEvent.TIMER_EXPIRED,
            TrafficColor.YELLOW,
            action=lambda: print("  action: 关绿灯，点亮黄灯"),
        ),
        Transition(
            TrafficColor.YELLOW,
            TrafficEvent.TIMER_EXPIRED,
            TrafficColor.RED,
            action=lambda: print("  action: 关黄灯，点亮红灯"),
        ),
    ]
    return FSM(
        initial_state=TrafficColor.RED,
        transitions=transitions,
        on_illegal=log_illegal,
    )


def demo_v4() -> None:
    print("\n--- v4: 通用 FSM 框架片段 (可复用) ---")
    fsm = build_traffic_light_fsm()
    print(f"初始状态: {fsm.current_state.name}")
    for _ in range(4):
        fsm.fire(TrafficEvent.TIMER_EXPIRED)
        print(f"-> {fsm.current_state.name}")
    # 触发一个未定义事件，演示"非法输入被忽略并保持原状态"。
    fsm.fire(TrafficEvent.TIMER_EXPIRED)


if __name__ == "__main__":
    demo_v1()
    demo_v2()
    demo_v3()
    demo_v4()
