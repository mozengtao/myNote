#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional


class State(Enum):
    SEND_INITIAL = auto()
    HANDSHAKE = auto()
    CONNECTED = auto()
    DRAINING = auto()
    CLOSED = auto()


class Event(Enum):
    INITIAL_ACK = auto()
    HANDSHAKE_DONE = auto()
    APP_CLOSE = auto()
    IDLE_TIMEOUT = auto()
    PATH_FAIL = auto()


@dataclass
class Fsm:
    state: State = State.SEND_INITIAL


ActionFn = Callable[[Fsm], None]


@dataclass
class Cell:
    action: Optional[ActionFn]
    next: State


def action_send_handshake(fsm: Fsm) -> None:
    """Initial was acknowledged; send Handshake packets"""
    print("action: send Handshake")


def action_connected(fsm: Fsm) -> None:
    """handshake confirmed; streams may be created (not implemented)"""
    print("action: connected")


def action_connection_close(fsm: Fsm) -> None:
    """application closes; send CONNECTION_CLOSE and drain"""
    print("action: send CONNECTION_CLOSE")


def action_abandon(fsm: Fsm) -> None:
    """abandon before the session is up"""
    print("action: abandon handshake")


def action_idle_close(fsm: Fsm) -> None:
    """idle timeout tore the connection down"""
    print("action: idle timeout")


def action_path_fail_hard(fsm: Fsm) -> None:
    """path failed during handshake; no session to drain"""
    print("action: path fail, close")


def action_path_fail_drain(fsm: Fsm) -> None:
    """path failed after connect; drain leftover packets"""
    print("action: path fail, drain")


def action_drained(fsm: Fsm) -> None:
    """draining period ended"""
    print("action: drained, close")


table: dict[tuple[State, Event], Cell] = {
    (State.SEND_INITIAL, Event.INITIAL_ACK): Cell(
        action_send_handshake, State.HANDSHAKE
    ),
    (State.SEND_INITIAL, Event.APP_CLOSE): Cell(action_abandon, State.CLOSED),
    (State.SEND_INITIAL, Event.IDLE_TIMEOUT): Cell(
        action_idle_close, State.CLOSED
    ),
    (State.SEND_INITIAL, Event.PATH_FAIL): Cell(
        action_path_fail_hard, State.CLOSED
    ),
    (State.HANDSHAKE, Event.HANDSHAKE_DONE): Cell(
        action_connected, State.CONNECTED
    ),
    (State.HANDSHAKE, Event.APP_CLOSE): Cell(
        action_connection_close, State.DRAINING
    ),
    (State.HANDSHAKE, Event.IDLE_TIMEOUT): Cell(
        action_idle_close, State.CLOSED
    ),
    (State.HANDSHAKE, Event.PATH_FAIL): Cell(
        action_path_fail_hard, State.CLOSED
    ),
    (State.CONNECTED, Event.APP_CLOSE): Cell(
        action_connection_close, State.DRAINING
    ),
    (State.CONNECTED, Event.IDLE_TIMEOUT): Cell(
        action_idle_close, State.CLOSED
    ),
    (State.CONNECTED, Event.PATH_FAIL): Cell(
        action_path_fail_drain, State.DRAINING
    ),
    (State.DRAINING, Event.IDLE_TIMEOUT): Cell(action_drained, State.CLOSED),
    (State.DRAINING, Event.PATH_FAIL): Cell(action_drained, State.CLOSED),
}


def fsm_init(fsm: Fsm) -> None:
    """put the teaching QUIC FSM into SEND_INITIAL"""
    fsm.state = State.SEND_INITIAL


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event by table lookup: table[(state, event)]"""
    prev = fsm.state
    cell = table.get((fsm.state, event))
    if cell is None:
        print(f"ignore: {prev.name} + {event.name}")
        return
    if cell.action is not None:
        cell.action(fsm)
    fsm.state = cell.next
    print(f"{prev.name} --{event.name}--> {fsm.state.name}")


def main() -> None:
    quic = Fsm()
    print("=== quic teaching FSM ===")

    print("-- handshake --")
    fsm_init(quic)
    print(f"init: {quic.state.name}")
    fsm_dispatch(quic, Event.INITIAL_ACK)
    fsm_dispatch(quic, Event.HANDSHAKE_DONE)

    print("-- app close then drain --")
    fsm_dispatch(quic, Event.APP_CLOSE)
    fsm_dispatch(quic, Event.IDLE_TIMEOUT)

    print("-- idle timeout from CONNECTED --")
    fsm_init(quic)
    fsm_dispatch(quic, Event.INITIAL_ACK)
    fsm_dispatch(quic, Event.HANDSHAKE_DONE)
    fsm_dispatch(quic, Event.IDLE_TIMEOUT)

    print("-- path fail after connect drains --")
    fsm_init(quic)
    fsm_dispatch(quic, Event.INITIAL_ACK)
    fsm_dispatch(quic, Event.HANDSHAKE_DONE)
    fsm_dispatch(quic, Event.PATH_FAIL)
    fsm_dispatch(quic, Event.IDLE_TIMEOUT)

    print("-- path fail during handshake --")
    fsm_init(quic)
    fsm_dispatch(quic, Event.INITIAL_ACK)
    fsm_dispatch(quic, Event.PATH_FAIL)

    print("-- illegal input is a missing table cell --")
    fsm_init(quic)
    fsm_dispatch(quic, Event.HANDSHAKE_DONE)


if __name__ == "__main__":
    main()
