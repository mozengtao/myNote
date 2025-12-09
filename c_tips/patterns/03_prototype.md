# 原型模式 (Prototype Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      PROTOTYPE PATTERN                            |
+------------------------------------------------------------------+

    NORMAL CREATION (From Scratch):
    
    create_employee("John", "123 Main St", "London", "Engineer", 50000)
    create_employee("Jane", "123 Main St", "London", "Engineer", 52000)
    create_employee("Bob",  "123 Main St", "London", "Engineer", 48000)
    
    Problem: Repeating same values every time!


    PROTOTYPE CREATION (Clone + Customize):
    
    +------------------+
    |    PROTOTYPE     |
    |  (Template)      |
    |  +------------+  |
    |  | address:   |  |
    |  | "123 Main" |  |
    |  | "London"   |  |
    |  | job:       |  |
    |  | "Engineer" |  |
    |  +------------+  |
    +--------+---------+
             |
             | clone()
             |
    +--------v---------+--------v---------+--------v---------+
    |     CLONE 1      |     CLONE 2      |     CLONE 3      |
    |  name: "John"    |  name: "Jane"    |  name: "Bob"     |
    |  salary: 50000   |  salary: 52000   |  salary: 48000   |
    |  (rest cloned)   |  (rest cloned)   |  (rest cloned)   |
    +------------------+------------------+------------------+


    DEEP COPY vs SHALLOW COPY:
    
    SHALLOW COPY:                    DEEP COPY:
    +----------+                     +----------+
    | Clone    |                     | Clone    |
    | addr: ---+--> [Shared Obj]     | addr: ---+--> [New Copy]
    +----------+        ^            +----------+
                        |
    +----------+        |            +----------+
    | Original |        |            | Original |
    | addr: ---+--------+            | addr: ---+--> [Original Obj]
    +----------+                     +----------+
    
    Problem: Modifying one       OK: Independent copies
    affects the other!
```

**核心思想说明：**
- 通过复制（克隆）现有对象创建新对象
- 避免重复初始化相同的属性
- 深拷贝确保嵌套对象也被完整复制
- 适合创建相似对象的场景

## 实现思路

1. **原型结构**：定义包含克隆函数的结构体
2. **深拷贝函数**：递归复制所有嵌套对象
3. **原型注册**：预定义常用原型模板
4. **定制函数**：克隆后修改差异属性

## 典型应用场景

- 游戏中创建相似敌人/道具
- 配置模板复制
- 文档模板系统
- GUI组件克隆
- 数据记录批量创建

## 完整代码示例

```c
/*============================================================================
 * 原型模式示例 - 员工信息管理系统
 *============================================================================*/

/*---------------------------------------------------------------------------
 * prototype.h - 原型接口和结构定义
 *---------------------------------------------------------------------------*/
#ifndef PROTOTYPE_H
#define PROTOTYPE_H

#include <stdint.h>
#include <stdbool.h>

/* 地址结构（嵌套对象） */
typedef struct {
    char street[64];
    char city[32];
    char postal_code[16];
} address_t;

/* 职位结构（嵌套对象） */
typedef struct {
    char title[32];
    char department[32];
    uint32_t salary;
} position_t;

/* 员工结构（包含嵌套对象） */
typedef struct employee {
    uint32_t id;
    char name[64];
    address_t *address;     /* 关键点：指针成员，需要深拷贝 */
    position_t *position;   /* 关键点：指针成员，需要深拷贝 */
    
    /* 关键点：克隆函数指针 */
    struct employee* (*clone)(const struct employee *self);
} employee_t;

/* 原型管理器 */
typedef enum {
    PROTO_MAIN_OFFICE_ENGINEER,
    PROTO_MAIN_OFFICE_MANAGER,
    PROTO_BRANCH_OFFICE_ENGINEER,
    PROTO_MAX
} prototype_type_t;

/* API */
void prototype_registry_init(void);
employee_t* prototype_clone(prototype_type_t type);
void employee_destroy(employee_t *emp);
void employee_print(const employee_t *emp);

/* 定制函数 */
void employee_set_name(employee_t *emp, const char *name);
void employee_set_id(employee_t *emp, uint32_t id);
void employee_set_salary(employee_t *emp, uint32_t salary);

#endif /* PROTOTYPE_H */


/*---------------------------------------------------------------------------
 * prototype.c - 原型实现
 *---------------------------------------------------------------------------*/
#include "prototype.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 关键点：深拷贝函数 - 复制嵌套对象
 *---------------------------------------------------------------------------*/
static address_t* address_clone(const address_t *src) {
    if (src == NULL) return NULL;
    
    address_t *copy = malloc(sizeof(address_t));
    if (copy != NULL) {
        /* 值类型直接复制 */
        memcpy(copy, src, sizeof(address_t));
    }
    return copy;
}

