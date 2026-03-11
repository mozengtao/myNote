# Hash Table (Separate Chaining)

## 1. Technical Specification

### Characteristics

| Property | Value |
|----------|-------|
| Memory Layout | Array of bucket pointers + linked chains |
| Insert (average) | O(1) |
| Search (average) | O(1) |
| Delete (average) | O(1) |
| Worst case (all) | O(n) — hash collision pathological |
| Load factor threshold | 0.75 (grow), 0.10 (shrink) |
| Growth | 2x nearest prime (or power-of-two with good hash) |

### Use Cases

- **DPDK**: `rte_hash` — high-performance exact-match flow classification.
- **Linux Kernel**: `hlist_head` chaining for routing tables, inode caches.
- **DNS/Networking**: Connection tracking, ARP caches, session tables.
- **Compilers**: Symbol tables, string interning.
- **Databases**: Hash joins, in-memory indexes.

### Trade-offs

| vs. Tree-based Map | Hash Table Wins | Tree Wins |
|--------------------|----------------|-----------|
| Lookup speed | O(1) average | O(log n) guaranteed |
| Ordered iteration | Not supported | Natural |
| Worst case | O(n) possible | O(log n) |
| Memory predictability | Resize spikes | Smooth |

---

## 2. Implementation Strategy

```
htab_t
+------------------+
| buckets ---------+--> [0] -> (k,v) -> (k,v) -> NULL
| nbuckets = 8     |   [1] -> NULL
| size = 5         |   [2] -> (k,v) -> NULL
| load_max = 0.75  |   [3] -> NULL
| hash_fn          |   [4] -> (k,v) -> (k,v) -> NULL
| key_cmp          |   [5] -> NULL
| key_free         |   [6] -> NULL
| val_free         |   [7] -> NULL
+------------------+
```

- **Separate chaining** with singly-linked nodes per bucket.
- Caller provides: hash function, key comparator, optional key/value destructors.
- Auto-resize when load factor exceeds threshold.
- Bucket count is always a power of two for fast modulo (`hash & (nbuckets - 1)`).

---

## 3. Implementation

