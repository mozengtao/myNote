# Case 4: Block Layer I/O Submission

## Subsystem Context

```
+=============================================================================+
|                      BLOCK LAYER ARCHITECTURE                                |
+=============================================================================+

                           USER SPACE
    +----------------------------------------------------------+
    |   Application:  read(fd, buf, 4096)                      |
    |                 write(fd, buf, 4096)                     |
    +----------------------------------------------------------+
                              |
                              | System Call
                              v
    +----------------------------------------------------------+
    |                    VFS LAYER                              |
    |          generic_file_read() / generic_file_write()      |
    +----------------------------------------------------------+
                              |
                              | Page Cache Miss
                              v
    +----------------------------------------------------------+
    |                    BLOCK LAYER                            |
    |  +--------------------------------------------------+    |
    |  |  submit_bio()  <-- TEMPLATE METHOD               |    |
    |  |                                                  |    |
    |  |  1. Validate bio                                 |    |
    |  |  2. Update statistics                            |    |
    |  |  3. Check device state                           |    |
    |  |  4. Merge/plug optimization                      |    |
    |  |  5. CALL make_request_fn() --------------+       |    |
    |  |  6. Handle completion callback           |       |    |
    |  +------------------------------------------|-------+    |
    +--------------------------------------------------|---------+
                                                       |
                                                       v
    +----------------------------------------------------------+
    |                    DEVICE LAYER                           |
    |  +----------------+  +----------------+  +---------------+|
    |  |   SCSI disk    |  |    NVMe        |  |   virtio-blk  ||
    |  | scsi_request_fn|  | nvme_make_rq   |  | virtblk_make  ||
    |  +----------------+  +----------------+  +---------------+|
    +----------------------------------------------------------+
                              |
                              v
                      [ Physical Storage ]
```

**中文说明：**

块层是Linux内核中管理块设备I/O的子系统。用户空间的文件读写经过VFS层，在页缓存未命中时进入块层。`submit_bio()`是模板方法：它验证bio、更新统计、检查设备状态、执行合并/插塞优化，然后调用设备特定的`make_request_fn()`实际提交I/O，最后处理完成回调。不同存储设备（SCSI磁盘、NVMe、virtio-blk等）只需实现I/O提交逻辑。

---

## The Template Method: submit_bio()

### Components

| Component | Role |
|-----------|------|
| **Template Method** | `submit_bio()` |
| **Fixed Steps** | Bio validation, accounting, plugging, completion |
| **Variation Point** | `q->make_request_fn()` |
| **Ops Table** | `struct request_queue` with function pointer |

### Control Flow Diagram

```
    submit_bio(rw, bio)
    ===================

    +----------------------------------+
    |  1. VALIDATE BIO                 |
    |     - Check bio is valid         |
    |     - Check sector alignment     |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  2. GET REQUEST QUEUE            |
    |     - bio->bi_bdev->bd_disk      |
    |     - Get queue from block dev   |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  3. UPDATE ACCOUNTING            |
    |     - task_io_account_read/write |
    |     - Per-task I/O statistics    |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  4. BIO TRACING                  |
    |     - trace_block_bio_queue      |
    |     - blktrace infrastructure    |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  5. PLUGGING CHECK               |
    |     - blk_queue_bio()            |
    |     - Merge with existing reqs   |
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  6. VARIATION POINT                    ||
    ||     q->make_request_fn(q, bio)         ||
    ||     Device handles bio                 ||
    +==========================================+
                   |
                   v
    +----------------------------------+
    |  7. (Async) COMPLETION           |
    |     - bio->bi_end_io callback    |
    |     - Called when I/O done       |
    +----------------------------------+
```

**中文说明：**

`submit_bio()`的控制流：(1) 验证bio有效性和扇区对齐；(2) 从块设备获取请求队列；(3) 更新I/O统计（每任务统计）；(4) bio追踪（blktrace基础设施）；(5) 插塞检查和请求合并优化；(6) **变化点**——调用设备的`make_request_fn()`处理bio；(7) 异步完成——I/O完成时调用`bio->bi_end_io`回调。

---

## Why Template Method is Required Here

### 1. I/O Ordering Must Be Centralized

