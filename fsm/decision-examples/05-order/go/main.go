package main

import "fmt"

// ---------------------------------------------------------------------------
// 1. Types
// ---------------------------------------------------------------------------

type State int
type EventKind int
type ActionKind int

const (
	StateCreated State = iota
	StatePaid
	StateShipped
	StateCancelled
)

const (
	EventPaySuccess EventKind = iota
	EventShip
	EventCancel
)

const (
	ActionDeductStock ActionKind = iota
	ActionNotifyLogistics
	ActionAddPoints
	ActionSendSms
	ActionReleaseStock
)

func (s State) String() string {
	return [...]string{"CREATED", "PAID", "SHIPPED", "CANCELLED"}[s]
}

func (e EventKind) String() string {
	return [...]string{"PAY_SUCCESS", "SHIP", "CANCEL"}[e]
}

type Event struct {
	Kind    EventKind
	OrderID int
	SKU     string
	Qty     int
	Amount  int
	Phone   string
}

type Action struct {
	Kind    ActionKind
	SKU     string
	Qty     int
	OrderID int
	Amount  int
	Phone   string
	Msg     string
}

type Ctx struct {
	State   State
	OrderID int
	SKU     string
	Qty     int
	Amount  int
	Phone   string
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
	{StateCreated, EventPaySuccess}: {
		StatePaid,
		[]ActionKind{
			ActionDeductStock,
			ActionNotifyLogistics,
			ActionAddPoints,
			ActionSendSms,
		},
	},
	{StatePaid, EventShip}:      {StateShipped, []ActionKind{ActionSendSms}},
	{StateCreated, EventCancel}: {StateCancelled, []ActionKind{ActionSendSms}},
	{StatePaid, EventCancel}: {
		StateCancelled,
		[]ActionKind{ActionReleaseStock, ActionSendSms},
	},
}

var smsMsg = map[EventKind]string{
	EventPaySuccess: "paid",
	EventShip:       "shipped",
	EventCancel:     "cancelled",
}

// ---------------------------------------------------------------------------
// 3. Engine  -- table lookup + bind args from Event or Ctx
// ---------------------------------------------------------------------------

func fsmDecide(ctx Ctx, ev Event) Decision {
	hit, ok := table[transKey{ctx.State, ev.Kind}]
	if !ok {
		return Decision{Accepted: false, NextState: ctx.State}
	}
	orderID, sku, qty, amount, phone := ctx.OrderID, ctx.SKU, ctx.Qty, ctx.Amount, ctx.Phone
	if ev.Kind == EventPaySuccess {
		orderID, sku, qty, amount, phone = ev.OrderID, ev.SKU, ev.Qty, ev.Amount, ev.Phone
	}
	actions := make([]Action, 0, len(hit.kinds))
	for _, kind := range hit.kinds {
		switch kind {
		case ActionDeductStock:
			actions = append(actions, Action{Kind: kind, SKU: sku, Qty: qty})
		case ActionNotifyLogistics:
			actions = append(actions, Action{Kind: kind, OrderID: orderID, SKU: sku})
		case ActionAddPoints:
			actions = append(actions, Action{Kind: kind, OrderID: orderID, Amount: amount})
		case ActionSendSms:
			actions = append(actions, Action{Kind: kind, Phone: phone, Msg: smsMsg[ev.Kind]})
		case ActionReleaseStock:
			actions = append(actions, Action{Kind: kind, SKU: sku, Qty: qty})
		}
	}
	return Decision{Accepted: true, NextState: hit.next, Actions: actions}
}

// ---------------------------------------------------------------------------
// 4. Workers  -- parameterized functions; no FSM state
// ---------------------------------------------------------------------------

func inventoryDeduct(sku string, qty int) {
	fmt.Printf("[EXEC] inventory_deduct(\"%s\", %d)\n", sku, qty)
}

