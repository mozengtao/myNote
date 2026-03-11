# Dynamic Array (Vector)

## 1. Technical Specification

### Characteristics

| Property | Value |
|----------|-------|
| Memory Layout | Contiguous block, cache-friendly |
| Access (by index) | O(1) |
| Append (amortized) | O(1) |
| Insert (at position) | O(n) |
| Remove (at position) | O(n) |
| Search (unsorted) | O(n) |
| Growth Factor | 2x (geometric) |
| Shrink Threshold | size < capacity/4, shrink to capacity/2 |

### Use Cases

- **DPDK**: `rte_mempool` internals, packet descriptor arrays.
- **Linux Kernel**: `kvec` patterns, flexible arrays in structs.
- **General**: Any scenario requiring fast indexed access with dynamic sizing.
- **Networking**: Flow tables, connection tracking arrays.

### Trade-offs

| vs. Linked List | Dynamic Array Wins | Linked List Wins |
|-----------------|-------------------|-----------------|
| Cache locality | Contiguous → prefetch-friendly | N/A |
| Random access | O(1) | O(n) |
| Insert at head | O(n) — must shift | O(1) |
| Memory overhead | Low (just capacity slack) | High (pointer per node) |

---

## 2. Implementation Strategy

```
+---------------------------------------------------+
| darr_t                                            |
+---------------------------------------------------+
| void **items    --> [ ptr0 | ptr1 | ... | ptrN ]  |
| size_t size          (number of elements)          |
| size_t capacity      (allocated slots)             |
| void (*free_fn)(void *)   (element destructor)     |
+---------------------------------------------------+
```

- **Generic**: Stores `void *` pointers to any data type.
- **Ownership**: Optional `free_fn` destructor is called on each element during `_destroy` or `_remove`.
- **Growth**: Capacity doubles when full; halves when utilization drops below 25%.
- **Safety**: All public functions check for NULL `darr_t *` and bounds.

---

## 3. Implementation

