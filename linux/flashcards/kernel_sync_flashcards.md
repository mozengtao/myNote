# Linux Kernel Synchronization Mechanisms Flashcards
## Kernel Version: v3.2

---

# Section 1: Fundamentals and Race Conditions

Q: What is a race condition in the context of kernel programming?
A: A race condition occurs when two or more execution contexts access shared data concurrently, and at least one access is a write, leading to unpredictable results depending on the timing/order of execution.
[Basic]

Q: What are the three main sources of concurrency in the Linux kernel?
A: 1) SMP (Symmetric Multiprocessing) - multiple CPUs executing kernel code simultaneously
2) Preemption - kernel code can be preempted by higher-priority tasks
3) Interrupts - hardware/software interrupts can occur at almost any point
[Basic]

Q: Define "critical section" in kernel synchronization.
A: A critical section is a code region that accesses shared resources and must be executed atomically (without interruption by other contexts that access the same resources) to prevent race conditions.
[Basic]

Q: What is mutual exclusion and why is it necessary?
A: Mutual exclusion ensures that only one execution context can execute a critical section at a time. It prevents race conditions by serializing access to shared data, maintaining data consistency.
[Basic]

Q: [Cloze] A race condition requires at least ___ concurrent accesses where at least ___ is a write operation.
A: two; one
[Basic]

Q: What is the difference between a "safe" and "unsafe" race condition?
A: A safe race condition is one where the outcome doesn't affect correctness (e.g., statistics counters where slight inaccuracy is acceptable). An unsafe race condition corrupts data or causes incorrect program behavior.
[Intermediate]

Q: Why can't you simply disable interrupts to solve all synchronization problems on SMP systems?
A: Disabling interrupts only affects the local CPU. On SMP systems, other CPUs continue executing and can still access shared data concurrently. You need proper locking mechanisms that work across all CPUs.
[Intermediate]

Q: What are the four Coffman conditions for deadlock?
A: 1) Mutual Exclusion - resources cannot be shared
2) Hold and Wait - process holds resources while waiting for others
3) No Preemption - resources cannot be forcibly taken
4) Circular Wait - circular chain of processes waiting for each other
[Basic]

Q: How can you prevent deadlock by breaking the "Circular Wait" condition?
A: Establish a global ordering of all locks and always acquire locks in that order. If you need locks A and B, and A < B in the ordering, always acquire A before B.
[Intermediate]

Q: What is a livelock and how does it differ from deadlock?
A: In a livelock, processes are not blocked but continuously change state in response to each other without making progress. Unlike deadlock where processes are stuck waiting, livelock processes are actively executing but accomplishing nothing.
[Intermediate]

Q: What is priority inversion?
A: Priority inversion occurs when a high-priority task is blocked waiting for a resource held by a low-priority task, which itself is preempted by medium-priority tasks. The high-priority task effectively runs at low priority.
[Intermediate]

Q: What is priority inheritance and how does it solve priority inversion?
A: Priority inheritance temporarily raises the priority of a low-priority task holding a lock to match the highest-priority task waiting for that lock. This prevents medium-priority tasks from preempting the lock holder.
[Intermediate]

Q: [Reverse] The condition where a high-priority process waits for a low-priority process which is preempted by medium-priority processes.
A: Priority Inversion
[Basic]

Q: What happens if kernel code sleeps while holding a spinlock?
A: This can cause deadlock. If the CPU is needed by the task that would release the lock (or another task in the wait path), the system hangs. Spinlocks must never be held across sleeping operations.
[Intermediate]

Q: In what order should nested locks be released?
A: Locks should be released in the reverse order of acquisition (LIFO - Last In, First Out). If you acquire A then B, you should release B then A.
[Basic]

Q: What is lock contention?
A: Lock contention occurs when multiple execution contexts frequently compete for the same lock, causing delays as contexts wait for lock availability. High contention degrades performance and scalability.
[Basic]

Q: [ASCII Diagram] Visualize a simple race condition scenario:
A:
```
CPU 0                    CPU 1
─────                    ─────
read counter (=5)        
                         read counter (=5)
increment (6)            
                         increment (6)
write counter (=6)       
                         write counter (=6)
                         
Result: counter = 6 (should be 7!)
```
[Basic]

Q: What is the "check-then-act" race condition pattern?
A: A race where code checks a condition and then acts on it, but the condition can change between check and act:
```c
if (ptr != NULL)    /* check */
    use(ptr);       /* act - ptr may be NULL now! */
```
Another context may set ptr to NULL between the check and use.
[Intermediate]

Q: What does "atomic" mean in the context of kernel operations?
A: An atomic operation completes entirely without interruption - no other CPU or context can observe an intermediate state. The operation appears to happen instantaneously from the perspective of other observers.
[Basic]

Q: Why is read-modify-write on a shared variable problematic without synchronization?
A: Read-modify-write (like `counter++`) involves three steps: read current value, modify it, write back. Another CPU can read/modify the same variable between these steps, causing lost updates.
[Basic]

Q: What is the difference between "data race" and "race condition"?
A: A data race is specifically unsynchronized concurrent access to shared memory (one write). A race condition is the broader concept of timing-dependent incorrect behavior. All data races are race conditions, but not all race conditions involve data races.
[Advanced]

---

# Section 2: Atomic Operations

Q: What is `atomic_t` in the Linux kernel?
A: `atomic_t` is an opaque type that holds an integer value and guarantees atomic access. Defined in `<linux/types.h>` as:
```c
typedef struct {
    int counter;
} atomic_t;
```
All operations on it are guaranteed to be atomic.
[Basic]

Q: Why can't you directly access the value inside `atomic_t` like `v.counter++`?
A: Direct access bypasses atomic guarantees. The compiler may optimize or reorder operations, and on some architectures, simple increment isn't atomic. Always use atomic_*() functions to ensure proper memory barriers and atomicity.
[Basic]

Q: What does `atomic_read(v)` do?
A: Reads and returns the current value of the atomic variable `v`. While a simple read of an aligned int is typically atomic on most architectures, this function ensures proper semantics and compiler barriers.
```c
int val = atomic_read(&counter);
```
[Basic]

Q: What does `atomic_set(v, i)` do?
A: Sets the atomic variable `v` to the integer value `i`:
```c
atomic_set(&counter, 0);  /* Set counter to 0 */
```
[Basic]

Q: What does `atomic_add(i, v)` do?
A: Atomically adds integer `i` to atomic variable `v`:
```c
atomic_add(5, &counter);  /* counter += 5 atomically */
```
Note: parameter order is (value, variable).
[Basic]

Q: What does `atomic_sub(i, v)` do?
A: Atomically subtracts integer `i` from atomic variable `v`:
```c
atomic_sub(3, &counter);  /* counter -= 3 atomically */
```
[Basic]

Q: What do `atomic_inc(v)` and `atomic_dec(v)` do?
A: `atomic_inc()` atomically increments v by 1.
`atomic_dec()` atomically decrements v by 1.
```c
atomic_inc(&counter);  /* counter++ atomically */
atomic_dec(&counter);  /* counter-- atomically */
```
[Basic]

Q: What does `atomic_dec_and_test(v)` return?
A: Atomically decrements `v` and returns true (non-zero) if the result is zero, false otherwise. Commonly used for reference counting:
```c
if (atomic_dec_and_test(&refcount))
    free_object(obj);  /* Last reference gone */
```
[Intermediate]

Q: What does `atomic_inc_and_test(v)` return?
A: Atomically increments `v` and returns true if the result is zero. Less commonly used than dec_and_test (mainly for testing if counter wrapped around to zero).
[Intermediate]

Q: What does `atomic_add_return(i, v)` do?
A: Atomically adds `i` to `v` and returns the new value:
```c
int new_val = atomic_add_return(5, &counter);
/* counter += 5, new_val = new counter value */
```
[Intermediate]

Q: What does `atomic_sub_and_test(i, v)` return?
A: Atomically subtracts `i` from `v` and returns true if the result is zero, false otherwise.
[Intermediate]

Q: What is `atomic_cmpxchg(v, old, new)` and when is it used?
A: Compare-and-exchange: atomically compares `v` with `old`, and if equal, sets `v` to `new`. Returns the original value of `v`. Used for lock-free algorithms:
```c
old_val = atomic_cmpxchg(&v, expected, desired);
if (old_val == expected)
    /* exchange succeeded */
```
[Advanced]

Q: What is `set_bit(nr, addr)` used for?
A: Atomically sets bit number `nr` in the memory location pointed to by `addr`:
```c
unsigned long flags = 0;
set_bit(3, &flags);  /* flags |= (1 << 3) atomically */
```
[Intermediate]

Q: What is `clear_bit(nr, addr)` used for?
A: Atomically clears bit number `nr` in the memory location pointed to by `addr`:
```c
clear_bit(3, &flags);  /* flags &= ~(1 << 3) atomically */
```
[Intermediate]

Q: What does `test_and_set_bit(nr, addr)` do?
A: Atomically sets bit `nr` and returns the previous value of that bit. Used for implementing spinlock-like constructs:
```c
if (test_and_set_bit(LOCK_BIT, &flags))
    /* bit was already set - lock held by someone */
else
    /* we acquired the lock */
```
[Intermediate]

Q: What does `test_and_clear_bit(nr, addr)` do?
A: Atomically clears bit `nr` and returns the previous value of that bit.
[Intermediate]

