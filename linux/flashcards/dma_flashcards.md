# DMA Related Structures and API Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] 什么是DMA(Direct Memory Access)？
A: DMA允许设备直接访问内存而无需CPU参与：
```
传统I/O（CPU参与）:
+-------+  读请求   +--------+
|  CPU  |<--------->| 设备   |
+---+---+           +--------+
    |  数据搬运
    v
+-------+
| 内存  |
+-------+

DMA I/O（CPU不参与数据传输）:
+-------+  1.配置DMA  +--------+
|  CPU  |------------>| DMA    |
+-------+             |控制器  |
    |                 +---+----+
    |                     |
    | 2.DMA完成中断        | 3.直接传输数据
    v                     v
+---------------------------+
|          内存              |
+---------------------------+

DMA优点：
1. 释放CPU用于其他任务
2. 更高的数据传输速率
3. 减少CPU中断次数
4. 适合大块数据传输
```

Q: [Basic] Linux DMA映射的类型有哪些？
A: Linux提供两种主要的DMA映射类型：
```
+------------------+---------------------------+---------------------------+
|      类型        |      一致性映射            |      流式映射              |
|                  |  (Coherent/Consistent)    |      (Streaming)          |
+------------------+---------------------------+---------------------------+
| 用途             | 设备和CPU频繁访问          | 单次或少量访问             |
| 生命周期         | 长期存在（驱动生命周期）    | 短期（单次传输）           |
| 分配方式         | dma_alloc_coherent()      | dma_map_single/page/sg()  |
| 缓存一致性       | 硬件自动保证               | 需要显式同步               |
| 性能             | 通常较慢（非缓存）          | 较快（可缓存）             |
| 典型应用         | 描述符环、共享控制结构      | 数据缓冲区                 |
+------------------+---------------------------+---------------------------+

一致性映射 (Coherent):
+-------+                +--------+
|  CPU  |<-------------->| 设备   |
|       |  共享内存区域  |        |
+-------+  (不缓存)      +--------+
     \        |          /
      \       |         /
       +------+--------+
       | DMA缓冲区     |
       | (一致性内存)   |
       +---------------+

流式映射 (Streaming):
      发送前                    接收后
+-------+                    +--------+
|  CPU  |                    | 设备   |
+---+---+                    +---+----+
    |                            |
    v sync_for_device            v sync_for_cpu
+--------+                  +--------+
| 缓冲区 |   DMA传输        | 缓冲区 |
+--------+ ------------->   +--------+
```

Q: [Basic] DMA地址类型dma_addr_t是什么？
A: dma_addr_t是设备看到的总线地址：
```c
/* 定义 */
typedef u64 dma_addr_t;  // 或u32，取决于架构

/* CPU地址 vs DMA地址 */
+------------------------------------------+
|           CPU视角                        |
|  虚拟地址 -> 物理地址                     |
+------------------------------------------+
           |
           | (可能不同！)
           v
+------------------------------------------+
|           设备视角                        |
|  DMA地址（总线地址）                      |
+------------------------------------------+

/* 为什么可能不同？*/
1. IOMMU重映射
2. 系统内存偏移
3. PCI主桥地址翻译

/* 错误示例 */
void *vaddr = kmalloc(size, GFP_KERNEL);
// 错误：直接使用虚拟地址作为DMA地址
dma_addr_t bad_addr = (dma_addr_t)vaddr;  // 错！

// 错误：使用virt_to_phys作为DMA地址
dma_addr_t bad_addr = virt_to_phys(vaddr);  // 可能错！

/* 正确做法 */
dma_addr_t dma_handle;
void *vaddr = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
// dma_handle是设备可用的地址
// vaddr是CPU可用的地址
```

---

## DMA映射方向 (DMA Direction)

Q: [Basic] enum dma_data_direction的值有哪些？
A: DMA方向指定数据传输方向：
```c
/* include/linux/dma-direction.h */
enum dma_data_direction {
    DMA_BIDIRECTIONAL = 0,  // 双向（设备和CPU都可读写）
    DMA_TO_DEVICE = 1,      // CPU -> 设备（发送）
    DMA_FROM_DEVICE = 2,    // 设备 -> CPU（接收）
    DMA_NONE = 3,           // 不使用（用于调试）
};

/* 使用场景 */
DMA_TO_DEVICE:
- 网络发送
- 磁盘写入
- 数据发送到外设

DMA_FROM_DEVICE:
- 网络接收
- 磁盘读取
- 从外设接收数据

DMA_BIDIRECTIONAL:
- 命令/响应缓冲区
- 同一缓冲区用于读写
- 不确定方向时使用（但会影响性能）

/* 为什么方向很重要？*/
1. 缓存同步优化
   DMA_TO_DEVICE: 只需写回CPU缓存
   DMA_FROM_DEVICE: 只需无效化CPU缓存

2. IOMMU权限设置
   DMA_TO_DEVICE: 设备只读
   DMA_FROM_DEVICE: 设备只写

3. 调试和错误检测
   内核可检测方向不匹配
```

