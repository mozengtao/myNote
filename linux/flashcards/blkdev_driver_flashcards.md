# Block Device Driver Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel block device driver, blk-mq, gendisk, and I/O processing
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. Block Device Overview (块设备概述)

---

Q: What is the difference between block and character devices?
A: 块设备和字符设备的主要区别：

```
+------------------------------------------------------------------+
|              Block vs Character Devices                           |
+------------------------------------------------------------------+
|                                                                  |
|  字符设备 (Character Device):                                     |
|  +----------------------------------------------------------+   |
|  | - 顺序访问，字节流                                         |   |
|  | - 无缓冲（或最小缓冲）                                     |   |
|  | - 不可寻址（seek无意义或受限）                             |   |
|  | - 示例：串口、键盘、鼠标                                   |   |
|  | - 直接通过file_operations访问                              |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  块设备 (Block Device):                                          |
|  +----------------------------------------------------------+   |
|  | - 随机访问，固定大小块                                     |   |
|  | - 有缓冲（页缓存）                                        |   |
|  | - 可寻址（任意位置读写）                                   |   |
|  | - 示例：硬盘、SSD、U盘                                     |   |
|  | - 通过块层和请求队列访问                                   |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

| 特性 | 字符设备 | 块设备 |
|------|----------|--------|
| 访问方式 | 顺序 | 随机 |
| 基本单位 | 字节 | 块(扇区) |
| 缓存 | 无 | 页缓存 |
| 文件系统 | 不支持 | 支持 |
| I/O调度 | 无 | 有 |
| 接口 | file_operations | block_device_operations |
[Basic]

---

Q: What is the architecture of Linux block layer?
A: Linux块层架构：

```
+------------------------------------------------------------------+
|                    Block Layer Architecture                       |
+------------------------------------------------------------------+
|                                                                  |
|  +------------------------------------------------------------+  |
|  |                   VFS (虚拟文件系统)                        |  |
|  |  read(), write(), fsync(), ...                             |  |
|  +-----------------------------+------------------------------+  |
|                                |                                 |
|                          Page Cache                              |
|                                |                                 |
|  +-----------------------------v------------------------------+  |
|  |                   Block Layer (块层)                        |  |
|  |  +----------------+  +----------------+  +----------------+ |  |
|  |  |   bio 结构     |  | I/O Scheduler  |  | blk-mq层       | |  |
|  |  |  (块I/O请求)   |  |  (I/O调度器)   |  | (多队列)       | |  |
|  |  +----------------+  +----------------+  +----------------+ |  |
|  +-----------------------------+------------------------------+  |
|                                |                                 |
|  +-----------------------------v------------------------------+  |
|  |                Block Device Driver (块设备驱动)             |  |
|  |  gendisk, request_queue, block_device_operations           |  |
|  +-----------------------------+------------------------------+  |
|                                |                                 |
|  +-----------------------------v------------------------------+  |
|  |                   Hardware (硬件)                           |  |
|  |  HDD, SSD, NVMe, RAID, ...                                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

关键组件：
| 组件 | 作用 |
|------|------|
| bio | 块I/O请求描述符 |
| request | 合并后的I/O请求 |
| request_queue | 请求队列 |
| gendisk | 通用磁盘结构 |
| blk-mq | 多队列块层 |
[Basic]

---

## 2. gendisk Structure (通用磁盘结构)

---

Q: What is struct gendisk?
A: `gendisk`表示一个块设备磁盘：

```c
// include/linux/blkdev.h (简化版)
struct gendisk {
    /*=== 设备标识 ===*/
    int major;                        // 主设备号
    int first_minor;                  // 第一个次设备号
    int minors;                       // 次设备号数量（分区数+1）
    char disk_name[DISK_NAME_LEN];    // 磁盘名 (sda, nvme0n1)
    
    /*=== 分区管理 ===*/
    struct disk_part_tbl __rcu *part_tbl;  // 分区表
    struct block_device *part0;       // 整盘设备
    
    /*=== 请求队列 ===*/
    struct request_queue *queue;      // 请求队列
    
    /*=== 操作函数 ===*/
    const struct block_device_operations *fops;
    
    /*=== 私有数据 ===*/
    void *private_data;               // 驱动私有数据
    
    /*=== 磁盘属性 ===*/
    int flags;                        // GENHD_FL_* 标志
    unsigned long state;              // 磁盘状态
    
    struct kobject *slave_dir;
    
    struct timer_rand_state *random;
    atomic_t sync_io;                 // 同步I/O计数
    struct disk_events *ev;           // 磁盘事件
    
    // ... 更多字段
};

// 磁盘标志
#define GENHD_FL_REMOVABLE      (1 << 0)  // 可移动
#define GENHD_FL_CD             (1 << 3)  // CD-ROM
#define GENHD_FL_SUPPRESS_PARTITION_INFO (1 << 5)
#define GENHD_FL_EXT_DEVT       (1 << 6)  // 扩展设备号
#define GENHD_FL_NO_PART_SCAN   (1 << 9)  // 不扫描分区
#define GENHD_FL_HIDDEN         (1 << 10) // 隐藏
```

gendisk操作：
```c
// 分配gendisk
struct gendisk *alloc_disk(int minors);           // 传统方式
struct gendisk *blk_alloc_disk(int node);         // 新方式(5.14+)
struct gendisk *blk_mq_alloc_disk(struct blk_mq_tag_set *set,
                                   void *queuedata);

// 添加磁盘到系统
int add_disk(struct gendisk *disk);
int device_add_disk(struct device *parent, struct gendisk *disk,
                    const struct attribute_group **groups);

// 删除磁盘
void del_gendisk(struct gendisk *disk);

// 设置容量（512字节扇区数）
void set_capacity(struct gendisk *disk, sector_t size);

// 获取/设置私有数据
void set_disk_ro(struct gendisk *disk, int flag);  // 设置只读
```
[Intermediate]

---

Q: What is struct block_device_operations?
A: `block_device_operations`定义块设备操作：

