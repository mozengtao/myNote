# Linux Kernel v3.2 Virtualization Mechanism Flashcards

> Comprehensive Anki-style flashcards covering namespaces, cgroups, KVM, and container building blocks in the Linux kernel.

---

## Section 1: Virtualization Fundamentals

---

Q: What is virtualization in the context of computing?
A: Virtualization is the abstraction of physical computing resources (CPU, memory, I/O, network) to create isolated execution environments. It allows multiple virtual systems to run on a single physical machine by presenting a virtualized view of hardware resources to each guest.
[Basic]

---

Q: What are the three main types of virtualization?
A: 1. **Full virtualization**: Complete hardware abstraction; guest OS runs unmodified (e.g., KVM, VMware)
2. **Paravirtualization**: Guest OS is modified to use hypervisor APIs for better performance (e.g., Xen PV)
3. **OS-level virtualization**: Kernel provides isolated user-space instances sharing the same kernel (e.g., containers via namespaces/cgroups)
[Basic]

---

Q: (Cloze) In full virtualization, the guest OS runs _____, while in paravirtualization, the guest OS is _____ to communicate with the hypervisor.
A: unmodified, modified (hypercall-aware)
[Basic]

---

Q: What is a hypervisor?
A: A hypervisor (also called Virtual Machine Monitor or VMM) is software that creates and manages virtual machines. It abstracts the physical hardware and allocates resources to guest VMs while maintaining isolation between them.
[Basic]

---

Q: What is the difference between Type 1 and Type 2 hypervisors?
A: **Type 1 (Bare-metal)**: Runs directly on hardware without a host OS. Examples: Xen, VMware ESXi, Microsoft Hyper-V.
**Type 2 (Hosted)**: Runs as an application on top of a host OS. Examples: VirtualBox, VMware Workstation.
KVM is a hybrid - Linux kernel becomes the hypervisor (Type 1 aspect) but runs as a full OS (Type 2 aspect).
[Basic]

---

Q: (ASCII Diagram) Draw the architecture difference between Type 1 and Type 2 hypervisors.
A:
```
TYPE 1 (Bare-metal)              TYPE 2 (Hosted)
+-------+-------+-------+        +-------+-------+
| VM 1  | VM 2  | VM 3  |        | VM 1  | VM 2  |
+-------+-------+-------+        +-------+-------+
|     HYPERVISOR        |        |   HYPERVISOR  |
+-----------------------+        +---------------+
|      HARDWARE         |        |    HOST OS    |
+-----------------------+        +---------------+
                                 |   HARDWARE    |
                                 +---------------+
```
[Basic]

---

Q: What are x86 privilege rings and how do they relate to virtualization?
A: x86 CPUs have 4 privilege levels (rings 0-3):
- Ring 0: Kernel mode (highest privilege)
- Ring 3: User mode (lowest privilege)
- Rings 1-2: Rarely used

Traditional problem: Guest OS expects to run in Ring 0, but hypervisor needs Ring 0. Solutions include binary translation, paravirtualization, or hardware-assisted virtualization.
[Intermediate]

---

Q: What is the "trap-and-emulate" virtualization technique?
A: When a guest executes a privileged instruction, it causes a trap (exception) to the hypervisor. The hypervisor then emulates the instruction's effect on behalf of the guest. This works well for architectures where all sensitive instructions are privileged (cause traps).
[Intermediate]

---

Q: Why was x86 historically difficult to virtualize?
A: x86 has "sensitive but non-privileged" instructions that behave differently in different privilege levels but don't trap. Examples: `POPF` (modifies flags differently), `SGDT/SIDT` (reveal real hardware). These required binary translation or hardware extensions to solve.
[Intermediate]

---

Q: What is Intel VT-x (VMX)?
A: Intel VT-x (Virtualization Technology for x86) adds hardware support for virtualization with:
- **VMX root mode**: Where hypervisor runs
- **VMX non-root mode**: Where guest runs
- **VMCS**: Control structure for VM state
- Automatic traps on sensitive operations (VM exits)
- `VMLAUNCH`/`VMRESUME` instructions to enter guest
[Intermediate]

---

Q: What is AMD-V (SVM)?
A: AMD-V (AMD Virtualization), also called SVM (Secure Virtual Machine), is AMD's hardware virtualization extension with:
- **Host mode** and **Guest mode**
- **VMCB** (Virtual Machine Control Block) instead of VMCS
- `VMRUN` instruction to enter guest
- Similar capabilities to Intel VT-x
[Intermediate]

---

Q: (Reverse) This hardware feature allows the CPU to trap sensitive guest operations automatically without binary translation.
A: Q: What is hardware-assisted virtualization (VT-x/AMD-V)?
[Basic]

---

Q: What is the difference between VMX root and non-root operation?
A: - **VMX root**: Normal CPU operation where the hypervisor (VMM) runs with full control
- **VMX non-root**: Restricted CPU operation where the guest VM runs; certain instructions and events cause automatic exits to VMX root (VM exits)

Both modes have all four privilege rings (0-3), so guest kernel runs in non-root Ring 0.
[Intermediate]

---

Q: What triggers a VM exit?
A: VM exits occur when guest execution must be handled by the hypervisor:
- Privileged instructions (e.g., `CPUID`, `HLT`, `INVLPG`)
- Access to control registers (CR0, CR3, CR4)
- I/O port access (IN/OUT)
- External interrupts (configured)
- Page faults (when using shadow paging)
- VMCALL (explicit hypercall)
[Intermediate]

---

Q: What is OS-level virtualization and how does it differ from hardware virtualization?
A: OS-level virtualization creates isolated user-space environments (containers) that share the host kernel:
- **No guest kernel**: All containers use host kernel
- **Lighter weight**: No hardware emulation overhead
- **Faster startup**: Milliseconds vs seconds
- **Less isolation**: Share kernel attack surface

Mechanisms in Linux: namespaces (resource isolation) + cgroups (resource limits).
[Basic]

---

Q: (Comparison) Compare the isolation boundaries of VMs vs containers.
A:
| Aspect | Virtual Machines | Containers |
|--------|-----------------|------------|
| Kernel | Separate guest kernel | Shared host kernel |
| Isolation | Hardware-enforced | Kernel-enforced |
| Overhead | Higher (full OS) | Lower (processes) |
| Boot time | Seconds-minutes | Milliseconds |
| Security | Stronger boundary | Weaker boundary |
| Syscall path | Through hypervisor | Direct to kernel |
[Intermediate]

---

Q: What is a hypercall?
A: A hypercall is the virtualization equivalent of a syscall. It's a software-initiated trap from guest to hypervisor for requesting services. In paravirtualization, the guest OS uses hypercalls instead of privileged instructions. In KVM, the `VMCALL` instruction triggers a hypercall VM exit.
[Intermediate]

---

Q: What is Extended Page Tables (EPT) / Nested Page Tables (NPT)?
A: EPT (Intel) / NPT (AMD) provide hardware-assisted memory virtualization:
- Two-level address translation: Guest Virtual → Guest Physical → Host Physical
- Hardware walks both page tables automatically
- Eliminates need for shadow page tables
- Reduces VM exit overhead for memory operations
[Advanced]

---

Q: (Understanding) Why do containers share the host kernel, and what are the security implications?
A: Containers share the host kernel because they use namespaces for isolation rather than hardware virtualization. Security implications:
- Kernel vulnerability affects all containers
- Syscall filtering (seccomp) needed to reduce attack surface
- Root in container may escalate if not properly configured
- User namespaces provide UID mapping but add complexity
[Intermediate]

---

## Section 2: Linux Namespaces - Core Concepts

---

Q: What are Linux namespaces?
A: Namespaces are a kernel feature that partitions kernel resources so that one set of processes sees one set of resources, while another set sees a different set. They provide the isolation foundation for containers by virtualizing global system resources (PIDs, network, mounts, etc.) per-namespace.
[Basic]

---

Q: What are the six namespace types available in Linux kernel v3.2?
A: 1. **Mount (CLONE_NEWNS)** - Isolates mount points (first namespace, 2002)
2. **UTS (CLONE_NEWUTS)** - Isolates hostname and domain name
3. **IPC (CLONE_NEWIPC)** - Isolates System V IPC and POSIX message queues
4. **PID (CLONE_NEWPID)** - Isolates process IDs
5. **Network (CLONE_NEWNET)** - Isolates network stack
6. **User (CLONE_NEWUSER)** - Isolates user and group IDs (partial in v3.2)
[Basic]

---

Q: (Cloze) The mount namespace was the first namespace added to Linux and uses the clone flag _____.
A: CLONE_NEWNS (not CLONE_NEWMNT, for historical reasons - it was the only namespace when created)
[Basic]

---

Q: What is the `struct nsproxy` and what does it contain?
A: `struct nsproxy` (in `include/linux/nsproxy.h`) holds pointers to all namespace objects for a task:
```c
struct nsproxy {
    atomic_t count;
    struct uts_namespace *uts_ns;
    struct ipc_namespace *ipc_ns;
    struct mnt_namespace *mnt_ns;
    struct pid_namespace *pid_ns;
    struct net *net_ns;
};
```
Multiple tasks can share the same nsproxy if they share all namespaces.
[Intermediate]

---

Q: How do tasks share or isolate namespaces?
A: Tasks point to namespaces through `task_struct->nsproxy`. When:
- **fork()**: Child inherits parent's nsproxy (shares namespaces)
- **clone(CLONE_NEW*)**: Child gets new namespace(s)
- **unshare()**: Current task leaves shared namespace, creates new one
- **setns()**: Current task joins an existing namespace
[Intermediate]

---

Q: What are the three main APIs for namespace manipulation?
A: 1. **clone(flags)**: Create new process in new namespace(s)
   - Pass `CLONE_NEWNS`, `CLONE_NEWPID`, etc.
2. **unshare(flags)**: Disassociate parts of current process's execution context
   - Current process gets new namespace(s)
3. **setns(fd, nstype)**: Join an existing namespace
   - fd from `/proc/[pid]/ns/*` files
[Basic]

---

Q: (Code Interpretation) What does this code accomplish?
```c
clone(child_fn, stack + STACK_SIZE,
      CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | SIGCHLD,
      NULL);
```
A: Creates a new child process that runs `child_fn` with:
- A new PID namespace (child sees itself as PID 1)
- A new mount namespace (isolated mount table)
- A new network namespace (empty network stack)
The child runs in a container-like isolated environment.
[Intermediate]

---

Q: What is `/proc/[pid]/ns/` directory?
A: This directory contains symbolic links representing the namespaces of process `[pid]`:
```
/proc/1234/ns/
├── ipc -> ipc:[4026531839]
├── mnt -> mnt:[4026531840]
├── net -> net:[4026531956]
├── pid -> pid:[4026531836]
├── user -> user:[4026531837]
└── uts -> uts:[4026531838]
```
The inode number in brackets uniquely identifies each namespace. These files can be opened and passed to `setns()`.
[Basic]

---

Q: How can you check if two processes are in the same namespace?
A: Compare the inode numbers of their `/proc/[pid]/ns/*` symlinks:
```bash
readlink /proc/1234/ns/net  # net:[4026531956]
readlink /proc/5678/ns/net  # net:[4026531956]
# Same inode = same namespace
```
Or in C, use `stat()` on the namespace file and compare `st_ino`.
[Basic]

---

Q: (Reverse) This system call allows a process to join an existing namespace by passing a file descriptor obtained from `/proc/[pid]/ns/*`.
A: Q: What is `setns()`?
[Basic]

---

Q: What happens when a namespace has no more processes?
A: By default, when all processes in a namespace exit, the namespace is destroyed and its resources are released. However, namespaces can be kept alive by:
- Keeping a file descriptor open to `/proc/[pid]/ns/*`
- Bind-mounting the namespace file to another location
[Intermediate]

---

Q: What is the `unshare()` system call and how does it differ from `clone()`?
A: `unshare()` changes the namespace of the calling process itself (no new process created):
```c
unshare(CLONE_NEWNS | CLONE_NEWPID);
```
Difference from `clone()`:
- `clone()`: Creates a new process in new namespace(s)
- `unshare()`: Moves current process to new namespace(s)

Note: After `unshare(CLONE_NEWPID)`, the calling process itself isn't in the new PID namespace - only its future children will be.
[Intermediate]

---

Q: (ASCII Diagram) Show the relationship between task_struct and namespaces.
A:
```
task_struct                     nsproxy
+-----------------+            +------------------+
| pid             |            | count            |
| comm            |            | uts_ns --------->| struct uts_namespace
| nsproxy --------|----------->| ipc_ns --------->| struct ipc_namespace
| ...             |            | mnt_ns --------->| struct mnt_namespace
+-----------------+            | pid_ns --------->| struct pid_namespace
                               | net_ns --------->| struct net
task_struct                    +------------------+
+-----------------+                   ^
| nsproxy --------|-------------------+
+-----------------+                   
  (shares same nsproxy = shares all namespaces)
```
[Intermediate]

---

Q: What is a namespace "init" process?
A: In a PID namespace, the first process (PID 1 within that namespace) is special:
- It becomes the parent of orphaned processes in that namespace
- It receives signals intended for the namespace
- If it exits, all processes in the namespace are killed
- It has similar responsibilities to the global init (PID 1)
[Intermediate]

---

Q: How are namespaces created internally in the kernel?
A: When `clone()` is called with `CLONE_NEW*` flags:
1. `copy_namespaces()` is called from `copy_process()`
2. For each `CLONE_NEW*` flag set:
   - New namespace structure is allocated
   - Initialized with copy/defaults from parent
   - Attached to new nsproxy
3. New nsproxy is attached to the child task
[Intermediate]

---

Q: (Cloze) The _____ namespace isolates the hostname and domain name, while the _____ namespace isolates System V IPC objects.
A: UTS, IPC
[Basic]

---

Q: What does UTS stand for in UTS namespace?
A: UTS stands for "UNIX Time-Sharing System" - a historical reference to UNIX. The UTS namespace isolates the `uname()` system call results:
- `nodename` (hostname)
- `domainname` (NIS domain name)

This allows each container to have its own hostname.
[Basic]

---

Q: What resources does the IPC namespace isolate?
A: The IPC namespace isolates:
- System V IPC objects:
  - Semaphore sets (`semget()`)
  - Message queues (`msgget()`)
  - Shared memory segments (`shmget()`)
- POSIX message queues (`mq_open()`)

Each IPC namespace has its own set of IPC identifiers and keys.
[Intermediate]

---

Q: (Understanding) Why does the PID namespace have a hierarchical structure?
A: PID namespaces form a tree because:
- A process in a child PID namespace is also visible (with different PID) in all ancestor namespaces
- Parent namespace can see and signal child namespace processes
- Child namespace cannot see parent namespace processes
- This enables the host to manage containers while containers remain isolated from each other
[Intermediate]

---

Q: What is the significance of PID 1 in a new PID namespace?
A: PID 1 in a new PID namespace (the "container init"):
- Is the first process created in that namespace
- Becomes the reaper for orphaned processes
- Receives SIGCHLD when children exit
- If it dies, the entire namespace is torn down (all processes killed with SIGKILL)
- Has signal handling restrictions (can't be killed by signals it doesn't handle)
[Intermediate]

---

Q: How does the network namespace affect process networking?
A: Each network namespace has its own:
- Network devices (interfaces)
- IP addresses
- Routing tables
- Firewall rules (iptables)
- `/proc/net` and `/sys/class/net`
- Port numbers (can bind to same port in different namespaces)
- Unix domain sockets (abstract namespace is per-network-ns)
[Basic]

---

Q: What is a veth (virtual ethernet) pair?
A: A veth pair is two virtual network interfaces connected like a pipe:
- Packets sent to one end appear on the other
- One end can be placed in a container's network namespace
- Other end stays in host namespace (often connected to a bridge)
- Primary mechanism for container network connectivity
```
Container NS          Host NS
+--------+           +--------+
| veth0  |<=========>| veth1  |---> bridge ---> physical NIC
+--------+           +--------+
```
[Intermediate]

