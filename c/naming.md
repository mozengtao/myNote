## variable naming
- Local Variables
```c
/* Good Examples */
int user_id;
float account_balance;
char* filename;
size_t buffer_size;
int retry_count;
bool is_connected;
FILE* input_file;
struct user* current_user;
int* scores_array;
unsigned int flags;

/* Bad Examples */
int a;                     /* Too vague */
float f;                   /* Meaningless */
char* s;                   /* What string? */
int x1, x2, x3;            /* Cryptic */
```
- Global Variables
```c
/* Module prefix + descriptive name */
/* Good Examples */
int g_debug_level;         /* 'g_' prefix for global */
volatile sig_atomic_t g_signal_received;
FILE* g_log_file;
int g_total_connections;   /* Thread-safe with atomic ops */

/* Avoid when possible - use accessors instead */
/* Bad Examples */
int counter;               /* No context, global side effects */
char buffer[1024];         /* Exposed mutable global */
```
- Static Variables
```c
/* Good Examples */
static int s_initialized = 0;      /* 's_' prefix for static */
static const char* s_module_name = "net";
static pthread_mutex_t s_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static struct list_head s_connection_pool;

/* Alternative: descriptive without prefix */
static int connection_count = 0;
static float cache_hit_ratio = 0.0f;
```
- Constants
```c
/* Preprocessor macros - UPPER_CASE */
#define MAX_BUFFER_SIZE 4096
#define DEFAULT_TIMEOUT_MS 5000
#define PI 3.141592653589793
#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* const variables - UPPER_CASE or descriptive */
const int MAX_CONNECTIONS = 100;    /* Sometimes UPPER_CASE */
const char* const LOG_DIRECTORY = "/var/log"; /* Constant pointer to constant */
const float acceleration_gravity = 9.81f;  /* Or snake_case for runtime consts */

/* Enum constants - typically UPPER_CASE */
typedef enum {
    STATE_IDLE = 0,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_DISCONNECTING
} connection_state_t;
```
- Pointers and Arrays
```c
/* Good Examples */
char* name_ptr;           /* Simple suffix */
int* numbers_array;       /* Type in name */
struct node** head_ref;   /* Reference to pointer */
FILE* file_handle;        /* Descriptive */
void (*callback_fn)(int); /* Function pointer */

/* Array naming */
int scores[10];
char buffer[BUFFER_SIZE];
struct user users[MAX_USERS];

/* Alternative pointer notation (choose one style) */
int *p_number;            /* Hungarian-ish (less common in modern C) */
char *sz_filename;        /* Hungarian (Windows legacy) - generally avoid */
```
- Struct/Union Members
```c
/* Good Examples */
struct person {
    char first_name[32];
    char last_name[32];
    int age_years;
    float height_cm;
    bool is_employed;
    struct address* home_address;  /* Pointer member */
};

struct linked_list {
    struct node* head;
    struct node* tail;
    size_t count;
    pthread_mutex_t lock;
};
```
- Loop Variables and Iterators
```c
/* Good Examples */
for (size_t i = 0; i < count; i++) {  /* Simple loops */
    process_item(items[i]);
}

for (int row = 0; row < ROWS; row++) {  /* Descriptive 2D */
    for (int col = 0; col < COLS; col++) {
        matrix[row][col] = 0;
    }
}

struct node* current = list_head;  /* Linked list traversal */
while (current != NULL) {
    process_node(current);
    current = current->next;
}

/* Bad Examples */
for (int a = 0; a < b; a++) {  /* What are a and b? */
    for (int c = 0; c < d; c++) {
        e[a][c] = f;
    }
}
```
- Function Parameters
```c
/* Good Examples */
int open_file(const char* filename, const char* mode) {
    /* filename and mode clearly describe purpose */
}

void sort_array(int* array, size_t array_size, 
                bool ascending, compare_fn_t compare) {
    /* Clear parameter names */
}

/* Bad Examples */
void process(int a, int b, char* c) {  /* What do these mean? */
    /* Implementation */
}
```
- Boolean/Flag Variables
```c
/* Good Examples */
bool is_valid;           /* State check */
bool has_permission;     /* Possession check */
bool should_retry;       /* Action flag */
bool was_initialized;    /* Past state */
bool can_write;          /* Capability */
int error_occurred;      /* If not using bool (C99+) */

/* Using prefixes */
int flag_debug;          /* Alternative style */
int enable_logging;

/* Bad Examples */
bool b;                  /* Meaningless */
int flag1, flag2;        /* Unclear */
int x;                   /* Used as boolean */
```
- type Definitions
```c
/* Good Examples */
typedef int user_id_t;           /* _t suffix (POSIX caution) */
typedef float temperature_celsius_t;
typedef struct point point_t;    /* Opaque handle pattern */
typedef void (*callback_t)(int); /* Function pointer type */

/* Alternative without _t (avoiding POSIX conflicts) */
typedef int UserId;
typedef float TemperatureCelsius;
typedef struct Point Point;

/* Bad Examples */
typedef int INT;                 /* Redundant/all caps */
typedef char* STRING;            /* Misleading - not a string class */
```
- complete example
```c
/* file: network_client.c */

/* Module-level statics */
static int s_client_count = 0;
static pthread_mutex_t s_count_lock = PTHREAD_MUTEX_INITIALIZER;

/* Constants */
#define MAX_CLIENTS 1024
#define RECV_TIMEOUT_MS 5000
#define SEND_BUFFER_SIZE 4096

/* Type definitions */
typedef struct {
    int socket_fd;
    char client_ip[INET_ADDRSTRLEN];
    time_t connect_time;
    bool is_authenticated;
    pthread_t thread_id;
} client_connection_t;

/* Function with well-named parameters and locals */
int handle_client_connection(int client_socket, 
                            const char* client_address) {
    /* Local variables */
    char recv_buffer[SEND_BUFFER_SIZE];
    int bytes_received;
    bool connection_active = true;
    time_t last_activity_time = time(NULL);
    client_connection_t* current_client = NULL;

    /* Allocate client structure */
    current_client = malloc(sizeof(client_connection_t));
    if (current_client == NULL) {
        log_error("Failed to allocate client structure");
        return -1;
    }

    /* Initialize */
    current_client->socket_fd = client_socket;
    strncpy(current_client->client_ip, client_address, 
            INET_ADDRSTRLEN - 1);
    current_client->connect_time = time(NULL);
    current_client->is_authenticated = false;

    /* Update global count (thread-safe) */
    pthread_mutex_lock(&s_count_lock);
    s_client_count++;
    pthread_mutex_unlock(&s_count_lock);

    /* Main connection loop */
    while (connection_active) {
        bytes_received = recv_data(client_socket, 
                                  recv_buffer, 
                                  sizeof(recv_buffer) - 1);

        if (bytes_received > 0) {
            recv_buffer[bytes_received] = '\0';
            last_activity_time = time(NULL);

            /* Process the received data */
            if (!process_client_message(current_client, 
                                       recv_buffer, 
                                       bytes_received)) {
                connection_active = false;
            }
        } 
        else if (bytes_received == 0) {
            log_info("Client disconnected: %s", client_address);
            connection_active = false;
        } 
        else {
            log_error("Receive error from %s", client_address);
            connection_active = false;
        }

        /* Check for timeout */
        if (difftime(time(NULL), last_activity_time) > 
            CONNECTION_TIMEOUT_SECONDS) {
            log_warn("Connection timeout: %s", client_address);
            connection_active = false;
        }
    }

    /* Cleanup */
    close(client_socket);
    free(current_client);

    pthread_mutex_lock(&s_count_lock);
    s_client_count--;
    pthread_mutex_unlock(&s_count_lock);

    return 0;
}
```
## function naming
- Key Pattern Usage in Example:
1. Module Prefix (kv_)​
    Groups all related functionality
    Avoids naming collisions
    Clear ownership: kv_item_put(), kv_store_create()
