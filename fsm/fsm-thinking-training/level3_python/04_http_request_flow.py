"""Lesson 04: HTTP请求/连接生命周期状态机 (Level 3 - Python工程场景).

配合 04_http_request_flow.md 阅读。这一课刻意只对"值得用FSM的那一
层"(请求接收/处理/响应/复用/异常的连接生命周期)写状态机代码;
"解析->路由->中间件->业务处理"这条固定顺序主线在代码里只用一个
普通函数表示,不套状态机写法——这个对照本身就是教学重点。

四版实现:
    v1: Enum + match-case
    v2: Transition Table(同一事件在不同阶段映射到不同处理函数)
    v3: State Pattern(CLOSED状态对迟到事件"安全丢弃并告警"而不是抛异常)
    v4: 通用FSM框架
"""

from __future__ import annotations

import logging
from abc import ABC
from collections.abc import Callable
from dataclasses import dataclass, field
from enum import Enum, auto

logger = logging.getLogger(__name__)


class RequestState(Enum):
    IDLE = auto()
    RECEIVING = auto()
    PROCESSING = auto()
    RESPONDING = auto()
    DONE = auto()
    ERROR = auto()
    CLOSED = auto()


class RequestEvent(Enum):
    REQUEST_ARRIVED = auto()
    REQUEST_COMPLETE = auto()
    PROCESSING_COMPLETE = auto()
    RESPONSE_SENT = auto()
    TIMEOUT = auto()
    CLIENT_DISCONNECTED = auto()
    ERROR_OCCURRED = auto()
    KEEP_ALIVE_REUSE = auto()
    CONNECTION_CLOSE = auto()


class InvalidTransitionError(Exception):
    """结构性非法转移: 说明状态机的调用方(通常是服务器实现自身)出现了bug。"""

    def __init__(self, state: RequestState, event: RequestEvent) -> None:
        self.state = state
        self.event = event
        super().__init__(
            f"非法转移: 当前状态={state.name}, 事件={event.name} 不允许发生"
        )


def run_business_pipeline(raw_request: str) -> str:
    """Step 1 提到的"顺序主线": 解析->路由->中间件->业务处理->渲染。

    这条主线本身不分支、不循环、不依赖历史,所以刻意不用状态机写法,
    只是一串普通的函数调用——这正是本课要对照的"反面案例"。
    """

    parsed = _parse(raw_request)
    route = _route(parsed)
    _run_middlewares(route)
    result = _handle(route)
    return _render(result)


def _parse(raw_request: str) -> dict[str, str]:
    return {"path": raw_request}


def _route(parsed: dict[str, str]) -> str:
    return parsed["path"]


def _run_middlewares(route: str) -> None:
    return None


def _handle(route: str) -> str:
    return f"handled:{route}"


def _render(result: str) -> str:
    return f"200 OK {result}"


# ============================================================
# v1: 最简单版本 —— Enum + match-case
#
# 设计理由: client_disconnected() 方法内部用一个 match 集中展示
# "同一事件在RECEIVING/PROCESSING/RESPONDING三个阶段执行不同清理
# 动作"这一核心教学点。
# ============================================================