---

## 一致性DMA映射 (Coherent DMA Mapping)

Q: [Intermediate] dma_alloc_coherent的用法是什么？
A: dma_alloc_coherent分配一致性DMA内存：
```c
/* 函数原型 */
void *dma_alloc_coherent(struct device *dev,     // 设备
                         size_t size,             // 大小
                         dma_addr_t *dma_handle,  // 输出：DMA地址
                         gfp_t gfp);              // 分配标志

void dma_free_coherent(struct device *dev,
                       size_t size,
                       void *cpu_addr,           // CPU地址
                       dma_addr_t dma_handle);   // DMA地址

/* 使用示例 */
struct my_driver {
    void *desc_ring;        // CPU可访问的虚拟地址
    dma_addr_t desc_dma;    // 设备可访问的DMA地址
    size_t desc_size;
};

int my_probe(struct pci_dev *pdev, ...)
{
    struct my_driver *drv;
    
    drv->desc_size = sizeof(struct descriptor) * RING_SIZE;
    
    /* 分配一致性内存 */
    drv->desc_ring = dma_alloc_coherent(&pdev->dev,
                                        drv->desc_size,
                                        &drv->desc_dma,
                                        GFP_KERNEL);
    if (!drv->desc_ring)
        return -ENOMEM;
    
    /* 将DMA地址告诉设备 */
    writel(lower_32_bits(drv->desc_dma), ioaddr + DESC_ADDR_LO);
    writel(upper_32_bits(drv->desc_dma), ioaddr + DESC_ADDR_HI);
    
    /* CPU直接访问，无需同步 */
    drv->desc_ring[0].control = TX_CTRL_START;
    
    return 0;
}

void my_remove(struct pci_dev *pdev)
{
    struct my_driver *drv = pci_get_drvdata(pdev);
    
    dma_free_coherent(&pdev->dev, drv->desc_size,
                      drv->desc_ring, drv->desc_dma);
}
```

Q: [Intermediate] dma_zalloc_coherent是什么？
A: dma_zalloc_coherent分配并清零一致性内存：
```c
/* 定义（include/linux/dma-mapping.h）*/
static inline void *dma_zalloc_coherent(struct device *dev, size_t size,
                                        dma_addr_t *dma_handle, gfp_t flag)
{
    void *ret = dma_alloc_coherent(dev, size, dma_handle, flag);
    if (ret)
        memset(ret, 0, size);
    return ret;
}

/* 使用场景 */
// 当需要确保缓冲区初始化为零时使用
// 例如：描述符环初始化

struct my_desc *desc = dma_zalloc_coherent(dev, 
                                           sizeof(struct my_desc) * count,
                                           &dma_handle,
                                           GFP_KERNEL);

/* 注意：较新内核中已被废弃 */
// 使用 dma_alloc_coherent + GFP_ZERO
void *buf = dma_alloc_coherent(dev, size, &dma, GFP_KERNEL | __GFP_ZERO);
```

Q: [Advanced] 一致性内存的特性和限制是什么？
A: 一致性内存的重要特性：
```c
/* 特性 */
1. 缓存一致性
   - CPU和设备看到相同的数据
   - 无需手动同步

2. 内存类型
   - 通常是非缓存(uncached)或写合并(write-combining)
   - 比普通内存访问慢

3. 对齐要求
   - 通常页对齐
   - 某些架构有特殊要求

/* 限制 */
1. 资源限制
   - 某些平台上DMA内存区域有限
   - 可能失败

2. 性能
   - 非缓存访问比缓存访问慢10-100倍
   - 不适合大量CPU访问的数据

3. 地址限制
   - 受设备DMA掩码限制
   - 可能需要SWIOTLB

/* 最佳实践 */
// 1. 仅用于设备和CPU共享的控制结构
struct tx_desc {
    __le64 buffer_addr;   // 数据缓冲区DMA地址
    __le32 length;
    __le32 status;        // 设备更新，CPU读取
};

// 2. 数据缓冲区使用流式映射
void *data_buf = kmalloc(size, GFP_KERNEL);
dma_addr_t data_dma = dma_map_single(dev, data_buf, size, DMA_TO_DEVICE);

// 3. 批量分配以减少调用次数
desc_ring = dma_alloc_coherent(dev, desc_count * sizeof(*desc), ...);
```

---

## 流式DMA映射 (Streaming DMA Mapping)

