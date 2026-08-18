#!/usr/bin/env python3
"""Hierarchical state machine skeleton extracted from lesson 13.

Lesson 13 does not import this module. Each sub-lesson keeps a
self-contained player so the demo runs alone. After that lesson,
copy this file into your own programs.

A state node has parent, entry, exit, and an optional history slot
name on the Hsm (see HsmState.history_attr). The current state is
always a leaf.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Optional

HsmAction = Callable[["Hsm"], None]


@dataclass
class HsmState:
    name: str
    parent: Optional["HsmState"] = None
    entry: Optional[HsmAction] = None
    exit: Optional[HsmAction] = None
    history_attr: Optional[str] = None


@dataclass
class Hsm:
    current: Optional[HsmState] = None
    history: dict[str, HsmState] = field(default_factory=dict)


def hsm_is_ancestor(ancestor: Optional[HsmState], node: Optional[HsmState]) -> bool:
    """true if ancestor is on the parent chain of node (or is node)"""
    p = node
    while p is not None:
        if p is ancestor:
            return True
        p = p.parent
    return False


def hsm_lca(a: Optional[HsmState], b: Optional[HsmState]) -> Optional[HsmState]:
    """least common ancestor of a and b, or None"""
    if a is None or b is None:
        return None
    p = a
    while p is not None:
        if hsm_is_ancestor(p, b):
            return p
        p = p.parent
    return None


def hsm_record_history(hsm: Hsm, node: HsmState) -> None:
    """record node as the last direct child of its parent"""
    if node.parent is None or node.parent.history_attr is None:
        return
    hsm.history[node.parent.history_attr] = node


def hsm_exit_to(hsm: Hsm, stop: Optional[HsmState]) -> None:
    """exit from leaf up to (but not including) stop"""
    s = hsm.current
    while s is not None and s is not stop:
        if s.exit is not None:
            s.exit(hsm)
        hsm_record_history(hsm, s)
        s = s.parent


def hsm_enter_to(
    hsm: Hsm, stop: Optional[HsmState], target: Optional[HsmState]
) -> None:
    """enter the chain from below stop down to target"""
    path: list[HsmState] = []
    s = target
    while s is not None and s is not stop:
        path.append(s)
        s = s.parent
    for node in reversed(path):
        if node.entry is not None:
            node.entry(hsm)


def hsm_init(hsm: Hsm, initial: HsmState) -> None:
    """enter the path from the root down to initial and set current"""
    hsm.current = None
    hsm_enter_to(hsm, None, initial)
    hsm.current = initial


def hsm_transition_act(
    hsm: Hsm, target: HsmState, action: Optional[HsmAction] = None
) -> None:
    """external transition with a transition action between exit and enter"""
    lca = hsm_lca(hsm.current, target)
    hsm_exit_to(hsm, lca)
    if action is not None:
        action(hsm)
    hsm_enter_to(hsm, lca, target)
    hsm.current = target


def hsm_transition(hsm: Hsm, target: HsmState) -> None:
    """external transition: exit to LCA, then enter to target"""
    hsm_transition_act(hsm, target, None)


def hsm_internal(hsm: Hsm, action: Optional[HsmAction]) -> None:
    """internal transition: run action, stay in current, no entry/exit"""
    if action is not None:
        action(hsm)


def hsm_shallow_history(hsm: Hsm, composite: HsmState) -> Optional[HsmState]:
    """last direct child of a composite, or None if never visited"""
    if composite.history_attr is None:
        return None
    return hsm.history.get(composite.history_attr)


def hsm_deep_history(hsm: Hsm, composite: HsmState) -> Optional[HsmState]:
    """walk history slots down to a leaf, or None if none recorded"""
    s = hsm_shallow_history(hsm, composite)
    if s is None:
        return None
    while s.history_attr is not None and s.history_attr in hsm.history:
        s = hsm.history[s.history_attr]
    return s