@dataclass
class RequestLifecycleV1:
    state: RequestState = RequestState.IDLE
    cleanup_log: list[str] = field(default_factory=list)

    def request_arrived(self) -> None:
        match self.state:
            case RequestState.IDLE:
                self.state = RequestState.RECEIVING
            case _:
                raise InvalidTransitionError(self.state, RequestEvent.REQUEST_ARRIVED)

    def request_complete(self) -> None:
        match self.state:
            case RequestState.RECEIVING:
                self.state = RequestState.PROCESSING
            case _:
                raise InvalidTransitionError(self.state, RequestEvent.REQUEST_COMPLETE)

    def processing_complete(self) -> None:
        match self.state:
            case RequestState.PROCESSING:
                self.state = RequestState.RESPONDING
            case _:
                raise InvalidTransitionError(
                    self.state, RequestEvent.PROCESSING_COMPLETE
                )

    def response_sent(self) -> None:
        match self.state:
            case RequestState.RESPONDING | RequestState.ERROR:
                self.state = RequestState.DONE
            case _:
                raise InvalidTransitionError(self.state, RequestEvent.RESPONSE_SENT)

    def timeout(self) -> None:
        match self.state:
            case RequestState.RECEIVING:
                self.state = RequestState.ERROR
            case _:
                raise InvalidTransitionError(self.state, RequestEvent.TIMEOUT)

    def error_occurred(self) -> None:
        match self.state:
            case RequestState.PROCESSING:
                self.state = RequestState.ERROR
            case _:
                raise InvalidTransitionError(self.state, RequestEvent.ERROR_OCCURRED)

    def client_disconnected(self) -> None:
        # 同一个事件,在不同阶段有完全不同的合理反应——这是本课的核心。
        match self.state:
            case RequestState.RECEIVING:
                self.cleanup_log.append("接收阶段断开: 无资源需要清理,直接关闭")
                self.state = RequestState.CLOSED
            case RequestState.PROCESSING:
                self.cleanup_log.append("处理阶段断开: 回滚未完成的事务")
                self.state = RequestState.ERROR
            case RequestState.RESPONDING:
                self.cleanup_log.append("响应阶段断开: 终止未完成的写入流")
                self.state = RequestState.ERROR
            case _:
                raise InvalidTransitionError(
                    self.state, RequestEvent.CLIENT_DISCONNECTED
                )

    def keep_alive_reuse(self) -> None:
        match self.state:
            case RequestState.DONE:
                self.state = RequestState.IDLE
            case _:
                raise InvalidTransitionError(self.state, RequestEvent.KEEP_ALIVE_REUSE)

    def connection_close(self) -> None:
        match self.state:
            case RequestState.DONE | RequestState.ERROR:
                self.state = RequestState.CLOSED
            case _:
                raise InvalidTransitionError(self.state, RequestEvent.CONNECTION_CLOSE)


# ============================================================
# v2: Transition Table
#
# 相比v1的改进: CLIENT_DISCONNECTED 在表里对应三个不同的
# (状态, 事件) key,分别映射到三个不同的处理函数——"同一事件不同
# 后果"被表结构本身自然地区分开,不需要在一个方法里用大段match。
# ============================================================


@dataclass
class RequestLifecycleV2:
    state: RequestState = RequestState.IDLE
    cleanup_log: list[str] = field(default_factory=list)


def _v2_request_arrived(r: RequestLifecycleV2) -> None:
    r.state = RequestState.RECEIVING


def _v2_request_complete(r: RequestLifecycleV2) -> None:
    r.state = RequestState.PROCESSING


def _v2_processing_complete(r: RequestLifecycleV2) -> None:
    r.state = RequestState.RESPONDING


def _v2_response_sent(r: RequestLifecycleV2) -> None:
    r.state = RequestState.DONE


def _v2_timeout(r: RequestLifecycleV2) -> None:
    r.state = RequestState.ERROR
    r.cleanup_log.append("接收超时,准备408响应")


def _v2_error_occurred(r: RequestLifecycleV2) -> None:
    r.state = RequestState.ERROR
    r.cleanup_log.append("处理异常,准备500响应")


def _v2_disconnect_during_receiving(r: RequestLifecycleV2) -> None:
    r.cleanup_log.append("接收阶段断开: 无资源需要清理,直接关闭")
    r.state = RequestState.CLOSED


def _v2_disconnect_during_processing(r: RequestLifecycleV2) -> None:
    r.cleanup_log.append("处理阶段断开: 回滚未完成的事务")
    r.state = RequestState.ERROR


def _v2_disconnect_during_responding(r: RequestLifecycleV2) -> None:
    r.cleanup_log.append("响应阶段断开: 终止未完成的写入流")
    r.state = RequestState.ERROR