---

Q: (Code Interpretation) What does this code sequence do?
```c
int fd = open("/proc/1234/ns/net", O_RDONLY);
setns(fd, CLONE_NEWNET);
close(fd);
```
A: This code makes the calling process join the network namespace of process 1234:
1. Opens the network namespace file of PID 1234
2. Calls `setns()` to switch to that namespace
3. Closes the file descriptor

After this, the process shares the same network stack as PID 1234.
[Intermediate]

---

Q: What is the user namespace and why is it significant?
A: The user namespace maps UIDs/GIDs between namespaces:
- Process can be root (UID 0) inside namespace but unprivileged outside
- Enables unprivileged container creation
- Provides capability isolation per-namespace

In v3.2, user namespace support was limited. Full support came in later kernels (3.8+).
[Advanced]

---

Q: (Comparison) What's the difference between namespace and cgroup?
A: 
| Aspect | Namespace | Cgroup |
|--------|-----------|--------|
| Purpose | Resource **isolation** (what you can see) | Resource **limiting** (how much you can use) |
| Example | Separate PID space | Limit to 50% CPU |
| Mechanism | Virtualize global resources | Account and limit usage |
| Together | Containers use both for full isolation |
[Basic]

---

## Section 3: PID and Mount Namespaces Deep Dive

---

Q: What is `struct pid_namespace` and where is it defined?
A: `struct pid_namespace` (in `include/linux/pid_namespace.h`) represents a PID namespace:
```c
struct pid_namespace {
    struct kref kref;
    struct pidmap pidmap[PIDMAP_ENTRIES];
    int last_pid;
    struct task_struct *child_reaper;
    struct kmem_cache *pid_cachep;
    unsigned int level;
    struct pid_namespace *parent;
    struct vfsmount *proc_mnt;
    struct bsd_acct_struct *bacct;
};
```
Key fields: `level` (depth in hierarchy), `parent` (parent namespace), `child_reaper` (init process).
[Intermediate]

---

Q: How are PIDs allocated in a PID namespace?
A: Each PID namespace has its own `pidmap` bitmap:
1. `alloc_pid()` searches for free bit in `pidmap`
2. PID is allocated at each level from current to root namespace
3. A process has different PIDs in each ancestor namespace
4. `last_pid` tracks where to start searching (wrap around)
[Intermediate]

---

Q: (ASCII Diagram) Show PID translation across nested PID namespaces.
A:
```
Root PID Namespace (level 0)
+----------------------------------------+
| Process A: PID=1234                    |
|   +-----------------------------------+|
|   | Child PID NS (level 1)            ||
|   | Process B: PID=1234:1 (host:child)||
|   |   +------------------------------+||
|   |   | Grandchild PID NS (level 2)  |||
|   |   | Process C: PID=1234:5:1      |||
|   |   +------------------------------+||
|   +-----------------------------------+|
+----------------------------------------+

Process C sees itself as PID 1
Parent NS sees Process C as PID 5
Root NS sees Process C as PID 1234
```
[Intermediate]

---

Q: What is the `child_reaper` in a PID namespace?
A: The `child_reaper` is the "init" process of the PID namespace:
- Set to the first process in the namespace (PID 1)
- Adopts orphaned processes when their parent exits
- Receives SIGCHLD for adopted children
- Its death triggers namespace teardown
- Accessed via `task_active_pid_ns(current)->child_reaper`
[Intermediate]

---

Q: How does `struct pid` support multiple namespaces?
A: `struct pid` (in `include/linux/pid.h`) has a flexible array for multi-namespace support:
```c
struct pid {
    atomic_t count;
    unsigned int level;
    struct hlist_head tasks[PIDTYPE_MAX];
    struct rcu_head rcu;
    struct upid numbers[1]; /* variable length */
};

struct upid {
    int nr;                    /* PID value */
    struct pid_namespace *ns;  /* namespace */
    struct hlist_node pid_chain;
};
```
One `upid` entry per namespace level (from root to current).
[Advanced]

---

Q: (Code Interpretation) What does `task_pid_nr_ns(task, ns)` return?
A: Returns the PID of `task` as seen in namespace `ns`:
- If task is not visible in `ns`, returns 0
- Walks the `struct pid->numbers[]` array to find matching namespace
- Used to translate PIDs across namespace boundaries

Example: A container's init (PID 1 inside) might be PID 5432 in the host namespace.
[Intermediate]

---

Q: What happens when the init process (PID 1) of a PID namespace exits?
A: When the namespace init exits:
1. `zap_pid_ns_processes()` is called
2. All processes in that PID namespace receive SIGKILL
3. The namespace waits for all processes to exit
4. Namespace resources are freed
5. Child PID namespaces are also terminated (cascade)
[Intermediate]

---

Q: Can a process in a child PID namespace see processes in the parent namespace?
A: No. PID namespaces are hierarchical and visibility is one-way:
- Parent can see and signal child namespace processes
- Child cannot see parent namespace processes
- Child namespace processes don't know they're in a "nested" namespace
- This provides isolation from the container's perspective
[Basic]

---

Q: What is `struct mnt_namespace`?
A: `struct mnt_namespace` (in `fs/mount.h` or `include/linux/mnt_namespace.h`) represents a mount namespace:
```c
struct mnt_namespace {
    atomic_t count;
    struct vfsmount *root;      /* root mount */
    struct list_head list;      /* list of mounts */
    wait_queue_head_t poll;
    int event;
};
```
Contains the root mount and list of all mounts in this namespace.
[Intermediate]

---

Q: How is a mount namespace created?
A: When `clone(CLONE_NEWNS)` or `unshare(CLONE_NEWNS)` is called:
1. `copy_mnt_ns()` allocates new `struct mnt_namespace`
2. `copy_tree()` duplicates the entire mount tree
3. Each mount point gets a new `struct vfsmount`
4. New namespace is attached to task's nsproxy
5. Future mounts in either namespace are independent
[Intermediate]

---

Q: What is mount propagation?
A: Mount propagation defines how mount/unmount events spread between mount namespaces. Four propagation types:
- **shared**: Events propagate bidirectionally
- **private**: Events don't propagate (default for new mounts)
- **slave**: Receives events from master, doesn't send
- **unbindable**: Private + can't be bind mounted
[Intermediate]

---

Q: (Cloze) Mount propagation type _____ receives mount events from its master but doesn't propagate its own events back.
A: slave
[Intermediate]

---

Q: What is a "peer group" in mount propagation?
A: A peer group is a set of mounts that share propagation events:
- Created when mounts are marked as "shared"
- Members propagate mount/unmount to each other
- Bind mounting a shared mount joins the peer group
- Listed in `/proc/[pid]/mountinfo` as "shared:N" (N = peer group ID)
[Advanced]

---

Q: How do you change mount propagation type?
A: Use the `mount` command or `mount()` syscall with specific flags:
```bash
mount --make-private /mnt    # MS_PRIVATE
mount --make-shared /mnt     # MS_SHARED  
mount --make-slave /mnt      # MS_SLAVE
mount --make-unbindable /mnt # MS_UNBINDABLE
```
Can also use `--make-rprivate` etc. for recursive operation on subtree.
[Intermediate]

---

Q: (ASCII Diagram) Show mount propagation between namespaces.
A:
```
Mount NS A (shared)          Mount NS B (slave of A)
+------------------+         +------------------+
| /mnt (shared:1)  |<------->| /mnt (slave)     |
+------------------+    |    +------------------+
        |               |
        v               |    mount /dev/sdb /mnt/data in NS A
+------------------+    |    +------------------+
| /mnt/data        |----+--->| /mnt/data        |  (propagates)
+------------------+         +------------------+

mount /dev/sdc /mnt/other in NS B
+------------------+         +------------------+
| (no change)      |         | /mnt/other       |  (doesn't propagate back)
+------------------+         +------------------+
```
[Advanced]

---

Q: Why does each PID namespace need its own `/proc` mount?
A: `/proc` exposes PIDs and process information:
- Different PID namespaces have different PID values
- Each namespace needs `/proc` showing its own PIDs
- `pid_namespace->proc_mnt` stores namespace-specific `/proc`
- Mounting `/proc` in a container shows only that namespace's processes
[Intermediate]

---

Q: (Reverse) This field in `struct pid_namespace` points to the process that adopts orphans in the namespace.
A: Q: What is `child_reaper`?
[Intermediate]

---

Q: What is a "pivot_root" and why is it used with mount namespaces?
A: `pivot_root(new_root, put_old)` changes the root filesystem:
- Moves current root to `put_old` directory
- Makes `new_root` the new root
- More secure than `chroot` for containers
- Used to replace the entire filesystem view
- Requires mount namespace isolation to not affect host
[Intermediate]

---

Q: How does `pivot_root` differ from `chroot`?
A: 
| Aspect | chroot | pivot_root |
|--------|--------|------------|
| Scope | Changes process root dir | Swaps entire root mount |
| Escape | Easier to escape (relative paths) | Harder to escape |
| Old root | Still accessible via `..` tricks | Moved to specified location, can be unmounted |
| Use case | Development/testing | Container isolation |
[Intermediate]

---

Q: What is `/proc/[pid]/mountinfo` and what does it show?
A: `/proc/[pid]/mountinfo` shows the mount table for a process's mount namespace:
```
36 35 98:0 /mnt1 /mnt2 rw shared:1 - ext3 /dev/sda1 rw
mount_id parent_id major:minor root mount_point options propagation - fstype source super_options
```
Key fields include mount ID, propagation info (shared/master/unbindable), and filesystem details.
[Intermediate]

---

Q: (Code Interpretation) What does this code accomplish?
```c
unshare(CLONE_NEWNS);
mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL);
mount("/newroot", "/newroot", NULL, MS_BIND, NULL);
pivot_root("/newroot", "/newroot/oldroot");
umount2("/oldroot", MNT_DETACH);
```
A: Creates an isolated container-like filesystem:
1. Creates new mount namespace
2. Makes entire mount tree private (no propagation)
3. Bind-mounts the new root to itself (required for pivot_root)
4. Swaps root filesystem from `/` to `/newroot`
5. Detaches old root, leaving only new filesystem visible
[Advanced]

---

Q: What is the relationship between mount namespace and `chroot` jail?
A: Mount namespace provides stronger isolation than chroot:
- `chroot`: Only changes root directory reference; process can escape
- Mount namespace: Complete mount table isolation
- Combined: `pivot_root` in mount namespace provides robust root isolation
- Containers use mount namespace + pivot_root, not just chroot
[Intermediate]

---

Q: (Understanding) Why can't you create a new PID namespace and immediately see yourself as PID 1?
A: When `unshare(CLONE_NEWPID)` is called:
- The calling process remains in its original PID namespace
- Only the calling process's **children** will be in the new namespace
- The first child becomes PID 1 in the new namespace
- This is because a process's PID is assigned at creation and cannot change

Use `clone(CLONE_NEWPID)` to create a new process that starts in the new namespace.
[Intermediate]

---

Q: How are orphaned processes handled in a PID namespace?
A: When a parent process exits:
1. Children become orphans
2. `forget_original_parent()` is called
3. Orphans are reparented to `child_reaper` of their PID namespace
4. Not the global init (PID 1), but the namespace's init
5. Namespace init receives SIGCHLD and must `wait()` on them
[Intermediate]

---

## Section 4: Network and IPC Namespaces

---

Q: What is `struct net` and what does it represent?
A: `struct net` (in `include/net/net_namespace.h`) represents a network namespace:
```c
struct net {
    atomic_t count;
    struct list_head list;
    struct list_head cleanup_list;
    struct user_namespace *user_ns;
    struct proc_dir_entry *proc_net;
    struct net_device *loopback_dev;
    struct hlist_head *dev_name_head;
    struct hlist_head *dev_index_head;
    /* Protocol-specific: */
    struct netns_ipv4 ipv4;
    struct netns_ipv6 ipv6;
    struct net_generic *gen;
    /* ... */
};
```
Contains all network state including devices, routing, and protocol data.
[Intermediate]

---

Q: What resources are isolated by a network namespace?
A: A network namespace isolates:
- Network devices (interfaces)
- IPv4/IPv6 protocol stacks
- IP routing tables
- Firewall rules (iptables/netfilter)
- Network port numbers
- `/proc/net` and `/sys/class/net`
- Unix domain sockets (abstract namespace)
- Network-related sysctl parameters
[Basic]

---

Q: What is the initial network namespace?
A: The initial (default) network namespace is `init_net`:
- Created at boot time
- Contains all physical network interfaces initially
- Defined as `struct net init_net` in `net/core/net_namespace.c`
- All processes start in `init_net` unless explicitly moved
- Physical NICs can be moved to other namespaces
[Intermediate]

---

Q: How do you create a new network namespace?
A: Several methods:
1. **clone()**: `clone(CLONE_NEWNET, ...)`
2. **unshare()**: `unshare(CLONE_NEWNET)`
3. **ip command**: `ip netns add <name>`
4. **setns()**: Join existing namespace via `/proc/[pid]/ns/net`

A new network namespace starts empty (only loopback interface, unconfigured).
[Basic]

---

Q: (Cloze) A newly created network namespace contains only the _____ interface, which must be explicitly brought up.
A: loopback (lo)
[Basic]

---

Q: What is a veth pair and how is it created?
A: A veth (virtual ethernet) pair is two connected virtual interfaces:
```bash
ip link add veth0 type veth peer name veth1
```
- Packets sent to `veth0` emerge from `veth1` and vice versa
- One end can be moved to a different namespace
- Primary mechanism for container networking
- Acts like a virtual patch cable
[Basic]

---

Q: (ASCII Diagram) Show how veth pairs connect a container to the host network.
A:
```
Container Network NS              Host Network NS
+---------------------+          +-------------------------+
|                     |          |                         |
|  +-----------+      |          |  +-----------+          |
|  | eth0      |      |          |  | veth-abc  |          |
|  | 10.0.0.2  |<=============>  |  | (bridge)  |          |
|  +-----------+      |          |  +-----+-----+          |
|       |             |          |        |                |
|   Container         |          |  +-----+-----+   +----+ |
|   processes         |          |  |   br0     |---|eth0|-|-> Internet
|                     |          |  | (bridge)  |   +----+ |
+---------------------+          |  +-----------+          |
                                 +-------------------------+

veth pair connects container eth0 to host bridge
```
[Intermediate]

---

Q: How do you move a network interface to a different namespace?
A: Use `ip link set`:
```bash
# Move veth1 to PID 1234's network namespace
ip link set veth1 netns 1234

# Or move to named namespace
ip link set veth1 netns mycontainer
```
Kernel function: `dev_change_net_namespace()` handles the move.
[Basic]

---

Q: What happens to socket connections when moving to a new network namespace?
A: Existing sockets are tied to their creation namespace:
- Open sockets stay in original namespace
- Cannot be moved to new namespace
- `unshare(CLONE_NEWNET)` doesn't affect existing sockets
- New sockets after `unshare()` use new namespace
- This is why containers typically fork new processes
[Intermediate]

---

Q: (Reverse) This kernel structure contains all network state for a namespace including `loopback_dev`, routing tables, and protocol-specific data.
A: Q: What is `struct net`?
[Intermediate]

---

Q: How does the kernel track which network namespace a socket belongs to?
A: Each socket has a reference to its network namespace:
```c
struct sock {
    /* ... */
    struct net *sk_net;  /* Network namespace */
    /* ... */
};
```
`sock_net(sk)` macro returns the namespace. All network operations use this to determine context.
[Intermediate]

---

Q: What is `dev_net(dev)` and when is it used?
A: `dev_net(dev)` returns the network namespace owning a network device:
```c
static inline struct net *dev_net(const struct net_device *dev)
{
    return read_pnet(&dev->nd_net);
}
```
Used whenever kernel code needs to know which namespace a device belongs to for routing, filtering, etc.
[Intermediate]

---