```c
// include/linux/blkdev.h
struct block_device_operations {
    // 打开/释放
    int (*open)(struct block_device *bdev, fmode_t mode);
    void (*release)(struct gendisk *disk, fmode_t mode);
    
    // ioctl
    int (*ioctl)(struct block_device *bdev, fmode_t mode,
                 unsigned cmd, unsigned long arg);
    int (*compat_ioctl)(struct block_device *bdev, fmode_t mode,
                        unsigned cmd, unsigned long arg);
    
    // 直接访问(DAX)
    long (*direct_access)(struct block_device *bdev, sector_t sector,
                          void **kaddr, pfn_t *pfn, long size);
    
    // 介质变化检测
    unsigned int (*check_events)(struct gendisk *disk,
                                 unsigned int clearing);
    void (*unlock_native_capacity)(struct gendisk *disk);
    
    // 获取磁盘几何信息
    int (*getgeo)(struct block_device *bdev, struct hd_geometry *geo);
    
    // 交换区激活
    int (*swap_slot_free_notify)(struct block_device *bdev,
                                 unsigned long offset);
    
    // 报告区域(分区)
    int (*report_zones)(struct gendisk *disk, sector_t sector,
                        unsigned int nr_zones, report_zones_cb cb, void *data);
    
    // 所有者模块
    struct module *owner;
    
    // 特性信息
    const struct pr_ops *pr_ops;      // 持久预留操作
};
```

常见实现：
```c
static int my_open(struct block_device *bdev, fmode_t mode)
{
    struct my_device *dev = bdev->bd_disk->private_data;
    
    // 增加使用计数
    spin_lock(&dev->lock);
    dev->users++;
    spin_unlock(&dev->lock);
    
    return 0;
}

static void my_release(struct gendisk *disk, fmode_t mode)
{
    struct my_device *dev = disk->private_data;
    
    spin_lock(&dev->lock);
    dev->users--;
    spin_unlock(&dev->lock);
}

static int my_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    struct my_device *dev = bdev->bd_disk->private_data;
    
    // 设置CHS几何信息（用于旧工具兼容）
    geo->heads = 64;
    geo->sectors = 32;
    geo->cylinders = dev->size / (64 * 32 * 512);
    
    return 0;
}

static const struct block_device_operations my_fops = {
    .owner      = THIS_MODULE,
    .open       = my_open,
    .release    = my_release,
    .getgeo     = my_getgeo,
    .ioctl      = my_ioctl,
};
```
[Intermediate]

---

## 3. Request Queue (请求队列)

---

Q: What is struct request_queue?
A: `request_queue`管理块设备的I/O请求：

```c
// include/linux/blkdev.h (简化版)
struct request_queue {
    /*=== 队列状态 ===*/
    unsigned long queue_flags;        // 队列标志
    
    /*=== 限制参数 ===*/
    struct queue_limits limits;       // I/O限制
    
    /*=== 多队列支持 ===*/
    struct blk_mq_tag_set *tag_set;   // blk-mq标签集
    struct list_head tag_set_list;
    
    /*=== 调度器 ===*/
    struct elevator_queue *elevator;   // I/O调度器
    
    /*=== 设备关联 ===*/
    struct gendisk *disk;             // 关联的gendisk
    struct kobject kobj;              // sysfs对象
    
    /*=== 队列数据 ===*/
    void *queuedata;                  // 驱动私有数据
    
    // ... 更多字段
};

// 队列限制
struct queue_limits {
    unsigned int max_hw_sectors;      // 硬件最大扇区数
    unsigned int max_dev_sectors;     // 设备最大扇区数
    unsigned int max_sectors;         // 当前最大扇区数
    unsigned int max_segment_size;    // 最大段大小
    unsigned int max_segments;        // 最大段数
    unsigned short logical_block_size;   // 逻辑块大小
    unsigned short physical_block_size;  // 物理块大小
    // ...
};
```

队列限制设置：
```c
// 设置逻辑块大小（最小I/O单位，通常512或4096）
blk_queue_logical_block_size(q, 512);

// 设置物理块大小（设备最优I/O大小）
blk_queue_physical_block_size(q, 4096);

// 设置最大扇区数（每个请求）
blk_queue_max_hw_sectors(q, 1024);

// 设置最大段数（scatter-gather）
blk_queue_max_segments(q, 128);

// 设置最大段大小
blk_queue_max_segment_size(q, 65536);

// 设置边界限制（DMA边界）
blk_queue_segment_boundary(q, 0xffffffff);

// 设置队列标志
blk_queue_flag_set(QUEUE_FLAG_NONROT, q);  // SSD/非旋转
blk_queue_flag_set(QUEUE_FLAG_ADD_RANDOM, q);  // 添加熵
```
[Intermediate]

---

## 4. BIO Structure (块I/O结构)

---

Q: What is struct bio?
A: `bio`是块I/O的基本单位：

```c
// include/linux/blk_types.h
struct bio {
    struct bio              *bi_next;      // 链表下一个
    struct block_device     *bi_bdev;      // 目标块设备
    unsigned int            bi_opf;        // 操作和标志
    unsigned short          bi_flags;      // bio标志
    unsigned short          bi_ioprio;     // I/O优先级
    unsigned short          bi_write_hint;
    blk_status_t            bi_status;     // 完成状态
    atomic_t                __bi_remaining;
    
    struct bvec_iter        bi_iter;       // 当前迭代器位置
    
    bio_end_io_t            *bi_end_io;    // 完成回调
    void                    *bi_private;   // 私有数据
    
    unsigned short          bi_vcnt;       // bio_vec数量
    unsigned short          bi_max_vecs;   // 最大bio_vec数
    atomic_t                __bi_cnt;      // 引用计数
    
    struct bio_vec          *bi_io_vec;    // bio_vec数组
    // ...
};

// bio向量（页面段）
struct bio_vec {
    struct page     *bv_page;    // 页面
    unsigned int    bv_len;      // 长度
    unsigned int    bv_offset;   // 页内偏移
};

// 迭代器
struct bvec_iter {
    sector_t        bi_sector;   // 起始扇区
    unsigned int    bi_size;     // 剩余字节数
    unsigned int    bi_idx;      // 当前bio_vec索引
    unsigned int    bi_bvec_done;// 当前bio_vec已处理字节
};
```