Q: [Intermediate] dma_map_single的用法是什么？
A: dma_map_single映射单个连续缓冲区：
```c
/* 函数原型 */
dma_addr_t dma_map_single(struct device *dev,
                          void *cpu_addr,          // CPU虚拟地址
                          size_t size,
                          enum dma_data_direction dir);

void dma_unmap_single(struct device *dev,
                      dma_addr_t dma_addr,
                      size_t size,
                      enum dma_data_direction dir);

/* 使用示例：发送数据 */
void send_packet(struct device *dev, void *data, size_t len)
{
    dma_addr_t dma_handle;
    
    /* 映射缓冲区 */
    dma_handle = dma_map_single(dev, data, len, DMA_TO_DEVICE);
    
    /* 检查映射错误 */
    if (dma_mapping_error(dev, dma_handle)) {
        dev_err(dev, "DMA mapping failed\n");
        return;
    }
    
    /* 告诉设备DMA地址 */
    write_to_device(dma_handle, len);
    
    /* 启动传输 */
    start_transfer();
    
    /* 等待完成（或在中断中处理）*/
    wait_for_completion();
    
    /* 取消映射 */
    dma_unmap_single(dev, dma_handle, len, DMA_TO_DEVICE);
}

/* 使用示例：接收数据 */
void receive_packet(struct device *dev, void *data, size_t len)
{
    dma_addr_t dma_handle;
    
    /* 映射缓冲区用于接收 */
    dma_handle = dma_map_single(dev, data, len, DMA_FROM_DEVICE);
    if (dma_mapping_error(dev, dma_handle))
        return;
    
    /* 设置设备进行接收 */
    setup_receive(dma_handle, len);
    
    /* 等待接收完成 */
    wait_for_receive();
    
    /* 取消映射后CPU才能访问数据 */
    dma_unmap_single(dev, dma_handle, len, DMA_FROM_DEVICE);
    
    /* 现在可以处理data中的数据 */
    process_data(data, len);
}
```

Q: [Intermediate] dma_map_page的用法是什么？
A: dma_map_page映射物理页的一部分：
```c
/* 函数原型 */
dma_addr_t dma_map_page(struct device *dev,
                        struct page *page,
                        unsigned long offset,    // 页内偏移
                        size_t size,
                        enum dma_data_direction dir);

void dma_unmap_page(struct device *dev,
                    dma_addr_t dma_addr,
                    size_t size,
                    enum dma_data_direction dir);

/* 使用场景 */
// 1. 网络驱动中的skb片段
static dma_addr_t skb_frag_dma_map(struct device *dev,
                                   const skb_frag_t *frag,
                                   size_t offset, size_t size,
                                   enum dma_data_direction dir)
{
    return dma_map_page(dev, skb_frag_page(frag),
                        frag->page_offset + offset, size, dir);
}

/* 2. 高端内存页 */
struct page *page = alloc_page(GFP_KERNEL);
void *vaddr = kmap(page);  // 临时映射用于CPU访问
/* ... 填充数据 ... */
kunmap(page);

// 直接用page映射，不需要虚拟地址
dma_addr_t dma = dma_map_page(dev, page, 0, PAGE_SIZE, DMA_TO_DEVICE);

/* dma_map_single vs dma_map_page */
// dma_map_single使用虚拟地址，内部转换为page+offset
// dma_map_page直接使用page结构
// 对于高端内存，必须使用dma_map_page
```

Q: [Intermediate] DMA同步函数的作用是什么？
A: 同步函数维护CPU和设备间的缓存一致性：
```c
/* 同步函数 */
void dma_sync_single_for_cpu(struct device *dev,
                             dma_addr_t dma_handle,
                             size_t size,
                             enum dma_data_direction dir);

void dma_sync_single_for_device(struct device *dev,
                                dma_addr_t dma_handle,
                                size_t size,
                                enum dma_data_direction dir);

/* 为什么需要同步？*/
// 流式映射通常使用可缓存内存
// CPU缓存和设备看到的内存可能不一致

/* 同步时机 */
1. sync_for_device: CPU修改后，设备访问前
   - 确保CPU写入对设备可见
   - 清理CPU缓存到内存

2. sync_for_cpu: 设备修改后，CPU访问前
   - 确保设备写入对CPU可见
   - 无效化CPU缓存

/* 使用示例：共享缓冲区 */
dma_addr_t dma = dma_map_single(dev, buf, size, DMA_BIDIRECTIONAL);

/* 第一轮：CPU填充，设备处理 */
fill_buffer(buf);
dma_sync_single_for_device(dev, dma, size, DMA_TO_DEVICE);
start_device_processing();
wait_completion();
dma_sync_single_for_cpu(dev, dma, size, DMA_FROM_DEVICE);
read_results(buf);

/* 第二轮：重用同一缓冲区 */
fill_buffer(buf);
dma_sync_single_for_device(dev, dma, size, DMA_TO_DEVICE);
// ... 继续 ...

/* 最后取消映射 */
dma_unmap_single(dev, dma, size, DMA_BIDIRECTIONAL);
```

---

## Scatter-Gather列表 (Scatterlist)