Q: How are routing tables isolated per network namespace?
A: Each `struct net` has its own routing data:
- IPv4: `net->ipv4.fib_table_hash` (FIB tables)
- IPv6: `net->ipv6.fib6_main_tbl`
- Route lookups use `sock_net(sk)` or `dev_net(dev)` to find correct tables
- `ip route` commands operate on current namespace
[Intermediate]

---

Q: What is the "abstract" Unix domain socket namespace?
A: Abstract Unix sockets (name starting with '\0') are namespace-isolated:
- Per-network-namespace in kernel v3.2+
- Two containers can bind same abstract socket name
- Pathname-based Unix sockets are filesystem-based (mount namespace)
```c
addr.sun_path[0] = '\0';  // Abstract socket
strcpy(&addr.sun_path[1], "my_socket");
```
[Intermediate]

---

Q: What is `struct ipc_namespace`?
A: `struct ipc_namespace` (in `include/linux/ipc_namespace.h`) holds IPC namespace state:
```c
struct ipc_namespace {
    atomic_t count;
    struct ipc_ids ids[3];  /* sem, msg, shm */
    int sem_ctls[4];
    int msg_ctlmax;
    int msg_ctlmnb;
    int msg_ctlmni;
    size_t shm_ctlmax;
    size_t shm_ctlall;
    int shm_ctlmni;
    struct mq_attr mq_attr;
    /* ... */
};
```
Contains IPC limits and identifier tables for semaphores, messages, and shared memory.
[Intermediate]

---

Q: What IPC mechanisms are isolated by IPC namespace?
A: The IPC namespace isolates:
- **System V Semaphores**: `semget()`, `semop()`, `semctl()`
- **System V Message Queues**: `msgget()`, `msgsnd()`, `msgrcv()`
- **System V Shared Memory**: `shmget()`, `shmat()`, `shmdt()`
- **POSIX Message Queues**: `mq_open()`, `mq_send()`, `mq_receive()`

Each namespace has independent IPC identifier spaces and limits.
[Basic]

---

Q: (Cloze) System V IPC uses the _____ namespace for isolation, while filesystem-based IPC (like named pipes) uses the _____ namespace.
A: IPC, mount
[Intermediate]

---

Q: How are System V IPC identifiers managed per namespace?
A: Each IPC namespace has `struct ipc_ids ids[3]`:
```c
struct ipc_ids {
    int in_use;              /* # of allocated IPC objects */
    unsigned short seq;      /* seq counter for ipc_seq */
    struct rw_semaphore rw_mutex;
    struct idr ipcs_idr;     /* IDR tree of IPC objects */
};
```
- `ids[0]`: Semaphores
- `ids[1]`: Message queues  
- `ids[2]`: Shared memory
IPC keys are namespace-local.
[Intermediate]

---

Q: What happens to IPC objects when a namespace is destroyed?
A: When an IPC namespace is destroyed:
1. All semaphore sets are removed (`sem_exit_ns()`)
2. All message queues are removed (`msg_exit_ns()`)
3. All shared memory segments are marked for destruction (`shm_exit_ns()`)
4. Processes using these objects get errors on next operation
[Intermediate]

---

Q: (Code Interpretation) What does this code demonstrate?
```c
key_t key = ftok("/tmp/myfile", 'A');
int shmid = shmget(key, 4096, IPC_CREAT | 0666);
void *ptr = shmat(shmid, NULL, 0);
```
A: Creates and attaches System V shared memory:
1. Generates IPC key from file path and ID
2. Creates/opens shared memory segment (4KB)
3. Attaches segment to process address space

This shared memory is isolated to the process's IPC namespace - processes in other IPC namespaces cannot access it even with the same key.
[Intermediate]

---

Q: How do POSIX message queues relate to IPC namespaces?
A: POSIX message queues (`mq_*` functions) are also per-IPC-namespace:
- Each namespace has its own `/dev/mqueue` virtual filesystem
- Queue names are namespace-local
- Implemented in `ipc/mqueue.c`
- `mq_attr` limits stored in `struct ipc_namespace`
[Intermediate]

---

Q: (Understanding) Can two processes in different IPC namespaces communicate via System V shared memory?
A: No, System V shared memory is per-IPC-namespace:
- Same key generates different internal identifiers
- `shmget()` only sees segments in current namespace
- No cross-namespace access for System V IPC

For cross-namespace communication, use:
- Files (if mount namespace is shared/mapped)
- Network sockets
- `memfd_create()` with fd passing
[Intermediate]

---

Q: What is the difference between IPC namespace isolation of System V vs POSIX shared memory?
A: 
| Type | Namespace | Implementation |
|------|-----------|----------------|
| System V shm | IPC namespace | `shmget()`/`shmat()` |
| POSIX shm (`shm_open`) | Mount namespace | Files in `/dev/shm` tmpfs |
| POSIX mqueue | IPC namespace | Files in `/dev/mqueue` |

POSIX shared memory is filesystem-based, so it follows mount namespace!
[Advanced]

---

Q: How do you check what IPC namespace a process belongs to?
A: Check the namespace inode:
```bash
# Via /proc filesystem
ls -la /proc/[pid]/ns/ipc
# Output: ipc -> ipc:[4026531839]

# Compare two processes
readlink /proc/1/ns/ipc
readlink /proc/$$/ns/ipc
# Same number = same namespace
```
[Basic]

---

Q: (ASCII Diagram) Show IPC namespace isolation between containers.
A:
```
Container A (IPC NS 1)           Container B (IPC NS 2)
+------------------------+       +------------------------+
| shmget(key=1234)       |       | shmget(key=1234)       |
| Returns shmid=0        |       | Returns shmid=0        |
|                        |       |                        |
| +------------------+   |       | +------------------+   |
| | Shared Mem Seg A |   |       | | Shared Mem Seg B |   |
| +------------------+   |       | +------------------+   |
|                        |       |                        |
| msgget(key=5678)       |       | msgget(key=5678)       |
| Returns msqid=0        |       | Returns msqid=0        |
+------------------------+       +------------------------+
        |                                |
        |  Same keys, different objects  |
        +--------------------------------+

Host cannot see container IPC objects unless in same namespace
```
[Intermediate]

---

Q: What is `init_ipc_ns`?
A: `init_ipc_ns` is the initial/default IPC namespace:
- Created at boot time
- All processes start in `init_ipc_ns`
- Defined in `ipc/util.c`
- Contains default System V IPC limits
- Processes inherit parent's IPC namespace unless `CLONE_NEWIPC` used
[Intermediate]

---

## Section 5: Control Groups (cgroups) - Fundamentals

---

Q: What are control groups (cgroups)?
A: Cgroups are a Linux kernel feature for organizing processes into hierarchical groups and applying resource limits, accounting, and control. They provide:
- **Resource limiting**: Cap CPU, memory, I/O usage
- **Prioritization**: Set relative resource priorities
- **Accounting**: Track resource usage per group
- **Control**: Freeze/restart groups of processes
[Basic]

---

Q: What are the four main functions of cgroups?
A: 1. **Resource limiting**: Set hard limits on resource usage (e.g., memory limit)
2. **Prioritization**: Control relative resource allocation (e.g., CPU shares)
3. **Accounting**: Track resource consumption for billing/monitoring
4. **Control**: Manage process groups (freeze, checkpoint/restore)
[Basic]

---

Q: What is a cgroup subsystem (controller)?
A: A subsystem (or controller) is a module that controls a specific type of resource:
- `cpu`: CPU time allocation
- `cpuset`: CPU and memory node assignment
- `memory`: Memory limits and accounting
- `blkio`: Block I/O limits
- `devices`: Device access control
- `freezer`: Process freezing
- `net_cls`, `net_prio`: Network classification/priority

Each subsystem can be mounted independently or together.
[Basic]

---

Q: (Cloze) In cgroups, a _____ is a grouping of processes, while a _____ (or controller) manages a specific type of resource.
A: cgroup, subsystem
[Basic]

---

Q: What is the cgroup filesystem (cgroupfs)?
A: Cgroupfs is a virtual filesystem that exposes cgroup hierarchy:
- Mounted at `/sys/fs/cgroup` (or `/cgroup` on older systems)
- Directories represent cgroups
- Files represent control parameters
- Creating a directory creates a cgroup
- Writing PIDs to `tasks` file assigns processes
[Basic]

---

Q: (ASCII Diagram) Show the cgroup filesystem structure.
A:
```
/sys/fs/cgroup/
├── cpu/                      # CPU subsystem
│   ├── tasks                 # PIDs in root cgroup
│   ├── cpu.shares            # CPU weight (1024 default)
│   ├── cpu.cfs_quota_us      # Bandwidth limit
│   └── mycontainer/          # Child cgroup
│       ├── tasks
│       ├── cpu.shares
│       └── cpu.cfs_quota_us
├── memory/                   # Memory subsystem
│   ├── tasks
│   ├── memory.limit_in_bytes
│   └── mycontainer/
│       ├── tasks
│       └── memory.limit_in_bytes
└── cpuset/                   # CPUset subsystem
    └── ...
```
[Basic]

---

Q: What is `struct cgroup`?
A: `struct cgroup` (in `include/linux/cgroup.h`) represents a control group:
```c
struct cgroup {
    unsigned long flags;
    atomic_t count;
    struct list_head sibling;    /* sibling in parent's children */
    struct list_head children;   /* child cgroups */
    struct cgroup *parent;
    struct dentry *dentry;       /* cgroupfs directory */
    struct cgroup_subsys_state *subsys[CGROUP_SUBSYS_COUNT];
    struct cgroupfs_root *root;
    struct cgroup *top_cgroup;
    struct list_head css_sets;   /* list of css_set links */
    /* ... */
};
```
[Intermediate]

---

Q: What is `struct cgroup_subsys_state` (css)?
A: CSS represents a cgroup's state for one subsystem:
```c
struct cgroup_subsys_state {
    struct cgroup *cgroup;       /* containing cgroup */
    atomic_t refcnt;
    unsigned long flags;
    struct cgroup_subsys_state *parent;
    /* Subsystem-specific data follows in derived struct */
};
```
For example, `struct mem_cgroup` embeds a CSS for the memory controller.
[Intermediate]

---

Q: What is `struct css_set`?
A: `struct css_set` is a collection of subsystem states for a task:
```c
struct css_set {
    atomic_t refcount;
    struct hlist_node hlist;     /* hash table linkage */
    struct list_head tasks;      /* tasks using this css_set */
    struct list_head cg_links;   /* links to cgroups */
    struct cgroup_subsys_state *subsys[CGROUP_SUBSYS_COUNT];
    struct rcu_head rcu_head;
};
```
Tasks point to css_set, which points to CSS for each subsystem. Multiple tasks can share a css_set if they're in the same cgroups.
[Intermediate]

---

Q: (ASCII Diagram) Show the relationship between task, css_set, and cgroups.
A:
```
task_struct          css_set              cgroup_subsys_state    cgroup
+----------+        +-------------+       +----------------+    +--------+
| css_set -|------->| subsys[cpu] |------>| css (cpu)      |--->| cg_cpu |
+----------+        | subsys[mem] |--+    +----------------+    +--------+
                    | subsys[...] |  |
task_struct         | tasks list  |  |    +----------------+    +--------+
+----------+        +-------------+  +--->| css (memory)   |--->| cg_mem |
| css_set -|-------------^              +----------------+    +--------+
+----------+                            
(same css_set = same cgroups)           Each css links to its cgroup
```
[Intermediate]

---

Q: How do you assign a process to a cgroup?
A: Write the PID to the `tasks` file in the cgroup directory:
```bash
# Create cgroup
mkdir /sys/fs/cgroup/cpu/mygroup

# Add process
echo $$ > /sys/fs/cgroup/cpu/mygroup/tasks

# Or add by PID
echo 1234 > /sys/fs/cgroup/cpu/mygroup/tasks
```
Kernel function: `cgroup_attach_task()` handles the move.
[Basic]

---

Q: What is the difference between `tasks` and `cgroup.procs` files?
A: Both assign processes to cgroups:
- **tasks**: Operates on individual threads (TIDs)
- **cgroup.procs**: Operates on thread groups (PIDs)
  - Writing a PID moves all threads of that process

In v3.2, `cgroup.procs` moves the entire thread group atomically.
[Intermediate]

---

Q: What is a cgroup hierarchy?
A: A hierarchy is a tree of cgroups for one or more subsystems:
- Created by mounting cgroupfs with specific subsystems
- Child cgroups inherit parent's limits (can be more restrictive)
- A subsystem can only be in one hierarchy at a time
- Multiple hierarchies can exist with different subsystems
[Intermediate]

---

Q: (Code Interpretation) What does this shell sequence accomplish?
```bash
mount -t cgroup -o cpu,cpuacct none /sys/fs/cgroup/cpu
mkdir /sys/fs/cgroup/cpu/limited
echo 50000 > /sys/fs/cgroup/cpu/limited/cpu.cfs_quota_us
echo 100000 > /sys/fs/cgroup/cpu/limited/cpu.cfs_period_us
echo $$ > /sys/fs/cgroup/cpu/limited/tasks
```
A: 1. Mounts cpu and cpuacct controllers together
2. Creates a "limited" cgroup
3. Sets CPU quota: 50ms per 100ms period (50% CPU limit)
4. Adds current shell to the limited cgroup

Result: Shell and its children limited to 50% of one CPU.
[Intermediate]

---

Q: What is `struct cgroup_subsys`?
A: `struct cgroup_subsys` defines a cgroup controller:
```c
struct cgroup_subsys {
    struct cgroup_subsys_state *(*create)(struct cgroup *cgrp);
    void (*destroy)(struct cgroup *cgrp);
    int (*can_attach)(struct cgroup *cgrp, struct cgroup_taskset *tset);
    void (*attach)(struct cgroup *cgrp, struct cgroup_taskset *tset);
    void (*fork)(struct task_struct *task);
    void (*exit)(struct task_struct *task);
    const char *name;
    int subsys_id;
    struct cgroup_root *root;
    struct cftype *base_cftypes;
    /* ... */
};
```
Each subsystem registers callbacks for cgroup operations.
[Intermediate]

---

Q: How does a task inherit cgroup membership?
A: On `fork()`:
1. Child inherits parent's `css_set` (reference count incremented)
2. Same cgroup membership as parent
3. Subsystem `fork()` callbacks are invoked
4. Child can later be moved to different cgroups

`copy_cgroups()` in `kernel/cgroup.c` handles this.
[Intermediate]

---

Q: What is the root cgroup?
A: The root cgroup is the top of each hierarchy:
- Created when cgroupfs is mounted
- All processes start in the root cgroup
- Contains default values for control files
- Cannot be removed
- Represented by the mount point directory
[Basic]

---

Q: (Reverse) This data structure links a task to all its cgroup subsystem states, allowing multiple tasks in the same cgroups to share it.
A: Q: What is `struct css_set`?
[Intermediate]

---

Q: What are `cftype` structures?
A: `struct cftype` defines control files in cgroupfs:
```c
struct cftype {
    char name[MAX_CFTYPE_NAME];
    mode_t mode;
    size_t max_write_len;
    int (*read_u64)(struct cgroup *cgrp, struct cftype *cft);
    int (*write_u64)(struct cgroup *cgrp, struct cftype *cft, u64 val);
    int (*read_seq_string)(struct cgroup *cgrp, struct cftype *cft,
                           struct seq_file *m);
    /* ... */
};
```
Each subsystem defines its own cftypes (e.g., `memory.limit_in_bytes`).
[Intermediate]

---

Q: How do you check which cgroups a process belongs to?
A: Read `/proc/[pid]/cgroup`:
```bash
cat /proc/self/cgroup
# Output:
# 3:cpu,cpuacct:/user/mycontainer
# 2:memory:/user/mycontainer
# 1:devices:/
```
Format: `hierarchy-ID:controller-list:cgroup-path`
[Basic]

---

Q: What is the `notify_on_release` mechanism?
A: When a cgroup's last task exits:
1. If `notify_on_release` is 1, kernel executes a user-space program
2. Program path is in `release_agent` at hierarchy root
3. Cgroup path is passed as argument
4. Used for automatic cleanup of empty cgroups