bio操作：
```c
// 分配bio
struct bio *bio_alloc(gfp_t gfp, unsigned short nr_vecs);
struct bio *bio_alloc_bioset(gfp_t gfp, unsigned short nr_vecs,
                             struct bio_set *bs);

// 释放bio
void bio_put(struct bio *bio);

// 添加页面到bio
int bio_add_page(struct bio *bio, struct page *page,
                 unsigned len, unsigned off);

// 设置完成回调
bio->bi_end_io = my_bio_end_io;
bio->bi_private = my_data;

// 提交bio
void submit_bio(struct bio *bio);

// 遍历bio段
struct bio_vec bvec;
struct bvec_iter iter;
bio_for_each_segment(bvec, bio, iter) {
    // bvec.bv_page, bvec.bv_len, bvec.bv_offset
}
```

bio示意图：
```
struct bio
+------------------+
| bi_bdev          | --> block_device
| bi_opf           | --> READ/WRITE + flags
| bi_iter.bi_sector| --> 起始扇区
+------------------+
| bi_io_vec[]      |
|   [0] page/len/off --> [Page 0]
|   [1] page/len/off --> [Page 1]
|   [2] page/len/off --> [Page 2]
+------------------+
```
[Intermediate]

---

Q: How does bio processing work?
A: bio处理流程：

```
+------------------------------------------------------------------+
|                    BIO Processing Flow                            |
+------------------------------------------------------------------+
|                                                                  |
|  文件系统/页缓存                                                  |
|     |                                                            |
|     v                                                            |
|  创建bio                                                         |
|     |                                                            |
|     v                                                            |
|  submit_bio()                                                    |
|     |                                                            |
|     v                                                            |
|  generic_make_request() / submit_bio_noacct()                    |
|     |                                                            |
|     +---> bio重映射（如果是stacked driver）                       |
|     |                                                            |
|     v                                                            |
|  blk_mq_submit_bio()                                             |
|     |                                                            |
|     +---> bio合并（如果可能）                                     |
|     +---> 创建或复用request                                       |
|     |                                                            |
|     v                                                            |
|  I/O调度器                                                        |
|     |                                                            |
|     v                                                            |
|  blk_mq_dispatch_request()                                       |
|     |                                                            |
|     v                                                            |
|  驱动queue_rq()回调                                              |
|     |                                                            |
|     v                                                            |
|  硬件处理                                                         |
|     |                                                            |
|     v                                                            |
|  完成中断 --> blk_mq_complete_request()                          |
|     |                                                            |
|     v                                                            |
|  bio->bi_end_io() 回调                                           |
|                                                                  |
+------------------------------------------------------------------+
```

bio操作类型：
```c
// include/linux/blk_types.h
// 主操作（op）
REQ_OP_READ          // 读
REQ_OP_WRITE         // 写
REQ_OP_FLUSH         // 刷新缓存
REQ_OP_DISCARD       // 丢弃（TRIM）
REQ_OP_SECURE_ERASE  // 安全擦除
REQ_OP_ZONE_RESET    // 重置区域
REQ_OP_WRITE_SAME    // 写相同数据
REQ_OP_WRITE_ZEROES  // 写零

// 标志（flags）
REQ_SYNC             // 同步请求
REQ_META             // 元数据
REQ_PRIO             // 高优先级
REQ_NOMERGE          // 不合并
REQ_IDLE             // 空闲时处理
REQ_FUA              // Force Unit Access
REQ_PREFLUSH         // 预刷新
REQ_RAHEAD           // 预读

// 获取操作
unsigned int op = bio_op(bio);
bool is_write = op_is_write(op);
```
[Intermediate]

---

## 5. blk-mq (Multi-Queue Block Layer)

---

Q: What is blk-mq and why is it important?
A: blk-mq是现代块设备的多队列架构：

```
+------------------------------------------------------------------+
|                    blk-mq Architecture                            |
+------------------------------------------------------------------+
|                                                                  |
|  传统单队列 (已废弃):                                             |
|  +----------------------------------------------------------+   |
|  |  所有CPU共享一个请求队列                                   |   |
|  |  +--------+                                              |   |
|  |  | Queue  | <-- CPU 0,1,2,3 竞争锁                       |   |
|  |  +--------+                                              |   |
|  |  瓶颈：锁竞争，不能充分利用多核和高速设备                   |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  blk-mq多队列:                                                   |
|  +----------------------------------------------------------+   |
|  |  Software Queues (软件队列) - 每CPU一个                   |   |
|  |  +--------+ +--------+ +--------+ +--------+             |   |
|  |  | SW Q 0 | | SW Q 1 | | SW Q 2 | | SW Q 3 |             |   |
|  |  +---+----+ +---+----+ +---+----+ +---+----+             |   |
|  |      |          |          |          |                   |   |
|  |      +-----+----+----+-----+----+-----+                   |   |
|  |            |         |         |                          |   |
|  |  Hardware Queues (硬件队列) - 映射到硬件                  |   |
|  |  +--------+    +--------+    +--------+                  |   |
|  |  | HW Q 0 |    | HW Q 1 |    | HW Q 2 |                  |   |
|  |  +--------+    +--------+    +--------+                  |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

blk-mq核心数据结构：
```c
// 标签集 - 描述硬件能力
struct blk_mq_tag_set {
    const struct blk_mq_ops *ops;     // 操作回调
    unsigned int nr_hw_queues;         // 硬件队列数
    unsigned int queue_depth;          // 队列深度
    unsigned int cmd_size;             // 每个请求的命令大小
    int numa_node;
    unsigned int timeout;              // 请求超时
    unsigned int flags;                // 标志
    void *driver_data;                 // 驱动数据
    struct blk_mq_tags **tags;
    // ...
};

