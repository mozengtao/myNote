# 工厂模式 (Factory Pattern)

## 定义

工厂模式是一种创建型设计模式，它提供了一种创建对象的接口，但允许子类或函数决定实例化哪个类。工厂模式将对象的创建与使用分离，使代码更加灵活和可维护。

## 适用场景

- 需要根据不同条件创建不同类型对象时
- 对象创建过程复杂，需要集中管理时
- 需要隐藏具体实现类，只暴露统一接口时
- 驱动程序的加载（如不同的数据库驱动、硬件驱动）
- 图形界面中不同控件的创建
- 协议解析器的选择（如JSON、XML、YAML解析器）

## ASCII 图解

```
+------------------------------------------------------------------------+
|                         FACTORY PATTERN                                 |
+------------------------------------------------------------------------+
|                                                                         |
|                         +------------------+                            |
|                         |     Client       |                            |
|                         +--------+---------+                            |
|                                  |                                      |
|                                  | create("type")                       |
|                                  v                                      |
|                         +------------------+                            |
|                         |     Factory      |                            |
|                         +--------+---------+                            |
|                                  |                                      |
|           +----------------------+----------------------+               |
|           |                      |                      |               |
|           v                      v                      v               |
|   +---------------+      +---------------+      +---------------+       |
|   |   Product A   |      |   Product B   |      |   Product C   |       |
|   +---------------+      +---------------+      +---------------+       |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Factory Function Logic:                                               |
|                                                                         |
|   +-------------------+                                                 |
|   | Input: type_name  |                                                 |
|   +---------+---------+                                                 |
|             |                                                           |
|             v                                                           |
|   +---------+---------+                                                 |
|   |  switch(type)     |                                                 |
|   +---------+---------+                                                 |
|             |                                                           |
|    +--------+--------+--------+                                         |
|    |        |        |        |                                         |
|    v        v        v        v                                         |
|  "A"      "B"      "C"    default                                       |
|    |        |        |        |                                         |
|    v        v        v        v                                         |
| create   create   create   return                                       |
| Prod_A   Prod_B   Prod_C   NULL                                         |
|    |        |        |                                                  |
|    +--------+--------+                                                  |
|             |                                                           |
|             v                                                           |
|   +---------+---------+                                                 |
|   | Return: Product*  |                                                 |
|   +-------------------+                                                 |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Product Interface (Virtual Table):                                    |
|                                                                         |
|   +---------------------------+                                         |
|   |  ProductVTable            |                                         |
|   +---------------------------+                                         |
|   | + operation()  ---------> | Each product implements differently     |
|   | + destroy()    ---------> | Cleanup specific to product type        |
|   +---------------------------+                                         |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了工厂模式的核心结构。客户端（Client）不直接创建具体产品，而是通过工厂（Factory）的创建函数，传入类型参数来获取所需的产品对象。工厂内部根据类型参数判断应该创建哪种具体产品（Product A、B或C）。所有产品都实现相同的接口（通过虚函数表VTable实现），使得客户端可以用统一的方式操作不同类型的产品。

## 实现方法

在C语言中实现工厂模式的关键：

1. 定义统一的产品接口（使用函数指针结构体模拟虚函数表）
2. 为每种具体产品实现接口函数
3. 创建工厂函数，根据参数返回不同类型的产品
4. 使用枚举或字符串标识不同的产品类型

## C语言代码示例

### 简单工厂模式

```c
// shape_factory.h
#ifndef SHAPE_FACTORY_H
#define SHAPE_FACTORY_H

// 形状类型枚举
typedef enum {
    SHAPE_CIRCLE,
    SHAPE_RECTANGLE,
    SHAPE_TRIANGLE
} ShapeType;

// 形状虚函数表（接口）
typedef struct Shape Shape;
typedef struct {
    void (*draw)(Shape* self);
    double (*area)(Shape* self);
    void (*destroy)(Shape* self);
    const char* (*get_name)(Shape* self);
} ShapeVTable;

// 形状基础结构
struct Shape {
    const ShapeVTable* vtable;
};

// 工厂函数
Shape* shape_factory_create(ShapeType type);

// 通用操作（通过虚函数表调用）
void shape_draw(Shape* shape);
double shape_area(Shape* shape);
void shape_destroy(Shape* shape);
const char* shape_get_name(Shape* shape);

#endif // SHAPE_FACTORY_H
```

```c
// shape_factory.c
#include "shape_factory.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==================== Circle Implementation ====================
typedef struct {
    Shape base;
    double radius;
} Circle;

static void circle_draw(Shape* self) {
    Circle* circle = (Circle*)self;
    printf("Drawing Circle with radius %.2f\n", circle->radius);
    printf("    ****\n");
    printf("  **    **\n");
    printf(" *        *\n");
    printf(" *        *\n");
    printf("  **    **\n");
    printf("    ****\n");
}

