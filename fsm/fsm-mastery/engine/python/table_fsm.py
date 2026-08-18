#!/usr/bin/env python3
"""Generic table-driven FSM skeleton extracted from lesson 08.

Lesson 08 does not import this module. It keeps a protocol-specific
dict so the demo runs alone. After that lesson, copy this file into
your own programs.

cells is row-major: cells[state * nevent + event]
A cell with defined=False is ignored.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Optional

TableAction = Callable[[object], None]


@dataclass
class TableCell:
    action: Optional[TableAction]
    next_state: int
    defined: bool = True


@dataclass
class TableFsm:
    state: int = 0
    nstate: int = 0
    nevent: int = 0
    cells: tuple[TableCell, ...] | None = None
    ctx: object = None


def table_fsm_init(
    fsm: TableFsm,
    initial_state: int,
    nstate: int,
    nevent: int,
    cells: tuple[TableCell, ...],
    ctx: object,
) -> None:
    """bind a cell array and start in initial_state"""
    fsm.state = initial_state
    fsm.nstate = nstate
    fsm.nevent = nevent
    fsm.cells = cells
    fsm.ctx = ctx


def table_fsm_dispatch(fsm: TableFsm, event: int) -> None:
    """look up table[state][event], run action, move to next_state"""
    if fsm.cells is None:
        return
    if event < 0 or event >= fsm.nevent:
        return
    if fsm.state < 0 or fsm.state >= fsm.nstate:
        return
    cell = fsm.cells[fsm.state * fsm.nevent + event]
    if not cell.defined:
        return
    if cell.action is not None:
        cell.action(fsm.ctx)
    fsm.state = cell.next_state
