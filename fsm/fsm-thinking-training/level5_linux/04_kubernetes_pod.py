"""
Lesson 04: Kubernetes Pod状态 (Level 5 - Linux系统)

教学用简化模型,模拟Kubernetes Pod的status.phase(Pending/Running/
Succeeded/Failed/Unknown)以及容器级状态(Waiting/Running/Terminated),
重点演示"事件驱动FSM"思维在这个场景下需要修正为
"期望状态(spec) + reconcile循环"的混合模式。

不是kubelet的真实实现,只提炼这一课的核心教学点。
Python 3.12+, 无第三方依赖。
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto


# ---------------------------------------------------------------------------
# 公共类型:Pod phase与容器状态
# ---------------------------------------------------------------------------


class PodPhase(Enum):
    PENDING = auto()
    RUNNING = auto()
    SUCCEEDED = auto()
    FAILED = auto()
    UNKNOWN = auto()


class ContainerState(Enum):
    WAITING = auto()
    RUNNING = auto()
    TERMINATED = auto()


class RestartPolicy(Enum):
    ALWAYS = auto()
    ON_FAILURE = auto()
    NEVER = auto()


@dataclass
class ContainerStatus:
    name: str
    state: ContainerState
    exit_code: int | None = None


class PodEvent(Enum):
    SCHEDULED = auto()
    CONTAINER_STARTED = auto()
    CONTAINER_EXITED_ZERO = auto()
    CONTAINER_EXITED_NONZERO = auto()
    NODE_UNREACHABLE = auto()
    NODE_REACHABLE_AGAIN = auto()
    RECONCILE_TICK = auto()


# ---------------------------------------------------------------------------
# v1: 简单版 —— Enum + match-case 硬编码(故意先按传统事件驱动FSM来写)
# ---------------------------------------------------------------------------
# 设计取舍:
#   故意不引入reconcile概念,强行按"事件直接决定下一phase"的方式实现,
#   目的是让读者亲身感受到:CONTAINER_EXITED_NONZERO这个分支必须
#   额外检查"其它容器是否也都终止了""这个容器还能不能重启",
#   已经不是单纯查表能搞定的,这正是Step1讨论的裂缝在代码里的体现。
# ---------------------------------------------------------------------------


class PodV1:
    def __init__(self, name: str, container_names: list[str], restart_policy: RestartPolicy) -> None:
        self.name = name
        self.phase = PodPhase.PENDING
        self.restart_policy = restart_policy
        self.containers: dict[str, ContainerStatus] = {
            n: ContainerStatus(n, ContainerState.WAITING) for n in container_names
        }

    def handle(self, event: PodEvent, container: str | None = None) -> None:
        match self.phase, event:
            case PodPhase.PENDING, PodEvent.SCHEDULED:
                print(f"[{self.name}] Pending阶段: 已调度,记录PodScheduled=True(phase不变)")

            case PodPhase.PENDING, PodEvent.CONTAINER_STARTED:
                assert container is not None
                self.containers[container].state = ContainerState.RUNNING
                print(f"[{self.name}] Pending -> Running (容器{container}已启动)")
                self.phase = PodPhase.RUNNING

            case PodPhase.RUNNING, PodEvent.CONTAINER_STARTED:
                assert container is not None
                self.containers[container].state = ContainerState.RUNNING
                print(f"[{self.name}] 容器{container}进入Running,phase保持Running")

            case PodPhase.RUNNING, (PodEvent.CONTAINER_EXITED_ZERO | PodEvent.CONTAINER_EXITED_NONZERO):
                assert container is not None
                exit_code = 0 if event is PodEvent.CONTAINER_EXITED_ZERO else 1
                self.containers[container].state = ContainerState.TERMINATED
                self.containers[container].exit_code = exit_code
                self._on_container_exit(container, exit_code)

            case (PodPhase.PENDING | PodPhase.RUNNING), PodEvent.NODE_UNREACHABLE:
                print(f"[{self.name}] {self.phase.name} -> Unknown (节点失联,停止信任现有status)")
                self.phase = PodPhase.UNKNOWN

            case PodPhase.UNKNOWN, PodEvent.NODE_REACHABLE_AGAIN:
                print(f"[{self.name}] 节点重新上报心跳,需要重新汇总容器状态才能确定真实phase(v1暂不处理)")

            case _, PodEvent.RECONCILE_TICK:
                pass  # v1没有真正的reconcile概念,这里只是占位,不做任何事

            case _:
                print(f"[{self.name}] {self.phase.name}状态下忽略事件 {event.name}")

    def _on_container_exit(self, container: str, exit_code: int) -> None:
        # 这里已经不是"查表就能得出next_state"了:需要额外检查所有容器的
        # 当前状态和restart_policy,才能算出phase到底该变成什么——
        # 这正是Step1修正点1的体现:next_state依赖一份需要汇总的额外数据。
        if exit_code != 0 and self.restart_policy is not RestartPolicy.NEVER:
            print(f"[{self.name}] 容器{container}非零退出,restartPolicy允许重启,phase保持Running")
            self.containers[container].state = ContainerState.WAITING
            return
        if exit_code == 0 and self.restart_policy is RestartPolicy.ALWAYS:
            print(f"[{self.name}] 容器{container}成功退出但restartPolicy=Always,仍会重启,phase保持Running")
            self.containers[container].state = ContainerState.WAITING
            return

        all_terminated = all(c.state == ContainerState.TERMINATED for c in self.containers.values())
        if not all_terminated:
            print(f"[{self.name}] 容器{container}终止,但还有其它容器未终止,phase保持Running")
            return

        any_failed = any((c.exit_code or 0) != 0 for c in self.containers.values())
        if any_failed:
            print(f"[{self.name}] 全部容器终止,存在失败: Running -> Failed")
            self.phase = PodPhase.FAILED
        else:
            print(f"[{self.name}] 全部容器成功终止: Running -> Succeeded")
            self.phase = PodPhase.SUCCEEDED


# ---------------------------------------------------------------------------
# v2: Transition Table版(数据驱动) —— 演示转移表在这里"不太好用"的边界
# ---------------------------------------------------------------------------
# 设计取舍:
#   转移表本身只能覆盖"不需要汇总额外数据"的简单转移(如SCHEDULED)。
#   一旦next_state依赖"所有容器的状态快照",表就退化成
#   "(phase, event) -> 一个需要额外参数的判断函数",数据驱动带来的
#   收益(改数据不改代码)在这种场景下明显下降。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class SimpleTransitionV2:
    """只适用于"不需要汇总"的简单转移。"""

    next_phase: PodPhase
    note: str = ""


SIMPLE_TABLE_V2: dict[tuple[PodPhase, PodEvent], SimpleTransitionV2] = {
    (PodPhase.PENDING, PodEvent.SCHEDULED): SimpleTransitionV2(
        PodPhase.PENDING, "仅更新PodScheduled Condition,phase不变"
    ),
}

# CONTAINER_EXITED_*事件的下一phase无法只用(phase, event)确定,
# 需要传入容器快照,这里改用一个显式的"汇总函数"而不是继续硬塞进字典。


def summarize_phase_after_exit(
    current_phase: PodPhase,
    containers: dict[str, ContainerStatus],
    restart_policy: RestartPolicy,
) -> PodPhase:
    """对照v1的_on_container_exit,抽成独立函数,凸显"这不是查表"这件事。"""
    all_terminated = all(c.state == ContainerState.TERMINATED for c in containers.values())
    if not all_terminated:
        return PodPhase.RUNNING
    any_failed = any((c.exit_code or 0) != 0 for c in containers.values())
    return PodPhase.FAILED if any_failed else PodPhase.SUCCEEDED


class PodV2:
    def __init__(self, name: str, container_names: list[str], restart_policy: RestartPolicy) -> None:
        self.name = name
        self.phase = PodPhase.PENDING
        self.restart_policy = restart_policy
        self.containers: dict[str, ContainerStatus] = {
            n: ContainerStatus(n, ContainerState.WAITING) for n in container_names
        }

    def handle(self, event: PodEvent, container: str | None = None) -> None:
        simple = SIMPLE_TABLE_V2.get((self.phase, event))
        if simple is not None:
            print(f"[{self.name}] {simple.note}")
            self.phase = simple.next_phase
            return

        if event is PodEvent.CONTAINER_STARTED and container is not None:
            self.containers[container].state = ContainerState.RUNNING
            if self.phase is PodPhase.PENDING:
                print(f"[{self.name}] Pending -> Running (容器{container}已启动)")
                self.phase = PodPhase.RUNNING
            return

        if event in (PodEvent.CONTAINER_EXITED_ZERO, PodEvent.CONTAINER_EXITED_NONZERO) and container is not None:
            exit_code = 0 if event is PodEvent.CONTAINER_EXITED_ZERO else 1
            self.containers[container].exit_code = exit_code

            needs_restart = (exit_code != 0 and self.restart_policy is not RestartPolicy.NEVER) or (
                exit_code == 0 and self.restart_policy is RestartPolicy.ALWAYS
            )
            if needs_restart:
                self.containers[container].state = ContainerState.WAITING
                print(f"[{self.name}] 容器{container}退出({exit_code}),按restartPolicy将重启,phase保持Running")
                return

            self.containers[container].state = ContainerState.TERMINATED
            new_phase = summarize_phase_after_exit(self.phase, self.containers, self.restart_policy)
            print(f"[{self.name}] 汇总容器状态后: {self.phase.name} -> {new_phase.name}")
            self.phase = new_phase
            return

        if event is PodEvent.NODE_UNREACHABLE:
            print(f"[{self.name}] {self.phase.name} -> Unknown (节点失联)")
            self.phase = PodPhase.UNKNOWN
            return

        print(f"[{self.name}] {self.phase.name}状态下忽略事件 {event.name}")


# ---------------------------------------------------------------------------
# v3: State Pattern版 —— 每个phase是一个类,但输入是"状态快照"而不是单一事件
# ---------------------------------------------------------------------------
# 设计取舍:
#   继续沿用State Pattern的外壳,但handle方法接收的不再是一个孤立事件,
#   而是"当前所有容器状态的快照",内部做汇总——这已经不是教科书里
#   典型的State Pattern用法,而是在向reconcile的方向靠拢,故意保留
#   这种"变形"让读者看到过渡的痕迹。
# ---------------------------------------------------------------------------


@dataclass
class PodSnapshot:
    containers: dict[str, ContainerStatus]
    restart_policy: RestartPolicy
    node_reachable: bool = True


class PodPhaseHandler:
    phase: PodPhase

    def compute_next(self, snapshot: PodSnapshot) -> PodPhase:
        raise NotImplementedError


class PendingPhaseHandler(PodPhaseHandler):
    phase = PodPhase.PENDING

    def compute_next(self, snapshot: PodSnapshot) -> PodPhase:
        if not snapshot.node_reachable:
            return PodPhase.UNKNOWN
        if any(c.state == ContainerState.RUNNING for c in snapshot.containers.values()):
            return PodPhase.RUNNING
        return PodPhase.PENDING


class RunningPhaseHandler(PodPhaseHandler):
    phase = PodPhase.RUNNING

    def compute_next(self, snapshot: PodSnapshot) -> PodPhase:
        if not snapshot.node_reachable:
            return PodPhase.UNKNOWN
        all_terminated = all(c.state == ContainerState.TERMINATED for c in snapshot.containers.values())
        if not all_terminated:
            return PodPhase.RUNNING
        any_failed = any((c.exit_code or 0) != 0 for c in snapshot.containers.values())
        return PodPhase.FAILED if any_failed else PodPhase.SUCCEEDED


class UnknownPhaseHandler(PodPhaseHandler):
    phase = PodPhase.UNKNOWN

    def compute_next(self, snapshot: PodSnapshot) -> PodPhase:
        if not snapshot.node_reachable:
            return PodPhase.UNKNOWN
        # 节点重新上报心跳后,需要重新从容器快照汇总出真实phase。
        if all(c.state == ContainerState.TERMINATED for c in snapshot.containers.values()):
            any_failed = any((c.exit_code or 0) != 0 for c in snapshot.containers.values())
            return PodPhase.FAILED if any_failed else PodPhase.SUCCEEDED
        if any(c.state == ContainerState.RUNNING for c in snapshot.containers.values()):
            return PodPhase.RUNNING
        return PodPhase.PENDING


_TERMINAL_PHASES = (PodPhase.SUCCEEDED, PodPhase.FAILED)

_PHASE_HANDLERS: dict[PodPhase, PodPhaseHandler] = {
    PodPhase.PENDING: PendingPhaseHandler(),
    PodPhase.RUNNING: RunningPhaseHandler(),
    PodPhase.UNKNOWN: UnknownPhaseHandler(),
}


class PodV3:
    def __init__(self, name: str, container_names: list[str], restart_policy: RestartPolicy) -> None:
        self.name = name
        self.phase = PodPhase.PENDING
        self.snapshot = PodSnapshot(
            containers={n: ContainerStatus(n, ContainerState.WAITING) for n in container_names},
            restart_policy=restart_policy,
        )

    def resync(self) -> None:
        """对应kubelet每一轮sync loop:不依赖单一事件,重新汇总一次快照。"""
        if self.phase in _TERMINAL_PHASES:
            return  # 终态,不再需要继续resync
        handler = _PHASE_HANDLERS[self.phase]
        next_phase = handler.compute_next(self.snapshot)
        if next_phase != self.phase:
            print(f"[{self.name}] resync: {self.phase.name} -> {next_phase.name}")
            self.phase = next_phase
        else:
            print(f"[{self.name}] resync: 无变化,仍为{self.phase.name}")


# ---------------------------------------------------------------------------
# v4: reconcile函数(修正后的版本) —— 期望(spec) / 观测(status) / 动作 三分离
# ---------------------------------------------------------------------------
# 设计取舍:
#   不再勉强复用前三课的"事件驱动FSM框架",而是新增一个独立的reconcile
#   函数:reconcile(desired, current) -> action。它不直接返回下一个phase,
#   phase由summarize_phase()单独从观测事实汇总得出——这是这一课要示范的
#   "FSM变体":有限状态集合(phase)依然存在且有用,但驱动状态变化的
#   核心逻辑换成了持续的"期望-观测"比较,而不是事件表查找。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class PodSpec:
    """期望状态:要跑哪些容器,用什么重启策略。基本不随时间变化。"""

    container_names: tuple[str, ...]
    restart_policy: RestartPolicy


class PodAction(Enum):
    NOOP = auto()
    START_CONTAINER = auto()
    RESTART_CONTAINER = auto()
    MARK_UNKNOWN = auto()
    RESYNC_FROM_NODE = auto()


@dataclass(frozen=True)
class ReconcileResult:
    action: PodAction
    target_container: str | None = None
    reason: str = ""


def reconcile(desired: PodSpec, current: PodSnapshot) -> ReconcileResult:
    """核心修正点的落地:desired和current都作为显式输入,输出的是"该做的
    动作",不是"下一个phase"。这与前三课GenericFSM.dispatch(event)的签名
    形成直接对比——那里输入是一个事件,这里输入是两份状态。
    """
    if not current.node_reachable:
        return ReconcileResult(PodAction.MARK_UNKNOWN, reason="节点失联,无法确认真实状态")

    for name in desired.container_names:
        status = current.containers.get(name)
        if status is None or status.state == ContainerState.WAITING:
            return ReconcileResult(PodAction.START_CONTAINER, name, "期望的容器尚未启动")
        if status.state == ContainerState.TERMINATED:
            exit_code = status.exit_code or 0
            should_restart = (exit_code != 0 and desired.restart_policy is not RestartPolicy.NEVER) or (
                exit_code == 0 and desired.restart_policy is RestartPolicy.ALWAYS
            )
            if should_restart:
                return ReconcileResult(PodAction.RESTART_CONTAINER, name, f"退出码{exit_code},按策略需要重启")

    return ReconcileResult(PodAction.NOOP, reason="现状已满足期望,本轮无需动作")


def summarize_phase(desired: PodSpec, current: PodSnapshot) -> PodPhase:
    """phase是观测事实的摘要,独立于reconcile的动作决策,对应Step1修正点1。"""
    if not current.node_reachable:
        return PodPhase.UNKNOWN
    statuses = [current.containers.get(n) for n in desired.container_names]
    if any(s is None or s.state == ContainerState.WAITING for s in statuses):
        if any(s is not None and s.state == ContainerState.RUNNING for s in statuses):
            return PodPhase.RUNNING
        return PodPhase.PENDING
    if all(s is not None and s.state == ContainerState.TERMINATED for s in statuses):
        any_failed = any((s.exit_code or 0) != 0 for s in statuses if s is not None)
        return PodPhase.FAILED if any_failed else PodPhase.SUCCEEDED
    return PodPhase.RUNNING


class PodV4:
    """把spec、快照(观测)、phase(摘要)三者显式分开管理。"""

    def __init__(self, name: str, spec: PodSpec) -> None:
        self.name = name
        self.spec = spec
        self.snapshot = PodSnapshot(
            containers={n: ContainerStatus(n, ContainerState.WAITING) for n in spec.container_names},
            restart_policy=spec.restart_policy,
        )
        self.phase = summarize_phase(spec, self.snapshot)

    def apply_action(self, result: ReconcileResult) -> None:
        match result.action:
            case PodAction.START_CONTAINER | PodAction.RESTART_CONTAINER:
                assert result.target_container is not None
                print(f"[{self.name}] 执行动作: {result.action.name}({result.target_container}) 原因: {result.reason}")
                self.snapshot.containers[result.target_container].state = ContainerState.RUNNING
            case PodAction.MARK_UNKNOWN:
                print(f"[{self.name}] 执行动作: {result.action.name} 原因: {result.reason}")
            case PodAction.NOOP:
                print(f"[{self.name}] 本轮reconcile: {result.reason}")

    def sync_loop_tick(self) -> None:
        """对应kubelet的一轮sync,不依赖是否有新事件都会执行。"""
        result = reconcile(self.spec, self.snapshot)
        self.apply_action(result)
        new_phase = summarize_phase(self.spec, self.snapshot)
        if new_phase != self.phase:
            print(f"[{self.name}] phase更新: {self.phase.name} -> {new_phase.name}")
            self.phase = new_phase

    def container_exited(self, name: str, exit_code: int) -> None:
        """模拟"容器进程退出"这一真实事件,更新观测事实,不直接决定phase。"""
        self.snapshot.containers[name].state = ContainerState.TERMINATED
        self.snapshot.containers[name].exit_code = exit_code
        print(f"[{self.name}] 观测更新: 容器{name}退出,退出码{exit_code}")


if __name__ == "__main__":
    print("=== v1: 简单版(强行按事件驱动FSM实现) ===")
    pod1 = PodV1("job-a1", ["main"], RestartPolicy.NEVER)
    pod1.handle(PodEvent.SCHEDULED)
    pod1.handle(PodEvent.CONTAINER_STARTED, container="main")
    pod1.handle(PodEvent.CONTAINER_EXITED_ZERO, container="main")
    print(f"最终phase: {pod1.phase.name}")

    print("\n=== v2: Transition Table版(暴露表驱动的局限) ===")
    pod2 = PodV2("job-a2", ["main"], RestartPolicy.ON_FAILURE)
    pod2.handle(PodEvent.SCHEDULED)
    pod2.handle(PodEvent.CONTAINER_STARTED, container="main")
    pod2.handle(PodEvent.CONTAINER_EXITED_NONZERO, container="main")
    print(f"当前phase(应仍是Running,因为on-failure会重启): {pod2.phase.name}")

    print("\n=== v3: State Pattern版(输入变成状态快照,已向reconcile靠拢) ===")
    pod3 = PodV3("web-b1", ["nginx"], RestartPolicy.ALWAYS)
    pod3.snapshot.containers["nginx"].state = ContainerState.RUNNING
    pod3.resync()
    pod3.snapshot.containers["nginx"].state = ContainerState.TERMINATED
    pod3.snapshot.containers["nginx"].exit_code = 0
    pod3.resync()  # ALWAYS策略下,即使退出码0,真实kubelet也会重启;这里v3简化未模拟重启动作
    print(f"最终phase: {pod3.phase.name}")

    print("\n=== v4: reconcile函数(desired, current) -> action,修正后的模式 ===")
    spec4 = PodSpec(container_names=("web", "sidecar"), restart_policy=RestartPolicy.ON_FAILURE)
    pod4 = PodV4("app-c1", spec4)
    print(f"初始phase: {pod4.phase.name}")
    for _ in range(3):  # 模拟连续几轮sync loop,即使没有新事件也会跑
        pod4.sync_loop_tick()
    pod4.container_exited("web", exit_code=1)
    pod4.sync_loop_tick()  # 这一轮会决定要不要重启web
    pod4.container_exited("sidecar", exit_code=0)
    pod4.sync_loop_tick()
    print(f"最终phase: {pod4.phase.name}")
