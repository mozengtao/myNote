[C Online Compiler](https://www.programiz.com/c-programming/online-compiler/)  
[va_list in C: exploring ft_printf](https://medium.com/@turman1701/va-list-in-c-exploring-ft-printf-bb2a19fcd128)  
[SystemProgramming Wiki](https://csresources.github.io/SystemProgrammingWiki/SystemProgramming/C-Programming,-Part-1:-Introduction/)  
[**Beej's Guide to C Programming**](https://beej.us/guide/bgc/html/split/index.html)  
[]()  
[]()  
[**Object-oriented Programming with Ansi-C**](http://ebook.pldworld.com/-huihoo-/book/pdf/object-oriented-programming-with-ansi-c/)  
[Sources for "Object-Oriented Programming with ANSI-C"](https://github.com/shichao-an/ooc/tree/master)  
[Application Note Object-Oriented Programming in C](https://www.state-machine.com/doc/AN_Simple_OOP_in_C.pdf)  
[OOP-in-C](https://github.com/QuantumLeaps/OOP-in-C)  
[Object-oriented techniques in C](https://dmitryfrank.com/articles/oop_in_c)  
[OBJECT-ORIENTED PROGRAMMING IN C](https://home.cs.colorado.edu/~kena/classes/5448/f12/presentation-materials/srivastava.pdf)  
[Key Concept: Object-Oriented Programming](https://www.state-machine.com/oop)  
[]()  
[]()  
[EmbedSummary](https://github.com/zhengnianli/EmbedSummary)  
[QuantumLeaps](https://github.com/QuantumLeaps)  
> FSM
[**Practical UML Statecharts in C/C++**](https://www.state-machine.com/category/books)  
[**Practical Statecharts in C/C++**](https://www.state-machine.com/category/books)  
[专业嵌入式软件开发:全面走向高质高效编程]()  
[程序员修炼之道：通向务实的最高境界]()  
[七周七并发模型]()  
[]()  
[]()  

- Singleton pattern
```c
#include <stdio.h>
#include <pthread.h>

typedef struct Printer {
    int port;
    int pageCount;
} Printer;

/* simple implementation */
Printer *getPrinter() {
    static Printer printer;
    static int initialized = 0;

    if (!initialized) {
        printer.port = 0x123;
        printer.pageCount = 0;
        initialized = 1;
    }

    return &printer;
}

/*
 * Option 1: pthread_once (Recommended for POSIX systems)
 * - Guaranteed to run initialization exactly once
 * - No explicit locking needed by caller
 * - Most robust and efficient solution
 */
static Printer g_printer;
static pthread_once_t g_printer_once = PTHREAD_ONCE_INIT;

static void initPrinter(void) {
    g_printer.port = 0x123;
    g_printer.pageCount = 0;
}

Printer *getPrinter(void) {
    pthread_once(&g_printer_once, initPrinter);
    return &g_printer;
}

/*
 * Option 2: Double-Checked Locking with Mutex
 * - Use when pthread_once is not available
 * - Requires careful memory ordering
 */
static Printer g_printer;
static volatile int g_initialized = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
 *
Printer *getPrinter(void) {
    if (!g_initialized) {                    // First check (no lock)
        pthread_mutex_lock(&g_mutex);
        if (!g_initialized) {                // Second check (with lock)
            g_printer.port = 0x123;
            g_printer.pageCount = 0;
            __sync_synchronize();            // Memory barrier
            g_initialized = 1;
        }
        pthread_mutex_unlock(&g_mutex);
    }
    return &g_printer;
}

/*
 * Option 3: C11 call_once (if C11 available)
 */
#include <threads.h>
static Printer g_printer;
static once_flag g_flag = ONCE_FLAG_INIT;

static void initPrinter(void) {
    g_printer.port = 0x123;
    g_printer.pageCount = 0;
}

Printer *getPrinter(void) {
    call_once(&g_flag, initPrinter);
    return &g_printer;
}

int main(void) {
    Printer *printer1 = getPrinter();
    Printer *printer2 = getPrinter();

    printf("Printer1: %p, Printer2: %p\n", printer1, printer2);

    if (printer1 == printer2) {
        printf("Printer1 and Printer2 are the same\n");
    } else {
        printf("Printer1 and Printer2 are different\n");
    }

    return 0;
}
```

- Factory pattern
```c
/* Factory pattern */
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    VEHICLE_TYPE_CAR,
    VEHICLE_TYPE_TRUCK,
} VehicleType;

const char *getVehicleTypeString(VehicleType type) {
    switch (type) {
        case VEHICLE_TYPE_CAR:
            return "Car";
        case VEHICLE_TYPE_TRUCK:
            return "Truck";
    }
    return "Unknown";
}

typedef struct Vehicle {
    int speed;
    int gear;
    VehicleType type;

    void (*accelerate)(struct Vehicle *self);
    void (*brake)(struct Vehicle *self);
    void (*print)(struct Vehicle *self);
} Vehicle;

void car_accelerate(Vehicle *self) {
    printf("Car accelerating\n");
}

void car_brake(Vehicle *self) {
    printf("Car braking\n");
}

void car_print(Vehicle *self) {
    printf("Car: speed: %d, gear: %d, type: %s\n", self->speed, self->gear, getVehicleTypeString(self->type));
}

void truck_accelerate(Vehicle *self) {
    printf("Truck accelerating\n");
}

void truck_brake(Vehicle *self) {
    printf("Truck braking\n");
}

void truck_print(Vehicle *self) {
    printf("Truck: speed: %d, gear: %d, type: %s\n", self->speed, self->gear, getVehicleTypeString(self->type));
}


Vehicle *createVehicle(int speed, int gear, VehicleType type) {
    Vehicle *vehicle = (Vehicle *)malloc(sizeof(Vehicle));
    if (vehicle == NULL) {
        printf("Failed to create vehicle\n");
        return NULL;
    }

    switch (type) {
        case VEHICLE_TYPE_CAR:
            vehicle->accelerate = car_accelerate;
            vehicle->brake = car_brake;
            vehicle->print = car_print;
            break;
        case VEHICLE_TYPE_TRUCK:
            vehicle->accelerate = truck_accelerate;
            vehicle->brake = truck_brake;
            vehicle->print = truck_print;
            break;
        default:
            printf("Invalid vehicle type\n");
            free(vehicle);
            return NULL;
    }

    vehicle->speed = speed;
    vehicle->gear = gear;
    vehicle->type = type;

    return vehicle;
}

int main(void) {
    Vehicle *car = createVehicle(100, 1, VEHICLE_TYPE_CAR);
    if (car == NULL) {
        printf("Failed to create car\n");
        return 1;
    }
    car->print(car);
    car->accelerate(car);
    car->brake(car);

    Vehicle *truck = createVehicle(100, 1, VEHICLE_TYPE_TRUCK);
    if (truck == NULL) {
        printf("Failed to create truck\n");
        return 1;
    }
    truck->print(truck);
    truck->accelerate(truck);
    truck->brake(truck);

    free(car);
    free(truck);

    return 0;
}
```

## pthread
[memcached](https://github.com/memcached/memcached)  
> a high performance multithreaded event-based key/value cache store intended to be used in a distributed system.
[]()  
![INDEX](./c/pthread/00-INDEX.md)  
![WHY pthreads](./c/pthread/01-WHY-pthreads.md)  
![HOW pthreads](./c/pthread/02-HOW-pthreads.md)  
![WHAT advanced](./c/pthread/03-WHAT-advanced.md)  
![WHAT sync](./c/pthread/03-WHAT-sync.md)  
![WHAT threads](./c/pthread/03-WHAT-threads.md)  
![WHERE pthreads](./c/pthread/04-WHERE-pthreads.md)  
![PITFALLS pthreads](./c/pthread/05-PITFALLS-pthreads.md)  
![TRANSFER pthreads](./c/pthread/06-TRANSFER-pthreads.md)  

- mempool
[nginx](https://github.com/nginx/nginx)  
> the world's most popular Web Server, high performance Load Balancer, Reverse Proxy, API Gateway and Content Cache.
![WHY](./c/mempool/01-WHY.md)  
![HOW](./c/mempool/02-HOW.md)  
![WHAT](./c/mempool/03-WHAT.md)  
![WHERE](./c/mempool/04-WHERE.md)  
![PITFALLS](./c/mempool/05-PITFALLS.md)  
![TRANSFER](./c/mempool/06-TRANSFER.md)  

- Device OS (Firmware) for Particle Devices
[Device OS](https://docs.particle.io/getting-started/device-os/introduction-to-device-os/)  
[device-os](https://github.com/particle-iot/device-os)  
[]()  
[]()  


## memory pool
```c
/* memory pool management */

#include <stdio.h>
#include <stdlib.h>

typedef struct Object {
    int is_free;
    char data[100];
} Object;

typedef struct ObjectPool {
    Object *objects;
    int size;
    int free_count;
} ObjectPool;

ObjectPool *create_object_pool(int size) {
    ObjectPool *pool = (ObjectPool *)malloc(sizeof(ObjectPool));
    if (pool == NULL) {
        printf("Failed to create object pool\n");
        return NULL;
    }

    pool->objects = (Object *)malloc(size * sizeof(Object));
    if (pool->objects == NULL) {
        printf("Failed to create objects\n");
        free(pool);
        return NULL;
    }

    pool->size = size;
    pool->free_count = size;

    for (int i = 0; i < size; i++) {
        pool->objects[i].is_free = 1;
    }

    return pool;
}

void destroy_object_pool(ObjectPool *pool) {
    if (pool == NULL) {
        printf("Failed to destroy object pool\n");
        return;
    }

    free(pool->objects);
    free(pool);
}

Object *allocate_object(ObjectPool *pool) {
    if (pool == NULL || pool->free_count == 0) {
        return NULL;
    }

    for (int i = 0; i < pool->size; i++) {
        if (pool->objects[i].is_free) {
            pool->objects[i].is_free = 0;
            pool->free_count--;
            return &pool->objects[i];
        }
    }

    return NULL;
}

void free_object(Object *obj, ObjectPool *pool) {
    if (obj == NULL || pool == NULL) {
        printf("Failed to free object\n");
        return;
    }

    if(obj >= pool->objects && obj < pool->objects + pool->size) {
        obj->is_free = 1;
        pool->free_count++;
    } else {
        printf("Object is not in the pool\n");
    }
}

int main(void) {
    ObjectPool *pool = create_object_pool(100);
    if (pool == NULL) {
        printf("Failed to create object pool\n");
        return 1;
    }

    Object *obj1 = allocate_object(pool);
    if (obj1 == NULL) {
        printf("Failed to allocate object\n");
        return 1;
    }
    printf("Object1 allocated: %p, pool free count: %d\n", obj1, pool->free_count);

    Object *obj2 = allocate_object(pool);
    if (obj2 == NULL) {
        printf("Failed to allocate object\n");
        return 1;
    }
    printf("Object2 allocated: %p, pool free count: %d\n", obj2, pool->free_count);

    free_object(obj1, pool);
    printf("Object1 freed: %p, pool free count: %d\n", obj1, pool->free_count);

    free_object(obj2, pool);
    printf("Object2 freed: %p, pool free count: %d\n", obj2, pool->free_count);

    destroy_object_pool(pool);
    return 0;
}
```

## OOP
- 函数指针 function pointer
- 不透明指针 opaque pointer


- 函数指针
    C 函数指针，本质上只是一个 保存了“代码入口地址”的普通指针，即fp  ──►  address_of(foo)
    函数在编译后，被放进 代码段（text segment）
    函数名在大多数表达式中会 退化（decay）为该函数的入口地址
    函数指针变量只是：
        存了这个地址
        按约定的函数签名去跳转执行
```c
Memory (process address space)
┌──────────────────────────────────────────────┐
│                                              │
│  Code Segment (Text)                         │
│                                              │
│  +----------------------+                    │
│  |  function foo()      |◄───────┐           │
│  |  instructions        |        │           │
│  +----------------------+        │           │
│                                  │           │
│  +----------------------+        │           │
│  |  function bar()      |◄───┐   │           │
│  |  instructions        |    │   │           │
│  +----------------------+    │   │           │
│                              │   │           │
│  Data / Stack Segment        │   │           │
│                              │   │           │
│  +----------------------+    │   │           │
│  | function pointer fp  |────┘───┘           │
│  | (holds an address)   |                    │
│  +----------------------+                    │
│                                              │
└──────────────────────────────────────────────┘
```
- 函数指针是 C 语言里 唯一的“行为间接性”机制
- 多态（ops 表 / vtable in C）
> 核心思想：数据结构不变，行为在运行时切换
```c
#include <stdio.h>

struct ops {
    void (*start)(void);
    void (*stop)(void);
};

void eth_start(void) { printf("eth start\n"); }
void eth_stop(void)  { printf("eth stop\n"); }

void wifi_start(void) { printf("wifi start\n"); }
void wifi_stop(void)  { printf("wifi stop\n"); }

struct device {
    const struct ops *ops;
};

int main(void) {
    struct ops eth_ops  = { eth_start,  eth_stop };
    struct ops wifi_ops = { wifi_start, wifi_stop };

    struct device dev;

    dev.ops = &eth_ops;
    dev.ops->start();

    dev.ops = &wifi_ops;
    dev.ops->start();
}

/*
struct ops 就是 C 的虚函数表
device 不知道自己是什么设备
多态发生在：ops 指针被替换的那一刻
Linux 内核中：
    net_device_ops
    file_operations
    tc_ops
全部是这个模式
*/
```
- 回调（callback）
> 核心思想：控制流反转（IoC）
```c
#include <stdio.h>

void on_event(int code) {
    printf("event code = %d\n", code);
}

void register_callback(void (*cb)(int)) {
    /* 某个时刻发生事件 */
    cb(42);
}

int main(void) {
    register_callback(on_event);
}

/*
调用方 不知道 回调函数的具体逻辑
只知道：
    签名
    调用时机
这是：
    中断回调
    定时器
    网络事件
    ConfD DP callback
    的统一原型
*/
```
- 插件（plugin / module）
> 核心思想：行为在运行时“注入”
```c
#include <stdio.h>

typedef int (*plugin_fn)(int);

int plugin_a(int x) { return x + 1; }
int plugin_b(int x) { return x * 2; }

int run(plugin_fn fn, int value) {
    return fn(value);
}

int main(void) {
    printf("%d\n", run(plugin_a, 10));
    printf("%d\n", run(plugin_b, 10));
}

插件不是“动态库”这个形式
插件的本质是：
    “一组可替换的函数地址”
动态加载 (dlopen) 只是：
    把函数地址填进指针
```

- 状态机（FSM）
> 核心思想：状态 = 下一步该调用哪个函数
```c
#include <stdio.h>

typedef void (*state_fn)(void);

void state_idle(void);
void state_run(void);
void state_stop(void);

state_fn current;

void state_idle(void) {
    printf("IDLE\n");
    current = state_run;
}

void state_run(void) {
    printf("RUN\n");
    current = state_stop;
}

void state_stop(void) {
    printf("STOP\n");
    current = state_idle;
}

int main(void) {
    current = state_idle;

    for (int i = 0; i < 5; i++)
        current();
}

状态不是 enum
状态是：
    “当前函数指针指向哪里”
FSM 的“跳转表”本质就是：
    函数指针
    或函数指针数组
```

- 事件驱动（event-driven loop）
> 核心思想：事件 → handler 映射表
```c
#include <stdio.h>

typedef void (*handler_fn)(void);

void on_read(void)  { printf("READ event\n"); }
void on_write(void) { printf("WRITE event\n"); }

enum event_type {
    EV_READ,
    EV_WRITE,
    EV_MAX
};

handler_fn handlers[EV_MAX] = {
    [EV_READ]  = on_read,
    [EV_WRITE] = on_write,
};

void dispatch(enum event_type ev) {
    handlers[ev]();
}

int main(void) {
    dispatch(EV_READ);
    dispatch(EV_WRITE);
}

事件循环 ≠ while
核心是：
    “事件编号 → 函数地址”
epoll / select / libevent / 内核 softirq 全部是这个模型的变体
```
- Examples
```c
#include <stdio.h>
#include <string.h>

/*高内聚低耦合*/
struct dog {
    char name[20];
    int age;

    void (*bark)();
    viud (*eat)(char *food);
};

void dog_bark() {
    printf("Woof! Woof!\n");
}

void dog_eat(char *food) {
    printf("Yummy! I'm eating %s\n", food);
}

int main() {
    struct dog my_dog;
    strcpy(my_dog.name, "Buddy");
    my_dog.age = 3;
    my_dog.bark = dog_bark;
    my_dog.eat = dog_eat;

    my_dog.bark();
    my_dog.eat("bone");

    return 0;
}
```

- 不透明指针 opaque pointer
- bank_counter.h
```c
#ifndef BANK_ACCOUNT_H
#define BANK_ACCOUNT_H

#include <stdbool.h>

// forward decalaration
typedef struct bank_account bank_account;

bank_account *bank_account_create(char *name, double balance);
bool bank_account_deposit(bank_account *account, double amount);
bool bank_account_withdraw(bank_account *account, double amount);
double bank_account_get_balance(bank_account *account);
void bank_account_destroy(bank_account *account);

#endif
```
- bank_counter.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bank_account.h"

struct bank_account {
    char name[20];
    double balance;
    int transaction_count;
};

bank_account *bank_account_create(char *name, double balance) {
    bank_account *account = malloc(sizeof(bank_account));
    if (!account) {
        return NULL;
    }

    strncpy(account->name, name, sizeof(account->name) - 1);
    account->name[sizeof(account->name) - 1] = '\0';
    account->balance = balance;
    account->transaction_count = 0;

    return account;
}

void bank_account_destroy(bank_account *account) {
    free(account);
}

bool bank_account_deposit(bank_account *account, double amount) {
    if (account == NULL || amount <= 0) {
        return false;
    }

    account->balance += amount;
    account->transaction_count++;
    return true;
}

bool bank_account_withdraw(bank_account *account, double amount) {
    if (account == NULL || amount <= 0 || account->balance < amount) {
        return false;
    }

    account->balance -= amount;
    account->transaction_count++;
    return true;
}

double bank_account_get_balance(bank_account *account) {
    if (account == NULL) {
        return 0;
    }

    return account->balance;
}
```
- main.c
```c
#include <stdio.h>
#include "bank_account.h"

int main() {
    bank_account *account = bank_account_create("John Doe", 1000.00);
    if (!account) {
        printf("Failed to create account\n");
        return 1;
    }

    printf("Account created. Balance: %f\n", bank_account_get_balance(account));

    if (!bank_account_deposit(account, 500.00)) {
        printf("Failed to deposit\n");
        bank_account_destroy(account);
        return 1;
    }

    printf("Deposit successful. Balance: %f\n", bank_account_get_balance(account));

    if (!bank_account_withdraw(account, 200.00)) {
        printf("Failed to withdraw\n");
        bank_account_destroy(account);
        return 1;
    }

    printf("Withdraw successful. Balance: %f\n", bank_account_get_balance(account));

    bank_account_destroy(account);

    return 0;
}
```

- 继承: “在不改变调用方的前提下，扩展或替换行为和数据”
- 工程上最重要的三种方法
```
1. struct 前缀嵌入        → 数据继承
2. ops 表 + 回调         → 行为继承（多态）
3. container_of          → 上下文找回
```
- 结构体嵌套
> 核心思想:把“父类”放在子类 struct 的开头
```c
#include <stdio.h>

struct base {
    int id;
};

struct derived {
    struct base base;   // 必须是第一个成员
    int extra;
};

void base_print(struct base *b) {
    printf("id = %d\n", b->id);
}

int main(void) {
    struct derived d = {
        .base = { .id = 42 },
        .extra = 100,
    };

    base_print(&d.base);   // 向上转型
}

WHY 这算“继承”
    struct derived* 可以 安全地当作 struct base*
    内存布局前缀一致
    C ABI 级别保证
工程中的例子
    Linux 内核：
        struct device
        struct net_device
        struct inode
驱动模型的基石
```

- container_of（反向继承）
> 核心思想:从“父类指针”找回“子类对象”
```c
#include <stdio.h>
#include <stddef.h>

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct base {
    int id;
};

struct derived {
    struct base base;
    int extra;
};

void handle_base(struct base *b) {
    struct derived *d =
        container_of(b, struct derived, base);
    printf("extra = %d\n", d->extra);
}

WHY 重要
    支持 多态回调
    父接口不知道子类型
    子类型仍能拿回完整上下文
工程中的例子
    Linux：
        file_operations
        net_device_ops
        work_struct
```

- ops 表（行为继承 / 多态）
> 核心思想:数据继承 + 行为表
```c
#include <stdio.h>

struct ops {
    void (*print)(void *);
};

struct base {
    const struct ops *ops;
};

void base_print(void *self) {
    printf("base\n");
}

void derived_print(void *self) {
    printf("derived\n");
}

struct ops base_ops = { base_print };
struct ops derived_ops = { derived_print };

int main(void) {
    struct base b = { &base_ops };
    struct base d = { &derived_ops };

    b.ops->print(&b);
    d.ops->print(&d);
}

WHY 这是“真正的 OO 行为继承”
    调用方只依赖 base
    行为在运行时替换
    C 的虚函数表
工程中的例子
    struct file_operations
    struct net_device_ops
    ConfD DP callbacks
```

- 嵌套组合（has-a 继承）
> 核心思想:用组合替代“is-a”
```c
struct engine {
    int power;
};

struct car {
    struct engine eng;
};

什么时候算“继承”
    当接口只暴露 engine
    car 对外表现得像 engine
```

- 接口继承（纯行为）
> 核心思想:没有数据，只有函数表
```c
struct iface {
    int (*read)(void *);
    int (*write)(void *);
};

适用场景
    插件系统
    驱动接口
    跨模块 ABI
```

- Examples
```c
// 结构体嵌套实现数据的继承
// 用函数指针模拟"虚函数"，实现方法的继承和重写
typedef struct Vehicle {
    char brand[50];
    char color[50];
    int speed;

    void (*drive)(struct Vehicle *self);
    void (*display)(struct Vehicle *self);
} Vehicle;

typedef struct SportsCar {
    Vehicle base;   // 继承父类

    int maxSpeed;   // 子类特有属性
    int acceleration;

    // 子类可以重写父类的方法
} SportsCar;

//
#include <stdio.h>

typedef struct Animal {
    char name[20];
    int age;

    void (*speak)(struct Animal *animal);
} Animal;

typedef struct Dog {
    Animal animal;

    char breed[20];
} Dog;

typedef struct Cat {
    Animal animal;

    char color[20];
} Cat;

void dog_speak(Animal *animal) {
    printf("Dog %s says woof\n", animal->name);
}

void cat_speak(Animal *animal) {
    printf("Cat %s says meow\n", animal->name);
}

int main() {
    Dog dog = { .animal = { .name = "Buddy", .age = 3, .speak = dog_speak }, .breed = "Labrador" };
    Cat cat = { .animal = { .name = "Whiskers", .age = 2, .speak = cat_speak }, .color = "Gray" };

    Animal *animals[] = { (Animal *)&dog, (Animal *)&cat };

    for (int i = 0; i < sizeof(animals) / sizeof(Animal *); i++) {
        animals[i]->speak(animals[i]);
    }

    return 0;
}
```

- 多态: 同一个行为，不同对象有不同的表现形式(开闭原则)
> 核心思想：通过函数指针实现运行时对象行为的动态绑定
```c
// 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Animal {
    char name[20];

    void (*speak)(struct Animal *animal);
    void (*eat)(struct Animal *animal);
} Animal;

typedef struct Dog {
    Animal animal;

    char breed[20];
} Dog;

typedef struct Cat {
    Animal animal;

    char color[20];
} Cat;

void dog_speak(Animal *animal) {
    printf("Dog %s says woof\n", animal->name);
}

void dog_eat(Animal *animal) {
    printf("Dog %s is eating\n", animal->name);
}

Dog *dog_create(char *name, char *breed) {
    Dog *dog = malloc(sizeof(Dog));
    if (!dog) {
        return NULL;
    }

    strncpy(dog->animal.name, name, sizeof(dog->animal.name) - 1);
    dog->animal.name[sizeof(dog->animal.name) - 1] = '\0';
    dog->animal.speak = dog_speak;
    dog->animal.eat = dog_eat;
    strncpy(dog->breed, breed, sizeof(dog->breed) - 1);
    dog->breed[sizeof(dog->breed) - 1] = '\0';
    return dog;
}

void dog_destroy(Dog *dog) {
    free(dog);
}

void cat_speak(Animal *animal) {
    printf("Cat %s says meow\n", animal->name);
}

void cat_eat(Animal *animal) {
    printf("Cat %s is eating\n", animal->name);
}

Cat *cat_create(char *name, char *color) {
    Cat *cat = malloc(sizeof(Cat));
    if (!cat) {
        return NULL;
    }

    strncpy(cat->animal.name, name, sizeof(cat->animal.name) - 1);
    cat->animal.name[sizeof(cat->animal.name) - 1] = '\0';
    cat->animal.speak = cat_speak;
    cat->animal.eat = cat_eat;
    strncpy(cat->color, color, sizeof(cat->color) - 1);
    cat->color[sizeof(cat->color) - 1] = '\0';
    return cat;
}

void cat_destroy(Cat *cat) {
    free(cat);
}

void animal_speak(Animal *animal) {
    animal->speak(animal);
}

void animal_eat(Animal *animal) {
    animal->eat(animal);
}

int main() {
    Dog *dog = dog_create("Buddy", "Labrador");
    Cat *cat = cat_create("Whiskers", "Gray");

    animal_speak((Animal *)dog);
    animal_eat((Animal *)dog);
    animal_speak((Animal *)cat);
    animal_eat((Animal *)cat);

    dog_destroy(dog);
    cat_destroy(cat);

    return 0;
}

//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration
typedef struct Animal Animal;

typedef struct {
    void (*speak)(Animal *animal);
    void (*eat)(Animal *animal);
    void (*destroy)(Animal *animal);
} AnimalOps; // 类似C++的虚函数表

struct Animal {
    AnimalOps *ops;

    char name[20];
};

typedef struct Dog {
    Animal animal;

    char breed[20];
} Dog;

typedef struct Cat {
    Animal animal;

    char color[20];
} Cat;

#define CALL(obj, method, ...) ((obj)->ops->method(obj, ##__VA_ARGS__))

// Forward declare ops tables
static const AnimalOps dog_ops;
static const AnimalOps cat_ops;

void dog_speak(Animal *animal) {
    printf("Dog %s says woof\n", animal->name);
}

void dog_eat(Animal *animal) {
    printf("Dog %s is eating\n", animal->name);
}

void dog_destroy(Animal *animal) {
    free(animal);
}

Dog *dog_create(char *name, char *breed) {
    Dog *dog = malloc(sizeof(Dog));
    if (!dog) {
        return NULL;
    }

    strncpy(dog->animal.name, name, sizeof(dog->animal.name) - 1);
    dog->animal.name[sizeof(dog->animal.name) - 1] = '\0';
    strncpy(dog->breed, breed, sizeof(dog->breed) - 1);
    dog->breed[sizeof(dog->breed) - 1] = '\0';
    dog->animal.ops = (AnimalOps *)&dog_ops;
    return dog;
}

void cat_speak(Animal *animal) {
    printf("Cat %s says meow\n", animal->name);
}

void cat_eat(Animal *animal) {
    printf("Cat %s is eating\n", animal->name);
}

void cat_destroy(Animal *animal) {
    free(animal);
}

Cat *cat_create(char *name, char *color) {
    Cat *cat = malloc(sizeof(Cat));
    if (!cat) {
        return NULL;
    }

    strncpy(cat->animal.name, name, sizeof(cat->animal.name) - 1);
    cat->animal.name[sizeof(cat->animal.name) - 1] = '\0';
    strncpy(cat->color, color, sizeof(cat->color) - 1);
    cat->color[sizeof(cat->color) - 1] = '\0';
    cat->animal.ops = (AnimalOps *)&cat_ops;
    return cat;
}

void animal_speak(Animal *animal) {
    CALL(animal, speak);
}

void animal_eat(Animal *animal) {
    CALL(animal, eat);
}

void animal_destroy(Animal *animal) {
    CALL(animal, destroy);
}

static const AnimalOps dog_ops = {
    .speak = dog_speak,
    .eat = dog_eat,
    .destroy = dog_destroy,
};

static const AnimalOps cat_ops = {
    .speak = cat_speak,
    .eat = cat_eat,
    .destroy = cat_destroy,
};

int main() {
    Dog *dog = dog_create("Buddy", "Labrador");
    Cat *cat = cat_create("Whiskers", "Gray");

    animal_speak(&dog->animal);
    animal_eat(&dog->animal);
    animal_speak(&cat->animal);
    animal_eat(&cat->animal);

    animal_destroy(&dog->animal);
    animal_destroy(&cat->animal);

    return 0;
}
```

- uart example
- uart_device.h
```c
#ifndef UART_DEVICE_H
#define UART_DEVICE_H

#include <stdint.h>

typedef struct UART_Device UART_Device;

struct UART_Device {
    UART_TypeDef *uart;
    uint32_t baud_rate;
    uint8_t tx_buffer[256];
    uint8_t rx_buffer[256];

    void (*init)(UART_Device *self);
    void (*send)(UART_Device *self, const uint8_t *data, uint16_t len);
    void (*recv)(UART_Device *self, uint8_t *data, uint16_t max_len);
    void (*deinit)(UART_Device *self);
};

#endif
```
- uart_device.c
```c
#include <string.h>
#include "uart_device.h"

static void uart_device_init(UART_Device *self) {
    // 使能UART时钟
    if(self->uart == UART1) {
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    }

    // 设置UART波特率
    uint32_t usartdiv = 42000000 / (16 * self->baud_rate);
    self->uart->BRR = usartdiv;

    // 设置UART参数
    self->uart->CR1 = USART_CR1_UE |  // 使能UART
                      USART_CR1_TE |  // 使能发送
                      USART_CR1_RE;  // 使能接收

    // 清空TX和RX缓冲区
   memset(self->tx_buffer, 0, sizeof(self->tx_buffer));
   memset(self->rx_buffer, 0, sizeof(self->rx_buffer));
}

static void uart_device_send(UART_Device *self, const uint8_t *data, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) {
        // 等待TX寄存器空闲
        while(!(self->uart->SR & USART_SR_TXE));
        self->uart->DR = data[i];
        // 等待发送完成
        while(!(self->uart->SR & USART_SR_TC));
    }
}

static void uart_device_recv(UART_Device *self, uint8_t *data, uint16_t max_len) {
    for(uint16_t i = 0; i < max_len; i++) {
        // 等待RX寄存器非空
        while(!(self->uart->SR & USART_SR_RXNE));
        data[i] = self->uart->DR;
    }
}

static void uart_device_deinit(UART_Device *self) {
    self->uart->CR1 = 0;
    self->uart->CR2 = 0;
    self->uart->CR3 = 0;
}

UART_Device *uart_device_create(UART_TypeDef *uart, uint32_t baud_rate) {
    
    UART_Device *dev = malloc(sizeof(UART_Device));
    if(!dev) {
        return NULL;
    }

    dev->uart = uart;
    dev->baud_rate = baud_rate;

    dev->init = uart_device_init;
    dev->send = uart_device_send;
    dev->recv = uart_device_recv;
    dev->deinit = uart_device_deinit;

    dev->init(dev);

    return dev;
}
```
- main.c
```c
int main() {
    UART_Device *uart1 = uart_device_create(USART1, 115200);
    if(!uart1) {
        return -1;
    }

    UART_Device *uart2 = uart_device_create(USART2, 115200);
    if(!uart2) {
        return -1;
    }

    uart1->send(uart1, "Hello, World!", 13);
    uart1->recv(uart1, 10);

    uart2->send(uart2, "Hello, World!", 13);
    uart2->recv(uart2, 10);

    uart1->deinit(uart1);
    uart2->deinit(uart2);

    return 0;
}
```

## libraries
> jansson - C library for encoding, decoding and manipulating JSON data
[jansson](https://github.com/akheron/jansson)  

> json-c - JSON-C implements a reference counting object model that allows you to easily construct JSON objects in C
[json-c](https://github.com/json-c/json-c)  

> the LIBpcap interface to various kernel packet capture mechanism
[libpcap](https://github.com/the-tcpdump-group/libpcap)  

> CppUTest unit testing and mocking framework for C/C++
[cpputest](https://github.com/cpputest/cpputest)  

> High-level C binding for ØMQ
[czmq](https://github.com/zeromq/czmq)  

> a small daemon which collects system information periodically and provides mechanisms to store and monitor the values in a variety of ways.
[collectd](https://github.com/collectd/collectd)  

> Protocol Buffers (a.k.a., protobuf) are Google's language-neutral, platform-neutral, extensible mechanism for serializing structured data.
[protobuf](https://github.com/protocolbuffers/protobuf)  

> TLS/SSL and crypto library
[openssl](https://github.com/openssl/openssl)  

> The Apache Kafka C/C++ library
[librdkafka](https://github.com/confluentinc/librdkafka)  

> C++ based gRPC (C++, Python, Ruby, Objective-C, PHP, C#)
[grpc](https://github.com/grpc/grpc)  

> a set of utilities for Linux networking
[iproute2](https://github.com/iproute2/iproute2)  

> a tool to manipulate and display information about what parts of a program are actually executed (i.e. "covered") while running a particular test case or set of testcases.
[lcov](https://github.com/linux-test-project/lcov)  

> A command line tool and library for transferring data with URL syntax
[curl](https://github.com/curl/curl)  

> libxml2 - is an XML toolkit implemented in C, originally developed for the GNOME Project
[libxml2](https://github.com/GNOME/libxml2)  
[]()  
[]()  
[]()  
[]()  


![C Guidelines](./c/c_systems_programming_guidelines.md)  
![]()  


- Data Strutures
![array](./c/data_structures/ds_01_array.md)  
![singly linked list](./c/data_structures/ds_02_singly_linked_list.md)  
![doubly linked list](./c/data_structures/ds_03_doubly_linked_list.md)  
![intrusive linked list](./c/data_structures/ds_04_intrusive_linked_list.md)  
![stack](./c/data_structures/ds_05_stack.md)  
![queue](./c/data_structures/ds_06_queue.md)  
![ring buffer](./c/data_structures/ds_07_ring_buffer.md)  
![binary tree](./c/data_structures/ds_08_binary_tree.md)  
![binary search tree](./c/data_structures/ds_09_binary_search_tree.md)  
![balanced trees](./c/data_structures/ds_10_balanced_trees.md)  
![hash table](./c/data_structures/ds_11_hash_table.md)  
![advanced structures](./c/data_structures/ds_12_advanced_structures.md)  

> Raylib — Clean C Library with Layered Subsystems
[raylib](https://github.com/raysan5/raylib)  
![Learn Architecture from raylib](./c/raylib_ARCHITECTURE_CASE_STUDY.md)  

> sqlite - 
![Learn Architecture from sqlite](./c/sqlite_LAYERED_ARCHITECTURE_ANALYSIS.md)  
![Boundaries and Contracts in C Architectures](./c/sqlite_boundaries_and_contracts_in_c.md)  


[Understanding the C Runtime: crt0, crt1, crti, and crtn](https://www.inferara.com/en/blog/c-runtime/)  
[C/C++ Runtime Startup](https://etherealwake.com/2021/09/crt-startup/)  
[Explanation of crt0, crt1, crt1, and crtn (C- Runtime)](https://thejat.in/learn/explanation-of-crt0-crt1-crt1-and-crtn-c-runtime)  
[CRT: C Run Time Before Starting main()](https://vishalchovatiya.com/posts/crt-run-time-before-starting-main/)  
[**Linux-dlsym**](https://lambertxiao.github.io/posts/linux-dlsym/doc/)  
[真正理解 RTLD_NEXT 的作用](https://csstormq.github.io/)  
[Dynamic Linking Example](https://vishalchovatiya.com/posts/dynamic-linking-example/)  
[]()  
[]()  
[]()  
[How to make a system call in C](https://jameshfisher.com/2018/02/19/how-to-syscall-in-c/)  
[How do I call a program from C?](https://jameshfisher.com/2017/02/07/how-do-i-call-a-program-in-c/)  
[How do I call a program in C, setting up standard pipes?](https://jameshfisher.com/2017/02/17/how-do-i-call-a-program-in-c-with-pipes/)  
[How do I use fork in C?](https://jameshfisher.com/2017/02/06/how-do-i-use-fork-in-c/)  
[What is a “file descriptor”, really?](https://jameshfisher.com/2016/12/15/file-descriptor-misnomer/)  
[What does getaddrinfo do?](https://jameshfisher.com/2018/02/03/what-does-getaddrinfo-do/)  
[What is a a FIFO, or “named pipe”? What is mkfifo in C?](https://jameshfisher.com/2017/02/21/mkfifo-c/)  
[What is static linking in C?](https://jameshfisher.com/2017/08/26/static-linking/)  
[What are setjmp and longjmp in C?](https://jameshfisher.com/2017/02/18/what-are-setjmp-longjmp/)  
[How to generate Intel and AT&T assembly with clang](https://jameshfisher.com/2017/02/19/clang-assembly-att-intel/)  
[How do I duplicate a file descriptor in C?](https://jameshfisher.com/2017/02/15/c-dup-syscall/)  
[How do I use execve in C?](https://jameshfisher.com/2017/02/05/how-do-i-use-execve-in-c/)  
[How do I generate assembly from a C file?](https://jameshfisher.com/2017/02/03/c-generate-assembly/)  
[How can I read a file with mmap in C?](https://jameshfisher.com/2017/01/27/mmap-file-read/)  
[What is realloc in C?](https://jameshfisher.com/2016/12/01/c-realloc/)  
[What is mmap in C?](https://jameshfisher.com/2017/01/26/mmap/)  
[How can I write a file with mmap in C?](https://jameshfisher.com/2017/01/28/mmap-file-write/)  
[In what ways can processes communicate?](https://jameshfisher.com/2017/01/29/process-communication-methods/)  
[What are ‘signals’ in C?](https://jameshfisher.com/2017/01/09/c-signals/)  
[What does the C signal function return?](https://jameshfisher.com/2017/01/10/c-signal-return-value/)  
[How do I unregister a signal handler in C?](https://jameshfisher.com/2017/01/11/c-signal-unregister/)  
[What does void mean as a function parameter in C?](https://jameshfisher.com/2016/11/27/c-void-params/)  
[What are lvalue and rvalue in C?](https://jameshfisher.com/2017/01/21/c-lvalue-rvalue/)  
[What are the domain and type arguments to the socket system call?](https://jameshfisher.com/2017/02/27/socket-types/)  
[Variadic Function Working in C](https://www.thejat.in/blog/variadic-function-working-in-c)  
[Understanding Complex C/C++ Declarations](https://www.thejat.in/blog/understanding-complex-cc-declarations)  
[Exploring Singleton Pattern in C++: Ensuring Unique Instances](https://www.thejat.in/blog/exploring-singleton-pattern-in-c-ensuring-unique-instances)  
[]()  

## Methods to Denest(减少函数嵌套的方法)
1. Extraction(提取法:把函数的一部分提出出来变成独立的函数)
2. Inversion(反转法:反转提前判断，改用提前判断的方式)
```c
// Before
int calculate(int bottom, int top)
{
	if (top > bottom)
	{
		int sum = 0;

		for (int number = bottom; number <= top; number++)
		{
			if (number % 2 == 0)
			{
				sum += number;
			}
		}

		return sum;
	}
	else
	{
		return 0;
	}
}

// After
int filterNumber(int number)
{
	if (number % 2 == 0)
	{
		return number;
	}

	return 0;
}

int calculate(int bottom, int top)
{
	if (top < bottom)
	{
		return 0;
	}

	int sum = 0;

	for (int number = bottom; number <= top; number++)
	{
		sum += filterNumber(number);
	}

	return sum;
}
```

## C 分层结构
![嵌入式C可复用技巧](./c_tips/embedded_c_reusability.md)  
![C语言软件分层架构1](./c_tips/c_layer_interaction_part1.md)  
![C语言软件分层架构2](./c_tips/c_layer_interaction_part2.md)  

- 设计模式
![单例模式](./c_tips/patterns/singleton.md)  
![工厂模式](./c_tips/patterns/factory.md)  
![观察者模式](./c_tips/patterns/observer.md)  
![状态机模式](./c_tips/patterns/state_machine.md)  
![命令模式](./c_tips/patterns/command.md)  
![装饰器模式](./c_tips/patterns/decorator.md)  
![适配器模式](./c_tips/patterns/adapter.md)  
![对象池模式](./c_tips/patterns/object_pool.md)  
![责任链模式](./c_tips/patterns/chain_of_responsibility.md)  
![模板方法模式](./c_tips/patterns/template_method.md)  
![服务定位器模式](./c_tips/patterns/service_locator.md)  
![策略模式](./c_tips/patterns/strategy.md)  
![依赖注入模式](./c_tips/patterns/dependency_injection.md)  
![依赖注入 + 服务器定位 + 策略模式](./c_tips/patterns/DI_ServiceLoader_Strategy.md)  

## C 工程技巧
![单例模式](./c_tips/01_单例模式_Singleton.md)  
![单例模式](./c_tips/01_单例模式_Singleton_Pattern.md)  
![工厂模式](./c_tips/02_工厂模式_Factory.md)  
![适配器模式](./c_tips/03_适配器模式_Adapter.md)  
![观察者模式](./c_tips/04_观察者模式_Observer.md)  
![策略模式](./c_tips/05_策略模式_Strategy.md)  
![状态模式](./c_tips/06_状态模式_State.md)  
![回调模式](./c_tips/07_回调模式_Callback.md)  
![C语言软件工程技巧](./c_tips/08_C语言软件工程技巧.md)  
![不透明指针](./c_tips/09_不透明指针_Opaque_Pointer.md)  
![X-Mac技术](./c_tips/10_X-Macro技术.md)  
![内存池](./c_tips/11_内存池_Memory_Pool.md)  
![错误处理策](./c_tips/12_错误处理策略_Error_Handling.md)  
![契约式编程](./c_tips/13_契约式编程_Design_by_Contract.md)  
![侵入式数据结构](./c_tips/14_侵入式数据结构_Intrusive.md)  
![编译时断言](./c_tips/15_编译时断言_Static_Assert.md)  
![属性清理](./c_tips/16_属性清理_RAII_Cleanup.md)  
![泛型宏编程](./c_tips/17_泛型宏编程_Generic.md)  
[防御式编程](./c_tips/18_防御式编程_Defensive.md)  
![]()  
![]()  
![]()  
![]()  
![]()  
![]()  
![]()  


## link list
![单向链表](./link_list/singly_linked_list.md)  
![双向链表](./link_list/doubly_linked_list.md)  
![]()  

## tree
![树形数据结构](./tree/tree_data_structures.md)  
![]()  


## 类型擦除 + 依赖注入
![依赖注入 (Dependency Injection)](./DI/Dependency_Injection_in_C.md)  
[Dependency Injection](https://github.com/rahff/Software_book) #pdf  
[Dependency Injection in .NET](https://github.com/huutoannht/Books)#pdf  
[Design Pattern Online Training using .NET](https://dotnettutorials.net/lesson/design-patterns-online-training/)  
[Dependency Injection](https://blog.ploeh.dk/tags/#Dependency%20Injection-ref)  
[Hands-On Dependency Injection in Go](https://github.com/PacktPublishing/Hands-On-Dependency-Injection-in-Go)  
[]()  
[]()  
[]()  
[]()  
```
核心概念：

    WITHOUT DI (紧耦合)                    WITH DI (松耦合)

    ┌─────────────────┐                   ┌─────────────────┐
    │  OrderService   │                   │  OrderService   │
    │                 │                   │                 │
    │  FileLogger log │──┐                │  Logger *logger │──► <<interface>>
    │  log.write(...) │  │                │  logger->log()  │         Logger
    └─────────────────┘  │                └─────────────────┘            │
                        │                                         ┌────┴────┐
                        ▼                                         ▼         ▼
                ┌─────────────┐                              FileLogger  MockLogger
                │ FileLogger  │
                │ (hardcoded) │
                └─────────────┘

C语言实现关键技术：

    /* 接口 = 函数指针结构体 */
    typedef struct Logger {
        void (*log)(struct Logger *self, const char *msg);
        void (*destroy)(struct Logger *self);
        void *private_data;  /* 实现特定数据 */
    } Logger;

    /* 构造函数注入 */
    OrderService* order_service_create(Logger *logger) {
        OrderService *svc = malloc(sizeof(OrderService));
        svc->logger = logger;  /* 注入依赖 */
        return svc;
    }

识别需要依赖注入的代码坏味道：

    坏味道	        示例
    直接实例化	    FileLogger *log = file_logger_create();
    全局变量	    static Database *g_database;
    硬编码调用	    send_email("smtp.example.com", ...)
    条件分支切换	if (use_file) {...} else if (use_syslog) {...}

重构6步骤：
    1. 识别依赖 - 找到函数内创建的具体对象
    2. 提取接口 - 定义函数指针结构体
    3. 添加依赖字段 - 在主结构体中添加接口指针
    4. 构造函数注入 - 通过创建函数传入依赖
    5. 使用接口 - 业务逻辑只调用接口方法
    6. 组合根 - 在 main 中创建和组装所有对象
```
- 类型擦除  =  统一接口 + void* 作为 Opaque 实例
- 依赖注入  =  外部提供/赋值/注册 ops，实现控制反转和可替换实现

- 类型擦除 (Type Erasure)
	把具体类型隐藏/擦掉，只通过一个统一的抽象接口来操作对象

    - C实现方法:
        void* 作为通用容器（类型被“擦掉”）
        函数指针 (*fn)(void*) 作为统一操作接口

- 依赖注入 (Dependency Injection, DI)
	一个模块不自己创建它依赖的对象，而是由外部把依赖"注入"（传入）给它

    - 目的:
        解耦
        方便替换实现(Mock/Test/Plugin)
        控制反转(IoC: Inversion of Control)(你不找依赖，而是依赖找你)

    - C实现方法:
        通过函数参数传入依赖
        通过结构体保存依赖的接口或void*指针

- 示例:
```c
/* version 1: 函数参数注入依赖 */

#include <stdio.h>
#include <string.h>

// -------- 抽象依赖接口（类型擦除封装） --------
typedef struct {
    void *instance;                     // 被擦除的具体类型
    void (*logFn)(void *, const char *);// 统一操作接口
} Logger;

// -------- 具体依赖实现 1 --------
typedef struct {
    char prefix[16];
} ConsoleLogger;

void consoleLog(void *self, const char *msg) {
    ConsoleLogger *logger = (ConsoleLogger*)self;
    printf("%s: %s\n", logger->prefix, msg);
}

// -------- 具体依赖实现 2（可被替换）--------
typedef struct { } SimpleLogger;

void simpleLog(void *self, const char *msg) {
    (void)self;
    printf("[LOG] %s\n", msg);
}

// -------- 需要被注入依赖的函数 --------
void fun(Logger logger) {
    logger.logFn(logger.instance, "Linux boot study is fun!");
}

// -------- main 负责创建并注入依赖 --------
int main() {
    // 创建 logger 实现 1
    ConsoleLogger clog;
    strcpy(clog.prefix, "Kernel");

    Logger logger1 = { &clog, consoleLog };
    fun(logger1);  // 依赖注入 + 类型擦除调用

    // 也可以替换为实现 2
    SimpleLogger slog;
    Logger logger2 = { &slog, simpleLog };
    fun(logger2);  // 无需修改 fun 内部代码

    return 0;
}

/* version 2: 结构体成员注入依赖 */

#include <stdio.h>
#include <string.h>

// 被擦除的依赖接口
typedef struct {
    void *instance;
    void (*logFn)(void *, const char *);
} Logger;

// 具体依赖实现
typedef struct {
    char prefix[16];
} ConsoleLogger;

void consoleLog(void *self, const char *msg) {
    ConsoleLogger *logger = (ConsoleLogger*)self;
    printf("%s: %s\n", logger->prefix, msg);
}

// 业务模块结构体，依赖通过成员注入
typedef struct {
    Logger logger; // 依赖
} Module;

void fun(Module *module) {
    module->logger.logFn(module->logger.instance, "Injected by struct member!");
}

int main() {
    // 创建具体 Logger
    ConsoleLogger clog;
    strcpy(clog.prefix, "Boot");

    // 注入到 Module 结构体成员
    Module m = { .logger = { &clog, consoleLog } };

    fun(&m);
    return 0;
}


/* version 3: 函数指针表模拟面向对象 + 多态 */

#include <stdio.h>
#include <string.h>

// ----- 模拟抽象基类 -----
typedef struct Logger Logger;

typedef struct {
    void (*log)(Logger*, const char *);
} LoggerOps;

struct Logger {
    void *instance;
    LoggerOps *ops; // vtable
};

// ----- 具体类 1 -----
typedef struct {
    char prefix[16];
} ConsoleLogger;

void consoleLogger_log(Logger *logger, const char *msg) {
    ConsoleLogger *c = (ConsoleLogger*)logger->instance;
    printf("%s: %s\n", c->prefix, msg);
}

LoggerOps consoleOps = {
    .log = (void(*)(Logger*,const char*))consoleLogger_log
};

// ----- 具体类 2 -----
typedef struct { } SimpleLogger;

void simpleLogger_log(Logger *logger, const char *msg) {
    (void)logger;
    printf("[SIMPLE] %s\n", msg);
}

LoggerOps simpleOps = {
    .log = (void(*)(Logger*,const char*))simpleLogger_log
};

// ----- 业务代码，依赖抽象 Logger（多态） -----
typedef struct {
    Logger logger; // 通过成员组合持有
} Module;

void fun(Module *m) {
    m->logger.ops->log((Logger*)&m->logger.ops[-1], "Polymorphism style!"); 
    // ↑ 这里只是演示 Linux-like ops 调用，不推荐在真实代码里这么写
}

int main() {
    // 创建具体对象 1
    ConsoleLogger clog;
    strcpy(clog.prefix, "Kernel");
    Logger logger1 = { .instance = &clog, .ops = &consoleOps };

    // 创建具体对象 2
    SimpleLogger slog;
    Logger logger2 = { .instance = &slog, .ops = &simpleOps };

    Module m1 = { .logger = logger1 };
    Module m2 = { .logger = logger2 };

    printf("--- module 1 ---\n");
    fun(&m1);

    printf("--- module 2 ---\n");
    fun(&m2);

    return 0;
}

/* version 4: Mini 插件框架 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ----- 插件 Ops 接口（抽象能力，类似 Linux driver/file ops）-----
typedef struct {
    const char *name;
    void (*run)(void*);
    void (*destroy)(void*);
} PluginOps;

// ----- 插件实例的通用容器（类型擦除）-----
typedef struct Plugin {
    void *instance;
    PluginOps *ops;
} Plugin;

// ----- 插件实现 1 -----
typedef struct {
    char message[64];
} PluginA;

void pluginA_run(void *self) {
    PluginA *a = (PluginA*)self;
    printf("[PluginA Run] %s\n", a->message);
}

void pluginA_destroy(void *self) {
    printf("[PluginA Destroy]\n");
    free(self);
}

PluginOps pluginA_ops = {
    .name = "pluginA",
    .run = pluginA_run,
    .destroy = pluginA_destroy,
    .destroy = pluginA_destroy
};

// ----- 插件实现 2 -----
typedef struct { int x; } PluginB;

void pluginB_run(void *self) {
    PluginB *b = (PluginB*)self;
    printf("[PluginB Run] x = %d\n", b->x);
}

void pluginB_destroy(void *self) {
    printf("[PluginB Destroy]\n");
    free(self);
}

PluginOps pluginB_ops = {
    .name = "pluginB",
    .run = pluginB_run,
    .destroy = pluginB_destroy
};

// ----- 框架核心：插件注册与控制反转（依赖注入点）-----
#define MAX_PLUGINS 8
Plugin *registry[MAX_PLUGINS];
int count = 0;

void registerPlugin(void *instance, PluginOps *ops) {
    if (count >= MAX_PLUGINS) return;
    registry[count] = malloc(sizeof(Plugin));
    registry[count]->instance = instance;
    registry[count]->ops = ops;
    count++;
}

void runAll() {
    for (int i = 0; i < count; i++) {
        printf(">>> running %s\n", registry[i]->ops->name);
        registry[i]->ops->run(registry[i]->instance);
    }
}

void cleanup() {
    for (int i = 0; i < count; i++) {
        registry[i]->ops->destroy(registry[i]->instance);
        free(registry[i]);
    }
}

int main() {
    // 创建并注入 PluginA
    PluginA *a = malloc(sizeof(PluginA));
    strcpy(a->message, "Hello from pluginA");

    // 创建并注入 PluginB
    PluginB *b = malloc(sizeof(PluginB));
    b->x = 42;

    // 注册到框架（依赖注入 + 类型擦除）
    registerPlugin(a, &pluginA_ops);
    registerPlugin(b, &pluginB_ops);

    // 运行插件
    runAll();

    // 清理
    cleanup();
    return 0;
}

/* linux 内核代码中类型擦除与依赖注入 核心代码摘要 */

// 类型擦除 (Type Erasure)：void* + ops（函数指针表 / vtable）
// 抽象能力通过 ops 描述，实例用 void* 擦除具体类型
struct file_operations {
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    int     (*open) (struct inode *, struct file *);
};

struct device_driver {
    const char *name;
    int (*probe)(struct device *);   // 统一入口
    void(*remove)(struct device *);
};

// 具体实例是 opaque 的，只看成 void*
struct file {
    const struct file_operations *f_op;
    void *private_data;  // 类型被擦除
}

调用点：不关心实际类型，只关心 ops

file->f_op->read(...);
drv->probe(dev);  // 统一调用

// 依赖注入 (Dependency Injection / IoC)：核心不自己创建，由框架层传入或注册
// 框架层创建硬件抽象对象，并注入 Driver ops
driver->probe = my_probe_impl;  // 通过赋值注入依赖实现

// 平台/硬件信息由外部注入，而不是 Driver 内部生成
int my_probe_impl(struct device *dev) {
    struct of_node *dn = dev->of_node; 
    const void *dt_data = of_get_property(dn, "config", NULL);
    // ↑ 设备配置信息来自 Device Tree (bootloader/框架传入)
}

Linux 典型数据流（控制反转）：

Bootloader 解析 DTB -> 创建 device -> 注册/匹配 driver -> 调用 driver->probe(device)

Driver 不负责 new 设备对象或平台数据，它只是被动实现 ops->probe()，依赖由外部注入

```

- Linux `start_kernel()` 中的 IoC（控制反转）体现总结
    - Linux 启动初始化遵循典型的**控制反转 (IoC)** 设计：
        **核心（上层）代码掌控流程**
        **具体模块只注册能力/回调 (ops)，不直接执行调用**
        **对象和依赖实例由框架层创建或注入**
        **调用时机由框架编排，而非模块内部决定**

- 1. Console 机制：依赖注入 + 回调注册 (IoC + DI)

    - 关键点
        `start_kernel()` 调用 `console_init()` 只是初始化 console 框架
        **具体 `write` 实现由 Console Driver 通过 `register_console()` 注入**
        print/log 代码 later 只通过 `console->write()` 调用，不关心具体类型

    - 核心代码摘要
        ```c
        void console_init(void) {
            // 初始化 console 框架（console_list 等），但不包含具体 write 实现
        }

        register_console(&my_console);  // Console driver 注入具体 ops

        struct console {
            void (*write)(struct console *con, const char *s, unsigned n); // 回调表
        };
    
    - 说明
        print 代码不是主动控制谁打印、怎么打印，而是把控制权交给具体 console driver 通过 ops 提供实现 → 控制反转

- Bus/Driver/Device Model：设备由框架创建，Probe later 触发
    - 关键点
        内核先 bus_register() 建立统一总线框架
        driver_register() 只是注册 ops，不决定 probe 时机
        struct device 由 Bootloader 解析 DT、ACPI、bus scan 或 hotplug 生成并注入 Driver
        framework later 触发 driver->probe(dev)

    - 核心代码摘要
    ```c
    bus_register(&platform_bus_type); // 内核建立 Bus 框架

    driver_register(&drv); 
        // 仅注册 probe/remove/suspend 等 ops，不立即调用

    platform_device_register(dev); 
        // 设备由 platform/框架创建并注入 model

    ---------------------------------------

    struct device_driver {
        const char *name;
        int  (*probe)  (struct device *dev);
        void (*remove) (struct device *dev);
    };

    struct device {
        const struct device_driver *driver; // later 由框架依赖注入
        void *platform_data; // 类型擦除，opaque 依赖实例
    };
    ```

    -说明
        创建对象 + 调用调用时机控制权在 Bus 框架，不在 driver → IoC

- initcall 机制：初始化函数由模块注册，由内核 later 按 section 调用
    - 关键点
        模块通过 *_initcall(fn) 注册初始化能力
        start_kernel() later 统一按预定义顺序调用，不是模块内部调用
        让内核初始化过程可扩展 + 可定制

    - 核心代码摘要
    ```c
    early_initcall(my_early_fn);   // 关键基础能力注册
    core_initcall(my_core_fn);     // 核心机制注册
    device_initcall(my_driver_fn); // 设备驱动类初始化注册

    // start_kernel later 统一调用所有注册的 initcall（控制反转）
    do_initcalls();
    ```

    - 说明
        initcall 的注册者提供能力定义，真正执行者是内核框架，调用顺序来自 ELF section 注册布局 → IoC

-  ops 结构体 + void* 实例：Linux style OOP 抽象与多态
    - 关键点
        通过函数指针表实现面向对象与多态
        通过 void* 擦除类型实现Opaque 依赖实例
        具体能力通过 ops 提供，依赖通过成员注入 (DI)

    - 核心代码摘要
    ```c
    struct file_operations {
        ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
        int     (*open) (struct inode *, struct file *);
    };

    struct net_device_ops {
        int (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    };

    struct file {
        const struct file_operations *f_op; // 依赖注入 (DI)
        void *private_data; // 类型擦除 (Type Erasure)
    };
    ```

    - 说明
        这些系统调用点 later 通过 file->f_op->read()、dev->netdev_ops->ndo_start_xmit() 进行调用，
        上层不关心具体类型，只关心行为接口 ops → IoC + Polymorphism

-  IoC 在 Linux 启动阶段数据 & 控制流总览
    Power On
    ↓ (硬件流程掌控者)
    BIOS / Firmware (UEFI)
    ↓ (加载与包裹执行环境，但不掌控 kernel init 逻辑)
    Bootloader (GRUB / U-Boot / etc)
    ↓ (解析硬件配置，构建 Device Tree / 平台信息)
    Kernel (`start_kernel()`)
    ↓ (构建 IoC 框架容器，注册机制)
    bus_register(), console_init()
    ↓ (注册模块能力)
    driver_register(), *_initcall(fn)
    ↓ (Later 触发——调用时机交给 framework)
    driver->probe(device), console->write(...), do_initcalls()
    ↓ (最后控制反转交到用户态进一步接管)
    PID1 init 进程启动

- 一句话总结
Linux 启动初始化中的 IoC 本质：
    框架层创建对象 + 掌控调用时机；
    模块层注册能力/ops + 接收依赖实例；
    调用点不关心底层类型，仅调用 ops 指针

## 依赖注入DI/IoC(Inversion of Control)
![linux驱动模型](./01_driver_model_ioc.md)  
![linux console子系统](./02_console_subsystem_ioc.md)  
![VFS多态](./03_vfs_polymorphic_ops_ioc.md)  
![网络子系统](./04_net_device_ops_ioc.md)  
![initcall编排机制](./05_initcall_mechanism_ioc.md)  

- linux-ioc-patterns
├── README.md                    (360行)  - 索引与学习指南
├── 01_driver_model.md          (947行)  - Linux 驱动模型
├── 02_console_subsystem.md     (786行)  - 控制台子系统
├── 03_vfs_file_operations.md   (807行)  - VFS 多态 ops
├── 04_net_device_ops.md        (920行)  - 网络设备 ops
└── 05_initcall_mechanism.md    (791行)  - initcall 机制

- 五种 IoC 模式对比
    场景	        注入时机	                控制反转体现	                           核心好处
    Driver Model	运行时 (driver_register)	驱动不主动找设备，框架自动匹配绑定	        热插拔、统一生命周期
    Console	        运行时 (register_console)	printk 不知道输出到哪，console 动态注入	   多输出并行、运行时切换
    VFS ops	        运行时 (inode 创建时)	    read/write 调用不关心具体文件系统	        统一接口、多态实现
    net_device_ops	运行时 (probe 时)	        协议栈不关心具体硬件	                   硬件抽象、中间层设备
    initcall	    编译时 (链接器)	            模块不决定初始化时机和顺序	                自动排序、零耦合

- IoC 共同模式
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Linux 内核 IoC 通用模式                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   1. 定义接口契约 (函数指针结构体)                                           │
│      struct xxx_ops {                                                        │
│          int (*operation_a)(...);                                           │
│          int (*operation_b)(...);                                           │
│      };                                                                      │
│                                                                              │
│   2. 具体实现注入 (填充函数指针)                                             │
│      static const struct xxx_ops my_ops = {                                 │
│          .operation_a = my_impl_a,                                          │
│          .operation_b = my_impl_b,                                          │
│      };                                                                      │
│                                                                              │
│   3. 注册到框架 (关联对象与 ops)                                             │
│      obj->ops = &my_ops;                                                    │
│      register_xxx(obj);                                                     │
│                                                                              │
│   4. 框架调用 (多态分发)                                                     │
│      obj->ops->operation_a(...);                                            │
│                                                                              │
│   好处:                                                                      │
│   - 调用者与实现解耦                                                         │
│   - 支持运行时替换                                                           │
│   - 统一的生命周期管理                                                       │
│   - 易于扩展新实现                                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 二级指针
![C语言中二级指针](./C语言中二级指针.md)  

## 机制与策略分离
![Linux内核中机制与策略分离的典型场景](./Linux内核中机制与策略分离的典型场景.md)  
![C语言机制与策略分离示例代码](./C语言机制与策略分离示例代码.md)  
![private_data usage demo](./private_data_demo.c)  
![]()  
- 机制
    机制负责'能做什么'，它提供一组能力/钩子/API，本身不决定要怎么用

- 策略
    策略负责'具体怎么做'，决定具体行为

- 机制与策略分离的优势
    可扩展：增加新策略，只需实现一组回调 / 新的策略模块，不改通用机制
    可测试：机制代码可以用 '假策略' 测试（例如 stub 策略），方便单元测试
    可复用：同一机制可以被多个策略使用（例如不同调度算法、不同日志后端）
    隔离复杂性：复杂业务逻辑集中在策略层，机制层代码更小、更稳定

- 关键思想
    机制 = 通用框架 + 钩子
    策略 = 通过钩子填入的具体实现

- 如何实现机制与策略分离
    1. 识别机制
        找出 '总是需要的框架性工作'，例如
            循环调度框架（while(1) 轮询）
            共享内存映射、锁管理
            协议报文的通用解析
    2. 识别策略
        找出 '经常变' 的规则，例如
            不同的转发策略 / ACL / 计费规则
            不同的调度算法 / 优先级规则
            不同的导出目标（collectd / Prometheus / 自定义 log）
    3. 抽象出接口
        设计一个 *_ops 结构体，或一组函数指针
        让机制代码只面向这个接口，而不是具体实现
    4. 通过注入选择策略
        在初始化阶段，根据配置 / 编译选项创建具体策略对象，填好 ops 和 ctx
        把它们交给机制层统一使用
    5. 保持接口稳定
        改策略时，只动新策略模块，不改机制层 API
        机制层只做与策略无关的通用工作（资源管理、调度框架、错误处理）

- C语言中机制与策略分离的实现手段
    函数指针
    回调表
    不透明指针
    模块化设计

```c
// 回调表
// 典型写法: 机制定义一个 '操作表(ops)'，策略提供具体实现
typedef struct {
    int  (*open)(void *ctx);
    int  (*close)(void *ctx);
    int  (*send)(void *ctx, const void *buf, size_t len);
    int  (*recv)(void *ctx, void *buf, size_t len);
} net_ops_t;

typedef struct {
    net_ops_t *ops;  // 策略
    void      *ctx;  // 策略上下文（socket、fd、DPDK port 等）
} net_if_t;

// 机制：上层只依赖 net_if_t，不关心具体策略
int net_send_packet(net_if_t *iface, const void *buf, size_t len)
{
    return iface->ops->send(iface->ctx, buf, len);
}
/*
机制层：net_send_packet 等函数，只知道通过 ops->send 发送
策略层：提供 tcp_ops、udp_ops、dpdk_ops 等不同实现
当换成 DPDK、AF_XDP 或普通 socket，只需要换 ops 和 ctx，机制不变
*/

// 上下文结构 + 回调
// 常见于库或框架：机制定义一个“框架流程”，留出若干回调点
typedef struct {
    int  (*on_packet)(void *user_ctx, const uint8_t *pkt, size_t len);
    void (*on_error)(void *user_ctx, int err);
    void *user_ctx;
} packet_handler_t;

// 机制：轮询收包，并在适当时机调用策略
void run_loop(int fd, packet_handler_t *handler)
{
    uint8_t buf[2048];
    ssize_t len;

    for (;;) {
        len = recv(fd, buf, sizeof(buf), 0);
        if (len < 0) {
            if (handler->on_error)
                handler->on_error(handler->user_ctx, errno);
            continue;
        }
        if (handler->on_packet)
            handler->on_packet(handler->user_ctx, buf, (size_t)len);
    }
}

/*
run_loop 是数据面机制：负责收包、循环、错误处理框架
策略通过 handler->on_packet 和 user_ctx 决定“每个包怎么处理”
*/

// void * + 用户自定义数据结构
// 机制定义操作接口和一个不透明的 void * 句柄，策略定义真正的结构和行为
typedef struct allocator allocator_t;

allocator_t *allocator_create(void *impl, 
                              void *(*alloc)(void *impl, size_t),
                              void (*dealloc)(void *impl, void *));
void        *allocator_alloc(allocator_t *a, size_t size);
void         allocator_free(allocator_t *a, void *p);

/*
机制：只实现 allocator_t 的调度、统计、锁等通用逻辑
策略：在 impl 中放不同算法的数据结构（buddy/链表/slab），并提供相应分配/释放函数
*/

// 调度器（Scheduler）与调度算法
// 机制: 提供“添加任务”、“选择下一个任务”、“记录调度统计”等基本操作
// 策略: FIFO、优先级队列、加权轮询（WRR）、WFQ、DRR 等
// 定义通用的 scheduler_t
typedef struct scheduler_ops {
    void (*enqueue)(void *ctx, task_t *t);
    task_t *(*dequeue)(void *ctx);
} scheduler_ops_t;

typedef struct {
    scheduler_ops_t *ops;
    void            *ctx;
} scheduler_t;
// 各种调度算法实现自己的 ctx + ops，统一挂在 scheduler_t 上使用
// 很多网络栈、特别是 DPDK HQoS、SF 队列这类代码，都采用这种模式

// 日志机制与日志策略
机制：提供 log_debug, log_info, log_error 等统一入口，并处理线程安全、格式化
策略：
    输出到 syslog
    输出到文件
    输出到远程 server 或 ring buffer

C 实现：
    在 log_init() 时注册一个 log_backend 回调表（write_line, flush）
    不同部署通过配置选择不同策略实现
```

## mark a function parameter as intentionally unused
```c
void handler(int event, void *ctx)
{
    (void)ctx;  // ctx required by the signature, but unused
    printf("event = %d\n", event);
}

// how (void)parameter; works
It performs a cast-to-void, which evaluates parameter but explicitly throws away the result.
This counts as a “use”, so the compiler no longer warns.
It has zero runtime effect: the compiler optimizes it out completely.

(void)parameter; is used to:
    ✔ Silence "unused parameter" warnings
    ✔ Indicate intentional non-use
    ✔ Guarantee zero runtime cost
```

## kobject
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 简单的链表实现（放在最前以便后续结构体使用）
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

// 遍历宏
#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

// 模拟 kref (引用计数)
struct kref {
    int refcount;
};

// 模拟 kobject
struct kobject {
    char name[32];
    struct kref kref;
    void (*release)(struct kobject *kobj);
    struct list_head list;
};

// 全局对象列表
static LIST_HEAD(object_list);

// 初始化链表头
void list_head_init(void)
{
    INIT_LIST_HEAD(&object_list);
}

// kref 操作函数
void kref_init(struct kref *kref)
{
    kref->refcount = 1;
    printf("kref_init: refcount = 1\n");
}

void kref_get(struct kref *kref)
{
    kref->refcount++;
    printf("kref_get: refcount = %d\n", kref->refcount);
}

int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    kref->refcount--;
    printf("kref_put: refcount = %d\n", kref->refcount);
    
    if (kref->refcount == 0) {
        if (release) {
            release(kref);
        }
        return 1;
    }
    return 0;
}

// kobject 操作函数
void kobject_init(struct kobject *kobj, const char *name)
{
    strncpy(kobj->name, name, sizeof(kobj->name) - 1);
    kobj->name[sizeof(kobj->name) - 1] = '\0';
    kref_init(&kobj->kref);
    INIT_LIST_HEAD(&kobj->list);
    
    // 添加到全局列表
    list_add(&kobj->list, &object_list);
    printf("kobject_init: created '%s'\n", name);
}

struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj) {
        kref_get(&kobj->kref);
    }
    return kobj;
}

static void kobject_release(struct kref *kref)
{
    struct kobject *kobj = (struct kobject *)((char *)kref - 
                         (unsigned long)(&((struct kobject *)0)->kref));
    
    printf("kobject_release: releasing '%s'\n", kobj->name);
    
    // 从列表中删除
    list_del(&kobj->list);
    
    // 调用用户提供的release函数
    if (kobj->release) {
        kobj->release(kobj);
    }
}

void kobject_put(struct kobject *kobj)
{
    if (kobj) {
        printf("kobject_put: putting '%s'\n", kobj->name);
        kref_put(&kobj->kref, kobject_release);
    }
}

// 示例设备结构
struct my_device {
    char description[64];
    int device_id;
    struct kobject kobj;
};

void device_release(struct kobject *kobj)
{
    struct my_device *dev = list_entry(kobj, struct my_device, kobj);
    
    printf("device_release: freeing device '%s' (id=%d)\n", 
           dev->description, dev->device_id);
    
    // 实际释放设备内存
    free(dev);
}

// 创建设备函数
struct my_device *create_device(const char *desc, int id)
{
    struct my_device *dev = malloc(sizeof(struct my_device));
    if (!dev) {
        return NULL;
    }
    
    strncpy(dev->description, desc, sizeof(dev->description) - 1);
    dev->description[sizeof(dev->description) - 1] = '\0';
    dev->device_id = id;
    
    // 初始化内嵌的 kobject
    kobject_init(&dev->kobj, desc);
    dev->kobj.release = device_release;
    
    return dev;
}

// 显示当前所有对象
void show_all_objects(void)
{
    struct list_head *pos;
    struct kobject *kobj;
    
    printf("\n=== Current Objects ===\n");
    list_for_each(pos, &object_list) {
        kobj = list_entry(pos, struct kobject, list);
        printf("Object: %s (refcount=%d)\n", kobj->name, kobj->kref.refcount);
    }
    printf("=======================\n\n");
}

// 测试函数
void test_kobject_lifecycle(void)
{
    printf("=== 开始 kobject 生命周期测试 ===\n\n");
    
    // 创建设备对象
    struct my_device *dev1 = create_device("Network Card", 1);
    struct my_device *dev2 = create_device("USB Controller", 2);
    
    show_all_objects();
    
    // 模拟多个模块使用设备
    printf("=== 模拟模块A使用网络卡 ===\n");
    struct kobject *dev1_ref1 = kobject_get(&dev1->kobj);
    
    printf("=== 模拟模块B使用网络卡 ===\n");
    struct kobject *dev1_ref2 = kobject_get(&dev1->kobj);
    
    show_all_objects();
    
    // 模块B停止使用
    printf("=== 模块B停止使用网络卡 ===\n");
    kobject_put(dev1_ref2);
    
    show_all_objects();
    
    // 尝试释放 USB 控制器（应该不会真的释放，因为还有引用）
    printf("=== 尝试释放USB控制器 ===\n");
    kobject_put(&dev2->kobj);
    
    show_all_objects();
    
    // 模块A停止使用网络卡
    printf("=== 模块A停止使用网络卡 ===\n");
    kobject_put(dev1_ref1);
    
    show_all_objects();
    
    // 最终释放网络卡
    printf("=== 最终释放网络卡 ===\n");
    kobject_put(&dev1->kobj);
    
    show_all_objects();
    
    // 最终释放USB控制器
    printf("=== 最终释放USB控制器 ===\n");
    kobject_put(&dev2->kobj);
    
    show_all_objects();
}

int main(void)
{
    // 初始化全局链表
    list_head_init();
    
    // 运行测试
    test_kobject_lifecycle();
    
    printf("=== 测试完成 ===\n");
    return 0;
}
```

## C语言可执行程序的数据段大小是如何确定的
- c program's memory layout
```
+-------------------------+
| Stack (grows downward)  |
+-------------------------+
| Heap (grows upward)     |
+-------------------------+
| BSS Segment             | ← uninitialized globals/static vars
+-------------------------+
| Data Segment            | ← initialized globals/static vars
+-------------------------+
| Text Segment            | ← compiled code (functions)
+-------------------------+
```
- 编译和链接共同决定了数据段的大小
```
| 段名        | 作用                                      | 举例变量                           | 是否占用文件空间   |
| ---------   | ------------------------                 | ------------------------------     | ---------- |
| `.text`     | 程序指令（机器码）                         | 函数体                             | ✅          |
| `.data`     | **已初始化的全局/静态变量**                 | `int a = 5; static int b = 6;`    | ✅          |
| `.bss`      | **未初始化（或初始化为0）的全局/静态变量**   | `int c; static int d;`             | ❌（只在运行时分配） |
| `.rodata`   | 只读数据（常量字符串、`const`变量）          | `"Hello"` / `const int x = 1;`    | ✅          |

编译阶段(.c -> .o)
	编译器（如 gcc）会为每个源文件生成一个目标文件 .o
	编译器根据变量的初始化情况，把符号放入不同的段：
		已初始化的 → .data
		未初始化或初始化为0的 → .bss
		字符串常量或 const → .rodata
	编译器会生成这些段的“符号表项”，记录变量大小、对齐要求
👉 每个 .o 文件自己有独立的 .data、.bss 段

汇编与对齐
	编译器会在 .o 中指定每个段的对齐要求（alignment）：
		通常 .data 段按 8 或 16 字节对齐
		如果结构体中有大字段（例如 double），会增加对齐填充

链接阶段（.o → ELF 可执行文件）
链接器（ld）会：
	1. 把所有 .data 段拼接起来
	2. 计算总对齐、偏移
	3. 记录最终的段大小到 ELF header
	4. .bss 段不会实际写入文件，而只记录大小（加载时由内核分配零填充内存）
因此最终 .data 段的大小 = 所有已初始化静态/全局变量的大小 + 对齐填充


加载阶段
当程序运行时：
	Linux ELF loader（动态链接器）会：
		从可执行文件中 映射 .data 段内容到内存；
		为 .bss 段 分配相应大小的零填充空间；
		在进程虚拟地址空间中分别创建这两个内存区域
可以通过 /proc/<pid>/maps 看到它们的位置（通常相邻）
```

```c
#include <stdio.h>

int global_var = 10;
int global_var_uninitialized;

int main(void)
{
    static int static_var = 20;
    static int static_var_uninitialized;
    printf("global_var: %d\n", global_var);
    printf("static_var: %d\n", static_var);
    printf("global_var_uninitialized: %d\n", global_var_uninitialized);
    printf("static_var_uninitialized: %d\n", static_var_uninitialized);
    return 0;
}

gcc -o main main.c && size main
   text    data     bss     dec     hex filename
   1553     608      16    2177     881 main
```

## opaque handle
[Opaque Pointers](https://blog.mbedded.ninja/programming/design-patterns/opaque-pointers/)  
[Incomplete Types](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Incomplete-Types.html)  
[Incomplete Types](https://beej.us/guide/bgc/html/split/incomplete-types.html)  
[Incomplete Array Types](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Incomplete-Array-Types.html)  
[]()  
[]()  
[]()  
[]()  

- incomplete type
  - an incomplete type is a type that's declared but but yet fully defined, the compiler knows that the type exists, but not how big it is or what it contains.
```c
// "incomplete type" acts like a promise:
	// I’ll tell you what this type looks like later — but for now, you can pass pointers around

// why it works even without struct definition in main.c
	// The compiler only needs the type name and pointer size (8 bytes on x86-64) to compile calls and variable declarations.
	// The definition of the struct is only required where you dereference or allocate it (malloc, w->id, etc.).
	// The linker just connects symbols — it doesn’t care about structs at all.

| Concept                       | Meaning                                              |
| ----------------------------- | ---------------------------------------------------- |
| **Incomplete type**           | Type is declared but not defined (size unknown)      |
| **Opaque pointer**            | Pointer to such a type used to hide implementation   |
| **Compile-time in user code** | Compiler only checks pointer usage & prototypes      |
| **Compile-time in impl code** | Compiler knows struct layout                         |
| **Link-time**                 | Functions matched by symbol names, not struct layout |
| **Runtime**                   | Only addresses are passed — type info gone           |


struct Foo;		// declaration only - incomplete type
// what compiler knows:
	// 1. there is a type called 'struct Foo'
	// 2. but its size, members and layout are unknown

// you can use pointers or references to it:
struct Foo *p;							// Ok - pointer to incomplete type
extern struct Foo *foo_create(void);	// Ok
// but cannot
struct Foo f;		// ❌ Error: incomplete type
p->member = 1;		// ❌ Error: struct not defined
sizeof(struct Foo); // ❌ Error: incomplete type

// when you define the structure, it becomes "complete"
struct Foo {
	int a;
}
// what compiler knows:
	// its size, fields
	// how to allocate and access it
struct Foo f;      // ✅ OK
f.a = 10;          // ✅ OK
```

- opaque pointer
  - an opaque pointer is a pointer to an incomplete type used intentionally to hide implementation details
```c
// An opaque pointer is a pointer to an incomplete type that remains incomplete in the public interface, allowing you to compile, link, and run code that manipulates data whose layout is known only to its implementation file.

// don't expose the struct definition in the header file
typedef struct Foo Foo;   // forward-declare and alias

// Foo* is an opaque pointer, users can hold it, pass it abound, but cannot look inside
Foo* foo_create(void);
void foo_destroy(Foo* f);

// "Every opaque pointer is based on an incomplete type, but not every incomplete type is opaque."
// Incomplete type = "compiler doesn’t know layout yet."

// Opaque pointer = "you intentionally keep it incomplete in the public interface."
| Concept         | Example                                             | Visible to user?                              | Used for                           |
| --------------- | --------------------------------------------------- | --------------------------------------------- | ---------------------------------- |
| Incomplete type | `struct A;`                                         | Temporarily incomplete, will be defined later | Internal modular code organization |
| Opaque pointer  | `typedef struct Foo Foo;` (no definition in header) | Intentionally hidden forever                  | Encapsulation / API design         |

```

```c
// .h
typedef struct Foo Foo;
extern Foo* get_Foo(int);

// opaque handle
	Declare public-facing function prototypes in a header (.h),
	Without exposing the internal structure of Foo,
	While still allowing client code to use Foo* handles.

// declares incomplete type + alias
typedef struct Foo Foo;
does the following 2 things:
1. forward declares a struct type named 'struct Foo', meaning:
	"there exists a struct type called struct Foo, but its contents are not defined yet"
2. creates an alias (typedef) called Foo for 'struct Foo'

what means for the compiler:
| Expression        | Meaning                               | Defined?                         |
| ----------------- | ------------------------------------- | -------------------------------- |
| `struct Foo`      | A tag name for a struct type          | ✅ Declared, ❌ Not defined yet |
| `Foo`             | A typedef alias for `struct Foo`      | ✅ Declared, ❌ Not defined yet |

// the compiler can use them for pointer declarations or function prototypes, even though it doesn't yet know what the construct contains.

// pointer to incomplete type
extern Foo* get_Foo(int);
// because the compiler already knows that Foo is a typedef name for an incomplete struct type, it can handle:
1. declaring a pointer to that type (Foo*)
2. declaring a function that returns that pointer

// c allows pointers to incomplete types (like forward-declared structs), because the size and layout of the struct aren't needed to declare or pass around a pointer - only when you dereference it does the full definition become necessary.

```

- example 1
  - code structure
```
Couter library:
	a public header (counter.h) - for users, hides implementation details
	a private implementation (counter.c) - defines the real struct

Users can create, use and destroy counter only through provided API functions, without ever seeing the struct layout.

.
├── counter.c
├── counter.h
└── main.c
```
  - code
```c
// counter.h
#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

/* Forward declaration + typedef alias */
typedef struct Counter Counter;

/* Public API */
Counter* counter_create(void);
void counter_destroy(Counter* c);
void counter_increment(Counter* c);
int counter_get_value(const Counter* c);

#endif

// counter.c
#include "counter.h"
#include <stdlib.h>
#include <stdio.h>

/* Define the struct — private to this file */
struct Counter {
    int value;
};

Counter* counter_create(void) {
    Counter* c = malloc(sizeof(*c));
    if (c) c->value = 0;
    return c;
}

void counter_destroy(Counter* c) {
    free(c);
}

void counter_increment(Counter* c) {
    if (c) c->value++;
}

int counter_get_value(const Counter* c) {
    return c ? c->value : 0;
}

// main.c
#include "counter.h"
#include <stdio.h>

int main(void) {
    Counter* c = counter_create();
    counter_increment(c);
    counter_increment(c);
    printf("Counter = %d\n", counter_get_value(c));
    counter_destroy(c);
    return 0;
}

```
- example 2 (handle-style API version)
```c
// counter.h
#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

/* Fully opaque handle type */
typedef void* CounterHandle;

/* Public API */
CounterHandle counter_create(void);
void counter_destroy(CounterHandle h);
void counter_increment(CounterHandle h);
int counter_get_value(CounterHandle h);

#endif

// counter.c
#include "counter.h"
#include <stdlib.h>
#include <stdio.h>

/* Private internal struct (invisible to users) */
typedef struct Counter {
    int value;
} Counter;

CounterHandle counter_create(void) {
    Counter* c = malloc(sizeof(*c));
    if (c) c->value = 0;
    return (CounterHandle)c;
}

void counter_destroy(CounterHandle h) {
    Counter* c = (Counter*)h;
    free(c);
}

void counter_increment(CounterHandle h) {
    Counter* c = (Counter*)h;
    if (c) c->value++;
}

int counter_get_value(CounterHandle h) {
    Counter* c = (Counter*)h;
    return c ? c->value : 0;
}

// main.c
#include "counter.h"
#include <stdio.h>

int main(void) {
    CounterHandle h = counter_create();
    counter_increment(h);
    counter_increment(h);
    printf("Counter = %d\n", counter_get_value(h));
    counter_destroy(h);
    return 0;
}

// Struct Pointer vs Handle
| Feature                  | `typedef struct Counter Counter;`            | `typedef void* CounterHandle;` |
| ------------------------ | -------------------------------------------  | ------------------------------ |
| Type safety              | ✅ Strong (compiler knows it’s a `Counter*`) | ⚠️ Weak (just a `void*`)       |
| ABI stability            | ⚠️ Needs care if struct layout changes       | ✅ Totally opaque               |
| Can dereference directly | ❌ No                                        | ❌ No                           |
| Common in                | Library APIs, C codebases                    | System APIs, drivers, SDKs     |
| Example                  | POSIX `DIR*`, OpenSSL `SSL*`                 | Win32 `HANDLE`, Vulkan handles |


```
- example 3 (hybrid approach — where we keep type safety but still hide the struct definition (like how FILE* in C stdio works)
  - code
```c
// counter.h
#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

/* Forward declare + typedef alias (type-safe opaque pointer) */
typedef struct Counter Counter;

/* Public API */
Counter* counter_create(void);
void counter_destroy(Counter* c);
void counter_increment(Counter* c);
int counter_get_value(const Counter* c);

#endif

// counter.c
#include "counter.h"
#include <stdlib.h>
#include <stdio.h>

/* Define the struct privately — hidden from users */
struct Counter {
    int value;
};

Counter* counter_create(void) {
    Counter* c = malloc(sizeof(*c));
    if (c) c->value = 0;
    return c;
}

void counter_destroy(Counter* c) {
    free(c);
}

void counter_increment(Counter* c) {
    if (c) c->value++;
}

int counter_get_value(const Counter* c) {
    return c ? c->value : 0;
}

// main.c
#include "counter.h"
#include <stdio.h>

int main(void) {
    Counter* c = counter_create();
    counter_increment(c);
    counter_increment(c);
    printf("Counter = %d\n", counter_get_value(c));
    counter_destroy(c);
    return 0;
}

// Comparison of all three styles
Pattern					Example typedef					Type safety		ABI stability			 Struct hidden?		Common use
Struct pointer (public) typedef struct Counter { 		✅ Strong	  ❌ Low (layout public)	  ❌ No	  			Internal-only APIs
						int value; } Counter;
Opaque handle (void)*	typedef void* CounterHandle;	❌ Weak		  ✅ Very high			  ✅ Fully			System-level APIs
Opaque typed pointer	typedef struct Counter Counter;	✅ Strong	  ✅ High				  ✅ Yes				Most libraries (stdio, OpenSSL, SQLite)
```
- example 4 (multiple opaque types coexist cleanly while keeping each subsystem encapsulated and type-safe)
  - code structure
```
What this achieves:
| Feature                      | Description                                                                              |
| ---------------------------- | ---------------------------------------------------------------------------------------- |
| **Encapsulation per module** | Each `.c` file owns and hides its own `struct`.                                          |
| **Type safety**              | You can’t pass a `Logger*` to a `Connection` function by accident — compiler catches it. |
| **Separation of concerns**   | Each module’s internal details are private.                                              |
| **Stable ABI**               | Struct layouts can change without recompiling user code.                                 |
| **Real-world pattern**       | Matches the style of `FILE*`, `DIR*`, `SSL*`, etc.                                       |

.
├── connection.c
├── connection.h
├── logger.c
├── logger.h
├── main.c
├── session.c
├── session.h
```
  - code
```c
// logger.h
#ifndef LOGGER_H
#define LOGGER_H

typedef struct Logger Logger;

Logger* logger_create(const char* path);
void logger_log(Logger* l, const char* msg);
void logger_destroy(Logger* l);

#endif

// connection.h
#ifndef CONNECTION_H
#define CONNECTION_H

typedef struct Connection Connection;

Connection* connection_open(const char* addr, int port);
void connection_send(Connection* c, const char* data);
void connection_close(Connection* c);

#endif

// session.h
#ifndef SESSION_H
#define SESSION_H

#include "connection.h"
#include "logger.h"

typedef struct Session Session;

Session* session_create(const char* addr, int port, const char* log_path);
void session_send(Session* s, const char* msg);
void session_destroy(Session* s);

#endif

// logger.c
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

struct Logger {
    FILE* fp;
};

Logger* logger_create(const char* path) {
    Logger* l = malloc(sizeof(*l));
    if (!l) return NULL;
    l->fp = fopen(path, "a");
    return l;
}

void logger_log(Logger* l, const char* msg) {
    if (l && l->fp) fprintf(l->fp, "[log] %s\n", msg);
}

void logger_destroy(Logger* l) {
    if (!l) return;
    if (l->fp) fclose(l->fp);
    free(l);
}

// connection.c
#include "connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Connection {
    char addr[64];
    int port;
};

Connection* connection_open(const char* addr, int port) {
    Connection* c = malloc(sizeof(*c));
    if (!c) return NULL;
    snprintf(c->addr, sizeof(c->addr), "%s", addr);
    c->port = port;
    printf("Connecting to %s:%d...\n", addr, port);
    return c;
}

void connection_send(Connection* c, const char* data) {
    printf("[Conn %s:%d] sending: %s\n", c->addr, c->port, data);
}

void connection_close(Connection* c) {
    printf("[Conn %s:%d] closed\n", c->addr, c->port);
    free(c);
}

// session.c
#include "session.h"
#include <stdlib.h>

struct Session {
    Connection* conn;
    Logger* log;
};

Session* session_create(const char* addr, int port, const char* log_path) {
    Session* s = malloc(sizeof(*s));
    if (!s) return NULL;
    s->conn = connection_open(addr, port);
    s->log  = logger_create(log_path);
    return s;
}

void session_send(Session* s, const char* msg) {
    if (!s) return;
    connection_send(s->conn, msg);
    logger_log(s->log, msg);
}

void session_destroy(Session* s) {
    if (!s) return;
    connection_close(s->conn);
    logger_destroy(s->log);
    free(s);
}

// main.c
#include "session.h"
#include <stdio.h>

int main(void) {
    Session* s = session_create("127.0.0.1", 8080, "session.log");
    session_send(s, "Hello, world!");
    session_send(s, "Second message!");
    session_destroy(s);
    return 0;
}

```

## macro + struct + function pointer
- 基本思想
  -  macros reduce repetition
  -  structs bundle data
  -  function pointers enable runtime polymorphism

- 典型应用
  - 面向对象模拟
  - 状态机实现
  - 回调机制与事件系统
  - 泛型容器实现
  - 策略模式
  - 插件系统
  - plug-in registration
  - state machine
  - state machine

- 面向对象模拟
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 基类定义
typedef struct Animal {
	char name[50];
	int age;
	void (*speak)(struct Animal* self);
	void (*destroy)(struct Animal* self);
} Animal;

// 宏定义简化对象创建
#define ANIMAL_INIT(n, a) {.name = n, .age = a}

// 方法实现
void animal_speak(Animal* self) {
	printf("%s (age %d) makes a sound\n", self->name, self->age);
}

void animal_destroy(Animal* self) {
	printf("Destroying animal: %s\n", self->name);
	free(self);
}

// 派生类 - Dog
typedef struct {
	Animal base;
	char breed[50];
} Dog;

void dog_speak(Animal* self) {
	Dog* dog = (Dog*)self;
	printf("%s (a %s, age %d) says: Woof!\n", 
		   dog->base.name, dog->breed, dog->base.age);
}

Dog* dog_create(const char* name, int age, const char* breed) {
	Dog* dog = malloc(sizeof(Dog));
	strcpy(dog->base.name, name);
	dog->base.age = age;
	strcpy(dog->breed, breed);
	dog->base.speak = dog_speak;
	dog->base.destroy = (void (*)(Animal*))free;
	return dog;
}

// 派生类 - Cat
typedef struct {
	Animal base;
	int lives;
} Cat;

void cat_speak(Animal* self) {
	Cat* cat = (Cat*)self;
	printf("%s (lives: %d, age %d) says: Meow!\n", 
		   cat->base.name, cat->lives, cat->base.age);
}

Cat* cat_create(const char* name, int age, int lives) {
	Cat* cat = malloc(sizeof(Cat));
	strcpy(cat->base.name, name);
	cat->base.age = age;
	cat->lives = lives;
	cat->base.speak = cat_speak;
	cat->base.destroy = (void (*)(Animal*))free;
	return cat;
}

int main() {
	// 多态演示
	Animal* animals[3];
		
	animals[0] = (Animal*)dog_create("Buddy", 3, "Golden Retriever");
	animals[1] = (Animal*)cat_create("Whiskers", 2, 9);
	animals[2] = (Animal*)dog_create("Max", 5, "Labrador");
		
	printf("=== Animal Sounds ===\n");
	for (int i = 0; i < 3; i++) {
		animals[i]->speak(animals[i]);
	}
		
	// 清理
	for (int i = 0; i < 3; i++) {
		animals[i]->destroy(animals[i]);
	}
		
	return 0;
}
```
- 状态机实现
```c
#include <stdio.h>
#include <stdlib.h>

// 状态和事件定义
typedef enum {
	STATE_IDLE,
	STATE_RUNNING,
	STATE_PAUSED,
	STATE_STOPPED
} State;

typedef enum {
	EVENT_START,
	EVENT_PAUSE,
	EVENT_RESUME,
	EVENT_STOP
} Event;

// 状态机结构
typedef struct StateMachine {
	State current_state;
	void (*transition)(struct StateMachine*, Event);
	void (*on_enter[4])(void);  // 进入状态回调
	void (*on_exit[4])(void);   // 离开状态回调
} StateMachine;

// 状态进入/退出回调
void on_idle_enter() { printf("Entering Idle state\n"); }
void on_running_enter() { printf("Entering Running state\n"); }
void on_paused_enter() { printf("Entering Paused state\n"); }
void on_stopped_enter() { printf("Entering Stopped state\n"); }

void on_idle_exit() { printf("Leaving Idle state\n"); }
void on_running_exit() { printf("Leaving Running state\n"); }
void on_paused_exit() { printf("Leaving Paused state\n"); }
void on_stopped_exit() { printf("Leaving Stopped state\n"); }

// 状态转换函数
void state_transition(StateMachine* sm, Event event) {
	State old_state = sm->current_state;
	State new_state = old_state;
		
	// 状态转换逻辑
	switch (old_state) {
		case STATE_IDLE:
			if (event == EVENT_START) new_state = STATE_RUNNING;
			break;
		case STATE_RUNNING:
			if (event == EVENT_PAUSE) new_state = STATE_PAUSED;
			else if (event == EVENT_STOP) new_state = STATE_STOPPED;
			break;
		case STATE_PAUSED:
			if (event == EVENT_RESUME) new_state = STATE_RUNNING;
			else if (event == EVENT_STOP) new_state = STATE_STOPPED;
			break;
		case STATE_STOPPED:
			if (event == EVENT_START) new_state = STATE_RUNNING;
			break;
	}
		
	// 执行状态转换
	if (old_state != new_state) {
		if (sm->on_exit[old_state]) sm->on_exit[old_state]();
		sm->current_state = new_state;
		if (sm->on_enter[new_state]) sm->on_enter[new_state]();
	}
}

// 创建状态机的宏
#define CREATE_STATE_MACHINE() &(StateMachine){ \
	.current_state = STATE_IDLE, \
	.transition = state_transition, \
	.on_enter = {on_idle_enter, on_running_enter, on_paused_enter, on_stopped_enter}, \
	.on_exit = {on_idle_exit, on_running_exit, on_paused_exit, on_stopped_exit} \
}

int main() {
	StateMachine* sm = CREATE_STATE_MACHINE();
		
	printf("=== State Machine Demo ===\n");
		
	// 测试状态转换
	sm->transition(sm, EVENT_START);   // IDLE -> RUNNING
	sm->transition(sm, EVENT_PAUSE);	// RUNNING -> PAUSED
	sm->transition(sm, EVENT_RESUME);   // PAUSED -> RUNNING
	sm->transition(sm, EVENT_STOP);	 // RUNNING -> STOPPED
	sm->transition(sm, EVENT_START);	// STOPPED -> RUNNING
		
	//free(sm);
	return 0;
}
```
- 回调机制与事件系统
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 事件类型定义
typedef enum {
	EVENT_BUTTON_PRESS,
	EVENT_TIMER_EXPIRED,
	EVENT_DATA_RECEIVED
} EventType;

// 事件结构
typedef struct {
	EventType type;
	void* data;
	size_t data_size;
} Event;

// 事件回调函数类型
typedef void (*EventHandler)(Event* event);

// 事件管理器
typedef struct {
	EventHandler* handlers;
	int capacity;
	int count;
} EventManager;

// 创建事件管理器
EventManager* event_manager_create(int capacity) {
	EventManager* em = malloc(sizeof(EventManager));
	em->handlers = malloc(sizeof(EventHandler) * capacity);
	em->capacity = capacity;
	em->count = 0;
	return em;
}

// 注册事件处理器
#define EVENT_REGISTER(em, handler) \
	do { \
		if ((em)->count < (em)->capacity) { \
			(em)->handlers[(em)->count++] = handler; \
		} \
	} while(0)

// 触发事件
void event_trigger(EventManager* em, Event* event) {
	printf("Triggering event %d\n", event->type);
	for (int i = 0; i < em->count; i++) {
		em->handlers[i](event);
	}
}

// 具体的事件处理器
void button_press_handler(Event* event) {
	if (event->type == EVENT_BUTTON_PRESS) {
		char* button = (char*)event->data;
		printf("Button pressed: %s\n", button);
	}
}

void timer_handler(Event* event) {
	if (event->type == EVENT_TIMER_EXPIRED) {
		int* timer_id = (int*)event->data;
		printf("Timer %d expired\n", *timer_id);
	}
}

void data_handler(Event* event) {
	if (event->type == EVENT_DATA_RECEIVED) {
		char* data = (char*)event->data;
		printf("Data received: %s\n", data);
	}
}

// 日志中间件
void logging_middleware(Event* event) {
	printf("[LOG] Event %d processed\n", event->type);
}

int main() {
	EventManager* em = event_manager_create(10);

	// 注册事件处理器
	EVENT_REGISTER(em, button_press_handler);
	EVENT_REGISTER(em, timer_handler);
	EVENT_REGISTER(em, data_handler);
	EVENT_REGISTER(em, logging_middleware);  // 日志中间件

	printf("=== Event System Demo ===\n");

	// 创建并触发事件
	Event e1 = {EVENT_BUTTON_PRESS, "OK", 0};
	event_trigger(em, &e1);

	int timer_id = 123;
	Event e2 = {EVENT_TIMER_EXPIRED, &timer_id, sizeof(int)};
	event_trigger(em, &e2);

	Event e3 = {EVENT_DATA_RECEIVED, "Hello World", 0};
	event_trigger(em, &e3);

	free(em->handlers);
	free(em);
	return 0;
}
```
- 泛型容器实现
```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 通用链表节点
typedef struct ListNode {
	void* data;
	struct ListNode* next;
	void (*print_func)(void* data);
	int (*compare_func)(void* a, void* b);
	void (*free_func)(void* data);
} ListNode;

// 链表结构
typedef struct {
	ListNode* head;
	int size;
} LinkedList;

// 类型特定的函数
void int_print(void* data) {
	printf("%d", *(int*)data);
}

void string_print(void* data) {
	printf("%s", (char*)data);
}

int int_compare(void* a, void* b) {
	return *(int*)a - *(int*)b;
}

int string_compare(void* a, void* b) {
	return strcmp((char*)a, (char*)b);
}

void basic_free(void* data) {
	free(data);
}

// 创建节点的辅助函数（在堆上分配，避免宏参数与成员名冲突）
static ListNode* create_node(
	void* payload,
	void (*print_fn)(void*),
	int (*compare_fn)(void*, void*),
	void (*free_fn)(void*)
) {
	ListNode* node = (ListNode*)malloc(sizeof(ListNode));
	if (!node) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	node->data = payload;
	node->print_func = print_fn;
	node->compare_func = compare_fn;
	node->free_func = free_fn;
	node->next = NULL;
	return node;
}

// 宏包装，保持调用简洁
#define CREATE_NODE(payload, print_fn, compare_fn, free_fn) \
	create_node((payload), (print_fn), (compare_fn), (free_fn))

// 链表操作
void list_append(LinkedList* list, ListNode* node) {
	if (!list->head) {
		list->head = node;
	} else {
		ListNode* current = list->head;
		while (current->next) {
			current = current->next;
		}
		current->next = node;
	}
	list->size++;
}

void list_print(LinkedList* list) {
	ListNode* current = list->head;
	printf("List[%d]: ", list->size);
	while (current) {
		current->print_func(current->data);
		printf(" -> ");
		current = current->next;
	}
	printf("NULL\n");
}

void list_free(LinkedList* list) {
	ListNode* current = list->head;
	while (current) {
		ListNode* next = current->next;
		if (current->free_func) {
			current->free_func(current->data);
		}
		free(current);
		current = next;
	}
}

int main() {
	LinkedList int_list = {0};
	LinkedList string_list = {0};

	printf("=== Generic Container Demo ===\n");

	// 整数链表
	int* num1 = malloc(sizeof(int)); *num1 = 10;
	int* num2 = malloc(sizeof(int)); *num2 = 20;
	int* num3 = malloc(sizeof(int)); *num3 = 30;

	list_append(&int_list, CREATE_NODE(num1, int_print, int_compare, basic_free));
	list_append(&int_list, CREATE_NODE(num2, int_print, int_compare, basic_free));
	list_append(&int_list, CREATE_NODE(num3, int_print, int_compare, basic_free));

	list_print(&int_list);

	// 字符串链表
	char* str1 = strdup("Hello");
	char* str2 = strdup("World");
	char* str3 = strdup("!");

	list_append(&string_list, CREATE_NODE(str1, string_print, string_compare, basic_free));
	list_append(&string_list, CREATE_NODE(str2, string_print, string_compare, basic_free));
	list_append(&string_list, CREATE_NODE(str3, string_print, string_compare, basic_free));

	list_print(&string_list);

	// 清理
	list_free(&int_list);
	list_free(&string_list);

	return 0;
}
```
- 策略模式
```c
#include <stdio.h>
#include <stdlib.h>

// 排序策略接口
typedef struct {
	char name[50];
	void (*sort)(int* array, int size);
	int (*compare)(int a, int b);  // 比较策略
} SortStrategy;

// 具体的排序算法
void bubble_sort(int* array, int size) {
	for (int i = 0; i < size - 1; i++) {
		for (int j = 0; j < size - i - 1; j++) {
			if (array[j] > array[j + 1]) {
				int temp = array[j];
				array[j] = array[j + 1];
				array[j + 1] = temp;
			}
		}
	}
}

void selection_sort(int* array, int size) {
	for (int i = 0; i < size - 1; i++) {
		int min_idx = i;
		for (int j = i + 1; j < size; j++) {
			if (array[j] < array[min_idx]) {
				min_idx = j;
			}
		}
		int temp = array[i];
		array[i] = array[min_idx];
		array[min_idx] = temp;
	}
}

// 比较策略
int ascending(int a, int b) { return a - b; }
int descending(int a, int b) { return b - a; }

// 策略实例
SortStrategy bubble_ascending = {"Bubble Sort (Ascending)", bubble_sort, ascending};
SortStrategy bubble_descending = {"Bubble Sort (Descending)", bubble_sort, descending};
SortStrategy selection_ascending = {"Selection Sort (Ascending)", selection_sort, ascending};

// 排序上下文
typedef struct {
	SortStrategy* strategy;
	int* data;
	int size;
} SortContext;

// 设置策略的宏
#define SET_STRATEGY(ctx, s) do { (ctx)->strategy = s; } while(0)

void sort_context_execute(SortContext* ctx) {
	printf("Using strategy: %s\n", ctx->strategy->name);

	// 使用策略的比较函数进行排序
	for (int i = 0; i < ctx->size - 1; i++) {
		for (int j = 0; j < ctx->size - i - 1; j++) {
			if (ctx->strategy->compare(ctx->data[j], ctx->data[j + 1]) > 0) {
				int temp = ctx->data[j];
				ctx->data[j] = ctx->data[j + 1];
				ctx->data[j + 1] = temp;
			}
		}
	}
}

void print_array(int* array, int size) {
	for (int i = 0; i < size; i++) {
		printf("%d ", array[i]);
	}
	printf("\n");
}

int main() {
	int data[] = {64, 34, 25, 12, 22, 11, 90};
	int size = sizeof(data) / sizeof(data[0]);

	SortContext ctx = {0};
	ctx.data = data;
	ctx.size = size;

	printf("=== Strategy Pattern Demo ===\n");
	printf("Original array: ");
	print_array(data, size);

	// 测试不同策略
	SET_STRATEGY(&ctx, &bubble_ascending);
	sort_context_execute(&ctx);
	printf("Sorted array: ");
	print_array(data, size);

	// 重置数据
	int data2[] = {64, 34, 25, 12, 22, 11, 90};
	ctx.data = data2;

	SET_STRATEGY(&ctx, &bubble_descending);
	sort_context_execute(&ctx);
	printf("Sorted array (descending): ");
	print_array(data2, size);

	return 0;
}
```
- 插件系统
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 插件接口
typedef struct {
	char name[50];
	int version;
	int (*initialize)(void);
	int (*process)(const char* input, char* output, int output_size);
	void (*shutdown)(void);
} Plugin;

// 插件管理器
typedef struct {
	Plugin* plugins[10];
	int count;
} PluginManager;

// 注册插件的宏
#define PLUGIN_REGISTER(pm, p) \
	do { \
		if ((pm)->count < 10) { \
			(pm)->plugins[(pm)->count++] = p; \
		} \
	} while(0)

// 具体插件实现
int logger_init(void) {
	printf("Logger plugin initialized\n");
	return 0;
}

int logger_process(const char* input, char* output, int output_size) {
	snprintf(output, output_size, "[LOG] %s", input);
	return 0;
}

void logger_shutdown(void) {
	printf("Logger plugin shutdown\n");
}

Plugin logger_plugin = {
	.name = "Logger",
	.version = 1,
	.initialize = logger_init,
	.process = logger_process,
	.shutdown = logger_shutdown
};

// 加密插件
int encryptor_init(void) {
	printf("Encryptor plugin initialized\n");
	return 0;
}

int encryptor_process(const char* input, char* output, int output_size) {
	// 简单的"加密" - 字符偏移
	int len = strlen(input);
	for (int i = 0; i < len && i < output_size - 1; i++) {
		output[i] = input[i] + 1;  // 每个字符+1
	}
	output[len < output_size - 1 ? len : output_size - 1] = '\0';
	return 0;
}

void encryptor_shutdown(void) {
	printf("Encryptor plugin shutdown\n");
}

Plugin encryptor_plugin = {
	.name = "Encryptor",
	.version = 1,
	.initialize = encryptor_init,
	.process = encryptor_process,
	.shutdown = encryptor_shutdown
};

void plugin_manager_init(PluginManager* pm) {
	pm->count = 0;
}

void plugin_manager_start(PluginManager* pm) {
	printf("=== Starting %d plugins ===\n", pm->count);
	for (int i = 0; i < pm->count; i++) {
		pm->plugins[i]->initialize();
	}
}

void plugin_manager_process(PluginManager* pm, const char* input) {
	char output[256];

	for (int i = 0; i < pm->count; i++) {
		pm->plugins[i]->process(input, output, sizeof(output));
		printf("Plugin '%s' output: %s\n", pm->plugins[i]->name, output);
	}
}

void plugin_manager_shutdown(PluginManager* pm) {
	printf("=== Shutting down %d plugins ===\n", pm->count);
	for (int i = 0; i < pm->count; i++) {
		pm->plugins[i]->shutdown();
	}
}

int main() {
	PluginManager pm;
	plugin_manager_init(&pm);

	// 注册插件
	PLUGIN_REGISTER(&pm, &logger_plugin);
	PLUGIN_REGISTER(&pm, &encryptor_plugin);

	// 使用插件系统
	plugin_manager_start(&pm);

	printf("\n=== Processing Data ===\n");
	plugin_manager_process(&pm, "Hello, Plugin System!");
	plugin_manager_process(&pm, "Test Message");

	plugin_manager_shutdown(&pm);

	return 0;
}
```

- plug-in registration
```c
#include <stdio.h>

typedef struct plugin
{
	const char *name;
	void (*init)(void);
	void (*run)(void);
} plugin;

#define PLUGIN_DECL(NAME, INIT_FN, RUN_FN) \
	plugin NAME = {#NAME, INIT_FN, RUN_FN}

static void alpha_init(void) { puts("alpha init"); }
static void alpha_run(void) { puts("alpha run"); }
PLUGIN_DECL(alpha_plugin, alpha_init, alpha_run);

static void beta_init(void) { puts("beta init"); }
static void beta_run(void) { puts("beta run"); }
PLUGIN_DECL(beta_plugin, beta_init, beta_run);

static plugin *registry[] = {&alpha_plugin, &beta_plugin};

int main(void)
{
	for (size_t i = 0; i < sizeof registry / sizeof registry[0]; ++i)
	{
		registry[i]->init();
		registry[i]->run();
	}
	return 0;
}
```
- state machine
```c
#include <stdio.h>

typedef struct state state;
struct state
{
	const char *name;
	void (*on_enter)(void);
	const state *(*next)(void);
};

#define STATE(NAME)                                               \
	static void NAME##_enter(void);                               \
	static const state *NAME##_next(void);                        \
	static const state NAME = {#NAME, NAME##_enter, NAME##_next}; \
	static void NAME##_enter(void)
#define STATE_NEXT(NAME) static const state *NAME##_next(void)

STATE(idle) { puts("enter idle"); }
STATE(running) { puts("enter running"); }

STATE_NEXT(idle) { return &running; }
STATE_NEXT(running) { return &idle; }

int main(void)
{
	const state *current = &idle;
	for (int i = 0; i < 4; ++i)
	{
		current->on_enter();
		current = current->next();
	}
	return 0;
}
```
- command dispather
```c
#include <stdio.h>
#include <string.h>

typedef void (*command_fn)(const char *);

typedef struct command
{
	const char *opcode;
	command_fn handler;
} command;

#define CMD(OPCODE, FN) {OPCODE, FN}

static void handle_status(const char *payload)
{
	printf("status: %s\n", payload);
}

static void handle_reset(const char *payload)
{
	printf("reset issued by %s\n", payload);
}

static command table[] = {
	CMD("STATUS", handle_status),
	CMD("RESET", handle_reset)};

static command_fn lookup(const char *opcode)
{
	for (size_t i = 0; i < sizeof table / sizeof table[0]; ++i)
	{
		if (strcmp(table[i].opcode, opcode) == 0)
		{
			return table[i].handler;
		}
	}
	return NULL;
}

int main(void)
{
	struct
	{
		const char *opcode;
		const char *payload;
	} script[] = {
		{"STATUS", "worker-42"},
		{"RESET", "admin"}
	};

	for (size_t i = 0; i < sizeof script / sizeof script[0]; ++i)
	{
		command_fn fn = lookup(script[i].opcode);
		if (fn)
			fn(script[i].payload);
		else
			printf("unknown opcode: %s\n", script[i].opcode);
	}
	return 0;
}
```

- Example: stats handler for several subsystems
- file list
```
.
├── stats_handler1.c
├── stats_handler1.h
├── stats_handler2.c
├── stats_handler2.h
├── stats_handler.c
└── stats_level.h
```
- stats_level.h
```c
#ifndef STATS_LEVEL_H
#define STATS_LEVEL_H

enum stats_level {
	STATS_LEVEL_INFO = 0,
	STATS_LEVEL_DEBUG = 1
};

#endif
```
- stats_handler1.h
```c
#ifndef STATS_HANDLER1_H
#define STATS_HANDLER1_H

#include "stats_level.h"

void stats_handler1_init(const char *app);
void stats_handler1_free(void);
void stats_handler1_update(void);
void stats_handler1_print(enum stats_level level);
const char *stats_handler1_data_provider_get(void);

#endif
```
- stats_handler2.h
```c
#ifndef STATS_HANDLER2_H
#define STATS_HANDLER2_H

#include "stats_level.h"

void stats_handler2_init(const char *app);
void stats_handler2_free(void);
void stats_handler2_update(void);
void stats_handler2_print(enum stats_level level);
const char *stats_handler2_data_provider_get(void);

#endif
```
- stats_handler.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "stats_level.h"
#include "stats_handler1.h"
#include "stats_handler2.h"


typedef void (*stats_handler_ops_init)(const char *app);
typedef void (*stats_handler_ops_free)(void);
typedef void (*stats_handler_ops_update)(void);
typedef void (*stats_handler_ops_print)(enum stats_level);
typedef const char* (*stats_handler_ops_data_provider_get)(void);

struct stats_handler_ops {
	stats_handler_ops_init init;
	stats_handler_ops_free free;
	stats_handler_ops_update update;
	stats_handler_ops_print print;
	stats_handler_ops_data_provider_get data_provider_get;
};

static int app_common_enabled_check(const char *name)
{
	if (strcmp(name, "stats_handler1") == 0 ||
		strcmp(name, "stats_handler2") == 0) {
		return 1;
	}

	return 0;
}

/* Dummy "app" just needs to be non-NULL to pass the macro's check */
static int dummy_app_instance = 1;
static void *app = &dummy_app_instance;

/* Macro modeled after your code */
#define IF_PROVIDER_REGISTERED(component, operation)     \
do {                                                     \
	if (app) {                                           \
		const char *o = component.data_provider_get();   \
		if (o) {                                         \
			char *owner = strdup(o);                     \
			uint8_t enabled = 0;                         \
			char *pch, *sp;                              \
			pch = strtok_r(owner, "|", &sp);             \
			while (pch != NULL) {                        \
				if (app_common_enabled_check(pch)) {     \
					enabled = 1;                         \
					break;                               \
				}                                        \
				pch = strtok_r(NULL, "|", &sp);          \
			}                                            \
			if (enabled) {                               \
				component.operation;                     \
			}                                            \
			free(owner);                                 \
		}                                                \
	}                                                    \
}                                                        \
while (0)

static struct stats_handler_ops stats_handler1_ops = {
		.init = stats_handler1_init,
		.free = stats_handler1_free,
		.update = stats_handler1_update,
		.print = stats_handler1_print,
		.data_provider_get = stats_handler1_data_provider_get
};

static struct stats_handler_ops stats_handler2_ops = {
		.init = stats_handler2_init,
		.free = stats_handler2_free,
		.update = stats_handler2_update,
		.print = stats_handler2_print,
		.data_provider_get = stats_handler2_data_provider_get
};

static int stats_enabled = 1;

static void stats_handler_all_init(const char *app)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, init(app));
		IF_PROVIDER_REGISTERED(stats_handler2_ops, init(app));
	}
}

static void stats_handler_all_free(void)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, free());
		IF_PROVIDER_REGISTERED(stats_handler2_ops, free());
	}
}

static void stats_handler_all_update(void)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, update());
		IF_PROVIDER_REGISTERED(stats_handler2_ops, update());
	}
}

static void stats_handler_all_print(enum stats_level level)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, print(level));
		IF_PROVIDER_REGISTERED(stats_handler2_ops, print(level));
	}
}

int main(void)
{
	stats_handler_all_init("app1");
	stats_handler_all_update();
	stats_handler_all_print(STATS_LEVEL_INFO);
	stats_handler_all_free();

	return 0;
}
```
- stats_handler1.c
```c
#include <stdio.h>
#include "stats_level.h"

void stats_handler1_init(const char *app)
{
	printf("stats_handler1_init called (app=%s)\n", app);
}

void stats_handler1_free(void)
{
	printf("stats_handler1_free called\n");
}

void stats_handler1_update(void)
{
	printf("stats_handler1_update called\n");
}

void stats_handler1_print(enum stats_level level)
{
	printf("stats_handler1_print called (level=%d)\n", level);
}

const char *stats_handler1_data_provider_get(void)
{
	return "stats_handler1";
}
```
- stats_handler2.c
```c
#include <stdio.h>
#include "stats_level.h"

void stats_handler2_init(const char *app)
{
	printf("stats_handler2_init called (app=%s)\n", app);
}

void stats_handler2_free(void)
{
	printf("stats_handler2_free called\n");
}

void stats_handler2_update(void)
{
	printf("stats_handler2_update called\n");
}

void stats_handler2_print(enum stats_level level)
{
	printf("stats_handler2_print called (level=%d)\n", level);
}

const char *stats_handler2_data_provider_get(void)
{
	return "stats_handler2";
}
```

## 简化版插件架构示例
- 文件结构
```
plugin
├── main.c            # 主程序
├── plugin.h           # 插件接口定义
├── plugin_a.c        # 插件A实现
├── plugin_b.c        # 插件B实现
└── Makefile          # 编译脚本
```
- plugin.h, plugin_a.c, plugin_b.c
```c
// plugin.h
#ifndef PLUGIN_H
#define PLUGIN_H

// 插件接口结构体
typedef struct
{
	const char *name;
	void (*init)(void);
	void (*execute)(void);
	void (*cleanup)(void);
} Plugin;

// 插件注册函数类型
typedef Plugin *(*RegisterPluginFunc)(void);

#endif // PLUGIN_H

// plugin_a.c
#include <stdio.h>
#include "plugin.h"

// 插件A的初始化函数
static void plugin_a_init(void)
{
	printf("Plugin A initialized\n");
}

// 插件A的执行函数
static void plugin_a_execute(void)
{
	printf("Plugin A executing...\n");
}

// 插件A的清理函数
static void plugin_a_cleanup(void)
{
	printf("Plugin A cleaned up\n");
}

// 插件A的接口实例
static Plugin plugin_a = {
	.name = "PluginA",
	.init = plugin_a_init,
	.execute = plugin_a_execute,
	.cleanup = plugin_a_cleanup};

// 插件注册函数
__attribute__((visibility("default")))
Plugin *
register_plugin(void)
{
	return &plugin_a;
}

// plugin_b.c
#include <stdio.h>
#include "plugin.h"

// 插件B的初始化函数
static void plugin_b_init(void)
{
	printf("Plugin B initialized\n");
}

// 插件B的执行函数
static void plugin_b_execute(void)
{
	printf("Plugin B executing...\n");
}

// 插件B的清理函数
static void plugin_b_cleanup(void)
{
	printf("Plugin B cleaned up\n");
}

// 插件B的接口实例
static Plugin plugin_b = {
	.name = "PluginB",
	.init = plugin_b_init,
	.execute = plugin_b_execute,
	.cleanup = plugin_b_cleanup};

// 插件注册函数
__attribute__((visibility("default")))
Plugin *
register_plugin(void)
{
	return &plugin_b;
}

// main.c
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include "plugin.h"

#define PLUGIN_DIR "./plugins"

int main()
{
	DIR *dir;
	struct dirent *entry;

	// 打开插件目录
	if ((dir = opendir(PLUGIN_DIR)) == NULL)
	{
		perror("opendir");
		return EXIT_FAILURE;
	}

	// 遍历插件目录
	while ((entry = readdir(dir)) != NULL)
	{
		// 只处理.so文件
		if (entry->d_type != DT_REG)
			continue;
		char *ext = strrchr(entry->d_name, '.');
		if (!ext || strcmp(ext, ".so") != 0)
			continue;

		// 构建完整路径
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", PLUGIN_DIR, entry->d_name);

		// 加载插件
		void *handle = dlopen(path, RTLD_LAZY);
		if (!handle)
		{
			fprintf(stderr, "Error loading %s: %s\n", path, dlerror());
			continue;
		}

		// 获取注册函数
		RegisterPluginFunc register_plugin = dlsym(handle, "register_plugin");
		if (!register_plugin)
		{
			fprintf(stderr, "Error finding register_plugin in %s: %s\n", path, dlerror());
			dlclose(handle);
			continue;
		}

		// 注册并获取插件接口
		Plugin *plugin = register_plugin();
		if (!plugin)
		{
			fprintf(stderr, "Plugin registration failed for %s\n", path);
			dlclose(handle);
			continue;
		}

		// 使用插件
		printf("\n=== Using plugin: %s ===\n", plugin->name);
		plugin->init();
		plugin->execute();
		plugin->cleanup();

		// 关闭插件
		dlclose(handle);
	}

	closedir(dir);
	return EXIT_SUCCESS;
}
```

- Makefile
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -fPIC
LDFLAGS = -ldl

# 主程序目标
TARGET = plugin_demo

# 插件目录
PLUGIN_DIR = plugins

# 插件目标
PLUGINS = $(PLUGIN_DIR)/plugin_a.so $(PLUGIN_DIR)/plugin_b.so

all: $(TARGET) $(PLUGINS)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(PLUGIN_DIR)/plugin_a.so: plugin_a.c plugin.h
	@mkdir -p $(PLUGIN_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $<

$(PLUGIN_DIR)/plugin_b.so: plugin_b.c plugin.h
	@mkdir -p $(PLUGIN_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(PLUGINS)
	rmdir $(PLUGIN_DIR) 2>/dev/null || true

.PHONY: all run clean

```

## string
[Library Functions](https://www.ibm.com/docs/en/i/7.4.0?topic=functions-library)  
[String and Array Utilities](https://www.gnu.org/software/libc/manual/html_node/String-and-Array-Utilities.html)  
[The String Span Functions](https://c-for-dummies.com/blog/?p=5068)  
[Parsing Words with the strspn() Function](https://c-for-dummies.com/blog/?p=5072)  
[Slicing Strings with strsep()](https://c-for-dummies.com/blog/?p=1769)  
[How to Use strtok and strtok_r in C](https://systems-encyclopedia.cs.illinois.edu/articles/c-strtok/)  
[strtok](https://icarus.cs.weber.edu/~dab/cs1410/textbook/8.Strings/more_cstring.html)  
[The Standard C Library for Linux, part 7: String Handling](https://linuxgazette.net/issue76/rogers.html)  
[C Programming/String manipulation](https://en.wikibooks.org/wiki/C_Programming/String_manipulation)  
[String Functions in C (Full List With Examples)](https://www.wscubetech.com/resources/c-programming/string-functions)  
[Standard C Library Functions Table, By Name](https://www.ibm.com/docs/en/i/7.5.0?topic=extensions-standard-c-library-functions-table-by-name)  
[]()  
[]()  

## socket
```c
//
#include <sys/types.h>
#include <sys/socket.h>

/*
 * domain: specifies an address family.
 * type: specifies communication semantics.
 * protocol: specifies a concrete protocol type
*/
int socket(int domain, int type, int protocol);

// domain
/*
| Macro                  | Description                     | Struct Used               | Typical Use                       |
| ---------------------- | ------------------------------- | ------------------------- | --------------------------------- |
| `AF_INET`              | IPv4 Internet protocols         | `struct sockaddr_in`      | IPv4 TCP/UDP communication        |
| `AF_INET6`             | IPv6 Internet protocols         | `struct sockaddr_in6`     | IPv6 TCP/UDP communication        |
| `AF_UNIX` / `AF_LOCAL` | Local communication (same host) | `struct sockaddr_un`      | UNIX domain sockets               |
| `AF_PACKET`            | Raw packet access (Layer 2)     | `struct sockaddr_ll`      | Network sniffers, raw Ethernet    |
| `AF_NETLINK`           | Kernel-user communication       | `struct sockaddr_nl`      | Routing, Netfilter, kernel events |
| `AF_BLUETOOTH`         | Bluetooth protocol              | `struct sockaddr_rc` etc. | Bluetooth communication           |

// type
| Macro            | Description                          | Semantics                          |
| ---------------- | ------------------------------------ | ---------------------------------- |
| `SOCK_STREAM`    | Stream-oriented (connection-based)   | Reliable byte stream (TCP)         |
| `SOCK_DGRAM`     | Datagram-oriented (connectionless)   | Unreliable message (UDP)           |
| `SOCK_RAW`       | Raw network protocol access          | Direct access to IP layer          |
| `SOCK_SEQPACKET` | Sequenced, reliable, record-oriented | Used with UNIX domain or Bluetooth |
| `SOCK_PACKET`    | (Deprecated) raw packets             | Legacy code only                   |

// protocol
| Domain      | Type          | Protocol           | Meaning                     |
| ----------- | ------------- | ------------------ | --------------------------- |
| `AF_INET`   | `SOCK_STREAM` | `IPPROTO_TCP`      | TCP                         |
| `AF_INET`   | `SOCK_DGRAM`  | `IPPROTO_UDP`      | UDP                         |
| `AF_INET`   | `SOCK_RAW`    | `IPPROTO_ICMP`     | Raw ICMP socket             |
| `AF_PACKET` | `SOCK_RAW`    | `htons(ETH_P_ALL)` | Capture all Ethernet frames |
| `AF_UNIX`   | `SOCK_STREAM` | `0`                | Local stream socket         |
*/

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
}



// ipv4
struct sockaddr_in {
    sa_family_t    sin_family; // Address family (AF_INET)
    in_port_t      sin_port;   // Port number (network byte order)
    struct in_addr sin_addr;   // IP address
    unsigned char  sin_zero[8]; // Padding
};

struct in_addr {
  uint32_t       s_addr;     /* address in network byte order */
};

// ipv6
struct sockaddr_in6 {
    sa_family_t     sin6_family;   // AF_INET6
    in_port_t       sin6_port;     // Port number
    uint32_t        sin6_flowinfo; // Flow information
    struct in6_addr sin6_addr;     // IPv6 address
    uint32_t        sin6_scope_id; // Scope ID
};

struct in6_addr {
  unsigned char   s6_addr[16];   /* IPv6 address */
};

// unix domain socket
struct sockaddr_un {
    sa_family_t sun_family;              // AF_UNIX
    char        sun_path[108];           // Pathname
};

// tcp
// server.c
// tcp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[1024];

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a port
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 3. Listen
    listen(server_fd, 5);
    printf("Server listening on port 8080...\n");

    // 4. Accept
    client_fd = accept(server_fd, NULL, NULL);
    printf("Client connected!\n");

    // 5. Communicate
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Received: %s\n", buffer);
    send(client_fd, "Hello Client!", 13, 0);

    close(client_fd);
    close(server_fd);
    return 0;
}

// client.c
// tcp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server;
    char buffer[1024];

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Set up server address
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    // 3. Connect
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // 4. Communicate
    send(sock, "Hello Server!", 13, 0);
    recv(sock, buffer, sizeof(buffer), 0);
    printf("Received: %s\n", buffer);

    close(sock);
    return 0;
}

// udp
// server.c
// udp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[1024];
    socklen_t addr_len = sizeof(client_addr);

    // 1. Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port 9000...\n");

    // 3. Receive datagram
    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&client_addr, &addr_len);
    buffer[n] = '\0';
    printf("Received from client: %s\n", buffer);

    // 4. Send response
    sendto(sockfd, "Hello from UDP server!", 23, 0,
           (struct sockaddr*)&client_addr, addr_len);

    close(sockfd);
    return 0;
}

// client.c
// udp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];
    socklen_t addr_len = sizeof(server_addr);

    // 1. Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Define server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. Send datagram
    sendto(sockfd, "Hello UDP Server!", 18, 0,
           (struct sockaddr*)&server_addr, addr_len);

    // 4. Receive response
    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&server_addr, &addr_len);
    buffer[n] = '\0';
    printf("Server reply: %s\n", buffer);

    close(sockfd);
    return 0;
}

// unix domain socket
// server.c
// unix_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/demo_socket"

int main() {
    int server_fd, client_fd;
    struct sockaddr_un addr;
    char buffer[100];

    unlink(SOCKET_PATH); // Remove old socket file

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 5);
    printf("UNIX server listening on %s...\n", SOCKET_PATH);

    client_fd = accept(server_fd, NULL, NULL);
    read(client_fd, buffer, sizeof(buffer));
    printf("Received: %s\n", buffer);
    write(client_fd, "Hi UNIX client!", 15);

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}

// client.c
// unix_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/demo_socket"

int main() {
    int sockfd;
    struct sockaddr_un addr;
    char buffer[100];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    write(sockfd, "Hello UNIX server!", 19);
    read(sockfd, buffer, sizeof(buffer));
    printf("Server says: %s\n", buffer);

    close(sockfd);
    return 0;
}

// raw socket (requires root privileges)
/*
 * Raw sockets allow user-space programs to:
 * Capture or send raw Ethernet frames,
 * Bypass the TCP/UDP/IP stack,
 * Inspect headers for tools like tcpdump, Wireshark, or custom packet sniffers.
*/
// packet_sniffer.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>	// For ETH_P_ALL
#include <net/if.h>			// For if_nametoindex()

int main() {
    int sockfd;
    struct sockaddr_ll sll;
    unsigned char buffer[2048];

    // 1. Create a raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a specific network interface (e.g., eth0)
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex("eth0"); // replace with your NIC name
    if (sll.sll_ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Listening on interface eth0 for raw Ethernet frames...\n");

    // 3. Receive packets
    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n < 0) {
            perror("recvfrom");
            break;
        }

        struct ethhdr *eth = (struct ethhdr *)buffer;

        printf("\n--- Ethernet Frame ---\n");
        printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_dest[0], eth->h_dest[1], eth->h_dest[2],
               eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
        printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_source[0], eth->h_source[1], eth->h_source[2],
               eth->h_source[3], eth->h_source[4], eth->h_source[5]);
        printf("EtherType: 0x%04x\n", ntohs(eth->h_proto));
        printf("Payload length: %zd bytes\n", n - sizeof(struct ethhdr));
    }

    close(sockfd);
    return 0;
}

// packet_sender.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>

int main() {
    int sockfd;
    struct sockaddr_ll sll;
    unsigned char frame[1500];

    // 1. Create raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Get interface index
    int ifindex = if_nametoindex("eth0"); // replace with your interface
    if (ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    // 3. Prepare destination
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex;
    sll.sll_halen = ETH_ALEN;
    sll.sll_addr[0] = 0xff; // Broadcast address
    sll.sll_addr[1] = 0xff;
    sll.sll_addr[2] = 0xff;
    sll.sll_addr[3] = 0xff;
    sll.sll_addr[4] = 0xff;
    sll.sll_addr[5] = 0xff;

    // 4. Build Ethernet frame
    struct ethhdr *eth = (struct ethhdr *)frame;
    memset(eth->h_dest, 0xff, ETH_ALEN);       // Destination: broadcast
    memset(eth->h_source, 0x11, ETH_ALEN);     // Fake source
    eth->h_proto = htons(0x88B5);              // Custom Ethertype
    strcpy((char *)(frame + sizeof(struct ethhdr)), "Hello Raw World!");

    // 5. Send
    ssize_t frame_len = sizeof(struct ethhdr) + strlen("Hello Raw World!");
    if (sendto(sockfd, frame, frame_len, 0, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("sendto");
    } else {
        printf("Sent raw Ethernet frame (%zd bytes)\n", frame_len);
    }

    close(sockfd);
    return 0;
}

// socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)
// icmp_ping.c
// headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

// ICMP Header (as defined in <netinet/ip_icmp.h>)
struct icmphdr {
    uint8_t  type;      // e.g., ICMP_ECHO
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

// ICMP checksum function
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// ping_raw.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: sudo %s <destination IP>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in dest;
    char packet[64];
    struct icmphdr *icmp = (struct icmphdr*)packet;

    // 1. Create raw socket for ICMP
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Setup destination address
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &dest.sin_addr);

    // 3. Build ICMP Echo Request
    memset(packet, 0, sizeof(packet));
    icmp->type = ICMP_ECHO;   // 8
    icmp->code = 0;
    icmp->un.echo.id = getpid() & 0xFFFF;
    icmp->un.echo.sequence = 1;
    icmp->checksum = 0;
    icmp->checksum = checksum(packet, sizeof(packet));

    // 4. Record time and send
    struct timeval start, end;
    gettimeofday(&start, NULL);

    if (sendto(sockfd, packet, sizeof(packet), 0,
               (struct sockaddr*)&dest, sizeof(dest)) <= 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    // 5. Wait for reply
    char recvbuf[1024];
    struct sockaddr_in reply_addr;
    socklen_t addr_len = sizeof(reply_addr);

    ssize_t n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0,
                         (struct sockaddr*)&reply_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&end, NULL);

    double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                 (end.tv_usec - start.tv_usec) / 1000.0;

    printf("Reply from %s: bytes=%zd time=%.3f ms\n",
           argv[1], n, rtt);

    close(sockfd);
    return 0;
}

// AF_UNIX (local IPC)
// server.c
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	int fd;
	struct sockaddr_un addr;
	const char *path = "/tmp/example.sock";
	char buf[256];
	ssize_t n;
	struct sockaddr_un peer;
	socklen_t peerlen = sizeof(peer);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) return 1;

	unlink(path); // ensure clean
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	n = recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&peer, &peerlen);
	if (n > 0) {
		buf[n] = '\0';
		printf("got: %s\n", buf);
	}
	close(fd);
	unlink(path);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	int fd;
	struct sockaddr_un dst;
	const char *path = "/tmp/example.sock";
	const char *msg = "hello";

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) return 1;

	memset(&dst, 0, sizeof(dst));
	dst.sun_family = AF_UNIX;
	strncpy(dst.sun_path, path, sizeof(dst.sun_path) - 1);

	sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&dst, sizeof(dst));
	close(fd);
	return 0;
}

// AF_INET (TCP)
// server.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr = {0};
	int c;
	if (s < 0) return 1;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(8080);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	if (listen(s, 16) < 0) return 1;

	c = accept(s, NULL, NULL);
	if (c >= 0) {
		const char *resp = "hi\n";
		send(c, resp, 3, 0);
		close(c);
	}
	close(s);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr = {0};
	if (s < 0) return 1;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	send(s, "hello", 5, 0);
	close(s);
	return 0;
}

// AF_INET6 (IPv6 TCP)
// server.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in6 addr = {0};
	int v6only = 1;

	if (s < 0) return 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

	addr.sin6_family = AF_INET6;
	addr.sin6_addr = in6addr_any;
	addr.sin6_port = htons(8080);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	if (listen(s, 16) < 0) return 1;

	int c = accept(s, NULL, NULL);
	if (c >= 0) close(c);
	close(s);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in6 addr = {0};
	if (s < 0) return 1;

	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(8080);
	inet_pton(AF_INET6, "::1", &addr.sin6_addr);

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	close(s);
	return 0;
}

// AF_NETLINK  (kernel-user communication)
// receiver.c
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	int s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	struct sockaddr_nl addr = {0};
	char buf[8192];
	struct nlmsghdr *nlh;

	if (s < 0) return 1;

	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();      // unique user-space PID
	addr.nl_groups = RTMGRP_LINK; // subscribe to link events

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	ssize_t n = recv(s, buf, sizeof(buf), 0);
	if (n > 0) {
		nlh = (struct nlmsghdr *)buf;
		printf("got netlink msg type=%d len=%d\n", nlh->nlmsg_type, nlh->nlmsg_len);
	}
	close(s);
	return 0;
}

// sender.c
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	struct sockaddr_nl dst = {0};
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg gen;
	} req = {0};

	if (s < 0) return 1;

	dst.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
	req.nlh.nlmsg_type = RTM_GETLINK;
	req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.gen.rtgen_family = AF_UNSPEC;

	struct iovec iov = { &req, req.nlh.nlmsg_len };
	struct msghdr msg = {0};
	msg.msg_name = &dst;
	msg.msg_namelen = sizeof(dst);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (sendmsg(s, &msg, 0) < 0) return 1;
	close(s);
	return 0;
}

// AF_PACKET (link layer, raw Ethernet; requires root)
// receiver.c
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(void) {
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	struct sockaddr_ll addr = {0};
	char buf[2048];
	ssize_t n;

	if (s < 0) return 1;

	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	addr.sll_ifindex = if_nametoindex("eth0");

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	n = recv(s, buf, sizeof(buf), 0);
	if (n > 0) printf("got %zd bytes\n", n);

	close(s);
	return 0;
}

// sender.c
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	struct sockaddr_ll dst = {0};
	unsigned char frame[64] = {0}; // fill with valid Ethernet frame

	if (s < 0) return 1;

	dst.sll_family = AF_PACKET;
	dst.sll_ifindex = if_nametoindex("eth0");
	dst.sll_halen = ETH_ALEN;
	// set dst.sll_addr[0..5] to target MAC

	sendto(s, frame, sizeof(frame), 0, (struct sockaddr *)&dst, sizeof(dst));
	close(s);
	return 0;
}

// function prototype for setsockopt
int setsockopt(int sockfd, int level, int optname,
               const void *optval, socklen_t optlen);

// SOL_SOCKET: socket level options
| Option                                |       Value type | Purpose / typical use                                                             |
| ------------------------------------- | ---------------: | --------------------------------------------------------------------------------- |
| `SO_REUSEADDR`                        |      `int` (0/1) | Allow binding to an address in `TIME_WAIT`. Common on servers to restart quickly. |
| `SO_REUSEPORT`                        |      `int` (0/1) | Allow multiple sockets bind same (addr,port) (load-sharing on some kernels).      |
| `SO_KEEPALIVE`                        |      `int` (0/1) | Enable TCP keepalive probes (basic on/off; details set via TCP_* options).        |
| `SO_BROADCAST`                        |      `int` (0/1) | Enable sending datagrams to broadcast addresses (UDP).                            |
| `SO_RCVBUF` / `SO_SNDBUF`             |    `int` (bytes) | Set kernel receive / send buffer sizes.                                           |
| `SO_RCVTIMEO` / `SO_SNDTIMEO`         | `struct timeval` | Set blocking I/O timeouts for `recv`/`send`.                                      |
| `SO_LINGER`                           |  `struct linger` | Control close behavior (block until sent or drop).                                |
| `SO_ERROR`                            |   `int` (output) | Query pending error on socket (use with `getsockopt`).                            |
| `SO_OOBINLINE`                        |      `int` (0/1) | Receive TCP OOB data inline with normal data.                                     |
| `SO_ACCEPTCONN`                       |   `int` (output) | Check if socket is listening (from `getsockopt`).                                 |
| `SO_DOMAIN`, `SO_TYPE`, `SO_PROTOCOL` |   `int` (output) | Query socket properties.                                                          |

// IPPROTO_TCP: TCP level options
| Option          |      Value type | Purpose                                                                         |
| --------------- | --------------: | ------------------------------------------------------------------------------- |
| `TCP_NODELAY`   |     `int` (0/1) | Disable Nagle (send small packets immediately). Useful for low-latency apps.    |
| `TCP_CORK`      |     `int` (0/1) | Linux: hold back partial frames until cork cleared (for packetization control). |
| `TCP_KEEPIDLE`  | `int` (seconds) | Idle time before first keepalive probe.                                         |
| `TCP_KEEPINTVL` | `int` (seconds) | Interval between keepalive probes.                                              |
| `TCP_KEEPCNT`   |           `int` | Number of probes before declaring connection dead.                              |
| `TCP_SYNCNT`    |           `int` | Number of SYN retransmits before aborting connect() (Linux).                    |
| `TCP_QUICKACK`  |           `int` | Control delayed ACK behavior (Linux).                                           |

// IPPROTO_IP / IP: IPv4 level options
| Option               | Value type / struct       | Purpose                                                  |
| -------------------- | ------------------------- | -------------------------------------------------------- |
| `IP_TTL`             | `int`                     | Set IP time-to-live for outgoing packets.                |
| `IP_MULTICAST_TTL`   | `unsigned char`           | TTL for IPv4 multicast packets.                          |
| `IP_MULTICAST_LOOP`  | `unsigned char`           | Loopback for multicast (0/1).                            |
| `IP_MULTICAST_IF`    | `struct in_addr` or `int` | Choose outgoing interface for multicast.                 |
| `IP_ADD_MEMBERSHIP`  | `struct ip_mreq`          | Join IPv4 multicast group.                               |
| `IP_DROP_MEMBERSHIP` | `struct ip_mreq`          | Leave multicast group.                                   |
| `IP_PKTINFO`         | `int`                     | Receive destination address and iface info in `recvmsg`. |
| `IP_HDRINCL`         | `int` (0/1)               | Include custom IP header when sending (raw sockets).     |

// IPPROTO_IPV6: IPv6 level options
| Option                |         Value type | Purpose                                                          |
| --------------------- | -----------------: | ---------------------------------------------------------------- |
| `IPV6_JOIN_GROUP`     | `struct ipv6_mreq` | Join IPv6 multicast group.                                       |
| `IPV6_LEAVE_GROUP`    | `struct ipv6_mreq` | Leave group.                                                     |
| `IPV6_MULTICAST_HOPS` |              `int` | Multicast hop limit (TTL).                                       |
| `IPV6_V6ONLY`         |              `int` | If set, socket will accept only IPv6 (no IPv4-mapped addresses). |
| `IPV6_PKTINFO`        |              `int` | Similar to `IP_PKTINFO` for IPv6.                                |

// SOL_SOCKET for advanced use
SO_ATTACH_FILTER / SO_DETACH_FILTER — attach BPF filter (raw capture) (Linux).
SO_PASSCRED — get sender credentials on Unix domain sockets.
SO_TIMESTAMP / SO_TIMESTAMPNS / SO_TIMESTAMPING — enable kernel packet timestamps.

// Typical application usage
TCP server that wants fast restart
	SO_REUSEADDR (and sometimes SO_REUSEPORT) before bind().
High-throughput network app
	Increase SO_RCVBUF / SO_SNDBUF and tune TCP window scaling.
Low-latency app (e.g., RPC, games)
	TCP_NODELAY (disable Nagle) + smaller send batching.
Long-lived idle connections (NAT/firewall keepalive)
	SO_KEEPALIVE + TCP_KEEPIDLE / TCP_KEEPINTVL / TCP_KEEPCNT.
UDP multicast receiver
	IP_ADD_MEMBERSHIP to join group, IP_MULTICAST_IF to pick interface.
Raw packet generation
	IP_HDRINCL if you want to provide the IP header.
Non-blocking I/O with timeout fallback
	SO_RCVTIMEO / SO_SNDTIMEO (or set non-blocking + select/poll/epoll).
Graceful close vs force close
	SO_LINGER with l_onoff controls whether close() blocks to send pending data.

```

## hash table
![00 index](./hash_table/00-index.md)  
![01 separate chaining](./hash_table/01-separate-chaining.md)  
![02 open addressing basic](./hash_table/02-open-addressing-basic.md)  
![03 open addressing advanced](./hash_table/03-open-addressing-advanced.md)  
![04 specialized methods](./hash_table/04-specialized-methods.md)  
![05 comparison and advice](./hash_table/05-comparison-and-advice.md)  
```c
/*
Hash Buckets
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  0  │  1  │  2  │  3  │ ... │ ... │ ... │ ... │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
   │     │     │     │           │     │     │
   │     │     │     │           │     │     └─► Node → Node → nil
   │     │     │     │           │     └─► nil
   │     │     │     │           └─► Node → nil
   │     │     │     └─► Node → Node → Node → nil
   │     │     └─► nil
   │     └─► Node → nil
   └─► Node → Node → nil
*/
/* -- 1 -- */
/*  hash.c  –  tiny educational hash table  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- tunables ---------- */
#define NUM_BUCKETS 16          /* must be power of two for fast mod */

/* ---------- node in linked list ---------- */
typedef struct Node {
    char *key;
    char *val;
    struct Node *next;
} Node;

/* ---------- the table ---------- */
typedef struct HashTable {
    Node *buckets[NUM_BUCKETS]; /* array of list heads */
} HashTable;

/* ---------- djb2 hash (Kernighan/Pike) ---------- */
static unsigned long hash(const char *str)
{
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;   /* h * 33 + c */
    return h;
}

/* ---------- create empty table ---------- */
HashTable *ht_create(void)
{
    HashTable *ht = malloc(sizeof *ht);
    for (int i = 0; i < NUM_BUCKETS; ++i)
        ht->buckets[i] = NULL;
    return ht;
}

/* ---------- insert or update ---------- */
void ht_insert(HashTable *ht, const char *key, const char *val)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1); /* fast modulo */
    Node *n = ht->buckets[idx];

    /* update if key already present */
    for (; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            free(n->val);
            n->val = strdup(val);
            return;
        }
    }

    /* create new node and push to front */
    n = malloc(sizeof *n);
    n->key   = strdup(key);
    n->val   = strdup(val);
    n->next  = ht->buckets[idx];
    ht->buckets[idx] = n;
}

/* ---------- search ---------- */
const char *ht_search(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    for (Node *n = ht->buckets[idx]; n; n = n->next)
        if (strcmp(n->key, key) == 0)
            return n->val;
    return NULL;                /* not found */
}

/* ---------- delete ---------- */
void ht_delete(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node **link = &ht->buckets[idx]; /* pointer to pointer trick */
    while (*link) {
        Node *n = *link;
        if (strcmp(n->key, key) == 0) {
            *link = n->next;    /* unlink */
            free(n->key);
            free(n->val);
            free(n);
            return;
        }
        link = &n->next;
    }
}

/* ---------- free everything ---------- */
void ht_destroy(HashTable *ht)
{
    for (int i = 0; i < NUM_BUCKETS; ++i) {
        Node *n = ht->buckets[i];
        while (n) {
            Node *next = n->next;
            free(n->key);
            free(n->val);
            free(n);
            n = next;
        }
    }
    free(ht);
}

/* ---------- demo ---------- */
int main(void)
{
    HashTable *ht = ht_create();

    ht_insert(ht, "apple", "red");
    ht_insert(ht, "banana", "yellow");
    ht_insert(ht, "lemon", "yellow");
    ht_insert(ht, "pear", "green");

    printf("banana → %s\n", ht_search(ht, "banana"));
    printf("pear   → %s\n", ht_search(ht, "pear"));
    printf("grape  → %s\n", ht_search(ht, "grape")); /* not found */

    ht_delete(ht, "banana");
    printf("banana after delete → %s\n", ht_search(ht, "banana"));

    ht_destroy(ht);
    return 0;
}

/* -- 2 -- */
/*  hash.c  –  tiny educational hash table  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- tunables ---------- */
#define NUM_BUCKETS 16          /* must be power of two for fast mod */

/* ---------- node in linked list ---------- */
typedef struct Node {
    char       *key;
    void       *value;
    struct Node *next;
} Node;

/* ---------- the table ---------- */
typedef struct {
    Node *buckets[NUM_BUCKETS];
    /* optional user-supplied helpers */
    void *(*value_copy)(const void *);   /* strdup for your type */
    void  (*value_free)(void *);         /* free for your type */
} HashTable;

/* ---------- djb2 hash (Kernighan/Pike) ---------- */
static unsigned long hash(const char *str)
{
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;   /* h * 33 + c */
    return h;
}

/* ---------- create empty table ---------- */
HashTable *ht_create(void *(*vc)(const void *), void (*vf)(void *))
{
    HashTable *ht = calloc(1, sizeof *ht);
    ht->value_copy = vc;
    ht->value_free = vf;
    return ht;
}

/* ---------- insert or update ---------- */
void ht_insert(HashTable *ht, const char *key, const void *value)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node *n;

    /* update existing key */
    for (n = ht->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (ht->value_free) ht->value_free(n->value);
            n->value = ht->value_copy ? ht->value_copy(value) : (void *)value;
            return;
        }
    }

    /* create new node */
    n = malloc(sizeof *n);
    n->key   = strdup(key);
    n->value = ht->value_copy ? ht->value_copy(value) : (void *)value;
    n->next  = ht->buckets[idx];
    ht->buckets[idx] = n;
}

/* ---------- search ---------- */
void *ht_search(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    for (Node *n = ht->buckets[idx]; n; n = n->next)
        if (strcmp(n->key, key) == 0) return n->value;
    return NULL;
}

/* ---------- delete ---------- */
void ht_delete(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node **link = &ht->buckets[idx];
    while (*link) {
        Node *n = *link;
        if (strcmp(n->key, key) == 0) {
            *link = n->next;
            free(n->key);
            if (ht->value_free) ht->value_free(n->value);
            free(n);
            return;
        }
        link = &n->next;
    }
}

/* ---------- free everything ---------- */
void ht_destroy(HashTable *ht)
{
    for (int i = 0; i < NUM_BUCKETS; ++i) {
        Node *n = ht->buckets[i];
        while (n) {
            Node *next = n->next;
            free(n->key);
            if (ht->value_free) ht->value_free(n->value);
            free(n);
            n = next;
        }
    }
    free(ht);
}

// 1
void *int_copy(const void *p) { return (void *)p; }

int main(void)
{
    HashTable *ht = ht_create(int_copy, NULL); /* no free needed */

    int k42 = 42, k7 = 7;
    ht_insert(ht, "forty-two", (void *)&k42);
    ht_insert(ht, "seven",     (void *)&k7);

    int v = *(int*)ht_search(ht, "forty-two");
    printf("forty-two = %d\n", v);   /* 42 */

    ht_destroy(ht);
}

// 2
typedef struct {
    double x, y;
} Point;

void *point_copy(const void *src)
{
    Point *p = malloc(sizeof *p);
    *p = *(Point *)src;
    return p;
}
void point_free(void *p) { free(p); }

int main(void)
{
    HashTable *ht = ht_create(point_copy, point_free);

    Point origin = {0.0, 0.0}, unit = {1.0, 1.0};
    ht_insert(ht, "origin", &origin);
    ht_insert(ht, "unit",   &unit);

    Point *u = ht_search(ht, "unit");
    printf("unit  = (%.1f, %.1f)\n", u->x, u->y);

    ht_destroy(ht);
}

// 3
int main(void)
{
    HashTable *ht = ht_create((void *(*)(const void *))strdup, free);

    ht_insert(ht, "apple", "red");
    ht_insert(ht, "banana", "yellow");
    ht_insert(ht, "lemon", "yellow");
    ht_insert(ht, "pear", "green");

    printf("banana → %s\n", (const char *)ht_search(ht, "banana"));
    printf("pear   → %s\n", (const char *)ht_search(ht, "pear"));
    printf("grape  → %s\n", (const char *)ht_search(ht, "grape")); /* not found */

    ht_delete(ht, "banana");
    printf("banana after delete → %s\n", (const char *)ht_search(ht, "banana"));

    ht_destroy(ht);
    return 0;
}

/* -- 3 -- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Node structure for key-value pairs
typedef struct Node {
    char* key;      // e.g., "Alice"
    int value;      // e.g., 30
    struct Node* next;  // For chaining collisions
} Node;

// HashTable structure
typedef struct HashTable {
    Node** buckets;  // Array of pointers to Node
    int size;        // Number of buckets
} HashTable;

// Simple hash function for strings: sum of ASCII % size
int hash(const char* key, int size) {
    int sum = 0;
    for (int i = 0; key[i] != '\0'; i++) {
        sum += key[i];
    }
    return sum % size;
}

// Create a new node
Node* createNode(const char* key, int value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->key = strdup(key);  // Copy string
    newNode->value = value;
    newNode->next = NULL;
    return newNode;
}

// Initialize hash table
HashTable* createHashTable(int size) {
    HashTable* ht = (HashTable*)malloc(sizeof(HashTable));
    ht->size = size;
    ht->buckets = (Node**)calloc(size, sizeof(Node*));  // Initialize to NULL
    return ht;
}

// Insert or update key-value
void insert(HashTable* ht, const char* key, int value) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];

    // Check if key exists (update value)
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            current->value = value;
            return;
        }
        current = current->next;
    }

    // Key doesn't exist: insert new node at front
    Node* newNode = createNode(key, value);
    newNode->next = ht->buckets[index];
    ht->buckets[index] = newNode;
}

// Search for key and return value (or -1 if not found)
int search(HashTable* ht, const char* key) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return -1;  // Not found
}

// Delete key
void delete(HashTable* ht, const char* key) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];
    Node* prev = NULL;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (prev == NULL) {
                ht->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current->key);  // Free duplicated string
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Print the entire table (for demo)
void printTable(HashTable* ht) {
    for (int i = 0; i < ht->size; i++) {
        printf("Bucket %d: ", i);
        Node* current = ht->buckets[i];
        while (current != NULL) {
            printf("(%s=%d) -> ", current->key, current->value);
            current = current->next;
        }
        printf("NULL\n");
    }
}

// Free the hash table (cleanup)
void freeHashTable(HashTable* ht) {
    for (int i = 0; i < ht->size; i++) {
        Node* current = ht->buckets[i];
        while (current != NULL) {
            Node* temp = current;
            current = current->next;
            free(temp->key);
            free(temp);
        }
    }
    free(ht->buckets);
    free(ht);
}

int main() {
    HashTable* ht = createHashTable(10);

    // Insert some data
    insert(ht, "Alice", 30);
    insert(ht, "Bob", 25);
    insert(ht, "Charlie", 35);  // Might collide with Alice depending on hash
    insert(ht, "David", 28);

    // Print table
    printf("Hash Table Contents:\n");
    printTable(ht);

    // Search example
    int age = search(ht, "Bob");
    printf("\nBob's age: %d\n", age);

    // Delete example
    delete(ht, "Alice");
    printf("\nAfter deleting Alice:\n");
    printTable(ht);

    // Cleanup
    freeHashTable(ht);
    return 0;
}
```

```lua
-- 1
-- Simple hash function for ints: key % size (handle negative)
local function hash(key, size)
    return ((key % size) + size) % size
end

-- Create a new node table
local function createNode(key, value)
    return {key = key, value = value, next = nil}
end

-- Initialize hash table
local function createHashTable(size)
    local ht = {size = size, buckets = {}}
    for i = 1, size do
        ht.buckets[i] = nil  -- Empty buckets
    end
    return ht
end

-- Insert or update key-value
local function insert(ht, key, value)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]

    -- Check if key exists (update value)
    while current ~= nil do
        if current.key == key then
            current.value = value
            return
        end
        current = current.next
    end

    -- Key doesn't exist: insert new node at front
    local newNode = createNode(key, value)
    newNode.next = ht.buckets[index]
    ht.buckets[index] = newNode
end

-- Search for key and return value (or -1 if not found)
local function search(ht, key)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]

    while current ~= nil do
        if current.key == key then
            return current.value
        end
        current = current.next
    end
    return -1  -- Not found
end

-- Delete key
local function delete(ht, key)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]
    local prev = nil

    while current ~= nil do
        if current.key == key then
            if prev == nil then
                ht.buckets[index] = current.next
            else
                prev.next = current.next
            end
            -- No explicit free; Lua GC handles it
            return
        end
        prev = current
        current = current.next
    end
end

-- Print the entire table (for demo)
local function printTable(ht)
    for i = 1, ht.size do
        io.write("Bucket " .. (i - 1) .. ": ")
        local current = ht.buckets[i]
        while current ~= nil do
            io.write("(" .. current.key .. "=" .. current.value .. ") -> ")
            current = current.next
        end
        print("nil")
    end
end

-- Main demo
local ht = createHashTable(10)

-- Insert some data (int keys: e.g., employee IDs)
insert(ht, 123, 30)   -- ID 123, age 30 (123 % 10 = 3)
insert(ht, 133, 25)   -- ID 133 hashes to 3
insert(ht, 143, 35)   -- ID 143 hashes to 3 (collision with 133!)
insert(ht, 100, 28)   -- ID 100 hashes to 0

-- Print table
print("Hash Table Contents:")
printTable(ht)

-- Search example
local age = search(ht, 133)
print("\nID 133's age: " .. age)

-- Delete example
delete(ht, 123)
print("\nAfter deleting ID 123:")
printTable(ht)

-- OOP style
-- HashTable prototype (methods)
local HashTable = {}
HashTable.__index = HashTable

-- Simple hash function for ints: key % size (handle negative)
local function hash(key, size)
    return ((key % size) + size) % size
end

-- Constructor: Create a new HashTable instance
function HashTable.new(size)
    local self = {
        size = size,
        buckets = {}
    }
    -- Initialize empty buckets
    for i = 1, size do
        self.buckets[i] = nil
    end
    -- Set metatable for method access
    setmetatable(self, HashTable)
    return self
end

-- Create a new node table
local function createNode(key, value)
    return {key = key, value = value, next = nil}
end

-- Insert or update key-value
function HashTable:insert(key, value)
    local index = hash(key, self.size)
    local current = self.buckets[index]

    -- Check if key exists (update value)
    while current ~= nil do
        if current.key == key then
            current.value = value
            return
        end
        current = current.next
    end

    -- Key doesn't exist: insert new node at front
    local newNode = createNode(key, value)
    newNode.next = self.buckets[index]
    self.buckets[index] = newNode
end

-- Search for key and return value (or -1 if not found)
function HashTable:search(key)
    local index = hash(key, self.size)
    local current = self.buckets[index]

    while current ~= nil do
        if current.key == key then
            return current.value
        end
        current = current.next
    end
    return -1  -- Not found
end

-- Delete key
function HashTable:delete(key)
    local index = hash(key, self.size)
    local current = self.buckets[index]
    local prev = nil

    while current ~= nil do
        if current.key == key then
            if prev == nil then
                self.buckets[index] = current.next
            else
                prev.next = current.next
            end
            -- No explicit free; Lua GC handles it
            return
        end
        prev = current
        current = current.next
    end
end

-- Print the entire table (for demo)
function HashTable:printTable()
    for i = 1, self.size do
        io.write("Bucket " .. (i - 1) .. ": ")
        local current = self.buckets[i]
        while current ~= nil do
            io.write("(" .. current.key .. "=" .. current.value .. ") -> ")
            current = current.next
        end
        print("nil")
    end
end

-- Main demo
local ht = HashTable.new(10)

-- Insert some data (int keys: e.g., employee IDs)
ht:insert(123, 30)   -- ID 123, age 30 (123 % 10 = 3)
ht:insert(133, 25)   -- ID 133 hashes to 3
ht:insert(143, 35)   -- ID 143 hashes to 3 (collision with 133!)
ht:insert(100, 28)   -- ID 100 hashes to 0

-- Print table
print("Hash Table Contents:")
ht:printTable()

-- Search example
local age = ht:search(133)
print("\nID 133's age: " .. age)

-- Delete example
ht:delete(123)
print("\nAfter deleting ID 123:")
ht:printTable()
```

## ring buffer
[Ring-Buffer](https://github.com/AndersKaloer/Ring-Buffer/tree/master)  
[c-ringbuf](https://github.com/dhess/c-ringbuf/tree/master)  
[]()  
[]()  
[]()  
[]()  
```c
# 1
#include <stdio.h>

#define BUFFER_SIZE 5

typedef struct {
    int data[BUFFER_SIZE];
    int count; // Number of elements in the buffer (0 to BUFFER_SIZE)
    int tail;  // Index where the next element will be inserted
} RingBuffer;

// Add a new integer to the ring buffer
void add(RingBuffer *buf, int value) {
    buf->data[buf->tail] = value;
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;

    if (buf->count < BUFFER_SIZE) {
        buf->count++;
    }
}

// Print the buffer contents from oldest to newest
void print_buffer(const RingBuffer *buf) {
    int start = (buf->tail - buf->count + BUFFER_SIZE) % BUFFER_SIZE;
    for (int i = 0; i < buf->count; i++) {
        int idx = (start + i) % BUFFER_SIZE;
        printf("%d ", buf->data[idx]);
    }
    printf("\n");
}

// Main: test the ring buffer
int main() {
    RingBuffer buf = { .count = 0, .tail = 0 };

    for (int i = 1; i <= 8; ++i) {
        add(&buf, i);
        printf("After adding %d: ", i);
        print_buffer(&buf);
    }

    return 0;
}

# 2
#include <stdio.h>

#define BUFFER_SIZE 5

typedef struct {
    int data[BUFFER_SIZE];
    int count; // Number of elements in the buffer (0 to BUFFER_SIZE)
    int head;  // Index of the oldest element
} RingBuffer;

// Add a new integer to the ring buffer
void add(RingBuffer *buf, int value) {
    int insert_index = (buf->head + buf->count) % BUFFER_SIZE;

    buf->data[insert_index] = value;

    if (buf->count < BUFFER_SIZE) {
        buf->count++;
    } else {
        // Buffer full, move head to next oldest
        buf->head = (buf->head + 1) % BUFFER_SIZE;
    }
}

// Print the buffer contents from oldest to newest
void print_buffer(const RingBuffer *buf) {
    for (int i = 0; i < buf->count; i++) {
        int idx = (buf->head + i) % BUFFER_SIZE;
        printf("%d ", buf->data[idx]);
    }
    printf("\n");
}

// Main: test the ring buffer
int main() {
    RingBuffer buf = { .count = 0, .head = 0 };

    for (int i = 1; i <= 8; ++i) {
        add(&buf, i);
        printf("After adding %d: ", i);
        print_buffer(&buf);
    }

    return 0;
}

# 3
#include<stdio.h>
#include<malloc.h>
#include<memory.h>
#include<unistd.h>
#include<stdlib.h>

typedef struct cbuff_{
    int * buff;
    int start;
    int end;
    int size;
    int count;
} cbuff_t;

cbuff_t* cbuff_new(int size)
{
  cbuff_t *cb = (cbuff_t*)malloc(sizeof(cbuff_t));
  memset(cb, 0, sizeof(cbuff_t));
  cb->size = size;
    cb->buff = (int*)malloc(sizeof(int)*size);
  
  return cb;
}

void cbuff_add(cbuff_t *cb, int elem)
{
  int end = cb->end;
  if(cb->count && (end % cb->size) == cb->start) {
    printf("Overflow Elem[%d] %d lost\n", cb->start, cb->buff[cb->start]);
    cb->start = (cb->start + 1 ) %cb->size;
    cb->count --;
  }

  printf("Added Elem[%d] = %d\n",cb->end, elem);
  cb->buff[cb->end] = elem;
  cb->end = (cb->end+1) % cb->size;
  cb->count ++;
}

int cbuff_remove(cbuff_t *cb)
{
  int start = cb->start ;
  int ret = -1;
  if(cb->count <= 0) {
    printf("Buffer is empty\n");
    return ret;
  }

  if(cb->count || (start % cb->size) != cb->end) {
    printf("Removed Elem[%d] = %d\n",cb->start, cb->buff[cb->start]);
    ret = cb->buff[cb->start];
    cb->start = (cb->start + 1 ) % cb->size;
    cb->count--;
  } else {
    printf("Buffer is empty\n");
  }

  return ret;
}

void cbuff_print(cbuff_t *cb)
{
  int start = cb->start ;
  int end = cb->end ;
  int i, count = 0;
  for(i = start; count < cb->count; i = (i + 1) % cb->size) {
    printf("Elem[%d] = %d\n", i, cb->buff[i]);
    count++;
    if(i == (end - 1)) {
      break;
    }
  }
}

void cbuff_delete(cbuff_t *cb)
{
  free(cb->buff);
  free(cb);
}

int main(int argc, char *argv[])
{
  char key;
  int elem;
  cbuff_t *cb = cbuff_new(5);

  while(1) {
    printf("circular buffer add[a], remove[r], print[p] : ");
    fflush(stdin);

    key = getchar();
    switch(key) {
    case 'a':
      printf("Element to add : ");
      scanf("%d", &elem);
      cbuff_add(cb, elem);
      break;
    case 'r':
      cbuff_remove(cb);
      break;
    case 'p':
      cbuff_print(cb);
      break;
    case 'q':
      cbuff_delete(cb);
      exit(0);
    }

    continue;
  }

  return 0;
}

# 4
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 5

typedef struct RingBuffer	RingBuffer;
struct RingBuffer
{
	int	count;	/* occupied size of data[]*/
	int	tail;	  /* index of last entry inserted + 1 */
	int data[BUFFER_SIZE];
};

void ringbuffer_add(RingBuffer *rb, int value)
{
    rb->data[rb->tail] = value;
    rb->tail = (rb->tail + 1) % BUFFER_SIZE;
    if (rb->count < BUFFER_SIZE) {
        rb->count++;
    }
}

void ringbuffer_print(RingBuffer *rb)
{
    if(rb->count == 0) {
        printf("RingBuffer is empty.\n");
        return;
    }
  
  int start = (rb->tail - rb->count + BUFFER_SIZE) % BUFFER_SIZE;
    for(int i = 0; i < rb->count; i++) {
        int idx = (start + i) % BUFFER_SIZE;
        printf("%d ", rb->data[idx]);
    }

    printf("\n");
}

int main(void)
{
    int i;
    RingBuffer rb;

    memset(&rb, 0, sizeof(rb));
    ringbuffer_print(&rb);

    for (i = 0; i < 10; i++) {
        ringbuffer_add(&rb, i);
        ringbuffer_print(&rb);
    }

    return 0;
}
```

## C library
[ctl](https://github.com/glouw/ctl)  
> C TEMPLATE LIBRARY (CTL) is a fast compiling, type safe, header only, template-like library for ISO C99/C11.
[C CONTAINER TEMPLATE LIBRARY (CTL)](https://rurban.github.io/ctl/)  
[]()  
[]()  
[]()  
[]()  

## pthread
[Multithreaded Programming (POSIX pthreads Tutorial)](https://randu.org/tutorials/threads/)  
[POSIX Threads API](https://www.cs.fsu.edu/~baker/opsys/notes/pthreads.html)  
[POSIX Threads API](https://www.cs.fsu.edu/~baker/realtime/restricted/notes/pthreads.html)  
[POSIX thread (pthread) libraries](https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html)  
[Pthreads Tutorial](https://www.eecs.umich.edu/courses/eecs570/discussions/w22/pthreads.html)  
[Parallel Programming with the PThread API](https://www.cs.emory.edu/~cheung/Courses/561/Syllabus/91-pthreads/create-threads.html)  
[Multithreaded Programming Guide](https://docs.oracle.com/cd/E19120-01/open.solaris/816-5137/index.html)  
[Multithreading for Beginners: Step-by-Step](https://hackernoon.com/multithreading-for-beginners-step-by-step)  
[Learning About Threads: An Essential Guide for Developers](https://hackernoon.com/learning-about-threads-an-essential-guide-for-developers)  
[What Every Gopher Should Know About the Go Memory Model](https://hackernoon.com/what-every-gopher-should-know-about-the-go-memory-model)  
[Threading Programming Guide](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/Introduction/Introduction.html)  
[MasteringPthreads](https://github.com/ANSANJAY/MasteringPthreads)  
[]()  
[]()  
[]()  


## 动态链接 和 静态链接
[**Program Library HOWTO**](https://tldp.org/HOWTO/Program-Library-HOWTO/index.html)  
[C++ dlopen mini HOWTO](https://tldp.org/HOWTO/C++-dlopen/)  
[Shared libraries with GCC on Linux](https://www.cprogramming.com/tutorial/shared-libraries-linux-gcc.html)  
[]()  
[]()  
[How dynamic linking for modular libraries works on Linux](https://opensource.com/article/22/5/dynamic-linking-modular-libraries-linux)  
[How to handle dynamic and static libraries in Linux](https://opensource.com/article/20/6/linux-libraries)  
```c
When we link an application against a shared library, the linker leaves some stubs (unresolved symbols) which need to be filled by dynamic linker at run time or at application loading time.

Loading of a shared library is of two types::
1. Dynamically linked libraries
2. Dynamically loaded libraries

// Dynamically linked libraries
gcc -c -Wall -Werror -fpic foo.c		// Compiling with Position Independent Code
gcc -shared -o libfoo.so foo.o			// Creating a shared library from an object file
gcc -L/home/username/foo -Wall -o test main.c -lfoo				// Linking with a shared library

// 1 use LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/home/username/foo:$LD_LIBRARY_PATH		// Making the library available at runtime
./test

// 2 use rpath
unset LD_LIBRARY_PATH
gcc -L/home/username/foo -Wl,-rpath=/home/username/foo -Wall -o test main.c -lfoo
./test

//  Dynamically loaded libraries
Useful for creating a "plug-in" architecture. 
The program takes full control by calling functions with the library. This is done using dlopen(), dlsym(), dlclose().


// 动态链接
// Locating a shared object during compilation
gcc -I ./include -c src/demo.c				// -I option: adds a directory to GCC's search path for header files

gcc -L`pwd`/lib -o myDemo demo.o -lexample	// -L option: adds an additional library path to GCC's search locations. 
											// -l: sets the name of the library you want to link against

ldd ./myDemo								// ldd: prints shared object dependencies
        linux-vdso.so.1 (0x00007ffe151df000)
        libexample.so => not found
        libc.so.6 => /lib64/libc.so.6 (0x00007f514b60a000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f514b839000)

LD_LIBRARY_PATH=`pwd`/lib ldd ./			// LD_LIBRARY_PATH: the environment variable which defines the path to libraries
   linux-vdso.so.1 (0x00007ffe515bb000)
   libexample.so => /tmp/Demo/lib/libexample.so (0x0000...
   libc.so.6 => /lib64/libc.so.6 (0x00007eff037ee000)
   /lib64/ld-linux-x86-64.so.2 (0x00007eff03a22000)

LD_LIBRARY_PATH=`pwd`/lib ./myDemo			// 

// When to use LD_LIBRARY_PATH
/*
	1. compiling software that needs to link against a library that itself has just been compiled and has not yet been installed
	2. bundling software that's designed to run out of a single directory, with no install script or an install script that places libraries in non-standard directories
*/

// 静态链接
A static library is an archive (ar) of object files

file libmy_static.a		// libmy_static.a: current ar archive

ar -t libmy_static.a 	// look into the archive

ar -x libmy_static.a	// extract the archive's files
```

## libraries
### ini parser
[iniparser](https://github.com/ndevilla/iniparser/tree/main)  
[inih](https://github.com/benhoyt/inih)  
[Notcurses: blingful TUIs and character graphics](https://github.com/dankamongmen/notcurses)  

## 函数指针的应用场景
```c
// 1 回调机制
#include <stdio.h>

typedef struct Button {
    void (*onClick)();  // 回调函数指针
} Button;

void buttonClicked() {
    printf("Button was clicked!\n");
}

int main() {
    Button btn;
    btn.onClick = buttonClicked;  // 注册回调函数
    if (btn.onClick) {
        btn.onClick();  // 调用回调函数
    }
    return 0;
}

// callback function
int compare(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

qsort(array, size, sizeof(int), compare);


// 2 策略模式
#include <stdio.h>

typedef struct Sorter {
    int (*compare)(int, int);  // 函数指针定义了比较策略
} Sorter;

int ascending(int a, int b) {	// 比较策略 1
    return a - b;
}

int descending(int a, int b) {	// 比较策略 2
    return b - a;
}

void sort(int arr[], int n, Sorter sorter) {	// 动态指定比较策略
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (sorter.compare(arr[j], arr[j + 1]) > 0) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int main() {
    int arr[] = {5, 3, 7, 2, 8};
    Sorter ascSorter = {ascending};
    Sorter descSorter = {descending};
    
    sort(arr, 5, ascSorter);	// 动态指定比较策略 1
    printf("Ascending: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");
    
    sort(arr, 5, descSorter);	// 动态指定比较策略 2
    printf("Descending: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");
    
    return 0;
}

// 3 插件系统
#include <stdio.h>

typedef struct Filter {
    void (*apply)(int*, int);  // 函数指针指向具体的处理函数
} Filter;

void grayscale(int* pixels, int size) {
    printf("Applying grayscale filter\n");
}

void sepia(int* pixels, int size) {
    printf("Applying sepia filter\n");
}

int main() {
    int pixels[100];
    Filter filters[] = {	// 数组存储多个Filter对象，每个对象关联一个Filter函数
        {grayscale},
        {sepia}
    };
    
    for (int i = 0; i < 2; i++) {		// 遍历数组并调用apply，实现插件式的Filter应用
        filters[i].apply(pixels, 100);
    }
    return 0;
}

// 4 状态机
#include <stdio.h>

typedef struct State {
    void (*handleInput)(char);  // 函数指针处理特定状态下的输入
} State;

void handleListen(char input) {
    if (input == 'S') printf("Received SYN, moving to SYN_RECEIVED\n");
}

void handleSynReceived(char input) {
    if (input == 'A') printf("Received ACK, moving to ESTABLISHED\n");
}

int main() {
    State states[] = {		  // 每个State（如LISTEN和SYN_RECEIVED）有独立的处理逻辑
        {handleListen},
        {handleSynReceived}
    };
    
    states[0].handleInput('S');  // 状态机在不同状态间的转换
    states[1].handleInput('A');  // 状态机在不同状态间的转换
    return 0;
}

//
#include <stdio.h>

typedef enum {
    STATE_RED,
    STATE_GREEN,
    STATE_YELLOW,
    STATE_COUNT
} TrafficLightState;

typedef TrafficLightState (*StateFunction)();

TrafficLightState red_state() {
    printf("State: RED\n");
    return STATE_GREEN;
}

TrafficLightState green_state() {
    printf("State: GREEN\n");
    return STATE_YELLOW;
}

TrafficLightState yellow_state() {
    printf("State: YELLOW\n");
    return STATE_RED;
}

int main() {
    StateFunction state_table[STATE_COUNT] = {
        red_state,
        green_state,
        yellow_state
    };

    TrafficLightState current_state = STATE_RED;

    for (int i = 0; i < 10; ++i) {
        current_state = state_table[current_state]();
    }

    return 0;
}


// 5 对象模拟
#include <stdio.h>

typedef struct Animal {
    void (*speak)();  // 函数指针模拟类的“方法”
} Animal;

void dogSpeak() {		// 不同的函数指针实现各自的行为
    printf("Woof!\n");
}

void catSpeak() {		// 不同的函数指针实现各自的行为
    printf("Meow!\n");
}

int main() {
    Animal dog = {dogSpeak};
    Animal cat = {catSpeak};
    
    dog.speak();
    cat.speak();
    return 0;
}

// 6 事件分发
#include <stdio.h>

typedef struct EventHandler {
    void (*handle)(int eventType);  // 函数指针用于事件处理
} EventHandler;

void handleEvent(int eventType) {	// 根据eventType执行不同逻辑
    if (eventType == 1) {
        printf("Handling event type 1\n");
    } else if (eventType == 2) {
        printf("Handling event type 2\n");
    }
}

int main() {
    EventHandler handler = {handleEvent};
    
    handler.handle(1);	// 分发事件，实现集中式事件处理
    handler.handle(2);	// 分发事件，实现集中式事件处理
    return 0;
}

// 7 线程池
#include <stdio.h>

typedef struct Task {			// 结构体包含execute函数指针和参数arg
    void (*execute)(void* arg);
    void* arg;
} Task;

void printNumber(void* arg) {	// 任务函数，接收参数并执行
    int num = *(int*)arg;
    printf("Number: %d\n", num);
}

int main() {
    int num = 42;
    Task task = {printNumber, &num};
    
    task.execute(task.arg);  // 执行线程池中的任务
    return 0;
}

// 8 虚拟函数表
#include <stdio.h>

typedef struct VTable {		// 虚函数表定义(包含函数指针)
    void (*speak)();
} VTable;

typedef struct Animal {		// 结构体通过函数表指针关联具体的实现
    VTable* vtable;
} Animal;

void dogSpeak() {
    printf("Woof!\n");
}

void catSpeak() {
    printf("Meow!\n");
}

VTable dogVTable = {dogSpeak};
VTable catVTable = {catSpeak};

int main() {
    Animal dog = {&dogVTable};	// 通过不同的虚函数表实现多态调用
    Animal cat = {&catVTable};	// 通过不同的虚函数表实现多态调用
    
    dog.vtable->speak();
    cat.vtable->speak();
    return 0;
}

// 9 钩子函数
#include <stdio.h>

typedef struct Monitor {				// 结构体中的函数指针注册钩子函数
    void (*onChange)(const char* file);
} Monitor;

void logChange(const char* file) {		// 钩子函数实现
    printf("File changed: %s\n", file);
}

int main() {
    Monitor monitor = {logChange};
    
    if (monitor.onChange) {
        monitor.onChange("example.txt");  // 触发钩子函数逻辑
    }
    return 0;
}

// 10 从动态库加载函数
#include <stdio.h>
#include <dlfcn.h>

typedef struct Plugin {
    void (*init)();
    void (*cleanup)();
} Plugin;

int main() {
    void* handle = dlopen("./libplugin.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    
    Plugin plugin;
    plugin.init = dlsym(handle, "plugin_init");			// dlsym获取函数地址
    plugin.cleanup = dlsym(handle, "plugin_cleanup");	// dlsym获取函数地址
    
    if (plugin.init)
		plugin.init();

    if (plugin.cleanup)
		plugin.cleanup();
    
    dlclose(handle);
    return 0;
}

// jump tables
void add() { printf("Add\n"); }
void subtract() { printf("Subtract\n"); }

void (*operation_table[2])() = { add, subtract };
operation_table[op_index]();  // Dynamically invoke the correct operation

// Plugin-like Architecture / Dynamic Behavior
typedef struct {
    void (*init)();
    void (*process)();
} Module;

void mod1_init() { printf("Mod1 Init\n"); }
void mod1_process() { printf("Mod1 Process\n"); }

Module mod1 = { mod1_init, mod1_process };
mod1.init();
mod1.process();

// Encapsulating Behavior in Data Structures
typedef struct {
    void (*draw)(void*);
} Shape;

void draw_circle(void* s) { printf("Drawing Circle\n"); }

Shape circle = { draw_circle };
circle.draw(&circle);

```

## 常用函数
```c
ioctl, fcntl
mmap, munmap
fopen, fread, fwrite, fclose, ferror
popen, pclose, fdopen
open, read, write, close
fprintf,  sprintf, snprintf
fscanf, scanf
strchr, strrchr, strstr, strtok, strtok_r
strdup
strlen
access
strncmp, strcasecmp
fgets
system
isalpha, isxdigit
atoi, strtoul, strtol
toupper, tolower
malloc, free
getenv
regcomp, regexec, regfree
fork, execve
waitpid
raise
memset, memmove, memcpy
kill
reboot
select, poll, epoll
socket, getsockopt, setsockopt
pthread_mutex_init, pthread_mutex_destroy
pthread_mutex_lock， pthread_mutex_unlock
pthread_mutexattr_init, pthread_mutexattr_setpshared
shm_open, shm_unlink
truncate, ftruncate
pthread_self, pthread_setaffinity_np, pthread_getaffinity_np
strerror

void *memcpy(void *dest, const void *src, size_t n);
The memcpy() function copies n bytes from memory area src to memory area dest. The memory areas must not overlap.  Use memmove(3) if the memory areas do overlap.

void *memmove(void *dest, const void *src, size_t n);
The  memmove()  function  copies n bytes from memory area src to memory area dest. The memory areas may overlap: copying takes place as though the bytes in src are first copied into a temporary array that does not overlap src or dest, and the bytes are then copied from the temporary array to dest.

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
	struct pollfd {
		int   fd;         /* file descriptor */
		short events;     /* requested events */
		short revents;    /* returned events */
	};
The caller should specify the number of items in the fds array in nfds.
The field fd contains a file descriptor for an open file.
The field events is an input parameter, a bit mask specifying the events the application is interested in for the file descriptor fd.
The field revents is an output parameter, filled by the kernel with the events that actually occurred. 

// strerror
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void)
{
		FILE *fp;
		char filename[128];

		snprintf(filename, sizeof(filename), "test.txt");
		fp = fopen(filename, "r");
		if(fp == NULL) {
						printf("fopen %s failed: %s\n", filename, strerror(errno));
						//printf("fopen %s failed: %m\n", filename);
		}

		return 0;
}

// waitpid
// return immediately if no child has exited
pid = waitpid(0, &status, WNOHANG);
if(pid <= 0)
	return;

// raise (发送信号给当前进程自己)
#include <signal.h>

void sig_handler(int signo, siginfo_t *info, void *ucontext)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	// inside first-level signal handler, set signal handler to default action
	act.sa_handler = SIG_DFL;

	sigaction(signo, &act, NULL);

	/* Take appropriate actions according to needs */
	......

	// send a signal to the caller, 在本例中会触发系统的默认的信号处理
	raise(signo);
}

int set_sighandler()
{
	int r;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sig_handler;
	sa.sa_flags = SA_SIGINFO;

	// assume signo is SIGUSR1
	r = sigaction(SIGUSR1, &sa, NULL);
	if(r < 0) {
		printf("sigaction failed: %s", strerror(errno));
		return -1;
	}
	/* Further code */

		return 0;
}

// statvfs (获取文件系统统计信息)
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>

#define SWM_TMP_SPACE 200

int
chk_tmp_space(void)
{
	int r;
	int free;
	struct statvfs vfs;

	memset(&vfs, 0, sizeof(vfs));
	r = statvfs("/", &vfs);
	if(r) {
		printf("statvfs failed: %s", strerror(errno));
		return -1;
	}

	free = (vfs.f_bsize * vfs.f_bfree) / (1024 * 1024);

	printf("tmp space required %dMB free %dMB\n",SWM_TMP_SPACE, free);

	if(free < SWM_TMP_SPACE) {
		printf("Not enough space, required %dMB, available %dMB\n", SWM_TMP_SPACE, free);
		return -1;
	}

	return 0;
}

// popen 解析shell命令执行结果，并在程序中进行使用
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

enum {
	/* lacp churn machine states */
	CHURN_MONITOR	= 0,	/* monitoring for churn */
	CHURNED		= 1,	/* churn detected (error) */
	NO_CHURN	= 2,	/* no churn (no error) */

	BLK_AGGR	= 0,	/* parsing aggregator info */
	BLK_S1		= 1,	/* parsing slave 1 (fm1-mac1.p1) info */
	BLK_S1_ACTOR	= 2,	/* parsing slave 1 actor lacp pdu */
	BLK_S1_PARTNER	= 3,	/* parsing slave 1 partner lacp pdu */
	BLK_S2		= 4,	/* parsing slave 2 (fm1-mac1.p1) info */
	BLK_S2_ACTOR	= 5,	/* parsing slave 2 actor lacp pdu */
	BLK_S2_PARTNER	= 6,	/* parsing slave 2 partner lacp pdu */
	};

typedef struct slave_status_t slave_status_t;
struct slave_status_t
{
	int	actor_system_prio;
	int	actor_port_key;
	int	actor_port_prio;
	int	actor_port_number;
	int	actor_port_state;
	int	actor_churn_state;
	int	actor_churn_count;
	int	partner_system_prio;
	int	partner_oper_key;
	int	partner_port_prio;
	int	partner_port_number;
	int	partner_port_state;
	int	partner_churn_state;
	int	partner_churn_count;
};

void print_slave_status(slave_status_t *slave)
{
	printf("actor_system_prio:%d\n", slave->actor_system_prio);
	printf("actor_port_key:%d\n", slave->actor_port_key);
	printf("actor_port_prio:%d\n", slave->actor_port_prio);
	printf("actor_port_number:%d\n", slave->actor_port_number);
	printf("actor_port_state:%d\n", slave->actor_port_state);
	printf("actor_churn_state:%d\n", slave->actor_churn_state);
	printf("actor_churn_count:%d\n", slave->actor_churn_count);
	printf("partner_system_prio:%d\n", slave->partner_system_prio);
	printf("partner_oper_key:%d\n", slave->partner_oper_key);
	printf("partner_port_prio:%d\n", slave->partner_port_prio);
	printf("partner_port_number:%d\n", slave->partner_port_number);
	printf("partner_port_state:%d\n", slave->partner_port_state);
	printf("partner_churn_state:%d\n", slave->partner_churn_state);
	printf("partner_churn_count:%d\n", slave->partner_churn_count);
}

int main(void)
{
	FILE *fp;
	int blk, val;
	const char delim[] = ":";
	slave_status_t	slaves[2], *s1, *s2;
	char cmd[128], line[256], *fstr, *vstr;

	s1 = &slaves[0];
	s2 = &slaves[1];

	snprintf(cmd, sizeof(cmd), "cat bond0.txt");

	fp = popen(cmd, "r");
	if(fp == NULL) {
		printf("popen failed: %s\n", strerror(errno));
		return -1;
	}

	blk = BLK_AGGR;
	while(fgets(line, sizeof(line), fp) != NULL) {
		if(strstr(line, delim) == NULL)
			continue;
		
		fstr = strtok(line, delim);
		vstr = strtok(NULL, "");

		if((fstr == NULL) ||
			((vstr == NULL) &&
			(strcasestr(fstr, "details actor lacp pdu") == NULL) &&
			(strcasestr(fstr, "details partner lacp pdu") == NULL)))
			continue;
		
		if(strcasestr(fstr, "Slave Interface") &&
			strcasestr(vstr, "fm1-mac1.p1")) {
				blk = BLK_S1;
			}
		else if(strcasestr(fstr, "Slave Interface") &&
			strcasestr(vstr, "fm1-mac1.p2")) {
			blk = BLK_S2;
		}
		else if(strcasestr(fstr, "details actor lacp pdu")) {
			if(blk == BLK_S1)
				blk = BLK_S1_ACTOR;
			else if(blk == BLK_S2)
				blk = BLK_S2_ACTOR;
		}
		else if(strcasestr(fstr, "details partner lacp pdu")) {
			if(blk == BLK_S1_ACTOR)
				blk = BLK_S1_PARTNER;
			else if(blk == BLK_S2_ACTOR)
				blk = BLK_S2_PARTNER;
		}
		else if(strcasestr(fstr, "Actor Churn State")) {
			if(strcasestr(vstr, "monitoring"))
				val = CHURN_MONITOR;
			else if(strcasestr(vstr, "churned"))
				val = CHURNED;
			else
				val = NO_CHURN;

			if(blk == BLK_S1)
				s1->actor_churn_state = val;
			else if(blk == BLK_S2)
				s2->actor_churn_state = val;
		}
		else if(strcasestr(fstr, "Partner Churn State")) {
			if(strcasestr(vstr, "monitoring"))
				val = CHURN_MONITOR;
			else if(strcasestr(vstr, "churned"))
				val = CHURNED;
			else
				val = NO_CHURN;

			if(blk == BLK_S1)
				s1->partner_churn_state = val;
			else if(blk == BLK_S2)
				s2->partner_churn_state = val;
		}
		else if(strcasestr(fstr, "Actor Churned Count")) {
			if(blk == BLK_S1)
				s1->actor_churn_count = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2)
				s2->actor_churn_count = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "Partner Churned Count")) {
			if(blk == BLK_S1)
				s1->partner_churn_count = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2)
				s2->partner_churn_count = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "system priority")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_system_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_system_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_system_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_system_prio = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port key")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_key = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_key = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "oper key")) {
			if(blk == BLK_S1_PARTNER)
				s1->partner_oper_key = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_oper_key = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port priority")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_port_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_port_prio = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port number")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_number = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_port_number = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_number = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_port_number = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port state")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_state = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_port_state = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_state = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_port_state = strtoul(vstr, NULL, 0);
		}
	}

	print_slave_status(s1);
	printf("----------------------------\n");
	print_slave_status(s2);

	pclose(fp);

	return 0;
}

// strtok 可以指定多个分隔符, strtok 内部维护 last token 的位置，因此 strtok 操作的字符串必须保存在 buffer 数组里，因为 strtok 会原地修改数组内容
// 以 `char buf[] = "this,is,a,string";`为例，连续调用 strtok 之后 buffer 数组的内容如下
	t  h  i  s  ,  i  s  ,  a  ,  s  t  r  i  n  g \0         this,is,a,string
	
	t  h  i  s  \0 i  s  ,  a  ,  s  t  r  i  n  g \0         this
	^
	t  h  i  s  \0 i  s  \0 a  ,  s  t  r  i  n  g \0         is
					^
	t  h  i  s  \0 i  s  \0 a  \0 s  t  r  i  n  g \0         a
							^
	t  h  i  s  \0 i  s  \0 a  \0 s  t  r  i  n  g \0         string

// scanf, fscanf, sscanf, vscanf, vsscanf, vfscanf
// scanf使用空白符(newlines, tabs, and spaces)将输入分割成各个不同的域(scanf matches up consecutive conversion specifications to consecutive fields, skipping over the whitespace in between)
[C scanf format specifier](https://www.demo2s.com/c/c-scanf-format-specifier.html)  
[带过滤器的scanf读取字符和字符串](https://www.demo2s.com/c/c-reading-characters-and-string-using-scanf-with-filter.html)  

// kill
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

int check_process_stop(int pid)
{
	int r, status;

	r = waitpid(pid, &status, WNOHANG);
	if(r == 0)
		return 1;

	if(r < 0) {
		printf("waitpid failed: %s\n", strerror(errno));
		return 1;
	}

	printf("process %d exited: r = %d\n", pid, r);

	return 0;
}

int main(void)
{
	int n, i, r, ai, my_pid;
	const char *av[10];
	char cmd[256];

	av[0] = "/mnt/c/Users/morrism/Downloads/test.sh";
	av[1] = "param1";
	ai = 2;
	av[ai++] = "param2";
	av[ai] = NULL;

	n = 0;
	for(i = 0; i < ai; i++)
		n += snprintf(cmd + n, sizeof(cmd) - n, " %s", av[i]);
	printf("cmd:%s\n", cmd);

	my_pid = fork();
	switch(my_pid) {
	default:
		// in parent process, doSomething such as monitoring
		printf("This is parent process\n");
		break;
	case -1:
		printf("fork failed: %s\n", strerror(errno));
		my_pid = 0;
		break;
	case 0:
		setpgid(0, 0);
		execv(av[0], (char*const*)av);
		exit(-1);
	}

		// sleep 5 secs for test purpose
	sleep(5);

	// tear down child process
	r = kill(my_pid, SIGTERM);
	if(r < 0)
		printf("kill failed: %s\n", strerror(errno));

	for(i = 0; i < 100; i++) {
		r = check_process_stop(my_pid);
		if(r == 0) {
			my_pid = 0;
			break;
		}
		usleep(100);
	}

	if(my_pid != 0) {
		printf("waitpid did not succeed: r = %d\n, pocess %d is now a zombie process", r, my_pid);
		my_pid = 0;
	}

	return 0;
}

// sysconf
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int main()
{
	int ncore;
	errno = 0;

	if ((ncore = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
		if (errno == 0)
			printf("_SC_NPROCESSORS_ONLN not supported by this implementation.\n");
		else
			perror("sysconf error.");
	else
		printf("ncore = %d\n", ncore);
}

// sysinfo
#include <sys/sysinfo.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define MB (1024 * 1024)

int main(void)
{
	int r;
	struct sysinfo si;
	int totalram, freeram, percent_inuse;

	r = sysinfo(&si);
	if(r < 0) {
		printf("sysinfo failed: %s", strerror(errno));
		return -1;
	}

	totalram = si.totalram/MB;
	freeram = si.freeram/MB;
	percent_inuse = (int)((float)(totalram - freeram) / (float)totalram * 100.0);

	printf("totalram: %d(MB), freeram:%d(MB), percent_inuse:%d\%\n",
		totalram, freeram, percent_inuse);

	return 0;
}

// readlink
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>

int main(void)
{
	int r;
	char *tz_path;
	char lpath[256], cmd[128];
	const char *symlink = "/etc/localtime";

	r = readlink(symlink, lpath, sizeof(lpath));
	if(r < 0) {
		printf("readlink failed\n");
		return -1;
	}

	printf("The value for symbol link %s is %s\n", symlink, lpath);

	tz_path = "/usr/share/zoneinfo/Asia/Shanghai";
	snprintf(cmd, sizeof(cmd), "ln -sf %s /etc/localtime", tz_path);

	r = system(cmd);
	if(r != 0) {
		printf("system failed\n");
		return -1;
	}
	printf("symlink %s to target %s succeed\n", symlink, tz_path);

	return 0;
}

// stat, fstat
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
	struct stat st;
	int r, n, fd;
	char *buf;

	fd = open("file.txt", O_RDONLY);
	if(fd < 0) {
		printf("open failed\n");
		return -1;
	}

	r = fstat(fd, &st);
	if(r < 0 || st.st_size == 0)
		n = 1024;
	else
		n = st.st_size;

	buf = malloc(n);

	n = read(fd, buf, n);
	if(n > 0)
		printf("buf:%s", buf);

	free(buf);
	close(fd);

	return 0;
}

// uname
#include <sys/types.h>
#include <sys/utsname.h>
#include <stdio.h>

int main()
{
	struct utsname sysInfo;
	if (uname(&sysInfo) != -1) {
		puts(sysInfo.sysname);
		puts(sysInfo.nodename);
		puts(sysInfo.release);
		puts(sysInfo.version);
		puts(sysInfo.machine);
	}
	else
		perror("uname() error");
}
```

[**Extensions to the C Language Family**](https://gcc.gnu.org/onlinedocs/gcc-12.2.0/gcc/C-Extensions.html)  
[GNU C Language Manual](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/index.html#SEC_Contents)  
[Online Python to C Converter](https://www.codeconvert.ai/python-to-c-converter)  
[The GNU C Reference Manual](https://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html)  
[C Operator Precedence](https://en.cppreference.com/w/c/language/operator_precedence)  
[**The GNU C Library**](https://www.gnu.org/software/libc/manual/html_node/index.html#SEC_Contents) #online  
[**The GNU C Library**](http://herbert.the-little-red-haired-girl.org/html/glibc/libc_toc.html)  
[**The GNU C Library (glibc) manual**](https://sourceware.org/glibc/manual/)  
[c函数使用参考实例](https://bytefreaks.net/category/programming-2/c-programming-2)  
[POXIS Function Reference Example](https://support.sas.com/documentation/onlinedoc/sasc/doc/lr2/lrv2ch20.htm#lr2posix)  
[C standard](https://www.open-std.org/jtc1/sc22/wg14/www/standards.html)  
[**glibc source code**](https://elixir.bootlin.com/glibc/latest/source) #sourcecode  
[The GNU C Library](https://www.gnu.org/software/libc/manual/html_mono/libc.html)  
[Memory Layout Of A C Program](https://hackthedeveloper.com/memory-layout-c-program/)  
[C Program Compilation Process](https://hackthedeveloper.com/c-program-compilation-process/)  
[C 语言常见问题集](https://c-faq-chn.sourceforge.net/ccfaq/ccfaq.html)  
[Notes for C and C++ Programming](https://www.codeinsideout.com/blog/c-cpp/notes/)  
[c for dummies](https://c-for-dummies.com/)  
[C and C++ Projects](https://www.codewithc.com/c-projects-with-source-code/)  
[How to split/tokenize a string? (strtok() string tokenizer)](https://www.equestionanswers.com/c/c-strtok.php)  
[declaring a flexible array member](https://wiki.sei.cmu.edu/confluence/display/c/DCL38-C.+Use+the+correct+syntax+when+declaring+a+flexible+array+member)  
[How to interpret complex C/C++ declarations](https://www.codeproject.com/Articles/7042/How-to-interpret-complex-C-C-declarations)  
[Reading C type declarations](http://unixwiz.net/techtips/reading-cdecl.html)  
[cdecl.org](https://cdecl.org/)  

[Awesome C](https://github.com/oz123/awesome-c?tab=readme-ov-file#awesome-c) #github  
[C](https://github.com/marcotrosi/C)  
[**Programming in C**](https://users.cs.cf.ac.uk/dave/C/)  
[HOWTO: Use Address Sanitizer](https://www.osc.edu/resources/getting_started/howto/howto_use_address_sanitizer)  
[Clang 19.0.0git documentation](https://clang.llvm.org/docs/index.html)  
[Signal Handling](https://www.gnu.org/software/libc/manual/html_node/Signal-Handling.html)  
[Introduction To Unix Signals Programming](https://www.cs.kent.edu/~ruttan/sysprog/lectures/signals.html)  
[CS 43203 : System Programming](https://www.cs.kent.edu/~ruttan/sysprog/)  
[Unix Pthreads tutorial](https://www.cs.kent.edu/~ruttan/sysprog/)  
[C program to implement linked list](https://www.programmingsimplified.com/c/data-structures)  
[Using Templates and Generics in C](https://levelup.gitconnected.com/using-templates-and-generics-in-c-968da223154d)  
[Tutorial: Generics in C](https://itnext.io/tutorial-generics-in-c-b3362b3376a3)  
[Generic Programming in C](https://cs.boisestate.edu/~amit/teaching/253/handouts/07-c-generic-coding-handout.pdf)  
[void * and Function Pointers](https://web.stanford.edu/class/archive/cs/cs107/cs107.1202/lab4/)  
[Functional Pointer and Callback in C++](https://primerpy.medium.com/functional-pointer-and-callback-in-c-86c208df6b2f)  
[C and C++ Language Syntax Reference](https://www.cprogramming.com/reference/)  
[Understanding the LD_LIBRARY_PATH Environment Variable](https://tecadmin.net/understanding-the-ld_library_path-environment-variable/)  
[Arrays of Length Zero](https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html)  
[Bounded Flexible Arrays in C](https://people.kernel.org/kees/bounded-flexible-arrays-in-c)  
[Flexible Array Members for C++](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1039r0.html)  
[Zero-length arrays in C have to go at the end of the struct](https://shivankaul.com/blog/zero-length-arrays)  
[How to Use typeof, Statement Expressions and Block-Scope Label Names](https://www.oracle.com/solaris/technologies/c-type.html)  
[The Unofficial C For Dummies Website](https://c-for-dummies.com/)  