Q: What is `atomic64_t` and when should you use it?
A: `atomic64_t` is a 64-bit atomic type for when you need atomic operations on values larger than 32 bits:
```c
atomic64_t big_counter;
atomic64_set(&big_counter, 0);
atomic64_inc(&big_counter);
```
Use when 32-bit range is insufficient.
[Intermediate]

Q: [Cloze] Atomic operations in Linux kernel provide atomicity but NOT necessarily ___ ordering guarantees without explicit barriers.
A: memory
[Advanced]

Q: When should you use atomic operations vs. locks?
A: Use atomic operations for:
- Simple counters (reference counts, statistics)
- Single-variable updates
- Lock-free algorithms

Use locks when:
- Multiple variables must be updated together
- Complex operations on shared data
- Operations that may sleep
[Intermediate]

Q: What is the common misconception about atomic operations and memory ordering?
A: Many assume atomic operations provide full memory ordering. In Linux, basic atomic ops (atomic_inc, etc.) do NOT imply memory barriers. Use atomic_add_return() variants or explicit barriers when ordering matters.
[Advanced]

Q: How do you initialize an atomic_t variable statically vs dynamically?
A: Static: `atomic_t v = ATOMIC_INIT(0);`
Dynamic: `atomic_set(&v, 0);`

```c
/* Static initialization */
static atomic_t counter = ATOMIC_INIT(0);

/* Dynamic initialization */
atomic_t runtime_counter;
atomic_set(&runtime_counter, initial_value);
```
[Basic]

---

# Section 3: Spinlocks

Q: What is a spinlock?
A: A spinlock is a low-level synchronization primitive where a thread trying to acquire an already-held lock "spins" (busy-waits) in a loop until the lock becomes available. It doesn't sleep, making it suitable for short critical sections.
[Basic]

Q: What is the type used for spinlocks in Linux kernel?
A: `spinlock_t`, defined in `<linux/spinlock.h>`:
```c
spinlock_t my_lock;
```
[Basic]

Q: How do you statically initialize a spinlock?
A: Use the `DEFINE_SPINLOCK` macro:
```c
static DEFINE_SPINLOCK(my_lock);
```
Or with `__SPIN_LOCK_UNLOCKED`:
```c
static spinlock_t my_lock = __SPIN_LOCK_UNLOCKED(my_lock);
```
[Basic]

Q: How do you dynamically initialize a spinlock?
A: Use `spin_lock_init()`:
```c
spinlock_t my_lock;
spin_lock_init(&my_lock);
```
[Basic]

Q: What does `spin_lock(lock)` do?
A: Acquires the spinlock. If the lock is already held, the CPU spins (busy-waits) until it becomes available. Disables preemption on the local CPU.
```c
spin_lock(&my_lock);
/* critical section */
spin_unlock(&my_lock);
```
[Basic]

Q: What does `spin_unlock(lock)` do?
A: Releases the spinlock and re-enables preemption on the local CPU:
```c
spin_unlock(&my_lock);
```
[Basic]

Q: What does `spin_trylock(lock)` return?
A: Attempts to acquire the lock without spinning. Returns non-zero (true) if lock acquired, 0 (false) if lock was already held:
```c
if (spin_trylock(&my_lock)) {
    /* got the lock */
    spin_unlock(&my_lock);
} else {
    /* lock held by someone else */
}
```
[Intermediate]

Q: Why does holding a spinlock disable preemption?
A: If preemption were allowed while holding a spinlock, a higher-priority task could preempt the lock holder and try to acquire the same lock, causing deadlock (spinning forever on a single CPU waiting for itself).
[Intermediate]

Q: What is the golden rule about sleeping while holding a spinlock?
A: NEVER sleep while holding a spinlock. Sleeping functions (kmalloc with GFP_KERNEL, mutex_lock, copy_to_user, etc.) must not be called. This would cause deadlock since the sleeping task holds the lock but can't run to release it.
[Basic]

Q: When should you use `spin_lock_irqsave()` instead of `spin_lock()`?
A: When the lock may be acquired from both process context and interrupt context. `spin_lock_irqsave()` disables local interrupts and saves the previous IRQ state to prevent deadlock if an interrupt tries to acquire the same lock.
[Intermediate]

Q: What does `spin_lock_irqsave(lock, flags)` do?
A: Saves current interrupt state in `flags`, disables local interrupts, then acquires the spinlock:
```c
unsigned long flags;
spin_lock_irqsave(&my_lock, flags);
/* critical section - interrupts disabled */
spin_unlock_irqrestore(&my_lock, flags);
```
[Intermediate]

Q: What does `spin_unlock_irqrestore(lock, flags)` do?
A: Releases the spinlock and restores interrupts to the state saved in `flags`. This properly handles nested interrupt disable/enable scenarios.
[Intermediate]

Q: When should you use `spin_lock_irq()` vs `spin_lock_irqsave()`?
A: Use `spin_lock_irq()` only when you're certain interrupts are currently enabled. Use `spin_lock_irqsave()` when you're unsure or in code that may be called with interrupts already disabled (safer choice).
[Intermediate]

Q: What does `spin_lock_bh(lock)` do?
A: Acquires the spinlock and disables bottom halves (softirqs) on the local CPU. Use when lock is shared between process context and softirq/tasklet context but NOT hard IRQ context.
[Intermediate]

Q: [ASCII Diagram] Show the spinlock acquisition flow:
A:
```
spin_lock(&lock)
      |
      v
+-------------+
| Lock free?  |----No----> [Spin/Busy-wait]
+-------------+               |
      | Yes                   |
      v                       |
+------------------+          |
| Disable preempt  |<---------+
| Acquire lock     |
+------------------+
      |
      v
[Enter Critical Section]
```
[Basic]

Q: What is the typical spinlock usage pattern?
A:
```c
spinlock_t lock;
unsigned long flags;

spin_lock_init(&lock);

/* Later, in critical section: */
spin_lock_irqsave(&lock, flags);
/* access shared data */
spin_unlock_irqrestore(&lock, flags);
```
[Basic]

Q: What happens if you call `spin_lock()` on a lock you already hold?
A: Deadlock. The CPU will spin forever waiting for itself to release the lock. Linux spinlocks are NOT recursive. Use separate locks or redesign your code.
[Intermediate]

Q: How does `CONFIG_DEBUG_SPINLOCK` help debugging?
A: Enables runtime checks for:
- Double unlock detection
- Unlocking lock not held
- Using uninitialized spinlocks
- Recursive locking attempts
Adds overhead but catches common bugs.
[Intermediate]

Q: [Cloze] Spinlocks should only protect ___ critical sections because other CPUs waste cycles spinning.
A: short
[Basic]

Q: Why are spinlocks more efficient than sleeping locks for very short critical sections?
A: Context switching (sleeping/waking) has significant overhead. For operations taking only a few CPU cycles, the cost of sleeping exceeds the cost of briefly spinning. Spinlocks avoid scheduler overhead.
[Intermediate]

Q: What is the SMP deadlock scenario with spinlocks and interrupts?
A:
```
CPU 0                    Interrupt on CPU 0
─────                    ──────────────────
spin_lock(&lock)         
/* holds lock */         
                         interrupt fires
                         spin_lock(&lock)
                         /* spins forever! */
```
Solution: Use `spin_lock_irqsave()` to disable interrupts.
[Intermediate]

Q: Can spinlocks be used in interrupt handlers?
A: Yes, but the interrupt handler must use the same locking variant that disables interrupts. If process context uses `spin_lock_irqsave()`, the interrupt can safely use `spin_lock()` since interrupts are already disabled in IRQ context.
[Intermediate]

Q: What is "lock ordering" and why is it critical for spinlocks?
A: Lock ordering means always acquiring multiple locks in a consistent, predefined order. Violating this causes deadlock:
```
CPU 0: lock(A), lock(B)
CPU 1: lock(B), lock(A)  /* Deadlock! */
```
Always document and enforce lock ordering.
[Intermediate]

Q: What does `spin_is_locked(lock)` return?
A: Returns non-zero if the lock is currently held, 0 otherwise. Useful for debugging assertions, but NOT for synchronization (result may be stale immediately after checking).
[Intermediate]

Q: What is the relationship between spinlocks and SMP?
A: On UP (uniprocessor) systems, spinlocks compile to just preemption disable/enable (no actual spinning needed). On SMP, they perform actual atomic test-and-set operations. This makes spinlock code portable across UP and SMP.
[Advanced]

Q: [Code Interpretation] What's wrong with this code?
```c
spin_lock(&lock);
ptr = kmalloc(size, GFP_KERNEL);
spin_unlock(&lock);
```
A: `kmalloc()` with `GFP_KERNEL` may sleep to reclaim memory. Sleeping while holding a spinlock causes deadlock. Fix: Use `GFP_ATOMIC`:
```c
spin_lock(&lock);
ptr = kmalloc(size, GFP_ATOMIC);
spin_unlock(&lock);
```
[Intermediate]

---

# Section 4: Reader-Writer Spinlocks

Q: What is a reader-writer spinlock (rwlock)?
A: A spinlock variant that allows multiple concurrent readers OR one exclusive writer. Optimizes for read-heavy workloads where reads far outnumber writes.
[Basic]

Q: What is the type used for reader-writer spinlocks?
A: `rwlock_t`, defined in `<linux/rwlock.h>`:
```c
rwlock_t my_rwlock;
```
[Basic]

