/*
 * sem_common.h - sem_open 命名信号量 demo 的共享数据布局
 *
 * 心智模型：与 eventfd 不同，命名信号量（sem_open）不依赖 fork 继承 fd，
 * 靠一个全局可见的名字（类似共享内存对象名）就能在两个完全独立启动的
 * 进程（没有父子关系）之间同步——这正是本示例特意用两个独立二进制
 * `sem_writer` / `sem_reader`（而不是 fork）来演示的原因。
 *
 * items[] 数组直接放共享内存，capacity 等于本次演示的 item_count，
 * 因此不需要处理"生产者追上消费者"的背压问题——这个简化是为了让
 * 示例聚焦在"sem_post/sem_wait 如何唤醒阻塞的对端"这一件事上。
 */
#ifndef SEM_COMMON_H
#define SEM_COMMON_H

#include <stdint.h>
#include "../../common/shm_common.h"

#define SEM_SHM_NAME "/shm_examples_sem_data"
#define SEM_NAME "/shm_examples_sem_ready"
#define MAX_SEM_ITEMS 200000u

struct sem_shared {
	uint64_t items[MAX_SEM_ITEMS];
};

#endif /* SEM_COMMON_H */