```bash
echo 1 > /sys/fs/cgroup/cpu/mygroup/notify_on_release
echo /usr/bin/cleanup.sh > /sys/fs/cgroup/cpu/release_agent
```
[Intermediate]

---

Q: (Understanding) Why can a subsystem only be attached to one hierarchy at a time?
A: Single-hierarchy-per-subsystem rule exists because:
- A task can only have one state per subsystem
- Multiple hierarchies would create ambiguous ownership
- Resource accounting must be unambiguous
- Exception: Co-mounting puts multiple subsystems in same hierarchy

In cgroups v2, all controllers share a single unified hierarchy.
[Advanced]

---

Q: What is co-mounting in cgroups?
A: Co-mounting puts multiple subsystems in the same hierarchy:
```bash
mount -t cgroup -o cpu,cpuacct none /sys/fs/cgroup/cpu,cpuacct
```
- Both controllers see the same cgroup tree
- Useful for related controllers (cpu + cpuacct)
- Task membership is synchronized
- Common pattern in container runtimes
[Intermediate]

---

Q: What is the difference between cgroups v1 and v2?
A: 
| Aspect | cgroups v1 | cgroups v2 |
|--------|------------|------------|
| Hierarchies | Multiple (per controller) | Single unified |
| Mounting | Mount each controller separately | Mount "cgroup2" |
| Thread control | `tasks` file for threads | Process-level only |
| Internal nodes | Can have tasks | Internal nodes have no tasks |
| Interface | Various controller-specific files | Standardized interface |

Linux 3.2 uses v1; v2 development started around 3.16.
[Intermediate]

---

Q: What kernel config options enable cgroups?
A: Key config options:
- `CONFIG_CGROUPS`: Core cgroup support
- `CONFIG_CGROUP_CPUACCT`: CPU accounting
- `CONFIG_CGROUP_SCHED`: CPU scheduler control
- `CONFIG_MEMCG`: Memory controller
- `CONFIG_BLK_CGROUP`: Block I/O controller
- `CONFIG_CGROUP_FREEZER`: Freezer controller
- `CONFIG_CGROUP_DEVICE`: Device whitelist controller
[Basic]

---

Q: (Cloze) The _____ file in a cgroup moves all threads of a process atomically, while the _____ file operates on individual threads.
A: cgroup.procs, tasks
[Intermediate]

---

Q: How does the kernel find a task's cgroup for a specific subsystem?
A: Via `task_css()` macro:
```c
static inline struct cgroup_subsys_state *
task_css(struct task_struct *task, int subsys_id)
{
    return rcu_dereference(task->cgroups->subsys[subsys_id]);
}
```
Path: `task->cgroups` (css_set) → `subsys[id]` (css) → cast to specific struct.
[Advanced]

---

Q: What happens when you delete a cgroup directory?
A: Deleting (rmdir) a cgroup:
1. Only succeeds if cgroup has no tasks and no child cgroups
2. `cgroup_destroy_locked()` is called
3. Subsystem `destroy()` callbacks are invoked
4. CSS structures are freed
5. Directory is removed from cgroupfs
[Intermediate]

---

## Section 6: CPU and Memory Controllers

---

Q: What is the CPU controller (cpu cgroup)?
A: The CPU controller limits and allocates CPU time to cgroups:
- Uses CFS (Completely Fair Scheduler) bandwidth control
- Two modes: **shares** (relative weight) and **quota** (hard limit)
- Implemented in `kernel/sched/core.c` and `kernel/sched/fair.c`
- Controller name: `cpu`
[Basic]

---

Q: What is `cpu.shares` and how does it work?
A: `cpu.shares` sets the relative CPU time allocation:
- Default value: 1024
- Higher value = more CPU time relative to siblings
- Only matters under contention
- Example: Two cgroups with shares 1024 and 512 get 2:1 CPU ratio when competing
```bash
echo 2048 > /sys/fs/cgroup/cpu/mygroup/cpu.shares
```
[Basic]

---

Q: What is CFS bandwidth control?
A: CFS bandwidth control provides hard CPU limits using quota and period:
- `cpu.cfs_period_us`: Time period (default 100ms = 100000us)
- `cpu.cfs_quota_us`: Max CPU time allowed per period
- Ratio determines CPU limit: quota/period

Example: quota=50000, period=100000 = 50% of one CPU
[Intermediate]

---

Q: (Cloze) CFS bandwidth control uses _____ to set the time window and _____ to set the maximum CPU time allowed in that window.
A: cpu.cfs_period_us, cpu.cfs_quota_us
[Basic]

---

Q: How do you limit a cgroup to 2 CPUs worth of time?
A: Set quota to 2x the period:
```bash
# 200ms quota per 100ms period = 2 CPUs
echo 200000 > /sys/fs/cgroup/cpu/mygroup/cpu.cfs_quota_us
echo 100000 > /sys/fs/cgroup/cpu/mygroup/cpu.cfs_period_us
```
Or keep defaults and set quota to 200000 (2x default period of 100000).
[Intermediate]

---

Q: What is `cpu.stat` and what does it show?
A: `cpu.stat` reports CPU usage statistics:
```
nr_periods 12345        # Number of enforcement periods
nr_throttled 100        # Times group was throttled
throttled_time 50000000 # Total throttle time (nanoseconds)
```
High `nr_throttled` indicates the group is hitting its CPU limit.
[Intermediate]

---

Q: What is the cpuset controller?
A: The cpuset controller assigns CPUs and memory nodes to cgroups:
- `cpuset.cpus`: Which CPUs tasks can run on
- `cpuset.mems`: Which NUMA memory nodes to use
- Provides isolation, not limiting
- Useful for NUMA optimization and partitioning

```bash
echo 0-3 > /sys/fs/cgroup/cpuset/mygroup/cpuset.cpus
echo 0 > /sys/fs/cgroup/cpuset/mygroup/cpuset.mems
```
[Intermediate]

---

Q: (Comparison) What's the difference between cpu and cpuset controllers?
A:
| Aspect | cpu controller | cpuset controller |
|--------|---------------|-------------------|
| Purpose | Limit CPU time | Restrict which CPUs |
| Mechanism | Bandwidth throttling | CPU affinity |
| Limiting | Yes (quota) | No (just pinning) |
| NUMA | No | Yes (memory nodes) |
| Isolation | Time-based | Physical CPU-based |
[Intermediate]

---

Q: What is the cpuacct controller?
A: The cpuacct (CPU accounting) controller tracks CPU usage without limits:
- `cpuacct.usage`: Total CPU time used (nanoseconds)
- `cpuacct.stat`: User/system time breakdown
- `cpuacct.usage_percpu`: Per-CPU breakdown
- Often co-mounted with cpu controller

```bash
cat /sys/fs/cgroup/cpu,cpuacct/mygroup/cpuacct.usage
# Output: 123456789012 (nanoseconds)
```
[Basic]

---

Q: What is `struct task_group` in the CPU controller?
A: `struct task_group` (in `kernel/sched/sched.h`) represents a CPU cgroup:
```c
struct task_group {
    struct cgroup_subsys_state css;
    struct sched_entity **se;    /* Per-CPU schedulable entity */
    struct cfs_rq **cfs_rq;      /* Per-CPU runqueue */
    unsigned long shares;        /* cpu.shares value */
    struct cfs_bandwidth cfs_bandwidth; /* Bandwidth control */
    /* ... */
};
```
Each task_group has per-CPU scheduling entities that participate in CFS.
[Advanced]

---

Q: How does the scheduler enforce CPU quota?
A: CFS bandwidth throttling works as follows:
1. Each period, the cgroup gets `quota` runtime
2. As tasks run, runtime is consumed from the pool
3. When runtime exhausted, tasks are dequeued (throttled)
4. At period boundary, runtime is refilled
5. Throttled tasks are re-enqueued

Implemented in `sched_cfs_period_timer()` and `__refill_cfs_bandwidth_runtime()`.
[Advanced]

---

Q: What is the memory controller (memcg)?
A: The memory controller limits and tracks memory usage:
- Hard limits: `memory.limit_in_bytes`
- Soft limits: `memory.soft_limit_in_bytes`
- Swap limits: `memory.memsw.limit_in_bytes`
- Accounting: `memory.usage_in_bytes`
- OOM control: `memory.oom_control`

Implemented in `mm/memcontrol.c`.
[Basic]

---

Q: What is `memory.limit_in_bytes`?
A: `memory.limit_in_bytes` sets the hard memory limit:
- Maximum memory the cgroup can use
- Exceeding triggers reclaim, then OOM if necessary
- Set to -1 for unlimited (default)
- Affects RSS + page cache attributed to cgroup

```bash
echo 256M > /sys/fs/cgroup/memory/mygroup/memory.limit_in_bytes
# Or in bytes:
echo 268435456 > /sys/fs/cgroup/memory/mygroup/memory.limit_in_bytes
```
[Basic]

---

Q: What is `memory.soft_limit_in_bytes`?
A: Soft limit is a best-effort target:
- Not enforced normally
- When system is under memory pressure, kernel reclaims from cgroups over soft limit first
- Used for prioritization, not hard limiting
- Set higher than expected normal usage

```bash
echo 512M > /sys/fs/cgroup/memory/mygroup/memory.soft_limit_in_bytes
```
[Intermediate]

---

Q: (ASCII Diagram) Show memory cgroup accounting components.
A:
```
memory.usage_in_bytes (current usage)
├── Anonymous pages (RSS)
│   └── Mapped: stack, heap, mmap(MAP_ANONYMOUS)
├── Page cache
│   └── File-backed pages attributed to cgroup
├── Kernel memory (if enabled)
│   └── Slab, page tables, etc.
└── Swap usage (separate or combined)

memory.stat breakdown:
+------------------+------------------+
| cache     (file) | rss    (anon)    |
| mapped_file      | pgpgin/pgpgout   |
| inactive_anon    | active_anon      |
| inactive_file    | active_file      |
+------------------+------------------+
```
[Intermediate]

---

Q: What is `struct mem_cgroup`?
A: `struct mem_cgroup` (in `mm/memcontrol.c`) represents a memory cgroup:
```c
struct mem_cgroup {
    struct cgroup_subsys_state css;
    struct res_counter res;      /* memory limit/usage */
    struct res_counter memsw;    /* memory+swap limit */
    struct mem_cgroup_lru_info info; /* LRU lists */
    bool oom_kill_disable;
    int swappiness;
    struct mem_cgroup_stat_cpu *stat;
    /* ... */
};
```
`res_counter` tracks usage against limits.
[Intermediate]

---

Q: What is `struct res_counter`?
A: `struct res_counter` provides hierarchical resource accounting:
```c
struct res_counter {
    unsigned long long usage;     /* current usage */
    unsigned long long max_usage; /* peak usage */
    unsigned long long limit;     /* hard limit */
    unsigned long long soft_limit;
    unsigned long long failcnt;   /* # of failures */
    struct res_counter *parent;   /* for hierarchy */
    spinlock_t lock;
};
```
Used by memory controller to track bytes against limits.
[Intermediate]

---

Q: How does memory cgroup OOM handling work?
A: When a cgroup hits its memory limit:
1. Kernel tries to reclaim pages from the cgroup
2. If reclaim fails, OOM killer is invoked
3. OOM killer selects a task within the cgroup to kill
4. `memory.oom_control` can pause OOM (set oom_kill_disable=1)
5. `memory.oom_control` file shows OOM state (under_oom)

```bash
echo 1 > /sys/fs/cgroup/memory/mygroup/memory.oom_control
```
[Intermediate]

---

Q: What is `memory.stat` file?
A: `memory.stat` provides detailed memory statistics:
```
cache 1234567890        # Page cache size
rss 987654321           # Anonymous + swap cache
mapped_file 12345678    # Mapped file pages
pgpgin 123456           # Pages charged (paged in)
pgpgout 123457          # Pages uncharged
swap 0                  # Swap usage
inactive_anon 1234      # Inactive anonymous pages
active_anon 5678        # Active anonymous pages
inactive_file 9012      # Inactive file pages
active_file 3456        # Active file pages
```
[Intermediate]

---

Q: (Code Interpretation) What resource state does this represent?
```
# cat /sys/fs/cgroup/memory/mycontainer/memory.stat
cache 104857600
rss 209715200
pgfault 1000000
pgmajfault 5000
```
A: The container has:
- 100MB in page cache (file-backed)
- 200MB in RSS (anonymous memory)
- 1 million page faults total
- 5000 major page faults (required disk I/O)

Total memory usage ~300MB. High major faults may indicate memory pressure or working set larger than available RAM.
[Intermediate]

---

Q: What is hierarchical memory accounting?
A: Memory usage is accounted hierarchically:
- Child cgroup usage counts toward parent
- Parent limit applies to sum of all children
- `memory.use_hierarchy` enables this (default: varies by kernel)

```
Root (limit: 1GB)
├── Child A (limit: 512MB, usage: 400MB)
└── Child B (limit: 512MB, usage: 300MB)
    Total: 700MB (within root's 1GB)
```
[Intermediate]

---

Q: What is `memory.memsw.limit_in_bytes`?
A: This limits memory + swap combined:
- `memory.limit_in_bytes`: RAM limit
- `memory.memsw.limit_in_bytes`: RAM + swap limit
- Must be >= memory.limit_in_bytes

Example: memory=256M, memsw=512M means 256MB RAM max, can use up to 256MB swap.
[Intermediate]

---

Q: (Reverse) This memory cgroup file shows the number of times the cgroup hit its limit and failed to allocate.
A: Q: What does `memory.failcnt` show?
[Intermediate]

---

Q: How does the memory controller track page ownership?
A: Page charging mechanism:
1. When page is allocated, `mem_cgroup_charge()` is called
2. Page is associated with task's memory cgroup
3. `page->mem_cgroup` (or page_cgroup struct) stores ownership
4. On page free, `mem_cgroup_uncharge()` decrements usage
5. File pages are charged to cgroup that first faults them in
[Advanced]

---

Q: What is memory cgroup "swappiness"?
A: `memory.swappiness` controls swap tendency for the cgroup:
- 0: Kernel avoids swapping anonymous pages
- 60: Default (balanced)
- 100: Kernel aggressively swaps
- Per-cgroup override of `/proc/sys/vm/swappiness`

```bash
echo 0 > /sys/fs/cgroup/memory/mygroup/memory.swappiness
```
[Intermediate]

---

Q: (Understanding) Why might a container have high memory.failcnt but not be OOM-killed?
A: High failcnt with no OOM can occur because:
1. **Reclaim succeeded**: Kernel freed enough pages after initial failure
2. **Retry worked**: Allocation succeeded after reclaim
3. **GFP_NOWAIT allocations**: Some allocations fail gracefully
4. **Soft limit exceeded**: Only soft limit triggers reclaim priority

failcnt is incremented even when reclaim eventually succeeds, so it shows "pressure events" not just OOM conditions.
[Advanced]

---

Q: What is the difference between `memory.usage_in_bytes` and `memory.max_usage_in_bytes`?
A: - `memory.usage_in_bytes`: Current memory usage (instantaneous)
- `memory.max_usage_in_bytes`: Peak/high-water mark since cgroup creation

Max usage is useful for capacity planning - shows the maximum memory the cgroup has ever needed.
[Basic]

---

Q: How do you reset the memory high-water mark?
A: Write 0 to `memory.max_usage_in_bytes`:
```bash
echo 0 > /sys/fs/cgroup/memory/mygroup/memory.max_usage_in_bytes
```
Also works for `memory.memsw.max_usage_in_bytes` (memory+swap peak).
[Basic]

---

## Section 7: I/O and Other Controllers

---

Q: What is the blkio (block I/O) controller?
A: The blkio controller limits and tracks block device I/O:
- **Weight-based**: `blkio.weight` for proportional I/O
- **Throttling**: `blkio.throttle.*` for hard limits
- Works with CFQ scheduler (weight) or any scheduler (throttle)
- Implemented in `block/blk-cgroup.c`
[Basic]

