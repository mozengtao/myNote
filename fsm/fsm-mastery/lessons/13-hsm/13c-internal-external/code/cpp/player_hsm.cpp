#include <cstdio>

enum class State {
	None,
	Stopped,
	Active,
	Connecting,
	Playing,
	Buffering
};

enum class Event {
	Play,
	Stop,
	Connected,
	DataStarve,
	DataOk,
	Volume
};

struct Fsm;

using TransAction = void (*)(Fsm *);

struct Fsm {
	State state;
	int volume;
};

/*
 * parent of a node; Active and Stopped have none
 */
static State
parent_of(State state)
{
	if (state == State::Connecting || state == State::Playing ||
	    state == State::Buffering)
		return State::Active;
	return State::None;
}

/*
 * true if ancestor is on the parent chain of node
 */
static int
is_ancestor(State ancestor, State node)
{
	State p;

	if (ancestor == State::None)
		return 0;
	for (p = node; p != State::None; p = parent_of(p)) {
		if (p == ancestor)
			return 1;
	}
	return 0;
}

/*
 * least common ancestor, or None
 */
static State
lca_of(State a, State b)
{
	State p;

	for (p = a; p != State::None; p = parent_of(p)) {
		if (is_ancestor(p, b))
			return p;
	}
	return State::None;
}

/*
 * run the entry action of one state
 */
static void
run_entry(State state)
{
	if (state == State::Connecting)
		std::printf("entry CONNECTING: open socket\n");
	else if (state == State::Playing)
		std::printf("entry PLAYING: start rendering\n");
	else if (state == State::Buffering)
		std::printf("entry BUFFERING\n");
	else if (state == State::Active)
		std::printf("entry ACTIVE\n");
	else if (state == State::Stopped)
		std::printf("entry STOPPED\n");
}

/*
 * run the exit action of one state
 */
static void
run_exit(State state)
{
	if (state == State::Connecting)
		std::printf("exit CONNECTING: close socket attempt\n");
	else if (state == State::Playing)
		std::printf("exit PLAYING: stop rendering\n");
	else if (state == State::Buffering)
		std::printf("exit BUFFERING\n");
	else if (state == State::Active)
		std::printf("exit ACTIVE\n");
	else if (state == State::Stopped)
		std::printf("exit STOPPED\n");
}

/*
 * exit from leaf up to (not including) stop
 */
static void
exit_up(State from, State stop)
{
	State s;

	for (s = from; s != State::None && s != stop; s = parent_of(s))
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
	for (s = target; s != State::None && s != stop; s = parent_of(s))
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
	if (action != nullptr)
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
	if (action != nullptr)
		action(fsm);
}

/*
 * raise volume without leaving Playing
 */
static void
action_volume(Fsm *fsm)
{
	fsm->volume++;
	std::printf("internal VOLUME: volume=%d (no exit/enter)\n",
	    fsm->volume);
}

static void
action_start_session(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: start session\n");
}

static void
action_wait_data(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: wait data\n");
}

static void
action_resume(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: resume\n");
}

static void
action_stop_session(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: stop session\n");
}

/*
 * put the player into Stopped
 */
void
fsm_init(Fsm *fsm)
{
	fsm->volume = 0;
	fsm->state = State::None;
	enter_down(State::None, State::Stopped);
	fsm->state = State::Stopped;
}

/*
 * dispatch one event; Volume is internal, Stop and DataStarve are external
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == Event::Volume) {
		if (fsm->state == State::Playing)
			fsm_internal(fsm, action_volume);
		return;
	}
	if (event == Event::Stop && parent_of(fsm->state) == State::Active) {
		fsm_transition(fsm, State::Stopped, action_stop_session);
		return;
	}
	if (fsm->state == State::Stopped && event == Event::Play)
		fsm_transition(fsm, State::Connecting, action_start_session);
	else if (fsm->state == State::Connecting && event == Event::Connected)
		fsm_transition(fsm, State::Playing, nullptr);
	else if (fsm->state == State::Playing && event == Event::DataStarve)
		fsm_transition(fsm, State::Buffering, action_wait_data);
	else if (fsm->state == State::Buffering && event == Event::DataOk)
		fsm_transition(fsm, State::Playing, action_resume);
}

int
main()
{
	Fsm player{};

	std::printf("=== player internal vs external demo ===\n");
	fsm_init(&player);

	std::printf("-- PLAY, CONNECTED, two VOLUME (internal), DATA_STARVE, STOP --\n");
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Connected);
	fsm_dispatch(&player, Event::Volume);
	fsm_dispatch(&player, Event::Volume);
	fsm_dispatch(&player, Event::DataStarve);
	fsm_dispatch(&player, Event::Stop);

	return 0;
}