```
    I/O ORDERING REQUIREMENTS:

    +----------------------------------------------------------+
    |   Application issues:                                    |
    |   write(block 100)  --> must complete before             |
    |   write(block 100)  --> this second write                |
    +----------------------------------------------------------+

    WITHOUT CENTRALIZED ORDERING:
    +----------------+     +----------------+
    | write(100) #1  |     | write(100) #2  |
    +----------------+     +----------------+
           |                      |
           v                      v
    +----------------+     +----------------+
    | Driver queue 1 |     | Driver queue 2 |
    +----------------+     +----------------+
           |                      |
           +----------+-----------+
                      |
                      v
              [ WHICH WINS? ]

    WITH TEMPLATE METHOD:
    +----------------------------------------------------------+
    | submit_bio() ensures:                                    |
    | - Requests queued in order                               |
    | - Barriers respected                                     |
    | - No reordering across barriers                          |
    +----------------------------------------------------------+
```

**中文说明：**

I/O顺序必须集中管理。如果没有集中控制，对同一块的两次写入可能乱序完成。`submit_bio()`确保：请求按顺序排队、屏障被尊重、不会跨越屏障重排。这对数据完整性至关重要。

### 2. Plugging and Merging Optimization

```
    BIO MERGING (Framework Optimization):

    Application                     After Merging
    ===========                     =============

    bio: sector 0-7                 +------------------+
    bio: sector 8-15    ------->    | bio: sector 0-23 |  Single I/O!
    bio: sector 16-23               +------------------+

    BENEFITS:
    - Fewer hardware operations
    - Better sequential throughput
    - Less interrupt overhead

    WHY FRAMEWORK MUST DO THIS:
    - Needs global view of pending I/Os
    - Driver doesn't see other requests
    - Merging algorithm is complex
```

### 3. Accounting Must Be Complete

```
    I/O ACCOUNTING POINTS:

    submit_bio() {
        /* Submission accounting */
        task_io_account_write(bio_sectors(bio));  <-- Track bytes
        
        /* Trace point for blktrace */
        trace_block_bio_queue(q, bio);            <-- Debug/trace
        
        q->make_request_fn(q, bio);
    }

    bio_endio() {
        /* Completion accounting */
        trace_block_bio_complete(q, bio);         <-- Track complete
        
        bio->bi_end_io(bio, error);
    }

    IF DRIVERS DID THIS:
    - Inconsistent accounting
    - Some drivers might forget
    - Tracing would have gaps
```

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL BLOCK LAYER TEMPLATE METHOD SIMULATION
 * 
 * Demonstrates the Template Method pattern in block I/O submission.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct bio;
struct request_queue;
struct block_device;

/* ==========================================================
 * BIO: Block I/O descriptor
 * ========================================================== */
typedef void (*bio_end_io_t)(struct bio *bio, int error);

struct bio {
    unsigned long bi_sector;       /* Starting sector */
    unsigned int bi_size;          /* Size in bytes */
    int bi_rw;                     /* READ or WRITE */
    struct block_device *bi_bdev;  /* Target device */
    bio_end_io_t bi_end_io;        /* Completion callback */
    void *bi_private;              /* Caller's private data */
};

#define READ  0
#define WRITE 1

/* ==========================================================
 * REQUEST QUEUE: Per-device queue with strategy function
 * ========================================================== */
typedef void (*make_request_fn_t)(struct request_queue *q, 
                                   struct bio *bio);

struct request_queue {
    make_request_fn_t make_request_fn;  /* Device strategy */
    const char *queue_name;
    int queue_depth;
    
    /* Statistics */
    unsigned long requests_submitted;
    unsigned long sectors_read;
    unsigned long sectors_written;
};

/* ==========================================================
 * BLOCK DEVICE: Represents a block device
 * ========================================================== */
struct gendisk {
    const char *disk_name;
    struct request_queue *queue;
};

struct block_device {
    struct gendisk *bd_disk;
};

/* ==========================================================
 * FRAMEWORK FIXED STEPS (Block Core)
 * ========================================================== */

static void task_io_account_read(unsigned int bytes)
{
    printf("  [BLK] Accounting: read %u bytes\n", bytes);
}

