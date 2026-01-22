/*
场景说明
list_head是内核最基础的双向链表结构，内核通过list_for_each/list_for_each_entry等宏实现迭代器模式 —— 上层仅需调用宏即可遍历链表，无需关心list_head的内部节点指针移动逻辑，且支持正向 / 反向遍历、安全遍历（删除节点时不崩溃）

核心说明
内核通过list_for_each_entry宏实现 “业务结构体迭代器”，封装了container_of（节点转结构体）、next指针移动、边界判断（&pos->member != head）等细节；
上层仅需指定 “迭代变量、链表头、结构体中的 list 成员名”，即可遍历所有进程，完全无需关心链表内部实现；
内核还提供list_for_each_entry_safe（删除节点时安全遍历）、list_for_each_entry_reverse（反向遍历）等变体迭代器，满足不同场景需求
*/

#include <stdio.h>
#include <stdlib.h>

// 模拟内核list_head双向链表节点
struct list_head {
    struct list_head *next, *prev;
};

// 模拟内核容器_of宏：通过成员指针获取结构体指针（核心）
#define container_of(ptr, type, member) ({          \
    const typeof(((type*)0)->member) *__mptr = (ptr);\
    (type*)((char*)__mptr - offsetof(type, member));})

// 模拟内核offsetof宏：获取结构体成员偏移量
#define offsetof(type, member) ((size_t)&((type*)0)->member)

// 初始化链表节点
#define INIT_LIST_HEAD(ptr) do {                    \
    (ptr)->next = (ptr); (ptr)->prev = (ptr);       \
} while (0)

// ---------------------- 迭代器宏1：遍历list_head节点（基础迭代器） ----------------------
#define list_for_each(pos, head)                    \
    for (pos = (head)->next; pos != (head); pos = pos->next)

// ---------------------- 迭代器宏2：遍历链表中的业务结构体（常用） ----------------------
#define list_for_each_entry(pos, head, member)      \
    for (pos = container_of((head)->next, typeof(*pos), member); \
         &pos->member != (head);                    \
         pos = container_of(pos->member.next, typeof(*pos), member))

// ---------------------- 业务结构体：进程信息（模拟task_struct） ----------------------
struct task_struct {
    int pid;                // 进程ID
    char name[32];          // 进程名
    struct list_head list;  // 链表节点（嵌入到业务结构体）
};

// 主函数测试：遍历进程链表
int main() {
    // 1. 初始化链表头
    struct list_head task_list;
    INIT_LIST_HEAD(&task_list);

    // 2. 创建3个进程节点并加入链表
    struct task_struct t1 = {.pid = 1, .name = "init"};
    struct task_struct t2 = {.pid = 2, .name = "kthreadd"};
    struct task_struct t3 = {.pid = 3, .name = "rcu_sched"};
    INIT_LIST_HEAD(&t1.list);
    INIT_LIST_HEAD(&t2.list);
    INIT_LIST_HEAD(&t3.list);

    // 加入链表（内核标准操作）
    t1.list.next = &t2.list; t2.list.prev = &t1.list;
    t2.list.next = &t3.list; t3.list.prev = &t2.list;
    t3.list.next = &task_list; task_list.prev = &t3.list;
    task_list.next = &t1.list; t1.list.prev = &task_list;

    // 3. 使用迭代器宏遍历进程链表（上层无需关心内部指针）
    printf("=== 遍历进程链表 ===\n");
    struct task_struct *pos;
    list_for_each_entry(pos, &task_list, list) {
        printf("进程PID：%d，名称：%s\n", pos->pid, pos->name);
    }

    return 0;
}