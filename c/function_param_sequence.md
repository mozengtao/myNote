# C 语言函数参数顺序
## Common Patterns
```c
Pattern					Order						Example
Assignment				dst, src					memcpy(dst, src, n)
Object Method			obj, params...				list_add(list, item)
CRUD Operations			what, how, data				db_op(table, OP_INSERT, record)
Buffer + Size			buf, size					read(fd, buf, size)
Input → Output			in, out						parse(input, &result)
Required → Optional		required, optional, flags	open(path, flags, mode)
Specific → General		specific, general			fprintf(file, format, ...)

int snmpdc_table_object_entry(
    PB_VmcOssiDcType table_id,  // 1. WHAT (identifier)
    PB_SnmpDcOp op,             // 2. HOW (operation)
    void *arg)                  // 3. WITH-WHAT (data)

static int snmpdc_docsIfUpstreamChannelTable_entry(
    PB_SnmpDcOp op,             // 1. HOW (operation - controls behavior)
    void *arg,                  // 2. Input data
    Object *obj,                // 3. Output object
    Entry *entry)               // 4. Output entry

Patterns:
Operation → Input → Outputs
```

## Input Before Output: Input parameters should precede output parameters
```c
// Good: input (src) before output (dst)
int parse_config(const char *filename, Config *out_config);

void memcpy(void *dst, const void *src, size_t n);
// memcpy(dst, src, n) is a notable exception where destination comes first, following the "assignment order" pattern (dst = src)
```
## Context/Handle First： Object handles, contexts, or "this" pointers come first.
```c
// File operations
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

// Object-oriented style in C
int widget_set_color(Widget *widget, Color color);
int db_execute(Database *db, const char *query, Result *result);
```
##  Identifier → Operation → Data： When you have an identifier, operation type, and data:
```c
// Pattern: WHAT → HOW → WITH-WHAT
int table_operation(TableType table_id, Operation op, void *data);
int ioctl(int fd, unsigned long request, void *arg);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
```
## Size/Length Follows Buffer： Buffer size parameters immediately follow their associated buffer
```c
// Good: buffer followed by its size
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *str, int n, FILE *stream);
int snprintf(char *buf, size_t size, const char *format, ...);

// For multiple buffers, each has its size
int compare(const void *a, size_t a_len, const void *b, size_t b_len);
```
## Flags/Options at the End: Optional flags or modifiers typically come last
```c
int open(const char *pathname, int flags);
int open(const char *pathname, int flags, mode_t mode);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int socket(int domain, int type, int protocol);
```
##  Error/Status Output Last: Error codes or status outputs come at the end
```c
long strtol(const char *str, char **endptr, int base);
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res);
```