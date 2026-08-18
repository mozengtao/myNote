#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional


class State(Enum):
    NONE = auto()
    STOPPED = auto()
    ACTIVE = auto()
    CONNECTING = auto()
    PLAYING = auto()
    BUFFERING = auto()


class Event(Enum):
    PLAY = auto()
    STOP = auto()
    CONNECTED = auto()
    DATA_STARVE = auto()
    DATA_OK = auto()


TransAction = Callable[[], None]


@dataclass
class Fsm:
    state: State = State.NONE


def parent_of(state: State) -> State:
    """parent of a node; ACTIVE and STOPPED have none"""
    if state in (State.CONNECTING, State.PLAYING, State.BUFFERING):
        return State.ACTIVE
    return State.NONE


def is_ancestor(ancestor: State, node: State) -> bool:
    """true if ancestor is on the parent chain of node"""
    if ancestor is State.NONE:
        return False
    p = node
    while p is not State.NONE:
        if p is ancestor:
            return True
        p = parent_of(p)
    return False


def lca_of(a: State, b: State) -> State:
    """least common ancestor, or NONE"""
    p = a
    while p is not State.NONE:
        if is_ancestor(p, b):
            return p
        p = parent_of(p)
    return State.NONE


def run_entry(state: State) -> None:
    """run the entry action of one state"""
    if state is State.CONNECTING:
        print("entry CONNECTING: open socket")
    elif state is State.PLAYING:
        print("entry PLAYING: start rendering")
    elif state is State.BUFFERING:
        print("entry BUFFERING")
    elif state is State.ACTIVE:
        print("entry ACTIVE")
    elif state is State.STOPPED:
        print("entry STOPPED")


def run_exit(state: State) -> None:
    """run the exit action of one state"""
    if state is State.CONNECTING:
        print("exit CONNECTING: close socket attempt")
    elif state is State.PLAYING:
        print("exit PLAYING: stop rendering")
    elif state is State.BUFFERING:
        print("exit BUFFERING")
    elif state is State.ACTIVE:
        print("exit ACTIVE")
    elif state is State.STOPPED:
        print("exit STOPPED")


def exit_up(frm: State, stop: State) -> None:
    """exit from leaf up to (not including) stop"""
    s = frm
    while s is not State.NONE and s is not stop:
        run_exit(s)
        s = parent_of(s)


def enter_down(stop: State, target: State) -> None:
    """enter from below stop down to target"""
    path: list[State] = []
    s = target
    while s is not State.NONE and s is not stop:
        path.append(s)
        s = parent_of(s)
    for node in reversed(path):
        run_entry(node)


def fsm_transition(
    fsm: Fsm, target: State, action: Optional[TransAction]
) -> None:
    """external transition: exit, action, enter"""
    lca = lca_of(fsm.state, target)
    exit_up(fsm.state, lca)
    if action is not None:
        action()
    enter_down(lca, target)
    fsm.state = target


def action_start_session() -> None:
    print("action: start session")


def action_wait_data() -> None:
    print("action: wait data")


def action_resume() -> None:
    print("action: resume")


def action_stop_session() -> None:
    print("action: stop session")


def fsm_init(fsm: Fsm) -> None:
    """put the player into STOPPED (enter the initial leaf)"""
    fsm.state = State.NONE
    enter_down(State.NONE, State.STOPPED)
    fsm.state = State.STOPPED


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event using external transitions only"""
    if event is Event.STOP and parent_of(fsm.state) is State.ACTIVE:
        fsm_transition(fsm, State.STOPPED, action_stop_session)
        return
    if fsm.state is State.STOPPED and event is Event.PLAY:
        fsm_transition(fsm, State.CONNECTING, action_start_session)
    elif fsm.state is State.CONNECTING and event is Event.CONNECTED:
        fsm_transition(fsm, State.PLAYING, None)
    elif fsm.state is State.PLAYING and event is Event.DATA_STARVE:
        fsm_transition(fsm, State.BUFFERING, action_wait_data)
    elif fsm.state is State.BUFFERING and event is Event.DATA_OK:
        fsm_transition(fsm, State.PLAYING, action_resume)


def main() -> None:
    player = Fsm()
    print("=== player entry/exit demo ===")
    fsm_init(player)

    print("-- PLAY, CONNECTED, DATA_STARVE, DATA_OK, STOP --")
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.CONNECTED)
    fsm_dispatch(player, Event.DATA_STARVE)
    fsm_dispatch(player, Event.DATA_OK)
    fsm_dispatch(player, Event.STOP)

    print("-- PLAY then STOP from CONNECTING --")
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.STOP)


if __name__ == "__main__":
    main()
