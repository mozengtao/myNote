# Linux Kernel v3.2 Process Management Flashcards

---

## Section 1: Process Fundamentals

Q: What is the fundamental unit of execution in the Linux kernel?
A: The **task** (represented by `task_struct`). Linux treats both processes and threads as tasks - they share the same underlying structure and scheduling mechanisms.
[Basic]

Q: How does Linux differentiate between a "process" and a "thread"?
A: In Linux, there is no fundamental distinction at the kernel level. Both are tasks. A "thread" is simply a task that shares certain resources (memory, files, etc.) with other tasks in the same thread group. The difference is determined by which resources are shared via clone() flags.
[Basic]

Q: What is a thread group in Linux?
A: A collection of tasks that share the same Thread Group ID (TGID). All threads created by pthread_create() share their parent's TGID. From userspace, the TGID appears as the PID (via getpid()).
[Basic]

Q: [Cloze] In Linux, getpid() returns the _____, while gettid() returns the _____.
A: In Linux, getpid() returns the **TGID** (thread group ID), while gettid() returns the **PID** (actual task ID).
[Basic]

Q: What is the PID of the first process (init/systemd) in Linux?
A: PID 1. This process is special: it adopts orphaned processes and cannot be killed by normal signals.
[Basic]

Q: What are the main process states in Linux kernel v3.2?
A: 
- TASK_RUNNING (0) - runnable or currently running
- TASK_INTERRUPTIBLE (1) - sleeping, can be woken by signals
- TASK_UNINTERRUPTIBLE (2) - sleeping, cannot be interrupted
- __TASK_STOPPED (4) - stopped by signal (SIGSTOP, SIGTSTP)
- __TASK_TRACED (8) - stopped by debugger (ptrace)
- EXIT_ZOMBIE (16) - terminated, waiting for parent to collect status
- EXIT_DEAD (32) - final state, being removed
[Basic]

Q: [ASCII Diagram] Draw the process state transition diagram.
A:
```
                    +------------------+
                    |   TASK_RUNNING   |<----+
                    | (Ready or Running)|    |
                    +--------+---------+     |
                             |               |
          +------------------+---------------+------------------+
          |                  |               |                  |
          v                  v               v                  v
+-------------------+ +-------------+ +-------------+ +------------------+
|TASK_INTERRUPTIBLE | |TASK_STOPPED | |TASK_TRACED  | |TASK_UNINTERRUPT- |
|   (sleeping,      | |(SIGSTOP/    | |(ptrace      | |IBLE (sleeping,   |
|    wakeable)      | | SIGTSTP)    | | attached)   | | not wakeable)    |
+-------------------+ +-------------+ +-------------+ +------------------+
          |                  |               |                  |
          +------------------+---------------+------------------+
                             |
                             v
                    +------------------+
                    |   EXIT_ZOMBIE    |
                    | (terminated,     |
                    |  awaiting wait())|
                    +--------+---------+
                             |
                             v (parent calls wait())
                    +------------------+
                    |    EXIT_DEAD     |
                    | (being removed)  |
                    +------------------+
```
[Intermediate]

Q: Why does TASK_UNINTERRUPTIBLE exist? When is it used?
A: TASK_UNINTERRUPTIBLE prevents a sleeping process from being woken by signals. It's used during critical I/O operations where waking early could leave the system in an inconsistent state (e.g., waiting for disk I/O to complete, NFS operations). The process must complete the operation before it can be interrupted.
[Intermediate]

Q: What is the "D state" in `ps` output and why is it concerning if processes stay there?
A: "D" represents TASK_UNINTERRUPTIBLE (disk sleep). Processes in D state cannot be killed, even with SIGKILL. If a process is stuck in D state indefinitely, it usually indicates a kernel bug or hardware problem (e.g., hung NFS mount, failing disk).
[Intermediate]

Q: What is a zombie process and why does it exist?
A: A zombie (EXIT_ZOMBIE) is a terminated process whose exit status has not yet been collected by its parent via wait(). It exists because the parent might need the exit status. Zombies consume minimal resources (just a task_struct entry) but waste PID slots.
[Basic]

Q: How are orphaned processes handled in Linux?
A: When a parent process terminates before its children, the orphaned children are "reparented" to init (PID 1) or the nearest subreaper process. Init periodically calls wait() to clean up any zombie orphans.
[Basic]

Q: What is PID 0 in Linux?
A: PID 0 is the idle task (swapper). There's one per CPU, and it runs when no other task is runnable. It's not a real process but a kernel construct that implements the idle loop.
[Intermediate]

Q: [Code Interpretation] What does this code check?
```c
if (current->mm == NULL)
    /* ... */
```
A: It checks if the current task is a kernel thread. Kernel threads have no user-space memory mapping (mm is NULL), while user processes always have an mm_struct.
[Intermediate]

Q: How do kernel threads differ from user processes?
A: Kernel threads:
- Run entirely in kernel mode
- Have no user-space address space (mm = NULL)
- Cannot be directly started from userspace
- Are created via kthread_create() or kernel_thread()
- Examples: kswapd, ksoftirqd, migration threads
[Intermediate]

Q: [Reverse] The `kthreadd` process has PID 2 and is responsible for...
A: Creating all other kernel threads. When kernel code needs a new kernel thread, it requests kthreadd to spawn it.
[Basic]

Q: What is the difference between `real_parent` and `parent` in task_struct?
A: 
- `real_parent`: The actual biological parent (the process that created this one)
- `parent`: The process that will receive SIGCHLD and wait() status (usually same as real_parent, but different when ptrace is involved - then parent is the tracer)
[Intermediate]

Q: [Cloze] The process family tree is linked via: parent points to _____, children is a list of _____, and sibling links _____.
A: The process family tree is linked via: parent points to **the parent task**, children is a list of **child tasks**, and sibling links **tasks with the same parent** (siblings).
[Basic]

Q: How can you traverse all processes in the system from kernel code?
A: Use the `for_each_process(p)` macro, which iterates through the circular doubly-linked list anchored at init_task using the `tasks` field.
```c
struct task_struct *p;
for_each_process(p) {
    printk("%s [%d]\n", p->comm, p->pid);
}
```
[Intermediate]

Q: What is `task->comm` and what's its maximum length?
A: `comm` is the process name (executable name), stored as a 16-character array (TASK_COMM_LEN = 16). It's what appears in `ps` output and can be changed via prctl(PR_SET_NAME).
[Basic]

Q: What is a "session" in Unix/Linux process management?
A: A session is a collection of process groups, typically associated with a controlling terminal. Created by setsid(), it's used for job control. The session leader's PID becomes the session ID (SID).
[Intermediate]

Q: What is a "process group" and how does it relate to job control?
A: A process group is a collection of related processes (e.g., a pipeline) that can receive signals together. Shell job control uses process groups to manage foreground/background jobs. Created via setpgid() or inherited from parent.
[Intermediate]

Q: [Understanding] Why does Linux use a unified task concept rather than separate process/thread implementations?
A: Design simplicity and flexibility. By using one structure (task_struct) with configurable resource sharing (via clone flags), Linux avoids code duplication and allows fine-grained control over what's shared. This 1:1 threading model maps each user thread to a kernel task.
[Intermediate]

Q: What is the significance of the `->flags` field in task_struct?
A: Contains per-task flags indicating various states and properties:
- PF_EXITING: process is exiting
- PF_KTHREAD: kernel thread
- PF_FORKNOEXEC: forked but hasn't exec'd
- PF_SUPERPRIV: used super-user privileges
- PF_SIGNALED: killed by a signal
[Intermediate]

Q: [Cloze] A process in state _____ appears as "R" in ps, while _____ appears as "S" (sleeping).
A: A process in state **TASK_RUNNING** appears as "R" in ps, while **TASK_INTERRUPTIBLE** appears as "S" (sleeping).
[Basic]