// 硬件队列上下文
struct blk_mq_hw_ctx {
    struct blk_mq_ctx *ctxs[];        // 软件队列映射
    unsigned int nr_ctx;               // 软件队列数
    struct request_queue *queue;
    // ...
};

// blk-mq操作
struct blk_mq_ops {
    // 处理请求（核心）
    blk_status_t (*queue_rq)(struct blk_mq_hw_ctx *hctx,
                             const struct blk_mq_queue_data *bd);
    
    // 完成请求
    void (*complete)(struct request *rq);
    
    // 初始化/退出
    int (*init_hctx)(struct blk_mq_hw_ctx *hctx, void *data,
                     unsigned int hctx_idx);
    void (*exit_hctx)(struct blk_mq_hw_ctx *hctx, unsigned int hctx_idx);
    int (*init_request)(struct blk_mq_tag_set *set, struct request *rq,
                        unsigned int hctx_idx, unsigned int numa_node);
    void (*exit_request)(struct blk_mq_tag_set *set, struct request *rq,
                         unsigned int hctx_idx);
    
    // 超时处理
    enum blk_eh_timer_return (*timeout)(struct request *rq, bool reserved);
    
    // 轮询
    int (*poll)(struct blk_mq_hw_ctx *hctx);
    
    // 映射队列
    int (*map_queues)(struct blk_mq_tag_set *set);
};
```
[Intermediate]

---

Q: How to implement a blk-mq driver?
A: blk-mq驱动实现模板：

```c
#include <linux/blk-mq.h>

struct my_device {
    struct gendisk *gendisk;
    struct blk_mq_tag_set tag_set;
    spinlock_t lock;
    void *data;
    size_t size;
};

// 请求私有数据（放在request后面）
struct my_cmd {
    struct my_device *dev;
    // 其他命令数据
};

// 核心：处理请求
static blk_status_t my_queue_rq(struct blk_mq_hw_ctx *hctx,
                                 const struct blk_mq_queue_data *bd)
{
    struct request *rq = bd->rq;
    struct my_device *dev = rq->q->queuedata;
    struct my_cmd *cmd = blk_mq_rq_to_pdu(rq);
    struct bio_vec bvec;
    struct req_iterator iter;
    sector_t sector = blk_rq_pos(rq);
    
    // 开始处理请求
    blk_mq_start_request(rq);
    
    // 遍历请求中的所有bio段
    rq_for_each_segment(bvec, rq, iter) {
        unsigned int len = bvec.bv_len;
        void *buf = page_address(bvec.bv_page) + bvec.bv_offset;
        sector_t offset = sector * 512;
        
        if (rq_data_dir(rq) == READ) {
            // 读操作
            memcpy(buf, dev->data + offset, len);
        } else {
            // 写操作
            memcpy(dev->data + offset, buf, len);
        }
        
        sector += len / 512;
    }
    
    // 完成请求
    blk_mq_end_request(rq, BLK_STS_OK);
    
    return BLK_STS_OK;
}

// 初始化请求
static int my_init_request(struct blk_mq_tag_set *set, struct request *rq,
                           unsigned int hctx_idx, unsigned int numa_node)
{
    struct my_cmd *cmd = blk_mq_rq_to_pdu(rq);
    cmd->dev = set->driver_data;
    return 0;
}

static const struct blk_mq_ops my_mq_ops = {
    .queue_rq       = my_queue_rq,
    .init_request   = my_init_request,
};

// 初始化设备
static int my_init_device(struct my_device *dev)
{
    int ret;
    
    // 1. 设置tag_set
    dev->tag_set.ops = &my_mq_ops;
    dev->tag_set.nr_hw_queues = 1;        // 硬件队列数
    dev->tag_set.queue_depth = 128;       // 队列深度
    dev->tag_set.numa_node = NUMA_NO_NODE;
    dev->tag_set.cmd_size = sizeof(struct my_cmd);
    dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    dev->tag_set.driver_data = dev;
    
    ret = blk_mq_alloc_tag_set(&dev->tag_set);
    if (ret)
        return ret;
    
    // 2. 分配gendisk
    dev->gendisk = blk_mq_alloc_disk(&dev->tag_set, dev);
    if (IS_ERR(dev->gendisk)) {
        blk_mq_free_tag_set(&dev->tag_set);
        return PTR_ERR(dev->gendisk);
    }
    
    // 3. 设置gendisk属性
    dev->gendisk->major = my_major;
    dev->gendisk->first_minor = 0;
    dev->gendisk->minors = 16;  // 支持15个分区
    dev->gendisk->fops = &my_fops;
    dev->gendisk->private_data = dev;
    snprintf(dev->gendisk->disk_name, DISK_NAME_LEN, "myblk");
    
    // 4. 设置容量
    set_capacity(dev->gendisk, dev->size / 512);
    
    // 5. 设置队列参数
    blk_queue_logical_block_size(dev->gendisk->queue, 512);
    blk_queue_physical_block_size(dev->gendisk->queue, 512);
    
    // 6. 添加磁盘
    ret = add_disk(dev->gendisk);
    if (ret) {
        put_disk(dev->gendisk);
        blk_mq_free_tag_set(&dev->tag_set);
        return ret;
    }
    
    return 0;
}

// 清理设备
static void my_cleanup_device(struct my_device *dev)
{
    del_gendisk(dev->gendisk);
    put_disk(dev->gendisk);
    blk_mq_free_tag_set(&dev->tag_set);
}
```
[Advanced]

---

## 6. I/O Schedulers (I/O调度器)

---

Q: What are the I/O schedulers in Linux?
A: Linux支持多种I/O调度器：

```
+------------------------------------------------------------------+
|                    I/O Schedulers                                 |
+------------------------------------------------------------------+
|                                                                  |
|  mq-deadline (默认):                                              |
|  +----------------------------------------------------------+   |
|  | - 为每个方向（读/写）维护FIFO队列                          |   |
|  | - 读请求优先级高于写请求                                   |   |
|  | - 设置截止时间防止饥饿                                     |   |
|  | - 适合大多数工作负载                                       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  bfq (Budget Fair Queueing):                                     |
|  +----------------------------------------------------------+   |
|  | - 基于进程公平分配I/O带宽                                  |   |
|  | - 低延迟优化                                               |   |
|  | - 适合桌面和交互式应用                                     |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  kyber:                                                          |
|  +----------------------------------------------------------+   |
|  | - 针对NVMe等高速设备优化                                   |   |
|  | - 基于延迟目标的简单调度                                   |   |
|  | - 低开销                                                   |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  none:                                                           |
|  +----------------------------------------------------------+   |
|  | - 无调度，直接传递给硬件                                   |   |
|  | - 适合NVMe SSD等自带调度的设备                            |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