def _v2_keep_alive_reuse(r: RequestLifecycleV2) -> None:
    r.state = RequestState.IDLE


def _v2_connection_close(r: RequestLifecycleV2) -> None:
    r.state = RequestState.CLOSED


V2_TRANSITIONS: dict[tuple[RequestState, RequestEvent], Callable[[RequestLifecycleV2], None]] = {
    (RequestState.IDLE, RequestEvent.REQUEST_ARRIVED): _v2_request_arrived,
    (RequestState.RECEIVING, RequestEvent.REQUEST_COMPLETE): _v2_request_complete,
    (RequestState.RECEIVING, RequestEvent.TIMEOUT): _v2_timeout,
    (RequestState.RECEIVING, RequestEvent.CLIENT_DISCONNECTED): _v2_disconnect_during_receiving,
    (RequestState.PROCESSING, RequestEvent.PROCESSING_COMPLETE): _v2_processing_complete,
    (RequestState.PROCESSING, RequestEvent.ERROR_OCCURRED): _v2_error_occurred,
    (RequestState.PROCESSING, RequestEvent.CLIENT_DISCONNECTED): _v2_disconnect_during_processing,
    (RequestState.RESPONDING, RequestEvent.RESPONSE_SENT): _v2_response_sent,
    (RequestState.RESPONDING, RequestEvent.CLIENT_DISCONNECTED): _v2_disconnect_during_responding,
    (RequestState.ERROR, RequestEvent.RESPONSE_SENT): _v2_response_sent,
    (RequestState.DONE, RequestEvent.KEEP_ALIVE_REUSE): _v2_keep_alive_reuse,
    (RequestState.DONE, RequestEvent.CONNECTION_CLOSE): _v2_connection_close,
    (RequestState.ERROR, RequestEvent.CONNECTION_CLOSE): _v2_connection_close,
}


def apply_event_v2(r: RequestLifecycleV2, event: RequestEvent) -> None:
    handler = V2_TRANSITIONS.get((r.state, event))
    if handler is None:
        raise InvalidTransitionError(r.state, event)
    handler(r)


# ============================================================
# v3: State Pattern
#
# 适用场景: CLOSED 是终态,但对"迟到的事件"(比如一个已经不再被
# 任何代码持有引用的旧连接对象,回调却延迟触发)选择"安全丢弃并
# 告警",而不是抛异常——因为这类事件通常没有明确的调用方能捕获
# 这个异常。这个策略差异被封装在 ClosedHandler 类里,和其它阶段
# "抛InvalidTransitionError"的策略清楚地区分开。
# ============================================================


@dataclass
class RequestLifecycleV3:
    state: RequestState = RequestState.IDLE
    cleanup_log: list[str] = field(default_factory=list)


class RequestStateHandler(ABC):
    state: RequestState

    def _reject(self, event: RequestEvent) -> None:
        raise InvalidTransitionError(self.state, event)

    def request_arrived(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.REQUEST_ARRIVED)

    def request_complete(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.REQUEST_COMPLETE)

    def processing_complete(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.PROCESSING_COMPLETE)

    def response_sent(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.RESPONSE_SENT)

    def timeout(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.TIMEOUT)

    def error_occurred(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.ERROR_OCCURRED)

    def client_disconnected(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.CLIENT_DISCONNECTED)

    def keep_alive_reuse(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.KEEP_ALIVE_REUSE)

    def connection_close(self, r: RequestLifecycleV3) -> None:
        self._reject(RequestEvent.CONNECTION_CLOSE)