Q: [Intermediate] struct scatterlist的结构是什么？
A: scatterlist描述分散的内存片段：
```c
/* 结构定义（arch/xxx/include/asm/scatterlist.h）*/
struct scatterlist {
    unsigned long page_link;  // 页指针(含链表标志)
    unsigned int offset;      // 页内偏移
    unsigned int length;      // 数据长度
    dma_addr_t dma_address;   // DMA地址（映射后填充）
#ifdef CONFIG_DEBUG_SG
    unsigned long sg_magic;
#endif
};

/* page_link的低位标志 */
// bit 0: 1=链表指针，0=实际页
// bit 1: 1=列表最后一项

/* 散列表表头 */
struct sg_table {
    struct scatterlist *sgl;   // 散列表指针
    unsigned int nents;        // 映射后的条目数
    unsigned int orig_nents;   // 原始条目数
};

/* Scatter-Gather的作用 */
物理内存不连续的情况：
+--------+        +--------+        +--------+
| Page A |        | Page B |        | Page C |
+--------+        +--------+        +--------+
     |                |                 |
     v                v                 v
+----------+  +----------+  +----------+
| sg[0]    |->| sg[1]    |->| sg[2]    |
| page=A   |  | page=B   |  | page=C   |
| offset=0 |  | offset=0 |  | offset=0 |
| length=  |  | length=  |  | length=  |
| 4096     |  | 4096     |  | 2048     |
+----------+  +----------+  +----------+

设备使用SG DMA一次传输所有片段
```

Q: [Intermediate] scatterlist的操作API有哪些？
A: Linux提供完整的scatterlist操作API：
```c
/* 初始化 */
void sg_init_table(struct scatterlist *sgl, unsigned int nents);
void sg_init_one(struct scatterlist *sg, const void *buf, unsigned int buflen);

/* 设置页/缓冲区 */
void sg_set_page(struct scatterlist *sg, struct page *page,
                 unsigned int len, unsigned int offset);
void sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen);

/* 获取页和地址 */
struct page *sg_page(struct scatterlist *sg);
void *sg_virt(struct scatterlist *sg);
dma_addr_t sg_phys(struct scatterlist *sg);
dma_addr_t sg_dma_address(struct scatterlist *sg);
unsigned int sg_dma_len(struct scatterlist *sg);

/* 遍历 */
struct scatterlist *sg_next(struct scatterlist *sg);
struct scatterlist *sg_last(struct scatterlist *s, unsigned int nents);
void sg_mark_end(struct scatterlist *sg);

/* 遍历宏 */
#define for_each_sg(sglist, sg, nr, __i) \
    for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

/* 使用示例 */
#define NUM_ENTRIES 3
struct scatterlist sg[NUM_ENTRIES];

sg_init_table(sg, NUM_ENTRIES);

/* 设置每个条目 */
sg_set_buf(&sg[0], buf0, len0);
sg_set_buf(&sg[1], buf1, len1);
sg_set_buf(&sg[2], buf2, len2);

/* 遍历散列表 */
struct scatterlist *s;
int i;
for_each_sg(sg, s, NUM_ENTRIES, i) {
    pr_info("sg[%d]: page=%p, offset=%u, len=%u\n",
            i, sg_page(s), s->offset, s->length);
}
```

Q: [Intermediate] dma_map_sg的用法是什么？
A: dma_map_sg映射整个散列表：
```c
/* 函数原型 */
int dma_map_sg(struct device *dev,
               struct scatterlist *sg,
               int nents,                    // 条目数
               enum dma_data_direction dir);
// 返回值：实际映射的条目数（可能小于nents，因为合并）

void dma_unmap_sg(struct device *dev,
                  struct scatterlist *sg,
                  int nents,
                  enum dma_data_direction dir);

/* 同步函数 */
void dma_sync_sg_for_cpu(struct device *dev,
                         struct scatterlist *sg,
                         int nents,
                         enum dma_data_direction dir);

void dma_sync_sg_for_device(struct device *dev,
                            struct scatterlist *sg,
                            int nents,
                            enum dma_data_direction dir);

/* 完整使用示例 */
#define MAX_SG 64
struct scatterlist sg[MAX_SG];
int nents = 0;

/* 构建散列表 */
sg_init_table(sg, MAX_SG);
for (each buffer) {
    sg_set_page(&sg[nents], page, len, offset);
    nents++;
}

/* 映射 */
int mapped = dma_map_sg(dev, sg, nents, DMA_TO_DEVICE);
if (mapped == 0) {
    dev_err(dev, "SG mapping failed\n");
    return -ENOMEM;
}

/* 设置设备 */
struct scatterlist *s;
int i;
for_each_sg(sg, s, mapped, i) {
    dma_addr_t addr = sg_dma_address(s);
    unsigned int len = sg_dma_len(s);
    
    /* 填充设备描述符 */
    desc[i].addr = addr;
    desc[i].len = len;
}

/* 启动传输 */
start_dma_transfer();

/* 等待完成 */
wait_for_completion();

/* 取消映射（使用原始nents）*/
dma_unmap_sg(dev, sg, nents, DMA_TO_DEVICE);
```

