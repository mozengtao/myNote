#ifndef HSM_HPP
#define HSM_HPP

/*
 * Hierarchical state machine skeleton extracted from lesson 13.
 *
 * Header-only, C++17. Lesson 13 does not #include this file; each
 * sub-lesson keeps a self-contained player so the demo runs alone.
 *
 * A state node has parent, entry, exit, and an optional history slot
 * pointer. See engine/c/hsm.h for the same contract in C.
 */

constexpr int HsmMaxDepth = 16;

using HsmAction = void (*)(void *ctx);

struct HsmState {
	const char *name;
	const HsmState *parent;
	HsmAction entry;
	HsmAction exit;
	const HsmState **history;
};

struct Hsm {
	const HsmState *current;
	void *ctx;
};

/*
 * true if ancestor is on the parent chain of node (or is node)
 */
inline int
hsm_is_ancestor(const HsmState *ancestor, const HsmState *node)
{
	const HsmState *p;

	for (p = node; p != nullptr; p = p->parent) {
		if (p == ancestor)
			return 1;
	}
	return 0;
}

/*
 * least common ancestor of a and b, or nullptr
 */
inline const HsmState *
hsm_lca(const HsmState *a, const HsmState *b)
{
	const HsmState *p;

	if (a == nullptr || b == nullptr)
		return nullptr;
	for (p = a; p != nullptr; p = p->parent) {
		if (hsm_is_ancestor(p, b))
			return p;
	}
	return nullptr;
}

/*
 * record node as the last direct child of its parent
 */
inline void
hsm_record_history(const HsmState *node)
{
	if (node == nullptr || node->parent == nullptr)
		return;
	if (node->parent->history != nullptr)
		*(node->parent->history) = node;
}

/*
 * exit from leaf up to (but not including) stop
 */
inline void
hsm_exit_to(Hsm *hsm, const HsmState *stop)
{
	const HsmState *s;

	s = hsm->current;
	while (s != nullptr && s != stop) {
		if (s->exit != nullptr)
			s->exit(hsm->ctx);
		hsm_record_history(s);
		s = s->parent;
	}
}

/*
 * enter the chain from below stop down to target
 */
inline void
hsm_enter_to(Hsm *hsm, const HsmState *stop, const HsmState *target)
{
	const HsmState *path[HsmMaxDepth];
	const HsmState *s;
	int n;
	int i;

	n = 0;
	for (s = target; s != nullptr && s != stop; s = s->parent) {
		if (n >= HsmMaxDepth)
			return;
		path[n++] = s;
	}
	for (i = n - 1; i >= 0; i--) {
		if (path[i]->entry != nullptr)
			path[i]->entry(hsm->ctx);
	}
}

/*
 * enter the path from the root down to initial and set current
 */
inline void
hsm_init(Hsm *hsm, const HsmState *initial, void *ctx)
{
	hsm->current = nullptr;
	hsm->ctx = ctx;
	hsm_enter_to(hsm, nullptr, initial);
	hsm->current = initial;
}

/*
 * external transition with a transition action between exit and enter
 */
inline void
hsm_transition_act(Hsm *hsm, const HsmState *target, HsmAction action)
{
	const HsmState *lca;

	if (target == nullptr)
		return;
	lca = hsm_lca(hsm->current, target);
	hsm_exit_to(hsm, lca);
	if (action != nullptr)
		action(hsm->ctx);
	hsm_enter_to(hsm, lca, target);
	hsm->current = target;
}

/*
 * external transition: exit to LCA, then enter to target
 */
inline void
hsm_transition(Hsm *hsm, const HsmState *target)
{
	hsm_transition_act(hsm, target, nullptr);
}

/*
 * internal transition: run action, stay in current, no entry/exit
 */
inline void
hsm_internal(Hsm *hsm, HsmAction action)
{
	if (action != nullptr)
		action(hsm->ctx);
}

/*
 * last direct child of a composite, or nullptr if never visited
 */
inline const HsmState *
hsm_shallow_history(const HsmState *composite)
{
	if (composite == nullptr || composite->history == nullptr)
		return nullptr;
	return *(composite->history);
}

/*
 * walk history slots down to a leaf, or nullptr if none recorded
 */
inline const HsmState *
hsm_deep_history(const HsmState *composite)
{
	const HsmState *s;

	s = hsm_shallow_history(composite);
	if (s == nullptr)
		return nullptr;
	while (s->history != nullptr && *(s->history) != nullptr)
		s = *(s->history);
	return s;
}

#endif