```c
/**
 * @file darr.c
 * @brief Generic dynamic array (vector) implementation.
 *
 * Provides amortized O(1) append with geometric growth.
 * Uses void* for generic data storage. Thread-unsafe;
 * caller must synchronize concurrent access.
 *
 * Standard: C99 / C11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DARR_INIT_CAP   16
#define DARR_GROW_FACTOR 2
#define DARR_SHRINK_NUM  1
#define DARR_SHRINK_DEN  4

typedef struct darr {
    void   **items;
    size_t   size;
    size_t   capacity;
    void   (*free_fn)(void *);
} darr_t;

/**
 * @brief Create a new dynamic array.
 * @param free_fn Optional destructor for elements (NULL if unmanaged).
 * @return Pointer to new darr_t, or NULL on allocation failure.
 */
static darr_t *
darr_create(void (*free_fn)(void *))
{
    darr_t *da;

    da = calloc(1, sizeof(*da));
    if (da == NULL)
        return NULL;

    da->items = calloc(DARR_INIT_CAP, sizeof(void *));
    if (da->items == NULL) {
        free(da);
        return NULL;
    }

    da->size     = 0;
    da->capacity = DARR_INIT_CAP;
    da->free_fn  = free_fn;

    return da;
}

/**
 * @brief Destroy the dynamic array and free all owned elements.
 * @param da Pointer to darr_t (safe to pass NULL).
 */
static void
darr_destroy(darr_t *da)
{
    size_t i;

    if (da == NULL)
        return;

    if (da->free_fn != NULL) {
        for (i = 0; i < da->size; i++) {
            if (da->items[i] != NULL)
                da->free_fn(da->items[i]);
        }
    }

    free(da->items);
    free(da);
}

/**
 * @brief Resize internal storage.
 * @return 0 on success, -1 on failure (original array untouched).
 */
static int
darr_resize(darr_t *da, size_t new_cap)
{
    void **tmp;

    if (new_cap < da->size)
        return -1;

    tmp = realloc(da->items, new_cap * sizeof(void *));
    if (tmp == NULL)
        return -1;

    if (new_cap > da->capacity)
        memset(tmp + da->capacity, 0, (new_cap - da->capacity) * sizeof(void *));

    da->items    = tmp;
    da->capacity = new_cap;

    return 0;
}

/**
 * @brief Append an element to the end of the array.
 * @param da  The dynamic array.
 * @param item Pointer to store (ownership transferred if free_fn set).
 * @return 0 on success, -1 on failure.
 */
static int
darr_push(darr_t *da, void *item)
{
    if (da == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (da->size >= da->capacity) {
        if (darr_resize(da, da->capacity * DARR_GROW_FACTOR) != 0)
            return -1;
    }

    da->items[da->size++] = item;
    return 0;
}

/**
 * @brief Insert an element at a specific index, shifting subsequent elements.
 * @return 0 on success, -1 on failure or out-of-bounds.
 */
static int
darr_insert(darr_t *da, size_t index, void *item)
{
    if (da == NULL || index > da->size) {
        errno = EINVAL;
        return -1;
    }

    if (da->size >= da->capacity) {
        if (darr_resize(da, da->capacity * DARR_GROW_FACTOR) != 0)
            return -1;
    }

    memmove(&da->items[index + 1], &da->items[index],
            (da->size - index) * sizeof(void *));

    da->items[index] = item;
    da->size++;

    return 0;
}

/**
 * @brief Get the element at a given index.
 * @return Pointer to element, or NULL if out of bounds.
 */
static void *
darr_get(const darr_t *da, size_t index)
{
    if (da == NULL || index >= da->size)
        return NULL;

    return da->items[index];
}

/**
 * @brief Remove the element at a given index, shifting subsequent elements.
 * @param da    The dynamic array.
 * @param index Index to remove.
 * @param out   If non-NULL, receives the removed pointer (caller takes ownership).
 *              If NULL and free_fn is set, the element is freed.
 * @return 0 on success, -1 on failure.
 */
static int
darr_remove(darr_t *da, size_t index, void **out)
{
    void *item;

    if (da == NULL || index >= da->size) {
        errno = EINVAL;
        return -1;
    }

    item = da->items[index];

    memmove(&da->items[index], &da->items[index + 1],
            (da->size - index - 1) * sizeof(void *));

    da->size--;
    da->items[da->size] = NULL;

    if (out != NULL) {
        *out = item;
    } else if (da->free_fn != NULL && item != NULL) {
        da->free_fn(item);
    }

    /* Shrink if utilization drops below 25% */
    if (da->size > 0 &&
        da->size * DARR_SHRINK_DEN <= da->capacity * DARR_SHRINK_NUM &&
        da->capacity > DARR_INIT_CAP) {
        darr_resize(da, da->capacity / DARR_GROW_FACTOR);
    }

    return 0;
}

/**
 * @brief Remove and return the last element.
 * @return Pointer to element, or NULL if empty.
 */
static void *
darr_pop(darr_t *da)
{
    void *item;

    if (da == NULL || da->size == 0)
        return NULL;

    item = da->items[--da->size];
    da->items[da->size] = NULL;

    if (da->size > 0 &&
        da->size * DARR_SHRINK_DEN <= da->capacity * DARR_SHRINK_NUM &&
        da->capacity > DARR_INIT_CAP) {
        darr_resize(da, da->capacity / DARR_GROW_FACTOR);
    }

    return item;
}

/**
 * @brief Linear search for an element.
 * @param da  The dynamic array.
 * @param key The key to search for.
 * @param cmp Comparator: returns 0 on match.
 * @return Index of first match, or (size_t)-1 if not found.
 */
static size_t
darr_search(const darr_t *da, const void *key,
            int (*cmp)(const void *, const void *))
{
    size_t i;

    if (da == NULL || cmp == NULL)
        return (size_t)-1;

    for (i = 0; i < da->size; i++) {
        if (cmp(da->items[i], key) == 0)
            return i;
    }

    return (size_t)-1;
}

/** @brief Return current element count. */
static inline size_t
darr_size(const darr_t *da)
{
    return da ? da->size : 0;
}

/** @brief Return current allocated capacity. */
static inline size_t
darr_capacity(const darr_t *da)
{
    return da ? da->capacity : 0;
}

/*
 * === Example / Self-test ===
 */
#ifdef DARR_TEST
#include <assert.h>

int
main(void)
{
    darr_t *da;
    int *val;
    int i;

    da = darr_create(free);
    assert(da != NULL);

    /* push 100 elements */
    for (i = 0; i < 100; i++) {
        val = malloc(sizeof(int));
        *val = i;
        assert(darr_push(da, val) == 0);
    }
    assert(darr_size(da) == 100);

    /* indexed access */
    val = darr_get(da, 42);
    assert(val != NULL && *val == 42);

    /* pop */
    val = darr_pop(da);
    assert(val != NULL && *val == 99);
    free(val);

    /* remove at index 0 (shifts all) */
    assert(darr_remove(da, 0, NULL) == 0);
    val = darr_get(da, 0);
    assert(val != NULL && *val == 1);

    darr_destroy(da);
    printf("darr: all tests passed\n");

    return 0;
}
#endif /* DARR_TEST */
```

Compile and test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -DDARR_TEST -o test_darr darr.c && ./test_darr
```

---

## 4. Memory / ASCII Visualization

### Logical Layout

```
darr_t
+------------+
| items -----+---> +-------+-------+-------+-------+-------+- - -+-------+
| size = 5   |     | ptr0  | ptr1  | ptr2  | ptr3  | ptr4  |     | NULL  |
| capacity=8 |     +---+---+---+---+---+---+---+---+---+---+- - -+-------+
| free_fn    |         |       |       |       |       |         capacity=8
+------------+         v       v       v       v       v
                     [dat0] [dat1] [dat2] [dat3] [dat4]
```

### Growth Sequence

```
push push push ... (capacity reached)
    |
    v
+---+---+---+---+           +---+---+---+---+---+---+---+---+
| A | B | C | D |  realloc  | A | B | C | D | E |   |   |   |
+---+---+---+---+  ----->>  +---+---+---+---+---+---+---+---+
  cap=4, size=4                cap=8, size=5
```

### Shrink Behavior

```
remove remove remove ... (size < capacity/4)
    |
    v
+---+---+---+---+---+---+---+---+           +---+---+---+---+
| A |   |   |   |   |   |   |   |  realloc  | A |   |   |   |
+---+---+---+---+---+---+---+---+  ----->>  +---+---+---+---+
  cap=8, size=1                                cap=4, size=1
```
