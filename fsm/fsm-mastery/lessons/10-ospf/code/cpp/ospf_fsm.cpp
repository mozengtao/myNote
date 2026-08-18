#include <cstdio>

enum class IsmState {
	Down = 0,
	PointToPoint,
	Waiting,
	Dr
};

enum {
	IsmStateCount = 4
};

enum class IsmEvent {
	IfUp = 0,
	IfUpBcast,
	IfDown,
	WaitTimer,
	BackupSeen
};

enum {
	IsmEventCount = 5
};

enum class NsmState {
	Down = 0,
	Init,
	TwoWay,
	ExStart,
	Full
};

enum {
	NsmStateCount = 5
};

enum class NsmEvent {
	HelloRcvd = 0,
	TwoWayRcvd,
	AdjOk,
	NegotiationDone,
	KillNbr
};

enum {
	NsmEventCount = 5
};

struct Ospf {
	IsmState ism;
	NsmState nsm;
};

using IsmAction = void (*)(Ospf *o);
using NsmAction = void (*)(Ospf *o);

struct IsmCell {
	IsmAction action;
	IsmState next;
	int defined;
};

struct NsmCell {
	NsmAction action;
	NsmState next;
	int defined;
};

static IsmCell ism_table[IsmStateCount][IsmEventCount];
static NsmCell nsm_table[NsmStateCount][NsmEventCount];
static int tables_ready;

void nsm_dispatch(Ospf *o, NsmEvent event);

/*
 * return a printable name for an ISM teaching state
 */
static const char *
ism_state_name(IsmState state)
{
	if (state == IsmState::PointToPoint)
		return "POINT_TO_POINT";
	if (state == IsmState::Waiting)
		return "WAITING";
	if (state == IsmState::Dr)
		return "DR";
	return "DOWN";
}

/*
 * return a printable name for an ISM teaching event
 */
static const char *
ism_event_name(IsmEvent event)
{
	if (event == IsmEvent::IfUpBcast)
		return "IF_UP_BCAST";
	if (event == IsmEvent::IfDown)
		return "IF_DOWN";
	if (event == IsmEvent::WaitTimer)
		return "WAIT_TIMER";
	if (event == IsmEvent::BackupSeen)
		return "BACKUP_SEEN";
	return "IF_UP";
}

/*
 * return a printable name for an NSM teaching state
 */
static const char *
nsm_state_name(NsmState state)
{
	if (state == NsmState::Init)
		return "INIT";
	if (state == NsmState::TwoWay)
		return "TWOWAY";
	if (state == NsmState::ExStart)
		return "EXSTART";
	if (state == NsmState::Full)
		return "FULL";
	return "DOWN";
}

/*
 * return a printable name for an NSM teaching event
 */
static const char *
nsm_event_name(NsmEvent event)
{
	if (event == NsmEvent::TwoWayRcvd)
		return "TWOWAY_RCVD";
	if (event == NsmEvent::AdjOk)
		return "ADJ_OK";
	if (event == NsmEvent::NegotiationDone)
		return "NEGOTIATION_DONE";
	if (event == NsmEvent::KillNbr)
		return "KILL_NBR";
	return "HELLO_RCVD";
}

/*
 * bring a point-to-point interface up
 */
static void
action_ism_up_p2p(Ospf *o)
{
	(void)o;
	std::printf("action: interface up (point-to-point)\n");
}

/*
 * bring a broadcast interface up; wait to elect DR
 */
static void
action_ism_up_bcast(Ospf *o)
{
	(void)o;
	std::printf("action: interface up (broadcast), start Wait timer\n");
}

/*
 * stub DR election: declare ourselves DR
 */
static void
action_ism_become_dr(Ospf *o)
{
	(void)o;
	std::printf("action: WaitTimer / become DR (election stub)\n");
}

/*
 * interface down: tell the neighbor machine to die
 */
static void
action_ism_down(Ospf *o)
{
	std::printf("action: interface down, post KILL_NBR to NSM\n");
	nsm_dispatch(o, NsmEvent::KillNbr);
}

/*
 * neighbor heard a Hello
 */
static void
action_nsm_hello(Ospf *o)
{
	(void)o;
	std::printf("action: neighbor seen in Hello\n");
}

/*
 * two-way connectivity
 */
static void
action_nsm_twoway(Ospf *o)
{
	(void)o;
	std::printf("action: two-way Hello\n");
}

/*
 * adjacency is allowed; start master/slave negotiation stub
 */
static void
action_nsm_adj_ok(Ospf *o)
{
	(void)o;
	std::printf("action: AdjOK, start DD negotiation stub\n");
}

/*
 * skip Exchange/Loading; go Full
 */
static void
action_nsm_full(Ospf *o)
{
	(void)o;
	std::printf("action: negotiation done (skip Exchange/Loading)\n");
}

/*
 * neighbor association torn down
 */
static void
action_nsm_kill(Ospf *o)
{
	(void)o;
	std::printf("action: KillNbr, neighbor DOWN\n");
}

/*
 * install one ISM table cell
 */
static void
ism_set(IsmState state, IsmEvent event, IsmAction action, IsmState next)
{
	ism_table[static_cast<int>(state)][static_cast<int>(event)] = {
		action, next, 1
	};
}

/*
 * install one NSM table cell
 */
