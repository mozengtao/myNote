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
## 回调（callback）
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
## 插件（plugin / module）
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

## 状态机（FSM）
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

## 事件驱动（event-driven loop）
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

## 不透明指针 opaque pointer
### bank_counter.h
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
### bank_counter.c
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

## 继承: “在不改变调用方的前提下，扩展或替换行为和数据”
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

## container_of（反向继承）
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

## ops 表（行为继承 / 多态）
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

## 接口继承（纯行为）
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

## 多态: 同一个行为，不同对象有不同的表现形式(开闭原则)
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

## uart example
### uart_device.h
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
### uart_device.c
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