---

Q: What is `blkio.weight` and how does it work?
A: `blkio.weight` sets relative I/O priority:
- Range: 10-1000 (default: 500)
- Only effective with CFQ I/O scheduler
- Higher weight = more I/O bandwidth under contention
- Per-device weights via `blkio.weight_device`

```bash
echo 1000 > /sys/fs/cgroup/blkio/mygroup/blkio.weight
echo "8:0 500" > /sys/fs/cgroup/blkio/mygroup/blkio.weight_device
```
[Intermediate]

---

Q: What are the blkio throttling controls?
A: Throttling provides hard I/O limits:
- `blkio.throttle.read_bps_device`: Read bytes/sec limit
- `blkio.throttle.write_bps_device`: Write bytes/sec limit
- `blkio.throttle.read_iops_device`: Read IOPS limit
- `blkio.throttle.write_iops_device`: Write IOPS limit

Format: `major:minor limit`
```bash
echo "8:0 10485760" > /sys/fs/cgroup/blkio/mygroup/blkio.throttle.read_bps_device
# Limits /dev/sda (8:0) to 10MB/s reads
```
[Intermediate]

---

Q: (Cloze) blkio.weight requires the _____ I/O scheduler, while blkio.throttle works with _____ scheduler.
A: CFQ, any
[Intermediate]

---

Q: How do you identify a block device's major:minor numbers?
A: Several methods:
```bash
# Using ls
ls -la /dev/sda
# brw-rw---- 1 root disk 8, 0 Jan 1 00:00 /dev/sda

# Using stat
stat -c '%t:%T' /dev/sda  # hex format
stat /dev/sda | grep Device

# Using /sys
cat /sys/block/sda/dev
# Output: 8:0
```
[Basic]

---

Q: What does `blkio.io_service_bytes` show?
A: `blkio.io_service_bytes` shows total bytes transferred:
```
8:0 Read 1234567890
8:0 Write 987654321
8:0 Sync 1111111111
8:0 Async 1111111100
Total 2222222211
```
Shows per-device breakdown of I/O performed by the cgroup.
[Intermediate]

---

Q: What is the devices controller?
A: The devices controller restricts device access:
- Whitelist-based access control
- Controls access to character and block devices
- `devices.allow`: Add device to whitelist
- `devices.deny`: Remove from whitelist
- `devices.list`: Show current whitelist

```bash
# Allow read/write to /dev/null (1:3)
echo "c 1:3 rw" > /sys/fs/cgroup/devices/mygroup/devices.allow
```
[Intermediate]

---

Q: What is the format of devices.allow entries?
A: Format: `type major:minor access`
- **type**: `a` (all), `c` (char), `b` (block)
- **major:minor**: Device numbers (`*` for wildcard)
- **access**: `r` (read), `w` (write), `m` (mknod)

Examples:
```bash
echo "c *:* m" > devices.allow     # mknod any char device
echo "b 8:* rw" > devices.allow    # read/write all SCSI disks
echo "a" > devices.deny            # deny all (reset)
```
[Intermediate]

---

Q: (ASCII Diagram) Show the devices cgroup access control flow.
A:
```
Process in cgroup attempts device access
              │
              ▼
     ┌────────────────────┐
     │ Check devices.list │
     │ whitelist          │
     └─────────┬──────────┘
               │
      ┌────────┴────────┐
      │                 │
      ▼                 ▼
 ┌─────────┐      ┌─────────┐
 │ Allowed │      │ Denied  │
 │ (match) │      │(no match│
 └────┬────┘      └────┬────┘
      │                │
      ▼                ▼
   Access          -EPERM
   granted         returned
```
[Intermediate]

---

Q: What is the freezer controller?
A: The freezer controller pauses and resumes cgroup processes:
- `freezer.state`: `THAWED`, `FREEZING`, `FROZEN`
- Writing `FROZEN` sends SIGSTOP-like signal to all tasks
- Writing `THAWED` resumes execution
- Used for checkpointing, batch scheduling, container pause

```bash
echo FROZEN > /sys/fs/cgroup/freezer/mygroup/freezer.state
echo THAWED > /sys/fs/cgroup/freezer/mygroup/freezer.state
```
[Intermediate]

---

Q: How does the freezer controller differ from sending SIGSTOP?
A: Freezer advantages over SIGSTOP:
- Atomic: All processes frozen together
- Cannot be caught/ignored by process
- Kernel-level freeze (more thorough)
- Useful for checkpoint/restore (CRIU)
- Process doesn't know it was frozen (no signal delivery)

SIGSTOP can be caught in debuggers and shows in process state.
[Intermediate]

---

Q: What is the net_cls controller?
A: The net_cls controller tags network packets with a class ID:
- `net_cls.classid`: Tag for traffic control
- Works with tc (traffic control) for QoS
- Format: major:minor (0xAAAABBBB)

```bash
echo 0x10001 > /sys/fs/cgroup/net_cls/mygroup/net_cls.classid
# Tag packets with class 1:1 for tc filtering
```
[Intermediate]

---

Q: What is the net_prio controller?
A: The net_prio controller sets network priority per interface:
- `net_prio.prioidx`: Internal index
- `net_prio.ifpriomap`: Per-interface priority

```bash
# Set priority 5 for eth0 traffic from this cgroup
echo "eth0 5" > /sys/fs/cgroup/net_prio/mygroup/net_prio.ifpriomap
```
Priority is used by tc qdisc for packet scheduling.
[Intermediate]

---

Q: (Reverse) This cgroup controller allows you to pause all processes in a cgroup atomically, useful for checkpointing.
A: Q: What is the freezer controller?
[Basic]

---

Q: What is the perf_event controller?
A: The perf_event controller enables per-cgroup performance monitoring:
- Allows perf to monitor cgroup as a whole
- `perf stat -G mygroup command`
- Tracks CPU cycles, cache misses, etc. for cgroup
- Uses `perf_event_open()` with cgroup file descriptor
[Intermediate]

---

Q: How do you use perf with cgroups?
A: Attach perf to a cgroup:
```bash
# Monitor existing cgroup
perf stat -a -G /sys/fs/cgroup/perf_event/mygroup \
    --for-each-cgroup /sys/fs/cgroup/perf_event/mygroup \
    sleep 10

# Or using cgroup fd
perf record -e cycles -g --cgroup mygroup ./program
```
[Intermediate]

---

Q: What is the hugetlb controller?
A: The hugetlb controller limits huge page usage:
- `hugetlb.<size>.limit_in_bytes`: Max huge pages
- `hugetlb.<size>.usage_in_bytes`: Current usage
- Controls both 2MB and 1GB huge pages
- Prevents one cgroup from consuming all huge pages

```bash
echo 1073741824 > /sys/fs/cgroup/hugetlb/mygroup/hugetlb.2MB.limit_in_bytes
# Limit to 1GB worth of 2MB huge pages
```
[Advanced]

---

Q: (Code Interpretation) What does this blkio configuration achieve?
```bash
echo "8:0 1048576" > /sys/fs/cgroup/blkio/mygroup/blkio.throttle.write_bps_device
echo "8:0 100" > /sys/fs/cgroup/blkio/mygroup/blkio.throttle.write_iops_device
```
A: Double-limits writes to device 8:0 (/dev/sda):
- Maximum 1MB/s write throughput
- Maximum 100 write IOPS

Both limits apply simultaneously. If writes are 10KB each:
- 100 IOPS × 10KB = 1MB/s (IOPS is limiting)
If writes are 100KB each:
- 1MB/s ÷ 100KB = 10 IOPS (bandwidth is limiting)
[Intermediate]

---

Q: (Understanding) Why might blkio.weight have no effect?
A: blkio.weight may not work because:
1. **Wrong scheduler**: Only works with CFQ, not deadline/noop/mq-deadline
2. **No contention**: Weight only matters when multiple cgroups compete
3. **Direct I/O**: Some bypass scheduler (O_DIRECT to NVMe)
4. **SSD with NCQ**: Hardware may not honor ordering
5. **Writeback**: Async writes may batch differently

Use throttling for guaranteed limits on modern systems.
[Advanced]

---

Q: What controllers are commonly co-mounted?
A: Common co-mount combinations:
- `cpu,cpuacct`: CPU control + accounting
- `net_cls,net_prio`: Network classification + priority
- `memory` (alone): Memory needs own hierarchy
- `devices` (alone): Security-critical, separate

Co-mounting reduces overhead and simplifies management when controllers are related.
[Intermediate]

---

## Section 8: KVM Architecture

---

Q: What is KVM (Kernel-based Virtual Machine)?
A: KVM is a Linux kernel module that turns Linux into a Type-1 hypervisor:
- Uses hardware virtualization (VT-x/AMD-V)
- Each VM is a regular Linux process
- vCPUs are threads within the process
- Leverages Linux scheduler, memory management
- Exposes `/dev/kvm` device for userspace control
[Basic]

---

Q: What makes KVM a "hybrid" hypervisor?
A: KVM combines Type-1 and Type-2 characteristics:
- **Type-1 aspect**: Linux kernel IS the hypervisor, runs on bare metal
- **Type-2 aspect**: Runs as part of a full Linux OS with userspace
- Benefits: Linux device drivers, scheduling, memory management
- Result: Near bare-metal performance with full OS ecosystem
[Basic]

---

