# Section 6: Performance Constraints in Wireshark FSMs

## 6.1 Why FSMs Must Be Extremely Lightweight

```
+------------------------------------------------------------------+
|              WIRESHARK PERFORMANCE REQUIREMENTS                   |
+------------------------------------------------------------------+
|                                                                   |
|  REAL-TIME CAPTURE                                                |
|  =================                                                |
|  - 10 Gbps = ~14.8 million packets/second (minimum size)          |
|  - Must process faster than wire speed to avoid drops             |
|  - Live capture requires sub-microsecond per-packet time          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  LARGE CAPTURE FILES                                              |
|  ====================                                             |
|  - Multi-gigabyte pcap files common                               |
|  - Millions of packets per file                                   |
|  - Users expect reasonable load times                             |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  INTERACTIVE ANALYSIS                                             |
|  ====================                                             |
|  - User scrolls through packets                                   |
|  - Apply display filters                                          |
|  - Re-dissection on demand                                        |
|                                                                   |
+------------------------------------------------------------------+
```

### Per-Packet Time Budget

```
Wire Speed:       10 Gbps
Minimum Packet:   64 bytes
Max Packets/sec:  14,880,952 pps
Time per packet:  ~67 nanoseconds

BUDGET BREAKDOWN (approximate):
================================
+-------------------+-------------+
| Operation         | Time Budget |
+-------------------+-------------+
| Packet capture    | 10-20 ns    |
| Frame dissection  | 20-30 ns    |
| TCP analysis      | 10-15 ns    |
| App dissection    | 10-20 ns    |
| Tree building     | 5-10 ns     |
+-------------------+-------------+
Total:              < 67 ns (ideal)
```

---

## 6.2 Memory Allocation Minimization

### Wireshark Memory Scopes

```
+------------------------------------------------------------------+
|                    WMEM ALLOCATION SCOPES                         |
+------------------------------------------------------------------+
|                                                                   |
|  wmem_epan_scope()                                                |
|  =================                                                |
|  - Lives for duration of application                              |
|  - Protocol registration, static tables                           |
|  - Rarely used for runtime state                                  |
|                                                                   |
|  wmem_file_scope()                                                |
|  =================                                                |
|  - Lives for duration of capture file                             |
|  - Conversation state, reassembly buffers                         |
|  - Freed when new file opened                                     |
|  - MOST FSM STATE LIVES HERE                                      |
|                                                                   |
|  wmem_packet_scope()                                              |
|  ===================                                              |
|  - Lives for single packet dissection                             |
|  - Temporary strings, scratch buffers                             |
|  - Automatically freed after each packet                          |
|  - FAST: No individual frees needed                               |
|                                                                   |
+------------------------------------------------------------------+
```

### Allocation Patterns

```c
// GOOD: Use appropriate scope
static int
dissect_my_protocol(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    // Per-packet temporary - automatically freed
    char *temp_str = wmem_strdup(wmem_packet_scope(), "temp");
    
    // Only allocate persistent state once per conversation
    conversation_t *conv = find_or_create_conversation(pinfo);
    my_state_t *state = conversation_get_proto_data(conv, proto_my);
    if (!state) {
        state = wmem_new0(wmem_file_scope(), my_state_t);
        conversation_add_proto_data(conv, proto_my, state);
    }
}

// BAD: Allocating per packet for persistent data
// (Causes memory growth with packet count)
static int
dissect_bad(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    // DON'T DO THIS - allocates every packet
    state = wmem_new0(wmem_file_scope(), my_state_t);  // Memory leak!
}
```

### Zero-Copy Patterns

```c
// Pattern: Avoid copying packet data
static void
process_data(tvbuff_t *tvb, int offset, int length)
{
    // GOOD: Get pointer to data in tvb (no copy)
    const uint8_t *data = tvb_get_ptr(tvb, offset, length);
    
    // GOOD: Create subset tvb (references original, no copy)
    tvbuff_t *subset = tvb_new_subset_length(tvb, offset, length);
    
    // BAD: Copy data unnecessarily
    // uint8_t *copy = wmem_alloc(wmem_packet_scope(), length);
    // tvb_memcpy(tvb, copy, offset, length);
}
```

---

## 6.3 Hash Table Efficiency

### Conversation Lookup Optimization

```
+------------------------------------------------------------------+
|              CONVERSATION HASH TABLE DESIGN                       |
+------------------------------------------------------------------+
|                                                                   |
|  KEY STRUCTURE:                                                   |
|  ==============                                                   |
|  - Addresses + Ports hashed together                              |
|  - Both directions map to same conversation                       |
|  - O(1) lookup for packet -> conversation                         |
|                                                                   |
|  HASH FUNCTION:                                                   |
|  ==============                                                   |
|  - Fast integer operations                                        |
|  - Good distribution to minimize collisions                       |
|  - Handles IPv4 and IPv6                                          |
|                                                                   |
+------------------------------------------------------------------+

Lookup Path:
============

Packet arrives
     |
     v
Hash(addr1, port1, addr2, port2)
     |
     v
conversation_hashtable_exact_addr_port[hash]
     |
     +---> Found? Return conversation
     |
     v (not found)
Try conversation_hashtable_no_addr2[hash]
     |
     v (not found)
Try conversation_hashtable_no_port2[hash]
     |
     v (not found)
Create new conversation
```

