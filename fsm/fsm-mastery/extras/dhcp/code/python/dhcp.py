#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional


class State(Enum):
    INIT = auto()
    SELECTING = auto()
    REQUESTING = auto()
    BOUND = auto()
    RENEWING = auto()


class Event(Enum):
    DISCOVER_SENT = auto()
    OFFER = auto()
    REQUEST_SENT = auto()
    ACK = auto()
    NAK = auto()
    T1 = auto()
    T2 = auto()
    LEASE_EXPIRED = auto()


@dataclass
class Fsm:
    state: State = State.INIT


ActionFn = Callable[[Fsm], None]


@dataclass
class Cell:
    action: Optional[ActionFn]
    next: State


def action_send_discover(fsm: Fsm) -> None:
    """client broadcasts DHCPDISCOVER"""
    print("action: send DISCOVER")


def action_send_request(fsm: Fsm) -> None:
    """client chose an OFFER and sends DHCPREQUEST"""
    print("action: send REQUEST")


def action_request_on_wire(fsm: Fsm) -> None:
    """REQUEST is on the wire; still waiting for ACK"""
    print("action: REQUEST on wire")


def action_bound(fsm: Fsm) -> None:
    """lease is bound"""
    print("action: bound")


def action_send_renew(fsm: Fsm) -> None:
    """T1 fired; unicast renew to the original server"""
    print("action: send renew REQUEST")


def action_renewed(fsm: Fsm) -> None:
    """renew ACK; keep the same lease"""
    print("action: renewed")


def action_nak(fsm: Fsm) -> None:
    """server refused the request"""
    print("action: NAK, restart")


def action_omit_rebinding(fsm: Fsm) -> None:
    """T2 would enter REBINDING; teaching path goes back to INIT"""
    print("action: T2, omit REBINDING, back to INIT")


def action_lease_dead(fsm: Fsm) -> None:
    """lease clock ran out"""
    print("action: lease expired")


table: dict[tuple[State, Event], Cell] = {
    (State.INIT, Event.DISCOVER_SENT): Cell(
        action_send_discover, State.SELECTING
    ),
    (State.SELECTING, Event.OFFER): Cell(action_send_request, State.REQUESTING),
    (State.REQUESTING, Event.REQUEST_SENT): Cell(
        action_request_on_wire, State.REQUESTING
    ),
    (State.REQUESTING, Event.ACK): Cell(action_bound, State.BOUND),
    (State.REQUESTING, Event.NAK): Cell(action_nak, State.INIT),
    (State.BOUND, Event.T1): Cell(action_send_renew, State.RENEWING),
    (State.BOUND, Event.LEASE_EXPIRED): Cell(action_lease_dead, State.INIT),
    (State.RENEWING, Event.ACK): Cell(action_renewed, State.BOUND),
    (State.RENEWING, Event.NAK): Cell(action_nak, State.INIT),
    (State.RENEWING, Event.T2): Cell(action_omit_rebinding, State.INIT),
    (State.RENEWING, Event.LEASE_EXPIRED): Cell(action_lease_dead, State.INIT),
}


def fsm_init(fsm: Fsm) -> None:
    """put the teaching DHCP FSM into INIT"""
    fsm.state = State.INIT


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
    dhcp = Fsm()
    print("=== dhcp teaching FSM ===")

    print("-- acquire lease --")
    fsm_init(dhcp)
    print(f"init: {dhcp.state.name}")
    fsm_dispatch(dhcp, Event.DISCOVER_SENT)
    fsm_dispatch(dhcp, Event.OFFER)
    fsm_dispatch(dhcp, Event.REQUEST_SENT)
    fsm_dispatch(dhcp, Event.ACK)

    print("-- T1 renew then ACK --")
    fsm_dispatch(dhcp, Event.T1)
    fsm_dispatch(dhcp, Event.ACK)

    print("-- NAK restarts --")
    fsm_init(dhcp)
    fsm_dispatch(dhcp, Event.DISCOVER_SENT)
    fsm_dispatch(dhcp, Event.OFFER)
    fsm_dispatch(dhcp, Event.NAK)

    print("-- T2 omits REBINDING --")
    fsm_init(dhcp)
    fsm_dispatch(dhcp, Event.DISCOVER_SENT)
    fsm_dispatch(dhcp, Event.OFFER)
    fsm_dispatch(dhcp, Event.ACK)
    fsm_dispatch(dhcp, Event.T1)
    fsm_dispatch(dhcp, Event.T2)

    print("-- lease expired from BOUND --")
    fsm_init(dhcp)
    fsm_dispatch(dhcp, Event.DISCOVER_SENT)
    fsm_dispatch(dhcp, Event.OFFER)
    fsm_dispatch(dhcp, Event.ACK)
    fsm_dispatch(dhcp, Event.LEASE_EXPIRED)

    print("-- illegal input is a missing table cell --")
    fsm_init(dhcp)
    fsm_dispatch(dhcp, Event.ACK)


if __name__ == "__main__":
    main()