调度器配置：
```bash
# 查看可用调度器
cat /sys/block/sda/queue/scheduler
# [mq-deadline] kyber bfq none

# 更改调度器
echo "bfq" > /sys/block/sda/queue/scheduler

# 查看调度器参数
ls /sys/block/sda/queue/iosched/

# 设置参数（mq-deadline）
echo 500 > /sys/block/sda/queue/iosched/read_expire
echo 5000 > /sys/block/sda/queue/iosched/write_expire
```

驱动中设置调度器：
```c
// 设置默认使用none调度器（高速设备）
blk_queue_flag_set(QUEUE_FLAG_NONROT, q);  // 标记为非旋转

// 或在sysfs中让用户选择
// 调度器会根据设备特性自动选择
```
[Intermediate]

---

## 7. Partitions (分区)

---

Q: How does partition management work?
A: Linux分区管理：

```c
// 分区结构
struct hd_struct {
    sector_t start_sect;      // 起始扇区
    sector_t nr_sects;        // 扇区数
    unsigned int partno;      // 分区号
    struct device __dev;
    struct kobject *holder_dir;
    // ...
};

// 分区表
struct disk_part_tbl {
    struct rcu_head rcu_head;
    int len;                   // 分区数
    struct hd_struct __rcu *part[];  // 分区数组
};
```

分区访问：
```c
// 获取分区信息
struct gendisk *disk = bdev->bd_disk;
struct hd_struct *part;

// 遍历分区
disk_part_iter_init(&piter, disk, DISK_PITER_INCL_EMPTY);
while ((part = disk_part_iter_next(&piter))) {
    printk("Partition %d: start=%llu size=%llu\n",
           part->partno, part->start_sect, part->nr_sects);
}
disk_part_iter_exit(&piter);
```

分区表类型：
```
+------------------------------------------------------------------+
|  MBR (Master Boot Record):                                        |
|  - 最多4个主分区（或3个主分区+1个扩展分区）                         |
|  - 最大支持2TB磁盘                                                |
|  - 传统BIOS系统使用                                               |
+------------------------------------------------------------------+
|  GPT (GUID Partition Table):                                      |
|  - 最多128个分区（可扩展）                                         |
|  - 支持超大磁盘（8ZB）                                            |
|  - UEFI系统使用                                                   |
|  - 有备份分区表                                                   |
+------------------------------------------------------------------+
```

分区相关命令：
```bash
# 查看分区
fdisk -l /dev/sda
parted /dev/sda print
lsblk

# 创建分区
fdisk /dev/sda
parted /dev/sda mkpart primary ext4 1MiB 100%

# 通知内核分区变化
partprobe /dev/sda
blockdev --rereadpt /dev/sda
```
[Intermediate]

---

## 8. Complete Block Driver Example (完整块驱动示例)

---

Q: Show a complete RAM disk driver example.
A: 完整的内存磁盘驱动：

