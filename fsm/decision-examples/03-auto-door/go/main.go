package main

import "fmt"

// ---------------------------------------------------------------------------
// 1. Types
// ---------------------------------------------------------------------------

type State int
type EventKind int
type ActionKind int
type Dir int

const (
	StateClosed State = iota
	StateOpening
	StateOpen
	StateClosing
)

const (
	EventPerson EventKind = iota
	EventTimeout
)

const (
	ActionMotorRun ActionKind = iota
	ActionLatchSet
)

const (
	DirOpen Dir = iota
	DirClose
)

func (s State) String() string {
	return [...]string{"CLOSED", "OPENING", "OPEN", "CLOSING"}[s]
}

func (e EventKind) String() string {
	return [...]string{"PERSON", "TIMEOUT"}[e]
}

func (d Dir) String() string {
	return [...]string{"OPEN", "CLOSE"}[d]
}

type Event struct {
	Kind      EventKind
	SensorID  int
	ElapsedMs uint
}

type Action struct {
	Kind      ActionKind
	Direction Dir
	Duty      int
	Open      int
}

type Ctx struct {
	State State
}

type Decision struct {
	Accepted  bool
	NextState State
	Actions   []Action
}

// ---------------------------------------------------------------------------
// 2. Table  (State, EventKind) -> (NextState, ActionKind list)
// ---------------------------------------------------------------------------

type transKey struct {
	s State
	e EventKind
}

type transVal struct {
	next  State
	kinds []ActionKind
}

var table = map[transKey]transVal{
	{StateClosed, EventPerson}:   {StateOpening, []ActionKind{ActionMotorRun}},
	{StateOpening, EventTimeout}: {StateOpen, []ActionKind{ActionLatchSet}},
	{StateOpen, EventPerson}:     {StateOpen, []ActionKind{}},
	{StateOpen, EventTimeout}:    {StateClosing, []ActionKind{ActionMotorRun}},
	{StateClosing, EventPerson}:  {StateOpening, []ActionKind{ActionMotorRun}},
	{StateClosing, EventTimeout}: {StateClosed, []ActionKind{ActionLatchSet}},
}

// ---------------------------------------------------------------------------
// 3. Engine  -- table lookup + bind args; zero side effects
// ---------------------------------------------------------------------------

func fsmDecide(ctx Ctx, ev Event) Decision {
	hit, ok := table[transKey{ctx.State, ev.Kind}]
	if !ok {
		return Decision{Accepted: false, NextState: ctx.State}
	}
	actions := make([]Action, 0, len(hit.kinds))
	for _, kind := range hit.kinds {
		if kind == ActionMotorRun {
			dir := DirClose
			if hit.next == StateOpening {
				dir = DirOpen
			}
			actions = append(actions, Action{Kind: kind, Direction: dir, Duty: 80})
			continue
		}
		open := 0
		if hit.next == StateOpen {
			open = 1
		}
		actions = append(actions, Action{Kind: kind, Open: open})
	}
	return Decision{Accepted: true, NextState: hit.next, Actions: actions}
}

// ---------------------------------------------------------------------------
// 4. Workers  -- parameterized functions; no FSM state
// ---------------------------------------------------------------------------

func motorRun(direction Dir, duty int) {
	fmt.Printf("[EXEC] motor_run(%s, %d)\n", direction, duty)
}

func latchSet(open int) {
	fmt.Printf("[EXEC] latch_set(%d)\n", open)
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, a := range actions {
		switch a.Kind {
		case ActionMotorRun:
			fmt.Printf("[DISPATCH] MOTOR_RUN(dir=%s, duty=%d)\n", a.Direction, a.Duty)
			motorRun(a.Direction, a.Duty)
		case ActionLatchSet:
			fmt.Printf("[DISPATCH] LATCH_SET(open=%d)\n", a.Open)
			latchSet(a.Open)
		}
	}
}

// ---------------------------------------------------------------------------
// 6. Apply  -- commit ctx, then dispatch
// ---------------------------------------------------------------------------

func formatEvent(ev Event) string {
	if ev.Kind == EventPerson {
		return fmt.Sprintf("PERSON(sensor_id=%d)", ev.SensorID)
	}
	return fmt.Sprintf("TIMEOUT(elapsed_ms=%d)", ev.ElapsedMs)
}

func apply(ctx *Ctx, ev Event) {
	fmt.Printf("[EVENT] %s | state=%s\n", formatEvent(ev), ctx.State)
	d := fsmDecide(*ctx, ev)
	if !d.Accepted {
		fmt.Printf("[DECISION] REJECT %s + %s  (state unchanged)\n",
			ctx.State, ev.Kind)
		return
	}
	fmt.Printf("[DECISION] %s + %s -> %s\n", ctx.State, ev.Kind, d.NextState)
	ctx.State = d.NextState
	dispatch(d.Actions)
}

// ---------------------------------------------------------------------------
// 7. Main
// ---------------------------------------------------------------------------

func main() {
	ctx := Ctx{State: StateClosed}
	fmt.Printf("[INIT] state=%s\n\n", ctx.State)
	events := []Event{
		{Kind: EventTimeout, ElapsedMs: 500},
		{Kind: EventPerson, SensorID: 1},
		{Kind: EventTimeout, ElapsedMs: 2000},
		{Kind: EventPerson, SensorID: 1},
		{Kind: EventTimeout, ElapsedMs: 5000},
		{Kind: EventPerson, SensorID: 2},
		{Kind: EventTimeout, ElapsedMs: 2000},
		{Kind: EventTimeout, ElapsedMs: 5000},
		{Kind: EventTimeout, ElapsedMs: 2000},
	}
	for _, ev := range events {
		apply(&ctx, ev)
		fmt.Println()
	}
}
