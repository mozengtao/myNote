#include <cstdio>

enum class State {
	None,
	Stopped,
	Active,
	Connecting,
	Playing,
	Normal,
	Seeking,
	Buffering
};

enum class Event {
	Play,
	Stop,
	Connected,
	DataStarve,
	DataOk,
	Seek,
	SeekDone
};

enum class HistoryMode {
	None,
	Shallow,
	Deep
};

struct Fsm;

using TransAction = void (*)(Fsm *);

struct Fsm {
	State state;
	HistoryMode mode;
	State active_hist;
	State playing_hist;
};

/*
 * return a printable state name
 */
static const char *
state_name(State state)
{
	if (state == State::Active)
		return "ACTIVE";
	if (state == State::Connecting)
		return "CONNECTING";
	if (state == State::Playing)
		return "PLAYING";
	if (state == State::Normal)
		return "NORMAL";
	if (state == State::Seeking)
		return "SEEKING";
	if (state == State::Buffering)
		return "BUFFERING";
	if (state == State::Stopped)
		return "STOPPED";
	return "NONE";
}

/*
 * return a printable history mode
 */
static const char *
mode_name(HistoryMode mode)
{
	if (mode == HistoryMode::Shallow)
		return "H";
	if (mode == HistoryMode::Deep)
		return "H*";
	return "none";
}

/*
 * parent of a node
 */
static State
parent_of(State state)
{
	if (state == State::Connecting || state == State::Playing ||
	    state == State::Buffering)
		return State::Active;
	if (state == State::Normal || state == State::Seeking)
		return State::Playing;
	return State::None;
}

/*
 * print the leaf path including composites
 */
static void
print_path(State state)
{
	if (state == State::Normal || state == State::Seeking)
		std::printf("ACTIVE/PLAYING/%s", state_name(state));
	else if (state == State::Connecting || state == State::Buffering)
		std::printf("ACTIVE/%s", state_name(state));
	else
		std::printf("%s", state_name(state));
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
	std::printf("entry %s\n", state_name(state));
}

/*
 * run the exit action of one state
 */
static void
run_exit(State state)
{
	std::printf("exit %s\n", state_name(state));
}

/*
 * remember the last child of each composite
 */
static void
record_history(Fsm *fsm, State leaving)
{
	State p;

	p = parent_of(leaving);
	if (p == State::Playing)
		fsm->playing_hist = leaving;
	if (p == State::Active)
		fsm->active_hist = leaving;
	if (leaving == State::Playing)
		fsm->active_hist = State::Playing;
}

/*
 * exit from leaf up to (not including) stop
 */
static void
exit_up(Fsm *fsm, State from, State stop)
{
	State s;

	for (s = from; s != State::None && s != stop; s = parent_of(s)) {
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
	exit_up(fsm, fsm->state, lca);
	if (action != nullptr)
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
	if (fsm->mode == HistoryMode::None || fsm->active_hist == State::None)
		return State::Connecting;
	if (fsm->active_hist == State::Playing) {
		if (fsm->mode == HistoryMode::Shallow)
			return State::Normal;
		if (fsm->playing_hist != State::None)
			return fsm->playing_hist;
		return State::Normal;
	}
	return fsm->active_hist;
}

/*
 * true if the leaf sits inside Active
 */
static int
in_active(State state)
{
	return parent_of(state) == State::Active ||
	    parent_of(parent_of(state)) == State::Active;
}

/*
 * put the player into Stopped
 */
void
fsm_init(Fsm *fsm, HistoryMode mode)
{
	fsm->mode = mode;
	fsm->active_hist = State::None;
	fsm->playing_hist = State::None;
	fsm->state = State::None;
	enter_down(State::None, State::Stopped);
	fsm->state = State::Stopped;
}

/*
 * dispatch one event; Play resumes using the history mode
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == Event::Stop && in_active(fsm->state)) {
		fsm_transition(fsm, State::Stopped, nullptr);
		std::printf("history slots: active=%s playing=%s\n",
		    state_name(fsm->active_hist),
		    state_name(fsm->playing_hist));
		return;
	}
	if (fsm->state == State::Stopped && event == Event::Play) {
		State target;

		target = resume_leaf(fsm);
		std::printf("PLAY resume mode=%s -> ", mode_name(fsm->mode));
		print_path(target);
		std::printf("\n");
		fsm_transition(fsm, target, nullptr);
		return;
	}
	if (fsm->state == State::Connecting && event == Event::Connected)
		fsm_transition(fsm, State::Normal, nullptr);
	else if (fsm->state == State::Normal && event == Event::Seek)
		fsm_transition(fsm, State::Seeking, nullptr);
	else if (fsm->state == State::Seeking && event == Event::SeekDone)
		fsm_transition(fsm, State::Normal, nullptr);
	else if ((fsm->state == State::Normal || fsm->state == State::Seeking) &&
	    event == Event::DataStarve)
		fsm_transition(fsm, State::Buffering, nullptr);
	else if (fsm->state == State::Buffering && event == Event::DataOk)
		fsm_transition(fsm, State::Normal, nullptr);
}

/*
 * run Play, Connected, Seek, Stop, Play under one history mode
 */
static void
run_seek_stop_play(HistoryMode mode)
{
	Fsm player{};

	std::printf("=== history mode %s ===\n", mode_name(mode));
	fsm_init(&player, mode);
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Connected);
	fsm_dispatch(&player, Event::Seek);
	std::printf("now at ");
	print_path(player.state);
	std::printf("\n");
	fsm_dispatch(&player, Event::Stop);
	fsm_dispatch(&player, Event::Play);
	std::printf("resumed at ");
	print_path(player.state);
	std::printf("\n");
}

int
main()
{
	std::printf("=== player history demo ===\n");
	run_seek_stop_play(HistoryMode::None);
	run_seek_stop_play(HistoryMode::Shallow);
	run_seek_stop_play(HistoryMode::Deep);
	return 0;
}