Q: [Advanced] SG链表(Chained SG)的用法是什么？
A: sg_chain连接多个散列表：
```c
/* 链接函数 */
void sg_chain(struct scatterlist *prv, unsigned int prv_nents,
              struct scatterlist *sgl);

/* 为什么需要链表？*/
// 当sg条目数超过预分配大小时
// 避免一次分配大量连续内存

/* 使用示例 */
#define SG_CHUNK_SIZE 64

struct scatterlist sg1[SG_CHUNK_SIZE];
struct scatterlist sg2[SG_CHUNK_SIZE];

sg_init_table(sg1, SG_CHUNK_SIZE);
sg_init_table(sg2, SG_CHUNK_SIZE);

/* 填充sg1和sg2 */
for (i = 0; i < SG_CHUNK_SIZE - 1; i++) {
    sg_set_page(&sg1[i], pages1[i], PAGE_SIZE, 0);
}
for (i = 0; i < SG_CHUNK_SIZE; i++) {
    sg_set_page(&sg2[i], pages2[i], PAGE_SIZE, 0);
}

/* 链接sg1[63]到sg2（sg1最后一个槽用于链接）*/
sg_chain(sg1, SG_CHUNK_SIZE, sg2);

/* 遍历整个链 */
struct scatterlist *s;
int i = 0;
for (s = sg1; s; s = sg_next(s)) {
    if (!sg_is_chain(s)) {
        pr_info("Entry %d: page=%p\n", i++, sg_page(s));
    }
}

/* 使用sg_alloc_table自动处理链表 */
struct sg_table sgt;
int ret = sg_alloc_table(&sgt, num_pages, GFP_KERNEL);
if (ret)
    return ret;

/* 填充 */
struct scatterlist *s;
int i;
for_each_sg(sgt.sgl, s, sgt.orig_nents, i) {
    sg_set_page(s, pages[i], PAGE_SIZE, 0);
}

/* 释放 */
sg_free_table(&sgt);
```

---

## DMA池 (DMA Pool)

Q: [Intermediate] DMA池的用途是什么？
A: DMA池用于分配小块一致性DMA内存：
```c
/* 为什么需要DMA池？*/
// dma_alloc_coherent通常分配页对齐的整页
// 对于小的描述符（如32/64字节）浪费严重
// DMA池从大块中切割小块

/* API */
struct dma_pool *dma_pool_create(const char *name,
                                 struct device *dev,
                                 size_t size,       // 每个对象大小
                                 size_t align,      // 对齐要求
                                 size_t boundary);  // 不跨越的边界

void dma_pool_destroy(struct dma_pool *pool);

void *dma_pool_alloc(struct dma_pool *pool,
                     gfp_t mem_flags,
                     dma_addr_t *handle);

void dma_pool_free(struct dma_pool *pool,
                   void *vaddr,
                   dma_addr_t dma);

/* 使用示例 */
struct my_driver {
    struct dma_pool *desc_pool;
};

/* 创建池 */
int my_probe(struct pci_dev *pdev, ...)
{
    drv->desc_pool = dma_pool_create("my_desc",
                                     &pdev->dev,
                                     sizeof(struct my_desc),  // 64字节
                                     8,                       // 8字节对齐
                                     0);                      // 无边界限制
    if (!drv->desc_pool)
        return -ENOMEM;
    
    return 0;
}

/* 分配描述符 */
struct my_desc *alloc_desc(struct my_driver *drv, dma_addr_t *dma)
{
    return dma_pool_alloc(drv->desc_pool, GFP_ATOMIC, dma);
}

/* 释放描述符 */
void free_desc(struct my_driver *drv, struct my_desc *desc, dma_addr_t dma)
{
    dma_pool_free(drv->desc_pool, desc, dma);
}

/* 销毁池 */
void my_remove(struct pci_dev *pdev)
{
    dma_pool_destroy(drv->desc_pool);
}
```

Q: [Intermediate] 托管DMA池(Managed DMA Pool)是什么？
A: dmam_pool_create自动管理资源：
```c
/* API */
struct dma_pool *dmam_pool_create(const char *name,
                                  struct device *dev,
                                  size_t size,
                                  size_t align,
                                  size_t boundary);
void dmam_pool_destroy(struct dma_pool *pool);

/* 优点 */
// 设备移除时自动销毁
// 简化错误处理路径
// 与devres框架集成

/* 使用示例 */
int my_probe(struct pci_dev *pdev, ...)
{
    struct dma_pool *pool;
    
    pool = dmam_pool_create("my_desc", &pdev->dev, 64, 8, 0);
    if (!pool)
        return -ENOMEM;
    
    // 不需要在remove中销毁
    // 设备移除时自动清理
    
    return 0;
}

/* 托管 vs 非托管 */
// 托管版本：dmam_pool_create
//   - 自动清理
//   - 简化错误处理
//   - 推荐用于驱动探测

// 非托管版本：dma_pool_create
//   - 手动管理生命周期
//   - 用于需要精确控制的场景
```

---

## DMA掩码 (DMA Mask)