static void task_io_account_write(unsigned int bytes)
{
    printf("  [BLK] Accounting: write %u bytes\n", bytes);
}

static void trace_block_bio_queue(struct request_queue *q, 
                                   struct bio *bio)
{
    printf("  [BLK] Trace: bio queued to %s, sector=%lu, size=%u\n",
           q->queue_name, bio->bi_sector, bio->bi_size);
}

static int bio_sectors(struct bio *bio)
{
    return bio->bi_size / 512;
}

/* Simulated plugging (simplified) */
static int try_merge_bio(struct request_queue *q, struct bio *bio)
{
    /* In real kernel: try to merge with existing request */
    printf("  [BLK] Checking for merge opportunities\n");
    return 0;  /* No merge in this simulation */
}

/* ==========================================================
 * TEMPLATE METHOD: submit_bio()
 * 
 * Framework controls the submission pipeline.
 * Device only implements make_request_fn.
 * ========================================================== */
void submit_bio(int rw, struct bio *bio)
{
    struct block_device *bdev = bio->bi_bdev;
    struct request_queue *q;

    printf("[submit_bio] TEMPLATE METHOD START\n");
    printf("  [BLK] Operation: %s, Sector: %lu, Size: %u bytes\n",
           rw == READ ? "READ" : "WRITE",
           bio->bi_sector, bio->bi_size);

    /* ========== FIXED STEP 1: Validate bio ========== */
    if (!bio || !bdev) {
        printf("  [BLK] ERROR: invalid bio or bdev\n");
        return;
    }
    printf("  [BLK] Bio validated\n");

    /* ========== FIXED STEP 2: Get request queue ========== */
    q = bdev->bd_disk->queue;
    if (!q || !q->make_request_fn) {
        printf("  [BLK] ERROR: no request queue or strategy\n");
        return;
    }
    printf("  [BLK] Request queue: %s\n", q->queue_name);

    /* ========== FIXED STEP 3: I/O accounting ========== */
    if (rw == READ) {
        task_io_account_read(bio->bi_size);
        q->sectors_read += bio_sectors(bio);
    } else {
        task_io_account_write(bio->bi_size);
        q->sectors_written += bio_sectors(bio);
    }

    /* ========== FIXED STEP 4: Tracing ========== */
    trace_block_bio_queue(q, bio);

    /* ========== FIXED STEP 5: Try to merge ========== */
    if (try_merge_bio(q, bio)) {
        printf("  [BLK] Bio merged with existing request\n");
        return;  /* Merged, no need to call make_request */
    }

    /* ========== VARIATION POINT: Call device strategy ========== */
    printf("  [BLK] >>> Calling device make_request_fn\n");
    q->make_request_fn(q, bio);
    printf("  [BLK] <<< Device strategy returned\n");

    q->requests_submitted++;

    printf("[submit_bio] TEMPLATE METHOD END\n\n");
}

/* Completion helper (called by device when I/O done) */
void bio_endio(struct bio *bio, int error)
{
    printf("  [BLK] Bio completion: sector=%lu, error=%d\n",
           bio->bi_sector, error);
    
    if (bio->bi_end_io) {
        bio->bi_end_io(bio, error);
    }
}

/* ==========================================================
 * DEVICE IMPLEMENTATIONS (Variation Points)
 * ========================================================== */

/* --- SCSI-like device implementation --- */
static void scsi_request_fn(struct request_queue *q, struct bio *bio)
{
    printf("    [SCSI] Building SCSI command for sector %lu\n", 
           bio->bi_sector);
    printf("    [SCSI] Sending command to SCSI host adapter\n");
    printf("    [SCSI] DMA transfer initiated\n");
    
    /* Simulate completion */
    bio_endio(bio, 0);
}

/* --- NVMe-like device implementation --- */
static void nvme_make_request(struct request_queue *q, struct bio *bio)
{
    printf("    [NVMe] Creating NVMe submission queue entry\n");
    printf("    [NVMe] Writing to doorbell register\n");
    printf("    [NVMe] Using native NVMe command\n");
    
    /* Simulate completion */
    bio_endio(bio, 0);
}

