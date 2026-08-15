#!/usr/bin/env python3
"""TCP 3-way handshake: SEND_* is an action; the NIC worker transmits."""

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


class Event(Enum):
    PASSIVE_OPEN = auto()
    ACTIVE_OPEN = auto()
    SYN = auto()
    SYNACK = auto()
    ACK = auto()


class Action(Enum):
    SEND_SYN = auto()
    SEND_SYNACK = auto()
    SEND_ACK = auto()


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, Event) -> (NextState, ActionList)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, Event], Tuple[State, List[Action]]] = {
    (State.CLOSED, Event.PASSIVE_OPEN): (State.LISTEN, []),
    (State.LISTEN, Event.SYN): (State.SYN_RCVD, [Action.SEND_SYNACK]),
    (State.SYN_RCVD, Event.ACK): (State.ESTABLISHED, []),
    (State.CLOSED, Event.ACTIVE_OPEN): (State.SYN_SENT, [Action.SEND_SYN]),
    (State.SYN_SENT, Event.SYNACK): (State.ESTABLISHED, [Action.SEND_ACK]),
}


# ---------------------------------------------------------------------------
# 3. Engine  -- table lookup only, zero side effects
# ---------------------------------------------------------------------------

def fsm_decide(state: State, event: Event) -> Decision:
    hit = TABLE.get((state, event))
    if hit is None:
        return Decision(False, state, ())
    next_state, actions = hit
    return Decision(True, next_state, tuple(actions))


# ---------------------------------------------------------------------------
# 4. Workers  -- execution; workers do not know current state
# ---------------------------------------------------------------------------

def worker_nic(action: Action) -> None:
    if action is Action.SEND_SYN:
        print("[EXEC] nic -> SEND SYN")
    elif action is Action.SEND_SYNACK:
        print("[EXEC] nic -> SEND SYN+ACK")
    elif action is Action.SEND_ACK:
        print("[EXEC] nic -> SEND ACK")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        print(f"[DISPATCH] {action.name}")
        worker_nic(action)


# ---------------------------------------------------------------------------
# 6. Apply  -- commit next state, then dispatch
# ---------------------------------------------------------------------------

def apply(state: State, event: Event) -> State:
    print(f"[EVENT] {event.name} | state={state.name}")
    decision = fsm_decide(state, event)
    if not decision.accepted:
        print(
            f"[DECISION] REJECT {state.name} + {event.name}  "
            "(state unchanged)"
        )
        return state
    names = ", ".join(a.name for a in decision.actions)
    print(
        f"[DECISION] {state.name} + {event.name} -> "
        f"{decision.next_state.name}  actions=[{names}]"
    )
    dispatch(decision.actions)
    return decision.next_state


def run_scenario(title: str, events: Sequence[Event]) -> None:
    state = State.CLOSED
    print(f"=== scenario: {title} ===")
    print(f"[INIT] state={state.name}")
    print()
    for event in events:
        state = apply(state, event)
        print()


# ---------------------------------------------------------------------------
# 7. Main
# ---------------------------------------------------------------------------

def main() -> None:
    run_scenario(
        "server",
        (Event.PASSIVE_OPEN, Event.SYN, Event.ACK, Event.SYN),
    )
    run_scenario(
        "client",
        (Event.ACTIVE_OPEN, Event.SYNACK, Event.ACK),
    )


if __name__ == "__main__":
    main()
