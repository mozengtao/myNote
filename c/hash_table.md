## hash table
![00 index](hashtable/00-index.md)  
![01 separate chaining](hashtable/01-separate-chaining.md)  
![02 open addressing basic](hashtable/02-open-addressing-basic.md)  
![03 open addressing advanced](hashtable/03-open-addressing-advanced.md)  
![04 specialized methods](hashtable/04-specialized-methods.md)  
![05 comparison and advice](hashtable/05-comparison-and-advice.md)  
```c
/*
Hash Buckets
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  0  │  1  │  2  │  3  │ ... │ ... │ ... │ ... │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
   │     │     │     │           │     │     │
   │     │     │     │           │     │     └─► Node → Node → nil
   │     │     │     │           │     └─► nil
   │     │     │     │           └─► Node → nil
   │     │     │     └─► Node → Node → Node → nil
   │     │     └─► nil
   │     └─► Node → nil
   └─► Node → Node → nil
*/
/* -- 1 -- */
/*  hash.c  –  tiny educational hash table  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- tunables ---------- */
#define NUM_BUCKETS 16          /* must be power of two for fast mod */

/* ---------- node in linked list ---------- */
typedef struct Node {
    char *key;
    char *val;
    struct Node *next;
} Node;

/* ---------- the table ---------- */
typedef struct HashTable {
    Node *buckets[NUM_BUCKETS]; /* array of list heads */
} HashTable;

/* ---------- djb2 hash (Kernighan/Pike) ---------- */
static unsigned long hash(const char *str)
{
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;   /* h * 33 + c */
    return h;
}

/* ---------- create empty table ---------- */
HashTable *ht_create(void)
{
    HashTable *ht = malloc(sizeof *ht);
    for (int i = 0; i < NUM_BUCKETS; ++i)
        ht->buckets[i] = NULL;
    return ht;
}

/* ---------- insert or update ---------- */
void ht_insert(HashTable *ht, const char *key, const char *val)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1); /* fast modulo */
    Node *n = ht->buckets[idx];

    /* update if key already present */
    for (; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            free(n->val);
            n->val = strdup(val);
            return;
        }
    }

    /* create new node and push to front */
    n = malloc(sizeof *n);
    n->key   = strdup(key);
    n->val   = strdup(val);
    n->next  = ht->buckets[idx];
    ht->buckets[idx] = n;
}

/* ---------- search ---------- */
const char *ht_search(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    for (Node *n = ht->buckets[idx]; n; n = n->next)
        if (strcmp(n->key, key) == 0)
            return n->val;
    return NULL;                /* not found */
}

/* ---------- delete ---------- */
void ht_delete(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node **link = &ht->buckets[idx]; /* pointer to pointer trick */
    while (*link) {
        Node *n = *link;
        if (strcmp(n->key, key) == 0) {
            *link = n->next;    /* unlink */
            free(n->key);
            free(n->val);
            free(n);
            return;
        }
        link = &n->next;
    }
}

/* ---------- free everything ---------- */
void ht_destroy(HashTable *ht)
{
    for (int i = 0; i < NUM_BUCKETS; ++i) {
        Node *n = ht->buckets[i];
        while (n) {
            Node *next = n->next;
            free(n->key);
            free(n->val);
            free(n);
            n = next;
        }
    }
    free(ht);
}

/* ---------- demo ---------- */
int main(void)
{
    HashTable *ht = ht_create();

    ht_insert(ht, "apple", "red");
    ht_insert(ht, "banana", "yellow");
    ht_insert(ht, "lemon", "yellow");
    ht_insert(ht, "pear", "green");

    printf("banana → %s\n", ht_search(ht, "banana"));
    printf("pear   → %s\n", ht_search(ht, "pear"));
    printf("grape  → %s\n", ht_search(ht, "grape")); /* not found */

    ht_delete(ht, "banana");
    printf("banana after delete → %s\n", ht_search(ht, "banana"));

    ht_destroy(ht);
    return 0;
}

/* -- 2 -- */
/*  hash.c  –  tiny educational hash table  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- tunables ---------- */
#define NUM_BUCKETS 16          /* must be power of two for fast mod */

/* ---------- node in linked list ---------- */
typedef struct Node {
    char       *key;
    void       *value;
    struct Node *next;
} Node;

/* ---------- the table ---------- */
typedef struct {
    Node *buckets[NUM_BUCKETS];
    /* optional user-supplied helpers */
    void *(*value_copy)(const void *);   /* strdup for your type */
    void  (*value_free)(void *);         /* free for your type */
} HashTable;

/* ---------- djb2 hash (Kernighan/Pike) ---------- */
static unsigned long hash(const char *str)
{
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;   /* h * 33 + c */
    return h;
}

/* ---------- create empty table ---------- */
HashTable *ht_create(void *(*vc)(const void *), void (*vf)(void *))
{
    HashTable *ht = calloc(1, sizeof *ht);
    ht->value_copy = vc;
    ht->value_free = vf;
    return ht;
}

/* ---------- insert or update ---------- */
void ht_insert(HashTable *ht, const char *key, const void *value)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node *n;

    /* update existing key */
    for (n = ht->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (ht->value_free) ht->value_free(n->value);
            n->value = ht->value_copy ? ht->value_copy(value) : (void *)value;
            return;
        }
    }

    /* create new node */
    n = malloc(sizeof *n);
    n->key   = strdup(key);
    n->value = ht->value_copy ? ht->value_copy(value) : (void *)value;
    n->next  = ht->buckets[idx];
    ht->buckets[idx] = n;
}

/* ---------- search ---------- */
void *ht_search(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    for (Node *n = ht->buckets[idx]; n; n = n->next)
        if (strcmp(n->key, key) == 0) return n->value;
    return NULL;
}

/* ---------- delete ---------- */
void ht_delete(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node **link = &ht->buckets[idx];
    while (*link) {
        Node *n = *link;
        if (strcmp(n->key, key) == 0) {
            *link = n->next;
            free(n->key);
            if (ht->value_free) ht->value_free(n->value);
            free(n);
            return;
        }
        link = &n->next;
    }
}

/* ---------- free everything ---------- */
void ht_destroy(HashTable *ht)
{
    for (int i = 0; i < NUM_BUCKETS; ++i) {
        Node *n = ht->buckets[i];
        while (n) {
            Node *next = n->next;
            free(n->key);
            if (ht->value_free) ht->value_free(n->value);
            free(n);
            n = next;
        }
    }
    free(ht);
}

// 1
void *int_copy(const void *p) { return (void *)p; }

int main(void)
{
    HashTable *ht = ht_create(int_copy, NULL); /* no free needed */

    int k42 = 42, k7 = 7;
    ht_insert(ht, "forty-two", (void *)&k42);
    ht_insert(ht, "seven",     (void *)&k7);

    int v = *(int*)ht_search(ht, "forty-two");
    printf("forty-two = %d\n", v);   /* 42 */

    ht_destroy(ht);
}

// 2
typedef struct {
    double x, y;
} Point;

void *point_copy(const void *src)
{
    Point *p = malloc(sizeof *p);
    *p = *(Point *)src;
    return p;
}
void point_free(void *p) { free(p); }

int main(void)
{
    HashTable *ht = ht_create(point_copy, point_free);

    Point origin = {0.0, 0.0}, unit = {1.0, 1.0};
    ht_insert(ht, "origin", &origin);
    ht_insert(ht, "unit",   &unit);

    Point *u = ht_search(ht, "unit");
    printf("unit  = (%.1f, %.1f)\n", u->x, u->y);

    ht_destroy(ht);
}

// 3
int main(void)
{
    HashTable *ht = ht_create((void *(*)(const void *))strdup, free);

    ht_insert(ht, "apple", "red");
    ht_insert(ht, "banana", "yellow");
    ht_insert(ht, "lemon", "yellow");
    ht_insert(ht, "pear", "green");

    printf("banana → %s\n", (const char *)ht_search(ht, "banana"));
    printf("pear   → %s\n", (const char *)ht_search(ht, "pear"));
    printf("grape  → %s\n", (const char *)ht_search(ht, "grape")); /* not found */

    ht_delete(ht, "banana");
    printf("banana after delete → %s\n", (const char *)ht_search(ht, "banana"));

    ht_destroy(ht);
    return 0;
}

/* -- 3 -- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Node structure for key-value pairs
typedef struct Node {
    char* key;      // e.g., "Alice"
    int value;      // e.g., 30
    struct Node* next;  // For chaining collisions
} Node;

// HashTable structure
typedef struct HashTable {
    Node** buckets;  // Array of pointers to Node
    int size;        // Number of buckets
} HashTable;

// Simple hash function for strings: sum of ASCII % size
int hash(const char* key, int size) {
    int sum = 0;
    for (int i = 0; key[i] != '\0'; i++) {
        sum += key[i];
    }
    return sum % size;
}

// Create a new node
Node* createNode(const char* key, int value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->key = strdup(key);  // Copy string
    newNode->value = value;
    newNode->next = NULL;
    return newNode;
}

// Initialize hash table
HashTable* createHashTable(int size) {
    HashTable* ht = (HashTable*)malloc(sizeof(HashTable));
    ht->size = size;
    ht->buckets = (Node**)calloc(size, sizeof(Node*));  // Initialize to NULL
    return ht;
}

// Insert or update key-value
void insert(HashTable* ht, const char* key, int value) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];

    // Check if key exists (update value)
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            current->value = value;
            return;
        }
        current = current->next;
    }

    // Key doesn't exist: insert new node at front
    Node* newNode = createNode(key, value);
    newNode->next = ht->buckets[index];
    ht->buckets[index] = newNode;
}

// Search for key and return value (or -1 if not found)
int search(HashTable* ht, const char* key) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return -1;  // Not found
}

// Delete key
void delete(HashTable* ht, const char* key) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];
    Node* prev = NULL;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (prev == NULL) {
                ht->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current->key);  // Free duplicated string
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Print the entire table (for demo)
void printTable(HashTable* ht) {
    for (int i = 0; i < ht->size; i++) {
        printf("Bucket %d: ", i);
        Node* current = ht->buckets[i];
        while (current != NULL) {
            printf("(%s=%d) -> ", current->key, current->value);
            current = current->next;
        }
        printf("NULL\n");
    }
}

// Free the hash table (cleanup)
void freeHashTable(HashTable* ht) {
    for (int i = 0; i < ht->size; i++) {
        Node* current = ht->buckets[i];
        while (current != NULL) {
            Node* temp = current;
            current = current->next;
            free(temp->key);
            free(temp);
        }
    }
    free(ht->buckets);
    free(ht);
}

int main() {
    HashTable* ht = createHashTable(10);

    // Insert some data
    insert(ht, "Alice", 30);
    insert(ht, "Bob", 25);
    insert(ht, "Charlie", 35);  // Might collide with Alice depending on hash
    insert(ht, "David", 28);

    // Print table
    printf("Hash Table Contents:\n");
    printTable(ht);

    // Search example
    int age = search(ht, "Bob");
    printf("\nBob's age: %d\n", age);

    // Delete example
    delete(ht, "Alice");
    printf("\nAfter deleting Alice:\n");
    printTable(ht);

    // Cleanup
    freeHashTable(ht);
    return 0;
}
```

