# Mapping collectd Concepts to Generic C Architecture

## Part 10: Reusable Patterns Beyond Monitoring

### 10.1 Concept Mapping Table

| collectd Concept | Generic C Architecture | Use Case Example |
|------------------|----------------------|------------------|
| `module_register()` | Plugin/Module entry point | Any plugin system |
| `plugin_register_*()` | Callback registration | Event handlers |
| `value_list_t` | Event/Message structure | Message queue |
| `data_set_t` | Schema/Type definition | Protocol buffer schema |
| `Interval` | Sampling contract | Polling rate |
| `plugin_dispatch_values()` | Event bus / Publish | Pub/sub systems |
| Write plugin | Sink / Backend | Output adapters |
| Read plugin | Source / Producer | Input adapters |
| Filter chain | Middleware / Interceptor | Request processing |
| `user_data_t` | Context / Closure | Callback state |
| `cdtime_t` | High-resolution time | Timer systems |
| `plugin_ctx_t` | Thread-local context | Request context |
| `read_heap` | Priority queue scheduler | Task scheduler |
| `write_queue` | Work queue | Producer-consumer |

**概念映射（中文解释）：**

collectd 的架构模式可以推广到其他系统：

1. **插件系统**：`module_register` + 回调注册
2. **事件总线**：dispatch + 写插件订阅
3. **生产者-消费者**：读线程 + 写队列 + 写线程
4. **优先队列调度**：最小堆 + 时间排序

---

### 10.2 The Plugin ABI Pattern

```c
// Generic plugin ABI (inspired by collectd)

// plugin.h - Public interface
typedef int (*plugin_callback_fn)(void *user_data);

typedef struct {
  void *data;
  void (*free_func)(void *);
} user_data_t;

int plugin_register_callback(const char *name,
                             plugin_callback_fn callback,
                             user_data_t *ud);
int plugin_unregister_callback(const char *name);

// plugin.c - Implementation
typedef struct {
  char *name;
  plugin_callback_fn callback;
  user_data_t user_data;
} callback_entry_t;

static callback_entry_t *callbacks;
static size_t callbacks_num;
static pthread_mutex_t callbacks_lock;

int plugin_register_callback(const char *name,
                             plugin_callback_fn callback,
                             user_data_t *ud) {
  pthread_mutex_lock(&callbacks_lock);
  
  // Grow array
  callbacks = realloc(callbacks, (callbacks_num + 1) * sizeof(*callbacks));
  
  callback_entry_t *e = &callbacks[callbacks_num++];
  e->name = strdup(name);
  e->callback = callback;
  if (ud != NULL)
    e->user_data = *ud;  // Copy
  
  pthread_mutex_unlock(&callbacks_lock);
  return 0;
}

// Plugin implementation
static int my_callback(void *user_data) {
  my_state_t *state = user_data;
  // Do work...
  return 0;
}

void module_register(void) {
  my_state_t *state = calloc(1, sizeof(*state));
  user_data_t ud = { .data = state, .free_func = free };
  plugin_register_callback("my_plugin", my_callback, &ud);
}
```

**通用插件 ABI 模式（中文解释）：**

从 collectd 提取的模式：
1. **回调函数指针**：定义接口契约
2. **user_data**：传递状态，支持闭包
3. **free_func**：自动内存管理
4. **注册函数**：建立回调到执行的映射

---

### 10.3 The Event/Message Pattern

```c
// Generic event structure (inspired by value_list_t)

typedef struct {
  // Routing information
  char source[64];
  char category[64];
  char subcategory[64];
  
  // Temporal information
  uint64_t timestamp_ns;
  uint64_t sequence;
  
  // Payload
  void *data;
  size_t data_len;
  int data_type;
  
  // Metadata
  struct metadata *meta;
} event_t;

#define EVENT_INIT { .data = NULL, .meta = NULL }

// Deep clone for queuing
event_t *event_clone(const event_t *orig) {
  event_t *e = malloc(sizeof(*e));
  memcpy(e, orig, sizeof(*e));
  
  if (orig->data != NULL) {
    e->data = malloc(orig->data_len);
    memcpy(e->data, orig->data, orig->data_len);
  }
  
  e->meta = metadata_clone(orig->meta);
  return e;
}

// Dispatch pattern
int event_dispatch(const event_t *e) {
  event_t *clone = event_clone(e);
  
  pthread_mutex_lock(&queue_lock);
  queue_append(clone);
  pthread_cond_signal(&queue_cond);
  pthread_mutex_unlock(&queue_lock);
  
  return 0;
}
```

