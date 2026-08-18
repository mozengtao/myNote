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
    NORMAL = auto()
    SEEKING = auto()
    BUFFERING = auto()


class Event(Enum):
    PLAY = auto()
    STOP = auto()
    CONNECTED = auto()
    DATA_STARVE = auto()
    DATA_OK = auto()
    SEEK = auto()
    SEEK_DONE = auto()


class HistoryMode(Enum):
    NONE = auto()
    SHALLOW = auto()
    DEEP = auto()


TransAction = Callable[["Fsm"], None]


@dataclass
class Fsm:
    state: State = State.NONE
    mode: HistoryMode = HistoryMode.NONE
    active_hist: State = State.NONE
    playing_hist: State = State.NONE


def mode_name(mode: HistoryMode) -> str:
    """return a printable history mode"""
    if mode is HistoryMode.SHALLOW:
        return "H"
    if mode is HistoryMode.DEEP:
        return "H*"
    return "none"


def parent_of(state: State) -> State:
    """parent of a node"""
    if state in (State.CONNECTING, State.PLAYING, State.BUFFERING):
        return State.ACTIVE
    if state in (State.NORMAL, State.SEEKING):
        return State.PLAYING
    return State.NONE


def path_name(state: State) -> str:
    """print the leaf path including composites"""
    if state in (State.NORMAL, State.SEEKING):
        return f"ACTIVE/PLAYING/{state.name}"
    if state in (State.CONNECTING, State.BUFFERING):
        return f"ACTIVE/{state.name}"
    return state.name


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
    print(f"entry {state.name}")


def run_exit(state: State) -> None:
    """run the exit action of one state"""
    print(f"exit {state.name}")


def record_history(fsm: Fsm, leaving: State) -> None:
    """remember the last child of each composite"""
    p = parent_of(leaving)
    if p is State.PLAYING:
        fsm.playing_hist = leaving
    if p is State.ACTIVE:
        fsm.active_hist = leaving
    if leaving is State.PLAYING:
        fsm.active_hist = State.PLAYING


def exit_up(fsm: Fsm, frm: State, stop: State) -> None:
    """exit from leaf up to (not including) stop"""
    s = frm
    while s is not State.NONE and s is not stop:
        run_exit(s)
        record_history(fsm, s)
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
    exit_up(fsm, fsm.state, lca)
    if action is not None:
        action(fsm)
    enter_down(lca, target)
    fsm.state = target


def resume_leaf(fsm: Fsm) -> State:
    """pick the PLAY resume leaf from history slots"""
    if fsm.mode is HistoryMode.NONE or fsm.active_hist is State.NONE:
        return State.CONNECTING
    if fsm.active_hist is State.PLAYING:
        if fsm.mode is HistoryMode.SHALLOW:
            return State.NORMAL
        if fsm.playing_hist is not State.NONE:
            return fsm.playing_hist
        return State.NORMAL
    return fsm.active_hist


def in_active(state: State) -> bool:
    """true if the leaf sits inside ACTIVE"""
    return parent_of(state) is State.ACTIVE or parent_of(
        parent_of(state)
    ) is State.ACTIVE


def fsm_init(fsm: Fsm, mode: HistoryMode) -> None:
    """put the player into STOPPED"""
    fsm.mode = mode
    fsm.active_hist = State.NONE
    fsm.playing_hist = State.NONE
    fsm.state = State.NONE
    enter_down(State.NONE, State.STOPPED)
    fsm.state = State.STOPPED


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event; PLAY resumes using the history mode"""
    if event is Event.STOP and in_active(fsm.state):
        fsm_transition(fsm, State.STOPPED, None)
        print(
            f"history slots: active={fsm.active_hist.name} "
            f"playing={fsm.playing_hist.name}"
        )
        return
    if fsm.state is State.STOPPED and event is Event.PLAY:
        target = resume_leaf(fsm)
        print(f"PLAY resume mode={mode_name(fsm.mode)} -> {path_name(target)}")
        fsm_transition(fsm, target, None)
        return
    if fsm.state is State.CONNECTING and event is Event.CONNECTED:
        fsm_transition(fsm, State.NORMAL, None)
    elif fsm.state is State.NORMAL and event is Event.SEEK:
        fsm_transition(fsm, State.SEEKING, None)
    elif fsm.state is State.SEEKING and event is Event.SEEK_DONE:
        fsm_transition(fsm, State.NORMAL, None)
    elif fsm.state in (State.NORMAL, State.SEEKING) and event is Event.DATA_STARVE:
        fsm_transition(fsm, State.BUFFERING, None)
    elif fsm.state is State.BUFFERING and event is Event.DATA_OK:
        fsm_transition(fsm, State.NORMAL, None)


def run_seek_stop_play(mode: HistoryMode) -> None:
    """run PLAY, CONNECTED, SEEK, STOP, PLAY under one history mode"""
    player = Fsm()
    print(f"=== history mode {mode_name(mode)} ===")
    fsm_init(player, mode)
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.CONNECTED)
    fsm_dispatch(player, Event.SEEK)
    print(f"now at {path_name(player.state)}")
    fsm_dispatch(player, Event.STOP)
    fsm_dispatch(player, Event.PLAY)
    print(f"resumed at {path_name(player.state)}")


def main() -> None:
    print("=== player history demo ===")
    run_seek_stop_play(HistoryMode.NONE)
    run_seek_stop_play(HistoryMode.SHALLOW)
    run_seek_stop_play(HistoryMode.DEEP)


if __name__ == "__main__":
    main()