static double circle_area(Shape* self) {
    Circle* circle = (Circle*)self;
    return M_PI * circle->radius * circle->radius;
}

static void circle_destroy(Shape* self) {
    printf("Destroying Circle\n");
    free(self);
}

static const char* circle_get_name(Shape* self) {
    (void)self;
    return "Circle";
}

static const ShapeVTable circle_vtable = {
    .draw = circle_draw,
    .area = circle_area,
    .destroy = circle_destroy,
    .get_name = circle_get_name
};

static Shape* circle_create(double radius) {
    Circle* circle = (Circle*)malloc(sizeof(Circle));
    if (circle) {
        circle->base.vtable = &circle_vtable;
        circle->radius = radius;
    }
    return (Shape*)circle;
}

// ==================== Rectangle Implementation ====================
typedef struct {
    Shape base;
    double width;
    double height;
} Rectangle;

static void rectangle_draw(Shape* self) {
    Rectangle* rect = (Rectangle*)self;
    printf("Drawing Rectangle (%.2f x %.2f)\n", rect->width, rect->height);
    printf("+----------+\n");
    printf("|          |\n");
    printf("|          |\n");
    printf("+----------+\n");
}

static double rectangle_area(Shape* self) {
    Rectangle* rect = (Rectangle*)self;
    return rect->width * rect->height;
}

static void rectangle_destroy(Shape* self) {
    printf("Destroying Rectangle\n");
    free(self);
}

static const char* rectangle_get_name(Shape* self) {
    (void)self;
    return "Rectangle";
}

static const ShapeVTable rectangle_vtable = {
    .draw = rectangle_draw,
    .area = rectangle_area,
    .destroy = rectangle_destroy,
    .get_name = rectangle_get_name
};

static Shape* rectangle_create(double width, double height) {
    Rectangle* rect = (Rectangle*)malloc(sizeof(Rectangle));
    if (rect) {
        rect->base.vtable = &rectangle_vtable;
        rect->width = width;
        rect->height = height;
    }
    return (Shape*)rect;
}

// ==================== Triangle Implementation ====================
typedef struct {
    Shape base;
    double base_len;
    double height;
} Triangle;

static void triangle_draw(Shape* self) {
    Triangle* tri = (Triangle*)self;
    printf("Drawing Triangle (base=%.2f, height=%.2f)\n", tri->base_len, tri->height);
    printf("    /\\\n");
    printf("   /  \\\n");
    printf("  /    \\\n");
    printf(" /______\\\n");
}

static double triangle_area(Shape* self) {
    Triangle* tri = (Triangle*)self;
    return 0.5 * tri->base_len * tri->height;
}

static void triangle_destroy(Shape* self) {
    printf("Destroying Triangle\n");
    free(self);
}

static const char* triangle_get_name(Shape* self) {
    (void)self;
    return "Triangle";
}

static const ShapeVTable triangle_vtable = {
    .draw = triangle_draw,
    .area = triangle_area,
    .destroy = triangle_destroy,
    .get_name = triangle_get_name
};

static Shape* triangle_create(double base, double height) {
    Triangle* tri = (Triangle*)malloc(sizeof(Triangle));
    if (tri) {
        tri->base.vtable = &triangle_vtable;
        tri->base_len = base;
        tri->height = height;
    }
    return (Shape*)tri;
}

// ==================== Factory Function ====================
Shape* shape_factory_create(ShapeType type) {
    switch (type) {
        case SHAPE_CIRCLE:
            return circle_create(5.0);  // 默认半径
        case SHAPE_RECTANGLE:
            return rectangle_create(4.0, 3.0);  // 默认宽高
        case SHAPE_TRIANGLE:
            return triangle_create(4.0, 3.0);  // 默认底和高
        default:
            printf("Unknown shape type!\n");
            return NULL;
    }
}

// ==================== Generic Operations ====================
void shape_draw(Shape* shape) {
    if (shape && shape->vtable && shape->vtable->draw) {
        shape->vtable->draw(shape);
    }
}

double shape_area(Shape* shape) {
    if (shape && shape->vtable && shape->vtable->area) {
        return shape->vtable->area(shape);
    }
    return 0.0;
}

void shape_destroy(Shape* shape) {
    if (shape && shape->vtable && shape->vtable->destroy) {
        shape->vtable->destroy(shape);
    }
}

const char* shape_get_name(Shape* shape) {
    if (shape && shape->vtable && shape->vtable->get_name) {
        return shape->vtable->get_name(shape);
    }
    return "Unknown";
}
```

### 带参数的工厂函数

```c
// extended_factory.h
#ifndef EXTENDED_FACTORY_H
#define EXTENDED_FACTORY_H

#include "shape_factory.h"

