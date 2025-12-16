# Linux Kernel Block Device Driver Deep Dive (v3.2)

This document provides a comprehensive, code-level walkthrough of the Linux 3.2 Block Device Driver subsystem. The block layer handles all I/O to block devices (disks, SSDs, USB storage, etc.) and provides scheduling, merging, and a unified interface for diverse storage hardware.

---

## 1. Subsystem Context (Big Picture)

### What Is the Block Device Driver Subsystem?

The **Block Layer** (often called the "block I/O subsystem" or "blk layer") is the kernel infrastructure that:

1. **Accepts I/O requests** from filesystems and user applications
2. **Optimizes I/O** through merging adjacent requests and reordering for seek optimization
3. **Schedules I/O** using pluggable I/O schedulers (elevators)
4. **Dispatches requests** to device drivers
5. **Handles completion** and error reporting back to callers

### What Problem Does It Solve?

| Problem | Solution |
|---------|----------|
| **Diverse hardware** | Unified API via `block_device_operations` and `request_queue` |
| **Slow disk seeks** | I/O schedulers (CFQ, Deadline, NOOP) optimize request ordering |
| **Small I/O overhead** | Request merging combines adjacent I/O into single requests |
| **Request limits** | Queue limits enforce hardware constraints |
| **Partition handling** | Transparent sector remapping for partitions |
| **Device enumeration** | gendisk/hd_struct expose disks in `/sys/block/` |
| **Completion notification** | Asynchronous bio completion via callbacks |

### Where It Sits in the Kernel Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              USER SPACE                                       │
│  ┌──────────────────────────────────────────────────────────────────────────┐│
│  │   Applications (dd, cp, database, etc.)                                   ││
│  │     read(fd, buf, 4096)  /  write(fd, buf, 4096)                         ││
│  └──────────────────────────────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────────────────────────────┘
                                    │
════════════════════════════════════│═══════════════════════════════════════════
                           System Calls
                                    │
┌──────────────────────────────────▼───────────────────────────────────────────┐
│                              KERNEL SPACE                                     │
│                                                                               │
│  ┌───────────────────────────────────────────────────────────────────────┐   │
│  │                    VFS (Virtual File System)                           │   │
│  │         vfs_read() / vfs_write() / generic_file_*                     │   │
│  └──────────────────────────────────┬────────────────────────────────────┘   │
│                                     │                                         │
│  ┌──────────────────────────────────▼────────────────────────────────────┐   │
│  │                    FILESYSTEM LAYER                                    │   │
│  │         ext4, xfs, btrfs, etc.                                        │   │
│  │         Converts file I/O to block I/O                                │   │
│  │              ↓                                                         │   │
│  │         submit_bio(READ/WRITE, bio)                                   │   │
│  └──────────────────────────────────┬────────────────────────────────────┘   │
│                                     │                                         │
│  ┌──────────────────────────────────▼────────────────────────────────────┐   │
│  │                    PAGE CACHE                                          │   │
│  │         Caches file data in memory                                    │   │
│  │         Triggers writeback for dirty pages                            │   │
│  └──────────────────────────────────┬────────────────────────────────────┘   │
│                                     │                                         │
│  ┌══════════════════════════════════▼════════════════════════════════════┐   │
│  ║                    BLOCK LAYER (block/)                               ║   │
│  ║  ┌─────────────────────────────────────────────────────────────────┐  ║   │
│  ║  │   generic_make_request(bio)                                     │  ║   │
│  ║  │        │                                                        │  ║   │
│  ║  │        ▼                                                        │  ║   │
│  ║  │   ┌─────────────────────────────────────────────────────────┐  │  ║   │
│  ║  │   │  make_request_fn (default: blk_queue_bio)               │  │  ║   │
│  ║  │   │    - Merge bio with existing request                    │  │  ║   │
│  ║  │   │    - Or allocate new request                            │  │  ║   │
│  ║  │   │    - Add to I/O scheduler                               │  │  ║   │
│  ║  │   └───────────────────────┬─────────────────────────────────┘  │  ║   │
│  ║  │                           ▼                                     │  ║   │
│  ║  │   ┌─────────────────────────────────────────────────────────┐  │  ║   │
│  ║  │   │  I/O SCHEDULER (ELEVATOR)                               │  │  ║   │
│  ║  │   │    - CFQ: Complete Fair Queuing (per-process fairness)  │  │  ║   │
│  ║  │   │    - Deadline: Request deadline guarantees              │  │  ║   │
│  ║  │   │    - NOOP: Simple FIFO (for SSDs/VMs)                   │  │  ║   │
│  ║  │   │    - Sorts, merges, and dispatches requests             │  │  ║   │
│  ║  │   └───────────────────────┬─────────────────────────────────┘  │  ║   │
│  ║  │                           ▼                                     │  ║   │
│  ║  │   ┌─────────────────────────────────────────────────────────┐  │  ║   │
│  ║  │   │  DISPATCH QUEUE                                         │  │  ║   │
│  ║  │   │    - Ordered list ready for driver                      │  │  ║   │
│  ║  │   └───────────────────────┬─────────────────────────────────┘  │  ║   │
│  ║  │                           ▼                                     │  ║   │
│  ║  │   request_fn(q) ──────────────────────────────────────────────► │  ║   │
│  ║  └─────────────────────────────────────────────────────────────────┘  ║   │
│  ╚═══════════════════════════════╤═══════════════════════════════════════╝   │
│                                  │                                            │
│  ┌───────────────────────────────▼──────────────────────────────────────┐    │
│  │                    BLOCK DEVICE DRIVERS                               │    │
│  │    ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌───────────┐  │    │
│  │    │  SCSI/SATA  │  │    NVMe     │  │   virtio    │  │   loop    │  │    │
│  │    │  (sd_mod)   │  │  (nvme.ko)  │  │  (virtio_   │  │  (loop.ko)│  │    │
│  │    │             │  │             │  │   blk.ko)   │  │           │  │    │
│  │    └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └─────┬─────┘  │    │
│  └───────────│────────────────│────────────────│───────────────│────────┘    │
│              │                │                │               │              │
└──────────────│────────────────│────────────────│───────────────│──────────────┘
               │                │                │               │
═══════════════│════════════════│════════════════│═══════════════│══════════════
               ▼                ▼                ▼               ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                           HARDWARE                                            │
