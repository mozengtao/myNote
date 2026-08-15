package main

import "fmt"

// ---------------------------------------------------------------------------
// 1. Types
// ---------------------------------------------------------------------------

type State int
type EventKind int
type ActionKind int

const (
	StateLocked State = iota
	StateUnlocked
)

const (
	EventCoin EventKind = iota
	EventPush
)

const (
	ActionCreditBox ActionKind = iota
	ActionSetMechanism
)

func (s State) String() string {
	return [...]string{"LOCKED", "UNLOCKED"}[s]
}

func (e EventKind) String() string {
	return [...]string{"COIN", "PUSH"}[e]
}

type Event struct {
	Kind  EventKind
	Cents int
}

type Action struct {
	Kind   ActionKind
	Cents  int
	Locked int
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
	{StateLocked, EventCoin}: {
		StateUnlocked,
		[]ActionKind{ActionCreditBox, ActionSetMechanism},
	},
	{StateUnlocked, EventPush}: {
		StateLocked,
		[]ActionKind{ActionSetMechanism},
	},
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
		if kind == ActionCreditBox {
			actions = append(actions, Action{Kind: kind, Cents: ev.Cents})
			continue
		}
		locked := 1
		if hit.next == StateUnlocked {
			locked = 0
		}
		actions = append(actions, Action{Kind: kind, Locked: locked})
	}
	return Decision{Accepted: true, NextState: hit.next, Actions: actions}
}

// ---------------------------------------------------------------------------
// 4. Workers  -- parameterized functions; no FSM state
// ---------------------------------------------------------------------------

func coinBoxCredit(cents int) {
	fmt.Printf("[EXEC] coin_box_credit(%d)\n", cents)
}

func mechanismSet(locked int) {
	fmt.Printf("[EXEC] mechanism_set(%d)\n", locked)
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, a := range actions {
		switch a.Kind {
		case ActionCreditBox:
			fmt.Printf("[DISPATCH] CREDIT_BOX(cents=%d)\n", a.Cents)
			coinBoxCredit(a.Cents)
		case ActionSetMechanism:
			fmt.Printf("[DISPATCH] SET_MECHANISM(locked=%d)\n", a.Locked)
			mechanismSet(a.Locked)
		}
	}
}

// ---------------------------------------------------------------------------
// 6. Apply  -- commit ctx, then dispatch
// ---------------------------------------------------------------------------

func formatEvent(ev Event) string {
	if ev.Kind == EventCoin {
		return fmt.Sprintf("COIN(cents=%d)", ev.Cents)
	}
	return "PUSH"
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
	ctx := Ctx{State: StateLocked}
	fmt.Printf("[INIT] state=%s\n\n", ctx.State)
	events := []Event{
		{Kind: EventCoin, Cents: 100},
		{Kind: EventPush},
		{Kind: EventPush},
		{Kind: EventCoin, Cents: 100},
		{Kind: EventCoin, Cents: 50},
		{Kind: EventPush},
	}
	for _, ev := range events {
		apply(&ctx, ev)
		fmt.Println()
	}
}