class _IdleHandler(RequestStateHandler):
    state = RequestState.IDLE

    def request_arrived(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.RECEIVING


class _ReceivingHandler(RequestStateHandler):
    state = RequestState.RECEIVING

    def request_complete(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.PROCESSING

    def timeout(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.ERROR
        r.cleanup_log.append("接收超时")

    def client_disconnected(self, r: RequestLifecycleV3) -> None:
        r.cleanup_log.append("接收阶段断开: 无需清理")
        r.state = RequestState.CLOSED


class _ProcessingHandler(RequestStateHandler):
    state = RequestState.PROCESSING

    def processing_complete(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.RESPONDING

    def error_occurred(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.ERROR

    def client_disconnected(self, r: RequestLifecycleV3) -> None:
        r.cleanup_log.append("处理阶段断开: 回滚事务")
        r.state = RequestState.ERROR


class _RespondingHandler(RequestStateHandler):
    state = RequestState.RESPONDING

    def response_sent(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.DONE

    def client_disconnected(self, r: RequestLifecycleV3) -> None:
        r.cleanup_log.append("响应阶段断开: 终止写入流")
        r.state = RequestState.ERROR


class _DoneHandler(RequestStateHandler):
    state = RequestState.DONE

    def keep_alive_reuse(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.IDLE

    def connection_close(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.CLOSED


class _ErrorHandler(RequestStateHandler):
    state = RequestState.ERROR

    def response_sent(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.DONE

    def connection_close(self, r: RequestLifecycleV3) -> None:
        r.state = RequestState.CLOSED


class _ClosedHandler(RequestStateHandler):
    """终态。迟到事件"安全丢弃并告警",不抛异常给一个通常已经不存在的调用方。"""

    state = RequestState.CLOSED

    def _reject(self, event: RequestEvent) -> None:
        logger.warning(
            "连接已关闭后收到迟到事件 %s,已安全丢弃(疑似实现bug)", event.name
        )


_V3_HANDLERS: dict[RequestState, RequestStateHandler] = {
    RequestState.IDLE: _IdleHandler(),
    RequestState.RECEIVING: _ReceivingHandler(),
    RequestState.PROCESSING: _ProcessingHandler(),
    RequestState.RESPONDING: _RespondingHandler(),
    RequestState.DONE: _DoneHandler(),
    RequestState.ERROR: _ErrorHandler(),
    RequestState.CLOSED: _ClosedHandler(),
}


def apply_event_v3(r: RequestLifecycleV3, event: RequestEvent) -> None:
    handler = _V3_HANDLERS[r.state]
    method: Callable[[RequestLifecycleV3], None] = getattr(handler, event.name.lower())
    method(r)


# ============================================================
# v4: 通用FSM框架
# ============================================================


@dataclass(frozen=True)
class Transition[S: Enum, E: Enum]:
    source: S
    event: E
    target: S
    action: Callable[[dict[str, object]], None] | None = None


class GenericFSM[S: Enum, E: Enum]:
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


def build_request_fsm() -> GenericFSM[RequestState, RequestEvent]:
    def log_disconnect(reason: str) -> Callable[[dict[str, object]], None]:
        def _action(ctx: dict[str, object]) -> None:
            log = ctx.setdefault("cleanup_log", [])
            assert isinstance(log, list)
            log.append(reason)

        return _action

    transitions = [
        Transition(RequestState.IDLE, RequestEvent.REQUEST_ARRIVED, RequestState.RECEIVING),
        Transition(RequestState.RECEIVING, RequestEvent.REQUEST_COMPLETE, RequestState.PROCESSING),
        Transition(RequestState.RECEIVING, RequestEvent.TIMEOUT, RequestState.ERROR),
        Transition(
            RequestState.RECEIVING,
            RequestEvent.CLIENT_DISCONNECTED,
            RequestState.CLOSED,
            action=log_disconnect("接收阶段断开: 无需清理"),
        ),
        Transition(RequestState.PROCESSING, RequestEvent.PROCESSING_COMPLETE, RequestState.RESPONDING),
        Transition(RequestState.PROCESSING, RequestEvent.ERROR_OCCURRED, RequestState.ERROR),
        Transition(
            RequestState.PROCESSING,
            RequestEvent.CLIENT_DISCONNECTED,
            RequestState.ERROR,
            action=log_disconnect("处理阶段断开: 回滚事务"),
        ),
        Transition(RequestState.RESPONDING, RequestEvent.RESPONSE_SENT, RequestState.DONE),
        Transition(
            RequestState.RESPONDING,
            RequestEvent.CLIENT_DISCONNECTED,
            RequestState.ERROR,
            action=log_disconnect("响应阶段断开: 终止写入流"),
        ),
        Transition(RequestState.ERROR, RequestEvent.RESPONSE_SENT, RequestState.DONE),
        Transition(RequestState.DONE, RequestEvent.KEEP_ALIVE_REUSE, RequestState.IDLE),
        Transition(RequestState.DONE, RequestEvent.CONNECTION_CLOSE, RequestState.CLOSED),
        Transition(RequestState.ERROR, RequestEvent.CONNECTION_CLOSE, RequestState.CLOSED),
    ]
    return GenericFSM(RequestState.IDLE, transitions)


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("--- v1: Enum + match-case ---")
    print(f"顺序主线(非FSM)结果: {run_business_pipeline('/orders')}")

    req = RequestLifecycleV1()
    req.request_arrived()
    req.request_complete()
    req.client_disconnected()  # PROCESSING阶段断开 -> ERROR
    print(f"处理阶段断开后状态: {req.state.name}, 清理日志: {req.cleanup_log}")

    try:
        req.keep_alive_reuse()
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v2() -> None:
    print("\n--- v2: Transition Table ---")
    req = RequestLifecycleV2()
    apply_event_v2(req, RequestEvent.REQUEST_ARRIVED)
    apply_event_v2(req, RequestEvent.REQUEST_COMPLETE)
    apply_event_v2(req, RequestEvent.PROCESSING_COMPLETE)
    apply_event_v2(req, RequestEvent.CLIENT_DISCONNECTED)  # RESPONDING阶段断开
    print(f"响应阶段断开后状态: {req.state.name}, 清理日志: {req.cleanup_log}")

    apply_event_v2(req, RequestEvent.RESPONSE_SENT)
    apply_event_v2(req, RequestEvent.KEEP_ALIVE_REUSE)
    print(f"Keep-Alive复用后状态(循环回到IDLE): {req.state.name}")

    try:
        apply_event_v2(req, RequestEvent.PROCESSING_COMPLETE)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v3() -> None:
    print("\n--- v3: State Pattern ---")
    req = RequestLifecycleV3()
    apply_event_v3(req, RequestEvent.REQUEST_ARRIVED)
    apply_event_v3(req, RequestEvent.CLIENT_DISCONNECTED)  # RECEIVING阶段断开
    print(f"接收阶段断开后状态: {req.state.name}, 清理日志: {req.cleanup_log}")

    apply_event_v3(req, RequestEvent.RESPONSE_SENT)  # CLOSED状态收到迟到事件
    print("(上面应打印一条WARNING日志,说明迟到事件被安全丢弃而不是抛异常)")

    try:
        apply_event_v3(req, RequestEvent.REQUEST_ARRIVED)
    except InvalidTransitionError:
        print("如果这里抛出异常说明实现有误(CLOSED应丢弃而不是抛出)")
    else:
        print("CLOSED状态正确地丢弃了迟到事件,没有抛出异常")


def _demo_v4() -> None:
    print("\n--- v4: 通用FSM框架 ---")
    fsm = build_request_fsm()
    context: dict[str, object] = {}
    fsm.fire(RequestEvent.REQUEST_ARRIVED, context)
    fsm.fire(RequestEvent.REQUEST_COMPLETE, context)
    fsm.fire(RequestEvent.ERROR_OCCURRED, context)
    fsm.fire(RequestEvent.RESPONSE_SENT, context)
    fsm.fire(RequestEvent.CONNECTION_CLOSE, context)
    print(f"最终状态: {fsm.state.name}")

    try:
        fsm.fire(RequestEvent.REQUEST_ARRIVED, context)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")

    print("转移历史:")
    for line in fsm.history:
        print(f"  {line}")


if __name__ == "__main__":
    logging.basicConfig(level=logging.WARNING)
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