### Per-Stream State Efficiency (HTTP/2)

```c
// From packet-http2.c
typedef struct {
    // Use wmem_map with direct hash for O(1) stream lookup
    wmem_map_t *per_stream_info;  // stream_id -> http2_stream_info_t
    
    // Cache current stream to avoid repeated lookups
    uint32_t current_stream_id;
    
    // Direction-indexed arrays avoid branching
    wmem_queue_t *settings_queue[2];
    nghttp2_hd_inflater *hd_inflater[2];
} http2_session_t;

// Fast stream lookup
static http2_stream_info_t*
get_stream_info(packet_info *pinfo, http2_session_t *h2session, uint32_t stream_id)
{
    // Direct hash lookup - O(1)
    return wmem_map_lookup(h2session->per_stream_info, 
                          GINT_TO_POINTER(stream_id));
}
```

---

## 6.4 FSM Design Impact on Processing Speed

### Design Principles for Fast FSMs

```
+------------------------------------------------------------------+
|              FAST FSM DESIGN PRINCIPLES                           |
+------------------------------------------------------------------+
|                                                                   |
|  1. MINIMIZE STATE LOOKUPS                                        |
|  ==========================                                       |
|  - Cache conversation pointer                                     |
|  - Cache frequently accessed state fields                         |
|  - Use local variables during packet processing                   |
|                                                                   |
|  2. AVOID DEEP CONDITIONALS                                       |
|  ==========================                                       |
|  - Flat switch statements over nested if-else                     |
|  - Table-driven dispatch when possible                            |
|  - Early exit on common cases                                     |
|                                                                   |
|  3. MINIMIZE ALLOCATIONS                                          |
|  =========================                                        |
|  - Pre-allocate fixed-size structures                             |
|  - Use packet-scope for temporaries                               |
|  - Avoid allocating in hot paths                                  |
|                                                                   |
|  4. CACHE-FRIENDLY DATA LAYOUT                                    |
|  ============================                                     |
|  - Keep hot fields together                                       |
|  - Avoid pointer chasing                                          |
|  - Use arrays over linked lists when possible                     |
|                                                                   |
+------------------------------------------------------------------+
```

### Example: Optimized TCP Analysis

```c
// Cache state for fast access during packet processing
struct tcp_analysis {
    // Hot fields at start (likely same cache line)
    tcp_flow_t *fwd;            // Current forward flow
    tcp_flow_t *rev;            // Current reverse flow
    uint32_t stream;            // Stream number
    
    // Flows embedded (no pointer chase)
    tcp_flow_t flow1;
    tcp_flow_t flow2;
    
    // Cold fields (less frequently accessed)
    nstime_t ts_first;
    nstime_t ts_prev;
    wmem_tree_t *acked_table;   // Only needed for detailed analysis
};
```

### Branch Prediction Friendly Code

```c
// GOOD: Common case first, unlikely case later
static int
dissect_tcp_payload(tvbuff_t *tvb, packet_info *pinfo, 
                    struct tcp_analysis *tcpd)
{
    // Common case: normal data packet
    if (likely(tcpd->conversation_completeness & TCP_COMPLETENESS_ACK)) {
        return dissect_normal_data(tvb, pinfo, tcpd);
    }
    
    // Uncommon: connection not yet established
    if (tcpd->conversation_completeness & TCP_COMPLETENESS_SYNACK) {
        // ...
    }
    
    // Rare: no handshake seen (mid-capture start)
    return handle_incomplete_connection(tvb, pinfo, tcpd);
}
```

---

## Performance Summary

```
+------------------------------------------------------------------+
|              WIRESHARK FSM PERFORMANCE CHECKLIST                  |
+------------------------------------------------------------------+
|                                                                   |
|  [ ] Use wmem_packet_scope for per-packet temporaries             |
|  [ ] Use wmem_file_scope for persistent state (allocate once)     |
|  [ ] Use hash tables for O(1) conversation/stream lookup          |
|  [ ] Cache frequently accessed state in local variables           |
|  [ ] Avoid allocations in main dissection path                    |
|  [ ] Use zero-copy tvb operations when possible                   |
|  [ ] Structure hot fields together for cache efficiency           |
|  [ ] Use switch statements over long if-else chains               |
|  [ ] Handle common cases first (branch prediction)                |
|  [ ] Limit recursion depth (stack overflow prevention)            |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 中文解释

### 性能约束

**1. 为什么FSM必须极其轻量**
- 10 Gbps线速 = 每秒约1500万个最小包
- 每包处理时间预算约67纳秒
- 实时捕获需要比线速更快处理
- 大型捕获文件有数百万个包

**2. 最小化内存分配**
- wmem_packet_scope：每包临时数据，自动释放
- wmem_file_scope：持久状态，文件关闭时释放
- 避免每包都分配持久内存
- 使用零拷贝操作访问数据包内容

**3. 哈希表效率**
- 会话查找使用哈希表，O(1)复杂度
- 流状态使用直接哈希映射
- 缓存当前流ID避免重复查找

**4. FSM设计对速度的影响**
- 最小化状态查找：缓存会话指针
- 避免深层条件：使用平铺switch
- 最小化分配：预分配固定大小结构
- 缓存友好布局：热字段放在一起
