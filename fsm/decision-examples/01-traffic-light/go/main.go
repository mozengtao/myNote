package main

import "fmt"

// ---------------------------------------------------------------------------
// 1. Types
// ---------------------------------------------------------------------------

type State int
type EventKind int
type ActionKind int
type Color int

const (
	StateRed State = iota
	StateGreen
	StateYellow
)

const (
	EventTick EventKind = iota
)

const (
	ActionSetLamp ActionKind = iota
)

const (
	ColorRed Color = iota
	ColorGreen
	ColorYellow
)

func (s State) String() string {
	return [...]string{"RED", "GREEN", "YELLOW"}[s]
}

func (e EventKind) String() string {
	return [...]string{"TICK"}[e]
}

func (c Color) String() string {
	return [...]string{"RED", "GREEN", "YELLOW"}[c]
}

type Event struct {
	Kind  EventKind
	NowMs uint
}

type Action struct {
	Kind  ActionKind
	Color Color
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
	{StateRed, EventTick}:    {StateGreen, []ActionKind{ActionSetLamp}},
	{StateGreen, EventTick}:  {StateYellow, []ActionKind{ActionSetLamp}},
	{StateYellow, EventTick}: {StateRed, []ActionKind{ActionSetLamp}},
}

func colorOf(st State) Color {
	switch st {
	case StateGreen:
		return ColorGreen
	case StateYellow:
		return ColorYellow
	default:
		return ColorRed
	}
}

// ---------------------------------------------------------------------------
// 3. Engine  -- table lookup + bind args; zero side effects
// ---------------------------------------------------------------------------

func fsmDecide(ctx Ctx, ev Event) Decision {
	hit, ok := table[transKey{ctx.State, ev.Kind}]
	if !ok {
		return Decision{Accepted: false, NextState: ctx.State}
	}
	return Decision{
		Accepted:  true,
		NextState: hit.next,
		Actions: []Action{{
			Kind:  ActionSetLamp,
			Color: colorOf(hit.next),
		}},
	}
}

// ---------------------------------------------------------------------------
// 4. Workers  -- parameterized functions; no FSM state
// ---------------------------------------------------------------------------

func lampSet(color Color) {
	fmt.Printf("[EXEC] lamp_set(%s)\n", color)
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, a := range actions {
		fmt.Printf("[DISPATCH] SET_LAMP(color=%s)\n", a.Color)
		lampSet(a.Color)
	}
}

// ---------------------------------------------------------------------------
// 6. Apply  -- commit ctx, then dispatch
// ---------------------------------------------------------------------------

func formatEvent(ev Event) string {
	return fmt.Sprintf("TICK(now_ms=%d)", ev.NowMs)
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
	ctx := Ctx{State: StateRed}
	fmt.Printf("[INIT] state=%s\n\n", ctx.State)
	for _, nowMs := range []uint{1000, 2000, 3000, 4000} {
		apply(&ctx, Event{Kind: EventTick, NowMs: nowMs})
		fmt.Println()
	}
}
