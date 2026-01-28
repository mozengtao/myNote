# Hash Table Implementation Guide - Part 5: Comparison and Practical Advice

## Summary Comparison Table

| Method | Collision Strategy | Cache Friendliness | Deletion Complexity | Resize Difficulty | Typical Load Factor | Implementation Complexity | Typical Usage Domain |
|--------|-------------------|-------------------|--------------------|--------------------|---------------------|---------------------------|---------------------|
| **Separate Chaining** | Linked list per bucket | Poor (pointer chasing) | Easy (unlink node) | Medium (rehash all) | 0.7-1.0+ | Low | General purpose, variable data |
| **Intrusive Chaining** | Embedded list nodes | Medium (fewer allocs) | Easy (unlink) | Medium | 0.7-1.0+ | Medium | Kernel/embedded, memory pools |
| **Linear Probing** | Sequential scan | **Excellent** (sequential) | Medium (tombstones) | Medium | 0.5-0.7 | Low | Read-heavy, small entries |
| **Quadratic Probing** | Quadratic steps | Medium (jumping) | Medium (tombstones) | Medium | 0.5-0.7 | Low | Avoiding primary clustering |
| **Double Hashing** | Key-dependent steps | Poor (random jumps) | Medium (tombstones) | Medium | 0.6-0.8 | Medium | High load, no clustering |
| **Robin Hood** | Linear + displacement | **Excellent** | Easy (backward shift) | Medium | **0.8-0.95** | Medium-High | Low latency variance |
| **Cuckoo** | Kick-out to alt table | Good (few accesses) | Easy (direct) | High (may fail) | ~0.5 | High | **O(1) worst-case lookup** |
| **Perfect Hashing** | None (by construction) | **Excellent** (direct) | N/A (static) | N/A | 0.3-0.5 | High | Static key sets |
| **Hopscotch** | Neighborhood + bitmap | **Excellent** (bounded) | Easy (clear bit) | Medium | 0.8-0.9 | High | Concurrent, bounded lookup |
| **HAMT** | Tree expansion | Medium (tree traversal) | Medium (tree modification) | **None** | N/A | High | Large dynamic sets, immutable |

---

## Practical Advice (Hard Rules)

### Rules of Thumb

**Default choice:**
> If you don't know what to use, start with **separate chaining** or **Robin Hood hashing**.

- Separate chaining: when implementation simplicity matters, variable-size data, or load factor > 1.0 possible
- Robin Hood: when performance matters and you want good cache behavior

**By workload:**

| Workload | Recommended |
|----------|-------------|
| Read-heavy, small entries | Linear probing or Robin Hood |
| Write-heavy, many deletes | Separate chaining |
| Static/known keys | Perfect hashing |
| Hard real-time (O(1) guarantee) | Cuckoo hashing |
| Very large datasets, no resize | HAMT |
| Concurrent access | Hopscotch or lock-free chaining |
| Memory-constrained | Robin Hood (high load factor) |
| Embedded/kernel, no malloc | Intrusive chaining |

### When NOT to Use Certain Methods

| Method | Avoid When |
|--------|-----------|
| Linear Probing | Deletions are frequent (tombstone buildup) |
| Quadratic Probing | Cache performance is critical |
| Double Hashing | Hash function is expensive |
| Cuckoo | Memory is limited (only 50% efficient) |
| Perfect Hashing | Keys change after construction |
| Hopscotch | Implementation time is limited |
| HAMT | Need absolute best cache performance |
| Separate Chaining | Cache misses are unacceptable |

### Common Mistakes

1. **Not measuring before optimizing**
   - Profile first. A "slow" hash table might not be your bottleneck.
   - The difference between methods often matters less than the hash function quality.

2. **Ignoring load factor**
   - Open addressing degrades badly above 0.7-0.8
   - Separate chaining handles > 1.0 but with increasing search time
   - Always set resize thresholds explicitly

3. **Bad hash functions**
   - Using modulo with non-prime table sizes and simple hash
   - Not mixing bits properly (e.g., just using pointer addresses)
   - Ignoring key distribution

4. **Memory leaks with tombstones**
   - Tombstones keep accumulating if you never resize
   - Track tombstone count, not just entry count

5. **Forgetting to handle resize failure**
   - malloc can fail during resize
   - Graceful degradation or error handling needed

