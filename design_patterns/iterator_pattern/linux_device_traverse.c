/*
场景说明
内核设备模型中，所有设备通过device结构体的链表节点组织，内核通过device_for_each_child迭代器宏遍历某设备的所有子设备，无需关心设备链表的内部结构，即可统一管理子设备（如初始化、卸载）

核心说明
device_for_each_child是内核针对设备模型封装的专用迭代器，底层仍基于list_for_each_entry，但更贴合设备管理的业务场景；
迭代器模式使得设备驱动开发者无需关心设备链表的组织方式，仅需调用统一的迭代器宏即可管理子设备
*/

#include <stdio.h>
#include <stdlib.h>

// 复用list_head和迭代器宏
struct list_head {
    struct list_head *next, *prev;
};
#define INIT_LIST_HEAD(ptr) do {                    \
    (ptr)->next = (ptr); (ptr)->prev = (ptr);       \
} while (0)
#define offsetof(type, member) ((size_t)&((type*)0)->member)
#define container_of(ptr, type, member) ({          \
    const typeof(((type*)0)->member) *__mptr = (ptr);\
    (type*)((char*)__mptr - offsetof(type, member));})
#define list_for_each_entry(pos, head, member)      \
    for (pos = container_of((head)->next, typeof(*pos), member); \
         &pos->member != (head);                    \
         pos = container_of(pos->member.next, typeof(*pos), member))

// ---------------------- 设备迭代器宏：遍历子设备 ----------------------
#define device_for_each_child(parent, child, member) \
    list_for_each_entry(child, &(parent)->children, member)

// ---------------------- 业务结构体：设备（模拟struct device） ----------------------
struct device {
    char name[32];          // 设备名（如"usb1"、"sdcard"）
    struct list_head children; // 子设备链表头
    struct list_head node;  // 嵌入到父设备的children链表
};

// 主函数测试：遍历USB控制器的子设备
int main() {
    // 1. 初始化父设备（USB控制器）
    struct device usb_controller = {.name = "usb-controller"};
    INIT_LIST_HEAD(&usb_controller.children);

    // 2. 创建子设备（U盘、鼠标）并加入链表
    struct device usb_disk = {.name = "usb-disk"};
    struct device usb_mouse = {.name = "usb-mouse"};
    INIT_LIST_HEAD(&usb_disk.node);
    INIT_LIST_HEAD(&usb_mouse.node);

    usb_disk.node.next = &usb_mouse.node; usb_mouse.node.prev = &usb_disk.node;
    usb_mouse.node.next = &usb_controller.children; usb_controller.children.prev = &usb_mouse.node;
    usb_controller.children.next = &usb_disk.node; usb_disk.node.prev = &usb_controller.children;

    // 3. 使用device_for_each_child迭代器遍历子设备
    printf("\n=== 遍历USB控制器的子设备 ===\n");
    struct device *child_dev;
    device_for_each_child(&usb_controller, child_dev, node) {
        printf("子设备：%s\n", child_dev->name);
    }

    return 0;
}