Q: How do you statically initialize an rwlock?
A: Use `DEFINE_RWLOCK`:
```c
static DEFINE_RWLOCK(my_rwlock);
```
[Basic]

Q: How do you dynamically initialize an rwlock?
A: Use `rwlock_init()`:
```c
rwlock_t my_rwlock;
rwlock_init(&my_rwlock);
```
[Basic]

Q: What does `read_lock(lock)` do?
A: Acquires a read lock. Multiple readers can hold the lock simultaneously. Blocks if a writer holds the lock. Disables preemption.
```c
read_lock(&my_rwlock);
/* read shared data */
read_unlock(&my_rwlock);
```
[Basic]

Q: What does `write_lock(lock)` do?
A: Acquires an exclusive write lock. Blocks until all readers and any writer release the lock. Only one writer can hold the lock.
```c
write_lock(&my_rwlock);
/* modify shared data */
write_unlock(&my_rwlock);
```
[Basic]

Q: What are the interrupt-safe variants of rwlock operations?
A:
- `read_lock_irqsave()` / `read_unlock_irqrestore()`
- `read_lock_irq()` / `read_unlock_irq()`
- `write_lock_irqsave()` / `write_unlock_irqrestore()`
- `write_lock_irq()` / `write_unlock_irq()`
- `read_lock_bh()` / `read_unlock_bh()`
- `write_lock_bh()` / `write_unlock_bh()`
[Intermediate]

Q: What is writer starvation in rwlocks?
A: When readers continuously acquire the lock, a waiting writer may never get it. New readers can keep acquiring the lock even while a writer waits, potentially starving the writer indefinitely.
[Intermediate]

Q: Does Linux kernel rwlock implementation favor readers or writers?
A: Linux rwlocks favor readers - new readers can acquire the lock even if a writer is waiting. This can cause writer starvation under heavy read load. Consider seqlocks or RCU for write-starved scenarios.
[Intermediate]

Q: [ASCII Diagram] Show rwlock concurrency:
A:
```
Reader-Writer Lock States:
                                  
[Unlocked] <---> [Read-locked by N readers]
    ^                    |
    |                    | (all readers release)
    v                    v
[Write-locked by 1 writer]

Concurrent reads:  YES (multiple readers)
Concurrent writes: NO  (exclusive)
Read + Write:      NO  (mutually exclusive)
```
[Basic]

Q: When should you use rwlock vs regular spinlock?
A: Use rwlock when:
- Reads significantly outnumber writes
- Read operations are longer than lock acquisition overhead
- Write starvation is acceptable

Use regular spinlock when:
- Read/write ratio is balanced
- Critical sections are very short
- Simpler code is preferred
[Intermediate]

Q: What is the performance tradeoff of rwlocks?
A: rwlocks have higher overhead than simple spinlocks due to tracking reader count. This overhead may exceed benefits if:
- Critical sections are very short
- Reads don't significantly outnumber writes
- Only a few CPUs contend for the lock
[Intermediate]

Q: Can a reader upgrade to a writer while holding the read lock?
A: No. Linux rwlocks don't support upgrade. Attempting to acquire write lock while holding read lock causes deadlock. You must release read lock first, then acquire write lock (which creates a window for races).
[Intermediate]

Q: [Code Interpretation] What's wrong with this code?
```c
read_lock(&rwlock);
if (need_modify) {
    write_lock(&rwlock);  /* upgrade */
    modify_data();
    write_unlock(&rwlock);
}
read_unlock(&rwlock);
```
A: Deadlock! Cannot acquire write lock while holding read lock. The write_lock will wait forever for the read lock to be released. Fix: release read lock, acquire write lock, handle potential race:
```c
read_lock(&rwlock);
if (need_modify) {
    read_unlock(&rwlock);
    write_lock(&rwlock);
    /* re-check condition! */
    modify_data();
    write_unlock(&rwlock);
} else {
    read_unlock(&rwlock);
}
```
[Intermediate]

Q: What are the try-lock variants for rwlock?
A: `read_trylock(lock)` and `write_trylock(lock)` - attempt to acquire without blocking. Return non-zero on success, 0 if lock not available.
[Intermediate]

Q: Why are rwlocks considered deprecated for new code in modern Linux?
A: Due to writer starvation issues and overhead, alternatives are preferred:
- RCU for read-mostly data structures
- seqlocks for simple data with rare writes
- Regular spinlocks for short critical sections
rwlocks are still supported but less commonly used in new code.
[Advanced]

---

# Section 5: Semaphores

Q: What is a semaphore in kernel programming?
A: A semaphore is a sleeping lock that maintains a count. When count > 0, acquisition succeeds and decrements count. When count = 0, the task sleeps until another task releases (increments) the semaphore.
[Basic]

Q: What is the structure for semaphores in Linux kernel?
A: `struct semaphore`, defined in `<linux/semaphore.h>`:
```c
struct semaphore {
    raw_spinlock_t      lock;
    unsigned int        count;
    struct list_head    wait_list;
};
```
[Basic]

Q: What is a counting semaphore vs binary semaphore?
A: Counting semaphore: count can be any non-negative value, allowing N concurrent accesses (e.g., resource pool).
Binary semaphore: count is 0 or 1, providing mutual exclusion (similar to mutex).
[Basic]

Q: How do you statically initialize a semaphore?
A: Use `DEFINE_SEMAPHORE` for binary (count=1) or `__SEMAPHORE_INITIALIZER` for any count:
```c
static DEFINE_SEMAPHORE(my_sem);  /* count = 1 */

static struct semaphore pool_sem = 
    __SEMAPHORE_INITIALIZER(pool_sem, 5);  /* count = 5 */
```
[Basic]

Q: How do you dynamically initialize a semaphore?
A: Use `sema_init()`:
```c
struct semaphore sem;
sema_init(&sem, count);  /* initialize with given count */
```
[Basic]

Q: What does `down(sem)` do?
A: Decrements semaphore count if > 0 (acquires). If count is 0, puts task to sleep (TASK_UNINTERRUPTIBLE) until semaphore is released. Cannot be interrupted by signals.
```c
down(&sem);  /* may sleep */
/* critical section */
up(&sem);
```
[Basic]

Q: What does `up(sem)` do?
A: Increments the semaphore count. If any tasks are sleeping on the semaphore, wakes one of them.
```c
up(&sem);  /* release semaphore */
```
[Basic]

Q: What does `down_interruptible(sem)` return?
A: Like `down()`, but sleeps in TASK_INTERRUPTIBLE state. Returns 0 on success, -EINTR if interrupted by signal. Must check return value!
```c
if (down_interruptible(&sem))
    return -ERESTARTSYS;  /* signal received */
/* got the semaphore */
```
[Intermediate]

Q: What does `down_trylock(sem)` return?
A: Attempts to acquire without sleeping. Returns 0 on success, non-zero if semaphore not available.
```c
if (down_trylock(&sem) == 0) {
    /* got it */
    up(&sem);
}
```
[Intermediate]

Q: What does `down_timeout(sem, timeout)` do?
A: Attempts to acquire semaphore with timeout (in jiffies). Returns 0 on success, -ETIME on timeout.
```c
if (down_timeout(&sem, HZ))  /* 1 second timeout */
    return -ETIMEDOUT;
```
[Intermediate]

Q: What is the key difference between semaphores and spinlocks?
A: Semaphores sleep when waiting (yield CPU), spinlocks busy-wait (consume CPU). Semaphores are for longer critical sections or when sleeping is acceptable. Spinlocks are for short critical sections where sleeping is forbidden.
[Basic]

Q: When can you hold a semaphore but NOT a spinlock?
A: When calling functions that might sleep:
- Memory allocation with GFP_KERNEL
- copy_to_user() / copy_from_user()
- File I/O operations
- Waiting for hardware

Semaphores allow sleeping; spinlocks do not.
[Intermediate]

Q: [ASCII Diagram] Show semaphore state transitions:
A:
```
                 down()
    +------+   count > 0   +----------+
    | Free |-------------->| Acquired |
    +------+               +----------+
        ^                       |
        |         up()          |
        +-----------------------+
        
    If count == 0 on down():
    
    +------+  down()   +-----------+  up()   +----------+
    | Free |---------->| Sleeping  |-------->| Acquired |
    +------+           | (waiting) |         +----------+
                       +-----------+
```
[Basic]

Q: Why might you use `down_interruptible()` instead of `down()`?
A: `down_interruptible()` allows the process to respond to signals (like Ctrl+C). Use it in code paths where user-space might want to abort. Using plain `down()` makes the process unresponsive to signals while waiting.
[Intermediate]

Q: Can semaphores be released by a different task than the one that acquired them?
A: Yes, unlike mutexes. Semaphores have no concept of "owner." Any task can call `up()`. This enables producer-consumer patterns but also makes debugging harder (no ownership tracking).
[Intermediate]

Q: What is the relationship between semaphores and wait queues?
A: Semaphores use wait queues internally. When a task sleeps in `down()`, it's added to the semaphore's `wait_list`. When `up()` is called, it removes and wakes a task from this list.
[Intermediate]

Q: Why are mutexes preferred over binary semaphores in modern Linux?
A: Mutexes provide:
- Ownership tracking (owner must release)
- Priority inheritance support
- Better debugging (lockdep integration)
- Optimized fast path

