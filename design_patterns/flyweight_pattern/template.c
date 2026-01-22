/*
以 “字体对象池” 为例（字体名 / 字号为内部状态，位置 / 颜色为外部状态），展示 C 语言如何实现享元模式

代码说明
状态分离：字体名 / 字号是内部状态（存储在对象中），位置 / 颜色是外部状态（仅在渲染时传入）；
复用核心：第二次获取 “宋体 - 12 号” 时直接复用池中的对象，无需重新创建；
无侵入性：外部状态仅在使用时传递，不修改享元对象的内部状态，保证复用安全
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------- 1. 定义享元对象（存储内部状态） ----------------------
// 字体享元对象：内部状态（可共享）+ 操作接口
typedef struct {
    char font_name[32]; // 字体名（如"宋体"，内部状态）
    int font_size;      // 字号（如12，内部状态）
    // 享元操作接口：渲染（外部状态通过参数传入）
    void (*render)(void *flyweight, int x, int y, int color);
} FontFlyweight;

// ---------------------- 2. 享元工厂（对象池管理） ----------------------
#define MAX_FONT_POOL 10  // 池大小上限
static FontFlyweight font_pool[MAX_FONT_POOL]; // 字体对象池
static int pool_count = 0;                     // 池中已存在的对象数

// 享元工厂核心函数：获取/创建字体对象（复用已有对象）
static FontFlyweight* get_font(const char *name, int size) {
    // 1. 检查池中有无匹配的享元对象（命中则直接返回）
    for (int i = 0; i < pool_count; i++) {
        if (strcmp(font_pool[i].font_name, name) == 0 && font_pool[i].font_size == size) {
            printf("[享元工厂] 命中共享对象：%s-%d号\n", name, size);
            return &font_pool[i];
        }
    }

    // 2. 未命中且池未满：创建新对象
    if (pool_count >= MAX_FONT_POOL) {
        printf("[享元工厂] 池已满，无法创建新对象\n");
        return NULL;
    }
    FontFlyweight *new_font = &font_pool[pool_count++];
    strncpy(new_font->font_name, name, sizeof(new_font->font_name) - 1);
    new_font->font_size = size;
    new_font->render = (void (*)(void*, int, int, int))font_render; // 绑定操作接口
    printf("[享元工厂] 创建新共享对象：%s-%d号（当前池数量=%d）\n", name, size, pool_count);
    return new_font;
}

// ---------------------- 3. 享元操作实现（外部状态动态传入） ----------------------
static void font_render(FontFlyweight *font, int x, int y, int color) {
    // 内部状态：font->font_name / font->font_size（固定、共享）
    // 外部状态：x/y/color（可变、传入）
    printf("[字体渲染] 内部状态：%s-%d号 | 外部状态：位置(%d,%d) 颜色=0x%06X\n",
           font->font_name, font->font_size, x, y, color);
}

// ---------------------- 4. 上层使用：复用享元对象 ----------------------
int main() {
    // 第一次获取：池无匹配，创建新对象
    FontFlyweight *font1 = get_font("宋体", 12);
    font1->render(font1, 100, 200, 0x000000); // 黑色（外部状态）

    // 第二次获取相同内部状态：复用已有对象
    FontFlyweight *font2 = get_font("宋体", 12);
    font2->render(font2, 300, 400, 0xFF0000); // 红色（外部状态）

    // 第三次获取不同内部状态：创建新对象
    FontFlyweight *font3 = get_font("黑体", 16);
    font3->render(font3, 500, 600, 0x00FF00); // 绿色（外部状态）

    return 0;
}