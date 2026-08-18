#!/usr/bin/env python3

from collections import deque
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional


MAILBOX_CAP = 8


class State(Enum):
    CLOSED = auto()
    SYN_SENT = auto()
    ESTABLISHED = auto()
    ERROR = auto()


class Event(Enum):
    CONNECT = auto()
    SYNACK = auto()
    DATA = auto()
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


def actor_send(to: Actor, event: Event) -> None:
    """enqueue one message; drop if the teaching mailbox is full"""
    if len(to.mailbox) >= MAILBOX_CAP:
        print(f"mailbox full, drop {event.name} -> {to.name}")
        return
    to.mailbox.append(event)


def mailbox_dequeue(actor: Actor) -> Optional[Event]:
    """dequeue one message or None"""
    if not actor.mailbox:
        return None
    return actor.mailbox.popleft()


def fsm_init(fsm: Fsm) -> None:
    """put one connection FSM into CLOSED"""
    fsm.state = State.CLOSED


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one mailbox message into this actor's FSM only"""
    prev = fsm.state
    nxt = prev
    if prev is State.CLOSED and event is Event.CONNECT:
        print("action: send SYN")
        nxt = State.SYN_SENT
    elif prev is State.SYN_SENT and event is Event.SYNACK:
        print("action: send ACK")
        nxt = State.ESTABLISHED
    elif prev is State.ESTABLISHED and event is Event.DATA:
        print("action: recv DATA")
        nxt = State.ESTABLISHED
    elif prev in (State.SYN_SENT, State.ESTABLISHED) and event is Event.FAIL:
        print("action: connection failed")
        nxt = State.ERROR
    elif prev is State.ERROR and event is Event.RESTART:
        print("action: reset connection")
        nxt = State.CLOSED
    else:
        print(f"ignore: {prev.name} + {event.name}")
        return
    fsm.state = nxt
    print(f"{prev.name} --{event.name}--> {fsm.state.name}")


def actor_init(name: str) -> Actor:
    """give one actor a name, empty mailbox, and CLOSED FSM"""
    actor = Actor(name=name)
    fsm_init(actor.fsm)
    return actor


def supervisor_on_error(actor: Actor) -> None:
    """supervisor: restart THIS actor by message, never by writing state"""
    print(f"supervisor: restart {actor.name} (enqueue RESTART)")
    actor_send(actor, Event.RESTART)


def scheduler_run(actors: list[Actor]) -> None:
    """round-robin: one message per actor per turn, until all mailboxes empty"""
    progress = True
    while progress:
        progress = False
        for actor in actors:
            event = mailbox_dequeue(actor)
            if event is None:
                continue
            progress = True
            print(f"sched: {actor.name} dequeue {event.name}")
            fsm_dispatch(actor.fsm, event)
            if actor.fsm.state is State.ERROR:
                supervisor_on_error(actor)


def snapshot(actors: list[Actor], wave: str) -> None:
    """print both actors' isolated state fields"""
    parts = " ".join(f"{a.name}={a.fsm.state.name}" for a in actors)
    print(f"snapshot {wave}: {parts}")


def main() -> None:
    a = actor_init("A")
    b = actor_init("B")
    actors = [a, b]

    print("=== actor + mailbox teaching runtime ===")
    snapshot(actors, "init")

    print("-- wave: CONNECT both --")
    actor_send(a, Event.CONNECT)
    actor_send(b, Event.CONNECT)
    scheduler_run(actors)
    snapshot(actors, "after CONNECT")

    print("-- wave: SYNACK both --")
    actor_send(a, Event.SYNACK)
    actor_send(b, Event.SYNACK)
    scheduler_run(actors)
    snapshot(actors, "after SYNACK")

    print("-- wave: DATA both --")
    actor_send(a, Event.DATA)
    actor_send(b, Event.DATA)
    scheduler_run(actors)
    snapshot(actors, "after DATA")

    print("-- wave: FAIL A and DATA B (isolation + recovery) --")
    actor_send(a, Event.FAIL)
    actor_send(b, Event.DATA)
    scheduler_run(actors)
    snapshot(actors, "after FAIL A")

    print("-- wave: A reconnects; B still ESTABLISHED --")
    actor_send(a, Event.CONNECT)
    actor_send(a, Event.SYNACK)
    actor_send(b, Event.DATA)
    scheduler_run(actors)
    snapshot(actors, "after A recovered")


if __name__ == "__main__":
    main()