Q: [Intermediate] DMA掩码的作用是什么？
A: DMA掩码定义设备可寻址的物理地址范围：
```c
/* 设置DMA掩码 */
int dma_set_mask(struct device *dev, u64 mask);
int dma_set_coherent_mask(struct device *dev, u64 mask);
int dma_set_mask_and_coherent(struct device *dev, u64 mask);

/* 获取DMA掩码 */
u64 dma_get_mask(struct device *dev);

/* 常用掩码 */
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

DMA_BIT_MASK(32)  // 0xffffffff - 4GB
DMA_BIT_MASK(64)  // 0xffffffffffffffff - 全地址空间
DMA_BIT_MASK(24)  // 0xffffff - 16MB (ISA DMA)
DMA_BIT_MASK(28)  // 0xfffffff - 256MB
DMA_BIT_MASK(31)  // 0x7fffffff - 2GB

/* 使用示例 */
int my_probe(struct pci_dev *pdev, ...)
{
    /* 尝试64位DMA */
    if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64))) {
        dev_info(&pdev->dev, "Using 64-bit DMA\n");
    } 
    /* 回退到32位DMA */
    else if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))) {
        dev_info(&pdev->dev, "Using 32-bit DMA\n");
    } 
    else {
        dev_err(&pdev->dev, "No suitable DMA available\n");
        return -EIO;
    }
    
    return 0;
}

/* 为什么有两种掩码？*/
// dma_mask: 流式映射使用
// coherent_dma_mask: 一致性映射使用
// 某些平台两者可能不同

/* 检查DMA支持 */
int dma_supported(struct device *dev, u64 mask);
```

---

## DMA引擎 (DMA Engine)

Q: [Advanced] DMA Engine子系统是什么？
A: DMA Engine提供通用的DMA控制器框架：
```c
/* DMA通道结构 */
struct dma_chan {
    struct dma_device *device;    // 所属设备
    dma_cookie_t cookie;          // 传输cookie
    int chan_id;                  // 通道ID
    struct dma_chan_dev *dev;     // sysfs设备
    struct list_head device_node; // 设备链表
    struct dma_chan_percpu __percpu *local;
    int client_count;
    int table_count;
    void *private;
};

/* DMA设备操作 */
struct dma_device {
    /* 能力 */
    dma_cap_mask_t cap_mask;
    
    /* 操作函数 */
    struct dma_async_tx_descriptor *(*device_prep_dma_memcpy)(
        struct dma_chan *chan, dma_addr_t dest, dma_addr_t src,
        size_t len, unsigned long flags);
    
    struct dma_async_tx_descriptor *(*device_prep_slave_sg)(
        struct dma_chan *chan, struct scatterlist *sgl,
        unsigned int sg_len, enum dma_data_direction direction,
        unsigned long flags);
    
    struct dma_async_tx_descriptor *(*device_prep_dma_cyclic)(
        struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
        size_t period_len, enum dma_data_direction direction);
    
    int (*device_control)(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
                          unsigned long arg);
    
    enum dma_status (*device_tx_status)(struct dma_chan *chan,
                                        dma_cookie_t cookie,
                                        struct dma_tx_state *txstate);
    
    void (*device_issue_pending)(struct dma_chan *chan);
};
```

Q: [Advanced] 如何使用DMA Engine进行内存复制？
A: DMA Engine的memcpy使用示例：
```c
#include <linux/dmaengine.h>

/* 获取DMA通道 */
struct dma_chan *chan;
dma_cap_mask_t mask;

dma_cap_zero(mask);
dma_cap_set(DMA_MEMCPY, mask);

chan = dma_request_channel(mask, NULL, NULL);
if (!chan) {
    pr_err("Failed to request DMA channel\n");
    return -ENODEV;
}

/* 准备DMA操作 */
struct dma_async_tx_descriptor *tx;
dma_addr_t src_dma, dst_dma;
dma_cookie_t cookie;

// 映射源和目标缓冲区
src_dma = dma_map_single(chan->device->dev, src, len, DMA_TO_DEVICE);
dst_dma = dma_map_single(chan->device->dev, dst, len, DMA_FROM_DEVICE);

// 准备memcpy描述符
tx = chan->device->device_prep_dma_memcpy(chan, dst_dma, src_dma, len,
                                          DMA_CTRL_ACK);
if (!tx) {
    dma_unmap_single(...);
    return -ENOMEM;
}

/* 设置回调 */
tx->callback = my_dma_callback;
tx->callback_param = my_data;

/* 提交传输 */
cookie = dmaengine_submit(tx);

/* 启动传输 */
dma_async_issue_pending(chan);

/* 等待完成 */
enum dma_status status;
status = dma_sync_wait(chan, cookie);
// 或使用回调异步处理

/* 检查状态 */
if (status == DMA_COMPLETE) {
    pr_info("DMA transfer completed\n");
}

/* 取消映射 */
dma_unmap_single(chan->device->dev, src_dma, len, DMA_TO_DEVICE);
dma_unmap_single(chan->device->dev, dst_dma, len, DMA_FROM_DEVICE);

/* 释放通道 */
dma_release_channel(chan);
```

