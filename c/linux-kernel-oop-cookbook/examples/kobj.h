/*
 * kobj.h - Shared object-model helpers for cookbook examples.
 *
 * Mirrors the Linux Kernel mental model:
 *   offsetof  -> member offset inside a struct
 *   container_of -> recover outer (derived) object from embedded member
 *
 * Uses __builtin_offsetof (same approach as modern Linux Kernel).
 */
#ifndef KOBJ_H
#define KOBJ_H

#include <stddef.h>
#include <stdint.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)
#endif

/*
 * container_of - cast a member pointer to the containing struct
 * @ptr:    pointer to the member
 * @type:   type of the container struct
 * @member: name of the member within the struct
 */
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* KOBJ_H */
