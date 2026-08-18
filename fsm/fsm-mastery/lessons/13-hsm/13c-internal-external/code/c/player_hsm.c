#include <stdio.h>

typedef enum {
	STATE_NONE = 0,
	STATE_STOPPED,
	STATE_ACTIVE,
	STATE_CONNECTING,
	STATE_PLAYING,
	STATE_BUFFERING
} State;

typedef enum {
	EVENT_PLAY = 0,
	EVENT_STOP,
	EVENT_CONNECTED,
	EVENT_DATA_STARVE,
	EVENT_DATA_OK,
	EVENT_VOLUME
} Event;

typedef void (*TransAction)(void *);

typedef struct {
	State state;
	int volume;
} Fsm;

/*
 * parent of a node; ACTIVE and STOPPED have none
 */
static State
parent_of(State state)
{
	if (state == STATE_CONNECTING || state == STATE_PLAYING ||
	    state == STATE_BUFFERING)
		return STATE_ACTIVE;
	return STATE_NONE;
}

/*
 * true if ancestor is on the parent chain of node
 */
static int
is_ancestor(State ancestor, State node)
{
	State p;

	if (ancestor == STATE_NONE)
		return 0;
	for (p = node; p != STATE_NONE; p = parent_of(p)) {
		if (p == ancestor)
			return 1;
	}
	return 0;
}

/*
 * least common ancestor, or NONE
 */
static State
lca_of(State a, State b)
{
	State p;

	for (p = a; p != STATE_NONE; p = parent_of(p)) {
		if (is_ancestor(p, b))
			return p;
	}
	return STATE_NONE;
}

/*
 * run the entry action of one state
 */
static void
run_entry(State state)
{
	if (state == STATE_CONNECTING)
		printf("entry CONNECTING: open socket\n");
	else if (state == STATE_PLAYING)
		printf("entry PLAYING: start rendering\n");
	else if (state == STATE_BUFFERING)
		printf("entry BUFFERING\n");
	else if (state == STATE_ACTIVE)
		printf("entry ACTIVE\n");
	else if (state == STATE_STOPPED)
		printf("entry STOPPED\n");
}

/*
 * run the exit action of one state
 */
static void
run_exit(State state)
{
	if (state == STATE_CONNECTING)
		printf("exit CONNECTING: close socket attempt\n");
	else if (state == STATE_PLAYING)
		printf("exit PLAYING: stop rendering\n");
	else if (state == STATE_BUFFERING)
		printf("exit BUFFERING\n");
	else if (state == STATE_ACTIVE)
		printf("exit ACTIVE\n");
	else if (state == STATE_STOPPED)
		printf("exit STOPPED\n");
}

/*
 * exit from leaf up to (not including) stop
 */
static void
exit_up(State from, State stop)
{
	State s;

	for (s = from; s != STATE_NONE && s != stop; s = parent_of(s))
		run_exit(s);
}

/*
 * enter from below stop down to target
 */
static void
enter_down(State stop, State target)
{
	State path[8];
	State s;
	int n;
	int i;

	n = 0;
	for (s = target; s != STATE_NONE && s != stop; s = parent_of(s))
		path[n++] = s;
	for (i = n - 1; i >= 0; i--)
		run_entry(path[i]);
}

/*
 * external transition: exit, action, enter
 */
static void
fsm_transition(Fsm *fsm, State target, TransAction action)
{
	State lca;

	lca = lca_of(fsm->state, target);
	exit_up(fsm->state, lca);
	if (action != 0)
		action(fsm);
	enter_down(lca, target);
	fsm->state = target;
}

/*
 * internal transition: action only, no exit or enter
 */
static void
fsm_internal(Fsm *fsm, TransAction action)
{
	if (action != 0)
		action(fsm);
}

/*
 * raise volume without leaving PLAYING
 */
static void
action_volume(void *ctx)
{
	Fsm *fsm;

	fsm = ctx;
	fsm->volume++;
	printf("internal VOLUME: volume=%d (no exit/enter)\n", fsm->volume);
}

static void
action_start_session(void *ctx)
{
	(void)ctx;
	printf("action: start session\n");
}

static void
action_wait_data(void *ctx)
{
	(void)ctx;
	printf("action: wait data\n");
}

static void
action_resume(void *ctx)
{
	(void)ctx;
	printf("action: resume\n");
}

static void
action_stop_session(void *ctx)
{
	(void)ctx;
	printf("action: stop session\n");
}

/*
 * put the player into STOPPED
 */
void
fsm_init(Fsm *fsm)
{
	fsm->volume = 0;
	fsm->state = STATE_NONE;
	enter_down(STATE_NONE, STATE_STOPPED);
	fsm->state = STATE_STOPPED;
}

/*
 * dispatch one event; VOLUME is internal, STOP and DATA_STARVE are external
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == EVENT_VOLUME) {
		if (fsm->state == STATE_PLAYING)
			fsm_internal(fsm, action_volume);
		return;
	}
	if (event == EVENT_STOP && parent_of(fsm->state) == STATE_ACTIVE) {
		fsm_transition(fsm, STATE_STOPPED, action_stop_session);
		return;
	}
	if (fsm->state == STATE_STOPPED && event == EVENT_PLAY)
		fsm_transition(fsm, STATE_CONNECTING, action_start_session);
	else if (fsm->state == STATE_CONNECTING && event == EVENT_CONNECTED)
		fsm_transition(fsm, STATE_PLAYING, 0);
	else if (fsm->state == STATE_PLAYING && event == EVENT_DATA_STARVE)
		fsm_transition(fsm, STATE_BUFFERING, action_wait_data);
	else if (fsm->state == STATE_BUFFERING && event == EVENT_DATA_OK)
		fsm_transition(fsm, STATE_PLAYING, action_resume);
}

int
main(void)
{
	Fsm player;

	printf("=== player internal vs external demo ===\n");
	fsm_init(&player);

	printf("-- PLAY, CONNECTED, two VOLUME (internal), DATA_STARVE, STOP --\n");
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_CONNECTED);
	fsm_dispatch(&player, EVENT_VOLUME);
	fsm_dispatch(&player, EVENT_VOLUME);
	fsm_dispatch(&player, EVENT_DATA_STARVE);
	fsm_dispatch(&player, EVENT_STOP);

	return 0;
}