2. Verb-Noun/Object Pattern
    Action + Target: item_put(), store_create(), iterator_next()
    Clear what the function does and to what
3. Boolean Predicates (is_, has_, can_)
    kv_item_exists(), kv_store_is_initialized(), kv_store_can_accept_more()
    Immediately recognizable as returning bool
4. Getter/Setter Convention
    kv_store_get_item_count(), kv_store_set_max_size()
    No ambiguity about modifying vs reading
5. Constructor/Destructor Pairs
    kv_store_create()/ kv_store_destroy()
    kv_iterator_create()/ kv_iterator_destroy()
    Consistent resource management
6. Internal/Helper Functions (_kv_prefix)
    Static functions use _kv_prefix (with caution for reserved names)
    Clearly separates public API from implementation details
    Alternative: Use no prefix but keep them static
7. Error Handling Convention
    Consistent return type: kv_error_tfor operations that can fail
    voidfor operations that cannot fail (or use internal error handling)
8. Callback Type Definitions
    kv_for_each_callback_t- typedef for function pointer
    Clear naming convention for callback types
    Additional Good Practices Shown:
    Opaque Handles: kv_store_tis an incomplete type in header
    const-correctness: constfor read-only parameters
    Out Parameters: *_outnaming convention
    Resource Pairs: create/destroy, init/cleanup
    Debug Macros: Conditionally compiled helpers
    Documentation: Comments for each function group
    Consistent Parameter Order: (context, input..., output...)