```c
/**
 * @file htab.c
 * @brief Generic hash table with separate chaining.
 *
 * Supports void* keys and values. Automatic resize on load factor
 * threshold. Bucket count is power-of-two for fast index computation.
 *
 * Standard: C99 / C11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define HTAB_INIT_BUCKETS  16
#define HTAB_LOAD_MAX_NUM   3    /* load factor = 3/4 = 0.75 */
#define HTAB_LOAD_MAX_DEN   4
#define HTAB_LOAD_MIN_NUM   1    /* shrink below 1/10 = 0.10 */
#define HTAB_LOAD_MIN_DEN  10

typedef struct htab_entry {
    void               *key;
    void               *value;
    struct htab_entry  *next;
} htab_entry_t;

typedef struct htab {
    htab_entry_t **buckets;
    size_t         nbuckets;
    size_t         size;
    uint64_t     (*hash_fn)(const void *key);
    int          (*key_cmp)(const void *a, const void *b);
    void         (*key_free)(void *);
    void         (*val_free)(void *);
} htab_t;

/* --- Internal helpers --- */

/**
 * @brief Round up to next power of two.
 */
static size_t
htab_next_pow2(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

static inline size_t
htab_bucket_idx(const htab_t *ht, const void *key)
{
    return (size_t)(ht->hash_fn(key) & (uint64_t)(ht->nbuckets - 1));
}

/**
 * @brief Rehash all entries into a new bucket array.
 * @return 0 on success, -1 on allocation failure (table unchanged).
 */
static int
htab_rehash(htab_t *ht, size_t new_nbuckets)
{
    htab_entry_t **new_buckets;
    htab_entry_t  *cur, *tmp;
    size_t         i, idx;

    new_nbuckets = htab_next_pow2(new_nbuckets);
    if (new_nbuckets < HTAB_INIT_BUCKETS)
        new_nbuckets = HTAB_INIT_BUCKETS;

    new_buckets = calloc(new_nbuckets, sizeof(htab_entry_t *));
    if (new_buckets == NULL)
        return -1;

    for (i = 0; i < ht->nbuckets; i++) {
        cur = ht->buckets[i];
        while (cur != NULL) {
            tmp = cur->next;
            idx = (size_t)(ht->hash_fn(cur->key) & (uint64_t)(new_nbuckets - 1));
            cur->next = new_buckets[idx];
            new_buckets[idx] = cur;
            cur = tmp;
        }
    }

    free(ht->buckets);
    ht->buckets  = new_buckets;
    ht->nbuckets = new_nbuckets;

    return 0;
}

/* --- Public API --- */

/**
 * @brief Create a new hash table.
 * @param hash_fn   Hash function for keys (required).
 * @param key_cmp   Key comparator: 0 on equal (required).
 * @param key_free  Optional key destructor.
 * @param val_free  Optional value destructor.
 * @return New hash table, or NULL on failure.
 */
static htab_t *
htab_create(uint64_t (*hash_fn)(const void *),
            int (*key_cmp)(const void *, const void *),
            void (*key_free)(void *),
            void (*val_free)(void *))
{
    htab_t *ht;

    if (hash_fn == NULL || key_cmp == NULL) {
        errno = EINVAL;
        return NULL;
    }

    ht = calloc(1, sizeof(*ht));
    if (ht == NULL)
        return NULL;

    ht->buckets = calloc(HTAB_INIT_BUCKETS, sizeof(htab_entry_t *));
    if (ht->buckets == NULL) {
        free(ht);
        return NULL;
    }

    ht->nbuckets = HTAB_INIT_BUCKETS;
    ht->hash_fn  = hash_fn;
    ht->key_cmp  = key_cmp;
    ht->key_free = key_free;
    ht->val_free = val_free;

    return ht;
}

/**
 * @brief Destroy the hash table, freeing all entries.
 */
static void
htab_destroy(htab_t *ht)
{
    htab_entry_t *cur, *tmp;
    size_t i;

    if (ht == NULL)
        return;

    for (i = 0; i < ht->nbuckets; i++) {
        cur = ht->buckets[i];
        while (cur != NULL) {
            tmp = cur->next;
            if (ht->key_free != NULL && cur->key != NULL)
                ht->key_free(cur->key);
            if (ht->val_free != NULL && cur->value != NULL)
                ht->val_free(cur->value);
            free(cur);
            cur = tmp;
        }
    }

    free(ht->buckets);
    free(ht);
}

/**
 * @brief Insert or update a key-value pair.
 *
 * If the key already exists, the old value is freed (if val_free set)
 * and replaced. The old key is freed and replaced with the new one.
 *
 * @return 0 on success, -1 on failure.
 */
static int
htab_insert(htab_t *ht, void *key, void *value)
{
    htab_entry_t *cur;
    size_t idx;

    if (ht == NULL || key == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* Check for existing key — update in place */
    idx = htab_bucket_idx(ht, key);
    for (cur = ht->buckets[idx]; cur != NULL; cur = cur->next) {
        if (ht->key_cmp(cur->key, key) == 0) {
            if (ht->val_free != NULL && cur->value != NULL)
                ht->val_free(cur->value);
            if (ht->key_free != NULL && cur->key != NULL)
                ht->key_free(cur->key);
            cur->key   = key;
            cur->value = value;
            return 0;
        }
    }

    /* Grow if load factor exceeded */
    if (ht->size * HTAB_LOAD_MAX_DEN >= ht->nbuckets * HTAB_LOAD_MAX_NUM) {
        htab_rehash(ht, ht->nbuckets * 2);
        idx = htab_bucket_idx(ht, key);
    }

    /* New entry at head of chain */
    cur = malloc(sizeof(*cur));
    if (cur == NULL)
        return -1;

    cur->key   = key;
    cur->value = value;
    cur->next  = ht->buckets[idx];
    ht->buckets[idx] = cur;
    ht->size++;

    return 0;
}

/**
 * @brief Look up a value by key.
 * @return Pointer to value, or NULL if not found.
 */
static void *
htab_search(const htab_t *ht, const void *key)
{
    htab_entry_t *cur;
    size_t idx;

    if (ht == NULL || key == NULL)
        return NULL;

    idx = htab_bucket_idx(ht, key);
    for (cur = ht->buckets[idx]; cur != NULL; cur = cur->next) {
        if (ht->key_cmp(cur->key, key) == 0)
            return cur->value;
    }

    return NULL;
}

/**
 * @brief Remove a key-value pair.
 * @param out_val If non-NULL, receives the value (caller takes ownership).
 * @return 0 on success, -1 if not found.
 */
static int
htab_remove(htab_t *ht, const void *key, void **out_val)
{
    htab_entry_t *cur, *prev;
    size_t idx;

    if (ht == NULL || key == NULL) {
        errno = EINVAL;
        return -1;
    }

    idx = htab_bucket_idx(ht, key);
    prev = NULL;

    for (cur = ht->buckets[idx]; cur != NULL; prev = cur, cur = cur->next) {
        if (ht->key_cmp(cur->key, key) != 0)
            continue;

        if (prev != NULL)
            prev->next = cur->next;
        else
            ht->buckets[idx] = cur->next;

        if (ht->key_free != NULL && cur->key != NULL)
            ht->key_free(cur->key);

        if (out_val != NULL) {
            *out_val = cur->value;
        } else if (ht->val_free != NULL && cur->value != NULL) {
            ht->val_free(cur->value);
        }

        free(cur);
        ht->size--;

        /* Shrink if very sparse */
        if (ht->nbuckets > HTAB_INIT_BUCKETS &&
            ht->size * HTAB_LOAD_MIN_DEN < ht->nbuckets * HTAB_LOAD_MIN_NUM) {
            htab_rehash(ht, ht->nbuckets / 2);
        }

        return 0;
    }

    return -1;
}

static inline size_t
htab_size(const htab_t *ht)
{
    return ht ? ht->size : 0;
}

/* --- Iteration --- */

typedef struct htab_iter {
    const htab_t *ht;
    size_t        bucket;
    htab_entry_t *entry;
} htab_iter_t;

/**
 * @brief Initialize an iterator.
 */
static void
htab_iter_init(htab_iter_t *it, const htab_t *ht)
{
    it->ht     = ht;
    it->bucket = 0;
    it->entry  = NULL;
}

/**
 * @brief Advance iterator. Returns 1 if valid entry, 0 if done.
 */
static int
htab_iter_next(htab_iter_t *it, void **key, void **value)
{
    if (it->entry != NULL)
        it->entry = it->entry->next;

    while (it->entry == NULL) {
        if (it->bucket >= it->ht->nbuckets)
            return 0;
        it->entry = it->ht->buckets[it->bucket++];
    }

    if (key != NULL)   *key   = it->entry->key;
    if (value != NULL) *value = it->entry->value;

    return 1;
}

/*
 * === Example / Self-test ===
 */
#ifdef HTAB_TEST
#include <assert.h>

/** FNV-1a hash for NUL-terminated strings. */
static uint64_t
fnv1a_str(const void *key)
{
    const unsigned char *p = key;
    uint64_t h = 14695981039346656037ULL;

    while (*p) {
        h ^= *p++;
        h *= 1099511628211ULL;
    }
    return h;
}

static int
str_cmp(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

int
main(void)
{
    htab_t *ht;
    char *val;
    int i;
    char buf[64];

    ht = htab_create(fnv1a_str, str_cmp, free, free);
    assert(ht != NULL);

    /* insert 1000 entries to force several rehashes */
    for (i = 0; i < 1000; i++) {
        char *k = malloc(64);
        char *v = malloc(64);
        snprintf(k, 64, "key_%d", i);
        snprintf(v, 64, "val_%d", i);
        assert(htab_insert(ht, k, v) == 0);
    }
    assert(htab_size(ht) == 1000);

    /* lookup */
    snprintf(buf, sizeof(buf), "key_42");
    val = htab_search(ht, buf);
    assert(val != NULL);
    assert(strcmp(val, "val_42") == 0);

    /* remove */
    snprintf(buf, sizeof(buf), "key_999");
    assert(htab_remove(ht, buf, NULL) == 0);
    assert(htab_size(ht) == 999);
    assert(htab_search(ht, buf) == NULL);

    /* iterate */
    {
        htab_iter_t it;
        void *k, *v;
        size_t count = 0;

        htab_iter_init(&it, ht);
        while (htab_iter_next(&it, &k, &v))
            count++;
        assert(count == 999);
    }

    htab_destroy(ht);
    printf("htab: all tests passed\n");

    return 0;
}
#endif /* HTAB_TEST */
```