Q: What is the relationship between KVM and QEMU?
A: KVM and QEMU work together:
- **KVM**: Kernel module for CPU/memory virtualization
- **QEMU**: Userspace emulator for devices and VM management
- QEMU creates the VM process and device emulation
- KVM accelerates CPU execution (instead of QEMU's software emulation)
- Communication via `/dev/kvm` ioctls
[Basic]

---

Q: What is `/dev/kvm` and what operations does it support?
A: `/dev/kvm` is the KVM control interface:
```c
// System-level operations
fd = open("/dev/kvm", O_RDWR);
ioctl(fd, KVM_GET_API_VERSION);
ioctl(fd, KVM_CHECK_EXTENSION, capability);
ioctl(fd, KVM_CREATE_VM);  // Returns VM fd

// VM-level operations (on VM fd)
ioctl(vmfd, KVM_CREATE_VCPU, vcpu_id);
ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);

// vCPU-level operations (on vCPU fd)
ioctl(vcpufd, KVM_RUN);
```
[Intermediate]

---

Q: (ASCII Diagram) Show the KVM/QEMU architecture.
A:
```
                    QEMU Process (per VM)
+----------------------------------------------------------+
|  +-----------+  +-----------+  +---------------------+   |
|  | vCPU      |  | vCPU      |  | Device Emulation    |   |
|  | Thread    |  | Thread    |  | (virtio, IDE, etc.) |   |
|  +-----+-----+  +-----+-----+  +----------+----------+   |
|        |              |                   |              |
|        |   ioctl(KVM_RUN)                 |              |
+--------|--------------|-------------------|--------------|
         |              |                   |
    =====|==============|==================|=============== User/Kernel
         |              |                   |
         v              v                   v
+----------------------------------------------------------+
|                    KVM Module                             |
|  +------------------+  +------------------+               |
|  | struct kvm       |  | struct kvm_vcpu  |               |
|  | (VM state)       |  | (CPU state)      |               |
|  +--------+---------+  +--------+---------+               |
|           |                     |                         |
|           v                     v                         |
|  +------------------+  +------------------+               |
|  | Memory (EPT)     |  | VMCS/VMCB        |               |
|  +------------------+  +------------------+               |
+----------------------------------------------------------+
         |                        |
    =====|========================|========================= VMX Root/Non-root
         v                        v
+----------------------------------------------------------+
|                    Guest VM                               |
|  Guest OS runs in VMX non-root mode                       |
+----------------------------------------------------------+
```
[Intermediate]

---

Q: What is `struct kvm`?
A: `struct kvm` (in `include/linux/kvm_host.h`) represents a virtual machine:
```c
struct kvm {
    spinlock_t mmu_lock;
    struct mutex slots_lock;
    struct mm_struct *mm;              // Host process mm
    struct kvm_memslots *memslots;     // Guest physical memory
    struct kvm_vcpu *vcpus[KVM_MAX_VCPUS];
    atomic_t online_vcpus;
    struct kvm_io_bus *buses[];        // I/O handlers
    struct kvm_irq_routing_table *irq_routing;
    struct kvm_arch arch;              // Arch-specific (EPT, etc.)
};
```
[Intermediate]

---

Q: What is `struct kvm_vcpu`?
A: `struct kvm_vcpu` (in `include/linux/kvm_host.h`) represents a virtual CPU:
```c
struct kvm_vcpu {
    struct kvm *kvm;                   // Parent VM
    int vcpu_id;
    struct mutex mutex;
    int cpu;                           // Physical CPU affinity
    struct kvm_run *run;               // Shared with userspace
    int fpu_active;
    int guest_fpu_loaded;
    struct kvm_vcpu_arch arch;         // Arch-specific state
    /* ... */
};
```
Each vCPU is scheduled as a kernel thread by the Linux scheduler.
[Intermediate]

---

Q: What is `struct kvm_run`?
A: `struct kvm_run` is a shared memory region between kernel and userspace:
```c
struct kvm_run {
    __u8 request_interrupt_window;
    __u8 immediate_exit;
    __u32 exit_reason;                 // Why VM exited
    union {
        struct { __u64 hardware_exit_reason; } hw;
        struct { __u32 error_code; } fail_entry;
        struct { __u8 direction; __u16 port; } io;
        struct { __u64 phys_addr; __u8 *data; } mmio;
        /* ... */
    };
};
```
Mapped via `mmap()` on the vCPU fd.
[Intermediate]

---

Q: (Cloze) In KVM, the _____ structure represents the entire VM, while _____ represents each virtual CPU.
A: struct kvm, struct kvm_vcpu
[Basic]

---

Q: What is a VMCS (Virtual Machine Control Structure)?
A: VMCS is Intel's hardware structure for VM state:
- Controls VM entry/exit behavior
- Contains guest state (registers, segments)
- Contains host state (for VM exit)
- Execution control fields (what causes exits)
- One VMCS per vCPU
- Manipulated via `VMREAD`/`VMWRITE` instructions

KVM wrapper: `vmcs_read*()`, `vmcs_write*()` in `arch/x86/kvm/vmx/vmx.c`
[Intermediate]

---

Q: What is a VMCB (Virtual Machine Control Block)?
A: VMCB is AMD's equivalent of Intel's VMCS:
- Memory-resident structure (not special CPU region)
- Contains guest state and control fields
- Intercept control (which events cause exits)
- State save area for guest registers
- One VMCB per vCPU

```c
struct vmcb {
    struct vmcb_control_area control;
    struct vmcb_save_area save;
};
```
[Intermediate]

---

Q: What are KVM memory slots?
A: Memory slots map guest physical to host virtual addresses:
```c
struct kvm_userspace_memory_region {
    __u32 slot;
    __u32 flags;
    __u64 guest_phys_addr;    // Guest physical address
    __u64 memory_size;
    __u64 userspace_addr;     // Host virtual (mmap'd)
};
```
- Created via `KVM_SET_USER_MEMORY_REGION` ioctl
- QEMU mmaps memory and registers it with KVM
- KVM builds EPT/NPT from slots
[Intermediate]

---

Q: What is EPT (Extended Page Tables)?
A: EPT provides hardware-assisted guest physical to host physical translation:
```
Guest Virtual ──► Guest Page Tables ──► Guest Physical
                                              │
                                        EPT Tables
                                              │
                                              ▼
                                        Host Physical
```
- Two-dimensional paging (guest PT + EPT)
- Hardware walks both automatically
- Eliminates shadow page table overhead
- EPT violation = exit to KVM for handling
[Intermediate]

---

Q: What is shadow page table and when is it used?
A: Shadow page tables are software-maintained mappings:
- KVM maintains "shadow" of guest page tables
- Maps guest virtual directly to host physical
- Used when EPT/NPT not available
- Higher overhead: every guest PT change requires shadow update
- Causes more VM exits than EPT

```
With Shadow PT: GVA ──► Shadow PT ──► HPA
With EPT:       GVA ──► Guest PT ──► GPA ──► EPT ──► HPA
```
[Advanced]

---

Q: (Reverse) This Intel hardware feature allows automatic two-level page table walking for guest virtual to host physical translation.
A: Q: What is EPT (Extended Page Tables)?
[Intermediate]

---

Q: What is the KVM MMU subsystem responsible for?
A: The KVM MMU (in `arch/x86/kvm/mmu.c`) handles:
- Building and maintaining EPT/NPT tables
- Shadow page table management (without EPT)
- Handling EPT violations and page faults
- TLB flushing for guest
- Memory slot to EPT mapping
- Write protection for dirty page tracking
[Intermediate]

---

Q: What is virtual APIC in KVM?
A: KVM emulates the Advanced Programmable Interrupt Controller:
- Local APIC per vCPU (LAPIC)
- I/O APIC for external interrupts
- `struct kvm_lapic` in `arch/x86/kvm/lapic.c`
- Handles interrupt delivery to guests
- APICv (hardware acceleration) for better performance
[Intermediate]

---

Q: What is interrupt injection in KVM?
A: Interrupt injection delivers interrupts to guest:
1. **External interrupt**: Set in VMCS guest interrupt info
2. **kvm_queue_interrupt()**: Queue for injection
3. **On VM entry**: Hardware injects queued interrupt
4. **kvm_inject_irq()**: Inject via virtual APIC

Guest sees interrupt as if from real hardware.
[Intermediate]

---

Q: What is `kvm_x86_ops`?
A: `kvm_x86_ops` is a function pointer table for CPU-specific operations:
```c
struct kvm_x86_ops {
    int (*hardware_enable)(void);
    void (*hardware_disable)(void);
    void (*vcpu_create)(struct kvm_vcpu *vcpu);
    void (*vcpu_load)(struct kvm_vcpu *vcpu, int cpu);
    void (*run)(struct kvm_vcpu *vcpu);
    void (*handle_exit)(struct kvm_vcpu *vcpu);
    /* ... */
};
```
Intel VMX and AMD SVM provide different implementations.
[Advanced]

---

Q: How does KVM leverage the Linux scheduler?
A: vCPUs are scheduled like regular processes:
- Each vCPU runs as a kernel thread
- Linux CFS schedules vCPU threads
- `ioctl(KVM_RUN)` blocks until exit
- vCPU can be preempted like any thread
- Cgroups can limit VM CPU time
- Nice values affect vCPU priority
[Intermediate]

---

Q: What is vhost?
A: vhost is in-kernel virtio backend for better I/O performance:
- `vhost-net`: Network I/O in kernel (bypasses QEMU)
- `vhost-scsi`: Storage I/O in kernel
- Uses eventfd for notifications
- Reduces context switches vs userspace emulation

```
Without vhost: Guest ──► KVM ──► QEMU ──► Kernel ──► Device
With vhost:    Guest ──► KVM ──► vhost (kernel) ──► Device
```
[Intermediate]

---

Q: (ASCII Diagram) Show VM memory mapping with slots.
A:
```
QEMU Process Address Space          Guest Physical Address Space
+-------------------------+         +-------------------------+
|        Code/Data        |         |                         |
+-------------------------+         |     (unmapped)          |
|  mmap region (slot 0)   |◄───────►| 0x00000000 RAM          |
|  Guest RAM 1GB          |         | (1GB)                   |
+-------------------------+         +-------------------------+
|  mmap region (slot 1)   |◄───────►| 0x100000000 RAM above   |
|  Guest RAM above 4G     |         | 4GB line                |
+-------------------------+         +-------------------------+
|        Heap/Stack       |         |                         |
+-------------------------+         |     (unmapped)          |
                                    +-------------------------+
                                    | 0xFEC00000 IOAPIC MMIO  |
                                    +-------------------------+

KVM_SET_USER_MEMORY_REGION creates slot mappings
```
[Advanced]

---

Q: What is IOMMU/VT-d and how does it relate to KVM?
A: IOMMU provides device DMA isolation:
- **VT-d** (Intel) / **AMD-Vi** (AMD)
- Translates device DMA addresses to host physical
- Enables safe device passthrough to VMs
- Guest gets direct device access with DMA remapping
- VFIO kernel module provides passthrough interface
[Intermediate]

---

Q: What is device passthrough?
A: Device passthrough gives a VM direct hardware access:
- Physical device assigned exclusively to one VM
- Near-native performance (no emulation)
- Uses IOMMU for DMA address translation
- Common for GPUs, network cards, storage controllers
- Configured via VFIO (Virtual Function I/O)
[Intermediate]

---

Q: (Understanding) Why does KVM need both kernel and userspace components?
A: Division of responsibilities:
- **Kernel (KVM)**: CPU execution, memory virtualization, performance-critical paths
- **Userspace (QEMU)**: Device emulation, VM configuration, flexibility

Benefits:
- Security: Less code in kernel
- Flexibility: Easy to add devices in userspace
- Performance: Hot paths in kernel
- Maintenance: Userspace changes don't need kernel rebuild
[Intermediate]

---

## Section 9: KVM vCPU Execution and Exit Handling

---

Q: What is the KVM_RUN ioctl?
A: `KVM_RUN` is the main vCPU execution ioctl:
```c
while (running) {
    ioctl(vcpufd, KVM_RUN, NULL);
    // Check kvm_run->exit_reason
    switch (run->exit_reason) {
        case KVM_EXIT_IO: /* handle I/O */
        case KVM_EXIT_MMIO: /* handle MMIO */
        case KVM_EXIT_HLT: /* guest halted */
        /* ... */
    }
}
```
Blocks until VM exit, returns exit reason in shared `kvm_run` structure.
[Basic]

---

Q: What is the vCPU execution loop?
A: The main execution loop in KVM:
```
QEMU calls ioctl(KVM_RUN)
        │
        ▼
┌─────────────────────────────┐
│ vcpu_enter_guest()          │
│   - Load guest state        │
│   - VMLAUNCH/VMRESUME       │◄──────┐
└──────────────┬──────────────┘       │
               │                       │
        Guest executes                 │
               │                       │
        VM Exit occurs                 │
               │                       │
               ▼                       │
┌─────────────────────────────┐       │
│ Handle exit in kernel       │       │
│   - EPT violation           │───────┘ (re-enter)
│   - Interrupt               │
│   - CPUID, etc.             │
└──────────────┬──────────────┘
               │ (exit to userspace)
               ▼
        Return to QEMU
```
[Intermediate]

---

Q: What are the common VM exit reasons?
A: Key exit reasons in `kvm_run->exit_reason`:
- `KVM_EXIT_IO`: IN/OUT instruction (port I/O)
- `KVM_EXIT_MMIO`: Memory-mapped I/O access
- `KVM_EXIT_HLT`: HLT instruction (CPU idle)
- `KVM_EXIT_IRQ_WINDOW_OPEN`: Ready for interrupt injection
- `KVM_EXIT_SHUTDOWN`: Triple fault or shutdown
- `KVM_EXIT_FAIL_ENTRY`: VM entry failed
- `KVM_EXIT_INTERNAL_ERROR`: KVM internal error
[Basic]

---

Q: What is an EPT violation?
A: EPT violation occurs when guest memory access fails EPT walk:
- Page not mapped in EPT
- Permission violation (write to read-only)
- Causes VM exit to KVM
- KVM handles by mapping page or emulating MMIO
- `kvm_mmu_page_fault()` processes the violation
[Intermediate]

---

Q: (Code Interpretation) What does this exit handling code do?
```c
switch (run->exit_reason) {
case KVM_EXIT_IO:
    if (run->io.direction == KVM_EXIT_IO_OUT) {
        handle_io_out(run->io.port, 
                      (char *)run + run->io.data_offset,
                      run->io.size, run->io.count);
    }
    break;
}
```
A: Handles guest port I/O (IN/OUT instructions):
- Checks if it's an OUT instruction (write to port)
- Extracts port number, data pointer, size, and count
- Calls handler to emulate the I/O operation
- Data is in `kvm_run` structure at `data_offset`

This is how QEMU emulates legacy devices (keyboard, serial, etc.).
[Intermediate]

---

Q: What is MMIO emulation?
A: Memory-Mapped I/O emulation handles device register access:
```c
case KVM_EXIT_MMIO:
    if (run->mmio.is_write) {
        handle_mmio_write(run->mmio.phys_addr,
                          run->mmio.data,
                          run->mmio.len);
    } else {
        handle_mmio_read(run->mmio.phys_addr,
                         run->mmio.data,
                         run->mmio.len);
    }
    break;
```
Guest accesses unmapped GPA → EPT violation → KVM detects MMIO → exit to QEMU.
[Intermediate]

---

Q: How does KVM handle the HLT instruction?
A: When guest executes HLT (halt CPU):
1. VM exit with `KVM_EXIT_HLT` (or handled in-kernel)
2. If interrupts pending, inject and resume
3. Otherwise, vCPU blocks until interrupt arrives
4. `kvm_vcpu_block()` puts thread to sleep
5. When interrupt arrives, vCPU is woken
6. Guest resumes execution after HLT
[Intermediate]

---

Q: What is the difference between handled-in-kernel and userspace exits?
A: Exit handling location affects performance:

**In-kernel handling** (fast):
- EPT violations for mapped memory
- Most CPUID instructions
- Simple CR access
- Timer interrupts

**Userspace exit** (slow):
- Device MMIO/PIO
- Complex operations needing QEMU
- Requires context switch to userspace
[Intermediate]

---

Q: What is instruction emulation in KVM?
A: KVM emulates instructions that can't run natively:
- Real mode instructions (no VT-x real mode support)
- Instructions that need special handling
- `arch/x86/kvm/emulate.c` - full x86 emulator
- `x86_emulate_insn()` decodes and emulates

Example: Some guests need BIOS code emulated in real mode before protected mode.
[Intermediate]

---

Q: (Cloze) When a guest executes an IN/OUT instruction, KVM exits with reason _____, while memory-mapped device access causes _____ exit.
A: KVM_EXIT_IO, KVM_EXIT_MMIO
[Basic]

---

Q: What is `vcpu_enter_guest()`?
A: `vcpu_enter_guest()` is the core function that runs guest code:
```c
static int vcpu_enter_guest(struct kvm_vcpu *vcpu)
{
    // Prepare for entry
    kvm_x86_ops->prepare_guest_switch(vcpu);
    
    // Load guest state, enter VMX non-root
    kvm_x86_ops->run(vcpu);  // VMLAUNCH/VMRESUME
    
    // Back from VM exit
    kvm_x86_ops->handle_exit(vcpu);
    
    return r;
}
```
[Advanced]

---

Q: What happens during VM entry?
A: VM entry (VMLAUNCH/VMRESUME) sequence:
1. Check for pending events (interrupts, exceptions)
2. Load guest state from VMCS/VMCB
3. Switch to VMX non-root mode
4. Guest starts/resumes execution

If entry checks fail, `KVM_EXIT_FAIL_ENTRY` is returned.
[Intermediate]

---

Q: What is a VM exit?
A: VM exit transfers control from guest to hypervisor:
1. Hardware detects exit condition
2. Guest state saved to VMCS/VMCB
3. Host state loaded
4. Execution continues in VMX root mode
5. Exit reason available in VMCS/VMCB

Exit conditions: privileged instructions, interrupts, faults, etc.
[Basic]

---

Q: (ASCII Diagram) Show the VM entry/exit flow.
A:
```
VMX Root Mode (Host/KVM)              VMX Non-Root Mode (Guest)
        │                                      
        │ VMLAUNCH/                             
        │ VMRESUME                              
        ├────────────────────────────►┌────────────────────┐
        │                             │  Guest executes    │
        │                             │  - Normal code     │
        │                             │  - Privileged ops  │
        │           VM Exit           │  - I/O access      │
        ◄─────────────────────────────┤  - etc.            │
        │                             └────────────────────┘
        │                                      
    Handle exit                                
        │                                      
        ▼                                      
    Re-enter or                                
    return to userspace                        
```
[Intermediate]

---

Q: How does KVM handle CPUID exits?
A: CPUID instruction handling:
1. Guest executes CPUID → VM exit
2. KVM intercepts and emulates
3. Returns filtered/modified CPUID results
4. Hides host features guest shouldn't see
5. Can expose "hypervisor present" bit

`kvm_emulate_cpuid()` in `arch/x86/kvm/cpuid.c` handles this.
[Intermediate]

---

Q: What is the interrupt window?
A: Interrupt window is when guest can accept interrupts:
- Guest must have interrupts enabled (IF flag)
- No blocking by STI, MOV SS, etc.
- `KVM_EXIT_IRQ_WINDOW_OPEN` signals ready state
- KVM can request notification via `request_interrupt_window`

```c
run->request_interrupt_window = 1;
ioctl(vcpufd, KVM_RUN);  // Returns when window opens
// Now safe to inject interrupt
```
[Intermediate]

---

Q: How are virtual interrupts injected?
A: Interrupt injection process:
1. QEMU/KVM determines interrupt to inject
2. Set injection fields in VMCS/VMCB
3. On next VM entry, hardware delivers interrupt
4. Guest sees interrupt as if from real hardware

```c
// Queue interrupt
kvm_queue_interrupt(vcpu, irq_num, soft);

// Or via ioctl
struct kvm_interrupt intr = { .irq = irq_num };
ioctl(vcpufd, KVM_INTERRUPT, &intr);
```
[Intermediate]

---

Q: What is posted interrupt?
A: Posted interrupts allow interrupt delivery without VM exit:
- Hardware feature (Intel APICv)
- Interrupt posted directly to guest APIC
- No VM exit for interrupt delivery
- Significantly reduces interrupt latency
- Requires compatible APIC virtualization
[Advanced]

---

Q: (Reverse) This data structure shared between kernel and userspace contains the VM exit reason and associated data.
A: Q: What is `struct kvm_run`?
[Basic]

---

Q: What causes KVM_EXIT_FAIL_ENTRY?
A: Entry failure reasons:
- Invalid guest state in VMCS
- VMCS misconfiguration
- Hardware consistency checks failed
- `run->fail_entry.hardware_exit_reason` has details

Common causes: segment register issues, CR0/CR4 invalid combinations.
[Intermediate]

---

Q: What is `KVM_EXIT_INTERNAL_ERROR`?
A: Internal error indicates KVM bug or unexpected state:
- `run->internal.suberror` gives details
- `KVM_INTERNAL_ERROR_EMULATION`: Emulation failed
- `KVM_INTERNAL_ERROR_SIMUL_EX`: Simultaneous exceptions
- Usually indicates guest did something unexpected
[Intermediate]

---

Q: How does KVM handle CR (control register) access?
A: CR access handling:
- CR0, CR3, CR4 accesses may cause VM exit
- VMCS controls which CR accesses trap
- `handle_cr()` in vmx.c processes exits
- CR3 changes may require EPT/shadow PT updates
- Some CR changes handled without exit (CR3 caching)
[Intermediate]

---

Q: (Code Interpretation) What does this code structure represent?
```c
struct kvm_run {
    __u32 exit_reason;
    union {
        struct {
            __u8 direction;
            __u8 size;
            __u16 port;
            __u32 count;
            __u64 data_offset;
        } io;
        struct {
            __u64 phys_addr;
            __u8 data[8];
            __u32 len;
            __u8 is_write;
        } mmio;
    };
};
```
A: The `kvm_run` exit information union:
- `io`: Port I/O details (IN/OUT)
  - direction: in(0) or out(1)
  - port: I/O port number
  - data_offset: where data is in kvm_run
- `mmio`: Memory-mapped I/O details
  - phys_addr: guest physical address
  - data: actual data (up to 8 bytes)
  - is_write: read(0) or write(1)
[Intermediate]

---

Q: What is the MSR (Model Specific Register) exit?
A: MSR access handling:
- RDMSR/WRMSR may cause VM exit
- KVM emulates or passes through MSRs
- Bitmap controls which MSRs trap
- Some MSRs virtualized (TSC, APIC)
- `kvm_get_msr()`, `kvm_set_msr()` handle access
[Intermediate]

---

Q: (Understanding) Why do some exits return to userspace while others are handled in kernel?
A: Performance vs complexity tradeoff:

**In-kernel** (no userspace):
- Fast path, no context switch
- Simple, deterministic operations
- Memory faults, simple instructions

**Userspace** (QEMU):
- Complex device emulation
- Flexibility for different devices
- Less critical path
- Easier to debug and modify

Goal: Minimize userspace exits for VM performance.
[Intermediate]

---

## Section 10: Container Building Blocks Integration

---

Q: What are the core building blocks of a Linux container?
A: Linux containers combine multiple kernel features:
1. **Namespaces**: Resource isolation (PID, net, mount, etc.)
2. **Cgroups**: Resource limiting (CPU, memory, I/O)
3. **Capabilities**: Privilege reduction
4. **Seccomp**: System call filtering
5. **LSMs**: Security policies (SELinux, AppArmor)

Together they create an isolated, resource-controlled environment.
[Basic]

---

Q: (ASCII Diagram) Show how container building blocks fit together.
A:
```
┌─────────────────────────────────────────────────────────┐
│                    CONTAINER                             │
│  ┌─────────────────────────────────────────────────────┐│
│  │              Application Process                     ││
│  └──────────────────────┬──────────────────────────────┘│
│                         │                                │
│  ┌──────────────────────┼──────────────────────────────┐│
│  │    ISOLATION         │         LIMITATION           ││
│  │    (Namespaces)      │         (Cgroups)            ││
│  │                      │                               ││
│  │  ┌──────────────┐    │    ┌─────────────────────┐   ││
│  │  │ PID NS       │    │    │ cpu.cfs_quota_us    │   ││
│  │  │ NET NS       │    │    │ memory.limit        │   ││
│  │  │ MNT NS       │    │    │ blkio.throttle      │   ││
│  │  │ UTS NS       │    │    │ devices.allow       │   ││
│  │  │ IPC NS       │    │    └─────────────────────┘   ││
│  │  └──────────────┘    │                               ││
│  └──────────────────────┼──────────────────────────────┘│
│                         │                                │
│  ┌──────────────────────┼──────────────────────────────┐│
│  │    SECURITY          │                               ││
│  │  ┌───────────────────┼───────────────────────────┐  ││
│  │  │ Capabilities  Seccomp-BPF  SELinux/AppArmor   │  ││
│  │  │ (drop privs)  (filter      (mandatory         │  ││
│  │  │               syscalls)    access control)    │  ││
│  │  └───────────────────┴───────────────────────────┘  ││
│  └─────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘
```
[Intermediate]

---

Q: What is seccomp (secure computing mode)?
A: Seccomp filters system calls a process can make:
- **Mode 1 (strict)**: Only `read()`, `write()`, `exit()`, `sigreturn()`
- **Mode 2 (filter/BPF)**: Custom filter using BPF program

```c
prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
```
- Reduces kernel attack surface
- Violations cause SIGKILL or SIGSYS
- Containers use to block dangerous syscalls
[Intermediate]

---

Q: What is seccomp-BPF?
A: Seccomp-BPF uses BPF programs to filter syscalls:
```c
struct sock_filter filter[] = {
    /* Load syscall number */
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 
             offsetof(struct seccomp_data, nr)),
    /* Allow specific syscall */
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_read, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    /* Kill on anything else */
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
};
```
Can allow, deny, trace, or trap specific syscalls.
[Intermediate]

---

Q: What are Linux capabilities?
A: Capabilities break up root privileges into distinct units:
- `CAP_NET_ADMIN`: Network configuration
- `CAP_SYS_PTRACE`: Trace processes
- `CAP_SYS_ADMIN`: Broad admin (mount, etc.)
- `CAP_SETUID`: Change UIDs
- `CAP_NET_RAW`: Raw sockets

Containers drop capabilities to reduce privilege:
```c
cap_t caps = cap_get_proc();
cap_set_flag(caps, CAP_EFFECTIVE, 1, (cap_value_t[]){CAP_NET_ADMIN}, CAP_CLEAR);
cap_set_proc(caps);
```
[Intermediate]

---

Q: (Cloze) Containers use _____ to filter system calls and _____ to limit which privileged operations are allowed.
A: seccomp, capabilities
[Basic]

---

Q: How do containers typically set up isolation?
A: Container creation sequence:
```c
// 1. Create new namespaces
clone(CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | 
      CLONE_NEWUTS | CLONE_NEWIPC, ...);

// 2. Set up filesystem (in child)
mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL);
pivot_root(newroot, oldroot);
umount2(oldroot, MNT_DETACH);

// 3. Configure network
// (veth pair created by parent, moved to namespace)

// 4. Apply cgroup limits
// (process added to cgroup by parent)

// 5. Drop privileges
prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &filter);
cap_set_proc(reduced_caps);

// 6. Execute workload
execve(container_init, ...);
```
[Intermediate]

---

Q: What is the role of `/proc` and `/sys` in containers?
A: These virtual filesystems need special handling:
- **`/proc`**: Must mount namespace-specific `/proc` for correct PID view
- **`/sys`**: Some paths expose host info (security concern)
- Containers mount fresh copies or mask sensitive paths
- `/proc/sys` contains sysctls (some namespace-aware)

```bash
mount -t proc proc /proc
# Mask sensitive paths
mount --bind /dev/null /proc/kcore
```
[Intermediate]

---

Q: What is the "container escape" concern?
A: Container escape is breaking out of isolation:
- Kernel vulnerabilities bypass namespace isolation
- Capability misconfigurations grant excess privilege
- Mounted host paths provide access
- `/proc` or `/sys` can leak information

Mitigations:
- Drop all unnecessary capabilities
- Use seccomp to limit syscalls
- Don't mount host paths unnecessarily
- Use user namespaces (UID remapping)
[Intermediate]

---

Q: (Comparison) Compare the isolation spectrum from least to most isolated.
A:
```
Least Isolated                              Most Isolated
     │                                             │
     ▼                                             ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│ Threads │ │Processes│ │Container│ │   VM    │ │Separate │
│         │ │         │ │         │ │         │ │Hardware │
└────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘
     │          │           │           │           │
  shared     separate    separate    separate   separate
  address    address     + ns/cg     kernel     machine
  space      space       + seccomp   + IOMMU    

Overhead:  lowest ◄─────────────────────────► highest
Isolation: weakest ◄────────────────────────► strongest
```
[Basic]

---

Q: How does user namespace improve container security?
A: User namespace maps container UID 0 to unprivileged host UID:
```
Container              Host
UID 0 (root)  ───────► UID 100000 (nobody)
UID 1000      ───────► UID 101000
```
Benefits:
- Root in container is not root on host
- Kernel exploits don't give host root
- Can create namespaces without host root

Configured via `/proc/[pid]/uid_map` and `/proc/[pid]/gid_map`.
[Intermediate]

---

Q: What is `/proc/[pid]/uid_map`?
A: UID map file defines user ID translation:
```bash
# Format: container_start host_start length
echo "0 100000 65536" > /proc/$$/uid_map
```
This maps:
- Container UID 0-65535 → Host UID 100000-165535
- Process appears as root inside but is unprivileged outside

Must be written once before using user namespace.
[Intermediate]

---

Q: (Reverse) This security feature uses BPF programs to filter which system calls a containerized process can make.
A: Q: What is seccomp-BPF?
[Intermediate]

---

Q: What is AppArmor/SELinux's role in containers?
A: LSMs (Linux Security Modules) provide Mandatory Access Control:
- Define what resources container can access
- Independent of DAC (file permissions)
- AppArmor: Path-based profiles
- SELinux: Label-based policies

```bash
# AppArmor
docker run --security-opt apparmor=docker-default

# SELinux
docker run --security-opt label=type:container_t
```
[Intermediate]

---

Q: How do cgroups integrate with container runtimes?
A: Container runtimes (Docker, containerd) manage cgroups:
1. Create cgroup for container
2. Set resource limits before starting process
3. Add container process to cgroups
4. Monitor usage via cgroup files
5. Clean up cgroups on container exit

```bash
# Example cgroup path for Docker container
/sys/fs/cgroup/memory/docker/<container-id>/
```
[Basic]

---

Q: What is the `devices` cgroup's role in containers?
A: Devices cgroup restricts device access for security:
```bash
# Default: deny all
echo "a" > devices.deny

# Allow only specific devices
echo "c 1:3 rwm" > devices.allow   # /dev/null
echo "c 1:5 rwm" > devices.allow   # /dev/zero
echo "c 1:8 rwm" > devices.allow   # /dev/random
echo "c 1:9 rwm" > devices.allow   # /dev/urandom
echo "c 5:0 rwm" > devices.allow   # /dev/tty
echo "c 5:2 rwm" > devices.allow   # /dev/ptmx
```
Prevents container from accessing disks, hardware, etc.
[Intermediate]

---

Q: (Understanding) Why do containers need both namespaces AND cgroups?
A: They serve different purposes:

**Namespaces**: *What you can see*
- Isolate the view of system resources
- Container sees only its processes, network, mounts
- Provides *identity* isolation

**Cgroups**: *How much you can use*
- Limit resource consumption
- Container can only use X% CPU, Y MB RAM
- Provides *resource* isolation

Both are needed: namespace hides other containers' processes, cgroup prevents one container from starving others.
[Basic]

---

Q: What is a container runtime?
A: Container runtime creates and manages containers:
- **Low-level**: runc, crun (OCI runtime, talks to kernel)
- **High-level**: containerd, CRI-O (image management, API)

runc example:
```bash
# Creates namespaces, applies cgroups, sets up rootfs
runc create mycontainer
runc start mycontainer
```
Runtime handles all namespace/cgroup/seccomp setup.
[Basic]

---

## Section 11: Kernel Implementation Details

---

Q: Where is namespace creation handled in the kernel?
A: Namespace creation call path:
```
sys_clone() / sys_unshare()
    └── copy_namespaces()  [kernel/nsproxy.c]
        ├── create_new_namespaces()
        │   ├── copy_mnt_ns()    [fs/namespace.c]
        │   ├── copy_utsname()   [kernel/utsname.c]
        │   ├── copy_ipcs()      [ipc/namespace.c]
        │   ├── copy_pid_ns()    [kernel/pid_namespace.c]
        │   └── copy_net_ns()    [net/core/net_namespace.c]
        └── switch_task_namespaces()
```
Each namespace type has its own creation function.
[Intermediate]

---

Q: What is `copy_namespaces()` responsible for?
A: `copy_namespaces()` in `kernel/nsproxy.c`:
```c
int copy_namespaces(unsigned long flags, struct task_struct *tsk)
{
    struct nsproxy *old_ns = tsk->nsproxy;
    struct nsproxy *new_ns;
    
    // If no new namespaces requested, share parent's
    if (!(flags & (CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC |
                   CLONE_NEWPID | CLONE_NEWNET)))
        return 0;
    
    // Create new nsproxy with selected new namespaces
    new_ns = create_new_namespaces(flags, tsk, ...);
    tsk->nsproxy = new_ns;
    return 0;
}
```
[Advanced]

---

Q: How is `setns()` implemented?
A: `setns()` implementation in `kernel/nsproxy.c`:
```c
SYSCALL_DEFINE2(setns, int, fd, int, nstype)
{
    struct nsproxy *new_nsproxy;
    struct file *file = fget(fd);
    struct ns_common *ns = get_proc_ns(file->f_inode);
    
    // Validate nstype matches file
    if (nstype && (ns->ops->type != nstype))
        return -EINVAL;
    
    // Create nsproxy with the new namespace
    new_nsproxy = create_nsproxy();
    // Install new namespace into nsproxy
    ns->ops->install(new_nsproxy, ns);
    
    switch_task_namespaces(current, new_nsproxy);
    return 0;
}
```
[Advanced]

---

Q: What is the `ns_operations` structure?
A: `struct ns_operations` defines namespace-type-specific operations:
```c
struct ns_operations {
    const char *name;
    int type;           // CLONE_NEW* flag
    struct ns_common *(*get)(struct task_struct *task);
    void (*put)(struct ns_common *ns);
    int (*install)(struct nsproxy *nsproxy, struct ns_common *ns);
    struct user_namespace *(*owner)(struct ns_common *ns);
};
```
Each namespace type registers its ops: `mntns_operations`, `netns_operations`, etc.
[Advanced]

---

Q: (Cloze) The kernel function _____ in `kernel/nsproxy.c` handles creating new namespaces during `clone()` and `unshare()`.
A: copy_namespaces() or create_new_namespaces()
[Intermediate]

---

Q: How does `clone()` handle the CLONE_NEWPID flag?
A: PID namespace creation path:
```c
// In copy_process() [kernel/fork.c]
copy_namespaces(clone_flags, p);
    └── copy_pid_ns() [kernel/pid_namespace.c]
        └── create_pid_namespace()
            ├── Allocate struct pid_namespace
            ├── Initialize pidmap
            ├── Set parent pointer
            ├── Set level = parent->level + 1
            └── Return new namespace
            
// Later in copy_process()
alloc_pid(pid_ns);  // Allocate PID in new namespace
```
[Advanced]

---

Q: What is `struct pid` and how does it support multi-level PIDs?
A: `struct pid` holds PID values for all namespace levels:
```c
struct pid {
    atomic_t count;
    unsigned int level;  // Deepest namespace level
    struct upid numbers[1];  // Flexible array
};

struct upid {
    int nr;                      // PID value
    struct pid_namespace *ns;    // Namespace
    struct hlist_node pid_chain; // Hash chain
};
```
Array has `level + 1` entries, one per namespace from root to current.
[Advanced]

---

Q: How does the cgroup filesystem handle mkdir?
A: Cgroup directory creation in `kernel/cgroup.c`:
```c
// VFS mkdir calls cgroup_mkdir()
static int cgroup_mkdir(struct inode *dir, struct dentry *dentry, 
                        umode_t mode)
{
    struct cgroup *parent = dentry->d_parent->d_fsdata;
    struct cgroup *cgrp;
    
    // Allocate new cgroup
    cgrp = kzalloc(sizeof(*cgrp), GFP_KERNEL);
    cgrp->parent = parent;
    
    // Initialize subsystem states
    for_each_subsys(ss, i) {
        css = ss->create(cgrp);
        cgrp->subsys[i] = css;
    }
    
    // Add to parent's children list
    list_add_tail(&cgrp->sibling, &parent->children);
    
    return 0;
}
```
[Advanced]

---

Q: What is `cgroup_attach_task()` responsible for?
A: `cgroup_attach_task()` moves a task to a cgroup:
```c
int cgroup_attach_task(struct cgroup *cgrp, struct task_struct *tsk)
{
    struct css_set *cset;
    
    // Find or create css_set for new cgroup combination
    cset = find_css_set(tsk->cgroups, cgrp);
    
    // Call subsystem can_attach callbacks
    for_each_subsys(ss, i) {
        ss->can_attach(cgrp, tsk);
    }
    
    // Update task's cgroups pointer
    rcu_assign_pointer(tsk->cgroups, cset);
    
    // Call subsystem attach callbacks
    for_each_subsys(ss, i) {
        ss->attach(cgrp, tsk);
    }
    
    return 0;
}
```
[Advanced]

---

Q: How is memory charged to a cgroup?
A: Memory charging flow in `mm/memcontrol.c`:
```c
// Called when page is allocated
int mem_cgroup_charge(struct page *page, struct mm_struct *mm, 
                      gfp_t gfp_mask)
{
    struct mem_cgroup *memcg = get_mem_cgroup_from_mm(mm);
    
    // Try to charge against cgroup limit
    ret = try_charge(memcg, gfp_mask, nr_pages);
    if (ret)
        return ret;  // Over limit, reclaim or fail
    
    // Associate page with cgroup
    page->mem_cgroup = memcg;
    
    return 0;
}
```
[Advanced]

---

Q: (ASCII Diagram) Show the cgroup task migration data flow.
A:
```
echo <pid> > /sys/fs/cgroup/.../tasks
              │
              ▼
    cgroup_tasks_write()
              │
              ▼
    cgroup_attach_task()
              │
    ┌─────────┴─────────┐
    │                   │
    ▼                   ▼
find_css_set()    can_attach() callbacks
    │                   │
    ▼                   ▼
rcu_assign_pointer   attach() callbacks
(task->cgroups)         │
    │                   │
    └─────────┬─────────┘
              │
              ▼
    Task now in new cgroup
    (old css_set refcount--)
```
[Intermediate]

---

Q: What locks protect cgroup operations?
A: Cgroup locking hierarchy:
- `cgroup_mutex`: Top-level, protects hierarchy changes
- `cgroup_root_mutex`: Protects root operations
- `css_set_lock`: Protects css_set hash table
- Per-cgroup `mutex`: Fine-grained cgroup lock
- RCU: For lockless task->cgroups access

Lock order: `cgroup_mutex` → `css_set_lock` → per-subsystem locks
[Advanced]

---

Q: How does the network namespace initialize `loopback_dev`?
A: Loopback setup in `net/core/net_namespace.c`:
```c
static int __init net_ns_init(void)
{
    // Register pernet operations
    register_pernet_device(&loopback_net_ops);
}

static struct pernet_operations loopback_net_ops = {
    .init = loopback_net_init,
    .exit = loopback_net_exit,
};

static int loopback_net_init(struct net *net)
{
    struct net_device *dev;
    dev = alloc_netdev(0, "lo", loopback_setup);
    register_netdev(dev);
    net->loopback_dev = dev;
    return 0;
}
```
[Advanced]

---

Q: What are "pernet operations"?
A: Pernet operations initialize/cleanup per-namespace data:
```c
struct pernet_operations {
    int (*init)(struct net *net);      // Called on ns creation
    void (*exit)(struct net *net);     // Called on ns destruction
    void (*exit_batch)(struct list_head *net_exit_list);
    int *id;
    size_t size;
};
```
Used by network subsystems (routing, netfilter, etc.) to maintain per-namespace state. Registered via `register_pernet_subsys()`.
[Advanced]

---

Q: (Reverse) This kernel function is called when a task's memory usage is charged against its memory cgroup's limit.
A: Q: What is `mem_cgroup_charge()` or `try_charge()`?
[Advanced]

---

Q: How does KVM register with the kernel?
A: KVM module initialization in `virt/kvm/kvm_main.c`:
```c
static int __init kvm_init(void)
{
    // Check hardware support
    r = kvm_arch_init();  // Checks VT-x/AMD-V
    
    // Register misc device /dev/kvm
    r = misc_register(&kvm_dev);
    
    // Initialize architecture-specific
    r = kvm_arch_hardware_setup();
    
    // Register CPU hotplug notifier
    register_cpu_notifier(&kvm_cpu_notifier);
    
    return 0;
}
module_init(kvm_init);
```
[Intermediate]

---

Q: What is `kvm_vm_ioctl()` responsible for?
A: `kvm_vm_ioctl()` handles VM-level operations:
```c
static long kvm_vm_ioctl(struct file *filp, unsigned int ioctl, 
                         unsigned long arg)
{
    struct kvm *kvm = filp->private_data;
    
    switch (ioctl) {
    case KVM_CREATE_VCPU:
        r = kvm_vm_ioctl_create_vcpu(kvm, arg);
        break;
    case KVM_SET_USER_MEMORY_REGION:
        r = kvm_vm_ioctl_set_memory_region(kvm, &region);
        break;
    case KVM_CREATE_IRQCHIP:
        r = kvm_vm_ioctl_create_irqchip(kvm);
        break;
    // ...
    }
    return r;
}
```
[Intermediate]

---

Q: How is `KVM_RUN` implemented?
A: `KVM_RUN` implementation path:
```c
kvm_vcpu_ioctl(KVM_RUN)
    └── kvm_arch_vcpu_ioctl_run()  [arch/x86/kvm/x86.c]
        └── vcpu_run()
            └── while (!need_exit) {
                    vcpu_enter_guest()
                        ├── prepare_guest_switch()
                        ├── kvm_x86_ops->run()  // VMLAUNCH
                        └── handle_exit()
                    
                    if (exit_to_userspace)
                        break;
                }
```
Loops until exit requires userspace handling.
[Advanced]

---

Q: What is the EPT violation handling path?
A: EPT violation handling in KVM:
```c
vmx_handle_exit()
    └── handle_ept_violation()
        └── kvm_mmu_page_fault()  [arch/x86/kvm/mmu.c]
            ├── Check if MMIO region
            │   └── If yes, exit to userspace
            ├── Try to map the page
            │   └── tdp_page_fault() / nonpaging_page_fault()
            └── If successful, re-enter guest
```
[Advanced]

---

Q: (Code Interpretation) What does this kernel code do?
```c
void switch_task_namespaces(struct task_struct *p, 
                            struct nsproxy *new)
{
    struct nsproxy *ns;
    
    ns = p->nsproxy;
    rcu_assign_pointer(p->nsproxy, new);
    
    if (ns)
        put_nsproxy(ns);
}
```
A: Atomically switches a task's namespace proxy:
1. Saves old nsproxy pointer
2. RCU-assigns new nsproxy to task (atomic update)
3. Decrements old nsproxy refcount (may free if zero)

RCU ensures readers accessing task->nsproxy see consistent state.
[Advanced]

---

Q: What is the css_set refcounting model?
A: css_set reference counting:
- Each task holds a reference to its css_set
- css_set holds references to each CSS (subsystem state)
- When task moves cgroups:
  1. Find/create new css_set
  2. Increment new css_set refcount
  3. Decrement old css_set refcount
  4. If old refcount hits 0, free css_set

Multiple tasks in same cgroups share css_set (efficiency).
[Advanced]

---

Q: How does the kernel handle container process termination?
A: On process exit with namespaces:
```c
do_exit()
    └── exit_task_namespaces()  [kernel/nsproxy.c]
        └── switch_task_namespaces(NULL)
            └── put_nsproxy()
                └── free_nsproxy()
                    ├── put_mnt_ns()
                    ├── put_uts_ns()
                    ├── put_ipc_ns()
                    ├── put_pid_ns()
                    └── put_net_ns()
                    
    └── exit_creds() // Drop capabilities
    └── cgroup_exit() // Update cgroup task count
```
Namespaces freed when last reference dropped.
[Advanced]

---

## Section 12: Debugging, Comparison, and Misconceptions

---

Q: How do you inspect a process's namespace membership?
A: Use `/proc/[pid]/ns/` directory:
```bash
# List all namespaces
ls -la /proc/$$/ns/
# lrwxrwxrwx 1 user user 0 ipc -> ipc:[4026531839]
# lrwxrwxrwx 1 user user 0 mnt -> mnt:[4026531840]
# lrwxrwxrwx 1 user user 0 net -> net:[4026531956]
# lrwxrwxrwx 1 user user 0 pid -> pid:[4026531836]
# lrwxrwxrwx 1 user user 0 uts -> uts:[4026531838]

# Compare two processes
[ $(readlink /proc/1/ns/net) = $(readlink /proc/$$/ns/net) ] && \
    echo "Same network namespace"
```
[Basic]

---

Q: How do you check a process's cgroup membership?
A: Read `/proc/[pid]/cgroup`:
```bash
cat /proc/$$/cgroup
# 11:devices:/user.slice
# 10:memory:/user.slice/user-1000.slice
# 9:blkio:/user.slice
# 8:cpuset:/
# 7:cpu,cpuacct:/user.slice
# ...

# Full path shown; empty means root cgroup
```
[Basic]

---

Q: What is `/proc/cgroups` and what does it show?
A: `/proc/cgroups` shows system-wide cgroup controller info:
```bash
cat /proc/cgroups
#subsys_name  hierarchy  num_cgroups  enabled
#cpuset          1          3           1
#cpu             2         24           1
#cpuacct         2         24           1
#memory          3         42           1
#devices         4         24           1
#freezer         5          3           1
#blkio           6         24           1
```
Shows which controllers exist and how many cgroups use each.
[Basic]

---

Q: (Misconception) "Containers are lightweight VMs"
A: **This is misleading.** Key differences:
- Containers share host kernel; VMs have separate kernels
- Container isolation is kernel-enforced; VM isolation is hardware-enforced
- Kernel vulnerability affects all containers; VM escapes are much harder
- Containers are processes with restrictions; VMs are full machine emulation

Better mental model: "Containers are isolated processes with resource limits."
[Basic]

---

Q: (Misconception) "Docker is a virtualization technology"
A: **Docker is a container runtime**, not virtualization:
- Docker uses kernel namespaces and cgroups
- No hypervisor or hardware emulation
- Docker Engine creates/manages container processes
- The kernel provides the isolation, Docker just orchestrates

Docker ≈ User-friendly wrapper around Linux container primitives.
[Basic]

---

Q: (Misconception) "Namespaces provide complete isolation"
A: **Namespaces don't isolate everything**:
- Kernel is shared (kernel bugs affect all containers)
- Some `/proc` and `/sys` entries leak host info
- Time (`gettimeofday()`) is not namespaced (until recent kernels)
- Kernel keyring is not fully namespaced
- UID 0 in container may still be privileged for some operations

True isolation requires namespaces + cgroups + seccomp + capabilities.
[Intermediate]

---

Q: (Misconception) "Cgroups limit what a process can see"
A: **Cgroups limit usage, not visibility**:
- Cgroups: "You can use 50% CPU" (resource limit)
- Namespaces: "You can only see these processes" (resource isolation)

A process in a CPU cgroup can still see all host processes (if same PID namespace). Cgroups are for *resource control*, namespaces are for *visibility control*.
[Basic]

---

Q: What is a common container escape vector?
A: Common container escape scenarios:
1. **Kernel vulnerabilities**: Shared kernel means kernel bugs escape all containers
2. **Mounted Docker socket**: `/var/run/docker.sock` gives full control
3. **Privileged mode**: `--privileged` removes most isolation
4. **CAP_SYS_ADMIN**: Overly broad capability enables many escapes
5. **Sensitive host paths**: Mounting `/` or `/etc` compromises host

Defense: Drop capabilities, use seccomp, don't mount unnecessary paths.
[Intermediate]

---

Q: (Comparison) What are the performance overheads of containers vs VMs?
A:
| Aspect | Containers | VMs |
|--------|------------|-----|
| Startup | Milliseconds | Seconds-minutes |
| Memory | Process overhead only | Full OS + hypervisor |
| CPU | Native (no emulation) | Near-native (VT-x) |
| I/O | Native syscalls | Virtio or emulated |
| Density | 100s per host | 10s per host |

Containers: ~1-2% overhead (cgroup accounting)
VMs: ~2-10% overhead (EPT walks, I/O virtualization)
[Intermediate]

---

Q: How do you debug cgroup resource limits?
A: Debug cgroup limits with these files:
```bash
# Memory
cat memory.usage_in_bytes    # Current usage
cat memory.limit_in_bytes    # Current limit
cat memory.failcnt           # Times limit was hit
cat memory.stat              # Detailed breakdown

# CPU
cat cpu.stat                 # Throttling info
# nr_throttled shows if hitting limit

# I/O
cat blkio.throttle.io_service_bytes  # I/O done
cat blkio.throttle.io_serviced       # IOPS done
```
[Intermediate]

---

Q: How do you enter a container's namespace for debugging?
A: Use `nsenter` command:
```bash
# Enter all namespaces of PID 1234
nsenter -t 1234 -a /bin/sh

# Enter specific namespaces
nsenter -t 1234 --net --pid /bin/sh

# Or programmatically
fd = open("/proc/1234/ns/net", O_RDONLY);
setns(fd, CLONE_NEWNET);
close(fd);
```
[Basic]

---

Q: What is `lsns` command?
A: `lsns` lists namespaces in the system:
```bash
lsns
# NS         TYPE   NPROCS PID  USER  COMMAND
# 4026531835 cgroup    123  1   root  /sbin/init
# 4026531836 pid       123  1   root  /sbin/init
# 4026531837 user      123  1   root  /sbin/init
# 4026531838 uts       123  1   root  /sbin/init
# 4026531956 net         2  456 root  nginx
```
Shows namespace inode, type, process count, and initial process.
[Basic]

---

Q: (Reverse) This common misconception suggests containers provide VM-level isolation when they actually share the host kernel.
A: Q: What is the "containers are lightweight VMs" misconception?
[Basic]

---

Q: How do you trace namespace-related syscalls?
A: Use strace or ftrace:
```bash
# Trace namespace syscalls
strace -e clone,unshare,setns ./container_runtime

# Trace cgroup writes
echo 1 > /sys/kernel/debug/tracing/events/cgroup/enable
cat /sys/kernel/debug/tracing/trace

# Trace specific syscalls
strace -e 'trace=clone' -f docker run nginx
```
[Intermediate]

---

Q: What kernel parameters affect virtualization performance?
A: Key tunables:
```bash
# EPT/NPT support (usually auto-detected)
cat /sys/module/kvm_intel/parameters/ept  # 1 = enabled

# Cgroup memory pressure
cat /proc/sys/vm/swappiness  # Affects cgroup reclaim

# Network namespace performance
cat /proc/sys/net/core/netdev_budget  # Packet processing

# KVM halt polling (reduce latency)
cat /sys/module/kvm/parameters/halt_poll_ns
```
[Intermediate]

---

Q: (Misconception) "User namespace makes containers as secure as VMs"
A: **User namespace improves but doesn't equal VM security**:
- Root in container can still exploit kernel bugs
- Kernel doesn't fully validate all operations for user ns
- Some subsystems don't respect user namespace boundaries
- VM hardware isolation is fundamentally different

User namespace is one layer; defense in depth (seccomp, capabilities, LSM) still needed.
[Intermediate]

---

Q: How do you check if KVM hardware support is available?
A: Check hardware and module status:
```bash
# Check CPU flags
grep -E 'vmx|svm' /proc/cpuinfo

# Check KVM module loaded
lsmod | grep kvm
# kvm_intel (or kvm_amd)
# kvm

# Check /dev/kvm exists
ls -la /dev/kvm

# Check KVM capabilities
cat /sys/module/kvm_intel/parameters/nested  # Nested virt
```
[Basic]

---

Q: What is the `unshare` command line tool?
A: `unshare` creates new namespaces for a command:
```bash
# Run shell in new PID and mount namespace
unshare --pid --mount --fork /bin/bash

# Run in new network namespace (becomes isolated)
unshare --net ip addr  # Shows only loopback

# Run with new user namespace (become "root")
unshare --user --map-root-user /bin/bash
```
Useful for testing container isolation manually.
[Basic]

---

Q: (Understanding) Why can't you simply "pause" a container by sending SIGSTOP?
A: SIGSTOP limitations for container pause:
1. **Not atomic**: Race condition pausing multiple processes
2. **Catchable in debuggers**: GDB can intercept
3. **Process sees it**: State shows as stopped (T)
4. **Not thorough**: Kernel threads, pending timers not stopped

Freezer cgroup provides proper atomic pause:
- All processes frozen together
- Kernel-level freeze
- Used for checkpoint/restore (CRIU)
[Intermediate]

---

Q: How do you measure container overhead?
A: Measure container overhead:
```bash
# CPU overhead (compare same workload)
time docker run ubuntu:20.04 /bin/true
time /bin/true

# Memory overhead
cat /sys/fs/cgroup/memory/docker/<id>/memory.usage_in_bytes

# Syscall overhead (strace timing)
strace -c docker run ubuntu echo hello

# Network latency (ping between containers)
docker exec container1 ping -c 100 container2
```
Expect ~1-5% overhead for typical workloads.
[Intermediate]

---

