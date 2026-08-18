#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional


class IsmState(Enum):
    DOWN = auto()
    POINT_TO_POINT = auto()
    WAITING = auto()
    DR = auto()


class IsmEvent(Enum):
    IF_UP = auto()
    IF_UP_BCAST = auto()
    IF_DOWN = auto()
    WAIT_TIMER = auto()
    BACKUP_SEEN = auto()


class NsmState(Enum):
    DOWN = auto()
    INIT = auto()
    TWOWAY = auto()
    EXSTART = auto()
    FULL = auto()


class NsmEvent(Enum):
    HELLO_RCVD = auto()
    TWOWAY_RCVD = auto()
    ADJ_OK = auto()
    NEGOTIATION_DONE = auto()
    KILL_NBR = auto()


@dataclass
class Ospf:
    ism: IsmState = IsmState.DOWN
    nsm: NsmState = NsmState.DOWN


IsmAction = Callable[["Ospf"], None]
NsmAction = Callable[["Ospf"], None]


@dataclass
class IsmCell:
    action: Optional[IsmAction]
    next: IsmState


@dataclass
class NsmCell:
    action: Optional[NsmAction]
    next: NsmState


def action_ism_up_p2p(o: Ospf) -> None:
    """bring a point-to-point interface up"""
    print("action: interface up (point-to-point)")


def action_ism_up_bcast(o: Ospf) -> None:
    """bring a broadcast interface up; wait to elect DR"""
    print("action: interface up (broadcast), start Wait timer")


def action_ism_become_dr(o: Ospf) -> None:
    """stub DR election: declare ourselves DR"""
    print("action: WaitTimer / become DR (election stub)")


def action_ism_down(o: Ospf) -> None:
    """interface down: tell the neighbor machine to die"""
    print("action: interface down, post KILL_NBR to NSM")
    nsm_dispatch(o, NsmEvent.KILL_NBR)


def action_nsm_hello(o: Ospf) -> None:
    """neighbor heard a Hello"""
    print("action: neighbor seen in Hello")


def action_nsm_twoway(o: Ospf) -> None:
    """two-way connectivity"""
    print("action: two-way Hello")


def action_nsm_adj_ok(o: Ospf) -> None:
    """adjacency is allowed; start master/slave negotiation stub"""
    print("action: AdjOK, start DD negotiation stub")


def action_nsm_full(o: Ospf) -> None:
    """skip Exchange/Loading; go Full"""
    print("action: negotiation done (skip Exchange/Loading)")


def action_nsm_kill(o: Ospf) -> None:
    """neighbor association torn down"""
    print("action: KillNbr, neighbor DOWN")


ism_table: dict[tuple[IsmState, IsmEvent], IsmCell] = {
    (IsmState.DOWN, IsmEvent.IF_UP): IsmCell(
        action_ism_up_p2p, IsmState.POINT_TO_POINT
    ),
    (IsmState.DOWN, IsmEvent.IF_UP_BCAST): IsmCell(
        action_ism_up_bcast, IsmState.WAITING
    ),
    (IsmState.POINT_TO_POINT, IsmEvent.IF_DOWN): IsmCell(
        action_ism_down, IsmState.DOWN
    ),
    (IsmState.WAITING, IsmEvent.WAIT_TIMER): IsmCell(
        action_ism_become_dr, IsmState.DR
    ),
    (IsmState.WAITING, IsmEvent.BACKUP_SEEN): IsmCell(
        action_ism_become_dr, IsmState.DR
    ),
    (IsmState.WAITING, IsmEvent.IF_DOWN): IsmCell(
        action_ism_down, IsmState.DOWN
    ),
    (IsmState.DR, IsmEvent.IF_DOWN): IsmCell(action_ism_down, IsmState.DOWN),
}