6. **Overcomplicating for small tables**
   - For N < 100, a linear scan might be faster than any hash table
   - Hash table overhead (hashing, modulo) dominates for tiny tables

7. **Wrong key comparison**
   - Using `==` instead of `strcmp` for strings
   - Not handling NULL keys properly

8. **Concurrent access without synchronization**
   - Even "thread-safe" operations need external locking
   - Or use purpose-built concurrent structures

### When Profiling Justifies Switching

Switch implementations when:

1. **Hash table operations dominate profiling** (> 10% of runtime)
2. **Cache misses are measured** and traced to hash table access patterns
3. **Latency variance** is unacceptable and traced to probe length variance
4. **Memory pressure** is high and current method wastes space
5. **Deletion patterns** cause measured tombstone buildup

Do NOT switch based on:
- Theoretical complexity alone
- "I read that X is faster"
- Premature optimization before measurement

---

## Design Trade-off Summary

```
                      Memory Efficiency
                            ^
                            |
                 HAMT       |
                    \       |
                     \      |      Cuckoo (high space cost)
                      \     |     /
                       \    |    /
         Chaining       +---+---+       Perfect
              \         |       |      /
               \        |       |     /
                +-------+-------+----+
                |       |       |
   Low Latency  |       |       |  Guaranteed O(1)
   Variance     |  Robin Hood   |  Lookup
                |       |       |
                +-------+-------+----+
               /        |       |     \
              /         |       |      \
    Linear Probing      |   Hopscotch   \
                        |               \
                        |           Double Hashing
                        v
                  Cache Friendliness
```

设计权衡总结图说明：
- 没有完美的哈希表实现，每种方法都在某些维度上妥协
- 内存效率 vs 缓存友好性 vs 延迟稳定性 vs 实现复杂度
- 选择取决于你的具体约束和工作负载
- 测量、测量、再测量 —— 理论性能不等于实际性能

---

## Closing Perspective

### Hash Table Choice is a Systems Design Decision

Choosing a hash table implementation is not a coding detail—it's a **systems design decision** that affects:

1. **Memory layout** - How your data sits in RAM, how it interacts with caches
2. **Latency characteristics** - Average vs. worst-case, variance
3. **Failure modes** - What happens when full? What if resize fails?
4. **Operational complexity** - Memory fragmentation over time, garbage collection pressure
5. **Debuggability** - Can you inspect state easily? Are bugs reproducible?

### Simpler Often Wins

In real systems, simpler hash table implementations often outperform "clever" ones:

1. **Predictability beats theoretical optimality** - Operations teams prefer consistent 10μs over occasional 1μs with rare 100ms spikes.

2. **Maintainability has value** - A simple chained hash table that everyone understands beats a complex Robin Hood variant that one person can debug.

3. **Constants matter** - O(1) with a large constant loses to O(log n) with tiny constants for practical n.

4. **Cache effects dominate** - A theoretically worse algorithm with better cache behavior often wins.

5. **Debugging time is real** - Hours spent debugging a subtle cuckoo hashing cycle detector is time not spent on features.

### How to Think About Hash Tables

As an experienced C programmer, think about hash tables this way:

1. **Start with the simplest thing that works.** Profile. Only optimize what's measured as slow.

2. **Understand your access patterns.** Read/write ratio, deletion frequency, key distribution, concurrent access—these determine the right choice.

3. **Know the failure modes.** Every hash table fails somehow. Understand how yours fails and whether that's acceptable.

4. **The hash function matters more than the table.** A good hash function with a simple table beats a bad hash function with a fancy table.

5. **Memory allocation strategy matters.** A hash table that allocates on every insert in a tight loop is often the real problem, not the algorithm.

6. **Benchmark with realistic data.** Synthetic benchmarks lie. Test with your actual keys, your actual access patterns, your actual hardware.

### Final Words

The SELinux `hashtab.c` file you provided is a perfect example of production hash table code:

- Simple separate chaining
- Clear, readable implementation
- Appropriate for its use case (security contexts, moderate size, infrequent updates)
- No over-engineering

This is what good systems code looks like. Not the fanciest algorithm, but the **right algorithm for the job**, implemented clearly and correctly.

When in doubt, remember: **simple, correct, fast—pick three, because with hash tables you often can.**
