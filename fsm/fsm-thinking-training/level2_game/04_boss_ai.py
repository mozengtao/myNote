"""Lesson 04: Boss AI (Level 2 - 小游戏).

对应文档: 04_boss_ai.md

本课的核心教学点是"分层状态机(Hierarchical FSM)的必要性判断"。四版实现:

    v1: 单层Enum + match-case —— 故意按"天真设计"实现,亲眼看到状态爆炸
        和"逃生边"重复的问题(只完整展开Phase1/Phase2,Phase3按同样模式
        继续复制,此处从略,注释里说明该怎么继续加)
    v2: Transition Table —— 证明"把重复的规则搬进表格"不等于"消除重复"
    v3: State Pattern —— 引入共享基类/参数化,展示分层思想的自然雏形
    v4: HierarchicalFSM —— 真正的两层结构: 外层Phase FSM + 内层Behavior
        Sub-FSM,通过"事件冒泡"通信,内层被三个阶段完整复用
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
from typing import Any, Callable, Optional


# ============================================================
# v1: 单层Enum + match-case —— 故意展示"不分层会怎样"
# ============================================================


class BossStateV1(Enum):
    # ---- 阶段1的姿态子状态(5个) ----
    PHASE1_IDLE = auto()
    PHASE1_TELEGRAPH_SWING = auto()
    PHASE1_ATTACK_SWING = auto()
    PHASE1_TELEGRAPH_SLAM = auto()
    PHASE1_ATTACK_SLAM = auto()
    # ---- 阶段2的姿态子状态(8个,几乎和阶段1重复,只是多了Charge和Staggered) ----
    PHASE2_IDLE = auto()
    PHASE2_TELEGRAPH_SWING = auto()
    PHASE2_ATTACK_SWING = auto()
    PHASE2_TELEGRAPH_SLAM = auto()
    PHASE2_ATTACK_SLAM = auto()
    PHASE2_TELEGRAPH_CHARGE = auto()
    PHASE2_ATTACK_CHARGE = auto()
    PHASE2_STAGGERED = auto()
    # 阶段3照此模式还要再加10个状态(Idle/Telegraph_*/Attack_*/Staggered,
    # 外加Meteor这个新招式的Telegraph_Meteor/Attack_Meteor),此处从略。
    # ---- 全局状态 ----
    TRANSITIONING = auto()
    DEFEATED = auto()


class BossEventV1(Enum):
    PLAYER_IN_RANGE = auto()
    TELEGRAPH_DONE = auto()
    ATTACK_DONE = auto()
    WEAKPOINT_HIT = auto()
    STAGGER_DONE = auto()
    HP_BELOW_70 = auto()
    HP_BELOW_30 = auto()
    CUTSCENE_DONE = auto()
    HP_ZERO = auto()


class BossV1:
    """v1: 天真的单层实现。每个阶段的Idle/Telegraph/Attack处理逻辑几乎
    逐字重复,而且每个子状态都要单独写一条"血量阈值触发跳转"的逃生分支
    ——这两点正是Step 2里讨论的状态爆炸和重复规则问题的具体代码体现。
    """

    def __init__(self) -> None:
        self.state: BossStateV1 = BossStateV1.PHASE1_IDLE

    def handle_event(self, event: BossEventV1) -> None:
        if event is BossEventV1.HP_ZERO and self.state is not BossStateV1.DEFEATED:
            self.state = BossStateV1.DEFEATED
            print("[v1] * -> Defeated (最高优先级)")
            return

        # ---- 每个阶段的每个子状态都要重复写这条"逃生边" ----
        if event is BossEventV1.HP_BELOW_70 and self.state in (
            BossStateV1.PHASE1_IDLE,
            BossStateV1.PHASE1_TELEGRAPH_SWING,
            BossStateV1.PHASE1_ATTACK_SWING,
            BossStateV1.PHASE1_TELEGRAPH_SLAM,
            BossStateV1.PHASE1_ATTACK_SLAM,
        ):
            self.state = BossStateV1.TRANSITIONING
            print("[v1] Phase1的某个子状态 -> Transitioning (血量<=70%)")
            return

        if event is BossEventV1.HP_BELOW_30 and self.state in (
            BossStateV1.PHASE2_IDLE,
            BossStateV1.PHASE2_TELEGRAPH_SWING,
            BossStateV1.PHASE2_ATTACK_SWING,
            BossStateV1.PHASE2_TELEGRAPH_SLAM,
            BossStateV1.PHASE2_ATTACK_SLAM,
            BossStateV1.PHASE2_TELEGRAPH_CHARGE,
            BossStateV1.PHASE2_ATTACK_CHARGE,
            BossStateV1.PHASE2_STAGGERED,
        ):
            self.state = BossStateV1.TRANSITIONING
            print("[v1] Phase2的某个子状态 -> Transitioning (血量<=30%)")
            return

        match (self.state, event):
            case (BossStateV1.TRANSITIONING, BossEventV1.CUTSCENE_DONE):
                # 简化: 演示只处理Phase1->Phase2这一次过场。
                self.state = BossStateV1.PHASE2_IDLE
                print("[v1] Transitioning -> Phase2_Idle")

            # ---- 阶段1的招式循环(和阶段2几乎逐字重复) ----
            case (BossStateV1.PHASE1_IDLE, BossEventV1.PLAYER_IN_RANGE):
                self.state = BossStateV1.PHASE1_TELEGRAPH_SWING
                print("[v1] Phase1_Idle -> Phase1_Telegraph_Swing")
            case (BossStateV1.PHASE1_TELEGRAPH_SWING, BossEventV1.TELEGRAPH_DONE):
                self.state = BossStateV1.PHASE1_ATTACK_SWING
                print("[v1] Phase1_Telegraph_Swing -> Phase1_Attack_Swing")
            case (BossStateV1.PHASE1_ATTACK_SWING, BossEventV1.ATTACK_DONE):
                self.state = BossStateV1.PHASE1_IDLE
                print("[v1] Phase1_Attack_Swing -> Phase1_Idle")

            # ---- 阶段2的招式循环(结构和阶段1一模一样,只是名字前缀不同) ----
            case (BossStateV1.PHASE2_IDLE, BossEventV1.PLAYER_IN_RANGE):
                self.state = BossStateV1.PHASE2_TELEGRAPH_SWING
                print("[v1] Phase2_Idle -> Phase2_Telegraph_Swing")
            case (BossStateV1.PHASE2_TELEGRAPH_SWING, BossEventV1.TELEGRAPH_DONE):
                self.state = BossStateV1.PHASE2_ATTACK_SWING
                print("[v1] Phase2_Telegraph_Swing -> Phase2_Attack_Swing")
            case (BossStateV1.PHASE2_ATTACK_SWING, BossEventV1.ATTACK_DONE):
                self.state = BossStateV1.PHASE2_IDLE
                print("[v1] Phase2_Attack_Swing -> Phase2_Idle")
            case (
                BossStateV1.PHASE2_TELEGRAPH_SWING
                | BossStateV1.PHASE2_ATTACK_SWING
                | BossStateV1.PHASE2_IDLE,
                BossEventV1.WEAKPOINT_HIT,
            ):
                self.state = BossStateV1.PHASE2_STAGGERED
                print("[v1] Phase2的某个子状态 -> Phase2_Staggered (弱点命中)")
            case (BossStateV1.PHASE2_STAGGERED, BossEventV1.STAGGER_DONE):
                self.state = BossStateV1.PHASE2_IDLE
                print("[v1] Phase2_Staggered -> Phase2_Idle")

            case (BossStateV1.DEFEATED, _):
                print("[v1] Defeated 忽略一切事件")

            case _:
                print(f"[v1] {self.state.name} 收到 {event.name},无匹配分支,忽略")


# ============================================================
# v2: Transition Table —— 证明"表格化"不等于"消除重复"
# ============================================================

BossStateV2 = BossStateV1
BossEventV2 = BossEventV1


@dataclass
class TransitionV2:
    target: BossStateV2


class BossV2:
    """v2: 规则搬进表里,但下面这张表仍然能看到大量结构相同、只是状态名
    不同的行(Phase1/Phase2的Idle->Telegraph->Attack循环几乎是复制粘贴)。
    这证明"数据驱动"本身不会自动带来复用,复用需要结构性的改变(分层)。
    """

    TABLE: dict[tuple[BossStateV2, BossEventV2], TransitionV2] = {
        (BossStateV2.PHASE1_IDLE, BossEventV2.PLAYER_IN_RANGE): TransitionV2(
            BossStateV2.PHASE1_TELEGRAPH_SWING
        ),
        (BossStateV2.PHASE1_TELEGRAPH_SWING, BossEventV2.TELEGRAPH_DONE): TransitionV2(
            BossStateV2.PHASE1_ATTACK_SWING
        ),
        (BossStateV2.PHASE1_ATTACK_SWING, BossEventV2.ATTACK_DONE): TransitionV2(
            BossStateV2.PHASE1_IDLE
        ),
        # 下面这三行和上面三行结构完全一致,仅状态名从Phase1_*换成Phase2_*。
        (BossStateV2.PHASE2_IDLE, BossEventV2.PLAYER_IN_RANGE): TransitionV2(
            BossStateV2.PHASE2_TELEGRAPH_SWING
        ),
        (BossStateV2.PHASE2_TELEGRAPH_SWING, BossEventV2.TELEGRAPH_DONE): TransitionV2(
            BossStateV2.PHASE2_ATTACK_SWING
        ),
        (BossStateV2.PHASE2_ATTACK_SWING, BossEventV2.ATTACK_DONE): TransitionV2(
            BossStateV2.PHASE2_IDLE
        ),
        (BossStateV2.PHASE2_IDLE, BossEventV2.WEAKPOINT_HIT): TransitionV2(
            BossStateV2.PHASE2_STAGGERED
        ),
        (BossStateV2.PHASE2_STAGGERED, BossEventV2.STAGGER_DONE): TransitionV2(
            BossStateV2.PHASE2_IDLE
        ),
        (BossStateV2.TRANSITIONING, BossEventV2.CUTSCENE_DONE): TransitionV2(
            BossStateV2.PHASE2_IDLE
        ),
    }

    #: 逃生边同样要按阶段各写一份,和v1一样是重复的结构。
    PHASE1_STATES = frozenset(
        {
            BossStateV2.PHASE1_IDLE,
            BossStateV2.PHASE1_TELEGRAPH_SWING,
            BossStateV2.PHASE1_ATTACK_SWING,
        }
    )
    PHASE2_STATES = frozenset(
        {
            BossStateV2.PHASE2_IDLE,
            BossStateV2.PHASE2_TELEGRAPH_SWING,
            BossStateV2.PHASE2_ATTACK_SWING,
            BossStateV2.PHASE2_STAGGERED,
        }
    )

    def __init__(self) -> None:
        self.state: BossStateV2 = BossStateV2.PHASE1_IDLE

    def handle_event(self, event: BossEventV2) -> None:
        if event is BossEventV2.HP_ZERO and self.state is not BossStateV2.DEFEATED:
            self.state = BossStateV2.DEFEATED
            print("[v2] * -> Defeated")
            return
        if event is BossEventV2.HP_BELOW_70 and self.state in self.PHASE1_STATES:
            self.state = BossStateV2.TRANSITIONING
            print("[v2] Phase1 -> Transitioning")
            return
        if event is BossEventV2.HP_BELOW_30 and self.state in self.PHASE2_STATES:
            self.state = BossStateV2.TRANSITIONING
            print("[v2] Phase2 -> Transitioning")
            return

        transition = self.TABLE.get((self.state, event))
        if transition is None:
            print(f"[v2] {self.state.name} 收到 {event.name},忽略")
            return
        old_state = self.state
        self.state = transition.target
        print(f"[v2] {old_state.name} -> {self.state.name}")


# ============================================================
# v3: State Pattern + 共享基类 —— 分层思想的自然雏形
# ============================================================


@dataclass(frozen=True)
class PhaseConfig:
    """阶段配置数据: 决定该阶段的招式池与弱点机制,而不是靠新增状态类。"""

    name: str
    available_moves: tuple[str, ...]
    has_weakpoint: bool


PHASE1_CONFIG = PhaseConfig(name="Phase1", available_moves=("swing", "slam"), has_weakpoint=False)
PHASE2_CONFIG = PhaseConfig(
    name="Phase2", available_moves=("swing", "slam", "charge"), has_weakpoint=True
)
PHASE3_CONFIG = PhaseConfig(
    name="Phase3",
    available_moves=("swing", "slam", "charge", "meteor"),
    has_weakpoint=True,
)


class SubStateV3:
    """v3的共享基类: Idle/Telegraph/Attacking/Staggered的行为逻辑只写一份,
    通过构造时传入的PhaseConfig区分不同阶段的招式池/弱点机制。这一步已经
    是"用参数化共享逻辑取代逐阶段复制"的分层雏形,只是还没有正式拆成
    "外层Phase FSM + 内层Behavior FSM"这两台独立运作的状态机。
    """

    name: str = "SubStateBase"

    def __init__(self, config: PhaseConfig) -> None:
        self.config = config

    def on_enter(self, boss: "BossV3") -> None:
        pass

    def handle_event(
        self, boss: "BossV3", event: BossEventV2
    ) -> Optional["SubStateV3"]:
        return None


class IdleSubStateV3(SubStateV3):
    name = "Idle"

    def on_enter(self, boss: "BossV3") -> None:
        print(f"[v3] [{self.config.name}] 进入 Idle")

    def handle_event(
        self, boss: "BossV3", event: BossEventV2
    ) -> Optional[SubStateV3]:
        match event:
            case BossEventV2.PLAYER_IN_RANGE:
                return TelegraphSubStateV3(self.config)
            case BossEventV2.WEAKPOINT_HIT if self.config.has_weakpoint:
                return StaggeredSubStateV3(self.config)
            case _:
                return None


class TelegraphSubStateV3(SubStateV3):
    name = "Telegraph"

    def __init__(self, config: PhaseConfig) -> None:
        super().__init__(config)
        self.current_move = config.available_moves[0]

    def on_enter(self, boss: "BossV3") -> None:
        print(f"[v3] [{self.config.name}] 进入 Telegraph(招式={self.current_move})")

    def handle_event(
        self, boss: "BossV3", event: BossEventV2
    ) -> Optional[SubStateV3]:
        match event:
            case BossEventV2.TELEGRAPH_DONE:
                return AttackingSubStateV3(self.config, self.current_move)
            case BossEventV2.WEAKPOINT_HIT if self.config.has_weakpoint:
                return StaggeredSubStateV3(self.config)
            case _:
                return None


class AttackingSubStateV3(SubStateV3):
    name = "Attacking"

    def __init__(self, config: PhaseConfig, move: str) -> None:
        super().__init__(config)
        self.current_move = move

    def on_enter(self, boss: "BossV3") -> None:
        print(f"[v3] [{self.config.name}] 进入 Attacking(招式={self.current_move})")

    def handle_event(
        self, boss: "BossV3", event: BossEventV2
    ) -> Optional[SubStateV3]:
        match event:
            case BossEventV2.ATTACK_DONE:
                return IdleSubStateV3(self.config)
            case BossEventV2.WEAKPOINT_HIT if self.config.has_weakpoint:
                return StaggeredSubStateV3(self.config)
            case _:
                return None


class StaggeredSubStateV3(SubStateV3):
    name = "Staggered"

    def on_enter(self, boss: "BossV3") -> None:
        print(f"[v3] [{self.config.name}] 进入 Staggered")

    def handle_event(
        self, boss: "BossV3", event: BossEventV2
    ) -> Optional[SubStateV3]:
        if event is BossEventV2.STAGGER_DONE:
            return IdleSubStateV3(self.config)
        return None


class BossV3:
    """v3: 阶段切换仍然手写(还没有正式的外层FSM),但姿态逻辑已经完全
    共享,不再按阶段复制三遍。"""

    PHASE_ORDER = (PHASE1_CONFIG, PHASE2_CONFIG, PHASE3_CONFIG)

    def __init__(self) -> None:
        self.phase_index: int = 0
        self.sub_state: SubStateV3 = IdleSubStateV3(self.PHASE_ORDER[0])
        self.defeated: bool = False
        self.sub_state.on_enter(self)

    def handle_event(self, event: BossEventV2) -> None:
        if event is BossEventV2.HP_ZERO:
            self.defeated = True
            print("[v3] * -> Defeated")
            return
        if self.defeated:
            return

        if event in (BossEventV2.HP_BELOW_70, BossEventV2.HP_BELOW_30):
            self.phase_index = min(self.phase_index + 1, len(self.PHASE_ORDER) - 1)
            new_config = self.PHASE_ORDER[self.phase_index]
            self.sub_state = IdleSubStateV3(new_config)
            self.sub_state.on_enter(self)
            print(f"[v3] 阶段切换 -> {new_config.name},内层重置为Idle")
            return

        next_sub = self.sub_state.handle_event(self, event)
        if next_sub is None:
            return
        old_name = self.sub_state.name
        self.sub_state = next_sub
        self.sub_state.on_enter(self)
        print(f"[v3] {old_name} -> {self.sub_state.name}")


# ============================================================
# v4: HierarchicalFSM —— 外层Phase FSM + 内层Behavior Sub-FSM + 事件冒泡
# ============================================================


@dataclass(frozen=True)
class GenericTransition:
    target: Enum
    guard: Optional[Callable[[Any], bool]] = None
    action: Optional[Callable[[Any], None]] = None


class GenericFSM:
    """复用前几课定义的通用单层FSM引擎,本课直接拿来做"内层"和"外层"。"""

    def __init__(
        self, initial: Enum, table: dict[tuple[Enum, Enum], list[GenericTransition]]
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

    def known_events(self) -> set[Enum]:
        """这个状态在当前state下认识哪些事件(用于判断是否需要冒泡)。"""
        return {event for (state, event) in self._table if state == self.state}


class BossPhaseV4(Enum):
    PHASE1 = auto()
    PHASE2 = auto()
    PHASE3 = auto()
    TRANSITIONING = auto()
    DEFEATED = auto()


class BossSubStateV4(Enum):
    IDLE = auto()
    TELEGRAPH = auto()
    ATTACKING = auto()
    STAGGERED = auto()


@dataclass
class BossContextV4:
    config: PhaseConfig
    current_move: str = ""
    transition_target: Optional[BossPhaseV4] = None


def _guard_has_weakpoint(ctx: BossContextV4) -> bool:
    return ctx.config.has_weakpoint


def _pick_move(ctx: BossContextV4) -> None:
    ctx.current_move = ctx.config.available_moves[0]


def build_inner_fsm_v4() -> GenericFSM:
    """内层姿态子状态机: 只定义一次,三个阶段共用同一份规则表。"""
    S = BossSubStateV4
    E = BossEventV2
    table: dict[tuple[Enum, Enum], list[GenericTransition]] = {
        (S.IDLE, E.PLAYER_IN_RANGE): [GenericTransition(S.TELEGRAPH, action=_pick_move)],
        (S.IDLE, E.WEAKPOINT_HIT): [
            GenericTransition(S.STAGGERED, guard=_guard_has_weakpoint)
        ],
        (S.TELEGRAPH, E.TELEGRAPH_DONE): [GenericTransition(S.ATTACKING)],
        (S.TELEGRAPH, E.WEAKPOINT_HIT): [
            GenericTransition(S.STAGGERED, guard=_guard_has_weakpoint)
        ],
        (S.ATTACKING, E.ATTACK_DONE): [GenericTransition(S.IDLE)],
        (S.ATTACKING, E.WEAKPOINT_HIT): [
            GenericTransition(S.STAGGERED, guard=_guard_has_weakpoint)
        ],
        (S.STAGGERED, E.STAGGER_DONE): [GenericTransition(S.IDLE)],
    }
    return GenericFSM(initial=S.IDLE, table=table)


def _set_transition_target(target: BossPhaseV4) -> Callable[[BossContextV4], None]:
    def _action(ctx: BossContextV4) -> None:
        ctx.transition_target = target

    return _action


def build_outer_fsm_v4() -> GenericFSM:
    """外层阶段状态机: 只关心宏观阶段,对内层的具体姿态完全无感知。"""
    S = BossPhaseV4
    E = BossEventV2
    table: dict[tuple[Enum, Enum], list[GenericTransition]] = {
        (S.PHASE1, E.HP_BELOW_70): [
            GenericTransition(S.TRANSITIONING, action=_set_transition_target(S.PHASE2))
        ],
        (S.PHASE2, E.HP_BELOW_30): [
            GenericTransition(S.TRANSITIONING, action=_set_transition_target(S.PHASE3))
        ],
        (S.TRANSITIONING, E.CUTSCENE_DONE): [GenericTransition(S.PHASE2)],  # 占位,实际由target决定
    }
    for phase in (S.PHASE1, S.PHASE2, S.PHASE3, S.TRANSITIONING):
        table[(phase, E.HP_ZERO)] = [GenericTransition(S.DEFEATED)]
    return GenericFSM(initial=S.PHASE1, table=table)


PHASE_CONFIG_BY_STATE = {
    BossPhaseV4.PHASE1: PHASE1_CONFIG,
    BossPhaseV4.PHASE2: PHASE2_CONFIG,
    BossPhaseV4.PHASE3: PHASE3_CONFIG,
}


class HierarchicalBossFSM:
    """外层Phase FSM持有并驱动内层Behavior Sub-FSM,两者通过事件冒泡通信。

    处理顺序:
      1. 先尝试让内层处理事件。
      2. 内层不认识的事件(即内层当前状态没有为它注册转移),原样冒泡给外层。
      3. 外层处理阶段切换时,重建内层FSM并换上新阶段的PhaseConfig。
    """

    def __init__(self) -> None:
        self.outer = build_outer_fsm_v4()
        self.ctx = BossContextV4(config=PHASE1_CONFIG)
        self.inner = build_inner_fsm_v4()

    def handle_event(self, event: BossEventV2) -> None:
        if event in self.inner.known_events():
            self.inner.handle_event(event, self.ctx)
            print(f"[v4] 内层处理 {event.name} -> inner={self.inner.state.name}")
            return

        # 内层不认识这个事件,冒泡给外层。
        handled = self.outer.handle_event(event, self.ctx)
        if not handled:
            print(f"[v4] 外层也不认识 {event.name},忽略")
            return

        print(f"[v4] 外层处理 {event.name} -> outer={self.outer.state.name}")

        if self.outer.state is BossPhaseV4.TRANSITIONING:
            print("[v4] 进入过场,内层挂起")
        elif self.outer.state is BossPhaseV4.DEFEATED:
            print("[v4] Boss被击败,内层不再运作")

    def finish_cutscene(self) -> None:
        """过场演出播完: 根据ctx.transition_target跳到目标阶段,并重置内层。"""
        target = self.ctx.transition_target
        if target is None or self.outer.state is not BossPhaseV4.TRANSITIONING:
            return
        self.outer.state = target
        self.ctx = BossContextV4(config=PHASE_CONFIG_BY_STATE[target])
        self.inner = build_inner_fsm_v4()
        print(f"[v4] Transitioning -> {target.name},内层重置为该阶段的Idle")


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("\n===== v1: 单层Enum(天真设计,亲眼看状态爆炸) =====")
    boss = BossV1()
    boss.handle_event(BossEventV1.PLAYER_IN_RANGE)
    boss.handle_event(BossEventV1.TELEGRAPH_DONE)
    boss.handle_event(BossEventV1.HP_BELOW_70)  # 逃生边,打断Attack动画
    boss.handle_event(BossEventV1.CUTSCENE_DONE)
    boss.handle_event(BossEventV1.WEAKPOINT_HIT)
    boss.handle_event(BossEventV1.STAGGER_DONE)
    print(f"最终状态: {boss.state.name}")


def _demo_v2() -> None:
    print("\n===== v2: Transition Table(表格化,但仍重复) =====")
    boss = BossV2()
    boss.handle_event(BossEventV2.PLAYER_IN_RANGE)
    boss.handle_event(BossEventV2.HP_BELOW_70)
    boss.handle_event(BossEventV2.CUTSCENE_DONE)
    boss.handle_event(BossEventV2.WEAKPOINT_HIT)
    print(f"最终状态: {boss.state.name}")


def _demo_v3() -> None:
    print("\n===== v3: State Pattern + 共享基类(分层雏形) =====")
    boss = BossV3()
    boss.handle_event(BossEventV2.PLAYER_IN_RANGE)
    boss.handle_event(BossEventV2.HP_BELOW_70)  # 阶段切换,内层重置
    boss.handle_event(BossEventV2.WEAKPOINT_HIT)  # Phase2有弱点,生效
    boss.handle_event(BossEventV2.STAGGER_DONE)
    print(f"最终阶段: {boss.PHASE_ORDER[boss.phase_index].name}, 内层: {boss.sub_state.name}")


def _demo_v4() -> None:
    print("\n===== v4: HierarchicalFSM(外层+内层+事件冒泡) =====")
    boss = HierarchicalBossFSM()
    boss.handle_event(BossEventV2.PLAYER_IN_RANGE)  # 内层处理
    boss.handle_event(BossEventV2.HP_BELOW_70)  # 冒泡给外层,打断内层
    boss.finish_cutscene()  # Phase2,内层重置为Idle
    boss.handle_event(BossEventV2.WEAKPOINT_HIT)  # 内层处理,Phase2有弱点
    boss.handle_event(BossEventV2.STAGGER_DONE)
    boss.handle_event(BossEventV2.HP_BELOW_30)  # 冒泡给外层 -> Transitioning
    boss.finish_cutscene()  # Phase3
    boss.handle_event(BossEventV2.HP_ZERO)  # 冒泡给外层,最高优先级
    print(f"最终外层状态: {boss.outer.state.name}")


if __name__ == "__main__":
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
