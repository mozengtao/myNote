#include <stdio.h>

typedef enum {
	STATE_STOPPED = 0,
	STATE_CONNECTING,
	STATE_PLAYING,
	STATE_BUFFERING
} State;

typedef enum {
	EVENT_PLAY = 0,
	EVENT_STOP,
	EVENT_CONNECTED,
	EVENT_DATA_STARVE,
	EVENT_DATA_OK
} Event;

typedef struct {
	State state;
} Fsm;

/*
 * return a printable leaf name
 */
static const char *
state_name(State state)
{
	if (state == STATE_CONNECTING)
		return "CONNECTING";
	if (state == STATE_PLAYING)
		return "PLAYING";
	if (state == STATE_BUFFERING)
		return "BUFFERING";
	return "STOPPED";
}

/*
 * return a printable event name
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_STOP)
		return "STOP";
	if (event == EVENT_CONNECTED)
		return "CONNECTED";
	if (event == EVENT_DATA_STARVE)
		return "DATA_STARVE";
	if (event == EVENT_DATA_OK)
		return "DATA_OK";
	return "PLAY";
}

/*
 * true if the leaf sits inside the ACTIVE composite
 */
static int
in_active(State state)
{
	return state != STATE_STOPPED;
}

/*
 * print ACTIVE/leaf or the lone STOPPED name
 */
static void
print_path(State state)
{
	if (in_active(state))
		printf("ACTIVE/%s", state_name(state));
	else
		printf("%s", state_name(state));
}

/*
 * show the flat names we refuse to implement
 */
static void
print_flat_explosion(void)
{
	printf("=== flat explosion (do not implement) ===\n");
	printf("STOPPED\n");
	printf("CONNECTING_RETRY1\n");
	printf("CONNECTING_RETRY2\n");
	printf("CONNECTING_RETRY3\n");
	printf("PLAYING_HEALTHY\n");
	printf("PLAYING_BUFFERING\n");
	printf("PLAYING_SEEKING\n");
	printf("STOP must be drawn from every box above\n");
	printf("=== nested HSM ===\n");
	printf("Player\n");
	printf("  STOPPED\n");
	printf("  ACTIVE (composite)\n");
	printf("    CONNECTING\n");
	printf("    PLAYING\n");
	printf("    BUFFERING\n");
	printf("STOP is one arrow on ACTIVE\n");
}

/*
 * put the player into STOPPED
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_STOPPED;
}

/*
 * dispatch one event; STOP is handled once for the ACTIVE parent
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;

	prev = fsm->state;

	if (event == EVENT_STOP && in_active(fsm->state)) {
		printf("ACTIVE --STOP--> STOPPED (one arrow on parent)\n");
		fsm->state = STATE_STOPPED;
		return;
	}

	if (fsm->state == STATE_STOPPED && event == EVENT_PLAY)
		fsm->state = STATE_CONNECTING;
	else if (fsm->state == STATE_CONNECTING && event == EVENT_CONNECTED)
		fsm->state = STATE_PLAYING;
	else if (fsm->state == STATE_PLAYING && event == EVENT_DATA_STARVE)
		fsm->state = STATE_BUFFERING;
	else if (fsm->state == STATE_BUFFERING && event == EVENT_DATA_OK)
		fsm->state = STATE_PLAYING;

	if (fsm->state != prev) {
		print_path(prev);
		printf(" --%s--> ", event_name(event));
		print_path(fsm->state);
		printf("\n");
	}
}

int
main(void)
{
	Fsm player;

	print_flat_explosion();
	printf("=== player composite demo ===\n");
	fsm_init(&player);
	printf("init: ");
	print_path(player.state);
	printf("\n");

	printf("-- play, connect, starve, recover, stop from PLAYING --\n");
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_CONNECTED);
	fsm_dispatch(&player, EVENT_DATA_STARVE);
	fsm_dispatch(&player, EVENT_DATA_OK);
	fsm_dispatch(&player, EVENT_STOP);

	printf("-- play, then STOP from CONNECTING (same parent arrow) --\n");
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_STOP);

	return 0;
}