│        SATA HDD          NVMe SSD         Virtual Disk       File (loopback) │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Data Flow: Read Request Path

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       READ REQUEST PATH                                      │
│                                                                              │
│   1. Application: read(fd, buf, 4096)                                        │
│        │                                                                     │
│        ▼                                                                     │
│   2. VFS: vfs_read() → file->f_op->read()                                   │
│        │                                                                     │
│        ▼                                                                     │
│   3. Filesystem: ext4_file_read_iter() → generic_file_read_iter()           │
│        │                                                                     │
│        ▼                                                                     │
│   4. Page Cache: Check if page is cached                                     │
│        │                                                                     │
│        │  Cache HIT ─────────────► Return data immediately                  │
│        │                                                                     │
│        │  Cache MISS                                                         │
│        ▼                                                                     │
│   5. Create bio for disk read                                               │
│        │  - bio->bi_sector = target sector                                  │
│        │  - bio->bi_bdev = block device                                     │
│        │  - bio->bi_io_vec[] = page + offset + length                       │
│        │  - bio->bi_end_io = completion callback                            │
│        │                                                                     │
│        ▼                                                                     │
│   6. submit_bio(READ, bio)                                                  │
│        │                                                                     │
│        ▼                                                                     │
│   7. generic_make_request(bio)                                              │
│        │  - Partition remapping                                             │
│        │  - Size/bounds checking                                            │
│        │  - Throttling check                                                │
│        │                                                                     │
│        ▼                                                                     │
│   8. q->make_request_fn(q, bio)   [default: blk_queue_bio]                  │
│        │  - Try merge with existing request                                 │
│        │  - If no merge: allocate new request                               │
│        │  - Add request to elevator                                         │
│        │                                                                     │
│        ▼                                                                     │
│   9. Elevator: sort/merge/hold request                                      │
│        │                                                                     │
│        ▼                                                                     │
│  10. elv_dispatch() → move to dispatch queue                                │
│        │                                                                     │
│        ▼                                                                     │
│  11. q->request_fn(q)                                                       │
│        │  - Driver fetches request: blk_fetch_request(q)                    │
│        │  - Programs hardware DMA                                           │
│        │  - Starts transfer                                                 │
│        │                                                                     │
│        ▼                                                                     │
│  12. HARDWARE PERFORMS I/O                                                   │
│        │                                                                     │
│        ▼                                                                     │
│  13. IRQ: Hardware completion interrupt                                      │
│        │                                                                     │
│        ▼                                                                     │
│  14. blk_complete_request(rq) → softirq                                     │
│        │                                                                     │
│        ▼                                                                     │
│  15. __blk_end_request_all(rq, error)                                       │
│        │  - Calls bio->bi_end_io() for each bio                             │
│        │  - Unlocks page, marks uptodate                                    │
│        │                                                                     │
│        ▼                                                                     │
│  16. Wake up waiting process                                                 │
│                                                                              │
│  17. Return data to application                                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### How This Subsystem Interacts with Others

| Adjacent Subsystem | Interaction |
|-------------------|-------------|
| **VFS** | Block layer provides `block_device` for mounting filesystems |
| **Page Cache** | Cached reads bypass block layer; writebacks generate bios |
| **Filesystems** | Call `submit_bio()` for disk I/O |
| **Memory Management** | bio_vec references pages; DMA needs physical addresses |
| **Device Model** | gendisk integrates with `/sys/block/` |
| **SCSI Layer** | sd driver is a major consumer of block layer |
| **Device Mapper** | Stacks on block layer for LVM, RAID, encryption |

---

## 2. Directory & File Map (Code Navigation)

### Primary Directories

```
block/
├── blk-core.c                 → Core block layer functionality
│                                 - Request queue management
│                                 - submit_bio(), generic_make_request()
│                                 - Request allocation/completion
│                                 - blk_queue_bio() (default make_request_fn)
│
├── blk-merge.c                → Request merging logic
│                                 - blk_attempt_req_merge()
│                                 - ll_back_merge_fn(), ll_front_merge_fn()
│
├── blk-settings.c             → Queue limit configuration
│                                 - blk_set_default_limits()
│                                 - blk_queue_max_sectors()
│                                 - blk_queue_bounce_limit()
│
├── blk-softirq.c              → Request completion via softirq
│                                 - blk_complete_request()
│                                 - blk_done_softirq()
│
├── blk-exec.c                 → Synchronous request execution
│                                 - blk_execute_rq()
│
├── blk-flush.c                → Flush/FUA request handling
│                                 - blk_flush_complete_seq()
│
├── blk-timeout.c              → Request timeout handling
│                                 - blk_abort_request()
│
├── blk-tag.c                  → Tagged command queueing
│                                 - blk_queue_init_tags()
│
├── blk-map.c                  → Bio to segment mapping
│                                 - blk_rq_map_user()
│                                 - blk_rq_map_kern()
│
├── blk-ioc.c                  → I/O context management
│                                 - Per-process I/O tracking
│
├── blk-throttle.c             → Block I/O throttling (cgroups)
│
├── blk-sysfs.c                → Sysfs interface for queues
│                                 - /sys/block/sdX/queue/*
│
├── genhd.c                    → Generic disk (gendisk) management
│                                 - add_disk(), del_gendisk()
│                                 - Partition handling
│                                 - /sys/block/ registration
│
├── elevator.c                 → I/O scheduler framework
│                                 - elv_merge(), elv_dispatch_sort()
│                                 - Scheduler registration/switching
│
├── cfq-iosched.c              → CFQ scheduler implementation
│                                 - Complete Fair Queuing
│                                 - Per-process fairness
│
├── deadline-iosched.c         → Deadline scheduler
│                                 - Latency guarantees
│
├── noop-iosched.c             → NOOP scheduler
│                                 - Simple FIFO (for SSDs/VMs)
│
├── ioctl.c                    → Block device ioctls
│                                 - BLKGETSIZE, BLKSSZGET, etc.
│
├── scsi_ioctl.c               → SCSI passthrough ioctls
│
├── bsg.c                      → Block SCSI generic interface
│
└── blk.h                      → Internal block layer header

include/linux/
├── blkdev.h                   → Main block layer header
│                                 - struct request
│                                 - struct request_queue
│                                 - struct block_device_operations
│                                 - Request flags, queue flags
│
├── blk_types.h                → Bio and request type definitions
│                                 - struct bio
│                                 - struct bio_vec
│                                 - Request flags (REQ_*)
│
├── bio.h                      → Bio helper functions and macros
│                                 - bio_for_each_segment()
│                                 - bio_alloc(), bio_put()
│
├── genhd.h                    → Generic disk structures
│                                 - struct gendisk
│                                 - struct hd_struct (partitions)
│                                 - Partition flags
│
└── elevator.h                 → Elevator interface
                                  - struct elevator_ops
                                  - struct elevator_type

fs/
└── block_dev.c                → Block device file operations
                                  - blkdev_open(), blkdev_close()
                                  - struct block_device management
```

### Why Is the Code Split This Way?

1. **`blk-core.c`**: Central coordination — request lifecycle, submission
2. **`blk-merge.c`**: Merging is complex and heavily optimized
3. **`elevator.c`**: Scheduler framework — separates policy from mechanism
4. **`cfq-iosched.c`, etc.**: Each scheduler is a loadable module
5. **`genhd.c`**: Disk registration — separate from I/O handling
6. **`blk-sysfs.c`**: User interface — separated for maintainability

---

## 3. Core Data Structures

### 3.1 struct bio — Basic I/O Unit

**Location**: `include/linux/blk_types.h`

```c
struct bio {
    sector_t            bi_sector;      /* Device sector (512-byte units) */
    struct bio          *bi_next;       /* Request queue link */
    struct block_device *bi_bdev;       /* Target block device */
    unsigned long       bi_flags;       /* Status flags (BIO_UPTODATE, etc.) */
    unsigned long       bi_rw;          /* READ/WRITE + priority */
    
    unsigned short      bi_vcnt;        /* Number of bio_vecs */
    unsigned short      bi_idx;         /* Current index into bi_io_vec */
    
    unsigned int        bi_phys_segments; /* Segments after coalescing */
    unsigned int        bi_size;        /* Remaining I/O size (bytes) */
    
    unsigned int        bi_max_vecs;    /* Max bio_vecs we can hold */
    atomic_t            bi_cnt;         /* Reference count */
    
    struct bio_vec      *bi_io_vec;     /* Array of (page, offset, len) */
    
    bio_end_io_t        *bi_end_io;     /* Completion callback */
    void                *bi_private;    /* Caller's private data */
    
    bio_destructor_t    *bi_destructor; /* Destructor function */
    
    /* Inline bio_vecs to avoid separate allocation for small I/O */
    struct bio_vec      bi_inline_vecs[0];
};

/* A segment of the I/O */
struct bio_vec {
    struct page     *bv_page;       /* Page containing data */
    unsigned int    bv_len;         /* Length in bytes */
    unsigned int    bv_offset;      /* Offset within page */
};
```

