#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class Outer(Enum):
    STOPPED = auto()
    CONNECTING = auto()
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
    CONNECTED = auto()
    DATA_STARVE = auto()
    DATA_OK = auto()
    WIFI_DROP = auto()
    WIFI_RECOVER = auto()


@dataclass
class Fsm:
    outer: Outer = Outer.STOPPED
    play: PlayRegion = PlayRegion.NONE
    net: NetRegion = NetRegion.NONE
    cached: int = 0
    started: int = 0


def print_where(fsm: Fsm) -> None:
    """print the current stay-able configuration"""
    if fsm.outer is Outer.STOPPED:
        print("now: STOPPED")
    elif fsm.outer is Outer.CONNECTING:
        print("now: CONNECTING")
    else:
        print(f"now: ACTIVE | play={fsm.play.name} net={fsm.net.name}")


def maybe_join(fsm: Fsm) -> None:
    """Join: announce STARTED only when both regions are ready"""
    if fsm.outer is not Outer.ACTIVE or fsm.started:
        return
    if fsm.play is PlayRegion.PLAYING and fsm.net is NetRegion.WIFI_OK:
        print("join: STARTED (both regions ready)")
        fsm.started = 1


def fork_regions(fsm: Fsm, play: PlayRegion, net: NetRegion) -> None:
    """Fork: enter ACTIVE and light both regions in one step"""
    print(f"fork: play={play.name} net={net.name}")
    print("entry ACTIVE")
    print(f"entry {play.name}")
    print(f"entry {net.name}")
    fsm.outer = Outer.ACTIVE
    fsm.play = play
    fsm.net = net
    maybe_join(fsm)


def to_stopped(fsm: Fsm) -> None:
    """Junction target: several STOP arrows meet here, no wait"""
    print("junction: STOPPED")
    if fsm.outer is Outer.ACTIVE:
        print(f"exit {fsm.play.name}")
        print(f"exit {fsm.net.name}")
        print("exit ACTIVE")
    elif fsm.outer is Outer.CONNECTING:
        print("exit CONNECTING")
    fsm.outer = Outer.STOPPED
    fsm.play = PlayRegion.NONE
    fsm.net = NetRegion.NONE
    fsm.started = 0
    print("entry STOPPED")


def choice_on_play(fsm: Fsm) -> None:
    """Choice: evaluate cached at the moment PLAY arrives"""
    print(f"choice: cached={fsm.cached}")
    print("exit STOPPED")
    if fsm.cached:
        fork_regions(fsm, PlayRegion.PLAYING, NetRegion.WIFI_OK)
    else:
        print("entry CONNECTING")
        fsm.outer = Outer.CONNECTING


def fsm_init(fsm: Fsm, cached: int) -> None:
    """put the player into STOPPED with a cache flag"""
    fsm.outer = Outer.STOPPED
    fsm.play = PlayRegion.NONE
    fsm.net = NetRegion.NONE
    fsm.cached = cached
    fsm.started = 0
    print(f"entry STOPPED (cached={cached})")


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event; pseudostates are not stay-able states"""
    if event is Event.PLAY and fsm.outer is Outer.STOPPED:
        choice_on_play(fsm)
        print_where(fsm)
        return
    if event is Event.STOP and fsm.outer is not Outer.STOPPED:
        to_stopped(fsm)
        print_where(fsm)
        return
    if event is Event.CONNECTED and fsm.outer is Outer.CONNECTING:
        print("exit CONNECTING")
        fork_regions(fsm, PlayRegion.PLAYING, NetRegion.WIFI_WEAK)
        print_where(fsm)
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
        maybe_join(fsm)
    elif event is Event.WIFI_DROP and fsm.net is NetRegion.WIFI_OK:
        print("exit WIFI_OK")
        print("entry WIFI_WEAK")
        fsm.net = NetRegion.WIFI_WEAK
    elif event is Event.WIFI_RECOVER and fsm.net is NetRegion.WIFI_WEAK:
        print("exit WIFI_WEAK")
        print("entry WIFI_OK")
        fsm.net = NetRegion.WIFI_OK
        maybe_join(fsm)
    print_where(fsm)


def main() -> None:
    player = Fsm()
    print("=== player pseudostate demo ===")

    print("-- cached=0: choice -> CONNECTING, fork weak, join after recover --")
    fsm_init(player, 0)
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.CONNECTED)
    fsm_dispatch(player, Event.WIFI_RECOVER)
    fsm_dispatch(player, Event.STOP)

    print("-- cached=1: choice -> fork ready, join immediately --")
    fsm_init(player, 1)
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.STOP)

    print("-- junction also from CONNECTING --")
    fsm_init(player, 0)
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.STOP)


if __name__ == "__main__":
    main()
