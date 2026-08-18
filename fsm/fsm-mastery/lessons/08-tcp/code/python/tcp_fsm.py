#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional


class State(Enum):
    CLOSED = auto()
    LISTEN = auto()
    SYN_SENT = auto()
    ESTABLISHED = auto()
    TIME_WAIT = auto()


class Event(Enum):
    APP_LISTEN = auto()
    APP_CONNECT = auto()
    RCV_SYN = auto()
    RCV_SYNACK = auto()
    RCV_ACK = auto()
    APP_CLOSE = auto()
    RCV_FIN = auto()
    TIMEOUT = auto()


@dataclass
class Fsm:
    state: State = State.CLOSED


ActionFn = Callable[[Fsm], None]


@dataclass
class Cell:
    action: Optional[ActionFn]
    next: State


def action_listen(fsm: Fsm) -> None:
    """application starts listening"""
    print("action: listen")


def action_send_syn(fsm: Fsm) -> None:
    """send a SYN (active open)"""
    print("action: send SYN")


def action_send_synack(fsm: Fsm) -> None:
    """send SYN-ACK; teaching path skips SYN_RCVD"""
    print("action: send SYN-ACK (skip SYN_RCVD)")


def action_send_ack(fsm: Fsm) -> None:
    """send an ACK"""
    print("action: send ACK")


def action_send_fin(fsm: Fsm) -> None:
    """send a FIN"""
    print("action: send FIN")


def action_retransmit_syn(fsm: Fsm) -> None:
    """retransmit SYN after RTO (retry review)"""
    print("action: retransmit SYN")


def action_recv_ack(fsm: Fsm) -> None:
    """note that this ACK does not change the established session"""
    print("action: recv ACK (stay ESTABLISHED)")


table: dict[tuple[State, Event], Cell] = {
    (State.CLOSED, Event.APP_LISTEN): Cell(action_listen, State.LISTEN),
    (State.CLOSED, Event.APP_CONNECT): Cell(action_send_syn, State.SYN_SENT),
    (State.LISTEN, Event.RCV_SYN): Cell(action_send_synack, State.ESTABLISHED),
    (State.LISTEN, Event.APP_CLOSE): Cell(None, State.CLOSED),
    (State.SYN_SENT, Event.RCV_SYNACK): Cell(action_send_ack, State.ESTABLISHED),
    (State.SYN_SENT, Event.TIMEOUT): Cell(action_retransmit_syn, State.SYN_SENT),
    (State.SYN_SENT, Event.APP_CLOSE): Cell(None, State.CLOSED),
    (State.ESTABLISHED, Event.APP_CLOSE): Cell(action_send_fin, State.TIME_WAIT),
    (State.ESTABLISHED, Event.RCV_FIN): Cell(action_send_ack, State.TIME_WAIT),
    (State.ESTABLISHED, Event.RCV_ACK): Cell(action_recv_ack, State.ESTABLISHED),
    (State.TIME_WAIT, Event.TIMEOUT): Cell(None, State.CLOSED),
}


def fsm_init(fsm: Fsm) -> None:
    """put the teaching TCP FSM into CLOSED"""
    fsm.state = State.CLOSED


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
    tcp = Fsm()
    print("=== tcp teaching FSM ===")

    print("-- passive open then close --")
    fsm_init(tcp)
    print(f"init: {tcp.state.name}")
    fsm_dispatch(tcp, Event.APP_LISTEN)
    fsm_dispatch(tcp, Event.RCV_SYN)
    fsm_dispatch(tcp, Event.APP_CLOSE)
    fsm_dispatch(tcp, Event.TIMEOUT)

    print("-- active open, retransmit, then peer close --")
    fsm_init(tcp)
    print(f"init: {tcp.state.name}")
    fsm_dispatch(tcp, Event.APP_CONNECT)
    fsm_dispatch(tcp, Event.TIMEOUT)
    fsm_dispatch(tcp, Event.RCV_SYNACK)
    fsm_dispatch(tcp, Event.RCV_ACK)
    fsm_dispatch(tcp, Event.RCV_FIN)
    fsm_dispatch(tcp, Event.TIMEOUT)

    print("-- illegal input is a missing table cell --")
    fsm_init(tcp)
    fsm_dispatch(tcp, Event.RCV_FIN)


if __name__ == "__main__":
    main()
