#include "table_fsm.h"

/*
 * bind a cell array and start in initial_state
 */
void
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
void
table_fsm_dispatch(TableFsm *fsm, int event)
{
	const TableCell *cell;

	if (fsm->cells == 0)
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