// 工厂配置结构
typedef struct {
    ShapeType type;
    union {
        struct { double radius; } circle;
        struct { double width; double height; } rectangle;
        struct { double base; double height; } triangle;
    } params;
} ShapeConfig;

// 根据配置创建形状
Shape* shape_factory_create_with_config(const ShapeConfig* config);

// 便捷创建函数
Shape* create_circle(double radius);
Shape* create_rectangle(double width, double height);
Shape* create_triangle(double base, double height);

#endif
```

```c
// extended_factory.c
#include "extended_factory.h"
#include <stdlib.h>

// 这些函数需要访问内部创建函数，实际项目中可以通过其他方式实现
// 这里简化处理，直接重新实现

Shape* shape_factory_create_with_config(const ShapeConfig* config) {
    if (!config) return NULL;
    
    switch (config->type) {
        case SHAPE_CIRCLE:
            return create_circle(config->params.circle.radius);
        case SHAPE_RECTANGLE:
            return create_rectangle(config->params.rectangle.width,
                                   config->params.rectangle.height);
        case SHAPE_TRIANGLE:
            return create_triangle(config->params.triangle.base,
                                  config->params.triangle.height);
        default:
            return NULL;
    }
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "shape_factory.h"

int main() {
    printf("=== Factory Pattern Demo ===\n\n");
    
    // 使用工厂创建不同类型的形状
    Shape* shapes[3];
    shapes[0] = shape_factory_create(SHAPE_CIRCLE);
    shapes[1] = shape_factory_create(SHAPE_RECTANGLE);
    shapes[2] = shape_factory_create(SHAPE_TRIANGLE);
    
    // 统一接口操作所有形状
    for (int i = 0; i < 3; i++) {
        if (shapes[i]) {
            printf("\n--- %s ---\n", shape_get_name(shapes[i]));
            shape_draw(shapes[i]);
            printf("Area: %.2f\n", shape_area(shapes[i]));
        }
    }
    
    // 清理资源
    printf("\n--- Cleanup ---\n");
    for (int i = 0; i < 3; i++) {
        shape_destroy(shapes[i]);
    }
    
    return 0;
}

/* 输出示例:
=== Factory Pattern Demo ===

--- Circle ---
Drawing Circle with radius 5.00
    ****
  **    **
 *        *
 *        *
  **    **
    ****
Area: 78.54

--- Rectangle ---
Drawing Rectangle (4.00 x 3.00)
+----------+
|          |
|          |
+----------+
Area: 12.00

--- Triangle ---
Drawing Triangle (base=4.00, height=3.00)
    /\
   /  \
  /    \
 /______\
Area: 6.00

--- Cleanup ---
Destroying Circle
Destroying Rectangle
Destroying Triangle
*/
```

### 注册式工厂（可扩展）

```c
// registry_factory.h
#ifndef REGISTRY_FACTORY_H
#define REGISTRY_FACTORY_H

#include "shape_factory.h"

// 创建函数类型
typedef Shape* (*ShapeCreator)(void* params);

// 注册新的形状类型
int factory_register(const char* name, ShapeCreator creator);

// 通过名称创建形状
Shape* factory_create_by_name(const char* name, void* params);

// 列出所有已注册的类型
void factory_list_types(void);

#endif
```

```c
// registry_factory.c
#include "registry_factory.h"
#include <stdio.h>
#include <string.h>

#define MAX_REGISTERED_TYPES 32

typedef struct {
    char name[64];
    ShapeCreator creator;
} RegisteredType;

static RegisteredType g_registry[MAX_REGISTERED_TYPES];
static int g_registry_count = 0;

int factory_register(const char* name, ShapeCreator creator) {
    if (g_registry_count >= MAX_REGISTERED_TYPES) {
        return -1;  // 注册表已满
    }
    
    strncpy(g_registry[g_registry_count].name, name, 63);
    g_registry[g_registry_count].name[63] = '\0';
    g_registry[g_registry_count].creator = creator;
    g_registry_count++;
    
    printf("[Factory] Registered type: %s\n", name);
    return 0;
}

Shape* factory_create_by_name(const char* name, void* params) {
    for (int i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0) {
            return g_registry[i].creator(params);
        }
    }
    printf("[Factory] Unknown type: %s\n", name);
    return NULL;
}

void factory_list_types(void) {
    printf("Registered shape types:\n");
    for (int i = 0; i < g_registry_count; i++) {
        printf("  - %s\n", g_registry[i].name);
    }
}
```

## 优缺点

### 优点
- 将对象创建与使用分离，降低耦合
- 客户端无需知道具体产品类名，只需知道类型参数
- 易于扩展新产品类型
- 集中管理对象创建逻辑

### 缺点
- 增加了系统的复杂度
- 每增加一种产品需要修改工厂函数（简单工厂）
- 对于复杂的产品层次，可能需要多个工厂类