/* --- virtio-blk-like device implementation --- */
static void virtblk_make_request(struct request_queue *q, struct bio *bio)
{
    printf("    [virtio] Adding buffer to virtqueue\n");
    printf("    [virtio] Kicking hypervisor\n");
    printf("    [virtio] Waiting for completion virtqueue\n");
    
    /* Simulate completion */
    bio_endio(bio, 0);
}

/* --- RAM disk (no hardware) implementation --- */
static void ramdisk_make_request(struct request_queue *q, struct bio *bio)
{
    printf("    [ramdisk] Direct memory copy\n");
    printf("    [ramdisk] No hardware I/O needed\n");
    
    /* Immediate completion */
    bio_endio(bio, 0);
}

/* ==========================================================
 * COMPLETION CALLBACKS
 * ========================================================== */

static void my_read_complete(struct bio *bio, int error)
{
    printf("  [APP] Read completed: sector=%lu, error=%d\n",
           bio->bi_sector, error);
}

static void my_write_complete(struct bio *bio, int error)
{
    printf("  [APP] Write completed: sector=%lu, error=%d\n",
           bio->bi_sector, error);
}

/* ==========================================================
 * HELPER: Create test devices
 * ========================================================== */

static struct request_queue scsi_queue = {
    .make_request_fn = scsi_request_fn,
    .queue_name = "scsi_queue",
    .queue_depth = 32,
};

static struct gendisk scsi_disk = {
    .disk_name = "sda",
    .queue = &scsi_queue,
};

static struct block_device scsi_bdev = {
    .bd_disk = &scsi_disk,
};

static struct request_queue nvme_queue = {
    .make_request_fn = nvme_make_request,
    .queue_name = "nvme_queue",
    .queue_depth = 1024,
};

static struct gendisk nvme_disk = {
    .disk_name = "nvme0n1",
    .queue = &nvme_queue,
};

static struct block_device nvme_bdev = {
    .bd_disk = &nvme_disk,
};

static struct request_queue virtio_queue = {
    .make_request_fn = virtblk_make_request,
    .queue_name = "virtio_queue",
    .queue_depth = 128,
};

static struct gendisk virtio_disk = {
    .disk_name = "vda",
    .queue = &virtio_queue,
};