```c
// ramdisk.c - 简单的内存块设备驱动
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>

#define RAMDISK_SIZE    (16 * 1024 * 1024)  // 16MB
#define RAMDISK_NAME    "ramdisk"
#define KERNEL_SECTOR_SIZE  512

static int ramdisk_major;

struct ramdisk_device {
    unsigned char *data;
    size_t size;
    struct gendisk *gendisk;
    struct blk_mq_tag_set tag_set;
    struct request_queue *queue;
};

static struct ramdisk_device *ramdisk_dev;

// blk-mq请求处理
static blk_status_t ramdisk_queue_rq(struct blk_mq_hw_ctx *hctx,
                                      const struct blk_mq_queue_data *bd)
{
    struct request *rq = bd->rq;
    struct ramdisk_device *dev = rq->q->queuedata;
    struct bio_vec bvec;
    struct req_iterator iter;
    sector_t sector = blk_rq_pos(rq);
    void *buf;
    size_t offset;
    
    blk_mq_start_request(rq);
    
    rq_for_each_segment(bvec, rq, iter) {
        offset = sector * KERNEL_SECTOR_SIZE;
        buf = page_address(bvec.bv_page) + bvec.bv_offset;
        
        if (offset + bvec.bv_len > dev->size) {
            blk_mq_end_request(rq, BLK_STS_IOERR);
            return BLK_STS_IOERR;
        }
        
        if (rq_data_dir(rq) == READ)
            memcpy(buf, dev->data + offset, bvec.bv_len);
        else
            memcpy(dev->data + offset, buf, bvec.bv_len);
        
        sector += bvec.bv_len / KERNEL_SECTOR_SIZE;
    }
    
    blk_mq_end_request(rq, BLK_STS_OK);
    return BLK_STS_OK;
}

static const struct blk_mq_ops ramdisk_mq_ops = {
    .queue_rq = ramdisk_queue_rq,
};

// 块设备操作
static int ramdisk_open(struct block_device *bdev, fmode_t mode)
{
    return 0;
}

static void ramdisk_release(struct gendisk *disk, fmode_t mode)
{
}

static int ramdisk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    struct ramdisk_device *dev = bdev->bd_disk->private_data;
    
    geo->heads = 64;
    geo->sectors = 32;
    geo->cylinders = dev->size / (64 * 32 * KERNEL_SECTOR_SIZE);
    return 0;
}

static const struct block_device_operations ramdisk_fops = {
    .owner      = THIS_MODULE,
    .open       = ramdisk_open,
    .release    = ramdisk_release,
    .getgeo     = ramdisk_getgeo,
};

static int __init ramdisk_init(void)
{
    int ret;
    
    // 1. 分配设备结构
    ramdisk_dev = kzalloc(sizeof(*ramdisk_dev), GFP_KERNEL);
    if (!ramdisk_dev)
        return -ENOMEM;
    
    ramdisk_dev->size = RAMDISK_SIZE;
    
    // 2. 分配数据缓冲区
    ramdisk_dev->data = vzalloc(RAMDISK_SIZE);
    if (!ramdisk_dev->data) {
        ret = -ENOMEM;
        goto err_free_dev;
    }
    
    // 3. 注册主设备号
    ramdisk_major = register_blkdev(0, RAMDISK_NAME);
    if (ramdisk_major < 0) {
        ret = ramdisk_major;
        goto err_free_data;
    }
    
    // 4. 初始化blk-mq tag_set
    ramdisk_dev->tag_set.ops = &ramdisk_mq_ops;
    ramdisk_dev->tag_set.nr_hw_queues = 1;
    ramdisk_dev->tag_set.queue_depth = 128;
    ramdisk_dev->tag_set.numa_node = NUMA_NO_NODE;
    ramdisk_dev->tag_set.cmd_size = 0;
    ramdisk_dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    
    ret = blk_mq_alloc_tag_set(&ramdisk_dev->tag_set);
    if (ret)
        goto err_unreg_blkdev;
    
    // 5. 分配gendisk
    ramdisk_dev->gendisk = blk_mq_alloc_disk(&ramdisk_dev->tag_set,
                                              ramdisk_dev);
    if (IS_ERR(ramdisk_dev->gendisk)) {
        ret = PTR_ERR(ramdisk_dev->gendisk);
        goto err_free_tagset;
    }
    
    ramdisk_dev->queue = ramdisk_dev->gendisk->queue;
    
    // 6. 设置gendisk
    ramdisk_dev->gendisk->major = ramdisk_major;
    ramdisk_dev->gendisk->first_minor = 0;
    ramdisk_dev->gendisk->minors = 16;
    ramdisk_dev->gendisk->fops = &ramdisk_fops;
    ramdisk_dev->gendisk->private_data = ramdisk_dev;
    snprintf(ramdisk_dev->gendisk->disk_name, DISK_NAME_LEN, RAMDISK_NAME);
    
    // 7. 设置容量
    set_capacity(ramdisk_dev->gendisk, RAMDISK_SIZE / KERNEL_SECTOR_SIZE);
    
    // 8. 设置队列参数
    blk_queue_logical_block_size(ramdisk_dev->queue, KERNEL_SECTOR_SIZE);
    blk_queue_physical_block_size(ramdisk_dev->queue, KERNEL_SECTOR_SIZE);
    
    // 9. 添加磁盘
    ret = add_disk(ramdisk_dev->gendisk);
    if (ret)
        goto err_put_disk;
    
    pr_info("ramdisk: initialized, size=%zu bytes\n", ramdisk_dev->size);
    return 0;
    
err_put_disk:
    put_disk(ramdisk_dev->gendisk);
err_free_tagset:
    blk_mq_free_tag_set(&ramdisk_dev->tag_set);
err_unreg_blkdev:
    unregister_blkdev(ramdisk_major, RAMDISK_NAME);
err_free_data:
    vfree(ramdisk_dev->data);
err_free_dev:
    kfree(ramdisk_dev);
    return ret;
}

static void __exit ramdisk_exit(void)
{
    del_gendisk(ramdisk_dev->gendisk);
    put_disk(ramdisk_dev->gendisk);
    blk_mq_free_tag_set(&ramdisk_dev->tag_set);
    unregister_blkdev(ramdisk_major, RAMDISK_NAME);
    vfree(ramdisk_dev->data);
    kfree(ramdisk_dev);
    pr_info("ramdisk: removed\n");
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("Simple RAM Disk Driver");
```

使用方法：
```bash
# 编译并加载
make
insmod ramdisk.ko

# 查看设备
ls -l /dev/ramdisk
lsblk

# 创建文件系统
mkfs.ext4 /dev/ramdisk

# 挂载使用
mount /dev/ramdisk /mnt
df -h /mnt

# 卸载
umount /mnt
rmmod ramdisk
```
[Advanced]

---

## 9. Request Handling (请求处理)

---

Q: How to iterate and process requests?
A: 请求处理辅助函数：

```c
// 获取请求信息
sector_t blk_rq_pos(struct request *rq);      // 起始扇区
unsigned int blk_rq_bytes(struct request *rq); // 总字节数
unsigned int blk_rq_cur_bytes(struct request *rq); // 当前段字节
int rq_data_dir(struct request *rq);           // 方向(READ/WRITE)

// 遍历请求中的bio
struct bio *bio;
__rq_for_each_bio(bio, rq) {
    // 处理每个bio
}

// 遍历请求中的所有段
struct req_iterator iter;
struct bio_vec bvec;
rq_for_each_segment(bvec, rq, iter) {
    void *buf = page_address(bvec.bv_page) + bvec.bv_offset;
    size_t len = bvec.bv_len;
    // 处理数据
}

// 请求完成
void blk_mq_end_request(struct request *rq, blk_status_t status);

// 部分完成
bool blk_mq_end_request_partial(struct request *rq, blk_status_t status,
                                 unsigned int nr_bytes);

// 返回状态
BLK_STS_OK         // 成功
BLK_STS_NOTSUPP    // 不支持
BLK_STS_TIMEOUT    // 超时
BLK_STS_NOSPC      // 空间不足
BLK_STS_TRANSPORT  // 传输错误
BLK_STS_TARGET     // 目标错误
BLK_STS_IOERR      // I/O错误
BLK_STS_RESOURCE   // 资源不足（稍后重试）
```

