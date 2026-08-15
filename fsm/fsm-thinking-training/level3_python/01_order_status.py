"""Lesson 01: 电商订单状态机 (Level 3 - Python工程场景).

本文件配合 01_order_status.md 阅读,展示同一个"订单状态机"问题的四种实现方式:

    v1: Enum + match-case,手写在一个类里
    v2: Transition Table(数据驱动),集中校验非法转移
    v3: State Pattern,每个状态是一个类
    v4: 通用FSM框架片段,抽象出可复用的 State/Event/Transition/FSM

四版共用同一套 OrderState / OrderEvent 定义,方便对照。
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Callable
from dataclasses import dataclass, field
from enum import Enum, auto


class OrderState(Enum):
    """订单在生命周期中互斥的、有限的阶段。"""

    PENDING_PAYMENT = auto()  # 待支付
    PAID = auto()  # 已支付
    SHIPPED = auto()  # 已发货
    COMPLETED = auto()  # 已完成
    CANCELLED = auto()  # 已取消
    REFUNDING = auto()  # 退款中
    REFUNDED = auto()  # 已退款


class OrderEvent(Enum):
    """真正驱动状态跳变的离散事件(不包含"金额""地址"这类数据变更)。"""

    PAY = auto()
    PAYMENT_TIMEOUT = auto()
    SHIP = auto()
    CONFIRM_RECEIPT = auto()
    CANCEL = auto()
    REQUEST_REFUND = auto()
    APPROVE_REFUND = auto()
    REJECT_REFUND = auto()


class InvalidTransitionError(Exception):
    """收到一个当前状态不允许的事件时抛出——非法转移必须被明确拒绝,不能静默忽略。"""

    def __init__(self, state: OrderState, event: OrderEvent) -> None:
        self.state = state
        self.event = event
        super().__init__(
            f"非法转移: 当前状态={state.name}, 事件={event.name} 不允许发生"
        )


# ============================================================
# v1: 最简单版本 —— Enum + match-case,转移逻辑手写在一个类里
#
# 设计理由: 状态和转移用最直观的方式表达出来,能立刻跑起来、
# 能立刻看懂。缺点是转移规则散落在各个方法里,想知道"待支付能
# 转移到哪几个状态"必须把所有方法读一遍才能拼出完整答案。
# ============================================================


@dataclass
class OrderV1:
    order_id: str
    amount: float
    state: OrderState = OrderState.PENDING_PAYMENT
    pre_refund_state: OrderState | None = None

    def pay(self) -> None:
        match self.state:
            case OrderState.PENDING_PAYMENT:
                self.state = OrderState.PAID
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.PAY)

    def payment_timeout(self) -> None:
        match self.state:
            case OrderState.PENDING_PAYMENT:
                self.state = OrderState.CANCELLED
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.PAYMENT_TIMEOUT)

    def ship(self) -> None:
        match self.state:
            case OrderState.PAID:
                self.state = OrderState.SHIPPED
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.SHIP)

    def confirm_receipt(self) -> None:
        match self.state:
            case OrderState.SHIPPED:
                self.state = OrderState.COMPLETED
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.CONFIRM_RECEIPT)

    def cancel(self) -> None:
        match self.state:
            case OrderState.PENDING_PAYMENT:
                self.state = OrderState.CANCELLED
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.CANCEL)

    def request_refund(self) -> None:
        match self.state:
            case OrderState.PAID | OrderState.SHIPPED:
                self.pre_refund_state = self.state
                self.state = OrderState.REFUNDING
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.REQUEST_REFUND)

    def approve_refund(self) -> None:
        match self.state:
            case OrderState.REFUNDING:
                self.state = OrderState.REFUNDED
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.APPROVE_REFUND)

    def reject_refund(self) -> None:
        match self.state:
            case OrderState.REFUNDING:
                assert self.pre_refund_state is not None
                self.state = self.pre_refund_state
                self.pre_refund_state = None
            case _:
                raise InvalidTransitionError(self.state, OrderEvent.REJECT_REFUND)


# ============================================================
# v2: Transition Table —— 数据驱动,转移规则集中成一张表
#
# 相比v1的改进: (状态, 事件) -> 处理函数 被整理成一张显式的字典,
# 这张表本身就是"合法转移清单",可以被打印、被审计、被单独测试,
# 不需要读遍所有方法才能知道某个状态能转移到哪里。非法转移的
# 判断也统一到了一处(查表失败),不会有方法漏做校验的风险。
# ============================================================


@dataclass
class OrderV2:
    order_id: str
    amount: float
    state: OrderState = OrderState.PENDING_PAYMENT
    pre_refund_state: OrderState | None = None
    history: list[str] = field(default_factory=list)

    def _log(self, message: str) -> None:
        self.history.append(message)


def _v2_to_paid(order: OrderV2) -> None:
    order.state = OrderState.PAID
    order._log("支付成功")


def _v2_cancel(order: OrderV2, reason: str) -> None:
    order.state = OrderState.CANCELLED
    order._log(f"取消: {reason}")


def _v2_to_shipped(order: OrderV2) -> None:
    order.state = OrderState.SHIPPED
    order._log("已发货")


def _v2_to_completed(order: OrderV2) -> None:
    order.state = OrderState.COMPLETED
    order._log("买家确认收货,交易完成")


def _v2_to_refunding(order: OrderV2) -> None:
    order.pre_refund_state = order.state
    order.state = OrderState.REFUNDING
    order._log(f"申请退款(原状态={order.pre_refund_state.name})")


def _v2_approve_refund(order: OrderV2) -> None:
    order.state = OrderState.REFUNDED
    order._log("退款审核通过,已退款")


def _v2_reject_refund(order: OrderV2) -> None:
    assert order.pre_refund_state is not None
    order.state = order.pre_refund_state
    order._log(f"退款被拒绝,回到 {order.pre_refund_state.name}")
    order.pre_refund_state = None


# (当前状态, 事件) -> 处理函数。查不到 = 非法转移。
V2_TRANSITIONS: dict[tuple[OrderState, OrderEvent], Callable[[OrderV2], None]] = {
    (OrderState.PENDING_PAYMENT, OrderEvent.PAY): _v2_to_paid,
    (OrderState.PENDING_PAYMENT, OrderEvent.PAYMENT_TIMEOUT): lambda o: _v2_cancel(
        o, "支付超时"
    ),
    (OrderState.PENDING_PAYMENT, OrderEvent.CANCEL): lambda o: _v2_cancel(
        o, "用户取消"
    ),
    (OrderState.PAID, OrderEvent.SHIP): _v2_to_shipped,
    (OrderState.PAID, OrderEvent.REQUEST_REFUND): _v2_to_refunding,
    (OrderState.SHIPPED, OrderEvent.CONFIRM_RECEIPT): _v2_to_completed,
    (OrderState.SHIPPED, OrderEvent.REQUEST_REFUND): _v2_to_refunding,
    (OrderState.REFUNDING, OrderEvent.APPROVE_REFUND): _v2_approve_refund,
    (OrderState.REFUNDING, OrderEvent.REJECT_REFUND): _v2_reject_refund,
}


def apply_event_v2(order: OrderV2, event: OrderEvent) -> None:
    handler = V2_TRANSITIONS.get((order.state, event))
    if handler is None:
        raise InvalidTransitionError(order.state, event)
    handler(order)


# ============================================================
# v3: State Pattern —— 每个状态是一个类
#
# 适用场景: 当某个状态自身的处理逻辑很复杂(不只是"跳到下一个
# 状态",还涉及一堆子判断,比如退款金额超限要走人工审核),把这
# 些逻辑封装进对应的状态类里,比在一张扁平的转移表里塞满 Action
# 函数更容易维护和单独测试。对于订单状态机这种规则本身不复杂的
# 场景,这一版略显"过度设计",这里展示是为了让你认识这个选项。
# ============================================================


@dataclass
class OrderV3:
    order_id: str
    amount: float
    state: OrderState = OrderState.PENDING_PAYMENT
    pre_refund_state: OrderState | None = None


class OrderStateHandler(ABC):
    """状态类的公共基类:默认拒绝一切事件,子类只重写自己支持的事件。"""

    @property
    @abstractmethod
    def state(self) -> OrderState: ...

    def pay(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.PAY)

    def payment_timeout(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.PAYMENT_TIMEOUT)

    def ship(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.SHIP)

    def confirm_receipt(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.CONFIRM_RECEIPT)

    def cancel(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.CANCEL)

    def request_refund(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.REQUEST_REFUND)

    def approve_refund(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.APPROVE_REFUND)

    def reject_refund(self, order: OrderV3) -> None:
        raise InvalidTransitionError(self.state, OrderEvent.REJECT_REFUND)


class _PendingPaymentHandler(OrderStateHandler):
    state = OrderState.PENDING_PAYMENT

    def pay(self, order: OrderV3) -> None:
        order.state = OrderState.PAID

    def payment_timeout(self, order: OrderV3) -> None:
        order.state = OrderState.CANCELLED

    def cancel(self, order: OrderV3) -> None:
        order.state = OrderState.CANCELLED


class _PaidHandler(OrderStateHandler):
    state = OrderState.PAID

    def ship(self, order: OrderV3) -> None:
        order.state = OrderState.SHIPPED

    def request_refund(self, order: OrderV3) -> None:
        order.pre_refund_state = OrderState.PAID
        order.state = OrderState.REFUNDING


class _ShippedHandler(OrderStateHandler):
    state = OrderState.SHIPPED

    def confirm_receipt(self, order: OrderV3) -> None:
        order.state = OrderState.COMPLETED

    def request_refund(self, order: OrderV3) -> None:
        order.pre_refund_state = OrderState.SHIPPED
        order.state = OrderState.REFUNDING


class _RefundingHandler(OrderStateHandler):
    state = OrderState.REFUNDING

    def approve_refund(self, order: OrderV3) -> None:
        order.state = OrderState.REFUNDED

    def reject_refund(self, order: OrderV3) -> None:
        assert order.pre_refund_state is not None
        order.state = order.pre_refund_state
        order.pre_refund_state = None


class _TerminalHandler(OrderStateHandler):
    """COMPLETED / CANCELLED / REFUNDED 共用:终态不接受任何事件。"""

    def __init__(self, state: OrderState) -> None:
        self._state = state

    @property
    def state(self) -> OrderState:
        return self._state


_V3_HANDLERS: dict[OrderState, OrderStateHandler] = {
    OrderState.PENDING_PAYMENT: _PendingPaymentHandler(),
    OrderState.PAID: _PaidHandler(),
    OrderState.SHIPPED: _ShippedHandler(),
    OrderState.REFUNDING: _RefundingHandler(),
    OrderState.COMPLETED: _TerminalHandler(OrderState.COMPLETED),
    OrderState.CANCELLED: _TerminalHandler(OrderState.CANCELLED),
    OrderState.REFUNDED: _TerminalHandler(OrderState.REFUNDED),
}


def _v3_handler_for(order: OrderV3) -> OrderStateHandler:
    return _V3_HANDLERS[order.state]


def apply_event_v3(order: OrderV3, event: OrderEvent) -> None:
    handler = _v3_handler_for(order)
    method: Callable[[OrderV3], None] = getattr(handler, event.name.lower())
    method(order)


# ============================================================
# v4: 通用FSM框架片段
#
# 什么时候值得引入框架: 当系统里不止一个模块需要状态机(订单、
# 退款单、售后工单……),且都需要同样的"非法转移报错""转移历史
# 记录""转移前后钩子"能力时,抽出通用框架能避免每个状态机重新
# 发明一遍相同的校验逻辑。只有一个状态机时,v2的转移表已经够用,
# 引入框架反而是不必要的复杂度。
# ============================================================


@dataclass(frozen=True)
class Transition[S: Enum, E: Enum]:
    source: S
    event: E
    target: S
    action: Callable[[dict[str, object]], None] | None = None


class GenericFSM[S: Enum, E: Enum]:
    """一个最小可复用的通用FSM:声明式注册转移,查表驱动跳转。"""

    def __init__(self, initial: S, transitions: list[Transition[S, E]]) -> None:
        self._state = initial
        self._table: dict[tuple[S, E], Transition[S, E]] = {
            (t.source, t.event): t for t in transitions
        }
        self.history: list[str] = []

    @property
    def state(self) -> S:
        return self._state

    def fire(self, event: E, context: dict[str, object] | None = None) -> S:
        key = (self._state, event)
        transition = self._table.get(key)
        if transition is None:
            raise InvalidTransitionError(self._state, event)  # type: ignore[arg-type]
        if transition.action is not None:
            transition.action(context or {})
        self.history.append(
            f"{self._state.name} --{event.name}--> {transition.target.name}"
        )
        self._state = transition.target
        return self._state


def build_order_fsm(order_id: str, amount: float) -> tuple[GenericFSM[OrderState, OrderEvent], dict[str, object]]:
    """用通用FSM框架搭建订单状态机,context 字典携带订单业务数据。"""

    context: dict[str, object] = {
        "order_id": order_id,
        "amount": amount,
        "pre_refund_state": None,
    }

    def remember_pre_refund(source: OrderState) -> Callable[[dict[str, object]], None]:
        def _action(ctx: dict[str, object]) -> None:
            ctx["pre_refund_state"] = source

        return _action

    transitions = [
        Transition(OrderState.PENDING_PAYMENT, OrderEvent.PAY, OrderState.PAID),
        Transition(
            OrderState.PENDING_PAYMENT,
            OrderEvent.PAYMENT_TIMEOUT,
            OrderState.CANCELLED,
        ),
        Transition(OrderState.PENDING_PAYMENT, OrderEvent.CANCEL, OrderState.CANCELLED),
        Transition(OrderState.PAID, OrderEvent.SHIP, OrderState.SHIPPED),
        Transition(
            OrderState.PAID,
            OrderEvent.REQUEST_REFUND,
            OrderState.REFUNDING,
            action=remember_pre_refund(OrderState.PAID),
        ),
        Transition(OrderState.SHIPPED, OrderEvent.CONFIRM_RECEIPT, OrderState.COMPLETED),
        Transition(
            OrderState.SHIPPED,
            OrderEvent.REQUEST_REFUND,
            OrderState.REFUNDING,
            action=remember_pre_refund(OrderState.SHIPPED),
        ),
        Transition(OrderState.REFUNDING, OrderEvent.APPROVE_REFUND, OrderState.REFUNDED),
    ]
    fsm = GenericFSM(OrderState.PENDING_PAYMENT, transitions)
    return fsm, context


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("--- v1: Enum + match-case ---")
    order = OrderV1(order_id="O1", amount=99.0)
    order.pay()
    order.ship()
    order.confirm_receipt()
    print(f"最终状态: {order.state.name}")

    try:
        order.cancel()
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v2() -> None:
    print("\n--- v2: Transition Table ---")
    order = OrderV2(order_id="O2", amount=199.0)
    apply_event_v2(order, OrderEvent.PAY)
    apply_event_v2(order, OrderEvent.SHIP)
    apply_event_v2(order, OrderEvent.REQUEST_REFUND)
    print(f"当前状态: {order.state.name}, 历史: {order.history}")
    apply_event_v2(order, OrderEvent.REJECT_REFUND)
    print(f"退款被拒绝后状态: {order.state.name}")

    try:
        apply_event_v2(order, OrderEvent.APPROVE_REFUND)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v3() -> None:
    print("\n--- v3: State Pattern ---")
    order = OrderV3(order_id="O3", amount=299.0)
    apply_event_v3(order, OrderEvent.PAY)
    apply_event_v3(order, OrderEvent.REQUEST_REFUND)
    print(f"当前状态: {order.state.name}")
    apply_event_v3(order, OrderEvent.APPROVE_REFUND)
    print(f"退款通过后状态: {order.state.name}")

    try:
        apply_event_v3(order, OrderEvent.SHIP)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v4() -> None:
    print("\n--- v4: 通用FSM框架 ---")
    fsm, context = build_order_fsm("O4", 399.0)
    fsm.fire(OrderEvent.PAY)
    fsm.fire(OrderEvent.SHIP)
    fsm.fire(OrderEvent.CONFIRM_RECEIPT)
    print(f"最终状态: {fsm.state.name}")
    print("转移历史:")
    for line in fsm.history:
        print(f"  {line}")

    try:
        fsm.fire(OrderEvent.CANCEL)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


if __name__ == "__main__":
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