```c
/* file: kv_store.c  – Key-Value Store Module Implementation */
#ifndef KV_STORE_H
#define KV_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Module Prefix: kv_ for all public functions */
/* Type Definitions --------------------------------------------------------- */
typedef struct kv_store kv_store_t;              /* opaque handle */

/* Error Codes */
typedef enum {
    KV_SUCCESS = 0,
    KV_ERROR_NOT_FOUND,
    KV_ERROR_EXISTS,
    KV_ERROR_MEMORY,
    KV_ERROR_INVALID_ARG,
    KV_ERROR_IO
} kv_error_t;

/* Public API: Constructor/Destructor Pattern ------------------------------ */
kv_error_t kv_store_create(kv_store_t **store_out, 
                          const char *data_path);  /* allocate & init */
void kv_store_destroy(kv_store_t *store);        /* cleanup all resources */

/* Module/Component Prefix Pattern ----------------------------------------- */
kv_error_t kv_item_put(kv_store_t *store,        /* add/update */
                      const char *key, 
                      const void *value, 
                      size_t value_len);
kv_error_t kv_item_get(const kv_store_t *store,  /* retrieve */
                      const char *key,
                      void **value_out,
                      size_t *value_len_out);
kv_error_t kv_item_delete(kv_store_t *store,     /* remove */
                         const char *key);
bool kv_item_exists(const kv_store_t *store,     /* check existence */
                   const char *key);

/* Verb-Noun Pattern ------------------------------------------------------- */
kv_error_t kv_batch_put_multiple(kv_store_t *store, /* bulk operations */
                                const char *keys[],
                                const void *values[],
                                const size_t lengths[],
                                size_t count);
kv_error_t kv_import_from_json(kv_store_t *store,  /* data import */
                              const char *json_path);
kv_error_t kv_export_to_file(kv_store_t *store,    /* data export */
                            const char *output_path);
void kv_clear_all_entries(kv_store_t *store);    /* bulk clear */

/* State Checking Pattern (Boolean predicates) ----------------------------- */
bool kv_store_is_initialized(const kv_store_t *store);
bool kv_store_has_pending_writes(const kv_store_t *store);
bool kv_store_can_accept_more(const kv_store_t *store, size_t required_bytes);
bool kv_item_is_expired(const kv_store_t *store, const char *key);

/* Getter/Setter Pattern --------------------------------------------------- */
size_t kv_store_get_item_count(const kv_store_t *store);
size_t kv_store_get_total_bytes(const kv_store_t *store);
uint32_t kv_store_get_version(const kv_store_t *store);
void kv_store_set_max_size(kv_store_t *store, size_t max_bytes);
void kv_store_set_auto_save(kv_store_t *store, bool enabled);

/* Iterator Pattern -------------------------------------------------------- */
typedef struct kv_iterator kv_iterator_t;
kv_iterator_t* kv_iterator_create(const kv_store_t *store);
kv_error_t kv_iterator_next(kv_iterator_t *iter, 
                           const char **key_out,
                           const void **value_out);
void kv_iterator_destroy(kv_iterator_t *iter);

/* Callback Pattern -------------------------------------------------------- */
typedef void (*kv_for_each_callback_t)(const char *key, 
                                       const void *value, 
                                       size_t len, 
                                       void *user_data);
void kv_for_each(const kv_store_t *store, 
                kv_for_each_callback_t callback,
                void *user_data);

#endif /* KV_STORE_H */

/* ------------------------------------------------------------------------ */
/* file: kv_store.c – Internal Implementation with Static Helpers ---------- */

struct kv_store {
    /* Implementation details */
    void *data;
    size_t max_size;
    bool auto_save;
    /* ... */
};

struct kv_iterator {
    /* Iterator state */
    /* ... */
};

/* Module Prefix Pattern for Internals: kv_internal_ or _kv_ (static only) */
static kv_error_t _kv_validate_key(const char *key);
static kv_error_t _kv_check_space_available(kv_store_t *store, size_t needed);
static void _kv_compute_hash(const char *key, uint64_t *hash_out);
static void _kv_compress_value(void *value, size_t *len);
static void* _kv_allocate_aligned(size_t size, size_t alignment);
static void _kv_release_aligned(void *ptr, size_t alignment);

/* Type-Specific Helper Patterns ------------------------------------------ */
static struct kv_bucket* _kv_bucket_create(size_t capacity);
static void _kv_bucket_destroy(struct kv_bucket *bucket);
static struct kv_entry* _kv_entry_create(const char *key, 
                                         const void *value, 
                                         size_t len);
static void _kv_entry_destroy(struct kv_entry *entry);

/* File I/O Helpers ------------------------------------------------------- */
static kv_error_t _kv_save_to_disk(const kv_store_t *store, 
                                  const char *filename);
static kv_error_t _kv_load_from_disk(kv_store_t *store, 
                                    const char *filename);
static bool _kv_file_exists(const char *path);
static void _kv_create_backup(const char *source, const char *dest);

/* String Manipulation Helpers ------------------------------------------- */
static char* _kv_strdup(const char *str);
static char* _kv_format_key(const char *key, char *buffer, size_t size);
static bool _kv_str_ends_with(const char *str, const char *suffix);

/* Debug/Logging Helpers ------------------------------------------------- */
#ifdef DEBUG
    static void _kv_debug_dump_buckets(const kv_store_t *store);
    static void _kv_debug_print_stats(const kv_store_t *store);
    static void _kv_assert_invariant(const kv_store_t *store);
#else
    #define _kv_debug_dump_buckets(store) ((void)0)
    #define _kv_debug_print_stats(store) ((void)0)
    #define _kv_assert_invariant(store) ((void)0)
#endif

/* Performance/Utility Helpers ------------------------------------------- */
static size_t _kv_compute_optimal_size(size_t item_count);
static void _kv_sort_entries_by_access(struct kv_entry **entries, 
                                       size_t count);
static uint32_t _kv_crc32(const void *data, size_t length);

/* Thread Safety Helpers (if multithreaded) ----------------------------- */
static void _kv_lock_shared(const kv_store_t *store);
static void _kv_lock_exclusive(kv_store_t *store);
static void _kv_unlock(const kv_store_t *store);

/* Example Usage in main.c ---------------------------------------------- */
int main() {
    kv_store_t *store = NULL;

    // Constructor pattern
    if (kv_store_create(&store, "./data") != KV_SUCCESS) {
        return 1;
    }

    // Module prefix + verb-noun pattern
    kv_item_put(store, "username", "alice", 6);

    // Getter pattern
    if (kv_item_exists(store, "username")) {
        printf("Item count: %zu\n", kv_store_get_item_count(store));
    }

    // Boolean predicate pattern
    if (kv_store_can_accept_more(store, 1024)) {
        printf("Store can accept more data\n");
    }

    // Iterator pattern
    kv_iterator_t *iter = kv_iterator_create(store);
    const char *key;
    const void *value;

    while (kv_iterator_next(iter, &key, &value) == KV_SUCCESS) {
        printf("Key: %s\n", key);
    }

    kv_iterator_destroy(iter);

    // Callback pattern
    kv_for_each(store, print_callback, NULL);

    // Destructor pattern
    kv_store_destroy(store);
    return 0;
}
```