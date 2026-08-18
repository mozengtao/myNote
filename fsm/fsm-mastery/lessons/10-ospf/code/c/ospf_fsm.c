#include <stdio.h>

typedef enum {
	ISM_DOWN = 0,
	ISM_POINT_TO_POINT,
	ISM_WAITING,
	ISM_DR,
	ISM_COUNT
} IsmState;

typedef enum {
	ISM_IF_UP = 0,
	ISM_IF_UP_BCAST,
	ISM_IF_DOWN,
	ISM_WAIT_TIMER,
	ISM_BACKUP_SEEN,
	ISM_COUNT_EVENT
} IsmEvent;

typedef enum {
	NSM_DOWN = 0,
	NSM_INIT,
	NSM_TWOWAY,
	NSM_EXSTART,
	NSM_FULL,
	NSM_COUNT
} NsmState;

typedef enum {
	NSM_HELLO_RCVD = 0,
	NSM_TWOWAY_RCVD,
	NSM_ADJ_OK,
	NSM_NEGOTIATION_DONE,
	NSM_KILL_NBR,
	NSM_COUNT_EVENT
} NsmEvent;

typedef struct Ospf Ospf;

struct Ospf {
	IsmState ism;
	NsmState nsm;
};

typedef void (*IsmAction)(Ospf *o);
typedef void (*NsmAction)(Ospf *o);

typedef struct {
	IsmAction action;
	IsmState next;
	int defined;
} IsmCell;

typedef struct {
	NsmAction action;
	NsmState next;
	int defined;
} NsmCell;

/*
 * return a printable name for an ISM teaching state
 */
static const char *
ism_state_name(IsmState state)
{
	if (state == ISM_POINT_TO_POINT)
		return "POINT_TO_POINT";
	if (state == ISM_WAITING)
		return "WAITING";
	if (state == ISM_DR)
		return "DR";
	return "DOWN";
}

/*
 * return a printable name for an ISM teaching event
 */
static const char *
ism_event_name(IsmEvent event)
{
	if (event == ISM_IF_UP_BCAST)
		return "IF_UP_BCAST";
	if (event == ISM_IF_DOWN)
		return "IF_DOWN";
	if (event == ISM_WAIT_TIMER)
		return "WAIT_TIMER";
	if (event == ISM_BACKUP_SEEN)
		return "BACKUP_SEEN";
	return "IF_UP";
}

/*
 * return a printable name for an NSM teaching state
 */
static const char *
nsm_state_name(NsmState state)
{
	if (state == NSM_INIT)
		return "INIT";
	if (state == NSM_TWOWAY)
		return "TWOWAY";
	if (state == NSM_EXSTART)
		return "EXSTART";
	if (state == NSM_FULL)
		return "FULL";
	return "DOWN";
}

/*
 * return a printable name for an NSM teaching event
 */
static const char *
nsm_event_name(NsmEvent event)
{
	if (event == NSM_TWOWAY_RCVD)
		return "TWOWAY_RCVD";
	if (event == NSM_ADJ_OK)
		return "ADJ_OK";
	if (event == NSM_NEGOTIATION_DONE)
		return "NEGOTIATION_DONE";
	if (event == NSM_KILL_NBR)
		return "KILL_NBR";
	return "HELLO_RCVD";
}

void nsm_dispatch(Ospf *o, NsmEvent event);

/*
 * bring a point-to-point interface up
 */
static void
action_ism_up_p2p(Ospf *o)
{
	(void)o;
	printf("action: interface up (point-to-point)\n");
}

/*
 * bring a broadcast interface up; wait to elect DR
 */
static void
action_ism_up_bcast(Ospf *o)
{
	(void)o;
	printf("action: interface up (broadcast), start Wait timer\n");
}

/*
 * stub DR election: declare ourselves DR
 */
static void
action_ism_become_dr(Ospf *o)
{
	(void)o;
	printf("action: WaitTimer / become DR (election stub)\n");
}

/*
 * interface down: tell the neighbor machine to die
 */
static void
action_ism_down(Ospf *o)
{
	printf("action: interface down, post KILL_NBR to NSM\n");
	nsm_dispatch(o, NSM_KILL_NBR);
}

/*
 * neighbor heard a Hello
 */
static void
action_nsm_hello(Ospf *o)
{
	(void)o;
	printf("action: neighbor seen in Hello\n");
}

/*
 * two-way connectivity
 */
static void
action_nsm_twoway(Ospf *o)
{
	(void)o;
	printf("action: two-way Hello\n");
}

/*
 * adjacency is allowed; start master/slave negotiation stub
 */
static void
action_nsm_adj_ok(Ospf *o)
{
	(void)o;
	printf("action: AdjOK, start DD negotiation stub\n");
}

/*
 * skip Exchange/Loading; go Full
 */
static void
action_nsm_full(Ospf *o)
{
	(void)o;
	printf("action: negotiation done (skip Exchange/Loading)\n");
}

/*
 * neighbor association torn down
 */
static void
action_nsm_kill(Ospf *o)
{
	(void)o;
	printf("action: KillNbr, neighbor DOWN\n");
}

