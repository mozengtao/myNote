#include "hsm.h"

#define HSM_MAX_DEPTH 16

/*
 * true if ancestor is on the parent chain of node (or is node)
 */
static int
hsm_is_ancestor(const HsmState *ancestor, const HsmState *node)
{
	const HsmState *p;

	for (p = node; p != 0; p = p->parent) {
		if (p == ancestor)
			return 1;
	}
	return 0;
}

/*
 * least common ancestor of a and b, or NULL
 */
static const HsmState *
hsm_lca(const HsmState *a, const HsmState *b)
{
	const HsmState *p;

	if (a == 0 || b == 0)
		return 0;
	for (p = a; p != 0; p = p->parent) {
		if (hsm_is_ancestor(p, b))
			return p;
	}
	return 0;
}

/*
 * record node as the last direct child of its parent
 */
static void
hsm_record_history(const HsmState *node)
{
	if (node == 0 || node->parent == 0)
		return;
	if (node->parent->history != 0)
		*(node->parent->history) = node;
}

/*
 * exit from leaf up to (but not including) stop
 */
static void
hsm_exit_to(Hsm *hsm, const HsmState *stop)
{
	const HsmState *s;

	s = hsm->current;
	while (s != 0 && s != stop) {
		if (s->exit != 0)
			s->exit(hsm->ctx);
		hsm_record_history(s);
		s = s->parent;
	}
}

/*
 * enter the chain from below stop down to target
 */
static void
hsm_enter_to(Hsm *hsm, const HsmState *stop, const HsmState *target)
{
	const HsmState *path[HSM_MAX_DEPTH];
	const HsmState *s;
	int n;
	int i;

	n = 0;
	for (s = target; s != 0 && s != stop; s = s->parent) {
		if (n >= HSM_MAX_DEPTH)
			return;
		path[n++] = s;
	}
	for (i = n - 1; i >= 0; i--) {
		if (path[i]->entry != 0)
			path[i]->entry(hsm->ctx);
	}
}

/*
 * enter the path from the root down to initial and set current
 */
void
hsm_init(Hsm *hsm, const HsmState *initial, void *ctx)
{
	hsm->current = 0;
	hsm->ctx = ctx;
	hsm_enter_to(hsm, 0, initial);
	hsm->current = initial;
}

/*
 * external transition: exit to LCA, then enter to target
 */
void
hsm_transition(Hsm *hsm, const HsmState *target)
{
	hsm_transition_act(hsm, target, 0);
}

/*
 * external transition with a transition action between exit and enter
 */
void
hsm_transition_act(Hsm *hsm, const HsmState *target, HsmAction action)
{
	const HsmState *lca;

	if (target == 0)
		return;
	lca = hsm_lca(hsm->current, target);
	hsm_exit_to(hsm, lca);
	if (action != 0)
		action(hsm->ctx);
	hsm_enter_to(hsm, lca, target);
	hsm->current = target;
}

/*
 * internal transition: run action, stay in current, no entry/exit
 */
void
hsm_internal(Hsm *hsm, HsmAction action)
{
	if (action != 0)
		action(hsm->ctx);
}

/*
 * last direct child of a composite, or NULL if never visited
 */
const HsmState *
hsm_shallow_history(const HsmState *composite)
{
	if (composite == 0 || composite->history == 0)
		return 0;
	return *(composite->history);
}

/*
 * walk history slots down to a leaf, or NULL if none recorded
 */
const HsmState *
hsm_deep_history(const HsmState *composite)
{
	const HsmState *s;

	s = hsm_shallow_history(composite);
	if (s == 0)
		return 0;
	while (s->history != 0 && *(s->history) != 0)
		s = *(s->history);
	return s;
}
