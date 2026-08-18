#include <stdio.h>

#define RETRY_BUDGET 3

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
	EVENT_DATA_OK,
	EVENT_TIMEOUT,
	EVENT_TICK,
	EVENT_GIVE_UP
} Event;

typedef struct {
	State state;
	int retries_left;
	int backoff_ticks;
	int wait_ticks;
} Fsm;

/*
 * return a printable name for a player state
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
 * return a printable name for an event
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
	if (event == EVENT_TIMEOUT)
		return "TIMEOUT";
	if (event == EVENT_TICK)
		return "TICK";
	if (event == EVENT_GIVE_UP)
		return "GIVE_UP";
	return "PLAY";
}

/*
 * begin a connect attempt
 */
static void
action_start_connect(Fsm *fsm)
{
	printf("action: start connect (retries left=%d)\n", fsm->retries_left);
}

/*
 * begin playback
 */
static void
action_start_play(Fsm *fsm)
{
	(void)fsm;
	printf("action: start play\n");
}

/*
 * wait for more stream data
 */
static void
action_wait_data(Fsm *fsm)
{
	(void)fsm;
	printf("action: wait data\n");
}

/*
 * resume playback after buffering
 */
static void
action_resume_play(Fsm *fsm)
{
	(void)fsm;
	printf("action: resume play\n");
}

/*
 * stop playback and drop the session
 */
static void
action_stop(Fsm *fsm)
{
	(void)fsm;
	printf("action: stop\n");
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
 * put the player into STOPPED
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_STOPPED;
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

	if (event == EVENT_TICK && fsm->state == STATE_CONNECTING &&
	    fsm->wait_ticks > 0) {
		fsm->wait_ticks--;
		printf("backoff wait=%d\n", fsm->wait_ticks);
		if (fsm->wait_ticks == 0)
			printf("retry connect, retries left=%d\n",
			    fsm->retries_left);
		return;
	}

	if (event == EVENT_TIMEOUT && fsm->state == STATE_CONNECTING) {
		if (fsm->wait_ticks > 0)
			return;
		fsm->retries_left--;
		printf("retries left=%d\n", fsm->retries_left);
		if (fsm->retries_left <= 0) {
			event = EVENT_GIVE_UP;
		} else {
			fsm->wait_ticks = fsm->backoff_ticks;
			printf("backoff wait: %d\n", fsm->wait_ticks);
			fsm->backoff_ticks *= 2;
			printf("%s --TIMEOUT--> %s (retries left=%d)\n",
			    state_name(prev), state_name(fsm->state),
			    fsm->retries_left);
			return;
		}
	}

	switch (fsm->state) {
	case STATE_STOPPED:
		if (event == EVENT_PLAY) {
			fsm->retries_left = RETRY_BUDGET;
			fsm->backoff_ticks = 1;
			fsm->wait_ticks = 0;
			action_start_connect(fsm);
			fsm->state = STATE_CONNECTING;
		}
		break;
	case STATE_CONNECTING:
		if (event == EVENT_CONNECTED) {
			fsm->wait_ticks = 0;
			action_start_play(fsm);
			fsm->state = STATE_PLAYING;
		} else if (event == EVENT_GIVE_UP) {
			printf("give up\n");
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = STATE_STOPPED;
		} else if (event == EVENT_STOP) {
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = STATE_STOPPED;
		}
		break;
	case STATE_PLAYING:
		if (event == EVENT_DATA_STARVE) {
			action_wait_data(fsm);
			fsm->state = STATE_BUFFERING;
		} else if (event == EVENT_STOP) {
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = STATE_STOPPED;
		}
		break;
	case STATE_BUFFERING:
		if (event == EVENT_DATA_OK) {
			action_resume_play(fsm);
			fsm->state = STATE_PLAYING;
		} else if (event == EVENT_STOP) {
			action_stop(fsm);
			clear_budget(fsm);
			fsm->state = STATE_STOPPED;
		}
		break;
	}

	if (fsm->state != prev) {
		printf("%s --%s--> %s (retries left=%d wait=%d)\n",
		    state_name(prev), event_name(event),
		    state_name(fsm->state), fsm->retries_left,
		    fsm->wait_ticks);
	}
}

int
main(void)
{
	Fsm player;

	printf("=== player demo ===\n");
	fsm_init(&player);
	printf("init: %s\n", state_name(player.state));

	printf("-- connect fails three times, then give up --\n");
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_TIMEOUT);
	fsm_dispatch(&player, EVENT_TICK);
	fsm_dispatch(&player, EVENT_TIMEOUT);
	fsm_dispatch(&player, EVENT_TICK);
	fsm_dispatch(&player, EVENT_TICK);
	fsm_dispatch(&player, EVENT_TIMEOUT);

	printf("-- connect, starve, recover, stop --\n");
	fsm_dispatch(&player, EVENT_PLAY);
	fsm_dispatch(&player, EVENT_CONNECTED);
	fsm_dispatch(&player, EVENT_DATA_STARVE);
	fsm_dispatch(&player, EVENT_DATA_OK);
	fsm_dispatch(&player, EVENT_STOP);

	return 0;
}
