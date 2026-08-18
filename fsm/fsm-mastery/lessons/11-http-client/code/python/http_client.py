#!/usr/bin/env python3

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable, Optional

MAX_REDIRECT = 3


class State(Enum):
    IDLE = auto()
    SENDING = auto()
    RECEIVING = auto()
    DONE = auto()
    ERROR = auto()


class Event(Enum):
    START = auto()
    SENT = auto()
    HEADER_OK = auto()
    BODY_DONE = auto()
    REDIRECT = auto()
    FAIL = auto()
    CANCEL = auto()
    TIMEOUT = auto()
    CHUNK = auto()


@dataclass
class Context:
    cancel_requested: int = 0
    redirect_count: int = 0
    connection_open: int = 0
    timer_armed: int = 0


@dataclass
class Fsm:
    state: State = State.IDLE
    ctx: Context = field(default_factory=Context)


ActionFn = Callable[[Fsm], None]


def print_transition(prev: State, event: Event, fsm: Fsm) -> None:
    """print one transition and the request context"""
    print(
        f"{prev.name} --{event.name}--> {fsm.state.name} "
        f"(cancel={fsm.ctx.cancel_requested} "
        f"redirects={fsm.ctx.redirect_count} "
        f"conn={fsm.ctx.connection_open})"
    )


def action_send_get(fsm: Fsm) -> None:
    """begin a GET: new socket or reused connection"""
    fsm.ctx.cancel_requested = 0
    fsm.ctx.redirect_count = 0
    fsm.ctx.timer_armed = 1
    if fsm.ctx.connection_open:
        print("reuse connection")
    else:
        print("new connection")
    print("send GET")


def action_sent(fsm: Fsm) -> None:
    """the request bytes left the client"""
    fsm.ctx.connection_open = 1
    print("request sent")


def action_header_ok(fsm: Fsm) -> None:
    """response status line is a success (teaching: always 200)"""
    print("got 200")


def action_body_done(fsm: Fsm) -> None:
    """response body finished"""
    fsm.ctx.timer_armed = 0
    print("body done")


def action_follow_redirect(fsm: Fsm) -> None:
    """follow a 302: count one hop and send GET again"""
    fsm.ctx.redirect_count += 1
    print(
        f"got 302, follow redirect ({fsm.ctx.redirect_count}/{MAX_REDIRECT})"
    )
    print("send GET")


def action_too_many_redirects(fsm: Fsm) -> None:
    """redirect budget exhausted"""
    fsm.ctx.connection_open = 0
    fsm.ctx.timer_armed = 0
    print(f"too many redirects (max {MAX_REDIRECT})")


def action_fail(fsm: Fsm) -> None:
    """transport or protocol failure"""
    fsm.ctx.connection_open = 0
    fsm.ctx.timer_armed = 0
    print("fail")


def action_cancel(fsm: Fsm) -> None:
    """user abort: set the cancel flag, drop the request"""
    fsm.ctx.cancel_requested = 1
    fsm.ctx.connection_open = 0
    fsm.ctx.timer_armed = 0
    print("cancel")


def action_timeout(fsm: Fsm) -> None:
    """request timer fired"""
    fsm.ctx.connection_open = 0
    fsm.ctx.timer_armed = 0
    print("timeout")


def action_chunk(fsm: Fsm) -> None:
    """one body fragment arrived (streaming)"""
    print("chunk")


TABLE: dict[tuple[State, Event], tuple[Optional[ActionFn], State]] = {
    (State.IDLE, Event.START): (action_send_get, State.SENDING),
    (State.DONE, Event.START): (action_send_get, State.SENDING),
    (State.ERROR, Event.START): (action_send_get, State.SENDING),
    (State.SENDING, Event.SENT): (action_sent, State.RECEIVING),
    (State.SENDING, Event.FAIL): (action_fail, State.ERROR),
    (State.SENDING, Event.CANCEL): (action_cancel, State.ERROR),
    (State.SENDING, Event.TIMEOUT): (action_timeout, State.ERROR),
    (State.RECEIVING, Event.HEADER_OK): (action_header_ok, State.RECEIVING),
    (State.RECEIVING, Event.CHUNK): (action_chunk, State.RECEIVING),
    (State.RECEIVING, Event.BODY_DONE): (action_body_done, State.DONE),
    (State.RECEIVING, Event.FAIL): (action_fail, State.ERROR),
    (State.RECEIVING, Event.CANCEL): (action_cancel, State.ERROR),
    (State.RECEIVING, Event.TIMEOUT): (action_timeout, State.ERROR),
}


def fsm_init(fsm: Fsm) -> None:
    """put the request FSM into IDLE with a clean context"""
    fsm.state = State.IDLE
    fsm.ctx = Context()


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event into the HTTP request FSM"""
    prev = fsm.state

    if fsm.ctx.cancel_requested and event in {
        Event.SENT,
        Event.HEADER_OK,
        Event.BODY_DONE,
        Event.REDIRECT,
        Event.CHUNK,
    }:
        print(f"ignore {event.name} (cancel_requested)")
        return

    if event is Event.REDIRECT and fsm.state is State.RECEIVING:
        if fsm.ctx.redirect_count >= MAX_REDIRECT:
            action_too_many_redirects(fsm)
            fsm.state = State.ERROR
            print_transition(prev, event, fsm)
            return
        action_follow_redirect(fsm)
        fsm.state = State.SENDING
        print_transition(prev, event, fsm)
        return

    cell = TABLE.get((fsm.state, event))
    if cell is None:
        return

    action, nxt = cell
    if action is not None:
        action(fsm)
    fsm.state = nxt
    print_transition(prev, event, fsm)

    if event is Event.CANCEL and prev is not State.IDLE:
        fsm.state = State.IDLE
        print(f"{State.ERROR.name} --> {fsm.state.name} (after cancel)")


def main() -> None:
    req = Fsm()
    print("=== http client demo ===")

    print("-- 1. happy path --")
    fsm_init(req)
    print(f"init: {req.state.name}")
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.HEADER_OK)
    fsm_dispatch(req, Event.BODY_DONE)

    print("-- 2. connection reuse (second GET on same client) --")
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.HEADER_OK)
    fsm_dispatch(req, Event.BODY_DONE)

    print("-- 3. one redirect then 200 --")
    fsm_init(req)
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.REDIRECT)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.HEADER_OK)
    fsm_dispatch(req, Event.BODY_DONE)

    print("-- 4. too many redirects --")
    fsm_init(req)
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.REDIRECT)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.REDIRECT)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.REDIRECT)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.REDIRECT)

    print("-- 5. cancel while sending --")
    fsm_init(req)
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.CANCEL)
    fsm_dispatch(req, Event.SENT)

    print("-- 6. cancel while receiving --")
    fsm_init(req)
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.HEADER_OK)
    fsm_dispatch(req, Event.CANCEL)
    fsm_dispatch(req, Event.BODY_DONE)

    print("-- 7. streaming chunks then done --")
    fsm_init(req)
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.HEADER_OK)
    fsm_dispatch(req, Event.CHUNK)
    fsm_dispatch(req, Event.CHUNK)
    fsm_dispatch(req, Event.BODY_DONE)

    print("-- 8. timeout while sending --")
    fsm_init(req)
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.TIMEOUT)

    print("-- 9. fail while receiving --")
    fsm_init(req)
    fsm_dispatch(req, Event.START)
    fsm_dispatch(req, Event.SENT)
    fsm_dispatch(req, Event.FAIL)


if __name__ == "__main__":
    main()
