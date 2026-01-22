/*
场景说明
kobject是内核中用于设备模型的核心结构体（对应 sysfs 中的文件 / 目录），当kobject的状态（如设备插拔、属性修改）变化时，内核会通过uevent机制通知所有注册的观察者（内核模块 / 用户态进程），是观察者模式在设备驱动中的极致体现。

核心说明
内核中kobject是主题，uevent回调链表存储所有观察者（内核模块 / 用户态 udev）；
当设备插拔 / 属性修改时，kobject_uevent遍历回调链表通知所有观察者；
用户态 udev 通过 netlink 接收uevent事件，创建 / 删除设备节点，完全符合 “主题推送、观察者被动接收” 的核心思想。
*/

#include <stdio.h>
#include <string.h>

// 模拟内核kobject结构体（主题）
typedef struct kobject {
    char name[32];                // kobject名称（对应sysfs节点）
    void (*uevent_cb)(const char *action, const char *name); // 观察者回调链表（简化版）
} kobject;

// 模拟内核uevent观察者注册（绑定回调）
void kobject_uevent_register(kobject *kobj, void (*cb)(const char*, const char*)) {
    kobj->uevent_cb = cb;
    printf("kobject[%s]：注册uevent观察者\n", kobj->name);
}

// 模拟内核kobject事件触发（通知观察者）
void kobject_uevent(kobject *kobj, const char *action) {
    if (!kobj || !kobj->uevent_cb) return;
    printf("\nkobject[%s]：触发事件<%s>\n", kobj->name, action);
    kobj->uevent_cb(action, kobj->name); // 通知观察者
}

// ---------------------- 具体观察者实现 ----------------------
// 观察者1：用户态udev进程（处理设备插拔事件）
void udev_observer(const char *action, const char *kobj_name) {
    printf("[udev观察者] 接收kobject事件：%s %s → 创建/删除设备节点/dev/%s\n", 
           action, kobj_name, kobj_name);
}

// 观察者2：内核驱动模块（处理设备属性变化）
void driver_observer(const char *action, const char *kobj_name) {
    printf("[驱动观察者] 接收kobject事件：%s %s → 更新设备状态\n", 
           action, kobj_name);
}

// 主函数测试（模拟USB设备插拔）
int main() {
    // 1. 初始化kobject（USB设备节点）
    kobject usb_kobj = {.name = "usb1", .uevent_cb = NULL};
    
    // 2. 注册观察者（udev+驱动）
    kobject_uevent_register(&usb_kobj, udev_observer);
    // 注：内核中可注册多个观察者，此处简化为单回调，实际为链表
    
    // 3. 触发事件1：USB设备插入（add）
    kobject_uevent(&usb_kobj, "add");
    
    // 4. 切换观察者（驱动模块）
    kobject_uevent_register(&usb_kobj, driver_observer);
    
    // 5. 触发事件2：USB设备移除（remove）
    kobject_uevent(&usb_kobj, "remove");
    
    return 0;
}