**Memory Layout**:
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          struct bio                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│  bi_sector = 1048576    ← Starting sector (512B units)                      │
│  bi_bdev ────────────────► [struct block_device] (e.g., /dev/sda1)          │
│  bi_rw = READ | REQ_SYNC                                                     │
│  bi_vcnt = 2            ← Number of segments                                 │
│  bi_idx = 0             ← Current segment (for partial I/O)                  │
│  bi_size = 8192         ← Total bytes (decrements on partial completion)    │
│                                                                              │
│  bi_io_vec ─────────────► ┌────────────────────────────────────────┐        │
│                           │ bio_vec[0]:                             │        │
│                           │   bv_page ──► [Page A in page cache]    │        │
│                           │   bv_offset = 0                         │        │
│                           │   bv_len = 4096                         │        │
│                           ├────────────────────────────────────────┤        │
│                           │ bio_vec[1]:                             │        │
│                           │   bv_page ──► [Page B in page cache]    │        │
│                           │   bv_offset = 0                         │        │
│                           │   bv_len = 4096                         │        │
│                           └────────────────────────────────────────┘        │
│                                                                              │
│  bi_end_io ─────────────► mpage_end_io() [callback on completion]           │
│  bi_private ────────────► [filesystem private data]                         │
│                                                                              │
│  bi_inline_vecs[0..N]   ← Inline storage for small bios                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Allocation and Lifetime**:
```c
// Allocation
struct bio *bio = bio_alloc(GFP_KERNEL, nr_vecs);  // From mempool

// Add data pages
bio->bi_sector = start_sector;
bio->bi_bdev = bdev;
bio->bi_end_io = my_endio;
bio_add_page(bio, page, len, offset);

// Submit
submit_bio(READ, bio);

// Completion (called from softirq context)
void my_endio(struct bio *bio, int error) {
    // Process completion
    bio_put(bio);  // Release reference
}
```

### 3.2 struct request — Merged I/O Request

**Location**: `include/linux/blkdev.h`

```c
struct request {
    struct list_head    queuelist;      /* Link in queue */
    struct call_single_data csd;        /* For IPI completion */
    
    struct request_queue *q;            /* Containing queue */
    
    unsigned int        cmd_flags;      /* REQ_* flags */
    enum rq_cmd_type_bits cmd_type;     /* FS, BLOCK_PC, etc. */
    unsigned long       atomic_flags;   /* State flags */
    
    int                 cpu;            /* CPU that submitted */
    
    unsigned int        __data_len;     /* Total data length */
    sector_t            __sector;       /* Starting sector */
    
    struct bio          *bio;           /* First bio */
    struct bio          *biotail;       /* Last bio */
    
    struct hlist_node   hash;           /* For merge hash lookup */
    
    union {
        struct rb_node  rb_node;        /* For elevator sorting */
        void           *completion_data;
    };
    
    union {
        void *elevator_private[3];      /* Elevator private data */
        struct { /* Flush request data */
            unsigned int seq;
            struct list_head list;
            rq_end_io_fn *saved_end_io;
        } flush;
    };
    
    struct gendisk      *rq_disk;       /* Associated disk */
    struct hd_struct    *part;          /* Partition (for stats) */
    unsigned long       start_time;     /* For latency tracking */
    
    unsigned short      nr_phys_segments; /* DMA segments */
    unsigned short      ioprio;         /* I/O priority */
    
    int                 ref_count;      /* Reference count */
    int                 tag;            /* TCQ tag */
    int                 errors;         /* Error count */
    
    unsigned int        timeout;        /* Request timeout */
    rq_end_io_fn        *end_io;        /* Completion callback */
    void                *end_io_data;   /* Completion data */
};
```

**Request vs Bio Relationship**:
```
┌─────────────────────────────────────────────────────────────────────────────┐
│               REQUEST ←────── Multiple bios merged ──────→                  │
│                                                                              │
│   struct request                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  __sector = 1000                                                     │   │
│   │  __data_len = 16384  (4 pages)                                      │   │
│   │  cmd_flags = REQ_WRITE | REQ_SYNC                                   │   │
│   │                                                                      │   │
│   │  bio ────────┐                                                       │   │
│   │              │                                                       │   │
│   │              ▼                                                       │   │
│   │         ┌─────────┐     ┌─────────┐     ┌─────────┐                 │   │
│   │         │  bio 1  │────►│  bio 2  │────►│  bio 3  │                 │   │
│   │         │ 4096B   │     │ 4096B   │     │ 8192B   │                 │   │
│   │         │ sector  │     │ sector  │     │ sector  │                 │   │
│   │         │  1000   │     │  1008   │     │  1016   │                 │   │
│   │         └─────────┘     └─────────┘     └─────────┘                 │   │
│   │              ▲                               │                       │   │
│   │  biotail ────┘───────────────────────────────┘                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   Properties:                                                                │
│   - Contiguous sector range                                                  │
│   - Same direction (all READ or all WRITE)                                  │
│   - Same block device                                                        │
│   - Respects queue limits                                                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.3 struct request_queue — Per-Device Queue

**Location**: `include/linux/blkdev.h`

```c
struct request_queue {
    struct list_head    queue_head;     /* Dispatch queue */
    struct request      *last_merge;    /* Last merged request */
    struct elevator_queue *elevator;    /* I/O scheduler */
    
    struct request_list rq;             /* Request freelist */
    
    /* Callbacks */
    request_fn_proc     *request_fn;    /* Driver request handler */
    make_request_fn     *make_request_fn; /* Bio→request conversion */
    prep_rq_fn          *prep_rq_fn;    /* Request preparation */
    softirq_done_fn     *softirq_done_fn; /* Softirq completion */
    
    struct backing_dev_info backing_dev_info;
    void                *queuedata;     /* Driver private data */
    
    unsigned long       queue_flags;    /* QUEUE_FLAG_* */
    spinlock_t          __queue_lock;   /* Queue lock */
    spinlock_t          *queue_lock;    /* Pointer to lock */
    
    struct kobject      kobj;           /* For /sys/block/*/queue/ */
    
    unsigned long       nr_requests;    /* Max queued requests */
    unsigned int        nr_sorted;      /* Requests in elevator */
    unsigned int        in_flight[2];   /* Inflight [read, write] */
    
    struct queue_limits limits;         /* Hardware constraints */
    
    struct blk_queue_tag *queue_tags;   /* TCQ support */
    struct timer_list   timeout;        /* Request timeout timer */
    struct list_head    timeout_list;   /* Timed-out requests */
    
    /* Many more fields... */
};

struct queue_limits {
    unsigned long       bounce_pfn;         /* Max DMA address */
    unsigned long       seg_boundary_mask;  /* Segment boundary */
    
    unsigned int        max_hw_sectors;     /* Hardware max */
    unsigned int        max_sectors;        /* Soft max */
    unsigned int        max_segment_size;   /* Per-segment max */
    unsigned int        physical_block_size;
    unsigned int        io_min;             /* Minimum I/O size */
    unsigned int        io_opt;             /* Optimal I/O size */
    
    unsigned short      logical_block_size; /* Typically 512 */
    unsigned short      max_segments;       /* Max SG entries */
};
```

### 3.4 struct gendisk — Generic Disk

**Location**: `include/linux/genhd.h`

```c
struct gendisk {
    int                 major;          /* Major number */
    int                 first_minor;    /* First minor number */
    int                 minors;         /* Max partitions */
    
    char                disk_name[DISK_NAME_LEN]; /* "sda", "nvme0n1" */
    
    struct disk_part_tbl __rcu *part_tbl; /* Partition table */
    struct hd_struct    part0;          /* Whole disk "partition" */
    