```lua
-- 1
-- Simple hash function for ints: key % size (handle negative)
local function hash(key, size)
    return ((key % size) + size) % size
end

-- Create a new node table
local function createNode(key, value)
    return {key = key, value = value, next = nil}
end

-- Initialize hash table
local function createHashTable(size)
    local ht = {size = size, buckets = {}}
    for i = 1, size do
        ht.buckets[i] = nil  -- Empty buckets
    end
    return ht
end

-- Insert or update key-value
local function insert(ht, key, value)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]

    -- Check if key exists (update value)
    while current ~= nil do
        if current.key == key then
            current.value = value
            return
        end
        current = current.next
    end

    -- Key doesn't exist: insert new node at front
    local newNode = createNode(key, value)
    newNode.next = ht.buckets[index]
    ht.buckets[index] = newNode
end

-- Search for key and return value (or -1 if not found)
local function search(ht, key)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]

    while current ~= nil do
        if current.key == key then
            return current.value
        end
        current = current.next
    end
    return -1  -- Not found
end

-- Delete key
local function delete(ht, key)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]
    local prev = nil

    while current ~= nil do
        if current.key == key then
            if prev == nil then
                ht.buckets[index] = current.next
            else
                prev.next = current.next
            end
            -- No explicit free; Lua GC handles it
            return
        end
        prev = current
        current = current.next
    end
end

-- Print the entire table (for demo)
local function printTable(ht)
    for i = 1, ht.size do
        io.write("Bucket " .. (i - 1) .. ": ")
        local current = ht.buckets[i]
        while current ~= nil do
            io.write("(" .. current.key .. "=" .. current.value .. ") -> ")
            current = current.next
        end
        print("nil")
    end
end

-- Main demo
local ht = createHashTable(10)

-- Insert some data (int keys: e.g., employee IDs)
insert(ht, 123, 30)   -- ID 123, age 30 (123 % 10 = 3)
insert(ht, 133, 25)   -- ID 133 hashes to 3
insert(ht, 143, 35)   -- ID 143 hashes to 3 (collision with 133!)
insert(ht, 100, 28)   -- ID 100 hashes to 0

-- Print table
print("Hash Table Contents:")
printTable(ht)

-- Search example
local age = search(ht, 133)
print("\nID 133's age: " .. age)

-- Delete example
delete(ht, 123)
print("\nAfter deleting ID 123:")
printTable(ht)

-- OOP style
-- HashTable prototype (methods)
local HashTable = {}
HashTable.__index = HashTable

-- Simple hash function for ints: key % size (handle negative)
local function hash(key, size)
    return ((key % size) + size) % size
end

-- Constructor: Create a new HashTable instance
function HashTable.new(size)
    local self = {
        size = size,
        buckets = {}
    }
    -- Initialize empty buckets
    for i = 1, size do
        self.buckets[i] = nil
    end
    -- Set metatable for method access
    setmetatable(self, HashTable)
    return self
end

-- Create a new node table
local function createNode(key, value)
    return {key = key, value = value, next = nil}
end

-- Insert or update key-value
function HashTable:insert(key, value)
    local index = hash(key, self.size)
    local current = self.buckets[index]

    -- Check if key exists (update value)
    while current ~= nil do
        if current.key == key then
            current.value = value
            return
        end
        current = current.next
    end

    -- Key doesn't exist: insert new node at front
    local newNode = createNode(key, value)
    newNode.next = self.buckets[index]
    self.buckets[index] = newNode
end

-- Search for key and return value (or -1 if not found)
function HashTable:search(key)
    local index = hash(key, self.size)
    local current = self.buckets[index]

    while current ~= nil do
        if current.key == key then
            return current.value
        end
        current = current.next
    end
    return -1  -- Not found
end

-- Delete key
function HashTable:delete(key)
    local index = hash(key, self.size)
    local current = self.buckets[index]
    local prev = nil

    while current ~= nil do
        if current.key == key then
            if prev == nil then
                self.buckets[index] = current.next
            else
                prev.next = current.next
            end
            -- No explicit free; Lua GC handles it
            return
        end
        prev = current
        current = current.next
    end
end

-- Print the entire table (for demo)
function HashTable:printTable()
    for i = 1, self.size do
        io.write("Bucket " .. (i - 1) .. ": ")
        local current = self.buckets[i]
        while current ~= nil do
            io.write("(" .. current.key .. "=" .. current.value .. ") -> ")
            current = current.next
        end
        print("nil")
    end
end

-- Main demo
local ht = HashTable.new(10)

-- Insert some data (int keys: e.g., employee IDs)
ht:insert(123, 30)   -- ID 123, age 30 (123 % 10 = 3)
ht:insert(133, 25)   -- ID 133 hashes to 3
ht:insert(143, 35)   -- ID 143 hashes to 3 (collision with 133!)
ht:insert(100, 28)   -- ID 100 hashes to 0

-- Print table
print("Hash Table Contents:")
ht:printTable()

-- Search example
local age = ht:search(133)
print("\nID 133's age: " .. age)

-- Delete example
ht:delete(123)
print("\nAfter deleting ID 123:")
ht:printTable()
```