Q: [Advanced] 如何使用DMA Engine进行slave传输？
A: slave模式用于外设DMA传输：
```c
/* 从设备传输配置 */
struct dma_slave_config config = {
    .direction = DMA_MEM_TO_DEV,  // 内存到外设
    .dst_addr = dev->fifo_phys,   // 外设FIFO物理地址
    .dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,
    .dst_maxburst = 8,
};

dmaengine_slave_config(chan, &config);

/* 准备slave SG传输 */
struct scatterlist sg;
sg_init_one(&sg, buf, len);

dma_map_sg(chan->device->dev, &sg, 1, DMA_TO_DEVICE);

tx = dmaengine_prep_slave_sg(chan, &sg, 1, DMA_MEM_TO_DEV,
                             DMA_PREP_INTERRUPT | DMA_CTRL_ACK);

tx->callback = slave_callback;
tx->callback_param = my_data;

cookie = dmaengine_submit(tx);
dma_async_issue_pending(chan);

/* 循环DMA（如音频）*/
tx = dmaengine_prep_dma_cyclic(chan,
                               buf_dma,       // 缓冲区DMA地址
                               buf_len,       // 总长度
                               period_len,    // 周期长度
                               DMA_MEM_TO_DEV);

// 每个周期完成时调用回调

/* 终止传输 */
dmaengine_terminate_all(chan);

/* 暂停/恢复 */
dmaengine_pause(chan);
dmaengine_resume(chan);
```

---

## IOMMU和SWIOTLB

Q: [Advanced] IOMMU对DMA的影响是什么？
A: IOMMU提供设备地址翻译和隔离：
```c
/* IOMMU功能 */
1. 地址翻译
   - 设备看到的地址(IOVA) != 物理地址
   - 允许设备访问不连续的物理内存

2. 内存保护
   - 限制设备只能访问授权的内存
   - 防止恶意或错误的DMA

3. 虚拟化支持
   - 虚拟机设备直通(passthrough)
   - 虚拟IOMMU(vIOMMU)

/* 无IOMMU */
设备 --> 物理地址 --> 内存
        (1:1映射)

/* 有IOMMU */
设备 --> IOVA --> IOMMU --> 物理地址 --> 内存
                  翻译表

/* 驱动程序角度 */
// 使用标准DMA API，无需关心IOMMU存在与否
// dma_map_xxx()函数会自动处理

dma_addr_t dma = dma_map_single(dev, buf, size, dir);
// 有IOMMU: dma可能是IOVA
// 无IOMMU: dma就是物理地址

/* IOMMU组 */
// 共享IOMMU翻译的设备集合
#include <linux/iommu.h>

struct iommu_group *group = iommu_group_get(&pdev->dev);
```

Q: [Advanced] SWIOTLB是什么？
A: SWIOTLB是软件回弹缓冲区：
```c
/* 为什么需要SWIOTLB？*/
// 当物理内存超出设备DMA能力时
// 例如：32位DMA设备访问高于4GB的内存

/* 工作原理 */
物理内存布局：
0GB                     4GB                    8GB
+------------------------+------------------------+
|     低内存             |      高内存            |
|  设备可DMA             |  设备不可DMA           |
+------------------------+------------------------+

SWIOTLB操作：
1. 驱动请求映射高内存缓冲区
2. SWIOTLB分配低内存bounce buffer
3. 数据在高内存和bounce buffer间复制
4. 设备DMA到/从bounce buffer

发送数据：
高内存buf  --memcpy-->  bounce buffer  --DMA-->  设备
(CPU)                   (低内存)

接收数据：
设备  --DMA-->  bounce buffer  --memcpy-->  高内存buf
                (低内存)                     (CPU)

/* SWIOTLB配置 */
// 内核启动参数
swiotlb=65536  // 设置SWIOTLB大小（以页为单位）

/* 检查是否使用SWIOTLB */
// 查看dmesg
// $ dmesg | grep -i swiotlb
// swiotlb: mapped at virtual address ...

/* 性能影响 */
// SWIOTLB涉及内存复制，性能低于直接DMA
// 尽量使用64位DMA或IOMMU避免SWIOTLB
```

---

## 调试和最佳实践 (Debugging and Best Practices)

Q: [Intermediate] DMA调试工具有哪些？
A: Linux提供DMA调试机制：
```c
/* 内核配置 */
CONFIG_DMA_API_DEBUG=y     // 启用DMA调试
CONFIG_DMA_API_DEBUG_SG=y  // SG调试

/* 调试信息 */
// $ cat /sys/kernel/debug/dma-api/errors
// 显示DMA API使用错误

// $ cat /sys/kernel/debug/dma-api/num_errors
// 错误计数

// $ cat /sys/kernel/debug/dma-api/all_errors
// 详细错误信息

/* 常见错误 */
1. 方向不匹配
   mapped as DMA_TO_DEVICE, synced as DMA_FROM_DEVICE

2. 重复取消映射
   unmapping already unmapped memory

3. 映射后未取消映射
   device driver has pending DMA allocations

4. 越界访问
   DMA-API: out of range in sync operation

/* 调试函数 */
void debug_dma_alloc_coherent(struct device *dev, size_t size,
                              dma_addr_t dma_addr, void *virt);
void debug_dma_map_page(struct device *dev, struct page *page,
                        size_t offset, size_t size, int direction,
                        dma_addr_t dma_addr, bool map_single);
// 等等

/* ftrace */
$ echo 1 > /sys/kernel/debug/tracing/events/dma/enable
$ cat /sys/kernel/debug/tracing/trace
```