static position_t* position_clone(const position_t *src) {
    if (src == NULL) return NULL;
    
    position_t *copy = malloc(sizeof(position_t));
    if (copy != NULL) {
        memcpy(copy, src, sizeof(position_t));
    }
    return copy;
}

/*---------------------------------------------------------------------------
 * 关键点：员工深拷贝函数
 *---------------------------------------------------------------------------*/
static employee_t* employee_clone(const employee_t *self) {
    if (self == NULL) return NULL;
    
    /* 分配新对象 */
    employee_t *copy = malloc(sizeof(employee_t));
    if (copy == NULL) return NULL;
    
    /* 复制基本成员 */
    copy->id = self->id;
    strncpy(copy->name, self->name, sizeof(copy->name) - 1);
    copy->clone = self->clone;
    
    /* 关键点：深拷贝嵌套对象 */
    copy->address = address_clone(self->address);
    copy->position = position_clone(self->position);
    
    /* 检查深拷贝是否成功 */
    if ((self->address != NULL && copy->address == NULL) ||
        (self->position != NULL && copy->position == NULL)) {
        employee_destroy(copy);
        return NULL;
    }
    
    return copy;
}

/*---------------------------------------------------------------------------
 * 销毁员工对象
 *---------------------------------------------------------------------------*/
void employee_destroy(employee_t *emp) {
    if (emp == NULL) return;
    
    if (emp->address != NULL) {
        free(emp->address);
    }
    if (emp->position != NULL) {
        free(emp->position);
    }
    free(emp);
}

/*---------------------------------------------------------------------------
 * 打印员工信息
 *---------------------------------------------------------------------------*/
void employee_print(const employee_t *emp) {
    if (emp == NULL) {
        printf("Employee: NULL\n");
        return;
    }
    
    printf("Employee [ID: %u]\n", emp->id);
    printf("  Name: %s\n", emp->name);
    
    if (emp->address != NULL) {
        printf("  Address: %s, %s %s\n", 
               emp->address->street, 
               emp->address->city,
               emp->address->postal_code);
    }
    
    if (emp->position != NULL) {
        printf("  Position: %s (%s)\n", 
               emp->position->title,
               emp->position->department);
        printf("  Salary: $%u\n", emp->position->salary);
    }
}

/*---------------------------------------------------------------------------
 * 定制函数
 *---------------------------------------------------------------------------*/
void employee_set_name(employee_t *emp, const char *name) {
    if (emp != NULL && name != NULL) {
        strncpy(emp->name, name, sizeof(emp->name) - 1);
    }
}

void employee_set_id(employee_t *emp, uint32_t id) {
    if (emp != NULL) {
        emp->id = id;
    }
}

void employee_set_salary(employee_t *emp, uint32_t salary) {
    if (emp != NULL && emp->position != NULL) {
        emp->position->salary = salary;
    }
}

/*---------------------------------------------------------------------------
 * 关键点：原型注册表 - 预定义模板
 *---------------------------------------------------------------------------*/
static employee_t *prototype_registry[PROTO_MAX] = {NULL};

/* 创建原型模板 */
static employee_t* create_prototype(const char *street, const char *city,
                                     const char *postal, const char *title,
                                     const char *dept, uint32_t base_salary) {
    employee_t *proto = malloc(sizeof(employee_t));
    if (proto == NULL) return NULL;
    
    memset(proto, 0, sizeof(employee_t));
    proto->clone = employee_clone;
    
    /* 创建地址 */
    proto->address = malloc(sizeof(address_t));
    if (proto->address != NULL) {
        strncpy(proto->address->street, street, sizeof(proto->address->street) - 1);
        strncpy(proto->address->city, city, sizeof(proto->address->city) - 1);
        strncpy(proto->address->postal_code, postal, sizeof(proto->address->postal_code) - 1);
    }
    
    /* 创建职位 */
    proto->position = malloc(sizeof(position_t));
    if (proto->position != NULL) {
        strncpy(proto->position->title, title, sizeof(proto->position->title) - 1);
        strncpy(proto->position->department, dept, sizeof(proto->position->department) - 1);
        proto->position->salary = base_salary;
    }
    
    return proto;
}

/* 初始化原型注册表 */
void prototype_registry_init(void) {
    /* 关键点：预定义各种原型模板 */
    
    /* 主办公室工程师模板 */
    prototype_registry[PROTO_MAIN_OFFICE_ENGINEER] = create_prototype(
        "123 Main Street",      /* 地址 */
        "New York",             /* 城市 */
        "10001",                /* 邮编 */
        "Software Engineer",    /* 职位 */
        "Engineering",          /* 部门 */
        80000                   /* 基础薪资 */
    );
    
    /* 主办公室经理模板 */
    prototype_registry[PROTO_MAIN_OFFICE_MANAGER] = create_prototype(
        "123 Main Street",
        "New York",
        "10001",
        "Project Manager",
        "Management",
        100000
    );
    
    /* 分公司工程师模板 */
    prototype_registry[PROTO_BRANCH_OFFICE_ENGINEER] = create_prototype(
        "456 Branch Avenue",
        "Boston",
        "02101",
        "Software Engineer",
        "Engineering",
        75000
    );
    
    printf("[Prototype] Registry initialized with %d prototypes\n", PROTO_MAX);
}