func inventoryRelease(sku string, qty int) {
	fmt.Printf("[EXEC] inventory_release(\"%s\", %d)\n", sku, qty)
}

func logisticsCreate(orderID int, sku string) {
	fmt.Printf("[EXEC] logistics_create(%d, \"%s\")\n", orderID, sku)
}

func pointsAdd(orderID, amount int) {
	fmt.Printf("[EXEC] points_add(%d, %d)\n", orderID, amount)
}

func smsSend(phone, msg string) {
	fmt.Printf("[EXEC] sms_send(\"%s\", \"%s\")\n", phone, msg)
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, a := range actions {
		switch a.Kind {
		case ActionDeductStock:
			fmt.Printf("[DISPATCH] DEDUCT_STOCK(sku=%s, qty=%d)\n", a.SKU, a.Qty)
			inventoryDeduct(a.SKU, a.Qty)
		case ActionNotifyLogistics:
			fmt.Printf("[DISPATCH] NOTIFY_LOGISTICS(order_id=%d, sku=%s)\n", a.OrderID, a.SKU)
			logisticsCreate(a.OrderID, a.SKU)
		case ActionAddPoints:
			fmt.Printf("[DISPATCH] ADD_POINTS(order_id=%d, amount=%d)\n", a.OrderID, a.Amount)
			pointsAdd(a.OrderID, a.Amount)
		case ActionSendSms:
			fmt.Printf("[DISPATCH] SEND_SMS(phone=%s, msg=%s)\n", a.Phone, a.Msg)
			smsSend(a.Phone, a.Msg)
		case ActionReleaseStock:
			fmt.Printf("[DISPATCH] RELEASE_STOCK(sku=%s, qty=%d)\n", a.SKU, a.Qty)
			inventoryRelease(a.SKU, a.Qty)
		}
	}
}

// ---------------------------------------------------------------------------
// 6. Apply  -- commit ctx, then dispatch
// ---------------------------------------------------------------------------

func formatEvent(ev Event) string {
	if ev.Kind == EventPaySuccess {
		return fmt.Sprintf(
			"PAY_SUCCESS(order_id=%d, sku=%s, qty=%d, amount=%d, phone=%s)",
			ev.OrderID, ev.SKU, ev.Qty, ev.Amount, ev.Phone,
		)
	}
	return ev.Kind.String()
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
	if ev.Kind == EventPaySuccess {
		ctx.OrderID = ev.OrderID
		ctx.SKU = ev.SKU
		ctx.Qty = ev.Qty
		ctx.Amount = ev.Amount
		ctx.Phone = ev.Phone
	}
	dispatch(d.Actions)
}

func newOrder() Ctx {
	return Ctx{
		State:   StateCreated,
		OrderID: 1001,
		SKU:     "SKU-7",
		Qty:     2,
		Amount:  99,
		Phone:   "13800001111",
	}
}

func runScenario(title string, events []Event) {
	ctx := newOrder()
	fmt.Printf("=== scenario: %s ===\n", title)
	fmt.Printf("[INIT] state=%s order_id=%d sku=%s qty=%d amount=%d phone=%s\n\n",
		ctx.State, ctx.OrderID, ctx.SKU, ctx.Qty, ctx.Amount, ctx.Phone)
	for _, ev := range events {
		apply(&ctx, ev)
		fmt.Println()
	}
}

// ---------------------------------------------------------------------------
// 7. Main
// ---------------------------------------------------------------------------

func main() {
	pay := Event{
		Kind: EventPaySuccess, OrderID: 1001, SKU: "SKU-7",
		Qty: 2, Amount: 99, Phone: "13800001111",
	}
	runScenario("happy-path", []Event{
		{Kind: EventShip}, pay, {Kind: EventShip}, {Kind: EventCancel},
	})
	runScenario("cancel-created", []Event{{Kind: EventCancel}})
	runScenario("cancel-paid", []Event{pay, {Kind: EventCancel}})
}
