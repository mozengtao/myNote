/*
场景说明
inotify 是内核提供的文件系统事件通知机制：用户态进程通过inotify_init注册观察者，监听指定文件 / 目录的事件（创建、删除、修改）；当文件系统（主题）发生对应事件时，内核通过文件描述符向用户态推送事件，是观察者模式在用户态 - 内核态交互中的典型应用。

核心说明
内核文件系统是主题，inotify_watch 是观察者（用户态进程注册的监听）；
当文件系统发生事件时，内核遍历 inotify_watch 链表，向匹配的用户态进程推送事件；
用户态进程通过 read (inotify_fd) 读取事件，无需轮询，完全符合观察者模式的 “被动通知” 核心。
*/

#include <stdio.h>
#include <string.h>

// 模拟inotify事件类型
#define IN_CREATE 0x01
#define IN_MODIFY 0x02

// 模拟inotify观察者结构体（用户态进程注册的监听）
typedef struct {
    int wd;                // 监听描述符（watch descriptor）
    char path[64];         // 监听路径
    int mask;              // 监听事件掩码（CREATE/MODIFY）
    void (*cb)(int event, const char *path); // 观察者回调
} inotify_watch;

// 模拟内核inotify主题（文件系统事件管理器）
typedef struct {
    inotify_watch *watches[16]; // 观察者数组（简化版）
    int count;                  // 观察者数量
} inotify_subject;

// 1. 注册inotify观察者（用户态调用inotify_add_watch）
int inotify_add_watch(inotify_subject *subj, const char *path, int mask, void (*cb)(int, const char*)) {
    if (!subj || !path || !cb) return -1;
    inotify_watch *watch = (inotify_watch*)malloc(sizeof(inotify_watch));
    watch->wd = subj->count;
    strncpy(watch->path, path, sizeof(watch->path)-1);
    watch->mask = mask;
    watch->cb = cb;
    subj->watches[subj->count++] = watch;
    printf("inotify：注册观察者，监听%s（事件掩码：0x%x）\n", path, mask);
    return watch->wd;
}

// 2. 内核文件系统触发事件（主题通知观察者）
void fs_event_notify(inotify_subject *subj, const char *path, int event) {
    if (!subj || !path) return;
    printf("\n文件系统：%s 触发事件（0x%x）\n", path, event);
    
    // 遍历观察者，匹配路径和事件掩码
    for (int i=0; i<subj->count; i++) {
        inotify_watch *watch = subj->watches[i];
        if (strcmp(watch->path, path) == 0 && (watch->mask & event)) {
            watch->cb(event, path); // 通知观察者
        }
    }
}

// ---------------------- 具体观察者（用户态进程）实现 ----------------------
// 观察者：监听/tmp目录的创建/修改事件
void fs_observer(int event, const char *path) {
    char *event_name = (event == IN_CREATE) ? "CREATE" : "MODIFY";
    printf("[inotify观察者] 接收事件：%s %s\n", event_name, path);
}

// 主函数测试
int main() {
    // 1. 初始化inotify主题
    inotify_subject fs_subj = {.count = 0};
    
    // 2. 注册观察者（监听/tmp目录的CREATE/MODIFY事件）
    inotify_add_watch(&fs_subj, "/tmp", IN_CREATE | IN_MODIFY, fs_observer);
    
    // 3. 触发事件1：/tmp下创建文件
    fs_event_notify(&fs_subj, "/tmp", IN_CREATE);
    
    // 4. 触发事件2：/tmp下修改文件
    fs_event_notify(&fs_subj, "/tmp", IN_MODIFY);
    
    return 0;
}