**通用事件模式（中文解释）：**

从 `value_list_t` 提取的设计：
1. **路由字段**：类似 plugin/type/instance
2. **时间戳**：事件发生时间
3. **负载**：实际数据
4. **元数据**：可扩展的键值对
5. **深拷贝**：解耦生产者和消费者

---

### 10.4 The Scheduler Pattern

```c
// Priority queue scheduler (inspired by read_heap)

typedef struct {
  void (*callback)(void *);
  void *user_data;
  uint64_t next_run;
  uint64_t interval;
} scheduled_task_t;

typedef struct {
  scheduled_task_t **tasks;
  size_t tasks_num;
  size_t tasks_cap;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} scheduler_t;

// Min-heap operations
static void heap_sift_up(scheduler_t *s, size_t idx);
static void heap_sift_down(scheduler_t *s, size_t idx);

void scheduler_add_task(scheduler_t *s, scheduled_task_t *task) {
  pthread_mutex_lock(&s->lock);
  
  // Grow if needed
  if (s->tasks_num >= s->tasks_cap) {
    s->tasks_cap = s->tasks_cap ? s->tasks_cap * 2 : 16;
    s->tasks = realloc(s->tasks, s->tasks_cap * sizeof(*s->tasks));
  }
  
  // Insert and maintain heap property
  s->tasks[s->tasks_num] = task;
  heap_sift_up(s, s->tasks_num);
  s->tasks_num++;
  
  pthread_cond_signal(&s->cond);
  pthread_mutex_unlock(&s->lock);
}

void *scheduler_thread(void *arg) {
  scheduler_t *s = arg;
  
  while (running) {
    pthread_mutex_lock(&s->lock);
    
    while (s->tasks_num == 0)
      pthread_cond_wait(&s->cond, &s->lock);
    
    scheduled_task_t *task = s->tasks[0];
    uint64_t now = current_time_ns();
    
    if (task->next_run > now) {
      // Sleep until task is ready
      struct timespec ts = ns_to_timespec(task->next_run);
      pthread_cond_timedwait(&s->cond, &s->lock, &ts);
      pthread_mutex_unlock(&s->lock);
      continue;
    }
    
    // Remove from heap
    s->tasks[0] = s->tasks[--s->tasks_num];
    heap_sift_down(s, 0);
    
    pthread_mutex_unlock(&s->lock);
    
    // Execute
    task->callback(task->user_data);
    
    // Reschedule
    task->next_run += task->interval;
    scheduler_add_task(s, task);
  }
  
  return NULL;
}
```

**调度器模式（中文解释）：**

从 collectd 读调度提取：
1. **最小堆**：O(log n) 获取最早任务
2. **条件变量等待**：精确睡眠到下次执行
3. **自动重调度**：执行后更新 `next_run`
4. **并发安全**：锁保护堆操作

---

### 10.5 The Producer-Consumer Pattern

```c
// Thread-safe work queue (inspired by write_queue)

typedef struct queue_item {
  void *data;
  struct queue_item *next;
} queue_item_t;

typedef struct {
  queue_item_t *head;
  queue_item_t *tail;
  size_t length;
  size_t limit;
  pthread_mutex_t lock;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
  bool shutdown;
} work_queue_t;

int queue_enqueue(work_queue_t *q, void *data) {
  pthread_mutex_lock(&q->lock);
  
  // Backpressure: wait if full
  while (q->length >= q->limit && !q->shutdown)
    pthread_cond_wait(&q->not_full, &q->lock);
  
  if (q->shutdown) {
    pthread_mutex_unlock(&q->lock);
    return -1;
  }
  
  queue_item_t *item = malloc(sizeof(*item));
  item->data = data;
  item->next = NULL;
  
  if (q->tail)
    q->tail->next = item;
  else
    q->head = item;
  q->tail = item;
  q->length++;
  
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->lock);
  return 0;
}

void *queue_dequeue(work_queue_t *q) {
  pthread_mutex_lock(&q->lock);
  
  while (q->head == NULL && !q->shutdown)
    pthread_cond_wait(&q->not_empty, &q->lock);
  
  if (q->head == NULL) {
    pthread_mutex_unlock(&q->lock);
    return NULL;
  }
  
  queue_item_t *item = q->head;
  q->head = item->next;
  if (q->head == NULL)
    q->tail = NULL;
  q->length--;
  
  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->lock);
  
  void *data = item->data;
  free(item);
  return data;
}
```