Q: What is the maximum PID value in Linux kernel v3.2 and how is it configured?
A: Default max is 32768 (PID_MAX_DEFAULT), but can be increased up to 4194304 (2^22) on 64-bit systems via /proc/sys/kernel/pid_max. PIDs wrap around and are recycled.
[Intermediate]

---

## Section 2: task_struct Deep Dive

Q: What is task_struct and where is it defined?
A: task_struct is the process descriptor - the central data structure representing a task in Linux. Defined in `include/linux/sched.h`. It's a large structure (~1.7KB in v3.2) containing all information the kernel needs about a process.
[Basic]

Q: How is task_struct memory allocated?
A: Via the slab allocator from a dedicated cache (`task_struct_cachep`). This provides efficient allocation/deallocation and keeps all process descriptors in the same cache for better memory locality.
[Intermediate]

Q: [Code Interpretation] What does this task_struct field store?
```c
volatile long state;
```
A: The current state of the task (TASK_RUNNING, TASK_INTERRUPTIBLE, etc.). It's marked `volatile` because it can be changed asynchronously by other kernel code (e.g., wake_up functions).
[Basic]

Q: What is thread_info and how does it relate to task_struct?
A: thread_info is a small, architecture-specific structure stored at the bottom of the kernel stack. It contains low-level thread data (CPU, preempt_count, flags) and a pointer to task_struct. This design allows fast access via stack pointer arithmetic.
[Intermediate]

Q: [ASCII Diagram] Show the relationship between kernel stack and thread_info.
A:
```
High Address
+------------------------+
|                        |
|    Kernel Stack        |
|    (grows downward)    |
|          |             |
|          v             |
|                        |
+------------------------+
|    thread_info         |
|  +------------------+  |
|  | task_struct *task|--+---> task_struct
|  | flags            |  |
|  | cpu              |  |
|  | preempt_count    |  |
|  +------------------+  |
+------------------------+
Low Address (THREAD_SIZE aligned)

Size: THREAD_SIZE (typically 8KB on x86)
```
[Intermediate]

Q: What is the `mm` field in task_struct?
A: A pointer to `mm_struct` - the memory descriptor that defines the process's address space. Contains info about memory regions (VMAs), page tables, memory statistics. NULL for kernel threads.
[Basic]

Q: What is `active_mm` and why is it needed?
A: `active_mm` points to the address space currently loaded in the MMU. For user processes, mm == active_mm. For kernel threads (mm == NULL), active_mm borrows the previous task's mm to avoid unnecessary TLB flushes when context switching.
[Advanced]

Q: [Cloze] The field `task->files` is a pointer to _____ which contains the _____.
A: The field `task->files` is a pointer to **files_struct** which contains the **file descriptor table**.
[Basic]

Q: What does the `fs` field in task_struct contain?
A: Points to `fs_struct` containing filesystem-related info: root directory, current working directory (pwd), and umask. Can be shared between threads via CLONE_FS.
[Intermediate]

Q: Explain the priority-related fields in task_struct.
A:
- `static_prio`: Base priority set by nice value (100-139 for normal tasks)
- `normal_prio`: Priority considering scheduling policy (may differ for RT tasks)
- `prio`: Dynamic/effective priority used by scheduler (can be boosted)
- `rt_priority`: Real-time priority (0-99, higher = more priority)
[Intermediate]

Q: [Code Interpretation] What do these task_struct fields represent?
```c
int prio, static_prio, normal_prio;
unsigned int rt_priority;
const struct sched_class *sched_class;
struct sched_entity se;
struct sched_rt_entity rt;
```
A: Scheduling-related fields:
- Priority values at different levels
- `sched_class`: pointer to scheduling class (CFS, RT, etc.)
- `se`: scheduling entity for CFS (contains vruntime, load weight)
- `rt`: scheduling entity for real-time scheduler
[Intermediate]

Q: What is `sched_entity` and why is it separate from task_struct?
A: `sched_entity` holds CFS-specific scheduling info (vruntime, load weight, rb_node). It's separate to support group scheduling - a sched_entity can represent either a task or a group of tasks, enabling hierarchical scheduling.
[Advanced]