Binary semaphores lack these features and are more error-prone.
[Intermediate]

Q: [Reverse] A sleeping lock that maintains a count, allowing N concurrent holders.
A: Counting Semaphore
[Basic]

Q: What happens if `up()` is called more times than `down()` was called?
A: The semaphore count increases beyond its initial value. This is a bug for binary semaphores (should never exceed 1) but valid for some counting semaphore uses. No kernel warning is generated.
[Advanced]

Q: What is the typical use case for counting semaphores?
A: Managing a pool of resources:
```c
#define MAX_CONNECTIONS 10
static DEFINE_SEMAPHORE(conn_sem);
sema_init(&conn_sem, MAX_CONNECTIONS);

/* Acquire a connection slot */
down(&conn_sem);
use_connection();
up(&conn_sem);
```
[Intermediate]

---

# Section 6: Mutexes

Q: What is a mutex in Linux kernel?
A: A mutex (mutual exclusion) is a sleeping lock that provides exclusive access to a resource. Only one task can hold the mutex at a time, and only the owner can release it. Defined in `<linux/mutex.h>`.
[Basic]

Q: What is the structure for mutexes?
A: `struct mutex`:
```c
struct mutex {
    atomic_t            count;
    spinlock_t          wait_lock;
    struct list_head    wait_list;
    struct task_struct  *owner;  /* for debugging */
};
```
[Basic]

Q: How do you statically define a mutex?
A: Use `DEFINE_MUTEX`:
```c
static DEFINE_MUTEX(my_mutex);
```
[Basic]

Q: How do you dynamically initialize a mutex?
A: Use `mutex_init()`:
```c
struct mutex my_mutex;
mutex_init(&my_mutex);
```
[Basic]

Q: What does `mutex_lock(lock)` do?
A: Acquires the mutex. If held by another task, sleeps (TASK_UNINTERRUPTIBLE) until available. Marks the current task as owner.
```c
mutex_lock(&my_mutex);
/* critical section */
mutex_unlock(&my_mutex);
```
[Basic]

Q: What does `mutex_unlock(lock)` do?
A: Releases the mutex and wakes any waiting task. Must be called by the same task that called `mutex_lock()`.
[Basic]

Q: What does `mutex_trylock(lock)` return?
A: Returns 1 (true) if mutex acquired, 0 (false) if already held. Non-blocking.
```c
if (mutex_trylock(&my_mutex)) {
    /* got the lock */
    mutex_unlock(&my_mutex);
}
```
[Basic]

Q: What does `mutex_lock_interruptible(lock)` return?
A: Returns 0 on success, -EINTR if interrupted by signal. Task sleeps in TASK_INTERRUPTIBLE state.
```c
if (mutex_lock_interruptible(&my_mutex))
    return -ERESTARTSYS;
/* got the lock */
```
[Intermediate]

Q: What are the strict rules for using Linux mutexes?
A: 1. Only the owner can unlock the mutex
2. Recursive locking is forbidden (no nested lock by same task)
3. Cannot be used in interrupt/softirq context (sleeps)
4. Memory containing mutex must not be freed while locked
5. Cannot be reinitialized while held
[Intermediate]

Q: Why were mutexes introduced when semaphores already existed?
A: Mutexes provide:
- Stricter semantics (owner-release rule)
- Better debugging via ownership tracking
- Adaptive spinning optimization
- Priority inheritance support
- Lockdep integration for deadlock detection
[Intermediate]

Q: What is adaptive spinning (aka "spin-on-owner") in mutexes?
A: When a mutex is held by a running task on another CPU, the waiter briefly spins instead of immediately sleeping. If the owner is running, it will likely release soon. This avoids the expensive sleep/wake cycle for short critical sections.
[Advanced]

Q: When does adaptive spinning stop and the task sleep?
A: Adaptive spinning stops when:
- The mutex owner goes to sleep
- The spinner has spun for too long
- Need_resched() is set (scheduler wants to run)
- The owner releases the mutex (spinner acquires)
[Advanced]

Q: What does `mutex_is_locked(lock)` return?
A: Returns non-zero if mutex is currently held, 0 otherwise. Useful for debugging/assertions but NOT for synchronization decisions.
```c
WARN_ON(mutex_is_locked(&lock));  /* debugging */
```
[Intermediate]

Q: What is `mutex_lock_killable(lock)`?
A: Like `mutex_lock_interruptible()` but only woken by fatal signals (SIGKILL). Less responsive to signals but avoids spurious wakeups from non-fatal signals.
[Intermediate]

Q: [Cloze] Mutexes can sleep and must only be used in ___ context, never in interrupt or softirq context.
A: process
[Basic]

Q: What is the common pattern for mutex-protected data access?
A:
```c
struct my_data {
    struct mutex lock;
    int value;
    /* other fields */
};

void update_data(struct my_data *data, int new_val)
{
    mutex_lock(&data->lock);
    data->value = new_val;
    mutex_unlock(&data->lock);
}
```
[Basic]

Q: [Code Interpretation] What's wrong with this code?
```c
mutex_lock(&mutex);
mutex_lock(&mutex);  /* nested */
do_work();
mutex_unlock(&mutex);
mutex_unlock(&mutex);
```
A: Deadlock! Linux mutexes are NOT recursive. The second `mutex_lock()` will sleep forever waiting for itself. If you need recursive locking, redesign your code or use separate locks.
[Intermediate]

Q: How does CONFIG_DEBUG_MUTEXES help?
A: Enables runtime checks for:
- Unlocking mutex not held
- Unlocking by non-owner
- Recursive locking detection
- Using uninitialized mutex
- Memory corruption detection
[Intermediate]

Q: What is the performance comparison between mutex and semaphore?
A: Mutexes are generally faster due to:
- Optimized fast path (no contention case)
- Adaptive spinning avoiding sleep
- Simpler count semantics (binary only)
Semaphores have more overhead from counting and no adaptive spinning.
[Intermediate]

Q: When should you use mutex vs semaphore?
A: Use mutex for:
- Mutual exclusion (most common case)
- When only owner should release

Use semaphore for:
- Counting multiple resources
- When different task needs to release
- Legacy code compatibility
[Basic]

Q: [Reverse] A sleeping lock with owner tracking, adaptive spinning, and strict release-by-owner semantics.
A: Mutex
[Basic]

---

# Section 7: Reader-Writer Semaphores

Q: What is a reader-writer semaphore (rw_semaphore)?
A: A sleeping lock allowing multiple concurrent readers OR one exclusive writer. Unlike rwlocks, tasks sleep when blocked instead of spinning. Defined in `<linux/rwsem.h>`.
[Basic]

Q: What is the structure for reader-writer semaphores?
A: `struct rw_semaphore`:
```c
struct rw_semaphore {
    long                count;
    spinlock_t          wait_lock;
    struct list_head    wait_list;
};
```
[Basic]

Q: How do you statically initialize an rw_semaphore?
A: Use `DECLARE_RWSEM`:
```c
static DECLARE_RWSEM(my_rwsem);
```
[Basic]

Q: How do you dynamically initialize an rw_semaphore?
A: Use `init_rwsem()`:
```c
struct rw_semaphore my_rwsem;
init_rwsem(&my_rwsem);
```
[Basic]

Q: What does `down_read(sem)` do?
A: Acquires a read lock (shared). Multiple readers can hold simultaneously. Sleeps if a writer holds the lock or is waiting.
```c
down_read(&my_rwsem);
/* read shared data */
up_read(&my_rwsem);
```
[Basic]

Q: What does `up_read(sem)` do?
A: Releases the read lock. If this was the last reader and a writer is waiting, the writer is woken.
[Basic]

Q: What does `down_write(sem)` do?
A: Acquires an exclusive write lock. Sleeps until all readers and any writer release.
```c
down_write(&my_rwsem);
/* modify shared data */
up_write(&my_rwsem);
```
[Basic]

Q: What does `up_write(sem)` do?
A: Releases the write lock. Wakes waiting readers and/or writers.
[Basic]

Q: What are the trylock variants for rw_semaphore?
A: `down_read_trylock(sem)` - returns 1 on success, 0 if blocked
`down_write_trylock(sem)` - returns 1 on success, 0 if blocked
[Intermediate]

Q: What does `downgrade_write(sem)` do?
A: Atomically converts a write lock to a read lock without releasing. Useful when you've finished modifying and want to continue reading while allowing other readers.
```c
down_write(&sem);
modify_data();
downgrade_write(&sem);
/* now holds read lock - others can read */
continue_reading();
up_read(&sem);
```
[Intermediate]

Q: Why is there no `upgrade_read()` function?
A: Upgrading (read to write) is dangerous and can deadlock. If two readers try to upgrade simultaneously, both would wait for the other to release - deadlock. Always release read, then acquire write.
[Intermediate]

Q: How does rw_semaphore differ from rwlock_t?
A: | Feature | rw_semaphore | rwlock_t |
|---------|--------------|----------|
| Blocking | Sleeps | Spins |
| Context | Process only | Any (no sleep) |
| Duration | Long critical sections | Short only |
| downgrade | Supported | Not supported |
[Intermediate]

Q: What is a common use case for rw_semaphore in the kernel?
A: Memory management's `mmap_sem` (now `mmap_lock`):
- Read-locked for page faults, mmap queries
- Write-locked for mmap/munmap changes

Many concurrent readers (page faults) with rare writers.
[Intermediate]

