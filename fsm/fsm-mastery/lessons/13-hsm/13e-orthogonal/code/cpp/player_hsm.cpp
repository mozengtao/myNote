#include <cstdio>

enum class Outer {
	Stopped,
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
	DataStarve,
	DataOk,
	WifiDrop,
	WifiRecover
};

struct Fsm {
	Outer outer;
	PlayRegion play;
	NetRegion net;
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
 * print the outer state and both regions
 */
static void
print_pair(const Fsm *fsm)
{
	if (fsm->outer == Outer::Stopped)
		std::printf("STOPPED\n");
	else
		std::printf("ACTIVE | play=%s net=%s\n", play_name(fsm->play),
		    net_name(fsm->net));
}

/*
 * enter Active and start both regions at their defaults
 */
static void
enter_active(Fsm *fsm)
{
	std::printf("entry ACTIVE\n");
	std::printf("entry PLAYING\n");
	std::printf("entry WIFI_OK\n");
	fsm->outer = Outer::Active;
	fsm->play = PlayRegion::Playing;
	fsm->net = NetRegion::WifiOk;
}

/*
 * leave Active; both regions end together
 */
static void
exit_active(Fsm *fsm)
{
	std::printf("exit %s\n", play_name(fsm->play));
	std::printf("exit %s\n", net_name(fsm->net));
	std::printf("exit ACTIVE\n");
	fsm->outer = Outer::Stopped;
	fsm->play = PlayRegion::None;
	fsm->net = NetRegion::None;
}

/*
 * put the player into Stopped
 */
void
fsm_init(Fsm *fsm)
{
	fsm->outer = Outer::Stopped;
	fsm->play = PlayRegion::None;
	fsm->net = NetRegion::None;
	std::printf("entry STOPPED\n");
}

/*
 * dispatch one event; a region event must not reset the other region
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == Event::Play && fsm->outer == Outer::Stopped) {
		std::printf("exit STOPPED\n");
		enter_active(fsm);
		print_pair(fsm);
		return;
	}
	if (event == Event::Stop && fsm->outer == Outer::Active) {
		exit_active(fsm);
		std::printf("entry STOPPED\n");
		print_pair(fsm);
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
	} else if (event == Event::WifiDrop && fsm->net == NetRegion::WifiOk) {
		std::printf("exit WIFI_OK\n");
		std::printf("entry WIFI_WEAK\n");
		fsm->net = NetRegion::WifiWeak;
	} else if (event == Event::WifiRecover &&
	    fsm->net == NetRegion::WifiWeak) {
		std::printf("exit WIFI_WEAK\n");
		std::printf("entry WIFI_OK\n");
		fsm->net = NetRegion::WifiOk;
	}
	print_pair(fsm);
}

int
main()
{
	Fsm player{};

	std::printf("=== player orthogonal demo ===\n");
	std::printf("one object, two regions; not two FSM objects (lesson 10)\n");
	fsm_init(&player);
	print_pair(&player);

	std::printf("-- PLAY, starve (net stays), wifi drop (play stays) --\n");
	fsm_dispatch(&player, Event::Play);
	fsm_dispatch(&player, Event::DataStarve);
	fsm_dispatch(&player, Event::WifiDrop);
	fsm_dispatch(&player, Event::DataOk);
	fsm_dispatch(&player, Event::WifiRecover);
	fsm_dispatch(&player, Event::Stop);

	return 0;
}
