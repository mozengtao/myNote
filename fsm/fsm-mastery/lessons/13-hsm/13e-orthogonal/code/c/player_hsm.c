#include <stdio.h>

typedef enum {
	OUTER_STOPPED = 0,
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
	EVENT_DATA_STARVE,
	EVENT_DATA_OK,
	EVENT_WIFI_DROP,
	EVENT_WIFI_RECOVER
} Event;

typedef struct {
	Outer outer;
	PlayRegion play;
	NetRegion net;
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
 * print the outer state and both regions
 */
static void
print_pair(const Fsm *fsm)
{
	if (fsm->outer == OUTER_STOPPED)
		printf("STOPPED\n");
	else
		printf("ACTIVE | play=%s net=%s\n", play_name(fsm->play),
		    net_name(fsm->net));
}

/*
 * enter ACTIVE and start both regions at their defaults
 */
static void
enter_active(Fsm *fsm)
{
	printf("entry ACTIVE\n");
	printf("entry PLAYING\n");
	printf("entry WIFI_OK\n");
	fsm->outer = OUTER_ACTIVE;
	fsm->play = PLAY_PLAYING;
	fsm->net = NET_WIFI_OK;
}

/*
 * leave ACTIVE; both regions end together
 */
static void
exit_active(Fsm *fsm)
{
	printf("exit %s\n", play_name(fsm->play));
	printf("exit %s\n", net_name(fsm->net));
	printf("exit ACTIVE\n");
	fsm->outer = OUTER_STOPPED;
	fsm->play = PLAY_NONE;
	fsm->net = NET_NONE;
}

/*
 * put the player into STOPPED
 */
void
fsm_init(Fsm *fsm)
{
	fsm->outer = OUTER_STOPPED;
	fsm->play = PLAY_NONE;
	fsm->net = NET_NONE;
	printf("entry STOPPED\n");
}

/*
 * dispatch one event; a region event must not reset the other region
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	if (event == EVENT_PLAY && fsm->outer == OUTER_STOPPED) {
		printf("exit STOPPED\n");
		enter_active(fsm);
		print_pair(fsm);
		return;
	}
	if (event == EVENT_STOP && fsm->outer == OUTER_ACTIVE) {
		exit_active(fsm);
		printf("entry STOPPED\n");
		print_pair(fsm);
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
	} else if (event == EVENT_WIFI_DROP && fsm->net == NET_WIFI_OK) {
		printf("exit WIFI_OK\n");
		printf("entry WIFI_WEAK\n");
		fsm->net = NET_WIFI_WEAK;
	} else if (event == EVENT_WIFI_RECOVER && fsm->net == NET_WIFI_WEAK) {
		printf("exit WIFI_WEAK\n");
		printf("entry WIFI_OK\n");
		fsm->net = NET_WIFI_OK;
	}
	print_pair(fsm);
}

int
main(void)
{
	Fsm player;

	printf("=== player orthogonal demo ===\n");
	printf("one object, two regions; not two FSM objects (lesson 10)\n");
	fsm_init(&player);
	print_pair(&player);

	printf("-- PLAY, starve (net stays), wifi drop (play stays) --\n");
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_DATA_STARVE);
	fsm_dispatch(&player, EVENT_WIFI_DROP);
	fsm_dispatch(&player, EVENT_DATA_OK);
	fsm_dispatch(&player, EVENT_WIFI_RECOVER);
	fsm_dispatch(&player, EVENT_STOP);

	return 0;
}