/*---------------------------------------------------------------------------
 * 关键点：克隆原型
 *---------------------------------------------------------------------------*/
employee_t* prototype_clone(prototype_type_t type) {
    if (type >= PROTO_MAX || prototype_registry[type] == NULL) {
        printf("[Prototype] Invalid type: %d\n", type);
        return NULL;
    }
    
    /* 调用原型的克隆函数 */
    employee_t *proto = prototype_registry[type];
    return proto->clone(proto);
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "prototype.h"
#include <stdio.h>

int main(void) {
    printf("=== Prototype Pattern Demo ===\n\n");
    
    /* 初始化原型注册表 */
    prototype_registry_init();
    
    /* 关键点：通过克隆原型创建员工，只需定制差异部分 */
    
    printf("\n--- Creating Main Office Engineers ---\n");
    
    /* 员工1：克隆 + 定制 */
    employee_t *john = prototype_clone(PROTO_MAIN_OFFICE_ENGINEER);
    employee_set_id(john, 1001);
    employee_set_name(john, "John Smith");
    employee_set_salary(john, 85000);
    employee_print(john);
    
    printf("\n");
    
    /* 员工2：克隆 + 定制 */
    employee_t *jane = prototype_clone(PROTO_MAIN_OFFICE_ENGINEER);
    employee_set_id(jane, 1002);
    employee_set_name(jane, "Jane Doe");
    employee_set_salary(jane, 90000);
    employee_print(jane);
    
    printf("\n--- Creating Branch Office Engineer ---\n");
    
    /* 员工3：不同原型 */
    employee_t *bob = prototype_clone(PROTO_BRANCH_OFFICE_ENGINEER);
    employee_set_id(bob, 2001);
    employee_set_name(bob, "Bob Wilson");
    employee_print(bob);
    
    printf("\n--- Creating Manager ---\n");
    
    /* 员工4：经理原型 */
    employee_t *alice = prototype_clone(PROTO_MAIN_OFFICE_MANAGER);
    employee_set_id(alice, 3001);
    employee_set_name(alice, "Alice Brown");
    employee_set_salary(alice, 120000);
    employee_print(alice);
    
    /* 关键点：验证深拷贝 - 修改一个不影响另一个 */
    printf("\n--- Deep Copy Verification ---\n");
    printf("Modifying John's address...\n");
    strncpy(john->address->street, "999 New Street", sizeof(john->address->street) - 1);
    
    printf("John's address: %s\n", john->address->street);
    printf("Jane's address: %s (should be unchanged)\n", jane->address->street);
    
    /* 清理 */
    printf("\n--- Cleanup ---\n");
    employee_destroy(john);
    employee_destroy(jane);
    employee_destroy(bob);
    employee_destroy(alice);
    printf("All employees destroyed.\n");
    
    return 0;
}
```

## 运行输出示例

```
=== Prototype Pattern Demo ===

[Prototype] Registry initialized with 3 prototypes

--- Creating Main Office Engineers ---
Employee [ID: 1001]
  Name: John Smith
  Address: 123 Main Street, New York 10001
  Position: Software Engineer (Engineering)
  Salary: $85000

Employee [ID: 1002]
  Name: Jane Doe
  Address: 123 Main Street, New York 10001
  Position: Software Engineer (Engineering)
  Salary: $90000

--- Creating Branch Office Engineer ---
Employee [ID: 2001]
  Name: Bob Wilson
  Address: 456 Branch Avenue, Boston 02101
  Position: Software Engineer (Engineering)
  Salary: $75000

--- Creating Manager ---
Employee [ID: 3001]
  Name: Alice Brown
  Address: 123 Main Street, New York 10001
  Position: Project Manager (Management)
  Salary: $120000

--- Deep Copy Verification ---
Modifying John's address...
John's address: 999 New Street
Jane's address: 123 Main Street (should be unchanged)

--- Cleanup ---
All employees destroyed.
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **减少重复** | 相同属性无需重复指定 |
| **快速创建** | 克隆比从头构造更快 |
| **灵活定制** | 克隆后可自由修改差异部分 |
| **模板化** | 预定义模板，创建更规范 |
| **独立性** | 深拷贝保证对象完全独立 |

## 注意事项

| 问题 | 解决方案 |
|------|----------|
| 浅拷贝问题 | 必须实现深拷贝，递归复制所有嵌套对象 |
| 循环引用 | 需要特殊处理，记录已拷贝对象 |
| 内存管理 | 每个克隆对象需要独立释放 |

