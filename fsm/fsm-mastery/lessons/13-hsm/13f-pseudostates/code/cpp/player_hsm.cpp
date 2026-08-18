#include <cstdio>

enum class Outer {
	Stopped,
	Connecting,
	Active
};

enum class PlayRegion {
	None,
	Playing,
	Buffering
};

enum class NetRegion {
	None,
	WifiOk,
	WifiWeak
};

enum class Event {
	Play,
	Stop,
	Connected,
	DataStarve,
	DataOk,
	WifiDrop,
	WifiRecover
};

struct Fsm {
	Outer outer;
	PlayRegion play;
	NetRegion net;
	int cached;
	int started;
};

/*
 * return a printable play-region name
 */
static const char *
play_name(PlayRegion play)
{
	if (play == PlayRegion::Playing)
		return "PLAYING";
	if (play == PlayRegion::Buffering)
		return "BUFFERING";
	return "NONE";
}

/*
 * return a printable net-region name
 */
static const char *
net_name(NetRegion net)
{
	if (net == NetRegion::WifiOk)
		return "WIFI_OK";
	if (net == NetRegion::WifiWeak)
		return "WIFI_WEAK";
	return "NONE";
}

/*
 * print the current stay-able configuration
 */
static void
print_where(const Fsm *fsm)
{
	if (fsm->outer == Outer::Stopped)
		std::printf("now: STOPPED\n");
	else if (fsm->outer == Outer::Connecting)
		std::printf("now: CONNECTING\n");
	else
		std::printf("now: ACTIVE | play=%s net=%s\n",
		    play_name(fsm->play), net_name(fsm->net));
}

/*
 * Join: announce STARTED only when both regions are ready
 */
static void
maybe_join(Fsm *fsm)
{
	if (fsm->outer != Outer::Active || fsm->started)
		return;
	if (fsm->play == PlayRegion::Playing &&
	    fsm->net == NetRegion::WifiOk) {
		std::printf("join: STARTED (both regions ready)\n");
		fsm->started = 1;
	}
}

/*
 * Fork: enter Active and light both regions in one step
 */
static void
fork_regions(Fsm *fsm, PlayRegion play, NetRegion net)
{
	std::printf("fork: play=%s net=%s\n", play_name(play), net_name(net));
	std::printf("entry ACTIVE\n");
	std::printf("entry %s\n", play_name(play));
	std::printf("entry %s\n", net_name(net));
	fsm->outer = Outer::Active;
	fsm->play = play;
	fsm->net = net;
	maybe_join(fsm);
}

/*
 * Junction target: several STOP arrows meet here, no wait
 */
static void
to_stopped(Fsm *fsm)
{
	std::printf("junction: STOPPED\n");
	if (fsm->outer == Outer::Active) {
		std::printf("exit %s\n", play_name(fsm->play));
		std::printf("exit %s\n", net_name(fsm->net));
		std::printf("exit ACTIVE\n");
	} else if (fsm->outer == Outer::Connecting) {
		std::printf("exit CONNECTING\n");
	}
	fsm->outer = Outer::Stopped;
	fsm->play = PlayRegion::None;
	fsm->net = NetRegion::None;
	fsm->started = 0;
	std::printf("entry STOPPED\n");
}

/*
 * Choice: evaluate cached at the moment Play arrives
 */
static void
choice_on_play(Fsm *fsm)
{
	std::printf("choice: cached=%d\n", fsm->cached);
	std::printf("exit STOPPED\n");
	if (fsm->cached)
		fork_regions(fsm, PlayRegion::Playing, NetRegion::WifiOk);
	else {
		std::printf("entry CONNECTING\n");
		fsm->outer = Outer::Connecting;
	}
}

/*
 * put the player into Stopped with a cache flag
 */
void
fsm_init(Fsm *fsm, int cached)
{
	fsm->outer = Outer::Stopped;
	fsm->play = PlayRegion::None;
	fsm->net = NetRegion::None;
	fsm->cached = cached;
	fsm->started = 0;
	std::printf("entry STOPPED (cached=%d)\n", cached);
}

/*
 * dispatch one event; pseudostates are not stay-able states
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == Event::Play && fsm->outer == Outer::Stopped) {
		choice_on_play(fsm);
		print_where(fsm);
		return;
	}
	if (event == Event::Stop && fsm->outer != Outer::Stopped) {
		to_stopped(fsm);
		print_where(fsm);
		return;
	}
	if (event == Event::Connected && fsm->outer == Outer::Connecting) {
		std::printf("exit CONNECTING\n");
		fork_regions(fsm, PlayRegion::Playing, NetRegion::WifiWeak);
		print_where(fsm);
		return;
	}
	if (fsm->outer != Outer::Active)
		return;

	if (event == Event::DataStarve && fsm->play == PlayRegion::Playing) {
		std::printf("exit PLAYING\n");
		std::printf("entry BUFFERING\n");
		fsm->play = PlayRegion::Buffering;
	} else if (event == Event::DataOk &&
	    fsm->play == PlayRegion::Buffering) {
		std::printf("exit BUFFERING\n");
		std::printf("entry PLAYING\n");
		fsm->play = PlayRegion::Playing;
		maybe_join(fsm);
	} else if (event == Event::WifiDrop && fsm->net == NetRegion::WifiOk) {
		std::printf("exit WIFI_OK\n");
		std::printf("entry WIFI_WEAK\n");
		fsm->net = NetRegion::WifiWeak;
	} else if (event == Event::WifiRecover &&
	    fsm->net == NetRegion::WifiWeak) {
		std::printf("exit WIFI_WEAK\n");
		std::printf("entry WIFI_OK\n");
		fsm->net = NetRegion::WifiOk;
		maybe_join(fsm);
	}
	print_where(fsm);
}

int
main()
{
	Fsm player{};

	std::printf("=== player pseudostate demo ===\n");

	std::printf("-- cached=0: choice -> CONNECTING, fork weak, join after recover --\n");
	fsm_init(&player, 0);
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Connected);
	fsm_dispatch(&player, Event::WifiRecover);
	fsm_dispatch(&player, Event::Stop);

	std::printf("-- cached=1: choice -> fork ready, join immediately --\n");
	fsm_init(&player, 1);
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Stop);

	std::printf("-- junction also from CONNECTING --\n");
	fsm_init(&player, 0);
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::Stop);

	return 0;
}