**生产者-消费者模式（中文解释）：**

从写队列提取：
1. **链表队列**：高效的入队出队
2. **双条件变量**：`not_empty` 和 `not_full`
3. **背压**：队列满时阻塞生产者
4. **优雅关闭**：`shutdown` 标志唤醒等待者

---

### 10.6 Avoiding collectd-Specific Constraints

When reusing these patterns, be aware of collectd's constraints:

| collectd Constraint | Why It Exists | Generic Alternative |
|---------------------|---------------|---------------------|
| Fixed-size name fields | Memory efficiency, RRD compatibility | Use heap strings |
| Global hostname | Single-host monitoring | Per-event source ID |
| types.db requirement | RRD data source validation | Dynamic schema |
| Fixed 4 value types | RRD compatibility | Tagged union with more types |
| Interval as core concept | Time-series sampling | Event-driven or hybrid |
| cdtime_t representation | Efficiency, nanosecond precision | Standard `timespec` or custom |

**避免 collectd 特定约束（中文解释）：**

在通用系统中可以移除的约束：

1. **固定长度字符串**：在非监控系统中，使用堆分配字符串更灵活
2. **全局主机名**：分布式系统中每个事件应有独立来源标识
3. **types.db**：如果不需要 RRD 兼容，可以使用动态模式
4. **四种值类型**：可以扩展为更多类型（字符串、数组等）
5. **固定间隔**：事件驱动系统可能不需要固定采样间隔

---

### 10.7 Example: A Generic Data Pipeline

```c
// Combining patterns into a generic pipeline

// 1. Define event structure
typedef struct {
  char source[128];
  char type[64];
  uint64_t timestamp;
  double value;
  struct metadata *meta;
} metric_event_t;

// 2. Define plugin interface
typedef int (*read_fn)(void *);
typedef int (*write_fn)(const metric_event_t *, void *);

// 3. Scheduler for read plugins
scheduler_t *read_scheduler;

// 4. Queue for events
work_queue_t *event_queue;

// 5. Write plugin dispatch
void dispatch_to_writers(metric_event_t *e) {
  for (each registered write plugin) {
    write_fn fn = writer->callback;
    fn(e, writer->user_data);
  }
}

// 6. Writer thread
void *writer_thread(void *arg) {
  while (running) {
    metric_event_t *e = queue_dequeue(event_queue);
    if (e == NULL) break;
    
    dispatch_to_writers(e);
    event_free(e);
  }
  return NULL;
}

// 7. Main initialization
int main() {
  read_scheduler = scheduler_create();
  event_queue = queue_create(10000);
  
  // Load plugins
  load_plugins_from_dir("/usr/lib/mypipeline/");
  
  // Start worker threads
  for (int i = 0; i < num_writers; i++)
    pthread_create(&writer_threads[i], NULL, writer_thread, NULL);
  
  // Scheduler runs in its own threads
  for (int i = 0; i < num_readers; i++)
    pthread_create(&reader_threads[i], NULL, scheduler_thread, read_scheduler);
  
  // Wait for shutdown signal
  wait_for_signal();
  
  // Graceful shutdown
  queue_shutdown(event_queue);
  scheduler_shutdown(read_scheduler);
  
  // Join threads
  // ...
  
  return 0;
}
```

**通用数据管道示例（中文解释）：**

这个简化版本展示了如何组合 collectd 的模式：
1. 事件结构（简化的 value_list）
2. 插件接口（read/write 回调）
3. 调度器（类似 read_heap）
4. 工作队列（类似 write_queue）
5. 写分发（类似 plugin_write）

---

### 10.8 Learning Outcomes

After reading this section, you should be able to:

- [ ] Map collectd concepts to generic architecture patterns
- [ ] Extract and reuse the plugin ABI pattern
- [ ] Implement a generic event/message structure
- [ ] Build a priority queue scheduler
- [ ] Create a thread-safe producer-consumer queue
- [ ] Identify which collectd constraints to keep or remove
- [ ] Design a complete data pipeline using these patterns
