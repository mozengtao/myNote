"""Lesson 03: NPC状态 (Level 2 - 小游戏).

对应文档: 03_npc_state.md

本文件的核心教学点是"感知驱动的自动转移": 事件不是玩家按键,而是
距离/血量这类持续性数据跨越阈值后,由一个独立的"感知层"产生的
离散信号。四版实现:

    v1: Enum + match-case,感知转事件的逻辑写在update()里
    v2: Transition Table + 独立的迟滞感知滤波器(PerceptionGate)
    v3: State Pattern,状态对象携带时间戳等历史数据(如最后目击时间)
    v4: 复用通用FSM框架 + 可复用的HysteresisGate迟滞工具
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional, Protocol


# ============================================================
# v1: 最简单版本 —— Enum + match-case
# ============================================================


class NpcStateV1(Enum):
    IDLE = auto()
    PATROL = auto()
    CHASE = auto()
    ATTACK = auto()
    FLEE = auto()
    DEAD = auto()


class NpcEventV1(Enum):
    SIGHT_ACQUIRED = auto()
    SIGHT_LOST = auto()
    IN_ATTACK_RANGE = auto()
    OUT_OF_ATTACK_RANGE = auto()
    HP_BELOW_THRESHOLD = auto()
    HP_RECOVERED = auto()
    HP_ZERO = auto()
    HIT_TAKEN = auto()


#: 迟滞阈值: 视野获得用较小值,失去用较大值,避免临界点抽搐。
SIGHT_ACQUIRE_RANGE = 10.0
SIGHT_LOSE_RANGE = 13.0
ATTACK_RANGE = 2.0
FLEE_HP_RATIO = 0.2
RECOVER_HP_RATIO = 0.5


class NpcV1:
    """v1: 感知转事件的逻辑写在update()里,状态机分支用match处理。"""

    def __init__(self) -> None:
        self.state: NpcStateV1 = NpcStateV1.PATROL
        self._was_seeing_player: bool = False
        self._lost_sight_ticks: int = 0

    def update(self, distance: float, hp_ratio: float) -> None:
        """每帧调用一次: 先把连续量转换成离散事件,再交给状态机处理。"""
        for event in self._perceive(distance, hp_ratio):
            self.handle_event(event)

    def _perceive(self, distance: float, hp_ratio: float):
        events: list[NpcEventV1] = []

        # 视野: 用双阈值做迟滞,避免距离在边界附近抖动导致反复触发。
        if not self._was_seeing_player and distance < SIGHT_ACQUIRE_RANGE:
            self._was_seeing_player = True
            self._lost_sight_ticks = 0
            events.append(NpcEventV1.SIGHT_ACQUIRED)
        elif self._was_seeing_player and distance > SIGHT_LOSE_RANGE:
            self._lost_sight_ticks += 1
            if self._lost_sight_ticks >= 3:  # 容忍窗口: 3帧
                self._was_seeing_player = False
                events.append(NpcEventV1.SIGHT_LOST)
        else:
            self._lost_sight_ticks = 0

        if distance < ATTACK_RANGE:
            events.append(NpcEventV1.IN_ATTACK_RANGE)
        elif self.state is NpcStateV1.ATTACK and distance >= ATTACK_RANGE:
            events.append(NpcEventV1.OUT_OF_ATTACK_RANGE)

        if hp_ratio <= 0:
            events.append(NpcEventV1.HP_ZERO)
        elif hp_ratio < FLEE_HP_RATIO:
            events.append(NpcEventV1.HP_BELOW_THRESHOLD)
        elif hp_ratio > RECOVER_HP_RATIO and self.state is NpcStateV1.FLEE:
            events.append(NpcEventV1.HP_RECOVERED)

        return events

    def handle_event(self, event: NpcEventV1) -> None:
        if event is NpcEventV1.HP_ZERO and self.state is not NpcStateV1.DEAD:
            self.state = NpcStateV1.DEAD
            print("[v1] * -> Dead (hp_zero,最高优先级)")
            return

        match (self.state, event):
            case (NpcStateV1.PATROL | NpcStateV1.IDLE, NpcEventV1.SIGHT_ACQUIRED):
                self.state = NpcStateV1.CHASE
                print("[v1] -> Chase (发现玩家)")

            case (NpcStateV1.PATROL | NpcStateV1.IDLE, NpcEventV1.HIT_TAKEN):
                self.state = NpcStateV1.CHASE
                print("[v1] -> Chase (被偷袭反打)")

            case (NpcStateV1.CHASE, NpcEventV1.IN_ATTACK_RANGE):
                self.state = NpcStateV1.ATTACK
                print("[v1] Chase -> Attack")

            case (NpcStateV1.CHASE, NpcEventV1.SIGHT_LOST):
                self.state = NpcStateV1.PATROL
                print("[v1] Chase -> Patrol (丢失目标)")

            case (NpcStateV1.CHASE, NpcEventV1.HP_BELOW_THRESHOLD):
                self.state = NpcStateV1.FLEE
                print("[v1] Chase -> Flee (血量过低)")

            case (NpcStateV1.CHASE, NpcEventV1.SIGHT_ACQUIRED):
                print("[v1] Chase 收到重复的 sight_acquired -> 忽略")

            case (NpcStateV1.ATTACK, NpcEventV1.OUT_OF_ATTACK_RANGE):
                self.state = NpcStateV1.CHASE
                print("[v1] Attack -> Chase (脱离攻击范围)")

            case (NpcStateV1.ATTACK, NpcEventV1.HP_BELOW_THRESHOLD):
                self.state = NpcStateV1.FLEE
                print("[v1] Attack -> Flee (打断攻击,血量过低优先级更高)")

            case (NpcStateV1.FLEE, NpcEventV1.HP_RECOVERED):
                self.state = NpcStateV1.PATROL
                print("[v1] Flee -> Patrol (血量恢复)")

            case (NpcStateV1.FLEE, _):
                print(f"[v1] Flee 忽略 {event.name}(逃跑期间不关心视野/攻击范围)")

            case (NpcStateV1.DEAD, _):
                print("[v1] Dead 忽略一切事件")

            case _:
                pass  # 其余组合(如已在Chase时IN_ATTACK_RANGE已处理)静默忽略


# ============================================================
# v2: Transition Table + 独立的迟滞感知滤波器
# ============================================================

NpcStateV2 = NpcStateV1
NpcEventV2 = NpcEventV1


class SightGate:
    """独立的两态迟滞滤波器,只负责"是否看得见玩家"这一件事。

    它本身是一台微型状态机(Seeing/NotSeeing),但和NPC的行为FSM
    是两个不同层面的概念: 这台微型FSM的产出(边缘事件),才是
    行为FSM的输入。这是"用小型FSM为大型FSM生成干净事件"的组合手法。
    """

    def __init__(self) -> None:
        self._seeing: bool = False
        self._lost_ticks: int = 0

    def update(self, distance: float) -> Optional[NpcEventV2]:
        if not self._seeing and distance < SIGHT_ACQUIRE_RANGE:
            self._seeing = True
            self._lost_ticks = 0
            return NpcEventV2.SIGHT_ACQUIRED
        if self._seeing and distance > SIGHT_LOSE_RANGE:
            self._lost_ticks += 1
            if self._lost_ticks >= 3:
                self._seeing = False
                return NpcEventV2.SIGHT_LOST
        else:
            self._lost_ticks = 0
        return None


@dataclass
class TransitionV2:
    target: NpcStateV2
    action: Callable[["NpcV2"], None] = lambda npc: None


class NpcV2:
    """v2: 转移规则放进表里,感知逻辑放进独立的SightGate。"""

    TABLE: dict[tuple[NpcStateV2, NpcEventV2], TransitionV2] = {
        (NpcStateV2.PATROL, NpcEventV2.SIGHT_ACQUIRED): TransitionV2(NpcStateV2.CHASE),
        (NpcStateV2.PATROL, NpcEventV2.HIT_TAKEN): TransitionV2(NpcStateV2.CHASE),
        (NpcStateV2.CHASE, NpcEventV2.IN_ATTACK_RANGE): TransitionV2(NpcStateV2.ATTACK),
        (NpcStateV2.CHASE, NpcEventV2.SIGHT_LOST): TransitionV2(NpcStateV2.PATROL),
        (NpcStateV2.CHASE, NpcEventV2.HP_BELOW_THRESHOLD): TransitionV2(NpcStateV2.FLEE),
        (NpcStateV2.ATTACK, NpcEventV2.OUT_OF_ATTACK_RANGE): TransitionV2(NpcStateV2.CHASE),
        (NpcStateV2.ATTACK, NpcEventV2.HP_BELOW_THRESHOLD): TransitionV2(NpcStateV2.FLEE),
        (NpcStateV2.FLEE, NpcEventV2.HP_RECOVERED): TransitionV2(NpcStateV2.PATROL),
    }

    def __init__(self) -> None:
        self.state: NpcStateV2 = NpcStateV2.PATROL
        self._sight_gate = SightGate()

    def update(self, distance: float, hp_ratio: float) -> None:
        sight_event = self._sight_gate.update(distance)
        if sight_event is not None:
            self.handle_event(sight_event)

        if distance < ATTACK_RANGE:
            self.handle_event(NpcEventV2.IN_ATTACK_RANGE)
        elif self.state is NpcStateV2.ATTACK:
            self.handle_event(NpcEventV2.OUT_OF_ATTACK_RANGE)

        if hp_ratio <= 0:
            self.handle_event(NpcEventV2.HP_ZERO)
        elif hp_ratio < FLEE_HP_RATIO:
            self.handle_event(NpcEventV2.HP_BELOW_THRESHOLD)
        elif hp_ratio > RECOVER_HP_RATIO and self.state is NpcStateV2.FLEE:
            self.handle_event(NpcEventV2.HP_RECOVERED)

    def handle_event(self, event: NpcEventV2) -> None:
        if event is NpcEventV2.HP_ZERO and self.state is not NpcStateV2.DEAD:
            self.state = NpcStateV2.DEAD
            print("[v2] * -> Dead")
            return
        if self.state is NpcStateV2.DEAD or self.state is NpcStateV2.FLEE and event not in (
            NpcEventV2.HP_RECOVERED,
            NpcEventV2.HP_ZERO,
        ):
            return
        transition = self.TABLE.get((self.state, event))
        if transition is None:
            return
        transition.action(self)
        old_state = self.state
        self.state = transition.target
        print(f"[v2] {old_state.name} -> {self.state.name} (由 {event.name} 触发)")


# ============================================================
# v3: State Pattern —— 状态对象携带历史数据(时间戳/计数)
# ============================================================


class NpcStateV3Base(Protocol):
    name: str

    def on_enter(self, npc: "NpcV3") -> None: ...

    def handle_event(
        self, npc: "NpcV3", event: NpcEventV2
    ) -> Optional["NpcStateV3Base"]: ...


@dataclass
class PatrolStateV3:
    name: str = "Patrol"
    waypoint_index: int = 0

    def on_enter(self, npc: "NpcV3") -> None:
        print("[v3] 进入 Patrol")

    def handle_event(
        self, npc: "NpcV3", event: NpcEventV2
    ) -> Optional[NpcStateV3Base]:
        match event:
            case NpcEventV2.SIGHT_ACQUIRED | NpcEventV2.HIT_TAKEN:
                return ChaseStateV3()
            case _:
                return None


@dataclass
class ChaseStateV3:
    """携带"最后一次看到玩家距离更新的tick数"这类历史数据。"""

    name: str = "Chase"
    ticks_since_seen: int = 0

    def on_enter(self, npc: "NpcV3") -> None:
        print("[v3] 进入 Chase")

    def handle_event(
        self, npc: "NpcV3", event: NpcEventV2
    ) -> Optional[NpcStateV3Base]:
        match event:
            case NpcEventV2.IN_ATTACK_RANGE:
                return AttackStateV3()
            case NpcEventV2.SIGHT_LOST:
                return PatrolStateV3()
            case NpcEventV2.HP_BELOW_THRESHOLD:
                return FleeStateV3()
            case NpcEventV2.SIGHT_ACQUIRED:
                self.ticks_since_seen = 0
                print("[v3] Chase 收到重复 sight_acquired,仅刷新计数,不切状态")
                return None
            case _:
                return None


@dataclass
class AttackStateV3:
    name: str = "Attack"

    def on_enter(self, npc: "NpcV3") -> None:
        print("[v3] 进入 Attack")

    def handle_event(
        self, npc: "NpcV3", event: NpcEventV2
    ) -> Optional[NpcStateV3Base]:
        match event:
            case NpcEventV2.OUT_OF_ATTACK_RANGE:
                return ChaseStateV3()
            case NpcEventV2.HP_BELOW_THRESHOLD:
                return FleeStateV3()
            case _:
                return None


@dataclass
class FleeStateV3:
    name: str = "Flee"

    def on_enter(self, npc: "NpcV3") -> None:
        print("[v3] 进入 Flee")

    def handle_event(
        self, npc: "NpcV3", event: NpcEventV2
    ) -> Optional[NpcStateV3Base]:
        match event:
            case NpcEventV2.HP_RECOVERED:
                return PatrolStateV3()
            case _:
                print(f"[v3] Flee 忽略 {event.name}")
                return None


@dataclass
class DeadStateV3:
    name: str = "Dead"

    def on_enter(self, npc: "NpcV3") -> None:
        print("[v3] 进入 Dead (终态)")

    def handle_event(
        self, npc: "NpcV3", event: NpcEventV2
    ) -> Optional[NpcStateV3Base]:
        return None


class NpcV3:
    def __init__(self) -> None:
        self.current: NpcStateV3Base = PatrolStateV3()
        self.current.on_enter(self)
        self._sight_gate = SightGate()

    def update(self, distance: float, hp_ratio: float) -> None:
        sight_event = self._sight_gate.update(distance)
        if sight_event is not None:
            self.handle_event(sight_event)
        if distance < ATTACK_RANGE:
            self.handle_event(NpcEventV2.IN_ATTACK_RANGE)
        elif isinstance(self.current, AttackStateV3):
            self.handle_event(NpcEventV2.OUT_OF_ATTACK_RANGE)
        if hp_ratio <= 0:
            self.handle_event(NpcEventV2.HP_ZERO)
        elif hp_ratio < FLEE_HP_RATIO:
            self.handle_event(NpcEventV2.HP_BELOW_THRESHOLD)
        elif hp_ratio > RECOVER_HP_RATIO and isinstance(self.current, FleeStateV3):
            self.handle_event(NpcEventV2.HP_RECOVERED)

    def handle_event(self, event: NpcEventV2) -> None:
        if event is NpcEventV2.HP_ZERO and not isinstance(self.current, DeadStateV3):
            self.current = DeadStateV3()
            self.current.on_enter(self)
            return
        next_state = self.current.handle_event(self, event)
        if next_state is None:
            return
        old_name = self.current.name
        self.current = next_state
        self.current.on_enter(self)
        print(f"[v3] {old_name} -> {self.current.name}")


# ============================================================
# v4: 复用通用FSM框架 + 可复用的HysteresisGate迟滞工具
# ============================================================


@dataclass(frozen=True)
class GenericTransition:
    target: Enum
    guard: Optional[Callable[[object], bool]] = None
    action: Optional[Callable[[object], None]] = None


class GenericFSM:
    def __init__(
        self,
        initial: Enum,
        table: dict[tuple[Enum, Enum], list[GenericTransition]],
    ) -> None:
        self.state: Enum = initial
        self._table = table

    def handle_event(self, event: Enum, context: object) -> bool:
        for transition in self._table.get((self.state, event), []):
            if transition.guard is not None and not transition.guard(context):
                continue
            if transition.action is not None:
                transition.action(context)
            self.state = transition.target
            return True
        return False


class HysteresisGate:
    """通用的"连续量转离散边缘事件"迟滞工具,与NPC业务无关,可复用。

    enter_threshold: 数值低于此值时,如果当前是"未激活",转为"激活"。
    exit_threshold: 数值高于此值时,如果当前是"激活",转为"未激活"。
    exit_threshold 必须大于 enter_threshold,才能形成迟滞区间。
    """

    def __init__(self, enter_threshold: float, exit_threshold: float) -> None:
        if exit_threshold <= enter_threshold:
            raise ValueError("exit_threshold 必须大于 enter_threshold 才能形成迟滞区间")
        self._enter = enter_threshold
        self._exit = exit_threshold
        self._active = False

    def update(self, value: float) -> Optional[str]:
        if not self._active and value < self._enter:
            self._active = True
            return "enter"
        if self._active and value > self._exit:
            self._active = False
            return "exit"
        return None


@dataclass
class NpcContextV4:
    pass


def build_npc_fsm_v4() -> GenericFSM:
    S = NpcStateV2
    E = NpcEventV2
    table: dict[tuple[Enum, Enum], list[GenericTransition]] = {
        (S.PATROL, E.SIGHT_ACQUIRED): [GenericTransition(S.CHASE)],
        (S.PATROL, E.HIT_TAKEN): [GenericTransition(S.CHASE)],
        (S.CHASE, E.IN_ATTACK_RANGE): [GenericTransition(S.ATTACK)],
        (S.CHASE, E.SIGHT_LOST): [GenericTransition(S.PATROL)],
        (S.CHASE, E.HP_BELOW_THRESHOLD): [GenericTransition(S.FLEE)],
        (S.ATTACK, E.OUT_OF_ATTACK_RANGE): [GenericTransition(S.CHASE)],
        (S.ATTACK, E.HP_BELOW_THRESHOLD): [GenericTransition(S.FLEE)],
        (S.FLEE, E.HP_RECOVERED): [GenericTransition(S.PATROL)],
    }
    for state in (S.IDLE, S.PATROL, S.CHASE, S.ATTACK, S.FLEE):
        table[(state, E.HP_ZERO)] = [GenericTransition(S.DEAD)]
    return GenericFSM(initial=S.PATROL, table=table)


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("\n===== v1: Enum + match-case =====")
    npc = NpcV1()
    npc.update(distance=20.0, hp_ratio=1.0)  # 巡逻中,未发现
    npc.update(distance=8.0, hp_ratio=1.0)  # 进入视野 -> Chase
    npc.update(distance=1.5, hp_ratio=1.0)  # 进入攻击范围 -> Attack
    npc.update(distance=1.5, hp_ratio=0.1)  # 血量过低 -> Flee
    npc.update(distance=1.5, hp_ratio=0.9)  # 血量恢复 -> Patrol
    print(f"最终状态: {npc.state.name}")


def _demo_v2() -> None:
    print("\n===== v2: Transition Table + SightGate =====")
    npc = NpcV2()
    npc.update(distance=20.0, hp_ratio=1.0)
    npc.update(distance=8.0, hp_ratio=1.0)
    npc.update(distance=1.5, hp_ratio=1.0)
    print(f"最终状态: {npc.state.name}")


def _demo_v3() -> None:
    print("\n===== v3: State Pattern(携带历史数据) =====")
    npc = NpcV3()
    npc.update(distance=20.0, hp_ratio=1.0)
    npc.update(distance=8.0, hp_ratio=1.0)
    npc.update(distance=1.5, hp_ratio=0.1)
    print(f"最终状态: {npc.current.name}")


def _demo_v4() -> None:
    print("\n===== v4: 通用FSM框架 + HysteresisGate =====")
    fsm = build_npc_fsm_v4()
    ctx = NpcContextV4()
    hp_gate = HysteresisGate(enter_threshold=0.2, exit_threshold=0.5)

    fsm.handle_event(NpcEventV2.SIGHT_ACQUIRED, ctx)
    print(f"[v4] sight_acquired -> {fsm.state.name}")
    fsm.handle_event(NpcEventV2.IN_ATTACK_RANGE, ctx)
    print(f"[v4] in_attack_range -> {fsm.state.name}")

    for hp_ratio in (0.6, 0.3, 0.15, 0.6):
        edge = hp_gate.update(hp_ratio)
        print(f"[v4] hp_ratio={hp_ratio} 迟滞门输出={edge}")
        if edge == "enter":
            fsm.handle_event(NpcEventV2.HP_BELOW_THRESHOLD, ctx)
        elif edge == "exit":
            fsm.handle_event(NpcEventV2.HP_RECOVERED, ctx)
    print(f"最终状态: {fsm.state.name}")


if __name__ == "__main__":
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
