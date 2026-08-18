#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class Outer(Enum):
    STOPPED = auto()
    ACTIVE = auto()


class PlayRegion(Enum):
    NONE = auto()
    PLAYING = auto()
    BUFFERING = auto()


class NetRegion(Enum):
    NONE = auto()
    WIFI_OK = auto()
    WIFI_WEAK = auto()


class Event(Enum):
    PLAY = auto()
    STOP = auto()
    DATA_STARVE = auto()
    DATA_OK = auto()
    WIFI_DROP = auto()
    WIFI_RECOVER = auto()


@dataclass
class Fsm:
    outer: Outer = Outer.STOPPED
    play: PlayRegion = PlayRegion.NONE
    net: NetRegion = NetRegion.NONE


def print_pair(fsm: Fsm) -> None:
    """print the outer state and both regions"""
    if fsm.outer is Outer.STOPPED:
        print("STOPPED")
    else:
        print(f"ACTIVE | play={fsm.play.name} net={fsm.net.name}")


def enter_active(fsm: Fsm) -> None:
    """enter ACTIVE and start both regions at their defaults"""
    print("entry ACTIVE")
    print("entry PLAYING")
    print("entry WIFI_OK")
    fsm.outer = Outer.ACTIVE
    fsm.play = PlayRegion.PLAYING
    fsm.net = NetRegion.WIFI_OK


def exit_active(fsm: Fsm) -> None:
    """leave ACTIVE; both regions end together"""
    print(f"exit {fsm.play.name}")
    print(f"exit {fsm.net.name}")
    print("exit ACTIVE")
    fsm.outer = Outer.STOPPED
    fsm.play = PlayRegion.NONE
    fsm.net = NetRegion.NONE


def fsm_init(fsm: Fsm) -> None:
    """put the player into STOPPED"""
    fsm.outer = Outer.STOPPED
    fsm.play = PlayRegion.NONE
    fsm.net = NetRegion.NONE
    print("entry STOPPED")


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch; a region event must not reset the other region"""
    if event is Event.PLAY and fsm.outer is Outer.STOPPED:
        print("exit STOPPED")
        enter_active(fsm)
        print_pair(fsm)
        return
    if event is Event.STOP and fsm.outer is Outer.ACTIVE:
        exit_active(fsm)
        print("entry STOPPED")
        print_pair(fsm)
        return
    if fsm.outer is not Outer.ACTIVE:
        return

    if event is Event.DATA_STARVE and fsm.play is PlayRegion.PLAYING:
        print("exit PLAYING")
        print("entry BUFFERING")
        fsm.play = PlayRegion.BUFFERING
    elif event is Event.DATA_OK and fsm.play is PlayRegion.BUFFERING:
        print("exit BUFFERING")
        print("entry PLAYING")
        fsm.play = PlayRegion.PLAYING
    elif event is Event.WIFI_DROP and fsm.net is NetRegion.WIFI_OK:
        print("exit WIFI_OK")
        print("entry WIFI_WEAK")
        fsm.net = NetRegion.WIFI_WEAK
    elif event is Event.WIFI_RECOVER and fsm.net is NetRegion.WIFI_WEAK:
        print("exit WIFI_WEAK")
        print("entry WIFI_OK")
        fsm.net = NetRegion.WIFI_OK
    print_pair(fsm)


def main() -> None:
    player = Fsm()
    print("=== player orthogonal demo ===")
    print("one object, two regions; not two FSM objects (lesson 10)")
    fsm_init(player)
    print_pair(player)

    print("-- PLAY, starve (net stays), wifi drop (play stays) --")
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.DATA_STARVE)
    fsm_dispatch(player, Event.WIFI_DROP)
    fsm_dispatch(player, Event.DATA_OK)
    fsm_dispatch(player, Event.WIFI_RECOVER)
    fsm_dispatch(player, Event.STOP)


if __name__ == "__main__":
    main()