Q: What are `real_cred` and `cred` in task_struct?
A: Credential structures containing UIDs, GIDs, and capabilities:
- `real_cred`: Objective credentials (who we really are)
- `cred`: Subjective credentials (who we're acting as)
Usually identical, but differ for setuid programs during exec().
[Intermediate]

Q: [Reverse] This field contains a pointer to the structure holding UID, GID, and capability information.
A: `task->cred` (or `task->real_cred`) - the credentials structure.
[Basic]

Q: What is the `signal` field in task_struct?
A: A pointer to `signal_struct` which is shared by all threads in a thread group. Contains process-wide signal handling info, resource limits (rlimits), job control state, and accumulated CPU times.
[Intermediate]

Q: What is `sighand` in task_struct?
A: Pointer to `sighand_struct` containing the array of signal handlers (k_sigaction structures). Shared among threads in the same thread group (CLONE_SIGHAND).
[Intermediate]

Q: [Cloze] Individual pending signals for a task are stored in _____, while thread-group-wide pending signals are in _____.
A: Individual pending signals for a task are stored in **task->pending**, while thread-group-wide pending signals are in **task->signal->shared_pending**.
[Intermediate]

Q: What are the `children` and `sibling` fields used for?
A: They form linked lists for the process tree:
- `children`: head of list containing all children of this task
- `sibling`: links this task with its siblings (same parent)
Used by: `list_for_each_entry(child, &parent->children, sibling)`
[Intermediate]

Q: [Code Interpretation] What does iterating through this accomplish?
```c
struct task_struct *child;
list_for_each_entry(child, &task->children, sibling) {
    /* process child */
}
```
A: Iterates through all child processes of `task`. Uses the `children` list head and `sibling` as the list linkage field.
[Intermediate]

Q: What is the `nsproxy` field in task_struct?
A: Points to `nsproxy` structure containing pointers to various namespaces: UTS, IPC, mount, PID, network, user. Namespaces provide isolation for containerization.
[Intermediate]

Q: What is `task->pid` vs `task->tgid`?
A: 
- `pid`: Unique task identifier (each thread has different pid)
- `tgid`: Thread group ID (same for all threads in a process)

For the main thread: pid == tgid
For other threads: pid != tgid (but shares tgid with main thread)
[Basic]

Q: [Understanding] Why doesn't task_struct directly store the PID number in some architectures?
A: Modern Linux uses `struct pid` to represent PIDs, allowing:
1. Multiple PID namespaces (same task can have different PIDs in different namespaces)
2. Efficient PID lookup via hash tables
3. Reference counting for PID reuse safety
[Advanced]

Q: What is `task->stack` in task_struct?
A: Pointer to the task's kernel stack. The stack is typically 2 pages (8KB on x86). thread_info sits at the bottom of this stack.
[Intermediate]

Q: How do you find task_struct from a kernel stack pointer?
A: Use `current_thread_info()->task`. The thread_info is at a known offset from the stack base (THREAD_SIZE aligned), and it contains a pointer to task_struct.
[Intermediate]

Q: What is `task->flags` and name some important flags.
A:
```c
unsigned int flags; /* per-process flags */
```
Important flags:
- PF_EXITING (0x04): Task is exiting
- PF_KTHREAD (0x200000): Kernel thread
- PF_RANDOMIZE: Randomize address space
- PF_FORKNOEXEC: Forked but not yet exec'd
- PF_NOFREEZE: Don't freeze this task
[Intermediate]

Q: What is `task->exit_code` and `task->exit_signal`?
A:
- `exit_code`: The exit status to report to parent (set by exit(), or signal number for killed processes)
- `exit_signal`: Signal to send to parent on death (usually SIGCHLD, can be changed via clone)
[Intermediate]

Q: [Cloze] The _____ field determines which signal is sent to the parent when the task exits, typically _____.
A: The **exit_signal** field determines which signal is sent to the parent when the task exits, typically **SIGCHLD**.
[Basic]

Q: What is `task->ptrace` used for?
A: Contains ptrace-related flags indicating if/how the task is being traced by a debugger. Includes PTRACE_SEIZED, PT_PTRACED, etc.
[Intermediate]

Q: What are `task->utime` and `task->stime`?
A: CPU time accounting:
- `utime`: Time spent in user mode
- `stime`: Time spent in kernel mode (system time)
Stored in cputime_t units, reported via times() and /proc/[pid]/stat.
[Basic]

Q: What is `task->nvcsw` and `task->nivcsw`?
A: Context switch counters:
- `nvcsw`: Voluntary context switches (task blocked/slept)
- `nivcsw`: Involuntary context switches (preempted by scheduler)
Useful for performance analysis.
[Intermediate]

Q: [Code Interpretation] What information can you extract from this?
```c
printk("Task: %s, PID: %d, State: %ld, Prio: %d\n",
       p->comm, p->pid, p->state, p->prio);
```
A: Process name, PID, current state (as numeric value), and effective priority. Basic task identification and status.
[Basic]

Q: What is `task->on_cpu` and when is it used?
A: A flag indicating whether the task is currently executing on a CPU. Used by the scheduler to prevent races when migrating running tasks.
[Advanced]

Q: What is `task->cpu` field?
A: The CPU number the task is currently running on or was last running on. Used for CPU affinity and scheduling decisions.
[Intermediate]

Q: [Reverse] This linked list field connects a task to others in the system-wide task list, starting from init_task.
A: `task->tasks` - a list_head linking all tasks via the global task list.
[Intermediate]

Q: What is `task->thread` (struct thread_struct)?
A: Architecture-specific structure containing CPU register state that must be saved/restored during context switch. Includes general-purpose registers, FPU state, debug registers.
[Intermediate]

---

## Section 3: Process Creation

Q: What are the three main system calls for process creation in Linux?
A:
- `fork()`: Creates a child as a copy of parent, both continue from fork return
- `vfork()`: Like fork but shares address space, parent blocks until child exits/execs
- `clone()`: Fine-grained control over resource sharing via flags
[Basic]

Q: What does fork() return?
A:
- In parent: Child's PID (positive number)
- In child: 0
- On error: -1 (only in parent, child not created)
[Basic]

Q: [Code Interpretation] What does this code do?
```c
pid_t pid = fork();
if (pid == 0) {
    execl("/bin/ls", "ls", "-l", NULL);
    _exit(1);
} else if (pid > 0) {
    wait(NULL);
}
```
A: Classic fork-exec pattern: forks a child, child replaces itself with `ls -l`, parent waits for child to complete. The _exit(1) handles exec failure.
[Basic]

Q: What is the kernel function that implements fork(), vfork(), and clone()?
A: `do_fork()` - all three system calls eventually call do_fork() with different flags.
[Basic]

Q: [Cloze] fork() calls do_fork() with flags _____, while clone() passes _____ from userspace.
A: fork() calls do_fork() with flags **SIGCHLD**, while clone() passes **user-specified flags** from userspace.
[Intermediate]

Q: What is the call path for fork() from system call to process creation?
A:
```
sys_fork()
  └─> do_fork(SIGCHLD, ...)
        └─> copy_process()
              ├─> dup_task_struct()
              ├─> copy_creds()
              ├─> copy_flags()
              ├─> copy_mm()
              ├─> copy_files()
              ├─> copy_fs()
              ├─> copy_sighand()
              ├─> copy_signal()
              └─> copy_thread()
        └─> wake_up_new_task()
```
[Intermediate]

Q: What does `copy_process()` do?
A: The core function that creates a new process:
1. Allocates new task_struct and kernel stack
2. Copies/shares parent's resources based on clone flags
3. Initializes scheduling info
4. Sets up the child's return value to 0
5. Returns pointer to new task_struct
[Intermediate]

Q: What is Copy-on-Write (COW) and why is it important for fork()?
A: COW delays copying memory pages until either process writes to them. After fork(), parent and child share the same physical pages (marked read-only). On write, a page fault triggers copying just that page. This makes fork() fast even for large processes.
[Basic]

Q: [ASCII Diagram] Show how COW works after fork().
A:
```
After fork() - pages shared:

Parent                        Child
+--------+                   +--------+
| Page   |                   | Page   |
| Tables |                   | Tables |
+---+----+                   +----+---+
    |                             |
    |  (Read-Only)    (Read-Only) |
    +--------+     +--------+-----+
             |     |
             v     v
         +------------+
         | Physical   |
         | Page       |
         +------------+

After child writes - page copied:

Parent                        Child
+--------+                   +--------+
| Page   |                   | Page   |
| Tables |                   | Tables |
+---+----+                   +----+---+
    |                             |
    v (R/W)               (R/W)   v
+------------+           +------------+
| Original   |           | Copied     |
| Page       |           | Page       |
+------------+           +------------+
```
[Intermediate]

Q: Why does vfork() exist if we have COW?
A: Historical optimization (pre-COW). Today, vfork() is still slightly faster because it completely skips creating page table entries. Useful when you immediately exec(). However, it's dangerous - child shares address space and parent blocks.
[Intermediate]

Q: What makes vfork() dangerous?
A: The child shares the parent's address space. If child modifies any variables (except for the return value location) or returns from the function that called vfork(), it corrupts the parent's state. Child must call _exit() or exec() immediately.
[Intermediate]

Q: What is clone() and how does it differ from fork()?
A: clone() is the general-purpose process/thread creation syscall. Unlike fork(), it takes flags to specify exactly which resources to share vs. copy:
- CLONE_VM: share memory space
- CLONE_FILES: share file descriptors
- CLONE_FS: share filesystem info
- etc.
[Basic]

Q: What clone() flags are used to create a POSIX thread?
A:
```c
CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | 
CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID
```
Key ones: CLONE_VM (share memory), CLONE_THREAD (same thread group), CLONE_SIGHAND (share signal handlers).
[Intermediate]

Q: [Code Interpretation] What kind of entity does this create?
```c
clone(fn, stack, CLONE_VM | CLONE_FS | CLONE_FILES | 
      CLONE_SIGHAND | CLONE_THREAD, arg);
```
A: A thread - shares memory (CLONE_VM), file descriptors (CLONE_FILES), filesystem info (CLONE_FS), signal handlers (CLONE_SIGHAND), and belongs to same thread group (CLONE_THREAD).
[Intermediate]

Q: What is CLONE_NEWPID used for?
A: Creates a new PID namespace. The child becomes PID 1 in the new namespace while having a different PID in the parent namespace. Used for containers.
[Intermediate]

Q: What is the difference between CLONE_PARENT and CLONE_THREAD?
A:
- CLONE_PARENT: New task has same parent as caller (sibling, not child)
- CLONE_THREAD: New task in same thread group, shares signal handlers, parent is thread group leader's parent
[Advanced]

Q: What does `dup_task_struct()` do?
A: Allocates and initializes a new task_struct:
1. Allocates task_struct from slab cache
2. Allocates kernel stack (THREAD_SIZE)
3. Sets up thread_info at bottom of stack
4. Copies parent's task_struct as template
5. Links task to its stack
[Intermediate]

Q: [Cloze] After dup_task_struct(), the new task's _____ equals 1, and _____ is set to TASK_UNINTERRUPTIBLE.
A: After dup_task_struct(), the new task's **usage count** equals 1, and **state** is set to TASK_UNINTERRUPTIBLE.
[Intermediate]

Q: What does `copy_mm()` do with and without CLONE_VM?
A:
- Without CLONE_VM: Creates new mm_struct, copies VMAs, sets up COW page tables
- With CLONE_VM: Increments mm->mm_users and shares parent's mm_struct directly
[Intermediate]

Q: What does `copy_files()` do?
A: Handles the file descriptor table:
- Without CLONE_FILES: Duplicates files_struct, increments refcount on each open file
- With CLONE_FILES: Shares files_struct (increments files->count)
[Intermediate]

Q: What does `copy_thread()` do?
A: Architecture-specific function that sets up the new task's thread_struct:
- Copies kernel register state
- Sets up return path so child returns 0 from fork()
- Sets up the child's kernel stack
[Intermediate]

Q: Why does the child process return 0 from fork() while parent gets PID?
A: `copy_thread()` sets up the child's register state so that when it's scheduled, it returns from do_fork() with 0 in the return register (eax on x86). Parent's do_fork() actually returns the child's PID.
[Intermediate]

Q: What happens in `wake_up_new_task()`?
A: Makes the new task runnable:
1. Sets state to TASK_RUNNING
2. Selects a CPU for the new task
3. Inserts task into run queue
4. Potentially triggers preemption if new task has higher priority
[Intermediate]

Q: [Understanding] Why might the child run before the parent after fork()?
A: The scheduler may favor the child to minimize COW faults. If the child typically exec()s immediately, running it first means its pages never need to be copied. This is a heuristic in some kernel versions.
[Advanced]

Q: What is a "fork bomb" and how does the kernel protect against it?
A: A fork bomb recursively forks: `:(){ :|:& };:`
Kernel protection:
- RLIMIT_NPROC: max processes per user
- PID limit (pid_max)
- Memory limits (can't allocate more task_structs)
[Intermediate]

Q: How are PIDs allocated during fork?
A: Via `alloc_pid()` which:
1. Allocates from per-namespace PID bitmap
2. Creates `struct pid` with namespace hierarchy
3. Assigns smallest available PID (or next after last)
PIDs are recycled when processes exit.
[Intermediate]

Q: [Reverse] This flag in clone() causes the child's exit signal to not be SIGCHLD but another specified signal.
A: The low 8 bits of clone flags specify the exit signal. Example: `clone(..., CLONE_VM | SIGUSR1, ...)` makes child send SIGUSR1 on exit instead of SIGCHLD.
[Advanced]

Q: What does CLONE_CHILD_CLEARTID do?
A: Sets up the child to clear a memory location (specified by child_tidptr) to 0 and wake futex waiters when it exits. Used by threading libraries (NPTL) to implement pthread_join().
[Intermediate]

Q: What does CLONE_CHILD_SETTID do?
A: Stores the child's TID at the specified address (child_tidptr) in the child's memory. Threading libraries use this to know the TID of a newly created thread.
[Intermediate]

Q: [Code Interpretation] What does this clone flag combination achieve?
```c
CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID
```
A: The child's TID is stored at tidptr when created, and cleared to 0 when it exits. This allows the parent to detect thread termination by polling or using futex_wait on that address.
[Intermediate]

---

## Section 4: Process Execution (exec)

Q: What is the purpose of the exec() family of functions?
A: To replace the current process image with a new program. The process keeps its PID but gets new code, data, stack, and heap. Completes the "fork-exec" model for running new programs.
[Basic]

Q: List the exec() variants and their differences.
A:
- execl(): args as list, uses PATH
- execv(): args as array, uses PATH
- execle(): list args + environment
- execve(): array args + environment (actual syscall)
- execlp(): list args, searches PATH
- execvp(): array args, searches PATH

'l' = list, 'v' = vector, 'e' = environment, 'p' = PATH search
[Basic]

Q: Which exec() function is the actual system call?
A: `execve()` - all other exec functions are library wrappers that eventually call execve().
```c
int execve(const char *filename, 
           char *const argv[], 
           char *const envp[]);
```
[Basic]

Q: What is the kernel call path for execve()?
A:
```
sys_execve()
  └─> do_execve()
        └─> do_execve_common()
              ├─> open_exec() - open executable
              ├─> bprm_init() - init linux_binprm
              ├─> prepare_binprm() - fill binprm
              ├─> copy_strings() - copy argv/envp
              ├─> search_binary_handler()
              │     └─> load_elf_binary() [for ELF]
              └─> cleanup on success/failure
```
[Intermediate]

Q: What is `struct linux_binprm`?
A: Binary program descriptor holding information needed during exec:
- `buf`: first 128 bytes of file (magic number detection)
- `filename`: path to executable
- `interp`: interpreter path (for scripts)
- `argc`, `envc`: argument/environment counts
- `p`: current top of stack
- `cred`: new credentials to apply
[Intermediate]

Q: [Cloze] The first _____ bytes of the executable are read into bprm->buf to identify the _____.
A: The first **128** bytes of the executable are read into bprm->buf to identify the **binary format**.
[Intermediate]

Q: What is a binary format handler (struct linux_binfmt)?
A: A kernel module that knows how to load and execute a specific executable format:
- ELF format: `elf_format` (most common)
- Script format: `script_format` (#! scripts)
- Misc format: `misc_format` (for wine, java, etc.)
[Intermediate]

Q: How does the kernel handle a #! script?
A:
1. Reads first line, finds `#!interpreter [arg]`
2. Opens the interpreter
3. Constructs new argv: interpreter, optional-arg, script-path, original-args
4. Calls binary handler for interpreter
```
#!bin/sh becomes: /bin/sh script.sh arg1 arg2
```
[Intermediate]

Q: [Code Interpretation] What does this kernel code do?
```c
list_for_each_entry(fmt, &formats, lh) {
    retval = fmt->load_binary(bprm);
    if (retval >= 0)
        break;
}
```
A: Iterates through registered binary format handlers, trying each one until one successfully loads the binary. Returns success from first matching handler.
[Intermediate]

Q: What does `load_elf_binary()` do?
A: Loads and sets up an ELF executable:
1. Parses ELF header and program headers
2. Maps code and data segments into memory
3. Sets up the stack (argv, envp, aux vector)
4. Loads interpreter (ld-linux.so) if needed
5. Sets entry point (or interpreter entry)
6. Triggers actual execution
[Intermediate]

Q: What is the ELF auxiliary vector (auxv)?
A: Data passed on stack to user process, containing runtime information:
- AT_PHDR: program header location
- AT_ENTRY: program entry point
- AT_BASE: interpreter base address
- AT_UID, AT_EUID, AT_GID, AT_EGID
- AT_RANDOM: address of 16 random bytes
- AT_PLATFORM: architecture string
[Advanced]

Q: What happens to the process address space during exec()?
A: Completely replaced:
1. Old mm_struct released (refcount decremented)
2. New mm_struct created
3. Old mappings destroyed
4. New code, data, BSS segments mapped
5. New stack created
6. Heap reinitialized (brk reset)
[Intermediate]

Q: [Understanding] Why don't open file descriptors close on exec() by default?
A: Unix design decision - allows parent to set up file descriptors (stdin, stdout, stderr, pipes) before exec and have them inherited by the new program. FD_CLOEXEC flag closes a descriptor on exec.
[Intermediate]

Q: What is the close-on-exec flag and how is it set?
A: FD_CLOEXEC flag marks a file descriptor to be closed during exec():
```c
fcntl(fd, F_SETFD, FD_CLOEXEC);
// or when opening:
open("file", O_RDONLY | O_CLOEXEC);
```
[Basic]

Q: What happens to signal handlers during exec()?
A: All signal handlers are reset to default. Since the old code is gone, custom handlers would crash. Signal masks and ignored signals (except SIGKILL/SIGSTOP) are preserved.
[Intermediate]

Q: What happens to pending signals during exec()?
A: Pending signals remain pending. They'll be delivered to the new program (with default handlers, since custom handlers are reset).
[Intermediate]

Q: [Cloze] During exec(), memory mappings are _____, but file descriptors (without FD_CLOEXEC) are _____.
A: During exec(), memory mappings are **destroyed/replaced**, but file descriptors (without FD_CLOEXEC) are **inherited/preserved**.
[Basic]

Q: What is a setuid binary and how does exec() handle it?
A: A setuid binary has the SUID bit set and runs with the file owner's privileges (not caller's). During exec():
1. Kernel checks setuid bit
2. Sets euid to file owner
3. New cred->euid differs from real uid
4. Certain security measures activate (LD_PRELOAD ignored, etc.)
[Intermediate]

Q: How does exec() handle memory-mapped files?
A: All memory mappings (including mmap'd files) are unmapped. The mapping doesn't affect the underlying file - it was just a view into the process's address space which is being replaced.
[Intermediate]

Q: What is `flush_old_exec()` responsible for?
A: Point of no return in exec - cleans up old process state:
- Releases old mm
- Clears thread group state
- Resets signal handlers
- Called after we're committed to the new executable
[Advanced]

Q: [Reverse] This exec function searches PATH for the executable: execl, execv, or execvp?
A: `execvp()` (and `execlp()`) - the 'p' suffix indicates PATH searching.
[Basic]

---

## Section 5: Process Termination

Q: What is the difference between exit() and _exit()?
A:
- `exit()` (library function): Runs atexit handlers, flushes stdio buffers, then calls _exit()
- `_exit()` (syscall): Immediately terminates, no cleanup

Use _exit() in child after failed exec() to avoid double-flushing buffers.
[Basic]

Q: What is exit_group() and when is it used?
A: Terminates all threads in the thread group, not just the calling thread. The `exit()` library function actually calls exit_group() to ensure all threads terminate together.
[Intermediate]

Q: What is the kernel call path for process termination?
A:
```
sys_exit_group() / sys_exit()
  └─> do_group_exit()
        └─> do_exit()
              ├─> set PF_EXITING flag
              ├─> exit_signals() - dequeue signals
              ├─> exit_mm() - release address space
              ├─> exit_sem() - release semaphores
              ├─> exit_files() - close files
              ├─> exit_fs() - release fs_struct
              ├─> exit_notify() - reparent children, notify parent
              └─> schedule() - never returns
```
[Intermediate]

Q: What does `do_exit()` do?
A: Main termination handler:
1. Sets state to TASK_DEAD
2. Releases all resources (mm, files, fs, signals)
3. Reparents children to init
4. Sets exit_code
5. Notifies parent (SIGCHLD)
6. Calls schedule() - task becomes zombie until reaped
[Intermediate]

Q: [Cloze] After do_exit(), the task enters _____ state until the parent calls _____().
A: After do_exit(), the task enters **EXIT_ZOMBIE** state until the parent calls **wait()**.
[Basic]

Q: Why do zombie processes exist?
A: To preserve exit status for the parent. The parent might need to know:
- Exit code (return value or signal number)
- Resource usage statistics (rusage)
- Which child terminated

Without zombies, this information would be lost before parent can retrieve it.
[Basic]

Q: What resources does a zombie process hold?
A: Minimal - only the task_struct (process descriptor) and associated struct pid. All memory, files, and other resources have been released. This is why zombies are relatively harmless individually but waste PID slots.
[Intermediate]

Q: What is an orphan process and how is it handled?
A: A process whose parent has terminated. The kernel reparents orphans to:
1. A "child subreaper" (process that called prctl(PR_SET_CHILD_SUBREAPER))
2. Or init (PID 1) if no subreaper exists
Init regularly calls wait() to clean up zombie orphans.
[Basic]

Q: [Code Interpretation] What problem does this code pattern solve?
```c
signal(SIGCHLD, SIG_IGN);
```
A: Prevents zombie accumulation. When SIGCHLD is explicitly ignored, the kernel automatically reaps child processes without requiring wait(). However, you lose the ability to retrieve exit status.
[Intermediate]

Q: What is the difference between wait(), waitpid(), wait4(), and waitid()?
A:
- `wait()`: Waits for any child, blocks
- `waitpid()`: Waits for specific PID (or group), options for non-blocking
- `wait4()`: Like waitpid + returns resource usage (rusage)
- `waitid()`: Most flexible, waits by PID/PGID/any, detailed status
[Intermediate]

Q: What do the WNOHANG and WUNTRACED flags do?
A:
- `WNOHANG`: Don't block if no child has terminated (return immediately with 0)
- `WUNTRACED`: Also report stopped children (not just terminated)
- `WCONTINUED`: Also report continued children (after SIGCONT)
[Intermediate]

Q: [Code Interpretation] What does this code do?
```c
while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFEXITED(status))
        printf("Child %d exited with %d\n", 
               pid, WEXITSTATUS(status));
}
```
A: Non-blocking reap of all terminated children. Loops until no more zombies are ready, printing exit status for each normally-terminated child.
[Intermediate]

Q: What macros are used to interpret wait status?
A:
- `WIFEXITED(status)`: True if exited normally
- `WEXITSTATUS(status)`: Exit code (if WIFEXITED)
- `WIFSIGNALED(status)`: True if killed by signal
- `WTERMSIG(status)`: Signal number (if WIFSIGNALED)
- `WIFSTOPPED(status)`: True if stopped
- `WSTOPSIG(status)`: Stop signal (if WIFSTOPPED)
[Intermediate]

Q: What happens when the init process's parent terminates?
A: This cannot happen normally - init (PID 1) is special and has no parent. If init exits, the kernel panics because the system can no longer function (no process to adopt orphans).
[Intermediate]

Q: What is `exit_notify()` responsible for?
A:
1. Notifies parent of our death (SIGCHLD)
2. Reparents our children to init or subreaper
3. If we're a thread group leader, makes next thread the leader
4. Determines if we should become zombie or autodestruct
[Intermediate]

Q: [Cloze] The _____ flag on a process means it was terminated by a signal, and _____ flag means it used superuser privileges during its lifetime.
A: The **PF_SIGNALED** flag on a process means it was terminated by a signal, and **PF_SUPERPRIV** flag means it used superuser privileges during its lifetime.
[Intermediate]

Q: When is a task automatically reaped without requiring wait()?
A: 
1. When parent has SIGCHLD set to SIG_IGN
2. When clone() was called with exit_signal = 0
3. When all threads in a thread group die (non-leader threads don't zombie)
[Intermediate]

Q: What does `release_task()` do?
A: Final cleanup after a task is reaped:
1. Removes from task lists (PID hash, all-tasks list)
2. Releases struct pid
3. Frees task_struct and kernel stack
4. Decrements parent's child count
Called by wait() after collecting zombie status.
[Intermediate]

Q: [Understanding] Why must a process's memory be released before it becomes a zombie?
A: Memory is a significant resource. If zombies held onto memory, a fork bomb that creates zombies would quickly exhaust system memory. By releasing memory in do_exit() before entering zombie state, we minimize zombie resource consumption.
[Intermediate]

Q: How does a thread group exit work?
A: When a thread calls exit_group():
1. Sets SIGNAL_GROUP_EXIT in signal->flags
2. Sets signal->group_exit_code
3. Sends SIGKILL to all other threads in group
4. Each thread exits via do_exit()
5. Thread group leader stays zombie until reaped
[Advanced]

Q: What is the significance of exit_code in task_struct?
A: The value returned to the parent via wait():
- Low 8 bits: exit value (0-255) from exit() call
- Or: signal number if killed by signal
- High bits: core dump flag, stop signal info
[Intermediate]

Q: [Reverse] This kernel function is called when a parent successfully reaps a zombie child.
A: `release_task()` - frees the zombie's task_struct and associated resources.
[Intermediate]

Q: What happens to shared file descriptors when a process exits?
A: Reference counts are decremented. If the count reaches 0, the file is actually closed (file position, locks released). Other processes sharing the fd table still have access.
[Intermediate]

Q: [ASCII Diagram] Show the process termination and reaping flow.
A:
```
Process Running
     |
     | exit() called
     v
+-----------------+
| do_exit()       |
| - Release mm    |
| - Close files   |
| - Notify parent |
+-----------------+
     |
     v
+------------------+    Parent calls wait()    +------------------+
| EXIT_ZOMBIE      |-------------------------->| release_task()   |
| (awaiting reap)  |                           | - Free task_struct|
| - Holds exit_code|                           | - Remove from lists|
| - Minimal memory |                           +------------------+
+------------------+                                    |
                                                       v
                                              Process fully removed
```
[Intermediate]

---

## Section 6: CFS Scheduler Internals

Q: What is CFS and when was it introduced?
A: Completely Fair Scheduler - the default Linux scheduler for normal processes since kernel 2.6.23 (2007). It aims to give each process a fair share of CPU time based on its weight.
[Basic]

Q: What is the core idea behind CFS?
A: Model an ideal, perfectly fair multi-tasking processor where each runnable task runs simultaneously at 1/n of CPU speed (n = number of tasks). CFS approximates this by tracking how much CPU time each task "deserves" vs what it got.
[Intermediate]

Q: What is virtual runtime (vruntime)?
A: A measure of how much CPU time a task has received, weighted by its priority. Lower vruntime = has received less than its fair share = should run next. The task with smallest vruntime is always chosen to run.
[Basic]

Q: [Cloze] CFS always selects the task with the _____ vruntime to run next, stored in a _____ tree.
A: CFS always selects the task with the **smallest** vruntime to run next, stored in a **red-black** tree.
[Basic]

Q: How does CFS use a red-black tree?
A: Tasks are keyed by vruntime in a red-black tree (cfs_rq->tasks_timeline). The leftmost node (smallest vruntime) is cached for O(1) access. Insert/delete are O(log n). This allows efficient selection of the "most deserving" task.
[Intermediate]

Q: [ASCII Diagram] Show the CFS red-black tree structure.
A:
```
         struct cfs_rq
         +-----------------+
         | tasks_timeline  |---> Root of RB-tree
         | rb_leftmost     |---> Cached leftmost (next to run)
         | nr_running      |     
         | min_vruntime    |     Tracks minimum for fairness
         +-----------------+

Red-Black Tree (keyed by vruntime):
                    (50)
                   /    \
                (30)    (80)
               /    \      \
            (20)   (40)    (90)
              ^
              |
         rb_leftmost (runs next)
         
Each node is a sched_entity:
+-----------------+
| vruntime        | <- key
| load.weight     |
| rb_node         | <- tree linkage
+-----------------+
```
[Intermediate]

Q: What is struct sched_entity?
A: The CFS scheduling unit. Contains:
- `vruntime`: virtual runtime
- `load`: task weight
- `rb_node`: red-black tree node
- `on_rq`: whether on run queue
- Statistics (wait time, run time, etc.)

Can represent a task or a group (for group scheduling).
[Intermediate]

Q: What is `struct cfs_rq`?
A: The CFS run queue structure:
- `tasks_timeline`: root of RB-tree
- `rb_leftmost`: pointer to leftmost node (next to run)
- `min_vruntime`: monotonically increasing floor
- `nr_running`: count of runnable tasks
- `load`: total load of all tasks
- `curr`: currently running entity
[Intermediate]

Q: What is min_vruntime and why does it exist?
A: A monotonically increasing value representing the minimum vruntime across all tasks. Used to:
1. Set initial vruntime for new/waking tasks (prevents starvation of existing tasks)
2. Prevent vruntime values from overflowing
3. Normalize vruntime when tasks sleep/wake
[Intermediate]

Q: How is a new task's initial vruntime set?
A: Set to max(min_vruntime, parent's vruntime). This prevents a newly forked task from having a huge advantage over tasks that have been running. It starts "fair" relative to other tasks.
[Intermediate]

Q: [Code Interpretation] What does this simplified CFS logic do?
```c
se = __pick_first_entity(cfs_rq);
if (se)
    return se;
```
A: Selects the next task to run by getting the leftmost (lowest vruntime) entity from the red-black tree. Returns NULL if no runnable tasks.
[Basic]

Q: How does `update_curr()` work?
A:
```c
delta_exec = now - curr->exec_start;
curr->exec_start = now;
curr->sum_exec_runtime += delta_exec;
curr->vruntime += calc_delta_fair(delta_exec, curr);
update_min_vruntime(cfs_rq);
```
Adds actual runtime to vruntime (weighted by priority). Called frequently during execution.
[Intermediate]

Q: How does task weight (nice value) affect vruntime accumulation?
A: Higher weight (lower nice) = vruntime increases slower
Lower weight (higher nice) = vruntime increases faster

A nice 0 task (weight 1024) accumulates vruntime at 1:1 with real time.
A nice -20 task (weight 88761) accumulates ~88x slower.
A nice +19 task (weight 15) accumulates ~68x faster.
[Intermediate]

Q: [Cloze] A task with nice value _____ has the default weight of 1024, while _____ nice values result in slower vruntime accumulation.
A: A task with nice value **0** has the default weight of 1024, while **negative** nice values result in slower vruntime accumulation.
[Basic]

Q: What is the relationship between nice values and weights?
A: Weights follow a ~1.25x ratio per nice level:
```
Nice -20: weight ~88761
Nice -10: weight ~9548  
Nice 0:   weight 1024
Nice 10:  weight ~110
Nice 19:  weight ~15
```
Each nice level represents ~10% CPU time difference.
[Intermediate]

Q: What is `sched_latency_ns` and `sched_min_granularity_ns`?
A: CFS tuning parameters:
- `sched_latency_ns` (default 6ms): Target scheduling period - ideally each task runs once within this period
- `sched_min_granularity_ns` (default 0.75ms): Minimum time slice per task
If nr_running is high, latency is extended to ensure each task gets at least min_granularity.
[Intermediate]

Q: [Understanding] Why doesn't CFS use strict time slices?
A: CFS allocates "fair" share dynamically based on current competition. Time slices would require knowing task count in advance. Instead, CFS continuously rebalances - tasks that have received more than their share naturally get preempted for tasks that received less.
[Intermediate]

Q: How does CFS handle sleeping tasks?
A: When a task sleeps:
1. Removed from RB-tree
2. vruntime preserved

When it wakes:
1. vruntime adjusted to max(old_vruntime, min_vruntime - sched_latency)
2. Reinserted into RB-tree
3. May preempt current task if it now has smallest vruntime
[Intermediate]

Q: What is "sleeper fairness" in CFS?
A: Fairness toward tasks that have been sleeping. When a task wakes, it might have a very old (small) vruntime and would dominate. CFS limits the advantage by setting floor at min_vruntime - sched_latency. This balances interactivity vs CPU fairness.
[Intermediate]

Q: What is the `struct rq` run queue?
A: Per-CPU run queue containing all scheduler data:
- `cfs`: embedded CFS run queue (struct cfs_rq)
- `rt`: RT scheduler run queue
- `nr_running`: total runnable tasks
- `curr`: currently running task
- `idle`: idle task for this CPU
- `clock`: rq time (for accounting)
[Intermediate]

Q: [ASCII Diagram] Show the per-CPU run queue structure.
A:
```
CPU 0                         CPU 1
+------------------+          +------------------+
| struct rq        |          | struct rq        |
+------------------+          +------------------+
| nr_running: 5    |          | nr_running: 3    |
| curr: task_A     |          | curr: task_D     |
| clock: ...       |          | clock: ...       |
+------------------+          +------------------+
| struct cfs_rq    |          | struct cfs_rq    |
| +--------------+ |          | +--------------+ |
| | RB-tree of   | |          | | RB-tree of   | |
| | CFS tasks    | |          | | CFS tasks    | |
| +--------------+ |          | +--------------+ |
+------------------+          +------------------+
| struct rt_rq     |          | struct rt_rq     |
| +--------------+ |          | +--------------+ |
| | RT task lists| |          | | RT task lists| |
| +--------------+ |          | +--------------+ |
+------------------+          +------------------+
```
[Intermediate]

Q: How does CFS decide when to preempt the current task?
A: In `check_preempt_curr()` after wakeup:
1. Compare waking task's vruntime with current task's vruntime
2. If waking task's vruntime is significantly smaller (by `sched_wakeup_granularity_ns`), set TIF_NEED_RESCHED
3. On next scheduling point, schedule() is called
[Intermediate]

Q: What is `sched_wakeup_granularity_ns`?
A: Minimum vruntime difference required to preempt the current task (default 1ms). Prevents excessive context switching when vruntimes are similar. Must have at least this much "unfairness" before preemption.
[Intermediate]

Q: [Code Interpretation] What does this check determine?
```c
if (likely(curr->vruntime > se->vruntime))
    if (curr->vruntime - se->vruntime > 
        wakeup_gran(curr, se))
        resched_task(rq->curr);
```
A: Checks if the waking task's vruntime is sufficiently smaller than current's to warrant preemption. Only preempts if the difference exceeds the wakeup granularity.
[Advanced]

Q: What is load balancing in CFS?
A: Periodically moving tasks between CPUs to ensure balanced load:
1. Each CPU calculates its load (sum of task weights)
2. Imbalance detected when loads differ significantly
3. Tasks migrated from busy CPUs to idle CPUs
4. Triggered by scheduler_tick(), idle entry, task wakeup
[Intermediate]

Q: What is `pick_next_task_fair()`?
A: Main CFS function to select the next task:
1. If current is still runnable and has smallest vruntime, keep it
2. Otherwise, call `pick_next_entity()` to get leftmost from RB-tree
3. Call `set_next_entity()` to prepare chosen task
4. Return task_struct (via `task_of()` from sched_entity)
[Intermediate]

Q: What is the `sched_class` structure?
A: Interface for scheduling classes (strategy pattern):
```c
struct sched_class {
    const struct sched_class *next;
    void (*enqueue_task)(rq, task, flags);
    void (*dequeue_task)(rq, task, flags);
    void (*check_preempt_curr)(rq, task, flags);
    struct task_struct *(*pick_next_task)(rq);
    void (*put_prev_task)(rq, task);
    void (*task_tick)(rq, task, queued);
    ...
};
```
[Intermediate]

Q: [Cloze] CFS uses the _____ scheduling class, while real-time tasks use the _____ class.
A: CFS uses the **fair_sched_class** scheduling class, while real-time tasks use the **rt_sched_class** class.
[Basic]

Q: What is the priority order of scheduling classes in Linux?
A:
```
stop_sched_class (highest - migration/cpu hotplug)
    |
    v
rt_sched_class (real-time: SCHED_FIFO, SCHED_RR)
    |
    v
fair_sched_class (CFS: SCHED_NORMAL, SCHED_BATCH)
    |
    v
idle_sched_class (lowest - SCHED_IDLE, idle task)
```
Higher classes are checked first in pick_next_task.
[Intermediate]

Q: What is group scheduling in CFS?
A: Allows grouping tasks for fairness between groups rather than just tasks. Example: User A with 100 tasks and User B with 1 task - with group scheduling, each USER gets 50% CPU, not each task.
Uses cgroups (cpu controller) to define groups.
[Advanced]

Q: [Understanding] Why is CFS called "completely fair" when it clearly isn't mathematically perfect?
A: It's "completely fair" in the sense that it:
1. Always runs the task that has received least CPU time (relatively)
2. Doesn't use arbitrary time slices
3. Treats all tasks according to their weight
The name reflects the design philosophy, not a mathematical guarantee.
[Intermediate]

---

## Section 7: Real-Time and Other Schedulers

Q: What are the real-time scheduling policies in Linux?
A:
- `SCHED_FIFO`: First-in-first-out, no time slicing, runs until it blocks or is preempted by higher priority
- `SCHED_RR`: Round-robin, like FIFO but with time slicing among same-priority tasks
- `SCHED_DEADLINE`: Deadline-based (added later, experimental in v3.2)
[Basic]

Q: What is the priority range for real-time tasks?
A: RT priorities range from 1-99 (99 highest). In kernel representation:
- MAX_RT_PRIO = 100
- RT tasks have p->prio from 0-99
- Normal tasks have p->prio from 100-139 (nice -20 to +19)
[Intermediate]

Q: [Cloze] Real-time tasks with policy _____ share CPU time among same-priority tasks, while _____ tasks run until they voluntarily yield or are preempted by higher priority.
A: Real-time tasks with policy **SCHED_RR** share CPU time among same-priority tasks, while **SCHED_FIFO** tasks run until they voluntarily yield or are preempted by higher priority.
[Basic]

Q: How does SCHED_FIFO work?
A:
1. Task runs until it:
   - Blocks (I/O, sleep, etc.)
   - Yields voluntarily (sched_yield())
   - Is preempted by higher priority RT task
2. No time slicing - will run forever if not blocked
3. Among same priority: strict FIFO ordering
[Intermediate]

Q: How does SCHED_RR differ from SCHED_FIFO?
A: SCHED_RR adds round-robin time slicing among tasks with the same priority:
1. Each task gets a time quantum (default: 100ms)
2. When quantum expires, task goes to end of queue for its priority
3. Still preempted by higher priority tasks immediately
[Intermediate]

Q: What is the struct rt_rq?
A: Real-time run queue structure:
```c
struct rt_rq {
    struct rt_prio_array active;  /* priority-indexed queues */
    unsigned long rt_nr_running;
    int highest_prio;
    ...
};
```
Uses a bitmap + array of lists for O(1) priority lookup.
[Intermediate]

Q: [ASCII Diagram] Show the RT scheduler priority array structure.
A:
```
struct rt_prio_array
+------------------+
| bitmap[3]        |  <- Bit set if priority level has tasks
| [00000001...]    |     (100 bits for priorities 0-99)
+------------------+
| queue[100]       |  <- Array of list heads
| [0] list_head ---+---> task_99a <-> task_99b  (highest prio)
| [1] list_head ---+---> task_98a
| ...              |
| [98] list_head --+---> task_1a
| [99] list_head --+---> task_0a <-> task_0b  (lowest RT prio)
+------------------+

Selection: find_first_bit(bitmap) -> O(1)
```
[Advanced]

Q: How does pick_next_task_rt() work?
A:
1. Find highest priority with runnable tasks: `sched_find_first_bit(bitmap)`
2. Get first task from that priority's list
3. O(1) complexity due to bitmap and list structure
[Intermediate]

Q: What is priority inversion?
A: When a high-priority task is blocked waiting for a resource held by a low-priority task:
```
High-prio task (H) waits for lock held by low-prio (L)
Medium-prio task (M) preempts L
H is effectively blocked by M (inversion!)
```
[Intermediate]

Q: How does Linux mitigate priority inversion?
A: Priority Inheritance (PI):
1. When high-priority task blocks on mutex held by low-priority task
2. Low-priority task temporarily inherits high priority
3. Can now run without being preempted by medium-priority tasks
4. Priority restored when mutex released

Requires PI-aware mutex (rt_mutex).
[Intermediate]

Q: [Understanding] Why can't an RT task starve the entire system?
A: Kernel protections:
1. `rt_runtime_us` and `rt_period_us` limit RT CPU time (default: 950ms per 1000ms)
2. Non-RT tasks guaranteed at least 5% CPU
3. Can be disabled by setting rt_runtime_us = -1
4. Also: RT tasks can still be preempted by higher prio RT tasks
[Intermediate]

Q: What is SCHED_BATCH?
A: A CFS scheduling policy optimized for CPU-bound batch jobs:
- Treated as lower priority for preemption decisions
- Won't preempt interactive tasks even with smaller vruntime
- Batches CPU usage into longer runs
- Same vruntime accounting as SCHED_NORMAL
[Intermediate]

Q: What is SCHED_IDLE?
A: Lowest priority scheduling policy - only runs when no other tasks are runnable (except actual idle). Used for very low priority background work. Even lower priority than nice +19 SCHED_NORMAL tasks.
[Intermediate]

Q: [Cloze] The scheduling classes are checked in order: _____ (highest), _____, _____, _____ (lowest).
A: The scheduling classes are checked in order: **stop** (highest), **rt**, **fair**, **idle** (lowest).
[Basic]

Q: What is the stop scheduling class used for?
A: Highest priority class for critical kernel operations:
- CPU migration (move tasks between CPUs)
- CPU hotplug (bring CPUs online/offline)
- Stop-machine (all-CPU synchronization)
Not available to user tasks - kernel only.
[Advanced]

Q: How do you set a process to real-time scheduling?
A: Using sched_setscheduler():
```c
struct sched_param param;
param.sched_priority = 50;  // 1-99
sched_setscheduler(pid, SCHED_FIFO, &param);
```
Requires CAP_SYS_NICE or superuser.
[Intermediate]

---

## Section 8: Context Switching and Process Context

Q: What is a context switch?
A: The process of saving the current task's state and loading another task's state so it can run. Involves saving/restoring CPU registers, switching address space (page tables), and updating kernel data structures.
[Basic]

Q: What triggers a context switch?
A:
1. Task blocks (I/O, sleep, wait for lock)
2. Task exhausts its time slice / higher priority task needs CPU
3. Task explicitly yields (sched_yield())
4. Interrupt handler wakes higher priority task
5. Task exits
[Basic]

Q: What is the main function that performs context switching?
A: `schedule()` - the core scheduler function. Selects next task to run and calls `context_switch()` to perform the actual switch.
[Basic]

Q: [Code Interpretation] What does this schedule() simplified logic do?
```c
need_resched:
    prev = current;
    next = pick_next_task(rq);
    if (prev != next) {
        context_switch(rq, prev, next);
    }
```
A: Saves current task reference, selects next task to run via scheduling class, and if different from current, performs context switch. This is the core scheduling loop.
[Intermediate]

Q: What does `context_switch()` do?
A:
```c
context_switch(rq, prev, next)
{
    prepare_task_switch(rq, prev, next);
    
    mm = next->mm;
    if (!mm) {  // kernel thread
        next->active_mm = prev->active_mm;
        atomic_inc(&prev->active_mm->mm_count);
    } else {
        switch_mm(prev->active_mm, mm, next);
    }
    
    switch_to(prev, next, prev);
    
    finish_task_switch(prev);
}
```
[Intermediate]

Q: What is `switch_mm()` responsible for?
A: Switching the memory address space:
1. Loads new process's page table base into CR3 (x86) or equivalent
2. May flush TLB entries for old address space
3. Updates CPU's mm state
For kernel threads (no mm), skipped - they borrow previous mm.
[Intermediate]

Q: What does the `switch_to()` macro do?
A: Architecture-specific CPU state switch:
1. Saves prev's kernel stack pointer and registers
2. Switches to next's kernel stack
3. Restores next's saved registers
4. On x86: includes FPU state switch (lazy or eager)
5. Returns "prev" for finish_task_switch() cleanup
[Intermediate]

Q: [Cloze] switch_mm() handles _____ switching, while switch_to() handles _____ switching.
A: switch_mm() handles **address space (page table)** switching, while switch_to() handles **CPU register state** switching.
[Intermediate]

Q: What is the `current` macro and how does it work?
A: Returns pointer to currently running task's task_struct.
Implementation (x86-64):
```c
static inline struct task_struct *get_current(void)
{
    return percpu_read_stable(current_task);
}
#define current get_current()
```
Uses per-CPU variable for O(1) access.
[Basic]

Q: [Understanding] Why is `current` stored in a per-CPU variable rather than computed from stack pointer?
A: Performance and simplicity. While you can compute it from stack (thread_info is at stack base), reading a per-CPU variable is faster and more portable across architectures.
[Intermediate]

Q: What is TIF_NEED_RESCHED?
A: Thread flag indicating the task should be rescheduled ASAP. Set by:
- Scheduler when higher priority task becomes runnable
- Timer interrupt when task exceeds its fair share
- Checked at "scheduling points" (syscall return, interrupt return)
[Intermediate]

Q: Where are the "scheduling points" in the kernel?
A:
1. Return from system call (ret_from_syscall)
2. Return from interrupt (ret_from_interrupt)
3. Return from exception
4. Explicit schedule() calls
5. Preemption points (if kernel preemption enabled)
At these points, TIF_NEED_RESCHED is checked.
[Intermediate]

Q: What is kernel preemption?
A: Ability to preempt a task even while it's executing in kernel mode (not just at syscall/interrupt boundaries). Enabled with CONFIG_PREEMPT. Improves latency but adds complexity (more locking needed).
[Intermediate]

Q: What is `preempt_count` in thread_info?
A: Counter tracking nested preemption-disable sections:
- > 0: preemption disabled (holding locks, in interrupt, etc.)
- 0: preemption allowed

Incremented by spin_lock, local_irq_disable, etc. Decremented on unlock/enable.
[Intermediate]

Q: [Code Interpretation] What does this code do?
```c
preempt_disable();
/* critical section */
preempt_enable();
```
A: Temporarily disables kernel preemption around critical section. Prevents context switch even if TIF_NEED_RESCHED is set, until preempt_enable() is called.
[Intermediate]

Q: What is the difference between voluntary and involuntary context switches?
A:
- **Voluntary**: Task chose to sleep/block (waiting for I/O, mutex, sleep(), etc.)
- **Involuntary**: Task was preempted by scheduler (higher priority task, time slice expired)

Tracked in task->nvcsw (voluntary) and task->nivcsw (involuntary).
[Basic]

Q: [ASCII Diagram] Show the context switch flow.
A:
```
Task A running                    Task B (waiting)
     |                                  
     | Timer/Event triggers            
     | TIF_NEED_RESCHED set            
     v                                 
+----------------+                     
| schedule()     |                     
| - pick_next = B|                     
| - context_switch(A,B)                
+----------------+                     
     |                                 
     | switch_mm() - load B's page tables
     |                                 
     | switch_to()                     
     | - Save A's registers to A's stack
     | - Switch to B's stack           
     | - Restore B's registers         
     |                                 
     v                                 
Task A (waiting)                  Task B running
                                       |
                                       v
```
[Intermediate]

Q: What happens to FPU/SSE state during context switch?
A: Can be handled two ways:
1. **Lazy switching**: Don't save/restore until FPU actually used (trap on first FPU instruction)
2. **Eager switching**: Always save/restore (simpler, often faster on modern CPUs)

v3.2 supports both; eager is default on newer configs.
[Advanced]

Q: Why is context switching expensive?
A:
1. **Direct costs**: Register save/restore, function call overhead
2. **Indirect costs** (often larger):
   - TLB flush (new address space)
   - Cache pollution (new task's data not in cache)
   - Branch predictor state lost
   - Pipeline flush
[Intermediate]

Q: [Reverse] This thread flag is checked at scheduling points to determine if schedule() should be called.
A: TIF_NEED_RESCHED
[Basic]

