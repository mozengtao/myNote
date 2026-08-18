#include <cstdio>

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
	DataOk
};

struct Fsm {
	State state;
};

/*
 * return a printable leaf name
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
 * return a printable event name
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
	return "PLAY";
}

/*
 * true if the leaf sits inside the ACTIVE composite
 */
static int
in_active(State state)
{
	return state != State::Stopped;
}

/*
 * print ACTIVE/leaf or the lone STOPPED name
 */
static void
print_path(State state)
{
	if (in_active(state))
		std::printf("ACTIVE/%s", state_name(state));
	else
		std::printf("%s", state_name(state));
}

/*
 * show the flat names we refuse to implement
 */
static void
print_flat_explosion(void)
{
	std::printf("=== flat explosion (do not implement) ===\n");
	std::printf("STOPPED\n");
	std::printf("CONNECTING_RETRY1\n");
	std::printf("CONNECTING_RETRY2\n");
	std::printf("CONNECTING_RETRY3\n");
	std::printf("PLAYING_HEALTHY\n");
	std::printf("PLAYING_BUFFERING\n");
	std::printf("PLAYING_SEEKING\n");
	std::printf("STOP must be drawn from every box above\n");
	std::printf("=== nested HSM ===\n");
	std::printf("Player\n");
	std::printf("  STOPPED\n");
	std::printf("  ACTIVE (composite)\n");
	std::printf("    CONNECTING\n");
	std::printf("    PLAYING\n");
	std::printf("    BUFFERING\n");
	std::printf("STOP is one arrow on ACTIVE\n");
}

/*
 * put the player into Stopped
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Stopped;
}

/*
 * dispatch one event; STOP is handled once for the ACTIVE parent
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;

	prev = fsm->state;

	if (event == Event::Stop && in_active(fsm->state)) {
		std::printf("ACTIVE --STOP--> STOPPED (one arrow on parent)\n");
		fsm->state = State::Stopped;
		return;
	}

	if (fsm->state == State::Stopped && event == Event::Play)
		fsm->state = State::Connecting;
	else if (fsm->state == State::Connecting && event == Event::Connected)
		fsm->state = State::Playing;
	else if (fsm->state == State::Playing && event == Event::DataStarve)
		fsm->state = State::Buffering;
	else if (fsm->state == State::Buffering && event == Event::DataOk)
		fsm->state = State::Playing;

	if (fsm->state != prev) {
		print_path(prev);
		std::printf(" --%s--> ", event_name(event));
		print_path(fsm->state);
		std::printf("\n");
	}
}

int
main()
{
	Fsm player{};

	print_flat_explosion();
	std::printf("=== player composite demo ===\n");
	fsm_init(&player);
	std::printf("init: ");
	print_path(player.state);
	std::printf("\n");

	std::printf("-- play, connect, starve, recover, stop from PLAYING --\n");
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Connected);
	fsm_dispatch(&player, Event::DataStarve);
	fsm_dispatch(&player, Event::DataOk);
	fsm_dispatch(&player, Event::Stop);

	std::printf("-- play, then STOP from CONNECTING (same parent arrow) --\n");
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Stop);

	return 0;
}