Q: [Code Example] Show typical rw_semaphore usage pattern:
A:
```c
struct shared_state {
    struct rw_semaphore sem;
    struct list_head items;
    int count;
};

void read_state(struct shared_state *s)
{
    down_read(&s->sem);
    /* can read s->items, s->count */
    up_read(&s->sem);
}

void modify_state(struct shared_state *s)
{
    down_write(&s->sem);
    /* can modify s->items, s->count */
    up_write(&s->sem);
}
```
[Basic]

Q: Does rw_semaphore have writer starvation issues?
A: Linux rw_semaphore implementation gives preference to writers. Once a writer is waiting, new readers block until the writer completes. This prevents writer starvation but may reduce reader concurrency.
[Advanced]

Q: When should you use rw_semaphore vs rwlock_t?
A: Use rw_semaphore when:
- Critical section may be long
- Code may sleep while holding lock
- downgrade_write() is needed

Use rwlock_t when:
- Very short critical sections
- Cannot sleep
- Performance critical
[Intermediate]

---

# Section 8: Completion Variables

Q: What is a completion variable?
A: A simple synchronization mechanism for one task to signal another that an event has occurred. Used for "wait until done" patterns. Defined in `<linux/completion.h>`.
[Basic]

Q: What is the structure for completion variables?
A: `struct completion`:
```c
struct completion {
    unsigned int done;
    wait_queue_head_t wait;
};
```
[Basic]

Q: How do you statically declare a completion?
A: Use `DECLARE_COMPLETION`:
```c
static DECLARE_COMPLETION(my_completion);
```
[Basic]

Q: How do you dynamically initialize a completion?
A: Use `init_completion()`:
```c
struct completion done;
init_completion(&done);
```
[Basic]

Q: What does `wait_for_completion(c)` do?
A: Sleeps until `complete()` is called on the completion. Task sleeps in TASK_UNINTERRUPTIBLE state.
```c
wait_for_completion(&done);
/* now signaled - continue */
```
[Basic]

Q: What does `complete(c)` do?
A: Signals one waiting task (if any) to wake up. Increments the `done` counter.
```c
/* when work is done */
complete(&done);
```
[Basic]

Q: What does `complete_all(c)` do?
A: Wakes ALL tasks waiting on the completion (not just one). Useful for broadcast scenarios.
```c
/* signal all waiters */
complete_all(&done);
```
[Intermediate]

Q: What is `wait_for_completion_interruptible(c)` and what does it return?
A: Like `wait_for_completion()` but interruptible by signals. Returns 0 on completion, -ERESTARTSYS if interrupted.
```c
if (wait_for_completion_interruptible(&done))
    return -ERESTARTSYS;
```
[Intermediate]

Q: What does `wait_for_completion_timeout(c, timeout)` return?
A: Waits with timeout (in jiffies). Returns:
- 0 if timeout elapsed
- Remaining jiffies if completed before timeout
```c
if (!wait_for_completion_timeout(&done, 5*HZ))
    return -ETIMEDOUT;  /* 5 second timeout */
```
[Intermediate]

Q: How do you reinitialize a completion for reuse?
A: Use `reinit_completion()` (preferred) or `init_completion()`:
```c
reinit_completion(&done);  /* reset done to 0 */
/* can now wait again */
```
INIT_COMPLETION() macro is deprecated.
[Intermediate]

Q: When should you use completion vs semaphore?
A: Use completion for:
- One-shot "signal when done" patterns
- Thread initialization synchronization
- Waiting for hardware/interrupt events

Use semaphore for:
- Counting resources
- General mutual exclusion (use mutex instead)
[Intermediate]

Q: [Code Example] Show typical completion usage for thread sync:
A:
```c
struct completion thread_started;

int my_thread(void *data)
{
    /* thread initialization */
    complete(&thread_started);  /* signal ready */
    
    /* main work loop */
    return 0;
}

void start_thread(void)
{
    init_completion(&thread_started);
    kthread_run(my_thread, NULL, "worker");
    wait_for_completion(&thread_started);
    /* thread is now initialized */
}
```
[Intermediate]

Q: [Reverse] Synchronization primitive for signaling that an event or operation has completed.
A: Completion Variable (`struct completion`)
[Basic]

---

# Section 9: Sequential Locks (Seqlocks)

Q: What is a seqlock (sequential lock)?
A: A lock mechanism using a sequence counter to detect concurrent modifications. Readers optimistically read without locking, then verify no writer was active. Writers use traditional locking. Defined in `<linux/seqlock.h>`.
[Basic]

Q: What is the type for sequential locks?
A: `seqlock_t`:
```c
seqlock_t my_seqlock;
```
[Basic]

Q: How do you statically initialize a seqlock?
A: Use `DEFINE_SEQLOCK`:
```c
static DEFINE_SEQLOCK(my_seqlock);
```
[Basic]

Q: How do you dynamically initialize a seqlock?
A: Use `seqlock_init()`:
```c
seqlock_t my_seqlock;
seqlock_init(&my_seqlock);
```
[Basic]

Q: How does the reader side of a seqlock work?
A: Readers use `read_seqbegin()` to get sequence number, read data, then `read_seqretry()` to check if a write occurred. If retry returns true, the read must be repeated.
```c
unsigned seq;
do {
    seq = read_seqbegin(&lock);
    /* read shared data */
} while (read_seqretry(&lock, seq));
```
[Basic]

Q: What does `read_seqbegin(lock)` return?
A: Returns the current sequence number. If odd, a writer is active (readers should wait in tight loop). Implementation typically spins until even.
[Intermediate]

Q: What does `read_seqretry(lock, seq)` return?
A: Returns non-zero (true) if the sequence changed since `read_seqbegin()` - meaning a writer was active during the read. Reader must retry. Returns 0 if read was consistent.
[Intermediate]

Q: How does the writer side of a seqlock work?
A: Writers use `write_seqlock()` and `write_sequnlock()`. The sequence is incremented at lock (becomes odd) and at unlock (becomes even). Writers are serialized.
```c
write_seqlock(&lock);
/* modify shared data */
write_sequnlock(&lock);
```
[Basic]

Q: [ASCII Diagram] Show seqlock mechanism:
A:
```
Sequence Counter:
  0 (even) = no writer
  odd = writer active

Writer:           Reader:
────────          ────────
write_seqlock()   seq = read_seqbegin()
  seq++ (odd)       [wait if odd]
<modify data>     <read data>
write_sequnlock() if (read_seqretry())
  seq++ (even)        retry!

Readers never block writers.
Writers block other writers.
```
[Intermediate]

Q: What are the key advantages of seqlocks?
A: 1. Readers never block writers (low write latency)
2. No writer starvation (unlike rwlocks)
3. Very low overhead for read-mostly data
4. Good for frequently read, rarely written data
[Intermediate]

Q: What are the limitations/requirements for seqlocks?
A: 1. Readers may need to retry (unbounded in theory)
2. Data must remain valid during read (no freed pointers)
3. Reader code must be reentrant (may execute multiple times)
4. Cannot use for data with pointers that may become invalid
5. Writers must still synchronize with each other
[Intermediate]

Q: What is a common kernel use case for seqlocks?
A: Time keeping: `jiffies_lock` protecting `jiffies_64`, `xtime_lock` protecting system time. Time is read very frequently but updated rarely (on timer interrupt).
```c
do {
    seq = read_seqbegin(&xtime_lock);
    time = xtime;
} while (read_seqretry(&xtime_lock, seq));
```
[Intermediate]

Q: What is `seqcount_t` and how does it differ from `seqlock_t`?
A: `seqcount_t` provides only the sequence counter without internal spinlock. Use when you have external locking for writers or need more control:
```c
seqcount_t seq;
spinlock_t lock;  /* external lock for writers */
```
`seqlock_t` = `seqcount_t` + embedded `spinlock_t`
[Advanced]

Q: When should you use seqlock vs rwlock?
A: Use seqlock when:
- Writers must not be blocked by readers
- Data can be read atomically (simple values)
- Readers can tolerate retries

