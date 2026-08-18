#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class State(Enum):
    STOPPED = auto()
    CONNECTING = auto()
    PLAYING = auto()
    BUFFERING = auto()


class Event(Enum):
    PLAY = auto()
    STOP = auto()
    CONNECTED = auto()
    DATA_STARVE = auto()
    DATA_OK = auto()


@dataclass
class Fsm:
    state: State = State.STOPPED


def in_active(state: State) -> bool:
    """true if the leaf sits inside the ACTIVE composite"""
    return state is not State.STOPPED


def path_name(state: State) -> str:
    """print ACTIVE/leaf or the lone STOPPED name"""
    if in_active(state):
        return f"ACTIVE/{state.name}"
    return state.name


def print_flat_explosion() -> None:
    """show the flat names we refuse to implement"""
    print("=== flat explosion (do not implement) ===")
    print("STOPPED")
    print("CONNECTING_RETRY1")
    print("CONNECTING_RETRY2")
    print("CONNECTING_RETRY3")
    print("PLAYING_HEALTHY")
    print("PLAYING_BUFFERING")
    print("PLAYING_SEEKING")
    print("STOP must be drawn from every box above")
    print("=== nested HSM ===")
    print("Player")
    print("  STOPPED")
    print("  ACTIVE (composite)")
    print("    CONNECTING")
    print("    PLAYING")
    print("    BUFFERING")
    print("STOP is one arrow on ACTIVE")


def fsm_init(fsm: Fsm) -> None:
    """put the player into STOPPED"""
    fsm.state = State.STOPPED


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event; STOP is handled once for the ACTIVE parent"""
    prev = fsm.state

    if event is Event.STOP and in_active(fsm.state):
        print("ACTIVE --STOP--> STOPPED (one arrow on parent)")
        fsm.state = State.STOPPED
        return

    if fsm.state is State.STOPPED and event is Event.PLAY:
        fsm.state = State.CONNECTING
    elif fsm.state is State.CONNECTING and event is Event.CONNECTED:
        fsm.state = State.PLAYING
    elif fsm.state is State.PLAYING and event is Event.DATA_STARVE:
        fsm.state = State.BUFFERING
    elif fsm.state is State.BUFFERING and event is Event.DATA_OK:
        fsm.state = State.PLAYING

    if fsm.state is not prev:
        print(f"{path_name(prev)} --{event.name}--> {path_name(fsm.state)}")


def main() -> None:
    player = Fsm()
    print_flat_explosion()
    print("=== player composite demo ===")
    fsm_init(player)
    print(f"init: {path_name(player.state)}")

    print("-- play, connect, starve, recover, stop from PLAYING --")
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.CONNECTED)
    fsm_dispatch(player, Event.DATA_STARVE)
    fsm_dispatch(player, Event.DATA_OK)
    fsm_dispatch(player, Event.STOP)

    print("-- play, then STOP from CONNECTING (same parent arrow) --")
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.STOP)


if __name__ == "__main__":
    main()