    const struct block_device_operations *fops; /* Operations */
    struct request_queue *queue;        /* Associated queue */
    void                *private_data;  /* Driver private */
    
    int                 flags;          /* GENHD_FL_* */
    struct kobject      *slave_dir;     /* For device stacking */
    
    int                 node_id;        /* NUMA node */
};

/* Partition structure */
struct hd_struct {
    sector_t            start_sect;     /* Start sector */
    sector_t            nr_sects;       /* Size in sectors */
    struct device       __dev;          /* Embedded device */
    int                 partno;         /* Partition number */
    atomic_t            in_flight[2];   /* Inflight I/O */
    struct disk_stats __percpu *dkstats; /* Statistics */
};
```

### 3.5 struct block_device_operations — Driver Interface

**Location**: `include/linux/blkdev.h`

```c
struct block_device_operations {
    int (*open)(struct block_device *, fmode_t);
    int (*release)(struct gendisk *, fmode_t);
    int (*ioctl)(struct block_device *, fmode_t, unsigned, unsigned long);
    int (*compat_ioctl)(struct block_device *, fmode_t, unsigned, unsigned long);
    
    /* Direct access (for XIP, DAX) */
    int (*direct_access)(struct block_device *, sector_t, void **, unsigned long *);
    
    /* Media change detection */
    unsigned int (*check_events)(struct gendisk *, unsigned int);
    int (*media_changed)(struct gendisk *);  /* DEPRECATED */
    
    int (*revalidate_disk)(struct gendisk *);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
    
    struct module *owner;
};
```

### 3.6 struct elevator_ops — I/O Scheduler Interface

**Location**: `include/linux/elevator.h`

```c
struct elevator_ops {
    /* Merge a bio with existing request */
    elevator_merge_fn       *elevator_merge_fn;
    elevator_merged_fn      *elevator_merged_fn;
    elevator_merge_req_fn   *elevator_merge_req_fn;
    elevator_allow_merge_fn *elevator_allow_merge_fn;
    
    /* Dispatch requests to driver */
    elevator_dispatch_fn    *elevator_dispatch_fn;
    
    /* Add request to scheduler */
    elevator_add_req_fn     *elevator_add_req_fn;
    
    /* Request completion notification */
    elevator_completed_req_fn *elevator_completed_req_fn;
    
    /* Get adjacent requests */
    elevator_request_list_fn *elevator_former_req_fn;
    elevator_request_list_fn *elevator_latter_req_fn;
    
    /* Per-request data allocation */
    elevator_set_req_fn     *elevator_set_req_fn;
    elevator_put_req_fn     *elevator_put_req_fn;
    
    /* May queue check */
    elevator_may_queue_fn   *elevator_may_queue_fn;
    
    /* Init/exit */
    elevator_init_fn        *elevator_init_fn;
    elevator_exit_fn        *elevator_exit_fn;
};

struct elevator_type {
    struct list_head    list;           /* Global list */
    struct elevator_ops ops;            /* Operations */
    char elevator_name[ELV_NAME_MAX];   /* "cfq", "deadline", "noop" */
    struct module       *elevator_owner;
};
```

---

## 4. Entry Points & Call Paths

### 4.1 Block Device Registration

```
Driver initialization (e.g., sd_probe for SCSI disk):
       │
       ▼
alloc_disk(minors)                                    [block/genhd.c]
       │
       ├── Allocate struct gendisk
       ├── Allocate partition table
       └── Initialize part0
       │
       ▼
disk->major = major;
disk->first_minor = minor;
disk->fops = &my_fops;
disk->queue = blk_init_queue(request_fn, lock);       [block/blk-core.c]
       │
       ├── Allocate request_queue
       ├── Initialize elevator
       ├── Set queue limits
       └── Set request_fn
       │
       ▼
set_capacity(disk, nr_sectors);                       [include/linux/genhd.h]
       │
       ▼
add_disk(disk)                                        [block/genhd.c]
       │
       ├── blk_alloc_devt() — allocate dev_t
       ├── register_disk() — add to sysfs
       │     └── device_add() → /sys/block/sdX/
       ├── blk_register_queue() — /sys/block/sdX/queue/
       └── kobject_uevent(KOBJ_ADD) → udev creates /dev/sdX
```

### 4.2 I/O Submission Path

```
submit_bio(rw, bio)                                   [block/blk-core.c]
       │
       ├── Set bio->bi_rw flags
       ├── Account I/O statistics
       │
       ▼
generic_make_request(bio)                             [block/blk-core.c]
       │
       ├── generic_make_request_checks(bio)
       │     ├── Check sector bounds
       │     ├── Get request_queue from bio->bi_bdev
       │     ├── Partition remap: bio->bi_sector += partition start
       │     ├── Throttle check (cgroups)
       │     └── Integrity check
       │
       ├── Handle recursion via current->bio_list
       │
       ▼
q->make_request_fn(q, bio)                            [default: blk_queue_bio]
       │
       ├── Attempt merge with plug list
       │     └── attempt_plug_merge()
       │
       ├── Attempt merge with existing request
       │     └── elv_merge(q, &req, bio)
       │           ├── Check last_merge (cache)
       │           ├── Check hash table
       │           └── Call elevator_merge_fn
       │
       │  MERGE SUCCESS:
       │     └── bio_attempt_back_merge() or bio_attempt_front_merge()
       │           ├── Add bio to request
       │           ├── Update request->__data_len
       │           └── elv_bio_merged()
       │
       │  NO MERGE:
       │     ├── get_request() — allocate new request
       │     ├── init_request_from_bio() — copy bio info
       │     └── elv_insert() — add to elevator
       │
       └── Unplug if needed: __blk_run_queue()
```

### 4.3 Request Dispatch Path

```
__blk_run_queue(q)                                    [block/blk-core.c]
       │
       ├── Check if queue stopped/dead
       │
       ▼
__blk_run_queue_uncond(q)
       │
       ▼
