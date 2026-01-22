/*
代码说明
接口统一：所有观察者回调遵循ObserverCallback格式，主题无需关心观察者的具体逻辑；
动态管理：支持运行时注册 / 注销观察者，符合内核 “热插拔” 需求；
解耦核心：主题（disk_subj）仅负责触发事件，观察者（日志 / 告警）仅负责处理事件，二者无直接依赖；
内核风格：采用链表头插法（内核链表常用方式），结构体封装主题状态，贴近内核实现习惯。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 1. 定义观察者回调接口（统一规范：所有观察者必须遵循此格式）
// 参数：事件类型、事件数据、观察者私有数据
typedef void (*ObserverCallback)(const char *event_type, void *event_data, void *priv);

// 2. 定义观察者节点（链表节点：存储回调+私有数据）
typedef struct ObserverNode {
    ObserverCallback cb;          // 观察者回调函数
    void *priv;                   // 观察者私有数据
    struct ObserverNode *next;    // 链表下一个节点
} ObserverNode;

// 3. 定义主题（事件管理器）：管理观察者链表+事件状态
typedef struct {
    ObserverNode *observer_list;  // 观察者链表头
    char name[32];                // 主题名称（如"disk"、"net"）
} Subject;

// 4. 主题方法1：初始化主题
void subject_init(Subject *subj, const char *name) {
    if (!subj || !name) return;
    strncpy(subj->name, name, sizeof(subj->name)-1);
    subj->observer_list = NULL;
    printf("主题[%s]初始化完成\n", subj->name);
}

// 5. 主题方法2：注册观察者（订阅事件）
int subject_register(Subject *subj, ObserverCallback cb, void *priv) {
    if (!subj || !cb) return -1;
    
    // 创建观察者节点
    ObserverNode *node = (ObserverNode*)malloc(sizeof(ObserverNode));
    if (!node) return -1;
    node->cb = cb;
    node->priv = priv;
    node->next = subj->observer_list; // 头插法（内核常用）
    
    // 加入主题的观察者链表
    subj->observer_list = node;
    printf("主题[%s]：观察者注册成功\n", subj->name);
    return 0;
}

// 6. 主题方法3：注销观察者（取消订阅）
int subject_unregister(Subject *subj, ObserverCallback cb) {
    if (!subj || !cb) return -1;
    
    ObserverNode *prev = NULL, *curr = subj->observer_list;
    while (curr) {
        if (curr->cb == cb) {
            // 从链表中删除节点
            if (prev) prev->next = curr->next;
            else subj->observer_list = curr->next;
            free(curr);
            printf("主题[%s]：观察者注销成功\n", subj->name);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

// 7. 主题方法4：触发事件（通知所有观察者）
void subject_notify(Subject *subj, const char *event_type, void *event_data) {
    if (!subj || !event_type) return;
    
    printf("\n主题[%s]：触发事件<%s>，开始通知观察者\n", subj->name, event_type);
    // 遍历观察者链表，调用回调函数
    ObserverNode *curr = subj->observer_list;
    while (curr) {
        if (curr->cb) {
            curr->cb(event_type, event_data, curr->priv); // 通知观察者
        }
        curr = curr->next;
    }
    printf("主题[%s]：事件<%s>通知完成\n", subj->name, event_type);
}

// ---------------------- 测试：具体观察者实现 ----------------------
// 观察者1：日志模块（接收事件并写入日志）
void log_observer(const char *event_type, void *event_data, void *priv) {
    printf("[日志观察者] 接收事件<%s>，数据：%s → 写入日志文件\n", 
           event_type, (char*)event_data);
}

// 观察者2：告警模块（接收事件并触发告警）
void alert_observer(const char *event_type, void *event_data, void *priv) {
    printf("[告警观察者] 接收事件<%s>，数据：%s → 发送告警邮件至%s\n", 
           event_type, (char*)event_data, (char*)priv);
}

// 主函数测试
int main() {
    // 1. 初始化主题（磁盘事件管理器）
    Subject disk_subj;
    subject_init(&disk_subj, "disk");
    
    // 2. 注册观察者
    subject_register(&disk_subj, log_observer, NULL);
    subject_register(&disk_subj, alert_observer, "admin@linux.com");
    
    // 3. 触发事件1：磁盘空间不足
    char event_data1[] = "sda1磁盘使用率95%";
    subject_notify(&disk_subj, "disk_full", event_data1);
    
    // 4. 触发事件2：磁盘IO过高
    char event_data2[] = "sda1 IOPS达到10000";
    subject_notify(&disk_subj, "disk_high_io", event_data2);
    
    // 5. 注销告警观察者
    subject_unregister(&disk_subj, alert_observer);
    
    // 6. 再次触发事件（仅日志观察者接收）
    char event_data3[] = "sda1磁盘只读";
    subject_notify(&disk_subj, "disk_readonly", event_data3);
    
    return 0;
}