Compile and test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -DHTAB_TEST -o test_htab htab.c && ./test_htab
```

---

## 4. Memory / ASCII Visualization

### Hash Table Layout

```
htab_t
+----------------+
| buckets -------+--> Bucket Array (nbuckets = 8, mask = 0x7)
| nbuckets = 8   |
| size = 5       |   idx  chain
| hash_fn        |   [0]  -> NULL
| key_cmp        |   [1]  -> [k1|v1|*] -> [k5|v5|/] -> NULL
| key_free       |   [2]  -> NULL
| val_free       |   [3]  -> [k2|v2|/] -> NULL
+----------------+   [4]  -> NULL
                     [5]  -> [k3|v3|*] -> [k4|v4|/] -> NULL
                     [6]  -> NULL
                     [7]  -> NULL
```

### Hash + Insert Flow

```
htab_insert(ht, "foo", val_ptr):

  1. hash = hash_fn("foo")       = 0xA3B7...
  2. idx  = hash & (nbuckets-1)  = 0x05
  3. Walk chain at buckets[5]:
     - "bar" != "foo" -> next
     - NULL -> not found, prepend

  buckets[5]:  [foo|val|*] -> [bar|old|/] -> NULL
                ^new entry
```

### Rehash (Double Buckets)

```
Before: nbuckets = 4, size = 4, load = 1.0 > 0.75 -> GROW

  [0] -> [A|/]
  [1] -> [B|*] -> [C|/]
  [2] -> NULL
  [3] -> [D|/]

After:  nbuckets = 8, all entries re-hashed

  [0] -> NULL
  [1] -> [B|/]
  [2] -> NULL
  [3] -> [D|/]
  [4] -> [A|/]       <-- A moved from bucket 0
  [5] -> [C|/]       <-- C moved from bucket 1
  [6] -> NULL
  [7] -> NULL
```

### FNV-1a Hash Function

```
Input:  "foo"

  h = 14695981039346656037 (FNV offset basis)

  h ^= 'f' (0x66)
  h *= 1099511628211        (FNV prime)
  h ^= 'o' (0x6F)
  h *= 1099511628211
  h ^= 'o' (0x6F)
  h *= 1099511628211

  Result: 64-bit hash -> mask to bucket index
```