q->request_fn(q)                                      [Driver's request handler]
       │
       │  Driver typically:
       │
       ▼
while ((rq = blk_fetch_request(q)) != NULL) {         [block/blk-core.c]
       │
       ├── blk_peek_request(q)
       │     ├── elv_next_request() — get from dispatch queue
       │     │     └── elevator_dispatch_fn() if queue empty
       │     ├── prep_rq_fn() — driver preparation
       │     └── Set REQ_STARTED flag
       │
       └── blk_start_request(rq) — update stats, start timeout
}
       │
       │  For each request:
       ▼
Driver processes request:
       ├── Build scatter-gather list from rq->bio chain
       ├── Program hardware DMA
       ├── Start transfer
       │
       ▼
Hardware interrupt on completion
       │
       ▼
blk_complete_request(rq)                              [block/blk-softirq.c]
       │
       ├── Raise softirq
       │
       ▼
blk_done_softirq()                                    [softirq context]
       │
       └── q->softirq_done_fn(rq)
             │
             ▼
__blk_end_request_all(rq, error)                      [block/blk-core.c]
       │
       ├── For each bio in request:
       │     └── bio->bi_end_io(bio, error)
       │
       └── Free request to pool
```

### 4.4 Elevator Dispatch (CFQ Example)

```
cfq_dispatch_requests(q, force)                       [block/cfq-iosched.c]
       │
       ├── Select service tree (sync/async)
       │
       ├── Select cfq_group (cgroup-aware)
       │
       ├── Select cfq_queue (per-process)
       │     └── Based on priority, time slice
       │
       ├── cfq_dispatch_insert()
       │     ├── Remove request from cfq_queue's RB-tree
       │     └── elv_dispatch_sort(q, rq) — add to dispatch queue
       │           └── Sorted by sector for seek optimization
       │
       └── Update statistics, time slice
```

---

## 5. Core Workflows (Code-Driven)

### 5.1 Block Device Initialization (Simple RAM Disk Example)

```c
static struct gendisk *my_disk;
static struct request_queue *my_queue;
static DEFINE_SPINLOCK(my_lock);
static unsigned char *my_data;

/* Request handler */
static void my_request_fn(struct request_queue *q)
{
    struct request *rq;
    
    while ((rq = blk_fetch_request(q)) != NULL) {
        struct bio_vec *bvec;
        struct req_iterator iter;
        sector_t sector = blk_rq_pos(rq);
        
        if (rq->cmd_type != REQ_TYPE_FS) {
            __blk_end_request_all(rq, -EIO);
            continue;
        }
        
        rq_for_each_segment(bvec, rq, iter) {
            char *buffer = kmap_atomic(bvec->bv_page);
            unsigned long offset = sector * 512;
            
            if (rq_data_dir(rq) == WRITE)
                memcpy(my_data + offset, buffer + bvec->bv_offset, bvec->bv_len);
            else
                memcpy(buffer + bvec->bv_offset, my_data + offset, bvec->bv_len);
            
            kunmap_atomic(buffer);
            sector += bvec->bv_len / 512;
        }
        
        __blk_end_request_all(rq, 0);
    }
}

/* Module init */
static int __init my_init(void)
{
    int ret;
    
    /* Allocate data buffer */
    my_data = vmalloc(MY_SIZE);
    if (!my_data)
        return -ENOMEM;
    
    /* Create request queue */
    my_queue = blk_init_queue(my_request_fn, &my_lock);
    if (!my_queue) {
        vfree(my_data);
        return -ENOMEM;
    }
    
    /* Set queue limits */
    blk_queue_logical_block_size(my_queue, 512);
    blk_queue_physical_block_size(my_queue, 512);
    
    /* Allocate gendisk */
    my_disk = alloc_disk(1);  /* 1 minor = no partitions */
    if (!my_disk) {
        blk_cleanup_queue(my_queue);
        vfree(my_data);
        return -ENOMEM;
    }
    
    /* Setup gendisk */
    my_disk->major = MY_MAJOR;
    my_disk->first_minor = 0;
    my_disk->fops = &my_fops;
    my_disk->private_data = NULL;
    my_disk->queue = my_queue;
    strcpy(my_disk->disk_name, "myblock");
    set_capacity(my_disk, MY_SIZE / 512);
    
    /* Register disk */
    add_disk(my_disk);
    
    return 0;
}
```

### 5.2 Bio Submission Flow

```c
/* Filesystem submitting a read */
static void fs_read_page(struct block_device *bdev, sector_t sector,
                         struct page *page)
{
    struct bio *bio;
    
    /* Allocate bio with 1 segment */
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio)
        return;
    
    /* Setup bio */
    bio->bi_sector = sector;
    bio->bi_bdev = bdev;
    bio->bi_end_io = read_end_io;
    bio->bi_private = page;
    
    /* Add page to bio */
    bio_add_page(bio, page, PAGE_SIZE, 0);
    
    /* Submit */
    submit_bio(READ, bio);
}

/* Completion callback (called in softirq context) */
static void read_end_io(struct bio *bio, int error)
{
    struct page *page = bio->bi_private;
    
    if (!error && test_bit(BIO_UPTODATE, &bio->bi_flags)) {
        SetPageUptodate(page);
    } else {
        ClearPageUptodate(page);
        SetPageError(page);
    }
    
    unlock_page(page);
    bio_put(bio);
}
```

### 5.3 Request Merging

```c
/* In blk-core.c: blk_queue_bio */
static void blk_queue_bio(struct request_queue *q, struct bio *bio)
{
    struct request *req;
    int el_ret;
    unsigned int request_count = 0;
    
    /* Try to merge with plug list first */
    if (attempt_plug_merge(q, bio, &request_count))
        return;
    
    spin_lock_irq(q->queue_lock);
    
    /* Try to find existing request to merge with */
    el_ret = elv_merge(q, &req, bio);
    
    switch (el_ret) {
    case ELEVATOR_BACK_MERGE:
        /* bio can be added to end of existing request */
        if (bio_attempt_back_merge(q, req, bio)) {
            elv_bio_merged(q, req, bio);
            goto out_unlock;
        }
        break;
        
    case ELEVATOR_FRONT_MERGE:
        /* bio can be added to beginning of existing request */
        if (bio_attempt_front_merge(q, req, bio)) {
            elv_bio_merged(q, req, bio);
            goto out_unlock;
        }
        break;
    }
    
    /* No merge possible, allocate new request */
    req = get_request(q, bio->bi_rw, bio, GFP_NOIO);
    if (!req) {
        /* Out of requests, sleep and retry */
        // ... handle blocking
    }
    
    /* Initialize request from bio */
    init_request_from_bio(req, bio);
    
    /* Add to elevator */
    __elv_add_request(q, req, ELEVATOR_INSERT_SORT);
    
out_unlock:
    spin_unlock_irq(q->queue_lock);
}
```

### 5.4 Request Completion Path

```c
/* Driver calls this when hardware completes */
void blk_complete_request(struct request *rq)
{
    struct request_queue *q = rq->q;
    
    /* Will be processed in softirq context */
    if (!q->softirq_done_fn) {
        /* Direct completion */
        blk_end_request_all(rq, rq->errors ? -EIO : 0);
    } else {
        /* Raise softirq for completion */
        raise_softirq_irqoff(BLOCK_SOFTIRQ);
        // rq added to per-CPU completion list
    }
}

/* Softirq handler */
static void blk_done_softirq(struct softirq_action *h)
{
    struct list_head *cpu_list = &__get_cpu_var(blk_cpu_done);
    
    while (!list_empty(cpu_list)) {
        struct request *rq = list_entry(cpu_list->next,
                                        struct request, csd.list);
        list_del_init(&rq->csd.list);
        rq->q->softirq_done_fn(rq);
    }
}

