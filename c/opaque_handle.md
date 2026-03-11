## opaque handle
[Opaque Pointers](https://blog.mbedded.ninja/programming/design-patterns/opaque-pointers/)  
[Incomplete Types](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Incomplete-Types.html)  
[Incomplete Types](https://beej.us/guide/bgc/html/split/incomplete-types.html)  
[Incomplete Array Types](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Incomplete-Array-Types.html)  
[]()  
[]()  
[]()  
[]()  

- incomplete type
  - an incomplete type is a type that's declared but but yet fully defined, the compiler knows that the type exists, but not how big it is or what it contains.
```c
// "incomplete type" acts like a promise:
	// I’ll tell you what this type looks like later — but for now, you can pass pointers around

// why it works even without struct definition in main.c
	// The compiler only needs the type name and pointer size (8 bytes on x86-64) to compile calls and variable declarations.
	// The definition of the struct is only required where you dereference or allocate it (malloc, w->id, etc.).
	// The linker just connects symbols — it doesn’t care about structs at all.

| Concept                       | Meaning                                              |
| ----------------------------- | ---------------------------------------------------- |
| **Incomplete type**           | Type is declared but not defined (size unknown)      |
| **Opaque pointer**            | Pointer to such a type used to hide implementation   |
| **Compile-time in user code** | Compiler only checks pointer usage & prototypes      |
| **Compile-time in impl code** | Compiler knows struct layout                         |
| **Link-time**                 | Functions matched by symbol names, not struct layout |
| **Runtime**                   | Only addresses are passed — type info gone           |


struct Foo;		// declaration only - incomplete type
// what compiler knows:
	// 1. there is a type called 'struct Foo'
	// 2. but its size, members and layout are unknown

// you can use pointers or references to it:
struct Foo *p;							// Ok - pointer to incomplete type
extern struct Foo *foo_create(void);	// Ok
// but cannot
struct Foo f;		// ❌ Error: incomplete type
p->member = 1;		// ❌ Error: struct not defined
sizeof(struct Foo); // ❌ Error: incomplete type

// when you define the structure, it becomes "complete"
struct Foo {
	int a;
}
// what compiler knows:
	// its size, fields
	// how to allocate and access it
struct Foo f;      // ✅ OK
f.a = 10;          // ✅ OK
```

- opaque pointer
  - an opaque pointer is a pointer to an incomplete type used intentionally to hide implementation details
```c
// An opaque pointer is a pointer to an incomplete type that remains incomplete in the public interface, allowing you to compile, link, and run code that manipulates data whose layout is known only to its implementation file.

// don't expose the struct definition in the header file
typedef struct Foo Foo;   // forward-declare and alias

// Foo* is an opaque pointer, users can hold it, pass it abound, but cannot look inside
Foo* foo_create(void);
void foo_destroy(Foo* f);

// "Every opaque pointer is based on an incomplete type, but not every incomplete type is opaque."
// Incomplete type = "compiler doesn’t know layout yet."

// Opaque pointer = "you intentionally keep it incomplete in the public interface."
| Concept         | Example                                             | Visible to user?                              | Used for                           |
| --------------- | --------------------------------------------------- | --------------------------------------------- | ---------------------------------- |
| Incomplete type | `struct A;`                                         | Temporarily incomplete, will be defined later | Internal modular code organization |
| Opaque pointer  | `typedef struct Foo Foo;` (no definition in header) | Intentionally hidden forever                  | Encapsulation / API design         |

```

```c
// .h
typedef struct Foo Foo;
extern Foo* get_Foo(int);

// opaque handle
	Declare public-facing function prototypes in a header (.h),
	Without exposing the internal structure of Foo,
	While still allowing client code to use Foo* handles.

// declares incomplete type + alias
typedef struct Foo Foo;
does the following 2 things:
1. forward declares a struct type named 'struct Foo', meaning:
	"there exists a struct type called struct Foo, but its contents are not defined yet"
2. creates an alias (typedef) called Foo for 'struct Foo'

what means for the compiler:
| Expression        | Meaning                               | Defined?                         |
| ----------------- | ------------------------------------- | -------------------------------- |
| `struct Foo`      | A tag name for a struct type          | ✅ Declared, ❌ Not defined yet |
| `Foo`             | A typedef alias for `struct Foo`      | ✅ Declared, ❌ Not defined yet |

// the compiler can use them for pointer declarations or function prototypes, even though it doesn't yet know what the construct contains.

// pointer to incomplete type
extern Foo* get_Foo(int);
// because the compiler already knows that Foo is a typedef name for an incomplete struct type, it can handle:
1. declaring a pointer to that type (Foo*)
2. declaring a function that returns that pointer

// c allows pointers to incomplete types (like forward-declared structs), because the size and layout of the struct aren't needed to declare or pass around a pointer - only when you dereference it does the full definition become necessary.

```

- example 1
  - code structure
```
Couter library:
	a public header (counter.h) - for users, hides implementation details
	a private implementation (counter.c) - defines the real struct

Users can create, use and destroy counter only through provided API functions, without ever seeing the struct layout.

.
├── counter.c
├── counter.h
└── main.c
```
  - code
```c
// counter.h
#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

/* Forward declaration + typedef alias */
typedef struct Counter Counter;

/* Public API */
Counter* counter_create(void);
void counter_destroy(Counter* c);
void counter_increment(Counter* c);
int counter_get_value(const Counter* c);

#endif

// counter.c
#include "counter.h"
#include <stdlib.h>
#include <stdio.h>

/* Define the struct — private to this file */
struct Counter {
    int value;
};

Counter* counter_create(void) {
    Counter* c = malloc(sizeof(*c));
    if (c) c->value = 0;
    return c;
}

void counter_destroy(Counter* c) {
    free(c);
}

void counter_increment(Counter* c) {
    if (c) c->value++;
}

int counter_get_value(const Counter* c) {
    return c ? c->value : 0;
}

// main.c
#include "counter.h"
#include <stdio.h>

int main(void) {
    Counter* c = counter_create();
    counter_increment(c);
    counter_increment(c);
    printf("Counter = %d\n", counter_get_value(c));
    counter_destroy(c);
    return 0;
}

```
- example 2 (handle-style API version)
```c
// counter.h
#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

/* Fully opaque handle type */
typedef void* CounterHandle;

/* Public API */
CounterHandle counter_create(void);
void counter_destroy(CounterHandle h);
void counter_increment(CounterHandle h);
int counter_get_value(CounterHandle h);

#endif

// counter.c
#include "counter.h"
#include <stdlib.h>
#include <stdio.h>

/* Private internal struct (invisible to users) */
typedef struct Counter {
    int value;
} Counter;

CounterHandle counter_create(void) {
    Counter* c = malloc(sizeof(*c));
    if (c) c->value = 0;
    return (CounterHandle)c;
}

void counter_destroy(CounterHandle h) {
    Counter* c = (Counter*)h;
    free(c);
}

void counter_increment(CounterHandle h) {
    Counter* c = (Counter*)h;
    if (c) c->value++;
}

int counter_get_value(CounterHandle h) {
    Counter* c = (Counter*)h;
    return c ? c->value : 0;
}

// main.c
#include "counter.h"
#include <stdio.h>

int main(void) {
    CounterHandle h = counter_create();
    counter_increment(h);
    counter_increment(h);
    printf("Counter = %d\n", counter_get_value(h));
    counter_destroy(h);
    return 0;
}

// Struct Pointer vs Handle
| Feature                  | `typedef struct Counter Counter;`            | `typedef void* CounterHandle;` |
| ------------------------ | -------------------------------------------  | ------------------------------ |
| Type safety              | ✅ Strong (compiler knows it’s a `Counter*`) | ⚠️ Weak (just a `void*`)       |
| ABI stability            | ⚠️ Needs care if struct layout changes       | ✅ Totally opaque               |
| Can dereference directly | ❌ No                                        | ❌ No                           |
| Common in                | Library APIs, C codebases                    | System APIs, drivers, SDKs     |
| Example                  | POSIX `DIR*`, OpenSSL `SSL*`                 | Win32 `HANDLE`, Vulkan handles |


```
- example 3 (hybrid approach — where we keep type safety but still hide the struct definition (like how FILE* in C stdio works)
  - code
```c
// counter.h
#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

/* Forward declare + typedef alias (type-safe opaque pointer) */
typedef struct Counter Counter;

/* Public API */
Counter* counter_create(void);
void counter_destroy(Counter* c);
void counter_increment(Counter* c);
int counter_get_value(const Counter* c);

#endif

// counter.c
#include "counter.h"
#include <stdlib.h>
#include <stdio.h>

/* Define the struct privately — hidden from users */
struct Counter {
    int value;
};

Counter* counter_create(void) {
    Counter* c = malloc(sizeof(*c));
    if (c) c->value = 0;
    return c;
}

void counter_destroy(Counter* c) {
    free(c);
}

void counter_increment(Counter* c) {
    if (c) c->value++;
}

int counter_get_value(const Counter* c) {
    return c ? c->value : 0;
}

// main.c
#include "counter.h"
#include <stdio.h>

int main(void) {
    Counter* c = counter_create();
    counter_increment(c);
    counter_increment(c);
    printf("Counter = %d\n", counter_get_value(c));
    counter_destroy(c);
    return 0;
}

// Comparison of all three styles
Pattern					Example typedef					Type safety		ABI stability			 Struct hidden?		Common use
Struct pointer (public) typedef struct Counter { 		✅ Strong	  ❌ Low (layout public)	  ❌ No	  			Internal-only APIs
						int value; } Counter;
Opaque handle (void)*	typedef void* CounterHandle;	❌ Weak		  ✅ Very high			  ✅ Fully			System-level APIs
Opaque typed pointer	typedef struct Counter Counter;	✅ Strong	  ✅ High				  ✅ Yes				Most libraries (stdio, OpenSSL, SQLite)
```
- example 4 (multiple opaque types coexist cleanly while keeping each subsystem encapsulated and type-safe)
  - code structure
```
What this achieves:
| Feature                      | Description                                                                              |
| ---------------------------- | ---------------------------------------------------------------------------------------- |
| **Encapsulation per module** | Each `.c` file owns and hides its own `struct`.                                          |
| **Type safety**              | You can’t pass a `Logger*` to a `Connection` function by accident — compiler catches it. |
| **Separation of concerns**   | Each module’s internal details are private.                                              |
| **Stable ABI**               | Struct layouts can change without recompiling user code.                                 |
| **Real-world pattern**       | Matches the style of `FILE*`, `DIR*`, `SSL*`, etc.                                       |

.
├── connection.c
├── connection.h
├── logger.c
├── logger.h
├── main.c
├── session.c
├── session.h
```
  - code
```c
// logger.h
#ifndef LOGGER_H
#define LOGGER_H

typedef struct Logger Logger;

Logger* logger_create(const char* path);
void logger_log(Logger* l, const char* msg);
void logger_destroy(Logger* l);

#endif

// connection.h
#ifndef CONNECTION_H
#define CONNECTION_H

typedef struct Connection Connection;

Connection* connection_open(const char* addr, int port);
void connection_send(Connection* c, const char* data);
void connection_close(Connection* c);

#endif

// session.h
#ifndef SESSION_H
#define SESSION_H

#include "connection.h"
#include "logger.h"

typedef struct Session Session;

Session* session_create(const char* addr, int port, const char* log_path);
void session_send(Session* s, const char* msg);
void session_destroy(Session* s);

#endif

// logger.c
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

struct Logger {
    FILE* fp;
};

Logger* logger_create(const char* path) {
    Logger* l = malloc(sizeof(*l));
    if (!l) return NULL;
    l->fp = fopen(path, "a");
    return l;
}

void logger_log(Logger* l, const char* msg) {
    if (l && l->fp) fprintf(l->fp, "[log] %s\n", msg);
}

void logger_destroy(Logger* l) {
    if (!l) return;
    if (l->fp) fclose(l->fp);
    free(l);
}

// connection.c
#include "connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Connection {
    char addr[64];
    int port;
};

Connection* connection_open(const char* addr, int port) {
    Connection* c = malloc(sizeof(*c));
    if (!c) return NULL;
    snprintf(c->addr, sizeof(c->addr), "%s", addr);
    c->port = port;
    printf("Connecting to %s:%d...\n", addr, port);
    return c;
}

void connection_send(Connection* c, const char* data) {
    printf("[Conn %s:%d] sending: %s\n", c->addr, c->port, data);
}

void connection_close(Connection* c) {
    printf("[Conn %s:%d] closed\n", c->addr, c->port);
    free(c);
}

// session.c
#include "session.h"
#include <stdlib.h>

struct Session {
    Connection* conn;
    Logger* log;
};

Session* session_create(const char* addr, int port, const char* log_path) {
    Session* s = malloc(sizeof(*s));
    if (!s) return NULL;
    s->conn = connection_open(addr, port);
    s->log  = logger_create(log_path);
    return s;
}

void session_send(Session* s, const char* msg) {
    if (!s) return;
    connection_send(s->conn, msg);
    logger_log(s->log, msg);
}

void session_destroy(Session* s) {
    if (!s) return;
    connection_close(s->conn);
    logger_destroy(s->log);
    free(s);
}

// main.c
#include "session.h"
#include <stdio.h>

int main(void) {
    Session* s = session_create("127.0.0.1", 8080, "session.log");
    session_send(s, "Hello, world!");
    session_send(s, "Second message!");
    session_destroy(s);
    return 0;
}

```