#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional


class State(Enum):
    START = auto()
    WAIT_SERVER_HELLO = auto()
    WAIT_FINISHED = auto()
    ESTABLISHED = auto()
    CLOSED = auto()


class Event(Enum):
    CONNECT = auto()
    SERVER_HELLO = auto()
    CERT_OK = auto()
    FINISHED = auto()
    ALERT = auto()
    TIMEOUT = auto()


@dataclass
class Fsm:
    state: State = State.START


ActionFn = Callable[[Fsm], None]


@dataclass
class Cell:
    action: Optional[ActionFn]
    next: State


def action_send_client_hello(fsm: Fsm) -> None:
    """start handshake: send ClientHello"""
    print("action: send ClientHello")


def action_got_server_hello(fsm: Fsm) -> None:
    """ServerHello accepted; still waiting for cert and Finished"""
    print("action: got ServerHello")


def action_cert_ok(fsm: Fsm) -> None:
    """certificate chain verified (crypto is outside this FSM)"""
    print("action: cert ok")


def action_handshake_done(fsm: Fsm) -> None:
    """peer Finished accepted; handshake is done"""
    print("action: handshake done")


def action_alert(fsm: Fsm) -> None:
    """fatal alert; close the teaching session"""
    print("action: alert, close")


def action_timeout(fsm: Fsm) -> None:
    """handshake wait timed out"""
    print("action: handshake timeout")


table: dict[tuple[State, Event], Cell] = {
    (State.START, Event.CONNECT): Cell(
        action_send_client_hello, State.WAIT_SERVER_HELLO
    ),
    (State.WAIT_SERVER_HELLO, Event.SERVER_HELLO): Cell(
        action_got_server_hello, State.WAIT_FINISHED
    ),
    (State.WAIT_SERVER_HELLO, Event.ALERT): Cell(action_alert, State.CLOSED),
    (State.WAIT_SERVER_HELLO, Event.TIMEOUT): Cell(
        action_timeout, State.CLOSED
    ),
    (State.WAIT_FINISHED, Event.CERT_OK): Cell(
        action_cert_ok, State.WAIT_FINISHED
    ),
    (State.WAIT_FINISHED, Event.FINISHED): Cell(
        action_handshake_done, State.ESTABLISHED
    ),
    (State.WAIT_FINISHED, Event.ALERT): Cell(action_alert, State.CLOSED),
    (State.WAIT_FINISHED, Event.TIMEOUT): Cell(action_timeout, State.CLOSED),
    (State.ESTABLISHED, Event.ALERT): Cell(action_alert, State.CLOSED),
}


def fsm_init(fsm: Fsm) -> None:
    """put the teaching TLS FSM into START"""
    fsm.state = State.START


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
    tls = Fsm()
    print("=== tls teaching FSM ===")

    print("-- handshake --")
    fsm_init(tls)
    print(f"init: {tls.state.name}")
    fsm_dispatch(tls, Event.CONNECT)
    fsm_dispatch(tls, Event.SERVER_HELLO)
    fsm_dispatch(tls, Event.CERT_OK)
    fsm_dispatch(tls, Event.FINISHED)

    print("-- alert after established --")
    fsm_dispatch(tls, Event.ALERT)

    print("-- timeout waiting for ServerHello --")
    fsm_init(tls)
    fsm_dispatch(tls, Event.CONNECT)
    fsm_dispatch(tls, Event.TIMEOUT)

    print("-- alert while waiting for Finished --")
    fsm_init(tls)
    fsm_dispatch(tls, Event.CONNECT)
    fsm_dispatch(tls, Event.SERVER_HELLO)
    fsm_dispatch(tls, Event.ALERT)

    print("-- illegal input is a missing table cell --")
    fsm_init(tls)
    fsm_dispatch(tls, Event.FINISHED)


if __name__ == "__main__":
    main()