/* Actual completion work */
bool __blk_end_request(struct request *rq, int error, unsigned int nr_bytes)
{
    /* Update bio chain */
    while (rq->bio) {
        struct bio *bio = rq->bio;
        unsigned bio_bytes = min(bio->bi_size, nr_bytes);
        
        if (bio_bytes == bio->bi_size) {
            rq->bio = bio->bi_next;
        }
        
        /* Call bio completion */
        req_bio_endio(rq, bio, bio_bytes, error);
        
        nr_bytes -= bio_bytes;
        if (!nr_bytes)
            break;
    }
    
    /* Check if request is complete */
    if (!rq->bio) {
        /* Free request */
        __blk_put_request(rq->q, rq);
        return false;  /* Request done */
    }
    
    return true;  /* More to do */
}
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 I/O Schedulers Comparison

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     I/O SCHEDULER COMPARISON                                 │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ NOOP (No-Operation)                                                 │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ Algorithm: Simple FIFO queue with basic merging                     │   │
│   │                                                                      │   │
│   │   incoming bio ───► [merge check] ───► FIFO Queue ───► dispatch    │   │
│   │                                                                      │   │
│   │ Best for: SSDs, VMs (host handles scheduling)                       │   │
│   │ Pros: Minimal CPU overhead, low latency                             │   │
│   │ Cons: No seek optimization for HDDs                                 │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ DEADLINE                                                             │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ Algorithm: Two RB-trees (sorted by sector) + two FIFO queues        │   │
│   │            with deadlines (read: 500ms, write: 5s)                  │   │
│   │                                                                      │   │
│   │   ┌──────────────┐    ┌──────────────┐                              │   │
│   │   │  Read Queue  │    │ Write Queue  │                              │   │
│   │   │   (RB-tree)  │    │   (RB-tree)  │                              │   │
│   │   └──────┬───────┘    └──────┬───────┘                              │   │
│   │          │                   │                                       │   │
│   │   ┌──────▼───────┐    ┌──────▼───────┐                              │   │
│   │   │ Read FIFO    │    │ Write FIFO   │                              │   │
│   │   │ (by deadline)│    │ (by deadline)│                              │   │
│   │   └──────────────┘    └──────────────┘                              │   │
│   │                                                                      │   │
│   │ Dispatch: Sector-sorted unless deadline expired                     │   │
│   │ Best for: Databases, latency-sensitive workloads                    │   │
│   │ Pros: Bounded latency, good throughput                              │   │
│   │ Cons: No per-process fairness                                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ CFQ (Complete Fair Queuing)                                         │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │ Algorithm: Per-process queues with time slices                      │   │
│   │                                                                      │   │
│   │   Process A ───► cfq_queue_A (RB-tree by sector)                    │   │
│   │   Process B ───► cfq_queue_B (RB-tree by sector)                    │   │
│   │   Process C ───► cfq_queue_C (RB-tree by sector)                    │   │
│   │                        │                                             │   │
│   │                        ▼                                             │   │
│   │              ┌─────────────────┐                                    │   │
│   │              │  Service Tree   │  (schedules queues by vdisktime)   │   │
│   │              │   (RB-tree)     │                                    │   │
│   │              └─────────────────┘                                    │   │
│   │                        │                                             │   │
│   │   Each queue gets time slice (e.g., 100ms)                          │   │
│   │   Dispatches from active queue's RB-tree                            │   │
│   │                                                                      │   │
│   │ Best for: Desktop, multi-user, mixed workloads                      │   │
│   │ Pros: Fair bandwidth allocation, priority support                   │   │
│   │ Cons: Higher CPU overhead, not ideal for SSDs                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Request Merging Algorithm

```c
/* Merge decision logic */
int elv_merge(struct request_queue *q, struct request **req, struct bio *bio)
{
    struct elevator_queue *e = q->elevator;
    struct request *__rq;
    
    /* Check cached last merge first (hot path) */
    if (q->last_merge) {
        int type = elv_try_merge(q->last_merge, bio);
        if (type != ELEVATOR_NO_MERGE) {
            *req = q->last_merge;
            return type;
        }
    }
    
    /* Check hash table for back-merge by end sector */
    __rq = elv_rqhash_find(q, bio->bi_sector);
    if (__rq && elv_rq_merge_ok(__rq, bio)) {
        *req = __rq;
        return ELEVATOR_BACK_MERGE;
    }
    
    /* Ask elevator for front-merge (more expensive) */
    if (e->ops->elevator_merge_fn)
        return e->ops->elevator_merge_fn(q, req, bio);
    
    return ELEVATOR_NO_MERGE;
}

int elv_try_merge(struct request *rq, struct bio *bio)
{
    /* Back merge: bio starts where request ends */
    if (blk_rq_pos(rq) + blk_rq_sectors(rq) == bio->bi_sector)
        return ELEVATOR_BACK_MERGE;
    
    /* Front merge: bio ends where request starts */
    if (blk_rq_pos(rq) - bio_sectors(bio) == bio->bi_sector)
        return ELEVATOR_FRONT_MERGE;
    
    return ELEVATOR_NO_MERGE;
}
```

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         MERGE SCENARIOS                                      │
│                                                                              │
│   BACK MERGE (most common):                                                  │
│   ─────────────────────────                                                  │
│                                                                              │
│   Existing request:  [sector 100-199]                                        │
│   New bio:                          [sector 200-299]                         │
│                                      ▲                                       │
│                                      │                                       │
│                              bio->bi_sector == rq end sector                │
│                                                                              │
│   Result:            [sector 100-299]  ← Single request                     │
│                                                                              │
│   FRONT MERGE:                                                               │
│   ─────────────────                                                          │
│                                                                              │
│   Existing request:           [sector 200-299]                               │
│   New bio:           [sector 100-199]                                        │
│                              ▲                                               │
│                              │                                               │
│                    bio end sector == rq->bi_sector                          │
│                                                                              │
│   Result:            [sector 100-299]  ← Single request                     │
│                                                                              │
│   WHY MERGE?                                                                 │
│   ─────────────────                                                          │
│   1. Fewer interrupts (one completion vs multiple)                           │
│   2. Better disk utilization (one seek, continuous transfer)                 │
│   3. Reduced request overhead                                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.3 Plugging Mechanism

Plugging batches bios before submitting to the elevator:

```c
/* Per-task plug */
struct blk_plug {
    struct list_head list;      /* Pending requests */
    struct list_head cb_list;   /* Callbacks */
    unsigned int should_sort;
};

/* Start plugging */
void blk_start_plug(struct blk_plug *plug)
{
    INIT_LIST_HEAD(&plug->list);
    INIT_LIST_HEAD(&plug->cb_list);
    current->plug = plug;
}

/* Flush plug (unplug) */
void blk_finish_plug(struct blk_plug *plug)
{
    if (plug != current->plug)
        return;
    
    /* Sort requests by sector for better merging */
    list_sort(NULL, &plug->list, plug_rq_cmp);
    
    /* Flush all requests to their queues */
    flush_plug_callbacks(plug);
    
    current->plug = NULL;
}
```

Usage pattern:
```c
struct blk_plug plug;

blk_start_plug(&plug);

/* Submit multiple bios */
submit_bio(READ, bio1);
submit_bio(READ, bio2);
submit_bio(READ, bio3);
/* Bios are batched in plug list, sorted, merged */

blk_finish_plug(&plug);
/* Now all bios go to elevator together */
```

---

## 7. Concurrency & Synchronization

### 7.1 Lock Summary

| Lock | Type | Protects | Scope |
|------|------|----------|-------|
| `q->queue_lock` | Spinlock | Request queue state, dispatch queue, elevator | Per-queue |
| `blk_alloc_lock` | Mutex | Block device major allocation | Global |
| `block_class_lock` | Mutex | Block class registration | Global |
| `ext_devt_mutex` | Mutex | Extended device number allocation | Global |
| `elv_list_lock` | Spinlock | Elevator type list | Global |
| `ioc->lock` | Spinlock | I/O context state | Per-process |

### 7.2 Queue Lock Critical Sections

```c
/* Most operations require queue_lock */

void blk_queue_bio(struct request_queue *q, struct bio *bio)
{
    /* ... plug list processing (lockless) ... */
    
    spin_lock_irq(q->queue_lock);
    
    /* Protected operations: */
    elv_merge(q, &req, bio);        /* Elevator lookup */
    get_request(q, ...);             /* Request allocation */
    __elv_add_request(q, req, ...);  /* Add to elevator */
    __blk_run_queue(q);              /* Trigger dispatch */
    
    spin_unlock_irq(q->queue_lock);
}

void __blk_run_queue(struct request_queue *q)
{
    /* Called with queue_lock held */
    
    if (unlikely(blk_queue_stopped(q)))
        return;
    
    q->request_fn(q);  /* Driver's handler */
}

/* Driver fetching requests */
struct request *blk_fetch_request(struct request_queue *q)
{
    /* Called with queue_lock held */
    
    struct request *rq = blk_peek_request(q);
    if (rq)
        blk_start_request(rq);
    return rq;
}
```