nsm_table: dict[tuple[NsmState, NsmEvent], NsmCell] = {
    (NsmState.DOWN, NsmEvent.HELLO_RCVD): NsmCell(
        action_nsm_hello, NsmState.INIT
    ),
    (NsmState.INIT, NsmEvent.HELLO_RCVD): NsmCell(
        action_nsm_hello, NsmState.INIT
    ),
    (NsmState.INIT, NsmEvent.TWOWAY_RCVD): NsmCell(
        action_nsm_twoway, NsmState.TWOWAY
    ),
    (NsmState.TWOWAY, NsmEvent.ADJ_OK): NsmCell(
        action_nsm_adj_ok, NsmState.EXSTART
    ),
    (NsmState.EXSTART, NsmEvent.NEGOTIATION_DONE): NsmCell(
        action_nsm_full, NsmState.FULL
    ),
    (NsmState.DOWN, NsmEvent.KILL_NBR): NsmCell(
        action_nsm_kill, NsmState.DOWN
    ),
    (NsmState.INIT, NsmEvent.KILL_NBR): NsmCell(
        action_nsm_kill, NsmState.DOWN
    ),
    (NsmState.TWOWAY, NsmEvent.KILL_NBR): NsmCell(
        action_nsm_kill, NsmState.DOWN
    ),
    (NsmState.EXSTART, NsmEvent.KILL_NBR): NsmCell(
        action_nsm_kill, NsmState.DOWN
    ),
    (NsmState.FULL, NsmEvent.KILL_NBR): NsmCell(
        action_nsm_kill, NsmState.DOWN
    ),
}


def fsm_init(o: Ospf) -> None:
    """reset both teaching machines"""
    o.ism = IsmState.DOWN
    o.nsm = NsmState.DOWN


def ism_dispatch(o: Ospf, event: IsmEvent) -> None:
    """dispatch one ISM event by table lookup"""
    prev = o.ism
    cell = ism_table.get((o.ism, event))
    if cell is None:
        print(f"ISM ignore: {prev.name} + {event.name}")
        return
    if cell.action is not None:
        cell.action(o)
    o.ism = cell.next
    print(f"ISM: {prev.name} --{event.name}--> {o.ism.name}")


def nsm_dispatch(o: Ospf, event: NsmEvent) -> None:
    """dispatch one NSM event by table lookup"""
    prev = o.nsm
    cell = nsm_table.get((o.nsm, event))
    if cell is None:
        print(f"NSM ignore: {prev.name} + {event.name}")
        return
    if cell.action is not None:
        cell.action(o)
    o.nsm = cell.next
    print(f"NSM: {prev.name} --{event.name}--> {o.nsm.name}")


def fsm_dispatch_hello(o: Ospf) -> None:
    """Hello arrives from the wire; NSM only runs if the interface is up"""
    if o.ism is IsmState.DOWN:
        print("drop Hello: ISM is DOWN")
        return
    nsm_dispatch(o, NsmEvent.HELLO_RCVD)


def main() -> None:
    ospf = Ospf()
    print("=== ospf teaching FSM ===")

    print("-- p2p interface, one neighbor to FULL, then kill --")
    fsm_init(ospf)
    print(f"init: ISM={ospf.ism.name} NSM={ospf.nsm.name}")
    ism_dispatch(ospf, IsmEvent.IF_UP)
    fsm_dispatch_hello(ospf)
    nsm_dispatch(ospf, NsmEvent.TWOWAY_RCVD)
    nsm_dispatch(ospf, NsmEvent.ADJ_OK)
    nsm_dispatch(ospf, NsmEvent.NEGOTIATION_DONE)
    ism_dispatch(ospf, IsmEvent.IF_DOWN)

    print("-- broadcast Wait timer stub elects DR --")
    fsm_init(ospf)
    print(f"init: ISM={ospf.ism.name} NSM={ospf.nsm.name}")
    ism_dispatch(ospf, IsmEvent.IF_UP_BCAST)
    ism_dispatch(ospf, IsmEvent.WAIT_TIMER)

    print("-- Hello while ISM is DOWN is dropped --")
    fsm_init(ospf)
    fsm_dispatch_hello(ospf)


if __name__ == "__main__":
    main()
