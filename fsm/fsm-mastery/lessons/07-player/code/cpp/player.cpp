#include <cstdio>

enum {
	RetryBudget = 3
};

enum class State {
	Stopped,
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
	Timeout,
	Tick,
	GiveUp
};

struct Fsm {
	State state;
	int retries_left;
	int backoff_ticks;
	int wait_ticks;
};

/*
 * return a printable name for a player state
 */
static const char *
state_name(State state)
{
	if (state == State::Connecting)
		return "CONNECTING";
	if (state == State::Playing)
		return "PLAYING";
	if (state == State::Buffering)
		return "BUFFERING";
	return "STOPPED";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	if (event == Event::Stop)
		return "STOP";
	if (event == Event::Connected)
		return "CONNECTED";
	if (event == Event::DataStarve)
		return "DATA_STARVE";
	if (event == Event::DataOk)
		return "DATA_OK";
	if (event == Event::Timeout)
		return "TIMEOUT";
	if (event == Event::Tick)
		return "TICK";
	if (event == Event::GiveUp)
		return "GIVE_UP";
	return "PLAY";
}

/*
 * begin a connect attempt
 */
static void
action_start_connect(Fsm *fsm)
{
	std::printf("action: start connect (retries left=%d)\n",
	    fsm->retries_left);
}

/*
 * begin playback
 */
static void
action_start_play(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: start play\n");
}

/*
 * wait for more stream data
 */
static void
action_wait_data(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: wait data\n");
}

/*
 * resume playback after buffering
 */
static void
action_resume_play(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: resume play\n");
}

/*
 * stop playback and drop the session
 */
static void
action_stop(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: stop\n");
}

/*
 * clear retry budget and backoff wait
 */
static void
clear_budget(Fsm *fsm)
{
	fsm->retries_left = 0;
	fsm->backoff_ticks = 1;
	fsm->wait_ticks = 0;
}

/*
 * put the player into Stopped
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Stopped;
	clear_budget(fsm);
}

/*
 * dispatch one event into the player FSM
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;

	prev = fsm->state;

	if (event == Event::Tick && fsm->state == State::Connecting &&
	    fsm->wait_ticks > 0) {
		fsm->wait_ticks--;
		std::printf("backoff wait=%d\n", fsm->wait_ticks);
		if (fsm->wait_ticks == 0)
			std::printf("retry connect, retries left=%d\n",
			    fsm->retries_left);
		return;
	}

	if (event == Event::Timeout && fsm->state == State::Connecting) {
		if (fsm->wait_ticks > 0)
			return;
		fsm->retries_left--;
		std::printf("retries left=%d\n", fsm->retries_left);
		if (fsm->retries_left <= 0) {
			event = Event::GiveUp;
		} else {
			fsm->wait_ticks = fsm->backoff_ticks;
			std::printf("backoff wait: %d\n", fsm->wait_ticks);
			fsm->backoff_ticks *= 2;
			std::printf("%s --TIMEOUT--> %s (retries left=%d)\n",
			    state_name(prev), state_name(fsm->state),
			    fsm->retries_left);
			return;
		}
	}

	switch (fsm->state) {
	case State::Stopped:
		if (event == Event::Play) {
			fsm->retries_left = RetryBudget;
			fsm->backoff_ticks = 1;
			fsm->wait_ticks = 0;
			action_start_connect(fsm);
			fsm->state = State::Connecting;
		}
		break;
	case State::Connecting:
		if (event == Event::Connected) {
			fsm->wait_ticks = 0;
			action_start_play(fsm);
			fsm->state = State::Playing;
		} else if (event == Event::GiveUp) {
			std::printf("give up\n");
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = State::Stopped;
		} else if (event == Event::Stop) {
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = State::Stopped;
		}
		break;
	case State::Playing:
		if (event == Event::DataStarve) {
			action_wait_data(fsm);
			fsm->state = State::Buffering;
		} else if (event == Event::Stop) {
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = State::Stopped;
		}
		break;
	case State::Buffering:
		if (event == Event::DataOk) {
			action_resume_play(fsm);
			fsm->state = State::Playing;
		} else if (event == Event::Stop) {
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = State::Stopped;
		}
		break;
	}

	if (fsm->state != prev) {
		std::printf("%s --%s--> %s (retries left=%d wait=%d)\n",
		    state_name(prev), event_name(event),
		    state_name(fsm->state), fsm->retries_left,
		    fsm->wait_ticks);
	}
}

int
main()
{
	Fsm player{};

	std::printf("=== player demo ===\n");
	fsm_init(&player);
	std::printf("init: %s\n", state_name(player.state));

	std::printf("-- connect fails three times, then give up --\n");
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Timeout);
	fsm_dispatch(&player, Event::Tick);
	fsm_dispatch(&player, Event::Timeout);
	fsm_dispatch(&player, Event::Tick);
	fsm_dispatch(&player, Event::Tick);
	fsm_dispatch(&player, Event::Timeout);

	std::printf("-- connect, starve, recover, stop --\n");
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Connected);
	fsm_dispatch(&player, Event::DataStarve);
	fsm_dispatch(&player, Event::DataOk);
	fsm_dispatch(&player, Event::Stop);

	return 0;
}