### 7.3 IRQ Context Considerations

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                  CONTEXT TRANSITIONS IN BLOCK I/O                           │
│                                                                              │
│   Process Context                                                            │
│   ───────────────                                                            │
│   submit_bio()                                                               │
│        │  can sleep (bio_alloc with GFP_KERNEL)                             │
│        │                                                                     │
│        ▼                                                                     │
│   generic_make_request()                                                     │
│        │  might_sleep() check                                               │
│        │                                                                     │
│        ▼                                                                     │
│   blk_queue_bio()                                                            │
│        │  spin_lock_irq(q->queue_lock) — disables IRQs                      │
│        │                                                                     │
│        ▼                                                                     │
│   request_fn(q)                                                              │
│        │  Still holding queue_lock, IRQs disabled                           │
│        │  Driver programs hardware                                          │
│        │                                                                     │
│        ▼                                                                     │
│   spin_unlock_irq() — enables IRQs                                          │
│                                                                              │
│   ═══════════════════════════════════════════════════════════════════════   │
│                                                                              │
│   Hardware Interrupt Context                                                 │
│   ──────────────────────────                                                 │
│   IRQ handler                                                                │
│        │  Cannot sleep!                                                      │
│        │                                                                     │
│        ▼                                                                     │
│   blk_complete_request(rq)                                                   │
│        │  Raises BLOCK_SOFTIRQ                                              │
│        │  Returns from IRQ quickly                                          │
│                                                                              │
│   ═══════════════════════════════════════════════════════════════════════   │
│                                                                              │
│   Softirq Context                                                            │
│   ──────────────                                                             │
│   blk_done_softirq()                                                         │
│        │  Cannot sleep! BH disabled                                         │
│        │                                                                     │
│        ▼                                                                     │
│   __blk_end_request()                                                        │
│        │                                                                     │
│        ▼                                                                     │
│   bio->bi_end_io(bio, error)                                                │
│        │  Completion callback                                               │
│        │  Wakes up waiting process                                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 7.4 Race Prevention

**Double submission prevention**:
```c
void generic_make_request(struct bio *bio)
{
    /* Use per-task bio_list to prevent stack overflow */
    if (current->bio_list) {
        /* Already in make_request, just queue for later */
        bio_list_add(current->bio_list, bio);
        return;
    }
    
    /* First entry, process bio and any recursively submitted bios */
    bio_list_init(&bio_list_on_stack);
    current->bio_list = &bio_list_on_stack;
    do {
        q->make_request_fn(q, bio);
        bio = bio_list_pop(current->bio_list);
    } while (bio);
    current->bio_list = NULL;
}
```

**Request completion race**:
```c
/* In driver IRQ handler */
void my_irq_handler(int irq, void *dev_id)
{
    struct request *rq = get_completed_request();
    
    /* Must use atomic test to prevent double completion */
    if (!test_and_set_bit(REQ_ATOM_COMPLETE, &rq->atomic_flags))
        blk_complete_request(rq);
}
```

---

## 8. Performance Considerations

### 8.1 Hot Paths vs Cold Paths

| Path | Frequency | Optimization |
|------|-----------|--------------|
| `submit_bio()` | Every I/O | Minimize locking, use plugging |
| `blk_queue_bio()` | Every I/O | Cache last_merge, hash lookup |
| `request_fn()` | Per-request | Batch processing |
| `bio->bi_end_io()` | Every I/O | Softirq batching |
| `add_disk()` | Device probe | Cold, can sleep |
| Elevator switch | Manual | Very rare |

### 8.2 Cacheline Optimization

```c
/* struct request layout considerations */
struct request {
    /* Hot fields together for cache efficiency */
    struct list_head queuelist;     /* Frequently accessed */
    struct request_queue *q;
    unsigned int cmd_flags;
    unsigned int __data_len;
    sector_t __sector;
    struct bio *bio;
    struct bio *biotail;
    /* ... */
};

/* Comment from blkdev.h: */
/*
 * try to put the fields that are referenced together in the same cacheline.
 * if you modify this structure, be sure to check block/blk-core.c:blk_rq_init()
 * as well!
 */
```

### 8.3 Per-CPU Optimization

```c
/* Per-CPU completion lists */
static DEFINE_PER_CPU(struct list_head, blk_cpu_done);

/* Avoid cross-CPU cache bouncing */
void blk_complete_request(struct request *rq)
{
    struct list_head *list = &get_cpu_var(blk_cpu_done);
    list_add_tail(&rq->csd.list, list);
    put_cpu_var(blk_cpu_done);
    raise_softirq_irqoff(BLOCK_SOFTIRQ);
}
```

### 8.4 Scalability Limits in v3.2

1. **Single queue per device**: All CPUs contend on same queue_lock
2. **No multiqueue support**: Added in 3.13+ (blk-mq)
3. **Elevator complexity**: CFQ has significant per-request overhead
4. **Global elevator list**: Single lock for scheduler registration

---

## 9. Common Pitfalls & Bugs

### 9.1 Missing Error Handling in bio Completion

```c
/* BAD: Ignoring error */
static void bad_endio(struct bio *bio, int error)
{
    struct page *page = bio->bi_private;
    SetPageUptodate(page);  /* Wrong if error! */
    unlock_page(page);
    bio_put(bio);
}

/* GOOD: Proper error handling */
static void good_endio(struct bio *bio, int error)
{
    struct page *page = bio->bi_private;
    
    if (!error && test_bit(BIO_UPTODATE, &bio->bi_flags)) {
        SetPageUptodate(page);
    } else {
        ClearPageUptodate(page);
        SetPageError(page);
    }
    
    unlock_page(page);
    bio_put(bio);
}
```

### 9.2 Forgetting bio_put After Completion

```c
/* BAD: Memory leak */
static void leaky_endio(struct bio *bio, int error)
{
    /* Process completion */
    /* Missing bio_put(bio)! */
}

/* GOOD */
static void correct_endio(struct bio *bio, int error)
{
    /* Process completion */
    bio_put(bio);  /* Release bio back to pool */
}
```

### 9.3 Accessing bio After submit_bio

```c
/* BAD: Use after submit */
submit_bio(READ, bio);
sector = bio->bi_sector;  /* CRASH: bio might be freed! */

/* GOOD: Save data before submit */
sector = bio->bi_sector;
submit_bio(READ, bio);
/* bio is now owned by block layer */
```

### 9.4 Holding queue_lock Too Long

```c
/* BAD: Long critical section */
spin_lock_irq(q->queue_lock);
while ((rq = blk_fetch_request(q)) != NULL) {
    do_slow_operation(rq);  /* Don't do this! */
    __blk_end_request_all(rq, 0);
}
spin_unlock_irq(q->queue_lock);

/* GOOD: Minimize lock hold time */
spin_lock_irq(q->queue_lock);
rq = blk_fetch_request(q);
spin_unlock_irq(q->queue_lock);

if (rq) {
    do_slow_operation(rq);  /* Outside lock */
    blk_end_request_all(rq, 0);
}
```

### 9.5 Historical Issues in v3.2

1. **No blk-mq**: Single queue bottleneck for NVMe devices
2. **Bio split complexity**: Fixed in later versions
3. **Plug list sorting**: Could cause priority inversion
4. **CFQ starvation**: Some workloads could starve others

---

## 10. How to Read This Code Yourself

### 10.1 Recommended Reading Order

1. **`include/linux/blk_types.h`**: Start with `struct bio` and `struct bio_vec`

2. **`include/linux/blkdev.h`**: 
   - `struct request`
   - `struct request_queue`
   - `struct block_device_operations`

3. **`block/blk-core.c`**:
   - `blk_rq_init()` — Request initialization
   - `submit_bio()` — Entry point
   - `generic_make_request()` — Bio processing
   - `blk_queue_bio()` — Default make_request_fn

