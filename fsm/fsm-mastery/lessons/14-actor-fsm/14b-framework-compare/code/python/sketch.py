#!/usr/bin/env python3

"""Conceptual pipeline only. Not Akka, CAF, XState, or SObjectizer."""

from collections import deque
from dataclasses import dataclass, field
from enum import Enum, auto


class State(Enum):
    CLOSED = auto()
    SYN_SENT = auto()
    ESTABLISHED = auto()
    ERROR = auto()


class Event(Enum):
    CONNECT = auto()
    SYNACK = auto()
    FAIL = auto()
    RESTART = auto()


@dataclass
class Fsm:
    state: State = State.CLOSED


@dataclass
class Actor:
    name: str
    fsm: Fsm = field(default_factory=Fsm)
    mailbox: deque = field(default_factory=deque)


def tell(actor: Actor, event: Event) -> None:
    """message in -> mailbox

    Akka:          ref.tell(msg) / ref ! msg
    CAF:           self->send(actor, msg)
    XState:        actor.send(event)
    SObjectizer:   so_5::send<Msg>(mbox)
    lesson 14a:    actor_send(to, event)
    """
    actor.mailbox.append(event)
    print(f"tell {actor.name} {event.name}")


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch -> FSM transition (same idea as lesson 08 table lookup)"""
    prev = fsm.state
    table = {
        (State.CLOSED, Event.CONNECT): State.SYN_SENT,
        (State.SYN_SENT, Event.SYNACK): State.ESTABLISHED,
        (State.SYN_SENT, Event.FAIL): State.ERROR,
        (State.ESTABLISHED, Event.FAIL): State.ERROR,
        (State.ERROR, Event.RESTART): State.CLOSED,
    }
    nxt = table.get((prev, event))
    if nxt is None:
        print(f"ignore: {prev.name} + {event.name}")
        return
    fsm.state = nxt
    print(f"{prev.name} --{event.name}--> {fsm.state.name}")


def drain(actor: Actor) -> None:
    """mailbox -> dispatch, one message at a time"""
    while actor.mailbox:
        event = actor.mailbox.popleft()
        print(f"dispatch {actor.name} {event.name}")
        fsm_dispatch(actor.fsm, event)


def main() -> None:
    print("=== conceptual sequence: message -> mailbox -> dispatch -> FSM ===")
    conn = Actor("conn")
    tell(conn, Event.CONNECT)
    tell(conn, Event.SYNACK)
    drain(conn)
    tell(conn, Event.FAIL)
    drain(conn)
    tell(conn, Event.RESTART)
    drain(conn)
    print(f"final: {conn.fsm.state.name}")


if __name__ == "__main__":
    main()