异步完成示例：
```c
// 异步处理（如真实硬件）
static blk_status_t my_queue_rq(struct blk_mq_hw_ctx *hctx,
                                 const struct blk_mq_queue_data *bd)
{
    struct request *rq = bd->rq;
    struct my_cmd *cmd = blk_mq_rq_to_pdu(rq);
    
    blk_mq_start_request(rq);
    
    // 提交到硬件
    cmd->rq = rq;
    submit_to_hardware(cmd);
    
    // 不立即完成，等待中断
    return BLK_STS_OK;
}

// 中断处理中完成
static irqreturn_t my_irq_handler(int irq, void *data)
{
    struct my_device *dev = data;
    struct my_cmd *cmd;
    
    // 获取完成的命令
    cmd = get_completed_cmd(dev);
    
    // 完成请求
    blk_mq_complete_request(cmd->rq);
    
    return IRQ_HANDLED;
}

// 完成回调（可能在软中断上下文）
static void my_complete(struct request *rq)
{
    struct my_cmd *cmd = blk_mq_rq_to_pdu(rq);
    blk_status_t status = cmd->error ? BLK_STS_IOERR : BLK_STS_OK;
    
    blk_mq_end_request(rq, status);
}

static const struct blk_mq_ops my_mq_ops = {
    .queue_rq = my_queue_rq,
    .complete = my_complete,
};
```
[Intermediate]

---

## 10. Stacked Block Drivers (堆叠块驱动)

---

Q: What are stacked block drivers?
A: 堆叠驱动在现有块设备上添加功能：

```
+------------------------------------------------------------------+
|                  Stacked Block Drivers                            |
+------------------------------------------------------------------+
|                                                                  |
|                    VFS / 文件系统                                 |
|                         |                                        |
|                         v                                        |
|  +----------------------------------------------------------+   |
|  |                   dm (Device Mapper)                      |   |
|  |  - dm-linear    (线性映射)                                |   |
|  |  - dm-stripe    (条带化)                                  |   |
|  |  - dm-mirror    (镜像)                                    |   |
|  |  - dm-crypt     (加密)                                    |   |
|  |  - dm-snapshot  (快照)                                    |   |
|  |  - dm-thin      (精简配置)                                |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|  +----------------------------------------------------------+   |
|  |                   md (Software RAID)                      |   |
|  |  - RAID 0, 1, 5, 6, 10                                   |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|              底层块设备 (sda, sdb, nvme0n1, ...)                 |
|                                                                  |
+------------------------------------------------------------------+
```

Device Mapper架构：
```c
// dm_target操作
struct target_type {
    const char *name;
    struct module *module;
    
    // 构造/析构
    int (*ctr)(struct dm_target *ti, unsigned int argc, char **argv);
    void (*dtr)(struct dm_target *ti);
    
    // 映射bio
    int (*map)(struct dm_target *ti, struct bio *bio);
    
    // 完成后处理
    int (*end_io)(struct dm_target *ti, struct bio *bio, blk_status_t *error);
    
    // 状态报告
    void (*status)(struct dm_target *ti, status_type_t type,
                   unsigned status_flags, char *result, unsigned maxlen);
    
    // ...
};

// 简单的dm目标示例
static int my_map(struct dm_target *ti, struct bio *bio)
{
    struct my_target *mt = ti->private;
    
    // 修改bio的目标设备和扇区
    bio_set_dev(bio, mt->dev->bdev);
    bio->bi_iter.bi_sector = mt->start + dm_target_offset(ti, bio->bi_iter.bi_sector);
    
    // 返回DM_MAPIO_REMAPPED让dm提交bio
    return DM_MAPIO_REMAPPED;
}
```

使用dmsetup：
```bash
# 创建线性映射
dmsetup create mydev --table "0 1000000 linear /dev/sda 0"

# 创建条带
dmsetup create stripe --table "0 2000000 striped 2 128 /dev/sda 0 /dev/sdb 0"

# 查看映射
dmsetup table mydev
dmsetup status mydev

# 删除映射
dmsetup remove mydev
```
[Advanced]

---

## 11. NVMe Driver Overview (NVMe驱动概述)

---

Q: What is special about NVMe drivers?
A: NVMe是高性能块设备的标准接口：

```
+------------------------------------------------------------------+
|                    NVMe Architecture                              |
+------------------------------------------------------------------+
|                                                                  |
|  应用程序                                                        |
|     |                                                            |
|     v                                                            |
|  VFS / Block Layer                                               |
|     |                                                            |
|     v                                                            |
|  +----------------------------------------------------------+   |
|  |                   NVMe Driver                             |   |
|  |                                                          |   |
|  |  Submission Queues (SQ)    Completion Queues (CQ)        |   |
|  |  +---+---+---+---+        +---+---+---+---+              |   |
|  |  |SQ0|SQ1|SQ2|...|        |CQ0|CQ1|CQ2|...|              |   |
|  |  +---+---+---+---+        +---+---+---+---+              |   |
|  |      |                         ^                          |   |
|  |      | 写入命令               | 读取完成                   |   |
|  |      v                         |                          |   |
|  |  +---------------------------+--+                         |   |
|  |  |        NVMe Controller        |                        |   |
|  |  |        (PCIe设备)             |                        |   |
|  |  +-------------------------------+                        |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|                   NAND Flash                                     |
|                                                                  |
+------------------------------------------------------------------+
```

NVMe特点：
```c
// NVMe天然支持多队列
// - 每个CPU核心可以有自己的队列对
// - 队列通过Doorbell寄存器通知控制器
// - 支持高达65535个I/O队列

// NVMe命令结构
struct nvme_command {
    __u8 opcode;           // 操作码
    __u8 flags;
    __u16 command_id;       // 命令ID
    __le32 nsid;            // 命名空间ID
    __u64 rsvd2;
    __le64 metadata;        // 元数据指针
    union nvme_data_ptr dptr; // 数据指针
    __le64 slba;            // 起始LBA
    __le16 length;          // 块数
    // ...
};

// NVMe完成条目
struct nvme_completion {
    __le32 result;          // 命令特定结果
    __u32 rsvd;
    __le16 sq_head;         // SQ头指针
    __le16 sq_id;           // SQ ID
    __u16 command_id;       // 命令ID
    __le16 status;          // 状态
};

// NVMe驱动使用blk-mq
// drivers/nvme/host/pci.c
static const struct blk_mq_ops nvme_mq_ops = {
    .queue_rq       = nvme_queue_rq,
    .complete       = nvme_pci_complete_rq,
    .init_hctx      = nvme_init_hctx,
    .init_request   = nvme_init_request,
    .map_queues     = nvme_pci_map_queues,
    .timeout        = nvme_timeout,
    .poll           = nvme_poll,
};
```