4. **`include/linux/genhd.h`**: `struct gendisk`, `struct hd_struct`

5. **`block/genhd.c`**:
   - `add_disk()` — Device registration
   - `register_disk()` — Sysfs setup

6. **`include/linux/elevator.h`**: `struct elevator_ops`

7. **`block/elevator.c`**:
   - `elv_merge()` — Merge logic
   - `elv_dispatch_sort()` — Dispatch ordering

8. **`block/noop-iosched.c`**: Simplest scheduler (good learning target)

9. **`block/deadline-iosched.c`**: More complex but well-documented

### 10.2 What to Ignore Initially

- **blk-cgroup.c**: Cgroup integration (complex)
- **blk-integrity.c**: Data integrity (specialized)
- **bsg.c**: SCSI passthrough
- **cfq-iosched.c**: Very complex (3000+ lines)

### 10.3 Useful Search Commands

```bash
# Find all request_fn implementations
grep -r "\.request_fn\s*=" drivers/block/

# Find make_request_fn implementations (for stacking drivers)
grep -r "\.make_request_fn\s*=" drivers/

# Find block_device_operations definitions
grep -r "struct block_device_operations.*=" drivers/

# Find bio allocation
grep -r "bio_alloc\|bio_alloc_bioset" fs/ drivers/

# Find elevator registration
grep -r "elv_register\|elevator_type" block/

# Trace submit_bio callers
cscope -d
# Search: submit_bio
```

### 10.4 Debugging Tips

```bash
# Enable block tracing
echo 1 > /sys/block/sda/trace/enable
cat /sys/kernel/debug/tracing/trace

# Block device statistics
cat /sys/block/sda/stat
cat /proc/diskstats

# Queue information
ls /sys/block/sda/queue/
cat /sys/block/sda/queue/scheduler

# Change I/O scheduler
echo deadline > /sys/block/sda/queue/scheduler

# blktrace for detailed I/O analysis
blktrace -d /dev/sda -o trace
blkparse -i trace
```

---

## 11. Summary & Mental Model

### One-Paragraph Summary

The Linux Block Layer is a sophisticated I/O stack that transforms random filesystem requests into optimized device I/O. A **bio** represents a single I/O operation (sector range + page list + completion callback); multiple bios are merged into a **request** when they cover adjacent sectors. Requests are managed by a **request_queue** associated with each **gendisk** (physical disk). Pluggable **I/O schedulers** (CFQ, Deadline, NOOP) sort and batch requests to minimize seeks on HDDs or maximize throughput on SSDs. The **request_fn** callback dispatches requests to device drivers, which program hardware DMA. Completion flows back through softirqs, calling **bio->bi_end_io** to notify the original submitters. The entire stack is designed for high throughput while providing fair access across processes and bounded latency for critical I/O.

### Key Invariants

1. **Bio ownership transfers on submit**: After `submit_bio()`, caller must not touch bio
2. **Request completion must be idempotent**: Use atomic flags to prevent double completion
3. **Queue lock protects queue state**: Most queue operations need `queue_lock`
4. **Merging preserves direction**: Only same-direction (read/read or write/write) requests merge
5. **Completion context is softirq**: `bi_end_io` callbacks cannot sleep

### Mental Model

Think of the block layer as a **smart post office**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     THE BLOCK LAYER POST OFFICE                             │
│                                                                              │
│   1. LETTERS (bios) arrive from customers (filesystems)                     │
│      - Each letter has destination (sector), contents (pages), return      │
│        address (bi_end_io)                                                  │
│                                                                              │
│   2. MAIL ROOM (make_request_fn) sorts incoming mail                        │
│      - Combines letters to same street (merging)                            │
│      - Puts in proper mailbag (request)                                     │
│                                                                              │
│   3. SORTING FACILITY (elevator) organizes delivery route                   │
│      - Groups by neighborhood (sector locality)                             │
│      - Balances priority and fairness (CFQ)                                │
│      - Ensures deadlines met (Deadline)                                     │
│                                                                              │
│   4. MAIL TRUCK (request_fn) makes deliveries                               │
│      - Driver gets next mailbag (blk_fetch_request)                        │
│      - Drives to destination (hardware DMA)                                 │
│                                                                              │
│   5. DELIVERY CONFIRMATION (completion)                                     │
│      - Hardware signals done (IRQ)                                          │
│      - Confirmation sent back (bi_end_io)                                  │
│      - Customer notified (process wakeup)                                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. What to Study Next

### Recommended Learning Order

| Order | Subsystem | Why It Matters |
|-------|-----------|----------------|
| 1 | **SCSI Layer** (`drivers/scsi/`) | Major consumer of block layer |
| 2 | **Device Mapper** (`drivers/md/dm*.c`) | Stacking, LVM, encryption |
| 3 | **blk-mq** (kernel ≥3.13) | Modern multi-queue architecture |
| 4 | **NVMe Driver** (`drivers/nvme/`) | High-performance block driver |
| 5 | **Page Cache** (`mm/filemap.c`) | Connects filesystem to block |
| 6 | **VFS Block Interface** (`fs/block_dev.c`) | Block device file operations |

### Related Files for Further Study

**Device Drivers**:
- `drivers/block/loop.c` — Simple loopback driver (good learning example)
- `drivers/block/null_blk.c` — Null block device (for testing)
- `drivers/block/virtio_blk.c` — Virtio block driver

**Advanced Topics**:
- `block/blk-mq.c` (≥3.13) — Multi-queue block layer
- `drivers/md/dm.c` — Device mapper core
- `drivers/md/raid*.c` — Software RAID

**Integration Points**:
- `fs/buffer.c` — Buffer cache (older interface)
- `mm/page-writeback.c` — Dirty page flushing
- `fs/direct-io.c` — Direct I/O (bypasses page cache)

---

## Appendix A: Key Functions Quick Reference

### Bio Operations
```c
bio_alloc(gfp_mask, nr_vecs)            // Allocate bio
bio_put(bio)                             // Release reference
bio_add_page(bio, page, len, offset)     // Add page to bio
bio_endio(bio, error)                    // Complete bio
submit_bio(rw, bio)                      // Submit to block layer
```

### Request Operations
```c
blk_fetch_request(q)                     // Get next request (holds lock)
blk_peek_request(q)                      // Peek next request
blk_start_request(rq)                    // Mark request started
blk_end_request_all(rq, error)           // Complete entire request
blk_end_request(rq, error, nr_bytes)     // Partial completion
blk_requeue_request(q, rq)               // Put back in queue
```

### Queue Operations
```c
blk_init_queue(request_fn, lock)         // Create queue
blk_cleanup_queue(q)                     // Destroy queue
blk_run_queue(q)                         // Trigger request_fn
blk_stop_queue(q)                        // Pause queue
blk_start_queue(q)                       // Resume queue
```

### Disk Operations
```c
alloc_disk(minors)                       // Allocate gendisk
add_disk(disk)                           // Register disk
del_gendisk(disk)                        // Unregister disk
put_disk(disk)                           // Release reference
set_capacity(disk, sectors)              // Set disk size
```

### Queue Limits
```c
blk_queue_logical_block_size(q, size)    // Set logical block size
blk_queue_physical_block_size(q, size)   // Set physical block size
blk_queue_max_hw_sectors(q, sectors)     // Set max HW sectors
blk_queue_max_segments(q, segs)          // Set max SG segments
blk_queue_bounce_limit(q, dma_mask)      // Set bounce buffer limit
```

---

*Document generated for Linux kernel v3.2. Some details may differ in other versions, especially regarding blk-mq which was introduced in v3.13.*