static void
nsm_set(NsmState state, NsmEvent event, NsmAction action, NsmState next)
{
	nsm_table[static_cast<int>(state)][static_cast<int>(event)] = {
		action, next, 1
	};
}

/*
 * fill both sparse tables once
 */
static void
ensure_tables(void)
{
	if (tables_ready)
		return;
	ism_set(IsmState::Down, IsmEvent::IfUp, action_ism_up_p2p,
	    IsmState::PointToPoint);
	ism_set(IsmState::Down, IsmEvent::IfUpBcast, action_ism_up_bcast,
	    IsmState::Waiting);
	ism_set(IsmState::PointToPoint, IsmEvent::IfDown, action_ism_down,
	    IsmState::Down);
	ism_set(IsmState::Waiting, IsmEvent::WaitTimer, action_ism_become_dr,
	    IsmState::Dr);
	ism_set(IsmState::Waiting, IsmEvent::BackupSeen, action_ism_become_dr,
	    IsmState::Dr);
	ism_set(IsmState::Waiting, IsmEvent::IfDown, action_ism_down,
	    IsmState::Down);
	ism_set(IsmState::Dr, IsmEvent::IfDown, action_ism_down,
	    IsmState::Down);

	nsm_set(NsmState::Down, NsmEvent::HelloRcvd, action_nsm_hello,
	    NsmState::Init);
	nsm_set(NsmState::Init, NsmEvent::HelloRcvd, action_nsm_hello,
	    NsmState::Init);
	nsm_set(NsmState::Init, NsmEvent::TwoWayRcvd, action_nsm_twoway,
	    NsmState::TwoWay);
	nsm_set(NsmState::TwoWay, NsmEvent::AdjOk, action_nsm_adj_ok,
	    NsmState::ExStart);
	nsm_set(NsmState::ExStart, NsmEvent::NegotiationDone, action_nsm_full,
	    NsmState::Full);
	nsm_set(NsmState::Down, NsmEvent::KillNbr, action_nsm_kill,
	    NsmState::Down);
	nsm_set(NsmState::Init, NsmEvent::KillNbr, action_nsm_kill,
	    NsmState::Down);
	nsm_set(NsmState::TwoWay, NsmEvent::KillNbr, action_nsm_kill,
	    NsmState::Down);
	nsm_set(NsmState::ExStart, NsmEvent::KillNbr, action_nsm_kill,
	    NsmState::Down);
	nsm_set(NsmState::Full, NsmEvent::KillNbr, action_nsm_kill,
	    NsmState::Down);
	tables_ready = 1;
}

/*
 * reset both teaching machines
 */
void
fsm_init(Ospf *o)
{
	ensure_tables();
	o->ism = IsmState::Down;
	o->nsm = NsmState::Down;
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
	cell = &ism_table[static_cast<int>(o->ism)][static_cast<int>(event)];
	if (!cell->defined) {
		std::printf("ISM ignore: %s + %s\n", ism_state_name(prev),
		    ism_event_name(event));
		return;
	}
	if (cell->action)
		cell->action(o);
	o->ism = cell->next;
	std::printf("ISM: %s --%s--> %s\n", ism_state_name(prev),
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
	cell = &nsm_table[static_cast<int>(o->nsm)][static_cast<int>(event)];
	if (!cell->defined) {
		std::printf("NSM ignore: %s + %s\n", nsm_state_name(prev),
		    nsm_event_name(event));
		return;
	}
	if (cell->action)
		cell->action(o);
	o->nsm = cell->next;
	std::printf("NSM: %s --%s--> %s\n", nsm_state_name(prev),
	    nsm_event_name(event), nsm_state_name(o->nsm));
}

/*
 * Hello arrives from the wire; NSM only runs if the interface is up
 */
void
fsm_dispatch_hello(Ospf *o)
{
	if (o->ism == IsmState::Down) {
		std::printf("drop Hello: ISM is DOWN\n");
		return;
	}
	nsm_dispatch(o, NsmEvent::HelloRcvd);
}

int
main(void)
{
	Ospf ospf;

	std::printf("=== ospf teaching FSM ===\n");

	std::printf("-- p2p interface, one neighbor to FULL, then kill --\n");
	fsm_init(&ospf);
	std::printf("init: ISM=%s NSM=%s\n", ism_state_name(ospf.ism),
	    nsm_state_name(ospf.nsm));
	ism_dispatch(&ospf, IsmEvent::IfUp);
	fsm_dispatch_hello(&ospf);
	nsm_dispatch(&ospf, NsmEvent::TwoWayRcvd);
	nsm_dispatch(&ospf, NsmEvent::AdjOk);
	nsm_dispatch(&ospf, NsmEvent::NegotiationDone);
	ism_dispatch(&ospf, IsmEvent::IfDown);

	std::printf("-- broadcast Wait timer stub elects DR --\n");
	fsm_init(&ospf);
	std::printf("init: ISM=%s NSM=%s\n", ism_state_name(ospf.ism),
	    nsm_state_name(ospf.nsm));
	ism_dispatch(&ospf, IsmEvent::IfUpBcast);
	ism_dispatch(&ospf, IsmEvent::WaitTimer);

	std::printf("-- Hello while ISM is DOWN is dropped --\n");
	fsm_init(&ospf);
	fsm_dispatch_hello(&ospf);

	return 0;
}
