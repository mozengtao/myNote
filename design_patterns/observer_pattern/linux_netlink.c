/*
场景说明
netlink 是内核与用户态通信的核心机制，内核子系统（如网络、内存、进程）作为主题，用户态进程作为观察者：内核通过 netlink 套接字向用户态推送事件（如 OOM、网络状态变化），用户态进程被动接收并处理，是观察者模式在跨态通信中的核心应用。

核心说明
内核子系统（如内存管理）是主题，用户态监控进程是观察者；
内核通过 netlink 向指定组推送事件，匹配的观察者（进程）被动接收；
netlink 是内核最灵活的跨态通知机制，完全遵循观察者模式的 “一对多、解耦、被动通知” 核心。
*/

#include <stdio.h>
#include <string.h>

// 模拟netlink消息结构体
typedef struct {
    int type;          // 消息类型（OOM/_NETLINK_ROUTE等）
    char data[64];     // 消息数据
} netlink_msg;

// 模拟netlink观察者（用户态进程）
typedef struct {
    int pid;           // 用户态进程PID
    int group;         // 监听组（如网络组、内存组）
    void (*cb)(netlink_msg *msg); // 回调函数
} netlink_observer;

// 模拟netlink主题（内核netlink子系统）
typedef struct {
    netlink_observer *observers[16];
    int count;
} netlink_subject;

// 1. 注册netlink观察者（用户态进程绑定）
int netlink_register(netlink_subject *subj, int pid, int group, void (*cb)(netlink_msg*)) {
    if (!subj || !cb) return -1;
    netlink_observer *obs = (netlink_observer*)malloc(sizeof(netlink_observer));
    obs->pid = pid;
    obs->group = group;
    obs->cb = cb;
    subj->observers[subj->count++] = obs;
    printf("netlink：进程%d注册到组%d观察者\n", pid, group);
    return 0;
}

// 2. 内核发送netlink消息（主题通知观察者）
void netlink_send_msg(netlink_subject *subj, int group, netlink_msg *msg) {
    if (!subj || !msg) return;
    printf("\n内核netlink：向组%d发送消息（类型%d）\n", group, msg->type);
    
    // 遍历观察者，匹配组ID
    for (int i=0; i<subj->count; i++) {
        netlink_observer *obs = subj->observers[i];
        if (obs->group == group) {
            printf("netlink：向进程%d推送消息\n", obs->pid);
            obs->cb(msg); // 通知观察者
        }
    }
}

// ---------------------- 具体观察者（用户态进程）实现 ----------------------
// 观察者1：OOM监控进程
void oom_observer(netlink_msg *msg) {
    printf("[OOM观察者（PID=100）] 接收消息：类型%d，数据：%s → 触发进程查杀\n", 
           msg->type, msg->data);
}

// 主函数测试（模拟内核OOM事件）
int main() {
    // 1. 初始化netlink主题
    netlink_subject netlink_subj = {.count = 0};
    
    // 2. 注册观察者（PID=100，监听内存组（1））
    netlink_register(&netlink_subj, 100, 1, oom_observer);
    
    // 3. 内核触发OOM事件（类型=1，内存组=1）
    netlink_msg oom_msg = {
        .type = 1,
        .data = "内存不足，触发OOM killer"
    };
    netlink_send_msg(&netlink_subj, 1, &oom_msg);
    
    return 0;
}