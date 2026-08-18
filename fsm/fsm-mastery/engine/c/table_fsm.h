#ifndef TABLE_FSM_H
#define TABLE_FSM_H

/*
 * Generic table-driven FSM skeleton extracted from lesson 08.
 *
 * Lesson code stays self-contained: do not #include this header from
 * lessons/08-tcp. Copy the pattern, then reuse this file in your own
 * programs after you understand the 2-D lookup.
 *
 * cells is row-major: cells[state * nevent + event]
 *
 * A cell with defined == 0 is ignored (illegal or unused input).
 * action may be NULL when the transition only changes state.
 */

typedef void (*TableAction)(void *ctx);

typedef struct {
	TableAction action;
	int next_state;
	int defined;
} TableCell;

typedef struct {
	int state;
	int nstate;
	int nevent;
	const TableCell *cells;
	void *ctx;
} TableFsm;

/*
 * bind a cell array and start in initial_state
 */
void table_fsm_init(TableFsm *fsm, int initial_state, int nstate,
    int nevent, const TableCell *cells, void *ctx);

/*
 * look up table[state][event], run action, move to next_state
 */
void table_fsm_dispatch(TableFsm *fsm, int event);

#endif
