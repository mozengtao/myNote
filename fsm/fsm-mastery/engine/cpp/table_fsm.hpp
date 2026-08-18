#ifndef TABLE_FSM_HPP
#define TABLE_FSM_HPP

/*
 * Generic table-driven FSM skeleton extracted from lesson 08.
 *
 * Header-only, C++17. Lesson 08 does not #include this file; it
 * duplicates a protocol-specific table so the demo runs alone.
 *
 * cells is row-major: cells[state * nevent + event]
 */

using TableAction = void (*)(void *ctx);

struct TableCell {
	TableAction action;
	int next_state;
	int defined;
};

struct TableFsm {
	int state;
	int nstate;
	int nevent;
	const TableCell *cells;
	void *ctx;
};

/*
 * bind a cell array and start in initial_state
 */
inline void
table_fsm_init(TableFsm *fsm, int initial_state, int nstate, int nevent,
    const TableCell *cells, void *ctx)
{
	fsm->state = initial_state;
	fsm->nstate = nstate;
	fsm->nevent = nevent;
	fsm->cells = cells;
	fsm->ctx = ctx;
}

/*
 * look up table[state][event], run action, move to next_state
 */
inline void
table_fsm_dispatch(TableFsm *fsm, int event)
{
	const TableCell *cell;

	if (fsm->cells == nullptr)
		return;
	if (event < 0 || event >= fsm->nevent)
		return;
	if (fsm->state < 0 || fsm->state >= fsm->nstate)
		return;

	cell = &fsm->cells[fsm->state * fsm->nevent + event];
	if (!cell->defined)
		return;
	if (cell->action)
		cell->action(fsm->ctx);
	fsm->state = cell->next_state;
}

#endif