static struct block_device virtio_bdev = {
    .bd_disk = &virtio_disk,
};

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("==============================================\n");
    printf("BLOCK LAYER TEMPLATE METHOD DEMONSTRATION\n");
    printf("==============================================\n\n");

    /* Create bio for SCSI read */
    printf("--- Submit read to SCSI disk (sda) ---\n");
    struct bio scsi_read_bio = {
        .bi_sector = 1000,
        .bi_size = 4096,
        .bi_rw = READ,
        .bi_bdev = &scsi_bdev,
        .bi_end_io = my_read_complete,
    };
    submit_bio(READ, &scsi_read_bio);

    /* Create bio for NVMe write */
    printf("--- Submit write to NVMe disk (nvme0n1) ---\n");
    struct bio nvme_write_bio = {
        .bi_sector = 2000,
        .bi_size = 8192,
        .bi_rw = WRITE,
        .bi_bdev = &nvme_bdev,
        .bi_end_io = my_write_complete,
    };
    submit_bio(WRITE, &nvme_write_bio);

    /* Create bio for virtio read */
    printf("--- Submit read to virtio-blk (vda) ---\n");
    struct bio virtio_read_bio = {
        .bi_sector = 3000,
        .bi_size = 4096,
        .bi_rw = READ,
        .bi_bdev = &virtio_bdev,
        .bi_end_io = my_read_complete,
    };
    submit_bio(READ, &virtio_read_bio);

    /* Print statistics */
    printf("\n=== QUEUE STATISTICS ===\n");
    printf("scsi_queue: submitted=%lu, read_sectors=%lu, write_sectors=%lu\n",
           scsi_queue.requests_submitted,
           scsi_queue.sectors_read,
           scsi_queue.sectors_written);
    printf("nvme_queue: submitted=%lu, read_sectors=%lu, write_sectors=%lu\n",
           nvme_queue.requests_submitted,
           nvme_queue.sectors_read,
           nvme_queue.sectors_written);
    printf("virtio_queue: submitted=%lu, read_sectors=%lu, write_sectors=%lu\n",
           virtio_queue.requests_submitted,
           virtio_queue.sectors_read,
           virtio_queue.sectors_written);

    return 0;
}
```

---

## What the Implementation is NOT Allowed to Do

```
+=============================================================================+
|              BLOCK DEVICE IMPLEMENTATION RESTRICTIONS                        |
+=============================================================================+

    DEVICE CANNOT:

    1. REORDER I/O ARBITRARILY
       Framework controls ordering barriers
       Device must respect bio order for data integrity

       ESPECIALLY:
       - Cannot reorder across FUA (Force Unit Access)
       - Cannot reorder writes before barriers
       - Must honor REQ_FLUSH requests

    2. BYPASS ACCOUNTING
       Framework tracks all I/O before make_request_fn
       Device cannot do I/O without going through framework

    3. MODIFY BIO OWNERSHIP
       Bio belongs to caller until completion
       Device holds reference during I/O

    4. IGNORE COMPLETION CALLBACKS
       Must call bio_endio() when I/O completes
       Framework expects completion notification

    5. BLOCK INDEFINITELY
       make_request_fn should queue and return
       Actual I/O is asynchronous

    6. CHANGE SECTOR ADDRESSING
       Framework validates sector numbers
       Device must use sectors as given

    7. SPLIT WITHOUT FRAMEWORK HELP
       If bio is too large, use bio_split()
       Don't manually fragment requests

    +-----------------------------------------------------------------+
    |  BLOCK DEVICE IS A CONDUIT:                                     |
    |  - Receives formatted bio from framework                        |
    |  - Translates to hardware commands                              |
    |  - Reports completion back                                      |
    |  - CANNOT CHANGE WHAT OR WHEN TO DO I/O                         |
    +-----------------------------------------------------------------+
```

**中文说明：**

块设备实现的限制：(1) 不能任意重排I/O——必须尊重框架控制的顺序屏障；(2) 不能绕过统计——框架在调用前追踪所有I/O；(3) 不能修改bio所有权；(4) 不能忽略完成回调——必须调用`bio_endio()`；(5) 不能无限阻塞——应该排队后返回；(6) 不能改变扇区寻址；(7) 不能不通过框架帮助来分割请求。块设备是一个管道：接收格式化的bio、转换为硬件命令、报告完成，但不能改变做什么或何时做I/O。

---

## Real Kernel Code Reference (v3.2)

### submit_bio() in block/blk-core.c

```c
/* Simplified from actual kernel code */
void submit_bio(int rw, struct bio *bio)
{
    bio->bi_rw |= rw;

    /* Account I/O start */
    if (bio_has_data(bio)) {
        if (rw & WRITE)
            task_io_account_write(bio_sectors(bio));
        else
            task_io_account_read(bio_sectors(bio));
    }

    /* Tracepoint */
    trace_block_bio_queue(bdev_get_queue(bio->bi_bdev), bio);

    /* Call device strategy */
    generic_make_request(bio);
}

void generic_make_request(struct bio *bio)
{
    struct request_queue *q = bdev_get_queue(bio->bi_bdev);

    /* ... validation and recursion handling ... */

    q->make_request_fn(q, bio);
}
```

### Request queue setup in block/blk-core.c

```c
struct request_queue *blk_alloc_queue(gfp_t gfp_mask)
{
    struct request_queue *q;

    q = kmem_cache_alloc_node(blk_requestq_cachep, gfp_mask, ...);
    /* ... initialization ... */
    return q;
}

void blk_queue_make_request(struct request_queue *q,
                            make_request_fn *mfn)
{
    q->make_request_fn = mfn;
}
```

---

## Key Takeaways

1. **Framework owns I/O path**: All block I/O goes through `submit_bio()`
2. **Ordering is centralized**: Framework handles barriers and ordering
3. **Accounting is automatic**: All I/O is tracked consistently
4. **Merging is transparent**: Framework optimizes before device sees bio
5. **Devices are simple**: Just translate bio to hardware commands
