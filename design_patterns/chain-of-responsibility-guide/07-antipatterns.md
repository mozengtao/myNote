# Chain of Responsibility Anti-Patterns

Common mistakes to avoid when implementing Chain of Responsibility.

---

## Anti-Pattern 1: Greedy Handler

```c
/* BAD: Handler claims everything */
int greedy_handler(void *data)
{
    /* Always returns HANDLED even for things it can't handle */
    process(data);
    return HANDLED;  /* Blocks all other handlers! */
}

/* CORRECT: Only handle what you can */
int correct_handler(void *data)
{
    struct request *req = data;
    
    if (!can_handle(req))
        return NOT_HANDLED;  /* Pass to next */
    
    process(req);
    return HANDLED;
}
```

**中文说明：**

反模式1：贪婪处理器——处理器声称能处理所有请求，阻止其他处理器。正确做法是只处理自己能处理的。

---

## Anti-Pattern 2: No Handler Found

```c
/* BAD: No default handler, request unhandled */
int process_chain(void *data)
{
    struct handler *h;
    
    for (h = chain; h; h = h->next) {
        if (h->handle(data) == HANDLED)
            return HANDLED;
    }
    
    /* No handler found - request dropped! */
    return NOT_HANDLED;  /* Caller may not check this */
}

/* CORRECT: Have a default handler */
static struct handler default_handler = {
    .handle = default_handle,
    .priority = 999,  /* Lowest priority */
};

/* Or check and report */
int process_chain(void *data)
{
    struct handler *h;
    
    for (h = chain; h; h = h->next) {
        if (h->handle(data) == HANDLED)
            return HANDLED;
    }
    
    pr_warn("No handler for request!\n");
    return NOT_HANDLED;
}
```

---

## Anti-Pattern 3: Wrong Priority Order

```c
/* BAD: Specific handler has lower priority than generic */
static struct handler generic = { .priority = 100 };  /* Catches all */
static struct handler specific = { .priority = 200 }; /* Never reached! */

/* CORRECT: Specific handlers first */
static struct handler specific = { .priority = 100 };  /* Checked first */
static struct handler generic = { .priority = 200 };   /* Fallback */
```

**中文说明：**

反模式3：优先级顺序错误——通用处理器优先级高于特定处理器。正确做法是特定处理器优先级高。

---

## Anti-Pattern 4: Modifying During Iteration

```c
/* BAD: Unregistering while processing */
int my_handler(void *data)
{
    /* This can corrupt the chain! */
    unregister_handler(&another_handler);
    return HANDLED;
}

/* CORRECT: Defer modifications */
int my_handler(void *data)
{
    schedule_unregister(&another_handler);
    return HANDLED;
}
/* Actual unregister happens after chain processing */
```

---

## Anti-Pattern 5: Ignoring Return Values

```c
/* BAD: Caller doesn't check if handled */
void bad_caller(void *data)
{
    process_chain(data);
    /* Don't know if handled! */
}

/* CORRECT: Check and handle failures */
void good_caller(void *data)
{
    if (process_chain(data) == NOT_HANDLED) {
        handle_unhandled_request(data);
    }
}
```

---

## Summary Checklist

```
+=============================================================================+
|                    CHAIN SAFE USAGE                                          |
+=============================================================================+

    [X] Handlers only claim what they can handle
    [X] Have default handler or check for unhandled
    [X] Specific handlers before generic
    [X] Don't modify chain during iteration
    [X] Caller checks return value
    [X] Clear return value semantics
```

---

## Version

Based on **Linux kernel v3.2** chain patterns.
