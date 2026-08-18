#include <stdio.h>

typedef enum {
	OUTER_STOPPED = 0,
	OUTER_CONNECTING,
	OUTER_ACTIVE
} Outer;

typedef enum {
	PLAY_NONE = 0,
	PLAY_PLAYING,
	PLAY_BUFFERING
} PlayRegion;

typedef enum {
	NET_NONE = 0,
	NET_WIFI_OK,
	NET_WIFI_WEAK
} NetRegion;

typedef enum {
	EVENT_PLAY = 0,
	EVENT_STOP,
	EVENT_CONNECTED,
	EVENT_DATA_STARVE,
	EVENT_DATA_OK,
	EVENT_WIFI_DROP,
	EVENT_WIFI_RECOVER
} Event;

typedef struct {
	Outer outer;
	PlayRegion play;
	NetRegion net;
	int cached;
	int started;
} Fsm;

/*
 * return a printable play-region name
 */
static const char *
play_name(PlayRegion play)
{
	if (play == PLAY_PLAYING)
		return "PLAYING";
	if (play == PLAY_BUFFERING)
		return "BUFFERING";
	return "NONE";
}

/*
 * return a printable net-region name
 */
static const char *
net_name(NetRegion net)
{
	if (net == NET_WIFI_OK)
		return "WIFI_OK";
	if (net == NET_WIFI_WEAK)
		return "WIFI_WEAK";
	return "NONE";
}

/*
 * print the current stay-able configuration
 */
static void
print_where(const Fsm *fsm)
{
	if (fsm->outer == OUTER_STOPPED)
		printf("now: STOPPED\n");
	else if (fsm->outer == OUTER_CONNECTING)
		printf("now: CONNECTING\n");
	else
		printf("now: ACTIVE | play=%s net=%s\n", play_name(fsm->play),
		    net_name(fsm->net));
}

/*
 * Join: announce STARTED only when both regions are ready
 */
static void
maybe_join(Fsm *fsm)
{
	if (fsm->outer != OUTER_ACTIVE || fsm->started)
		return;
	if (fsm->play == PLAY_PLAYING && fsm->net == NET_WIFI_OK) {
		printf("join: STARTED (both regions ready)\n");
		fsm->started = 1;
	}
}

/*
 * Fork: enter ACTIVE and light both regions in one step
 */
static void
fork_regions(Fsm *fsm, PlayRegion play, NetRegion net)
{
	printf("fork: play=%s net=%s\n", play_name(play), net_name(net));
	printf("entry ACTIVE\n");
	printf("entry %s\n", play_name(play));
	printf("entry %s\n", net_name(net));
	fsm->outer = OUTER_ACTIVE;
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
	printf("junction: STOPPED\n");
	if (fsm->outer == OUTER_ACTIVE) {
		printf("exit %s\n", play_name(fsm->play));
		printf("exit %s\n", net_name(fsm->net));
		printf("exit ACTIVE\n");
	} else if (fsm->outer == OUTER_CONNECTING) {
		printf("exit CONNECTING\n");
	}
	fsm->outer = OUTER_STOPPED;
	fsm->play = PLAY_NONE;
	fsm->net = NET_NONE;
	fsm->started = 0;
	printf("entry STOPPED\n");
}

/*
 * Choice: evaluate cached at the moment PLAY arrives
 */
static void
choice_on_play(Fsm *fsm)
{
	printf("choice: cached=%d\n", fsm->cached);
	printf("exit STOPPED\n");
	if (fsm->cached)
		fork_regions(fsm, PLAY_PLAYING, NET_WIFI_OK);
	else {
		printf("entry CONNECTING\n");
		fsm->outer = OUTER_CONNECTING;
	}
}

/*
 * put the player into STOPPED with a cache flag
 */
void
fsm_init(Fsm *fsm, int cached)
{
	fsm->outer = OUTER_STOPPED;
	fsm->play = PLAY_NONE;
	fsm->net = NET_NONE;
	fsm->cached = cached;
	fsm->started = 0;
	printf("entry STOPPED (cached=%d)\n", cached);
}

/*
 * dispatch one event; pseudostates are not stay-able states
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == EVENT_PLAY && fsm->outer == OUTER_STOPPED) {
		choice_on_play(fsm);
		print_where(fsm);
		return;
	}
	if (event == EVENT_STOP && fsm->outer != OUTER_STOPPED) {
		to_stopped(fsm);
		print_where(fsm);
		return;
	}
	if (event == EVENT_CONNECTED && fsm->outer == OUTER_CONNECTING) {
		printf("exit CONNECTING\n");
		fork_regions(fsm, PLAY_PLAYING, NET_WIFI_WEAK);
		print_where(fsm);
		return;
	}
	if (fsm->outer != OUTER_ACTIVE)
		return;

	if (event == EVENT_DATA_STARVE && fsm->play == PLAY_PLAYING) {
		printf("exit PLAYING\n");
		printf("entry BUFFERING\n");
		fsm->play = PLAY_BUFFERING;
	} else if (event == EVENT_DATA_OK && fsm->play == PLAY_BUFFERING) {
		printf("exit BUFFERING\n");
		printf("entry PLAYING\n");
		fsm->play = PLAY_PLAYING;
		maybe_join(fsm);
	} else if (event == EVENT_WIFI_DROP && fsm->net == NET_WIFI_OK) {
		printf("exit WIFI_OK\n");
		printf("entry WIFI_WEAK\n");
		fsm->net = NET_WIFI_WEAK;
	} else if (event == EVENT_WIFI_RECOVER && fsm->net == NET_WIFI_WEAK) {
		printf("exit WIFI_WEAK\n");
		printf("entry WIFI_OK\n");
		fsm->net = NET_WIFI_OK;
		maybe_join(fsm);
	}
	print_where(fsm);
}

int
main(void)
{
	Fsm player;

	printf("=== player pseudostate demo ===\n");

	printf("-- cached=0: choice -> CONNECTING, fork weak, join after recover --\n");
	fsm_init(&player, 0);
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_CONNECTED);
	fsm_dispatch(&player, EVENT_WIFI_RECOVER);
	fsm_dispatch(&player, EVENT_STOP);

	printf("-- cached=1: choice -> fork ready, join immediately --\n");
	fsm_init(&player, 1);
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_STOP);

	printf("-- junction also from CONNECTING --\n");
	fsm_init(&player, 0);
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_STOP);

	return 0;
}
