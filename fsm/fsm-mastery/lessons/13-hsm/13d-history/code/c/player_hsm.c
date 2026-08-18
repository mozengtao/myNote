#include <stdio.h>

typedef enum {
	STATE_NONE = 0,
	STATE_STOPPED,
	STATE_ACTIVE,
	STATE_CONNECTING,
	STATE_PLAYING,
	STATE_NORMAL,
	STATE_SEEKING,
	STATE_BUFFERING
} State;

typedef enum {
	EVENT_PLAY = 0,
	EVENT_STOP,
	EVENT_CONNECTED,
	EVENT_DATA_STARVE,
	EVENT_DATA_OK,
	EVENT_SEEK,
	EVENT_SEEK_DONE
} Event;

typedef enum {
	HIST_NONE = 0,
	HIST_SHALLOW,
	HIST_DEEP
} HistoryMode;

typedef void (*TransAction)(void *);

typedef struct {
	State state;
	HistoryMode mode;
	State active_hist;
	State playing_hist;
} Fsm;

/*
 * return a printable state name
 */
static const char *
state_name(State state)
{
	if (state == STATE_ACTIVE)
		return "ACTIVE";
	if (state == STATE_CONNECTING)
		return "CONNECTING";
	if (state == STATE_PLAYING)
		return "PLAYING";
	if (state == STATE_NORMAL)
		return "NORMAL";
	if (state == STATE_SEEKING)
		return "SEEKING";
	if (state == STATE_BUFFERING)
		return "BUFFERING";
	if (state == STATE_STOPPED)
		return "STOPPED";
	return "NONE";
}

/*
 * return a printable history mode
 */
static const char *
mode_name(HistoryMode mode)
{
	if (mode == HIST_SHALLOW)
		return "H";
	if (mode == HIST_DEEP)
		return "H*";
	return "none";
}

/*
 * parent of a node
 */
static State
parent_of(State state)
{
	if (state == STATE_CONNECTING || state == STATE_PLAYING ||
	    state == STATE_BUFFERING)
		return STATE_ACTIVE;
	if (state == STATE_NORMAL || state == STATE_SEEKING)
		return STATE_PLAYING;
	return STATE_NONE;
}

/*
 * print the leaf path including composites
 */
static void
print_path(State state)
{
	if (state == STATE_NORMAL || state == STATE_SEEKING)
		printf("ACTIVE/PLAYING/%s", state_name(state));
	else if (state == STATE_CONNECTING || state == STATE_BUFFERING)
		printf("ACTIVE/%s", state_name(state));
	else
		printf("%s", state_name(state));
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
	printf("entry %s\n", state_name(state));
}

/*
 * run the exit action of one state
 */
static void
run_exit(State state)
{
	printf("exit %s\n", state_name(state));
}

/*
 * remember the last child of each composite
 */
static void
record_history(Fsm *fsm, State leaving)
{
	State p;

	p = parent_of(leaving);
	if (p == STATE_PLAYING)
		fsm->playing_hist = leaving;
	if (p == STATE_ACTIVE)
		fsm->active_hist = leaving;
	if (leaving == STATE_PLAYING)
		fsm->active_hist = STATE_PLAYING;
}

/*
 * exit from leaf up to (not including) stop
 */
static void
exit_up(Fsm *fsm, State from, State stop)
{
	State s;

	for (s = from; s != STATE_NONE && s != stop; s = parent_of(s)) {
		run_exit(s);
		record_history(fsm, s);
	}
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
	exit_up(fsm, fsm->state, lca);
	if (action != 0)
		action(fsm);
	enter_down(lca, target);
	fsm->state = target;
}

/*
 * pick the PLAY resume leaf from history slots
 */
static State
resume_leaf(const Fsm *fsm)
{
	if (fsm->mode == HIST_NONE || fsm->active_hist == STATE_NONE)
		return STATE_CONNECTING;
	if (fsm->active_hist == STATE_PLAYING) {
		if (fsm->mode == HIST_SHALLOW)
			return STATE_NORMAL;
		if (fsm->playing_hist != STATE_NONE)
			return fsm->playing_hist;
		return STATE_NORMAL;
	}
	return fsm->active_hist;
}

/*
 * true if the leaf sits inside ACTIVE
 */
static int
in_active(State state)
{
	return parent_of(state) == STATE_ACTIVE ||
	    parent_of(parent_of(state)) == STATE_ACTIVE;
}

/*
 * put the player into STOPPED
 */
void
fsm_init(Fsm *fsm, HistoryMode mode)
{
	fsm->mode = mode;
	fsm->active_hist = STATE_NONE;
	fsm->playing_hist = STATE_NONE;
	fsm->state = STATE_NONE;
	enter_down(STATE_NONE, STATE_STOPPED);
	fsm->state = STATE_STOPPED;
}

/*
 * dispatch one event; PLAY resumes using the history mode
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == EVENT_STOP && in_active(fsm->state)) {
		fsm_transition(fsm, STATE_STOPPED, 0);
		printf("history slots: active=%s playing=%s\n",
		    state_name(fsm->active_hist),
		    state_name(fsm->playing_hist));
		return;
	}
	if (fsm->state == STATE_STOPPED && event == EVENT_PLAY) {
		State target;

		target = resume_leaf(fsm);
		printf("PLAY resume mode=%s -> ", mode_name(fsm->mode));
		print_path(target);
		printf("\n");
		fsm_transition(fsm, target, 0);
		return;
	}
	if (fsm->state == STATE_CONNECTING && event == EVENT_CONNECTED)
		fsm_transition(fsm, STATE_NORMAL, 0);
	else if (fsm->state == STATE_NORMAL && event == EVENT_SEEK)
		fsm_transition(fsm, STATE_SEEKING, 0);
	else if (fsm->state == STATE_SEEKING && event == EVENT_SEEK_DONE)
		fsm_transition(fsm, STATE_NORMAL, 0);
	else if ((fsm->state == STATE_NORMAL || fsm->state == STATE_SEEKING) &&
	    event == EVENT_DATA_STARVE)
		fsm_transition(fsm, STATE_BUFFERING, 0);
	else if (fsm->state == STATE_BUFFERING && event == EVENT_DATA_OK)
		fsm_transition(fsm, STATE_NORMAL, 0);
}

/*
 * run PLAY, CONNECTED, SEEK, STOP, PLAY under one history mode
 */
static void
run_seek_stop_play(HistoryMode mode)
{
	Fsm player;

	printf("=== history mode %s ===\n", mode_name(mode));
	fsm_init(&player, mode);
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_CONNECTED);
	fsm_dispatch(&player, EVENT_SEEK);
	printf("now at ");
	print_path(player.state);
	printf("\n");
	fsm_dispatch(&player, EVENT_STOP);
	fsm_dispatch(&player, EVENT_PLAY);
	printf("resumed at ");
	print_path(player.state);
	printf("\n");
}

int
main(void)
{
	printf("=== player history demo ===\n");
	run_seek_stop_play(HIST_NONE);
	run_seek_stop_play(HIST_SHALLOW);
	run_seek_stop_play(HIST_DEEP);
	return 0;
}
