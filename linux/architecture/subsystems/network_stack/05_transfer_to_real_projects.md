# TRANSFER｜应用到实际项目

## 1. 用户空间流水线设计

```
PIPELINE DESIGN IN USER-SPACE
+=============================================================================+
|                                                                              |
|  KERNEL PATTERN: PACKET PIPELINE                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux network stack design:                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐           │    │ |
|  │  │  │Stage1│──►│Stage2│──►│Stage3│──►│Stage4│──►│Stage5│           │    │ |
|  │  │  │ NIC  │   │ GRO  │   │  IP  │   │ TCP  │   │Socket│           │    │ |
|  │  │  └──────┘   └──────┘   └──────┘   └──────┘   └──────┘           │    │ |
|  │  │                                                                  │    │ |
|  │  │  KEY CHARACTERISTICS:                                            │    │ |
|  │  │  • Single sk_buff flows through all stages                       │    │ |
|  │  │  • Each stage modifies metadata, rarely copies data              │    │ |
|  │  │  • Function calls connect stages (not queues)                    │    │ |
|  │  │  • Hooks allow extension at each stage                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE TRANSFER: MESSAGE PROCESSING PIPELINE                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Example: RPC framework or message broker                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Message carrier (like sk_buff)                               │    │ |
|  │  │  struct message {                                                │    │ |
|  │  │      // Data pointers (like sk_buff's head/data/tail)            │    │ |
|  │  │      uint8_t *buffer;       // Start of allocated buffer         │    │ |
|  │  │      uint8_t *data;         // Start of current data             │    │ |
|  │  │      size_t   len;          // Length of data                    │    │ |
|  │  │      size_t   capacity;     // Buffer capacity                   │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Metadata (accumulated through pipeline)                  │    │ |
|  │  │      uint32_t msg_type;                                          │    │ |
|  │  │      uint64_t timestamp;                                         │    │ |
|  │  │      uint32_t source_id;                                         │    │ |
|  │  │      uint32_t flags;                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Control buffer for stage-private data (like skb->cb)     │    │ |
|  │  │      uint8_t  cb[64];                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Reference counting                                       │    │ |
|  │  │      atomic_int refcount;                                        │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Operations (like skb_push, skb_pull)                         │    │ |
|  │  │  void *msg_push(struct message *m, size_t len);  // Prepend      │    │ |
|  │  │  void *msg_pull(struct message *m, size_t len);  // Strip        │    │ |
|  │  │  struct message *msg_clone(struct message *m);   // Share data   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Pipeline stages (like netif_receive_skb → ip_rcv → tcp_rcv):            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  typedef int (*stage_fn)(struct message *m, void *ctx);          │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct pipeline {                                               │    │ |
|  │  │      stage_fn stages[MAX_STAGES];                                │    │ |
|  │  │      void    *stage_ctx[MAX_STAGES];                             │    │ |
|  │  │      int      num_stages;                                        │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  int pipeline_process(struct pipeline *p, struct message *m) {   │    │ |
|  │  │      for (int i = 0; i < p->num_stages; i++) {                   │    │ |
|  │  │          int result = p->stages[i](m, p->stage_ctx[i]);          │    │ |
|  │  │          if (result != CONTINUE)                                 │    │ |
|  │  │              return result;  // DROP, STOLEN, etc.               │    │ |
|  │  │      }                                                           │    │ |
|  │  │      return ACCEPT;                                              │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Concrete stages                                              │    │ |
|  │  │  int stage_deserialize(struct message *m, void *ctx) {           │    │ |
|  │  │      // Parse wire format, strip header                          │    │ |
|  │  │      struct header *hdr = (struct header *)m->data;              │    │ |
|  │  │      m->msg_type = hdr->type;                                    │    │ |
|  │  │      msg_pull(m, sizeof(struct header));                         │    │ |
|  │  │      return CONTINUE;                                            │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  int stage_authenticate(struct message *m, void *ctx) {          │    │ |
|  │  │      if (!verify_signature(m)) return DROP;                      │    │ |
|  │  │      return CONTINUE;                                            │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  int stage_route(struct message *m, void *ctx) {                 │    │ |
|  │  │      // Like IP routing: determine where to send                 │    │ |
|  │  │      m->destination = lookup_destination(m->msg_type);           │    │ |
|  │  │      return CONTINUE;                                            │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  int stage_dispatch(struct message *m, void *ctx) {              │    │ |
|  │  │      // Like socket delivery                                     │    │ |
|  │  │      handler_fn handler = handlers[m->msg_type];                 │    │ |
|  │  │      return handler(m);                                          │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  HOOK SYSTEM (LIKE NETFILTER)                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  typedef int (*hook_fn)(struct message *m, void *priv);                  │ |
|  │                                                                          │ |
|  │  struct hook {                                                           │ |
|  │      hook_fn       func;                                                 │ |
|  │      void         *priv;                                                 │ |
|  │      int           priority;  // Like NF_IP_PRI_*                        │ |
|  │      struct hook  *next;                                                 │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  struct hook *hooks[NUM_HOOK_POINTS];                                    │ |
|  │                                                                          │ |
|  │  int run_hooks(int hook_point, struct message *m) {                      │ |
|  │      for (struct hook *h = hooks[hook_point]; h; h = h->next) {          │ |
|  │          int result = h->func(m, h->priv);                               │ |
|  │          if (result == DROP) return DROP;                                │ |
|  │          if (result == STOLEN) return STOLEN;                            │ |
|  │      }                                                                   │ |
|  │      return ACCEPT;                                                      │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  // Usage in pipeline stage                                              │ |
|  │  int stage_with_hooks(struct message *m, void *ctx) {                    │ |
|  │      if (run_hooks(HOOK_PRE_ROUTE, m) != ACCEPT)                         │ |
|  │          return DROP;                                                    │ |
|  │                                                                          │ |
|  │      // Do actual routing...                                             │ |
|  │                                                                          │ |
|  │      if (run_hooks(HOOK_POST_ROUTE, m) != ACCEPT)                        │ |
|  │          return DROP;                                                    │ |
|  │                                                                          │ |
|  │      return CONTINUE;                                                    │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**用户空间流水线设计**：

**内核模式**：包流水线
- 单个 sk_buff 流过所有阶段
- 每阶段修改元数据，很少复制数据
- 函数调用连接阶段
- 钩子允许扩展

**迁移到用户空间**：消息处理流水线
- **消息载体**（类似 sk_buff）：buffer/data/len 指针、元数据、控制缓冲区（cb）、引用计数
- **操作**：msg_push/msg_pull（类似 skb_push/skb_pull）
- **流水线阶段**：反序列化 → 认证 → 路由 → 分发

**钩子系统**（类似 Netfilter）：
- 注册钩子函数，带优先级
- 运行钩子链，支持 DROP/STOLEN/ACCEPT

---

## 2. 零拷贝经验

```
ZERO-COPY LESSONS
+=============================================================================+
|                                                                              |
|  KERNEL ZERO-COPY TECHNIQUES                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. POINTER MANIPULATION (sk_buff)                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Instead of copying:                                             │    │ |
|  │  │    memcpy(new_buf, old_buf + header_len, payload_len);           │    │ |
|  │  │                                                                  │    │ |
|  │  │  Just move pointer:                                              │    │ |
|  │  │    skb_pull(skb, header_len);  // skb->data += header_len        │    │ |
|  │  │                                                                  │    │ |
|  │  │  Requires: Pre-allocated headroom for adding headers             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. SCATTER-GATHER (fragments)                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  sk_buff with fragments:                                         │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ Linear data (headers) │ Page 1 │ Page 2 │ Page 3 │          │ │    │ |
|  │  │  │ (copied)              │ (ref)  │ (ref)  │ (ref)  │          │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Data pages are referenced, not copied                           │    │ |
|  │  │  NIC uses scatter-gather DMA                                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. SENDFILE / SPLICE                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Traditional:  file → user buffer → socket                       │    │ |
|  │  │                       ^^^ copy        ^^^ copy                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  sendfile():   file → socket (kernel-to-kernel)                  │    │ |
|  │  │                      no user-space copy!                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  With modern NICs:  file → DMA to NIC (truly zero-copy)          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  4. MSG_ZEROCOPY                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  send(sock, buf, len, MSG_ZEROCOPY);                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  • Kernel pins user pages                                        │    │ |
|  │  │  • DMA directly from user buffer                                 │    │ |
|  │  │  • Notification when safe to reuse buffer                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  Trade-off: Overhead of pinning, only worth it for large sends   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  APPLYING ZERO-COPY IN USER-SPACE                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Pattern 1: Pre-allocated buffers with headroom                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct buffer_pool {                                            │    │ |
|  │  │      uint8_t *chunks;                                            │    │ |
|  │  │      size_t   chunk_size;  // e.g., 4096                         │    │ |
|  │  │      size_t   headroom;    // e.g., 128 for headers              │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct message *alloc_message(struct buffer_pool *pool) {       │    │ |
|  │  │      struct message *m = ...;                                    │    │ |
|  │  │      m->buffer = get_chunk(pool);                                │    │ |
|  │  │      m->data = m->buffer + pool->headroom;  // Leave room        │    │ |
|  │  │      return m;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void add_header(struct message *m, void *header, size_t len) {  │    │ |
|  │  │      m->data -= len;  // Move pointer back                       │    │ |
|  │  │      memcpy(m->data, header, len);  // Only copy header          │    │ |
|  │  │      m->len += len;                                              │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Pattern 2: Reference-counted data sharing                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct shared_data {                                            │    │ |
|  │  │      atomic_int refcount;                                        │    │ |
|  │  │      uint8_t    data[];                                          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct message {                                                │    │ |
|  │  │      struct shared_data *sdata;                                  │    │ |
|  │  │      size_t   offset;    // Into sdata->data                     │    │ |
|  │  │      size_t   len;                                               │    │ |
|  │  │      // ... metadata                                             │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Clone without copying data (like skb_clone)                  │    │ |
|  │  │  struct message *clone_message(struct message *orig) {           │    │ |
|  │  │      struct message *m = alloc_message_struct();                 │    │ |
|  │  │      m->sdata = orig->sdata;                                     │    │ |
|  │  │      atomic_inc(&m->sdata->refcount);  // Share data             │    │ |
|  │  │      m->offset = orig->offset;                                   │    │ |
|  │  │      m->len = orig->len;                                         │    │ |
|  │  │      return m;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void free_message(struct message *m) {                          │    │ |
|  │  │      if (atomic_dec_and_test(&m->sdata->refcount))               │    │ |
|  │  │          free(m->sdata);                                         │    │ |
|  │  │      free(m);                                                    │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Pattern 3: Writev/Readv for scatter-gather I/O                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Instead of copying multiple buffers together:                │    │ |
|  │  │  struct iovec iov[3];                                            │    │ |
|  │  │  iov[0].iov_base = header;                                       │    │ |
|  │  │  iov[0].iov_len = header_len;                                    │    │ |
|  │  │  iov[1].iov_base = metadata;                                     │    │ |
|  │  │  iov[1].iov_len = metadata_len;                                  │    │ |
|  │  │  iov[2].iov_base = payload;                                      │    │ |
|  │  │  iov[2].iov_len = payload_len;                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  writev(fd, iov, 3);  // One syscall, no intermediate copy       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**零拷贝经验**：

**内核零拷贝技术**：
1. **指针操作**：`skb_pull()` 只移动指针，不复制
2. **Scatter-Gather**：数据页引用而非复制，NIC 使用 SG DMA
3. **sendfile/splice**：file → socket 内核到内核，无用户空间拷贝
4. **MSG_ZEROCOPY**：内核固定用户页，DMA 直接从用户缓冲区

**用户空间应用**：
- **模式 1**：预分配缓冲区带 headroom，添加头部只需移动指针
- **模式 2**：引用计数数据共享（类似 skb_clone）
- **模式 3**：writev/readv 用于 scatter-gather I/O

---

## 3. 何时内核风格网络是过度设计

```
WHEN KERNEL-STYLE NETWORKING IS OVERKILL
+=============================================================================+
|                                                                              |
|  KERNEL STACK: DESIGNED FOR EXTREME SCALE                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  The Linux network stack is optimized for:                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Millions of packets per second                                │    │ |
|  │  │  • Thousands of concurrent connections                           │    │ |
|  │  │  • Multi-core scalability (100+ CPUs)                            │    │ |
|  │  │  • Hardware diversity (100s of NIC drivers)                      │    │ |
|  │  │  • Protocol diversity (TCP, UDP, SCTP, raw, ...)                 │    │ |
|  │  │  • Security (privilege separation, namespaces)                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  This comes with COMPLEXITY COST:                                │    │ |
|  │  │  • ~200 byte sk_buff struct                                      │    │ |
|  │  │  • Multiple locking primitives                                   │    │ |
|  │  │  • Indirection through ops-tables                                │    │ |
|  │  │  • Netfilter hooks overhead                                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  WHEN SIMPLER IS BETTER                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  CASE 1: Low message rate                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  If processing < 10K messages/second:                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  DON'T: Complex zero-copy, reference counting                    │    │ |
|  │  │  DO:    Simple malloc/free, memcpy                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Simple is fine                                               │    │ |
|  │  │  struct message *m = malloc(sizeof(*m) + len);                   │    │ |
|  │  │  memcpy(m->data, received_data, len);                            │    │ |
|  │  │  process(m);                                                     │    │ |
|  │  │  free(m);                                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  Overhead: ~100ns for malloc + memcpy(1KB)                       │    │ |
|  │  │  At 10K/s: 1ms total overhead (negligible)                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CASE 2: Single protocol                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  If you only have ONE message format:                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  DON'T: Protocol ops-tables, type dispatch                       │    │ |
|  │  │  DO:    Direct function calls, compile-time types                │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Bad: unnecessary indirection                                 │    │ |
|  │  │  handlers[msg->type]->process(msg);                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Good: direct call                                            │    │ |
|  │  │  process_my_message(msg);                                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CASE 3: Single-threaded processing                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  If one thread handles all messages:                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  DON'T: Atomic refcounting, per-CPU caches, lock-free queues     │    │ |
|  │  │  DO:    Simple pointers, regular queues                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Kernel style: needed for multi-CPU                           │    │ |
|  │  │  atomic_dec_and_test(&skb->users);                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Simple style: fine for single thread                         │    │ |
|  │  │  if (--msg->refcount == 0) free(msg);                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CASE 4: Fixed message sizes                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  If all messages are same size:                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  DON'T: Variable-size buffers, headroom management               │    │ |
|  │  │  DO:    Fixed-size pool allocation                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct message_pool {                                           │    │ |
|  │  │      struct message msgs[1024];                                  │    │ |
|  │  │      int free_list[1024];                                        │    │ |
|  │  │      int free_head;                                              │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // O(1) alloc/free, cache-friendly                              │    │ |
|  │  │  struct message *alloc_msg(struct message_pool *p) {             │    │ |
|  │  │      return &p->msgs[p->free_list[p->free_head++]];              │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DECISION FRAMEWORK                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Use kernel patterns when:                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ✓ Message rate > 100K/second                                    │    │ |
|  │  │  ✓ Multiple message types requiring dispatch                     │    │ |
|  │  │  ✓ Multi-threaded processing                                     │    │ |
|  │  │  ✓ Need to share messages between components                     │    │ |
|  │  │  ✓ Variable-size messages with header manipulation               │    │ |
|  │  │  ✓ Need extensible hook points                                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Use simple patterns when:                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ✓ Message rate < 10K/second                                     │    │ |
|  │  │  ✓ Single message format                                         │    │ |
|  │  │  ✓ Single-threaded or coarse-grained threading                   │    │ |
|  │  │  ✓ Messages consumed immediately                                 │    │ |
|  │  │  ✓ Fixed-size messages                                           │    │ |
|  │  │  ✓ Closed system with known components                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SUMMARY: KEY TRANSFERABLE IDEAS                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. SINGLE CARRIER OBJECT                                                │ |
|  │     • sk_buff carries packet + metadata through entire stack             │ |
|  │     • Apply when: data flows through multiple processing stages          │ |
|  │                                                                          │ |
|  │  2. POINTER-BASED HEADER MANIPULATION                                    │ |
|  │     • skb_push/pull avoids copying                                       │ |
|  │     • Apply when: frequent header add/remove with same data              │ |
|  │                                                                          │ |
|  │  3. PROTOCOL POLYMORPHISM VIA OPS TABLES                                 │ |
|  │     • struct proto, net_device_ops                                       │ |
|  │     • Apply when: multiple protocol/driver implementations               │ |
|  │                                                                          │ |
|  │  4. STRATEGIC HOOK POINTS                                                │ |
|  │     • Netfilter hooks at key points in path                              │ |
|  │     • Apply when: need extensibility without modifying core path         │ |
|  │                                                                          │ |
|  │  5. BATCH PROCESSING                                                     │ |
|  │     • NAPI polls multiple packets                                        │ |
|  │     • Apply when: high-rate processing with shared setup cost            │ |
|  │                                                                          │ |
|  │  6. REFERENCE COUNTING FOR SHARED OWNERSHIP                              │ |
|  │     • skb->users, skb_clone                                              │ |
|  │     • Apply when: same data viewed by multiple components                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**何时内核风格网络是过度设计**：

内核栈针对极端规模优化：百万包/秒、千级并发连接、100+ CPU 可扩展性

**更简单更好的情况**：

**案例 1：低消息率**（< 10K/秒）
- 不需要：复杂零拷贝、引用计数
- 简单 malloc/free、memcpy 就够

**案例 2：单一协议**
- 不需要：协议操作表、类型分发
- 直接函数调用

**案例 3：单线程处理**
- 不需要：原子引用计数、per-CPU 缓存、无锁队列
- 简单指针、普通队列

**案例 4：固定消息大小**
- 不需要：可变大小缓冲区、headroom 管理
- 固定大小池分配

**决策框架**：
- 使用内核模式：消息率 > 100K/秒、多消息类型、多线程、需要共享消息、可变大小消息、需要扩展钩子点
- 使用简单模式：消息率 < 10K/秒、单消息格式、单线程、消息立即消费、固定大小、封闭系统

**关键可迁移思想**：
1. 单一载体对象（sk_buff）
2. 基于指针的头部操作
3. 协议多态通过操作表
4. 战略性钩子点
5. 批处理
6. 引用计数共享所有权