Q: [Intermediate] DMA编程的最佳实践是什么？
A: 遵循以下最佳实践：
```c
/* 1. 检查映射错误 */
dma_addr_t dma = dma_map_single(dev, buf, size, dir);
if (dma_mapping_error(dev, dma)) {
    dev_err(dev, "DMA mapping failed\n");
    return -ENOMEM;
}

/* 2. 使用正确的DMA掩码 */
// 优先尝试64位，回退到32位
if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64)))
    if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32)))
        return -EIO;

/* 3. 方向一致性 */
// map和unmap/sync必须使用相同方向
dma = dma_map_single(dev, buf, size, DMA_TO_DEVICE);
// ...
dma_unmap_single(dev, dma, size, DMA_TO_DEVICE);  // 相同方向！

/* 4. 先sync再访问 */
// CPU访问前
dma_sync_single_for_cpu(dev, dma, size, DMA_FROM_DEVICE);
data = buffer[0];  // 现在安全

// 设备访问前
buffer[0] = data;
dma_sync_single_for_device(dev, dma, size, DMA_TO_DEVICE);
// 现在可以告诉设备开始DMA

/* 5. 一致性映射用于共享结构 */
// 描述符环：一致性映射
desc_ring = dma_alloc_coherent(dev, size, &desc_dma, GFP_KERNEL);

// 数据缓冲区：流式映射
data_dma = dma_map_single(dev, data, len, DMA_TO_DEVICE);

/* 6. 使用DMA池分配小对象 */
// 不要为64字节对象使用dma_alloc_coherent
pool = dma_pool_create("descs", dev, 64, 8, 0);
desc = dma_pool_alloc(pool, GFP_KERNEL, &dma);

/* 7. 清理资源 */
// remove函数中取消所有映射
// 销毁DMA池
// 释放一致性内存

/* 8. 使用托管资源 */
// 简化错误处理和清理
buf = dmam_alloc_coherent(dev, size, &dma, GFP_KERNEL);
pool = dmam_pool_create("descs", dev, 64, 8, 0);
```

Q: [Intermediate] PCI设备的DMA设置流程是什么？
A: PCI设备DMA的典型初始化：
```c
static int my_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int ret;
    void __iomem *ioaddr;
    struct my_device *dev;

    /* 1. 启用PCI设备 */
    ret = pci_enable_device(pdev);
    if (ret)
        return ret;

    /* 2. 请求MMIO区域 */
    ret = pci_request_regions(pdev, "my_driver");
    if (ret)
        goto err_disable;

    /* 3. 设置DMA掩码 */
    if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64))) {
        if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))) {
            dev_err(&pdev->dev, "No suitable DMA configuration\n");
            ret = -EIO;
            goto err_release;
        }
    }

    /* 4. 启用总线主控 */
    pci_set_master(pdev);

    /* 5. 映射MMIO */
    ioaddr = pci_iomap(pdev, 0, 0);
    if (!ioaddr) {
        ret = -ENOMEM;
        goto err_release;
    }

    /* 6. 分配一致性内存（描述符环）*/
    dev->desc_ring = dma_alloc_coherent(&pdev->dev,
                                        sizeof(struct desc) * RING_SIZE,
                                        &dev->desc_dma,
                                        GFP_KERNEL);
    if (!dev->desc_ring) {
        ret = -ENOMEM;
        goto err_unmap;
    }

    /* 7. 创建DMA池（小对象）*/
    dev->buf_pool = dma_pool_create("buffers", &pdev->dev, 
                                    BUF_SIZE, 8, 0);
    if (!dev->buf_pool) {
        ret = -ENOMEM;
        goto err_free_desc;
    }

    /* 8. 初始化完成 */
    pci_set_drvdata(pdev, dev);
    return 0;

err_free_desc:
    dma_free_coherent(&pdev->dev, sizeof(struct desc) * RING_SIZE,
                      dev->desc_ring, dev->desc_dma);
err_unmap:
    pci_iounmap(pdev, ioaddr);
err_release:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
    return ret;
}

static void my_pci_remove(struct pci_dev *pdev)
{
    struct my_device *dev = pci_get_drvdata(pdev);

    /* 反序清理 */
    dma_pool_destroy(dev->buf_pool);
    dma_free_coherent(&pdev->dev, sizeof(struct desc) * RING_SIZE,
                      dev->desc_ring, dev->desc_dma);
    pci_iounmap(pdev, dev->ioaddr);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
}
```