Use rwlock when:
- Reader consistency is critical (can't retry)
- Data includes pointers that may become invalid
- Reader work is expensive to repeat
[Intermediate]

Q: [Code Interpretation] What's the issue with this seqlock reader?
```c
seq = read_seqbegin(&lock);
ptr = shared_ptr;
data = ptr->value;  /* dereference */
if (read_seqretry(&lock, seq))
    goto retry;
```
A: Dangerous! If writer frees `ptr` during read, the dereference causes use-after-free. Seqlocks don't protect pointer validity. Either:
- Copy data atomically (values only)
- Use RCU for pointer-based structures
- Ensure pointers outlive all potential readers
[Advanced]

Q: How do interrupt-safe seqlock variants work?
A: For writers: `write_seqlock_irqsave()`, `write_sequnlock_irqrestore()`
For readers: `read_seqbegin_irqsave()`, `read_seqretry_irqrestore()`

These disable interrupts to prevent reader/writer from same CPU.
[Intermediate]

---

# Section 10: RCU (Read-Copy-Update)

Q: What is RCU (Read-Copy-Update)?
A: A synchronization mechanism optimized for read-mostly data structures. Readers access data without locks (zero overhead), while writers create new versions and wait for all pre-existing readers to finish before reclaiming old data.
[Basic]

Q: What is the core philosophy behind RCU?
A: "Publish-subscribe" model: writers publish new versions atomically, readers subscribe by reading. Old data is kept valid until all readers that could have seen it are done. Deferred destruction, not deferred access.
[Intermediate]

Q: What is a "grace period" in RCU?
A: The time interval during which all pre-existing RCU readers complete. After a grace period, the updater knows no readers can reference old data. New readers see only new data.
[Basic]

Q: What is `rcu_read_lock()` and what does it do?
A: Marks the beginning of an RCU read-side critical section. On non-preemptible kernels, simply disables preemption. Does NOT acquire any lock - extremely fast.
```c
rcu_read_lock();
/* read RCU-protected data */
rcu_read_unlock();
```
[Basic]

Q: What is `rcu_read_unlock()` and what does it do?
A: Marks the end of an RCU read-side critical section. Re-enables preemption. Also does not acquire/release any lock.
[Basic]

Q: What is `rcu_dereference(ptr)` used for?
A: Safely dereferences an RCU-protected pointer. Contains necessary memory barriers to ensure proper ordering and prevents compiler optimizations that could break RCU semantics.
```c
rcu_read_lock();
p = rcu_dereference(global_ptr);
if (p)
    use(p->data);
rcu_read_unlock();
```
[Intermediate]

Q: What is `rcu_assign_pointer(ptr, value)` used for?
A: Used by updaters to publish a new pointer value with proper memory barriers. Ensures all prior initialization is complete before the pointer becomes visible to readers.
```c
new = kmalloc(...);
new->data = value;  /* initialize first */
rcu_assign_pointer(global_ptr, new);  /* then publish */
```
[Intermediate]

Q: What does `synchronize_rcu()` do?
A: Blocks the caller until a grace period has elapsed - meaning all pre-existing RCU readers have completed. After return, old data can safely be freed.
```c
old = global_ptr;
rcu_assign_pointer(global_ptr, new);
synchronize_rcu();  /* wait for grace period */
kfree(old);         /* safe to free now */
```
[Intermediate]

Q: What does `call_rcu(head, func)` do?
A: Registers a callback function to be called after a grace period. Non-blocking alternative to synchronize_rcu(). Used when you can't afford to block.
```c
call_rcu(&old->rcu_head, my_rcu_callback);
/* continues immediately - callback invoked later */

void my_rcu_callback(struct rcu_head *head)
{
    struct my_struct *p = container_of(head, struct my_struct, rcu_head);
    kfree(p);
}
```
[Intermediate]

Q: [ASCII Diagram] Visualize RCU grace period:
A:
```
Time ────────────────────────────────────────────>

CPU 0: [---Reader A---]
CPU 1:      [---Reader B---]
CPU 2:                [---Reader C---]
       
Writer:  Update│        │synchronize_rcu()│Free old
         data  │        │      blocks     │
               └────────┴─────────────────┘
                    Grace Period
                    
After grace period: no pre-existing readers remain
```
[Intermediate]

Q: What is a "quiescent state" in RCU?
A: A point where a CPU is known not to be in an RCU read-side critical section. Examples: context switch, idle loop, user mode. When all CPUs pass through a quiescent state, a grace period ends.
[Advanced]

Q: Can you sleep inside an RCU read-side critical section?
A: In classic (non-preemptible) RCU: NO - sleeping causes context switch which is a quiescent state, ending the read-side protection.
In preemptible RCU (CONFIG_PREEMPT_RCU): sleeping is allowed but blocks grace period completion.
[Intermediate]

Q: What is SRCU (Sleepable RCU)?
A: An RCU variant that allows sleeping in read-side critical sections. Each srcu_struct tracks its own readers. Uses `srcu_read_lock()`, `srcu_read_unlock()`, `synchronize_srcu()`.
```c
idx = srcu_read_lock(&my_srcu);
/* can sleep here */
srcu_read_unlock(&my_srcu, idx);
```
[Advanced]

Q: What is the typical RCU update pattern for linked lists?
A:
```c
/* Remove node from list */
old = list_entry(...);
list_del_rcu(&old->list);  /* unlink */
synchronize_rcu();          /* wait for readers */
kfree(old);                 /* free */

/* Or non-blocking: */
list_del_rcu(&old->list);
call_rcu(&old->rcu, free_callback);
```
[Intermediate]

Q: What is `list_for_each_entry_rcu()` used for?
A: RCU-safe list traversal macro. Uses `rcu_dereference()` internally to safely follow list pointers.
```c
rcu_read_lock();
list_for_each_entry_rcu(item, &my_list, list) {
    process(item);
}
rcu_read_unlock();
```
[Intermediate]

Q: What common kernel data structures use RCU?
A: - Routing tables (frequently read by packet forwarding)
- Network device lists
- Module list
- PID hash tables
- Dcache (directory entry cache)
- Many read-mostly kernel data structures
[Intermediate]

Q: [Cloze] RCU provides ___ overhead for readers but requires writers to wait for a ___.
A: zero (or near-zero); grace period
[Basic]

Q: What is the key constraint on RCU-protected data updates?
A: Updates must be atomic from reader's perspective. Either:
- Single pointer update (rcu_assign_pointer)
- Copy-modify-publish pattern (allocate new, copy old, modify, publish new)

Cannot modify in-place if readers may see inconsistent state.
[Intermediate]

Q: What is "RCU callback batching"?
A: RCU batches multiple call_rcu() callbacks together, invoking them all after the same grace period. Reduces overhead compared to tracking each callback's grace period individually.
[Advanced]

Q: What is Tree RCU?
A: The hierarchical RCU implementation for large SMP systems (default since 2.6.29). Organizes CPUs in a tree structure to reduce lock contention when tracking quiescent states. Scales to thousands of CPUs.
[Advanced]

Q: When should you use RCU vs rwlock/rw_semaphore?
A: Use RCU when:
- Reads vastly outnumber writes
- Read performance is critical (zero overhead)
- Can use deferred destruction pattern

Use rwlock/rw_semaphore when:
- Need synchronous update visibility
- Cannot use deferred destruction
- Simpler code is preferred
[Intermediate]

Q: What happens if you free RCU-protected data without waiting for grace period?
A: Use-after-free bug. Readers may still be accessing the old data. This is a serious memory corruption bug that may cause crashes or security vulnerabilities.
[Basic]

Q: [Code Interpretation] What's the RCU bug here?
```c
rcu_read_lock();
p = rcu_dereference(ptr);
rcu_read_unlock();
use(p->data);  /* use after unlock */
```
A: Bug! Using pointer after `rcu_read_unlock()`. The data may be freed immediately after unlock. Must complete all accesses within the read-side critical section:
```c
rcu_read_lock();
p = rcu_dereference(ptr);
use(p->data);  /* use inside lock */
rcu_read_unlock();
```
[Intermediate]

Q: What is `kfree_rcu(ptr, field)`?
A: Convenience macro combining call_rcu() and kfree(). Schedules kfree() after grace period without explicit callback:
```c
kfree_rcu(old, rcu_head);  /* frees after grace period */
```
Requires struct to have `struct rcu_head` member.
[Intermediate]

Q: How does RCU compare to reader-writer locks in scalability?
A: RCU scales nearly perfectly for reads (no cache-line bouncing, no locks). Reader-writer locks have limited scalability due to shared state updates. On large SMP systems, RCU can be orders of magnitude faster for read-heavy workloads.
[Advanced]

---

# Section 11: Memory Barriers

Q: Why are memory barriers needed?
A: Both compilers and CPUs can reorder memory operations for optimization. This reordering is invisible on a single CPU but can cause incorrect behavior when multiple CPUs access shared data. Memory barriers enforce ordering.
[Basic]

Q: What is a compiler barrier?
A: An instruction that prevents the compiler from reordering memory operations across it. Does NOT affect CPU reordering. In Linux: `barrier()` macro.
```c
a = 1;
barrier();  /* compiler won't move a=1 below or b=2 above */
b = 2;
```
[Basic]

Q: What is a CPU memory barrier?
A: An instruction that prevents the CPU from reordering memory operations across it. Ensures operations before the barrier complete before operations after. More expensive than compiler barriers.
[Basic]

Q: What does `mb()` (memory barrier) do?
A: Full memory barrier - prevents reordering of any memory operations (both loads and stores) across it. Most expensive but strongest ordering guarantee.
```c
WRITE_ONCE(a, 1);
mb();  /* all prior ops complete before subsequent ops */
WRITE_ONCE(b, 2);
```
[Intermediate]

Q: What does `rmb()` (read memory barrier) do?
A: Prevents reordering of read (load) operations across it. Reads before the barrier complete before reads after. Does not order writes.
```c
x = READ_ONCE(a);
rmb();
y = READ_ONCE(b);  /* guaranteed to read 'b' after 'a' */
```
[Intermediate]

Q: What does `wmb()` (write memory barrier) do?
A: Prevents reordering of write (store) operations across it. Writes before the barrier complete before writes after. Does not order reads.
```c
WRITE_ONCE(data, value);
wmb();
WRITE_ONCE(flag, 1);  /* flag written after data */
```
[Intermediate]

Q: What are the SMP variants: `smp_mb()`, `smp_rmb()`, `smp_wmb()`?
A: Barriers that compile to actual barriers on SMP systems but compile to nothing (or just compiler barrier) on UP systems. Use these when synchronizing between CPUs - no overhead on UP.
```c
smp_wmb();  /* write barrier only on SMP */
```
[Intermediate]

Q: What is the difference between `mb()` and `smp_mb()`?
A: `mb()` always generates a CPU barrier (for device I/O ordering).
`smp_mb()` generates CPU barrier only on SMP; compiles to compiler barrier on UP.
Use `smp_*` for CPU-CPU sync, use `mb()` for CPU-device sync.
[Intermediate]

Q: What are `READ_ONCE()` and `WRITE_ONCE()` used for?
A: Mark accesses that must not be optimized/split by compiler:
- Prevents load/store tearing
- Prevents invented loads/stores
- Prevents load/store fusion
```c
WRITE_ONCE(flag, 1);     /* single atomic store */
x = READ_ONCE(shared);   /* single atomic load */
```
[Intermediate]

Q: [ASCII Diagram] Show why memory barriers are needed:
A:
```
Without barriers, CPU may reorder:

Code:                   CPU may execute as:
─────                   ───────────────────
WRITE(data, 42);        WRITE(flag, 1);  ← reordered!
WRITE(flag, 1);         WRITE(data, 42);

Other CPU sees flag=1 but reads stale data!

Fix with barrier:
WRITE(data, 42);
wmb();                  ← forces order
WRITE(flag, 1);
```
[Intermediate]

Q: What implicit barriers do locking primitives provide?
A: Lock acquire (spin_lock, mutex_lock, etc.): ACQUIRE barrier - operations inside cannot move before lock.
Lock release (spin_unlock, mutex_unlock, etc.): RELEASE barrier - operations inside cannot move after unlock.
No explicit barriers needed around lock-protected code.
[Intermediate]

Q: What is the "paired barrier" rule?
A: Memory barriers typically need matching pairs between communicating CPUs:
- Writer uses wmb() between data write and flag write
- Reader uses rmb() between flag read and data read

Missing either barrier breaks the synchronization.
[Advanced]

Q: [Code Example] Show proper barrier pairing:
A:
```c
/* CPU 0 (writer) */          /* CPU 1 (reader) */
WRITE_ONCE(data, 42);         while (!READ_ONCE(flag))
smp_wmb();                        ;
WRITE_ONCE(flag, 1);          smp_rmb();
                              use(READ_ONCE(data));

/* wmb ensures data written before flag */
/* rmb ensures data read after flag seen as 1 */
```
[Advanced]

Q: What is an ACQUIRE barrier?
A: A one-way barrier that prevents subsequent operations from being reordered before it. Used when "acquiring" a resource - ensures you see updates from previous owner. Lock acquisition provides this.
[Advanced]

Q: What is a RELEASE barrier?
A: A one-way barrier that prevents prior operations from being reordered after it. Used when "releasing" a resource - ensures your updates are visible to next owner. Lock release provides this.
[Advanced]

Q: What is a "control dependency" and does it provide ordering?
A: A control dependency exists when a load value determines whether a store executes (via branch). On some architectures it provides ordering, but in Linux you should NOT rely on it - use proper barriers.
```c
if (READ_ONCE(flag))    /* control dependency */
    WRITE_ONCE(x, 1);   /* may or may not order! */
```
[Advanced]

Q: What is a "data dependency" and how does it provide ordering?
A: When one load's result is used to compute another load's address. Provides implicit ordering on most architectures (but not DEC Alpha). Linux's `rcu_dereference()` handles this.
```c
p = READ_ONCE(global_ptr);
x = p->value;  /* data dependency on p */
```
[Advanced]

Q: What happens if you forget memory barriers in lock-free code?
A: Race conditions that may only manifest on certain architectures or under specific timing. Code may work on x86 (strong ordering) but fail on ARM/PowerPC (weak ordering). Always use proper barriers for portable code.
[Advanced]

Q: Which architecture has the weakest memory ordering model?
A: DEC Alpha is historically the weakest - even data dependencies don't provide ordering without barriers. ARM and PowerPC are also weakly ordered. x86 is relatively strongly ordered but still requires barriers for some cases.
[Advanced]

Q: When should you use memory barriers vs locks?
A: Use locks (spinlock, mutex, etc.):
- When protecting multiple variables
- For mutual exclusion
- When code complexity matters

Use barriers directly:
- Lock-free algorithms
- Single producer/consumer patterns
- Performance-critical paths where locks are too expensive
[Intermediate]

---

# Section 12: Preemption and Interrupt Control

Q: What is kernel preemption?
A: The ability for the scheduler to preempt (interrupt and reschedule) a task while it's executing in kernel mode. Enabled by CONFIG_PREEMPT. Improves latency but requires careful synchronization.
[Basic]

Q: What does `preempt_disable()` do?
A: Increments the preemption counter, preventing the scheduler from preempting the current task on this CPU. Must be paired with `preempt_enable()`.
```c
preempt_disable();
/* safe from preemption */
preempt_enable();
```
[Basic]

Q: What does `preempt_enable()` do?
A: Decrements the preemption counter. If it reaches zero and preemption is pending, may trigger immediate reschedule. Must balance every `preempt_disable()`.
[Basic]

Q: What is the `preempt_count` and what does it track?
A: A per-task counter tracking reasons why the task cannot be preempted:
- Preemption disable depth
- Interrupt context depth (hardirq, softirq)
- Nesting of these contexts

Preemption allowed only when preempt_count == 0.
[Intermediate]

Q: What does `in_atomic()` return?
A: Returns true if in atomic context (preemption disabled, interrupt context, or holding spinlock). Used for debugging - sleeping in atomic context is a bug.
```c
if (in_atomic())
    BUG();  /* about to sleep but shouldn't! */
```
[Intermediate]

Q: What does `local_irq_disable()` do?
A: Disables interrupts on the local CPU only. Other CPUs can still receive interrupts. Must be paired with `local_irq_enable()`.
```c
local_irq_disable();
/* interrupts disabled on this CPU */
local_irq_enable();
```
[Basic]

Q: What does `local_irq_enable()` do?
A: Re-enables interrupts on the local CPU. Unconditionally enables - doesn't check previous state.
[Basic]

Q: Why use `local_irq_save(flags)` instead of `local_irq_disable()`?
A: `local_irq_save()` saves the current interrupt state before disabling. This handles nested disable/enable correctly:
```c
unsigned long flags;
local_irq_save(flags);    /* save state, disable */
/* interrupts disabled */
local_irq_restore(flags); /* restore previous state */
```
If interrupts were already disabled, restore keeps them disabled.
[Intermediate]

Q: When should you use interrupt disabling vs spinlocks?
A: Interrupt disabling alone:
- Protect against interrupts on SAME CPU only
- Lightweight for UP systems

Spinlocks:
- Protect against OTHER CPUs
- Use spin_lock_irqsave() for interrupt+SMP protection
[Intermediate]

Q: What does `local_bh_disable()` do?
A: Disables softirqs (bottom halves) on the local CPU. Softirqs won't run until `local_bh_enable()`. Use when sharing data with softirq handlers.
```c
local_bh_disable();
/* softirqs disabled */
local_bh_enable();
```
[Intermediate]

Q: What is the nesting behavior of interrupt/preemption disabling?
A: All these calls nest correctly:
- Multiple `local_irq_disable()` need same number of `local_irq_enable()`
- Multiple `preempt_disable()` need same number of `preempt_enable()`
- (Actually local_irq doesn't nest - use save/restore for nesting)

The counters track depth; only re-enabled at depth 0.
[Intermediate]

Q: [ASCII Diagram] Show the relationship between context types:
A:
```
Context Hierarchy (what can preempt what):
                                         
┌─────────────────────────────────────────┐
│           Hardirq (interrupt)           │ ← highest priority
│  ┌───────────────────────────────────┐  │
│  │         Softirq / Tasklet         │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │   Process Context (normal)  │  │  │ ← can be preempted
│  │  │                             │  │  │
│  │  └─────────────────────────────┘  │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘

Hardirq can interrupt anything
Softirq can interrupt process context
Process context can be preempted (if enabled)
```
[Intermediate]

Q: What happens if you call a sleeping function with interrupts disabled?
A: Bug! Sleeping requires scheduling, but scheduler cannot run with interrupts disabled (timer interrupts needed for preemption). System may hang. CONFIG_DEBUG_ATOMIC_SLEEP catches this.
[Intermediate]

Q: What is the difference between hardirq and softirq context?
A: Hardirq: Hardware interrupt handler, runs with interrupts disabled on that line, highest priority.
Softirq: Deferred work from hardirq, runs with interrupts enabled, can be preempted by hardirqs but not by normal tasks.
[Intermediate]

Q: What does `in_interrupt()` return?
A: Returns true if executing in any interrupt context (hardirq or softirq). Cannot sleep in this context.
```c
if (in_interrupt())
    alloc_flags = GFP_ATOMIC;
else
    alloc_flags = GFP_KERNEL;
```
[Intermediate]

Q: What does `in_irq()` return?
A: Returns true only if in hardirq context (hardware interrupt handler). More specific than `in_interrupt()`.
[Intermediate]

Q: When would you use `preempt_disable()` directly instead of spinlocks?
A: For per-CPU data access:
```c
preempt_disable();
this_cpu_ptr(&per_cpu_data)->value++;
preempt_enable();
```
No lock needed since only this CPU accesses its per-CPU data. Preempt disable prevents migration to another CPU.
[Intermediate]

---

# Section 13: Per-CPU Variables

Q: What are per-CPU variables?
A: Data that has a separate copy for each CPU in the system. Each CPU accesses only its own copy, eliminating the need for locking. Defined in `<linux/percpu.h>`.
[Basic]

Q: Why do per-CPU variables avoid locking?
A: Each CPU has its own private copy - no sharing means no race conditions between CPUs. Only need to prevent preemption (task migrating to different CPU mid-access).
[Basic]

Q: How do you declare a static per-CPU variable?
A: Use `DEFINE_PER_CPU()`:
```c
static DEFINE_PER_CPU(int, my_counter);
static DEFINE_PER_CPU(struct my_struct, my_data);
```
Creates one instance per CPU.
[Basic]

Q: How do you access the current CPU's per-CPU variable?
A: Use `get_cpu_var()` and `put_cpu_var()`:
```c
get_cpu_var(my_counter)++;  /* access this CPU's copy */
put_cpu_var(my_counter);
```
`get_cpu_var()` disables preemption; `put_cpu_var()` re-enables it.
[Basic]

Q: What does `this_cpu_ptr(ptr)` do?
A: Returns pointer to the current CPU's instance of a per-CPU variable. Does NOT disable preemption - caller must ensure preemption safety.
```c
preempt_disable();
p = this_cpu_ptr(&my_percpu_var);
p->value++;
preempt_enable();
```
[Intermediate]

Q: What does `per_cpu(var, cpu)` do?
A: Accesses a specific CPU's copy of a per-CPU variable. Used when you need to access another CPU's data (e.g., for statistics aggregation).
```c
int total = 0;
for_each_online_cpu(cpu)
    total += per_cpu(counter, cpu);
```
[Intermediate]

Q: Why must preemption be disabled when accessing per-CPU variables?
A: If preemption occurs, the task may resume on a different CPU and access that CPU's copy instead - causing inconsistent state and potential races.
[Basic]

Q: How do you allocate per-CPU data dynamically?
A: Use `alloc_percpu()` and `free_percpu()`:
```c
int __percpu *ptr;
ptr = alloc_percpu(int);
if (!ptr)
    return -ENOMEM;
/* use ptr */
free_percpu(ptr);
```
[Intermediate]

Q: What are the this_cpu_*() operations?
A: Optimized per-CPU operations that combine access and operation atomically:
- `this_cpu_read(var)` - read current CPU's value
- `this_cpu_write(var, val)` - write current CPU's value  
- `this_cpu_inc(var)` - increment
- `this_cpu_add(var, val)` - add

Faster than get_cpu_var/put_cpu_var for simple ops.
[Intermediate]

Q: What is a common use case for per-CPU variables?
A: Statistics counters:
```c
DEFINE_PER_CPU(unsigned long, packet_count);

/* In hot path (per-packet): */
this_cpu_inc(packet_count);

/* When reading stats: */
unsigned long total = 0;
for_each_online_cpu(cpu)
    total += per_cpu(packet_count, cpu);
```
No locking overhead in hot path!
[Intermediate]

Q: Can interrupts cause races with per-CPU variables?
A: Yes! An interrupt handler on the same CPU may access the same per-CPU variable. If both contexts access it, use `local_irq_save()` or per-CPU atomic operations.
[Intermediate]

Q: [Cloze] Per-CPU variables eliminate locking between CPUs but still require ___ to be disabled to prevent task migration.
A: preemption
[Basic]

Q: What's the performance advantage of per-CPU variables?
A: - No lock contention (each CPU has private copy)
- No cache-line bouncing (no shared writes)
- Minimal synchronization overhead
- Scales linearly with CPU count

Ideal for frequently updated data like statistics.
[Intermediate]

---

# Section 14: Debugging and Best Practices

Q: What is lockdep?
A: The Linux kernel lock dependency validator. Tracks lock acquisition order at runtime and detects potential deadlocks BEFORE they occur by finding circular dependencies in lock ordering.
[Basic]

Q: How do you enable lockdep?
A: Enable CONFIG_PROVE_LOCKING (and CONFIG_DEBUG_LOCK_ALLOC) in kernel config. Adds runtime overhead but catches locking bugs early.
[Basic]

Q: What types of bugs does lockdep detect?
A: - Circular lock dependencies (potential deadlocks)
- Lock ordering violations
- Recursive locking attempts
- Incorrect lock context (interrupt safety issues)
- Lock held across sleep
[Intermediate]

Q: What is a "lock class" in lockdep?
A: Lockdep groups locks by their static definition location. All instances of a lock defined at the same code location share a "class". This lets lockdep detect ordering violations across all instances.
[Advanced]

Q: What does CONFIG_DEBUG_ATOMIC_SLEEP do?
A: Detects sleeping in atomic context. Triggers warning when code tries to sleep while:
- Holding spinlock
- In interrupt context
- With preemption disabled

Essential for catching hard-to-find bugs.
[Intermediate]

Q: What is the lock ordering rule to prevent deadlocks?
A: Always acquire multiple locks in a consistent global order. Document the ordering. If you need A and B, and documentation says A < B, always lock A first, then B.
```c
/* Document: lock order is: parent->lock, child->lock */
spin_lock(&parent->lock);
spin_lock(&child->lock);  /* always after parent */
```
[Basic]

Q: What is the ABBA deadlock pattern?
A:
```
Thread 1:        Thread 2:
lock(A)          lock(B)
  lock(B)          lock(A)   ← DEADLOCK!
  unlock(B)        unlock(A)
unlock(A)        unlock(B)
```
Thread 1 holds A, waits for B. Thread 2 holds B, waits for A. Neither can proceed.
[Basic]

Q: [ASCII Diagram] Decision flowchart for choosing synchronization primitive:
A:
```
Need synchronization?
         │
         ▼
Can code sleep? ──No──► Need interrupt safety?
         │                      │
        Yes              Yes    │    No
         │                │     │     │
         ▼                ▼     ▼     ▼
   Use mutex          spin_lock_irqsave  spinlock
   or semaphore
         │
         ▼
Read-mostly data? ──Yes──► Consider RCU
         │
        No
         │
         ▼
   Multiple readers? ──Yes──► rw_semaphore
         │
        No
         │
         ▼
      mutex
```
[Intermediate]

Q: What are the rules for lock granularity?
A: Fine-grained (many small locks):
+ Better parallelism
- More complex, higher overhead, harder to debug

Coarse-grained (few big locks):
+ Simpler, easier to reason about
- May become bottleneck under load

Start coarse, refine if profiling shows contention.
[Intermediate]

Q: What is lock contention and how do you detect it?
A: Lock contention occurs when threads frequently wait for busy locks. Detect with:
- /proc/lock_stat (if CONFIG_LOCK_STAT enabled)
- perf lock analysis
- ftrace lock events

High contention = scalability problem.
[Intermediate]

Q: What are common mistakes that cause deadlocks?
A: 1. Inconsistent lock ordering (ABBA pattern)
2. Recursive locking (same lock twice)
3. Missing interrupt variants (irq handler vs process)
4. Sleeping with spinlock held
5. Lock under lock with opposite order
6. Signal handler locking (user space)
[Intermediate]

Q: What is the principle of minimal lock hold time?
A: Hold locks for the shortest time necessary:
- Do expensive operations (allocation, I/O) outside lock
- Copy data out, release lock, then process
- Reduces contention and deadlock window

```c
spin_lock(&lock);
local_copy = shared_data;  /* quick copy */
spin_unlock(&lock);
process(local_copy);       /* slow work outside lock */
```
[Intermediate]

Q: When should you use spinlock vs mutex decision criteria?
A: Use spinlock when:
- Critical section is very short
- Cannot sleep (interrupt context)
- Lock hold time < context switch time

Use mutex when:
- Critical section may be long
- May need to sleep while holding
- In process context only
[Basic]

Q: What lock annotations help documentation and static analysis?
A: Comments and lockdep annotations:
```c
/* Must be called with foo_lock held */
void foo_helper(void) __must_hold(&foo_lock);

/* Acquires foo_lock */
void foo_enter(void) __acquires(&foo_lock);

/* Releases foo_lock */  
void foo_exit(void) __releases(&foo_lock);
```
[Intermediate]

Q: What is the double-checked locking anti-pattern in kernel?
A: Checking condition without lock, then locking and rechecking:
```c
if (!initialized) {     /* racy check */
    mutex_lock(&lock);
    if (!initialized) { /* safe check */
        do_init();
        initialized = 1;
    }
    mutex_unlock(&lock);
}
```
May work on x86 but is technically racy. Use proper once mechanisms or always lock.
[Advanced]

Q: What is a "sleeping with spinlock" bug and why is it dangerous?
A: Calling a sleeping function (kmalloc GFP_KERNEL, mutex_lock, copy_to_user) while holding a spinlock. The task sleeps but holds the spinlock, blocking all other CPUs that need it. Often hangs the system. Always use GFP_ATOMIC in spinlock context.
[Intermediate]

Q: What general debugging tools help with synchronization bugs?
A: - lockdep: lock ordering validation
- CONFIG_DEBUG_SPINLOCK: spinlock sanity checks
- CONFIG_DEBUG_MUTEXES: mutex sanity checks
- CONFIG_DEBUG_ATOMIC_SLEEP: sleep in atomic detection
- ftrace: lock tracing
- KASAN/KCSAN: memory/concurrency sanitizers
[Intermediate]

