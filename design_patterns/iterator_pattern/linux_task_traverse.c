/*
场景说明
内核中遍历所有进程是迭代器模式的典型实战场景 —— 通过for_each_process宏（封装list_for_each_entry）遍历全局进程链表，无需关心task_struct的链表结构，即可获取所有进程的信息（如 PID、状态、内存占用）

核心说明
for_each_process是内核封装的高层迭代器宏，底层基于list_for_each_entry，进一步简化了进程遍历的代码；
上层可在迭代过程中添加任意筛选逻辑（如按状态、PID 范围筛选），迭代器仅负责提供统一的遍历接口
*/

#include <stdio.h>
#include <stdlib.h>

// 复用案例1的list_head和container_of宏
struct list_head {
    struct list_head *next, *prev;
};
#define offsetof(type, member) ((size_t)&((type*)0)->member)
#define container_of(ptr, type, member) ({          \
    const typeof(((type*)0)->member) *__mptr = (ptr);\
    (type*)((char*)__mptr - offsetof(type, member));})
#define list_for_each_entry(pos, head, member)      \
    for (pos = container_of((head)->next, typeof(*pos), member); \
         &pos->member != (head);                    \
         pos = container_of(pos->member.next, typeof(*pos), member))

// 模拟内核全局进程链表头
struct list_head init_task_list;

// 模拟内核for_each_process迭代器宏（封装list_for_each_entry）
#define for_each_process(p) \
    list_for_each_entry(p, &init_task_list, list)

// 模拟task_struct
struct task_struct {
    int pid;
    char state; // 进程状态：R(运行)/S(睡眠)/Z(僵尸)
    struct list_head list;
};

// 主函数测试：遍历所有进程并筛选运行态进程
int main() {
    // 1. 初始化全局进程链表并添加测试进程
    INIT_LIST_HEAD(&init_task_list);
    struct task_struct t1 = {.pid = 1, .state = 'R', .list = {&t2.list, &init_task_list}};
    struct task_struct t2 = {.pid = 2, .state = 'S', .list = {&t3.list, &t1.list}};
    struct task_struct t3 = {.pid = 3, .state = 'R', .list = {&init_task_list, &t2.list}};
    init_task_list.next = &t1.list; init_task_list.prev = &t3.list;

    // 2. 使用for_each_process迭代器遍历进程，筛选运行态（R）
    printf("\n=== 遍历所有进程，筛选运行态进程 ===\n");
    struct task_struct *p;
    for_each_process(p) {
        if (p->state == 'R') {
            printf("运行态进程：PID=%d\n", p->pid);
        }
    }

    return 0;
}