static const IsmCell ism_table[ISM_COUNT][ISM_COUNT_EVENT] = {
	[ISM_DOWN][ISM_IF_UP] = {
		action_ism_up_p2p, ISM_POINT_TO_POINT, 1
	},
	[ISM_DOWN][ISM_IF_UP_BCAST] = {
		action_ism_up_bcast, ISM_WAITING, 1
	},
	[ISM_POINT_TO_POINT][ISM_IF_DOWN] = {
		action_ism_down, ISM_DOWN, 1
	},
	[ISM_WAITING][ISM_WAIT_TIMER] = {
		action_ism_become_dr, ISM_DR, 1
	},
	[ISM_WAITING][ISM_BACKUP_SEEN] = {
		action_ism_become_dr, ISM_DR, 1
	},
	[ISM_WAITING][ISM_IF_DOWN] = {
		action_ism_down, ISM_DOWN, 1
	},
	[ISM_DR][ISM_IF_DOWN] = {
		action_ism_down, ISM_DOWN, 1
	},
};

static const NsmCell nsm_table[NSM_COUNT][NSM_COUNT_EVENT] = {
	[NSM_DOWN][NSM_HELLO_RCVD] = {
		action_nsm_hello, NSM_INIT, 1
	},
	[NSM_INIT][NSM_HELLO_RCVD] = {
		action_nsm_hello, NSM_INIT, 1
	},
	[NSM_INIT][NSM_TWOWAY_RCVD] = {
		action_nsm_twoway, NSM_TWOWAY, 1
	},
	[NSM_TWOWAY][NSM_ADJ_OK] = {
		action_nsm_adj_ok, NSM_EXSTART, 1
	},
	[NSM_EXSTART][NSM_NEGOTIATION_DONE] = {
		action_nsm_full, NSM_FULL, 1
	},
	[NSM_DOWN][NSM_KILL_NBR] = {
		action_nsm_kill, NSM_DOWN, 1
	},
	[NSM_INIT][NSM_KILL_NBR] = {
		action_nsm_kill, NSM_DOWN, 1
	},
	[NSM_TWOWAY][NSM_KILL_NBR] = {
		action_nsm_kill, NSM_DOWN, 1
	},
	[NSM_EXSTART][NSM_KILL_NBR] = {
		action_nsm_kill, NSM_DOWN, 1
	},
	[NSM_FULL][NSM_KILL_NBR] = {
		action_nsm_kill, NSM_DOWN, 1
	},
};

/*
 * reset both teaching machines
 */
void
fsm_init(Ospf *o)
{
	o->ism = ISM_DOWN;
	o->nsm = NSM_DOWN;
}

/*
 * dispatch one ISM event by table lookup
 */
void
ism_dispatch(Ospf *o, IsmEvent event)
{
	const IsmCell *cell;
	IsmState prev;

	prev = o->ism;
	cell = &ism_table[o->ism][event];
	if (!cell->defined) {
		printf("ISM ignore: %s + %s\n", ism_state_name(prev),
		    ism_event_name(event));
		return;
	}
	if (cell->action)
		cell->action(o);
	o->ism = cell->next;
	printf("ISM: %s --%s--> %s\n", ism_state_name(prev),
	    ism_event_name(event), ism_state_name(o->ism));
}

/*
 * dispatch one NSM event by table lookup
 */
void
nsm_dispatch(Ospf *o, NsmEvent event)
{
	const NsmCell *cell;
	NsmState prev;

	prev = o->nsm;
	cell = &nsm_table[o->nsm][event];
	if (!cell->defined) {
		printf("NSM ignore: %s + %s\n", nsm_state_name(prev),
		    nsm_event_name(event));
		return;
	}
	if (cell->action)
		cell->action(o);
	o->nsm = cell->next;
	printf("NSM: %s --%s--> %s\n", nsm_state_name(prev),
	    nsm_event_name(event), nsm_state_name(o->nsm));
}

/*
 * Hello arrives from the wire; NSM only runs if the interface is up
 */
void
fsm_dispatch_hello(Ospf *o)
{
	if (o->ism == ISM_DOWN) {
		printf("drop Hello: ISM is DOWN\n");
		return;
	}
	nsm_dispatch(o, NSM_HELLO_RCVD);
}

int
main(void)
{
	Ospf ospf;

	printf("=== ospf teaching FSM ===\n");

	printf("-- p2p interface, one neighbor to FULL, then kill --\n");
	fsm_init(&ospf);
	printf("init: ISM=%s NSM=%s\n", ism_state_name(ospf.ism),
	    nsm_state_name(ospf.nsm));
	ism_dispatch(&ospf, ISM_IF_UP);
	fsm_dispatch_hello(&ospf);
	nsm_dispatch(&ospf, NSM_TWOWAY_RCVD);
	nsm_dispatch(&ospf, NSM_ADJ_OK);
	nsm_dispatch(&ospf, NSM_NEGOTIATION_DONE);
	ism_dispatch(&ospf, ISM_IF_DOWN);

	printf("-- broadcast Wait timer stub elects DR --\n");
	fsm_init(&ospf);
	printf("init: ISM=%s NSM=%s\n", ism_state_name(ospf.ism),
	    nsm_state_name(ospf.nsm));
	ism_dispatch(&ospf, ISM_IF_UP_BCAST);
	ism_dispatch(&ospf, ISM_WAIT_TIMER);

	printf("-- Hello while ISM is DOWN is dropped --\n");
	fsm_init(&ospf);
	fsm_dispatch_hello(&ospf);

	return 0;
}