NVMe命令：
```bash
# 查看NVMe设备
nvme list

# 查看设备信息
nvme id-ctrl /dev/nvme0
nvme id-ns /dev/nvme0n1

# 查看SMART信息
nvme smart-log /dev/nvme0

# 格式化
nvme format /dev/nvme0n1
```
[Intermediate]

---

## 12. Block Device Debugging (块设备调试)

---

Q: How to debug block device drivers?
A: 块设备调试方法：

```bash
# 查看块设备
lsblk -a
cat /proc/partitions
ls -la /dev/sd* /dev/nvme*

# 查看块设备统计
cat /proc/diskstats
iostat -x 1

# 查看队列信息
cat /sys/block/sda/queue/scheduler
cat /sys/block/sda/queue/nr_requests
cat /sys/block/sda/queue/read_ahead_kb

# 查看blk-mq状态
cat /sys/kernel/debug/block/sda/state
cat /sys/kernel/debug/block/sda/hctx*/queued
cat /sys/kernel/debug/block/sda/hctx*/dispatched

# blktrace跟踪
blktrace -d /dev/sda -o trace
blkparse -i trace
# 或实时
blktrace -d /dev/sda -o - | blkparse -i -

# ftrace跟踪
echo 1 > /sys/kernel/debug/tracing/events/block/enable
cat /sys/kernel/debug/tracing/trace_pipe

# bpftrace
bpftrace -e 'tracepoint:block:block_rq_issue { printf("%s %d %s\n", comm, args->bytes, args->rwbs); }'

# 性能分析
perf record -e block:* -a
perf report
```

常见问题排查：
```c
// 1. 请求未完成
// - 检查blk_mq_start_request是否调用
// - 检查blk_mq_end_request是否调用
// - 检查超时处理

// 2. 性能问题
// - 检查队列深度设置
// - 检查合并设置
// - 检查调度器选择
// - 使用blktrace分析I/O模式

// 3. I/O错误
// - 检查返回状态
// - 检查DMA映射
// - 检查边界条件

// 调试打印
blk_status_t my_queue_rq(...)
{
    pr_debug("Request: sector=%llu, size=%u, dir=%s\n",
             blk_rq_pos(rq), blk_rq_bytes(rq),
             rq_data_dir(rq) ? "WRITE" : "READ");
    // ...
}
```
[Basic]

---

## 13. Summary (总结)

---

Q: Summarize Linux block device driver architecture.
A: 
```
+==================================================================+
||           LINUX BLOCK DEVICE DRIVER ARCHITECTURE               ||
+==================================================================+

核心数据结构:
+------------------------------------------------------------------+
|  gendisk             通用磁盘结构                                 |
|  request_queue       请求队列                                     |
|  blk_mq_tag_set      blk-mq标签集                                |
|  bio                 块I/O请求                                    |
|  request             合并后的请求                                 |
|  block_device_ops    设备操作                                     |
+------------------------------------------------------------------+


I/O处理流程:
+------------------------------------------------------------------+
|  submit_bio()                                                    |
|       |                                                          |
|       v                                                          |
|  blk_mq_submit_bio() --> bio合并/创建request                     |
|       |                                                          |
|       v                                                          |
|  I/O调度器 (mq-deadline/bfq/kyber/none)                         |
|       |                                                          |
|       v                                                          |
|  blk_mq_dispatch_request()                                       |
|       |                                                          |
|       v                                                          |
|  queue_rq() --> 驱动处理 --> 硬件                                |
|       |                                                          |
|       v                                                          |
|  blk_mq_complete_request() --> bi_end_io()                      |
+------------------------------------------------------------------+


blk-mq架构:
+------------------------------------------------------------------+
|  Software Queues (per-CPU)                                       |
|  +--------+  +--------+  +--------+  +--------+                  |
|  | SW Q 0 |  | SW Q 1 |  | SW Q 2 |  | SW Q 3 |                  |
|  +---+----+  +---+----+  +---+----+  +---+----+                  |
|      |           |           |           |                       |
|      +-----------+-----------+-----------+                       |
|                  |                                               |
|  Hardware Queues (映射到硬件)                                    |
|  +--------+      +--------+      +--------+                      |
|  | HW Q 0 |      | HW Q 1 |      | HW Q 2 |                      |
|  +--------+      +--------+      +--------+                      |
+------------------------------------------------------------------+


驱动生命周期:
    模块加载
        |
        +---> register_blkdev()           注册主设备号
        +---> blk_mq_alloc_tag_set()      分配标签集
        +---> blk_mq_alloc_disk()         分配gendisk
        +---> 设置gendisk属性
        +---> add_disk()                  添加磁盘
        |
    运行中
        |
        +---> queue_rq() 处理请求
        +---> blk_mq_end_request() 完成
        |
    模块卸载
        |
        +---> del_gendisk()               删除磁盘
        +---> put_disk()                  释放gendisk
        +---> blk_mq_free_tag_set()       释放标签集
        +---> unregister_blkdev()         注销设备号


关键API:
    alloc_disk() / blk_mq_alloc_disk()   分配磁盘
    add_disk() / del_gendisk()           添加/删除磁盘
    set_capacity()                       设置容量
    blk_mq_alloc/free_tag_set()          标签集管理
    blk_mq_start_request()               开始处理请求
    blk_mq_end_request()                 完成请求
    rq_for_each_segment()                遍历请求段
```
[Basic]

---

*Total: 100+ cards covering Linux kernel block device driver implementation*

