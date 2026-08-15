#!/usr/bin/env python3
"""TCP handshake: nic_send(flags, seq, ack); sequence numbers live in Ctx."""

from dataclasses import dataclass
from enum import Enum, auto
from typing import Dict, List, Sequence, Tuple


# ---------------------------------------------------------------------------
# 1. Types
# ---------------------------------------------------------------------------

class State(Enum):
    CLOSED = auto()
    LISTEN = auto()
    SYN_SENT = auto()
    SYN_RCVD = auto()
    ESTABLISHED = auto()


class EventKind(Enum):
    PASSIVE_OPEN = auto()
    ACTIVE_OPEN = auto()
    SYN = auto()
    SYNACK = auto()
    ACK = auto()


class ActionKind(Enum):
    NIC_SEND = auto()


@dataclass(frozen=True)
class Event:
    kind: EventKind
    seq: int = 0
    ack: int = 0


@dataclass(frozen=True)
class Action:
    kind: ActionKind
    flags: str
    seq: int
    ack: int


@dataclass
class Ctx:
    state: State
    local_seq: int
    remote_seq: int


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    local_seq: int
    remote_seq: int
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, EventKind) -> (NextState, ActionKind list)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, EventKind], Tuple[State, List[ActionKind]]] = {
    (State.CLOSED, EventKind.PASSIVE_OPEN): (State.LISTEN, []),
    (State.LISTEN, EventKind.SYN): (State.SYN_RCVD, [ActionKind.NIC_SEND]),
    (State.SYN_RCVD, EventKind.ACK): (State.ESTABLISHED, []),
    (State.CLOSED, EventKind.ACTIVE_OPEN): (State.SYN_SENT, [ActionKind.NIC_SEND]),
    (State.SYN_SENT, EventKind.SYNACK): (State.ESTABLISHED, [ActionKind.NIC_SEND]),
}


# ---------------------------------------------------------------------------
# 3. Engine  -- table lookup + bind seq/ack from Ctx and Event
# ---------------------------------------------------------------------------

def fsm_decide(ctx: Ctx, event: Event) -> Decision:
    hit = TABLE.get((ctx.state, event.kind))
    if hit is None:
        return Decision(False, ctx.state, ctx.local_seq, ctx.remote_seq, ())
    next_state, kinds = hit
    local_seq = ctx.local_seq
    remote_seq = ctx.remote_seq
    actions: List[Action] = []
    if ActionKind.NIC_SEND in kinds:
        if event.kind is EventKind.SYN:
            remote_seq = event.seq
            actions.append(
                Action(ActionKind.NIC_SEND, "SYN+ACK", local_seq, event.seq + 1)
            )
        elif event.kind is EventKind.ACTIVE_OPEN:
            actions.append(Action(ActionKind.NIC_SEND, "SYN", local_seq, 0))
        elif event.kind is EventKind.SYNACK:
            remote_seq = event.seq
            local_seq = ctx.local_seq + 1
            actions.append(
                Action(ActionKind.NIC_SEND, "ACK", local_seq, event.seq + 1)
            )
    return Decision(True, next_state, local_seq, remote_seq, tuple(actions))


# ---------------------------------------------------------------------------
# 4. Workers  -- parameterized functions; no FSM state
# ---------------------------------------------------------------------------

def nic_send(flags: str, seq: int, ack: int) -> None:
    print(f"[EXEC] nic_send({flags}, {seq}, {ack})")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        print(
            f"[DISPATCH] NIC_SEND(flags={action.flags}, "
            f"seq={action.seq}, ack={action.ack})"
        )
        nic_send(action.flags, action.seq, action.ack)


# ---------------------------------------------------------------------------
# 6. Apply  -- commit ctx (including seq), then dispatch
# ---------------------------------------------------------------------------

def format_event(event: Event) -> str:
    if event.kind in (EventKind.SYN, EventKind.SYNACK, EventKind.ACK):
        return f"{event.kind.name}(seq={event.seq}, ack={event.ack})"
    return event.kind.name


def apply(ctx: Ctx, event: Event) -> None:
    print(f"[EVENT] {format_event(event)} | state={ctx.state.name}")
    decision = fsm_decide(ctx, event)
    if not decision.accepted:
        print(
            f"[DECISION] REJECT {ctx.state.name} + {event.kind.name}  "
            "(state unchanged)"
        )
        return
    print(
        f"[DECISION] {ctx.state.name} + {event.kind.name} -> "
        f"{decision.next_state.name}"
    )
    ctx.state = decision.next_state
    ctx.local_seq = decision.local_seq
    ctx.remote_seq = decision.remote_seq
    dispatch(decision.actions)


def run_scenario(title: str, local_seq: int, events: Sequence[Event]) -> None:
    ctx = Ctx(state=State.CLOSED, local_seq=local_seq, remote_seq=0)
    print(f"=== scenario: {title} ===")
    print(
        f"[INIT] state={ctx.state.name} "
        f"local_seq={ctx.local_seq} remote_seq={ctx.remote_seq}"
    )
    print()
    for event in events:
        apply(ctx, event)
        print()


# ---------------------------------------------------------------------------
# 7. Main
# ---------------------------------------------------------------------------

def main() -> None:
    run_scenario(
        "server",
        1000,
        (
            Event(EventKind.PASSIVE_OPEN),
            Event(EventKind.SYN, seq=2000, ack=0),
            Event(EventKind.ACK, seq=2001, ack=1001),
            Event(EventKind.SYN, seq=2002, ack=1001),
        ),
    )
    run_scenario(
        "client",
        5000,
        (
            Event(EventKind.ACTIVE_OPEN),
            Event(EventKind.SYNACK, seq=8000, ack=5001),
            Event(EventKind.ACK, seq=8001, ack=5001),
        ),
    )


if __name__ == "__main__":
    main()
