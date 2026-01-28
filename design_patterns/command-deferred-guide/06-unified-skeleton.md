# Unified Deferred Execution Skeleton

Generic C skeletons for deferred execution patterns.

---

## Workqueue Skeleton

```c
typedef void (*work_func_t)(void *);

struct work_struct {
    work_func_t func;
    struct work_struct *next;
};

struct workqueue {
    struct work_struct *head;
    struct work_struct *tail;
};

void schedule_work(struct workqueue *wq, struct work_struct *work)
{
    work->next = NULL;
    if (wq->tail)
        wq->tail->next = work;
    else
        wq->head = work;
    wq->tail = work;
}

void worker_run(struct workqueue *wq)
{
    struct work_struct *work;
    while ((work = wq->head)) {
        wq->head = work->next;
        work->func(work);
    }
}
```

**中文说明：**

工作队列骨架：定义work_struct封装工作，schedule_work排队，worker_run处理。

---

## Tasklet Skeleton

```c
struct tasklet_struct {
    void (*func)(unsigned long);
    unsigned long data;
};

void tasklet_schedule(struct tasklet_struct *t);
void tasklet_action(void);  /* Process all scheduled */
```

---

## Version

Based on **Linux kernel v3.2**.
