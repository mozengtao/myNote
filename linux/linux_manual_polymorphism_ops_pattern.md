# Manual Polymorphism (Ops-Based Pattern) in C

## Overview

This document provides a **deep engineering analysis** of the ops-based polymorphism pattern in C, using the following code as the primary study subject:

```c
typedef struct eio_pin {
    struct eio_pin *next;
    const struct eio_pin_ops *ops;
    void *user_data;

    const char *name;
    uint8_t mode;
    bool status;
} eio_pin_t;

typedef struct eio_pin_ops {
    void (* init)(eio_pin_t * const me);
    void (* set_mode)(eio_pin_t * const me, uint8_t mode);
    bool (* get_status)(eio_pin_t * const me);
    void (* set_status)(eio_pin_t * const me, bool status);
} eio_pin_ops_t;
```

---

## 1. Core Concepts

### What "Manual Polymorphism" Means in C

```
+------------------------------------------------------------------+
|  POLYMORPHISM: "Many Forms"                                      |
+------------------------------------------------------------------+

    The ability for the SAME INTERFACE to invoke DIFFERENT BEHAVIOR
    depending on the CONCRETE TYPE of the object.

    In C++/Java:
    ┌─────────────────────────────────────────────────────────────┐
    │  Base* obj = new Derived();                                 │
    │  obj->doSomething();  // Calls Derived::doSomething()       │
    │                                                             │
    │  Compiler generates vtable lookup automatically             │
    └─────────────────────────────────────────────────────────────┘

    In C (Manual):
    ┌─────────────────────────────────────────────────────────────┐
    │  eio_pin_t *pin = get_some_pin();                           │
    │  pin->ops->set_status(pin, true);                           │
    │            │           │                                    │
    │            │           └── "this" pointer passed explicitly │
    │            └── Function pointer looked up manually          │
    └─────────────────────────────────────────────────────────────┘

    "Manual" = YOU build the indirection that C++ generates
```

**中文解释：**
- 多态："相同接口，不同行为"
- C++ 自动生成虚表查找
- C 需要手动构建：函数指针表 + 显式传递 "this" 指针
- "手动"意味着程序员构建编译器自动生成的间接层

### Why C Lacks Native Polymorphism

```
+------------------------------------------------------------------+
|  C's TYPE SYSTEM vs OOP LANGUAGES                                |
+------------------------------------------------------------------+

    C's DESIGN PHILOSOPHY:
    +----------------------------------------------------------+
    | - Minimal runtime overhead                                |
    | - No hidden mechanisms                                    |
    | - Explicit control over memory layout                     |
    | - What you write is (roughly) what executes               |
    +----------------------------------------------------------+

    WHAT C PROVIDES:
    +----------------------------------------------------------+
    | - struct (data aggregation only)                          |
    | - Function pointers (first-class callable values)         |
    | - void* (type-erased pointer)                             |
    | - Explicit casting                                        |
    +----------------------------------------------------------+

    WHAT C LACKS:
    +----------------------------------------------------------+
    | - Inheritance (no "struct B extends A")                   |
    | - Virtual methods (no automatic dispatch)                 |
    | - RTTI (runtime type information)                         |
    | - Constructors/destructors (lifecycle hooks)              |
    +----------------------------------------------------------+

    FUNCTION POINTERS COMPENSATE:
    +----------------------------------------------------------+
    | void (*fn)(int);  // Variable that holds a function       |
    |                                                           |
    | fn = concrete_implementation;                             |
    | fn(42);           // Indirect call                        |
    |                                                           |
    | This IS polymorphism - same call site, different behavior |
    +----------------------------------------------------------+
```

**中文解释：**
- C 的设计哲学：最小运行时开销、无隐藏机制、显式控制内存布局
- C 提供：struct、函数指针、void*、显式转换
- C 缺少：继承、虚方法、运行时类型信息、构造/析构
- 函数指针弥补：同一调用点，不同行为 = 多态

### Comparison to Other Dispatch Mechanisms

```
+------------------------------------------------------------------+
|  DISPATCH MECHANISM COMPARISON                                   |
+------------------------------------------------------------------+

    1. SWITCH/ENUM DISPATCH:
    ┌─────────────────────────────────────────────────────────────┐
    │  switch (pin->type) {                                       │
    │      case PIN_TYPE_GPIO:                                    │
    │          gpio_set_status(pin, status);                      │
    │          break;                                             │
    │      case PIN_TYPE_I2C:                                     │
    │          i2c_set_status(pin, status);                       │
    │          break;                                             │
    │  }                                                          │
    │                                                             │
    │  Problems:                                                  │
    │  - Every call site must know ALL types                      │
    │  - Adding new type requires modifying ALL switch statements │
    │  - Centralized knowledge, not extensible                    │
    └─────────────────────────────────────────────────────────────┘

    2. OPS-BASED (your code):
    ┌─────────────────────────────────────────────────────────────┐
    │  pin->ops->set_status(pin, status);                         │
    │                                                             │
    │  Advantages:                                                │
    │  - Call site doesn't know concrete type                     │
    │  - Adding new type doesn't touch existing code              │
    │  - Decentralized, extensible                                │
    └─────────────────────────────────────────────────────────────┘

    3. C++ VIRTUAL:
    ┌─────────────────────────────────────────────────────────────┐
    │  pin->set_status(status);  // Compiler handles lookup       │
    │                                                             │
    │  Differences from C ops:                                    │
    │  - Hidden vtable pointer (first 8 bytes of object)          │
    │  - No explicit "this" passing                               │
    │  - Compiler enforces interface contracts                    │
    │  - RTTI available                                           │
    └─────────────────────────────────────────────────────────────┘

    COMPARISON TABLE:
    
    ┌─────────────────┬────────────┬────────────┬────────────────┐
    │ Property        │ switch/enum│ ops-based  │ C++ virtual    │
    ├─────────────────┼────────────┼────────────┼────────────────┤
    │ Extensibility   │ Poor       │ Excellent  │ Excellent      │
    │ Call overhead   │ Branch     │ Indirect   │ Indirect       │
    │ Type safety     │ None       │ None       │ Compile-time   │
    │ Hidden cost     │ None       │ None       │ vtable per type│
    │ Flexibility     │ Centralize │ Decentral  │ Decentral      │
    │ Debug ease      │ Easy       │ Harder     │ Harder         │
    └─────────────────┴────────────┴────────────┴────────────────┘
```

**中文解释：**
- switch/enum：每个调用点必须知道所有类型，添加类型需修改所有 switch
- ops-based：调用点不知道具体类型，添加新类型不触及现有代码
- C++ virtual：编译器处理查找，隐藏 vtable，有类型安全

### Fields Enabling Polymorphism

```
+------------------------------------------------------------------+
|  ANATOMY OF YOUR CODE                                            |
+------------------------------------------------------------------+

    typedef struct eio_pin {
        struct eio_pin *next;           // Linked list (registration)
        const struct eio_pin_ops *ops;  // ★ POLYMORPHISM ENABLER
        void *user_data;                // ★ INSTANCE-SPECIFIC DATA
        
        const char *name;               // Common instance data
        uint8_t mode;
        bool status;
    } eio_pin_t;

    POLYMORPHISM-ENABLING FIELDS:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  const struct eio_pin_ops *ops;                             │
    │  ─────────────────────────────                              │
    │  - Pointer to behavior table                                │
    │  - Different instances can have different ops               │
    │  - THIS is the "vtable" equivalent                          │
    │  - const: behavior table is immutable                       │
    └─────────────────────────────────────────────────────────────┘
    
    ┌─────────────────────────────────────────────────────────────┐
    │  void *user_data;                                           │
    │  ─────────────────                                          │
    │  - Type-erased pointer to implementation-specific data      │
    │  - Allows each implementation to have private state         │
    │  - Only the implementation knows how to cast it             │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- `ops`：行为表指针，不同实例可有不同 ops，这是"虚表"等价物
- `user_data`：类型擦除指针，允许每个实现有私有状态，只有实现知道如何转换

### Role of the `me` Pointer

```
+------------------------------------------------------------------+
|  THE `me` POINTER (Explicit "this")                              |
+------------------------------------------------------------------+

    void (* set_status)(eio_pin_t * const me, bool status);
                        ─────────────────────
                              │
                              ▼
                        Explicit "this" pointer

    WHY EXPLICIT:
    +----------------------------------------------------------+
    | C has no hidden "this" - must pass object explicitly      |
    |                                                           |
    | In C++:  pin->set_status(true);                           |
    |          Compiler secretly passes: set_status(pin, true)  |
    |                                                           |
    | In C:    pin->ops->set_status(pin, true);                 |
    |          YOU must pass pin explicitly                     |
    +----------------------------------------------------------+

    WHY `eio_pin_t * const me`:
    +----------------------------------------------------------+
    | - `eio_pin_t *`: Pointer to the base object               |
    | - `const`:       The pointer itself won't change          |
    |                  (NOT const data - data can be modified)  |
    |                                                           |
    | This is a CONVENTION, not a requirement                   |
    | Kernel uses just `struct xxx *` typically                 |
    +----------------------------------------------------------+

    WHAT `me` PROVIDES TO IMPLEMENTATION:
    +----------------------------------------------------------+
    | void gpio_set_status(eio_pin_t * const me, bool status)   |
    | {                                                         |
    |     /* Access common fields */                            |
    |     me->status = status;                                  |
    |     me->name;  // Know who we are                         |
    |                                                           |
    |     /* Access private data */                             |
    |     struct gpio_private *priv = me->user_data;            |
    |     /* or: container_of(me, ...) */                       |
    | }                                                         |
    +----------------------------------------------------------+
```

**中文解释：**
- C 没有隐藏 "this"，必须显式传递对象指针
- `eio_pin_t * const me`：指针本身不变，但数据可修改
- `me` 提供：访问公共字段、访问私有数据（通过 user_data 或 container_of）

---

## 2. Architecture & Layering

### Object Model

```
+------------------------------------------------------------------+
|  CONCEPTUAL OBJECT MODEL                                         |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │                     INSTANCE (eio_pin_t)                    │
    ├─────────────────────────────────────────────────────────────┤
    │  next ────────────────────────▶ (linked list)              │
    │  ops ─────────────────────────┐                             │
    │  user_data ──────────────────────────────────────┐          │
    │  name                         │                  │          │
    │  mode                         │                  │          │
    │  status                       │                  │          │
    └───────────────────────────────┼──────────────────┼──────────┘
                                    │                  │
                                    ▼                  ▼
    ┌───────────────────────────────────┐   ┌─────────────────────┐
    │    BEHAVIOR TABLE (eio_pin_ops_t) │   │  PRIVATE DATA       │
    ├───────────────────────────────────┤   │  (implementation-   │
    │  init ──────▶ gpio_init()         │   │   specific)         │
    │  set_mode ──▶ gpio_set_mode()     │   ├─────────────────────┤
    │  get_status ▶ gpio_get_status()   │   │  hw_base_addr       │
    │  set_status ▶ gpio_set_status()   │   │  irq_number         │
    └───────────────────────────────────┘   │  ...                │
                                            └─────────────────────┘

    MEMORY LAYOUT:
    
    ┌───────────────────────────────────────────────────────────┐
    │            eio_pin_t (stack or heap)                      │
    │  ┌────────┬────────┬────────┬──────┬──────┬────────┐      │
    │  │  next  │  ops   │user_dat│ name │ mode │ status │      │
    │  │ (8B)   │ (8B)   │ (8B)   │ (8B) │ (1B) │  (1B)  │      │
    │  └────────┴───┬────┴────┬───┴──────┴──────┴────────┘      │
    │               │         │                                 │
    │               ▼         ▼                                 │
    │          ops table   private data                         │
    │          (static)    (heap or embedded)                   │
    └───────────────────────────────────────────────────────────┘
```

**中文解释：**
- 实例（eio_pin_t）包含：链表指针、ops 指针、私有数据指针、公共字段
- ops 表（eio_pin_ops_t）：静态分配，包含函数指针
- 私有数据：实现特定，堆分配或嵌入

### Why `ops` is a Pointer, Not Embedded

```
+------------------------------------------------------------------+
|  POINTER vs EMBEDDED OPS                                         |
+------------------------------------------------------------------+

    EMBEDDED (NOT DONE):
    ┌─────────────────────────────────────────────────────────────┐
    │  typedef struct eio_pin {                                   │
    │      eio_pin_ops_t ops;  // 32 bytes embedded               │
    │      ...                                                    │
    │  } eio_pin_t;                                               │
    │                                                              │
    │  Problems:                                                  │
    │  - Every instance carries full ops table (32 bytes)         │
    │  - 100 GPIO pins = 3200 bytes wasted                        │
    │  - Each pin could have different function pointers!         │
    │    (breaks invariant: same type = same behavior)            │
    └─────────────────────────────────────────────────────────────┘

    POINTER (YOUR DESIGN):
    ┌─────────────────────────────────────────────────────────────┐
    │  typedef struct eio_pin {                                   │
    │      const struct eio_pin_ops *ops;  // 8 bytes             │
    │      ...                                                    │
    │  } eio_pin_t;                                               │
    │                                                              │
    │  Benefits:                                                  │
    │  - All instances of same type share ONE ops table           │
    │  - Memory efficient: 8 bytes per instance                   │
    │  - Guarantees: same ops pointer = same behavior             │
    │  - Ops table can be in .rodata (read-only, cacheable)       │
    └─────────────────────────────────────────────────────────────┘

    SHARING PATTERN:
    
    static const eio_pin_ops_t gpio_ops = { ... };
    
    eio_pin_t gpio_pin_1 = { .ops = &gpio_ops, ... };
    eio_pin_t gpio_pin_2 = { .ops = &gpio_ops, ... };
    eio_pin_t gpio_pin_3 = { .ops = &gpio_ops, ... };
                    │              │              │
                    └──────────────┼──────────────┘
                                   │
                                   ▼
                            ┌──────────────┐
                            │   gpio_ops   │  ← ONE copy
                            │   (static)   │
                            └──────────────┘
```

**中文解释：**
- 嵌入式：每个实例携带完整 ops 表（浪费内存），且每个 pin 可能有不同函数指针
- 指针式：同类型所有实例共享一个 ops 表，内存高效，ops 表可在只读段

### Why `ops` is `const`

```
+------------------------------------------------------------------+
|  WHY const struct eio_pin_ops *ops                               |
+------------------------------------------------------------------+

    const struct eio_pin_ops *ops;
    ─────                          
      │
      └── The ops TABLE is const (behavior is immutable)

    GUARANTEES:
    +----------------------------------------------------------+
    | 1. Behavior cannot change at runtime                      |
    |    - No "ops->set_status = evil_function"                 |
    |    - Pin type is fixed after creation                     |
    |                                                           |
    | 2. Ops table can be in .rodata                            |
    |    - Protected from writes (hardware enforced)            |
    |    - Better cache behavior                                |
    |                                                           |
    | 3. Thread safety (partial)                                |
    |    - Multiple threads can read ops safely                 |
    |    - No need to lock for dispatch                         |
    +----------------------------------------------------------+

    COMPARISON:
    
    MUTABLE (dangerous):                CONST (safe):
    
    pin->ops->set_status = NULL;  ✗    Compiler error!
    pin->ops = other_ops;         ✓    Pointer can change
                                       (but table contents can't)
```

**中文解释：**
- `const`：ops 表内容不可变（行为固定）
- 保证：运行时行为不能改变、可放在只读段、多线程安全读取

### Role of `user_data`

```
+------------------------------------------------------------------+
|  user_data: PRIVATE IMPLEMENTATION DATA                          |
+------------------------------------------------------------------+

    TWO STRATEGIES FOR PRIVATE DATA:

    STRATEGY 1: user_data pointer (your code)
    ┌─────────────────────────────────────────────────────────────┐
    │  eio_pin_t base = {                                         │
    │      .ops = &gpio_ops,                                      │
    │      .user_data = &gpio_private_data,  ← void* to private   │
    │  };                                                         │
    │                                                              │
    │  In implementation:                                         │
    │  struct gpio_priv *priv = me->user_data;                    │
    │  priv->hw_register = ...;                                   │
    └─────────────────────────────────────────────────────────────┘

    STRATEGY 2: container_of (kernel style)
    ┌─────────────────────────────────────────────────────────────┐
    │  struct gpio_pin {                                          │
    │      struct gpio_priv priv;    // Private data              │
    │      eio_pin_t base;           // Base EMBEDDED             │
    │  };                                                         │
    │                                                              │
    │  In implementation:                                         │
    │  struct gpio_pin *gpio = container_of(me, struct gpio_pin,  │
    │                                        base);               │
    │  gpio->priv.hw_register = ...;                              │
    └─────────────────────────────────────────────────────────────┘

    COMPARISON:
    
    ┌─────────────────┬─────────────────────┬─────────────────────┐
    │ Property        │ user_data pointer   │ container_of        │
    ├─────────────────┼─────────────────────┼─────────────────────┤
    │ Memory layout   │ Two allocations     │ One allocation      │
    │ Cache locality  │ Worse (pointer hop) │ Better (adjacent)   │
    │ Type safety     │ None (void*)        │ Compile-time        │
    │ Flexibility     │ Can change at runtime│ Fixed at creation  │
    │ Kernel usage    │ platform_set_drvdata│ Most drivers        │
    └─────────────────┴─────────────────────┴─────────────────────┘
```

**中文解释：**
- user_data 指针：两次分配，灵活但类型不安全
- container_of：一次分配，缓存局部性好，类型安全
- 内核使用：大多数驱动用 container_of，平台数据用 platform_set_drvdata

### Linked List for Registration

```
+------------------------------------------------------------------+
|  next POINTER: SUBSYSTEM REGISTRATION                            |
+------------------------------------------------------------------+

    struct eio_pin *next;  // Linked list for registration

    KERNEL PATTERN:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  Global Registry                                            │
    │  ┌───────────┐                                              │
    │  │ pin_list  │                                              │
    │  │  (head)   │                                              │
    │  └─────┬─────┘                                              │
    │        │                                                    │
    │        ▼                                                    │
    │  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐     │
    │  │ gpio_pin_1   │──▶│ i2c_pin_1    │──▶│ spi_pin_1    │──▶NULL
    │  │ ops=gpio_ops │   │ ops=i2c_ops  │   │ ops=spi_ops  │     │
    │  └──────────────┘   └──────────────┘   └──────────────┘     │
    │                                                             │
    │  Mixed types, unified interface!                            │
    └─────────────────────────────────────────────────────────────┘

    REGISTRATION FUNCTION:
    
    static eio_pin_t *pin_list = NULL;
    
    void eio_pin_register(eio_pin_t *pin) {
        pin->next = pin_list;
        pin_list = pin;
        
        if (pin->ops->init)
            pin->ops->init(pin);  // Polymorphic init!
    }
    
    KERNEL EQUIVALENT:
    +----------------------------------------------------------+
    | misc_register()   - adds to misc_list                     |
    | platform_driver_register() - adds to platform bus         |
    | i2c_add_driver()  - adds to i2c bus                       |
    +----------------------------------------------------------+
```

**中文解释：**
- `next` 指针：用于子系统注册的链表
- 全局注册表：混合类型，统一接口
- 内核等价：misc_register、platform_driver_register、i2c_add_driver

### Call Flow Diagram

```
+------------------------------------------------------------------+
|  COMPLETE CALL FLOW                                              |
+------------------------------------------------------------------+

    HIGH-LEVEL API                OPS DISPATCH           CONCRETE IMPL
    ─────────────────             ────────────           ─────────────
    
    void eio_pin_set_status(eio_pin_t *pin, bool status)
    {
        │
        ▼
        if (pin && pin->ops && pin->ops->set_status)
                         │              │
                         │              │ ← NULL check
                         ▼              ▼
            pin->ops->set_status(pin, status);
                    │              │
                    │              └── Explicit "this"
                    │
                    └── Indirect call through function pointer
    }
                                   │
                                   ▼
    ┌────────────────────────────────────────────────────────────────┐
    │  Depending on pin->ops value, one of:                          │
    │                                                                │
    │  gpio_ops->set_status(pin, status)                             │
    │       │                                                        │
    │       ▼                                                        │
    │  static void gpio_set_status(eio_pin_t *me, bool status) {     │
    │      struct gpio_priv *priv = me->user_data;                   │
    │      write_hw_register(priv->base + GPIO_DATA, status);        │
    │      me->status = status;                                      │
    │  }                                                             │
    │                                                                │
    │  i2c_ops->set_status(pin, status)                              │
    │       │                                                        │
    │       ▼                                                        │
    │  static void i2c_set_status(eio_pin_t *me, bool status) {      │
    │      struct i2c_priv *priv = me->user_data;                    │
    │      i2c_write_byte(priv->adapter, priv->addr, status);        │
    │      me->status = status;                                      │
    │  }                                                             │
    └────────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 高层 API：检查 NULL，通过函数指针间接调用
- 分发：根据 pin->ops 值调用不同实现
- 具体实现：访问私有数据，操作硬件，更新状态

---

## 3. Call Flow & Control Inversion

### Registration Flow

```
+------------------------------------------------------------------+
|  REGISTRATION: eio_pin_register()                                |
+------------------------------------------------------------------+

    CALLER (Driver/Module):
    ┌─────────────────────────────────────────────────────────────┐
    │  static const eio_pin_ops_t gpio_ops = {                    │
    │      .init = gpio_init,                                     │
    │      .set_mode = gpio_set_mode,                             │
    │      .get_status = gpio_get_status,                         │
    │      .set_status = gpio_set_status,                         │
    │  };                                                         │
    │                                                             │
    │  static struct gpio_priv my_gpio_priv = { ... };            │
    │                                                             │
    │  static eio_pin_t my_gpio_pin = {                           │
    │      .ops = &gpio_ops,                                      │
    │      .user_data = &my_gpio_priv,                            │
    │      .name = "GPIO_0",                                      │
    │  };                                                         │
    │                                                             │
    │  void my_driver_init(void) {                                │
    │      eio_pin_register(&my_gpio_pin);                        │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
    
    FRAMEWORK (Subsystem Core):
    ┌─────────────────────────────────────────────────────────────┐
    │  static eio_pin_t *pin_list = NULL;                         │
    │                                                             │
    │  void eio_pin_register(eio_pin_t *pin) {                    │
    │      // Add to registry                                     │
    │      pin->next = pin_list;                                  │
    │      pin_list = pin;                                        │
    │                                                             │
    │      // Call implementation's init (if provided)            │
    │      if (pin->ops && pin->ops->init)                        │
    │          pin->ops->init(pin);  // ← CONTROL INVERSION       │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP:
    +----------------------------------------------------------+
    | eio_pin_t structure: Driver owns                          |
    | eio_pin_ops_t table: Driver owns (static const)           |
    | Registration list: Framework owns                         |
    | Initialization decision: Framework decides WHEN           |
    | Initialization behavior: Driver decides WHAT              |
    +----------------------------------------------------------+
```

**中文解释：**
- 驱动提供：ops 表（静态 const）、私有数据、pin 结构
- 框架提供：注册表、注册函数
- 所有权：pin 结构由驱动拥有，注册表由框架拥有
- 控制反转：框架决定何时初始化，驱动决定如何初始化

### Lookup Flow

```
+------------------------------------------------------------------+
|  LOOKUP: eio_pin_find()                                          |
+------------------------------------------------------------------+

    eio_pin_t *eio_pin_find(const char *name) {
        for (eio_pin_t *p = pin_list; p != NULL; p = p->next) {
            if (strcmp(p->name, name) == 0)
                return p;
        }
        return NULL;
    }

    USAGE:
    ┌─────────────────────────────────────────────────────────────┐
    │  // Consumer code - doesn't know concrete type!             │
    │  eio_pin_t *led = eio_pin_find("LED_STATUS");               │
    │  if (led) {                                                 │
    │      led->ops->set_status(led, true);                       │
    │      //                                                     │
    │      // Consumer has NO IDEA if this is:                    │
    │      // - GPIO pin                                          │
    │      // - I2C expander                                      │
    │      // - SPI shift register                                │
    │      // - USB-controlled relay                              │
    │      //                                                     │
    │      // It just works!                                      │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 查找：按名称遍历链表
- 消费者：不知道具体类型，只通过 ops 接口操作
- 多态效果：同一代码处理 GPIO、I2C、SPI 等不同实现

### Control Inversion Analysis

```
+------------------------------------------------------------------+
|  CONTROL INVERSION (IoC)                                         |
+------------------------------------------------------------------+

    WITHOUT IoC (Traditional):
    ┌─────────────────────────────────────────────────────────────┐
    │  Application                                                │
    │       │                                                     │
    │       │ directly calls                                      │
    │       ▼                                                     │
    │  gpio_init(), gpio_set_status()                             │
    │                                                             │
    │  Application CONTROLS the flow                              │
    │  Application KNOWS the concrete type                        │
    └─────────────────────────────────────────────────────────────┘

    WITH IoC (Your pattern):
    ┌─────────────────────────────────────────────────────────────┐
    │  Driver                      Framework                      │
    │       │                           │                         │
    │       │ provides ops              │                         │
    │       │─────────────────────────▶│                         │
    │       │                           │                         │
    │       │                           │ calls ops->init()       │
    │       │◀─────────────────────────│                         │
    │       │                           │                         │
    │  Driver provides WHAT             Framework decides WHEN    │
    │  Control is INVERTED              │                         │
    └─────────────────────────────────────────────────────────────┘

    LINUX KERNEL EXAMPLES:
    
    ┌───────────────────┬────────────────────────────────────────┐
    │ Pattern           │ IoC Point                              │
    ├───────────────────┼────────────────────────────────────────┤
    │ file_operations   │ VFS calls fops->read when user reads   │
    │ net_device_ops    │ Network core calls ndo_start_xmit      │
    │ gpio_chip         │ gpiolib calls chip->get when GPIO read │
    │ i2c_algorithm     │ i2c-core calls algo->master_xfer       │
    │ platform_driver   │ Driver core calls drv->probe           │
    └───────────────────┴────────────────────────────────────────┘
    
    KEY INSIGHT:
    +----------------------------------------------------------+
    | The FRAMEWORK owns the control flow                       |
    | The DRIVER provides the implementation                    |
    | The DRIVER never calls framework internals                |
    | → This is "Hollywood Principle": Don't call us, we'll     |
    |   call you                                                |
    +----------------------------------------------------------+
```

**中文解释：**
- 无 IoC：应用直接调用具体函数，控制流程，知道具体类型
- 有 IoC：驱动提供 ops，框架决定何时调用
- 好莱坞原则："别打电话给我们，我们会打给你"
- 内核示例：file_operations、net_device_ops、gpio_chip、i2c_algorithm

---

## 4. Design Decisions & Trade-offs

### Why Not Alternatives

```
+------------------------------------------------------------------+
|  WHY NOT SWITCH STATEMENTS                                       |
+------------------------------------------------------------------+

    void set_pin_status(pin_t *pin, bool status) {
        switch (pin->type) {
            case PIN_GPIO:  gpio_set_status(pin, status); break;
            case PIN_I2C:   i2c_set_status(pin, status); break;
            case PIN_SPI:   spi_set_status(pin, status); break;
            // Adding USB pin requires modifying THIS file!
        }
    }

    PROBLEMS:
    +----------------------------------------------------------+
    | 1. Centralized knowledge                                  |
    |    - Core must know ALL implementations                   |
    |    - Cannot add types without modifying core              |
    |                                                           |
    | 2. Combinatorial explosion                                |
    |    - N operations × M types = N×M cases                   |
    |    - Each function has full switch                        |
    |                                                           |
    | 3. No encapsulation                                       |
    |    - Implementation details visible in core               |
    |    - No clear ownership boundary                          |
    +----------------------------------------------------------+

+------------------------------------------------------------------+
|  WHY NOT CALLBACKS WITHOUT STRUCT                                |
+------------------------------------------------------------------+

    void register_pin(
        void (*init)(void *ctx),
        void (*set_status)(void *ctx, bool),
        void *ctx
    );

    PROBLEMS:
    +----------------------------------------------------------+
    | 1. Signature explosion                                    |
    |    - Each function passed separately                      |
    |    - Adding operation = changing all call sites           |
    |                                                           |
    | 2. No grouping                                            |
    |    - Related functions not bundled                        |
    |    - Easy to pass mismatched function sets                |
    |                                                           |
    | 3. No versioning                                          |
    |    - Can't extend interface compatibly                    |
    +----------------------------------------------------------+

+------------------------------------------------------------------+
|  WHY NOT OPAQUE HANDLES ONLY                                     |
+------------------------------------------------------------------+

    typedef void* pin_handle_t;
    
    pin_handle_t pin_create_gpio(void);
    void pin_set_status(pin_handle_t h, bool status);

    PROBLEMS:
    +----------------------------------------------------------+
    | 1. No introspection                                       |
    |    - Can't iterate all pins                               |
    |    - Can't query capabilities                             |
    |                                                           |
    | 2. Hidden dispatch mechanism                              |
    |    - Must be switch internally anyway                     |
    |    - Or store function pointers (= your design)           |
    |                                                           |
    | 3. Inflexible                                             |
    |    - Hard to add cross-cutting concerns                   |
    +----------------------------------------------------------+
```

**中文解释：**
- switch 问题：集中知识、组合爆炸、无封装
- 回调无结构问题：签名爆炸、无分组、无版本控制
- 不透明句柄问题：无内省、隐藏分发（最终还是要内部 switch 或函数指针）

### Advantages of Ops-Based Design

```
+------------------------------------------------------------------+
|  ADVANTAGES                                                      |
+------------------------------------------------------------------+

    1. EXTENSIBILITY
    +----------------------------------------------------------+
    | Add new pin type without touching existing code:          |
    |                                                           |
    | static const eio_pin_ops_t usb_ops = { ... };             |
    | eio_pin_t usb_pin = { .ops = &usb_ops, ... };             |
    | eio_pin_register(&usb_pin);  // Just works!               |
    +----------------------------------------------------------+

    2. SEPARATION OF CONCERNS
    +----------------------------------------------------------+
    | POLICY (what to do):    Application layer                 |
    | MECHANISM (how to do):  Implementation (ops)              |
    |                                                           |
    | Application: "Turn on LED when button pressed"            |
    | Implementation: "Write 1 to GPIO register 0x40"           |
    +----------------------------------------------------------+

    3. TESTABILITY
    +----------------------------------------------------------+
    | Create mock implementation for testing:                   |
    |                                                           |
    | static bool mock_status = false;                          |
    | static void mock_set_status(eio_pin_t *me, bool s) {      |
    |     mock_status = s;                                      |
    | }                                                         |
    | static const eio_pin_ops_t mock_ops = {                   |
    |     .set_status = mock_set_status,                        |
    | };                                                         |
    |                                                           |
    | // Test without real hardware!                            |
    | eio_pin_t test_pin = { .ops = &mock_ops };                |
    +----------------------------------------------------------+

    4. DECOUPLING
    +----------------------------------------------------------+
    | High-level code:      Compiled once, never changes        |
    | Low-level code:       Can be replaced, upgraded           |
    |                                                           |
    | No recompilation of high-level when low-level changes     |
    +----------------------------------------------------------+
```

**中文解释：**
- 可扩展性：添加新类型不触及现有代码
- 关注点分离：策略（做什么）vs 机制（如何做）
- 可测试性：创建 mock 实现用于测试
- 解耦：高层代码不因低层变化而重新编译

### Disadvantages

```
+------------------------------------------------------------------+
|  DISADVANTAGES                                                   |
+------------------------------------------------------------------+

    1. INDIRECT CALL OVERHEAD
    +----------------------------------------------------------+
    | Direct:    call gpio_set_status     (predictable)         |
    | Indirect:  mov rax, [pin->ops]                            |
    |            mov rax, [rax+offset]                          |
    |            call rax                  (unpredictable)       |
    |                                                           |
    | Cost: ~2-5 cycles extra + potential branch mispredict     |
    | Mitigation: Usually negligible for I/O operations         |
    +----------------------------------------------------------+

    2. DEBUGGING DIFFICULTY
    +----------------------------------------------------------+
    | Stack trace shows:                                        |
    |   ??? at 0x7f3a2b4c1234                                   |
    |                                                           |
    | Hard to see which implementation is called                |
    | GDB: need to print pin->ops to identify                   |
    |                                                           |
    | Mitigation: Good logging, debug prints with pin->name     |
    +----------------------------------------------------------+

    3. NO COMPILE-TIME ENFORCEMENT
    +----------------------------------------------------------+
    | Nothing prevents:                                         |
    |   pin->ops->set_status(wrong_pin, status);                |
    |   pin->ops = NULL;  // then crash later                   |
    |   ops->set_status = evil_function;  // (if not const)     |
    |                                                           |
    | C++ virtual: Compiler ensures type safety                 |
    | C ops: All checks are manual                              |
    +----------------------------------------------------------+

    4. ABI RIGIDITY
    +----------------------------------------------------------+
    | Adding new operation to eio_pin_ops_t:                    |
    |   - All existing implementations must be updated          |
    |   - Or new field must be NULL-checked everywhere          |
    |                                                           |
    | Changing existing signature:                              |
    |   - Breaks ALL implementations                            |
    |   - No compiler help to find all affected sites           |
    +----------------------------------------------------------+
```

**中文解释：**
- 间接调用开销：额外 2-5 周期 + 分支预测失败
- 调试困难：堆栈跟踪显示未知地址，需手动识别
- 无编译时强制：C 不检查类型安全，需手动验证
- ABI 刚性：添加/修改操作影响所有实现

### Trade-off Summary

```
+------------------------------------------------------------------+
|  TRADE-OFF SUMMARY                                               |
+------------------------------------------------------------------+

    ┌────────────────────┬────────────────┬─────────────────────────┐
    │ Dimension          │ Cost           │ Benefit                 │
    ├────────────────────┼────────────────┼─────────────────────────┤
    │ Performance        │ Indirect call  │ I/O-bound anyway        │
    │                    │ ~5 cycles      │                         │
    ├────────────────────┼────────────────┼─────────────────────────┤
    │ Memory             │ 8 bytes/inst   │ Shared ops table        │
    │                    │ for ops ptr    │                         │
    ├────────────────────┼────────────────┼─────────────────────────┤
    │ Maintenance        │ Manual checks  │ Independent evolution   │
    │                    │ needed         │ of implementations      │
    ├────────────────────┼────────────────┼─────────────────────────┤
    │ Type safety        │ None           │ Maximum flexibility     │
    ├────────────────────┼────────────────┼─────────────────────────┤
    │ Debugging          │ Harder         │ Clean separation        │
    └────────────────────┴────────────────┴─────────────────────────┘

    WHEN WORTH THE COST:
    +----------------------------------------------------------+
    | ✓ Multiple implementations expected                       |
    | ✓ Implementations added/changed independently             |
    | ✓ I/O-bound or infrequent operations                      |
    | ✓ Testing with mocks required                             |
    | ✓ Plugin-like architecture                                |
    +----------------------------------------------------------+

    WHEN NOT WORTH IT:
    +----------------------------------------------------------+
    | ✗ Only one implementation ever                            |
    | ✗ Performance-critical inner loops                        |
    | ✗ Simple, stable interface                                |
    | ✗ No testing isolation needed                             |
    +----------------------------------------------------------+
```

**中文解释：**
- 值得使用：多实现、独立演进、I/O 密集、需要 mock 测试、插件架构
- 不值得：只有一个实现、性能关键内循环、简单稳定接口、不需测试隔离

---

## 5. Safety & Correctness Considerations

### Failure Modes

```
+------------------------------------------------------------------+
|  FAILURE MODE 1: NULL ops POINTER                                |
+------------------------------------------------------------------+

    eio_pin_t pin = { .ops = NULL, .name = "test" };
    pin.ops->set_status(&pin, true);  // CRASH: NULL dereference

    DEFENSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  void eio_pin_set_status(eio_pin_t *pin, bool status) {     │
    │      if (!pin || !pin->ops) {                               │
    │          // Log error, return error code                    │
    │          return;                                            │
    │      }                                                      │
    │      if (pin->ops->set_status)                              │
    │          pin->ops->set_status(pin, status);                 │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

+------------------------------------------------------------------+
|  FAILURE MODE 2: NULL FUNCTION POINTER                           |
+------------------------------------------------------------------+

    static const eio_pin_ops_t partial_ops = {
        .init = gpio_init,
        .set_status = NULL,  // Not implemented!
    };
    
    pin.ops->set_status(&pin, true);  // CRASH

    DEFENSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  // Check before each call                                  │
    │  if (pin->ops->set_status)                                  │
    │      pin->ops->set_status(pin, status);                     │
    │                                                              │
    │  // Or provide wrapper that checks                          │
    │  static inline void eio_pin_set_status_safe(...) {          │
    │      if (pin && pin->ops && pin->ops->set_status)           │
    │          pin->ops->set_status(pin, status);                 │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

+------------------------------------------------------------------+
|  FAILURE MODE 3: LIFETIME MISMATCH                               |
+------------------------------------------------------------------+

    void bad_example(void) {
        struct gpio_priv priv = { ... };  // Stack allocated!
        eio_pin_t pin = { .user_data = &priv };
        eio_pin_register(&pin);
    }  // priv destroyed, but pin still registered!
    
    // Later: pin->user_data points to garbage

    DEFENSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  // Rule: user_data must outlive pin registration           │
    │                                                              │
    │  // Option 1: Static allocation                             │
    │  static struct gpio_priv priv = { ... };                    │
    │  static eio_pin_t pin = { .user_data = &priv };             │
    │                                                              │
    │  // Option 2: Heap with matching lifetime                   │
    │  struct gpio_priv *priv = malloc(sizeof(*priv));            │
    │  // Free only after eio_pin_unregister()                    │
    └─────────────────────────────────────────────────────────────┘

+------------------------------------------------------------------+
|  FAILURE MODE 4: CONCURRENCY                                     |
+------------------------------------------------------------------+

    Thread 1: eio_pin_register(&pin)  // Modifying list
    Thread 2: eio_pin_find("name")    // Reading list
    
    // DATA RACE!

    DEFENSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  static pthread_mutex_t pin_lock = PTHREAD_MUTEX_INITIALIZER;│
    │                                                              │
    │  void eio_pin_register(eio_pin_t *pin) {                    │
    │      pthread_mutex_lock(&pin_lock);                         │
    │      pin->next = pin_list;                                  │
    │      pin_list = pin;                                        │
    │      pthread_mutex_unlock(&pin_lock);                       │
    │  }                                                          │
    │                                                              │
    │  // Or use RCU for read-heavy workloads                     │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 失败模式1：ops 为 NULL → 检查后再调用
- 失败模式2：函数指针为 NULL → 每次调用前检查
- 失败模式3：生命周期不匹配 → user_data 必须比 pin 注册活得久
- 失败模式4：并发 → 加锁或使用 RCU

### Defensive Coding Practices

```
+------------------------------------------------------------------+
|  DEFENSIVE CODING RECOMMENDATIONS                                |
+------------------------------------------------------------------+

    1. MANDATORY OPERATIONS
    ┌─────────────────────────────────────────────────────────────┐
    │  // At registration time, validate required ops             │
    │  int eio_pin_register(eio_pin_t *pin) {                     │
    │      if (!pin || !pin->ops) {                               │
    │          return -EINVAL;                                    │
    │      }                                                      │
    │      // These are MANDATORY                                 │
    │      if (!pin->ops->get_status || !pin->ops->set_status) {  │
    │          return -EINVAL;                                    │
    │      }                                                      │
    │      // init is optional, others mandatory                  │
    │      ...                                                    │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    2. WRAPPER FUNCTIONS
    ┌─────────────────────────────────────────────────────────────┐
    │  // Never let users call ops directly                       │
    │  void eio_pin_set_status(eio_pin_t *pin, bool status) {     │
    │      BUG_ON(!pin);                       // Debug assert    │
    │      BUG_ON(!pin->ops);                                     │
    │      BUG_ON(!pin->ops->set_status);                         │
    │      pin->ops->set_status(pin, status);                     │
    │  }                                                          │
    │                                                              │
    │  // Or softer:                                              │
    │  int eio_pin_set_status(eio_pin_t *pin, bool status) {      │
    │      if (!pin || !pin->ops || !pin->ops->set_status)        │
    │          return -EINVAL;                                    │
    │      pin->ops->set_status(pin, status);                     │
    │      return 0;                                              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    3. CONST CORRECTNESS
    ┌─────────────────────────────────────────────────────────────┐
    │  // ops table should be const                               │
    │  const struct eio_pin_ops *ops;  // ✓                       │
    │                                                              │
    │  // Define ops as static const                              │
    │  static const eio_pin_ops_t gpio_ops = { ... };  // ✓       │
    └─────────────────────────────────────────────────────────────┘

    4. INITIALIZATION PATTERN
    ┌─────────────────────────────────────────────────────────────┐
    │  // Designated initializers catch missing fields            │
    │  static const eio_pin_ops_t gpio_ops = {                    │
    │      .init = gpio_init,                                     │
    │      .set_mode = gpio_set_mode,                             │
    │      .get_status = gpio_get_status,                         │
    │      .set_status = gpio_set_status,                         │
    │  };                                                         │
    │  // Missing fields are NULL (explicit)                      │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 强制操作：注册时验证必需 ops 存在
- 包装函数：不让用户直接调用 ops，提供检查的 API
- const 正确性：ops 表应为 const
- 初始化模式：使用指定初始化器，缺失字段为 NULL

---

## 6. Comparison to Other Patterns

```
+------------------------------------------------------------------+
|  PATTERN COMPARISON                                              |
+------------------------------------------------------------------+

    ┌────────────────┬────────────────────────────────────────────┐
    │ Pattern        │ Use When                                   │
    ├────────────────┼────────────────────────────────────────────┤
    │ enum + switch  │ - Finite, known set of types               │
    │                │ - Types change rarely                      │
    │                │ - Performance critical (branch predict)    │
    │                │ - Auditing all behavior in one place       │
    ├────────────────┼────────────────────────────────────────────┤
    │ ops-based      │ - Open set of implementations              │
    │ (your code)    │ - Implementations added independently      │
    │                │ - Need testing with mocks                  │
    │                │ - Driver/plugin architecture               │
    ├────────────────┼────────────────────────────────────────────┤
    │ C++ virtual    │ - Type hierarchy makes sense               │
    │                │ - Can use C++ compiler                     │
    │                │ - Want compile-time type checking          │
    │                │ - RTTI useful                              │
    ├────────────────┼────────────────────────────────────────────┤
    │ Function table │ - No per-instance state                    │
    │ (no instance)  │ - Pure strategy pattern                    │
    │                │ - Algorithm selection only                 │
    ├────────────────┼────────────────────────────────────────────┤
    │ Data-oriented  │ - Hot loop performance critical            │
    │                │ - Cache optimization needed                │
    │                │ - Batch processing                         │
    │                │ - Game engines, HPC                        │
    └────────────────┴────────────────────────────────────────────┘

    DECISION FLOWCHART:
    
    Is the set of types fixed and small?
    ├── YES → enum + switch
    └── NO
        │
        Is per-instance state needed?
        ├── NO → Simple function table
        └── YES
            │
            Is C++ available and appropriate?
            ├── YES → virtual functions
            └── NO → ops-based (your pattern)
```

**中文解释：**
- enum + switch：类型有限固定、性能关键、需要集中审计
- ops-based：开放实现集、独立添加、需要 mock 测试
- C++ virtual：类型层次有意义、需要编译时检查
- 函数表（无实例）：无每实例状态、纯策略模式
- 数据导向：热循环性能关键、缓存优化、批处理

---

## 7. Real Kernel Examples

### Mapping to Linux Kernel

```
+------------------------------------------------------------------+
|  KERNEL EXAMPLE 1: struct miscdevice                             |
+------------------------------------------------------------------+

    YOUR CODE                     KERNEL CODE (miscdevice.h)
    ─────────                     ─────────────────────────
    
    eio_pin_t                     struct miscdevice {
    ├── next                      │   struct list_head list;
    ├── ops (eio_pin_ops*)        │   const struct file_operations *fops;
    ├── user_data                 │   void *private; (via container_of)
    ├── name                      │   const char *name;
    └── mode, status              │   int minor;
                                  │   struct device *this_device;
                                  └── ...
                                  }

    Registration:
    eio_pin_register(&pin)        misc_register(&miscdev)

    Call site:
    pin->ops->set_status()        filp->f_op->read()

+------------------------------------------------------------------+
|  KERNEL EXAMPLE 2: struct gpio_chip                              |
+------------------------------------------------------------------+

    YOUR CODE                     KERNEL CODE
    ─────────                     ─────────────────────────
    
    eio_pin_t                     struct gpio_chip {
    ├── ops                       │   int (*request)(...)
    │   ├── init                  │   void (*free)(...)
    │   ├── set_mode              │   int (*direction_input)(...)
    │   ├── get_status            │   int (*get)(...)
    │   └── set_status            │   void (*set)(...)
    ├── user_data                 │   void *private (via container_of)
    └── name                      │   const char *label
                                  └── int base, ngpio
                                  }

    Registration:
    eio_pin_register()            gpiochip_add()

    Call site:
    pin->ops->get_status(pin)     chip->get(chip, offset)

+------------------------------------------------------------------+
|  KERNEL EXAMPLE 3: struct i2c_algorithm                          |
+------------------------------------------------------------------+

    Pattern:
    struct i2c_adapter {
        const struct i2c_algorithm *algo;  // ops table
        void *algo_data;                   // private data
        ...
    };

    struct i2c_algorithm {
        int (*master_xfer)(struct i2c_adapter *, ...);
        int (*smbus_xfer)(struct i2c_adapter *, ...);
        u32 (*functionality)(struct i2c_adapter *);
    };

    Call site (i2c-core.c):
    adap->algo->master_xfer(adap, msgs, num);
```

**中文解释：**
- miscdevice：list 对应 next，fops 对应 ops，misc_register 对应 register
- gpio_chip：回调直接嵌入结构（不分离 ops 结构），get/set 对应 get_status/set_status
- i2c_algorithm：algo 对应 ops，algo_data 对应 user_data

### What Varies vs What Stays Stable

```
+------------------------------------------------------------------+
|  STABILITY ANALYSIS                                              |
+------------------------------------------------------------------+

    STABLE (rarely changes):
    ┌─────────────────────────────────────────────────────────────┐
    │ - struct eio_pin_ops signature                              │
    │ - eio_pin_register() interface                              │
    │ - eio_pin_find() interface                                  │
    │ - Field layout of eio_pin_t                                 │
    │                                                             │
    │ This is the CONTRACT - changing it breaks all users         │
    └─────────────────────────────────────────────────────────────┘

    VARIES (per implementation):
    ┌─────────────────────────────────────────────────────────────┐
    │ - Concrete function implementations                         │
    │ - Private data structure                                    │
    │ - Hardware access methods                                   │
    │ - Error handling details                                    │
    │                                                              │
    │ Each implementation can evolve independently                │
    └─────────────────────────────────────────────────────────────┘

    KERNEL EVOLUTION EXAMPLE:
    
    v2.6: struct file_operations had 20 methods
    v3.0: Added new methods (fallocate, etc.)
    v5.x: Added more (copy_file_range, etc.)
    
    Old drivers: Still work (new fields are NULL)
    New drivers: Can use new features
    
    ← This is why NULL-checking is essential!
```

**中文解释：**
- 稳定部分：ops 签名、注册接口、查找接口、字段布局（这是契约）
- 变化部分：具体实现、私有数据、硬件访问、错误处理
- 内核演进：添加新方法，旧驱动仍工作（新字段为 NULL）

---

## 8. User-Space Application Guidance

### Application Domains

```
+------------------------------------------------------------------+
|  USER-SPACE APPLICATIONS                                         |
+------------------------------------------------------------------+

    1. HARDWARE ABSTRACTION LAYER (HAL)
    ┌─────────────────────────────────────────────────────────────┐
    │  struct hal_gpio_ops {                                      │
    │      int (*init)(struct hal_gpio *);                        │
    │      int (*set_direction)(struct hal_gpio *, int dir);      │
    │      int (*read)(struct hal_gpio *);                        │
    │      int (*write)(struct hal_gpio *, int value);            │
    │  };                                                         │
    │                                                              │
    │  // Implementations:                                        │
    │  - Linux sysfs backend                                      │
    │  - Memory-mapped register backend                           │
    │  - FTDI USB-GPIO backend                                    │
    │  - Simulation backend for testing                           │
    └─────────────────────────────────────────────────────────────┘

    2. PROTOCOL STACK
    ┌─────────────────────────────────────────────────────────────┐
    │  struct transport_ops {                                     │
    │      int (*open)(struct transport *, const char *addr);     │
    │      int (*send)(struct transport *, const void *, size_t); │
    │      int (*recv)(struct transport *, void *, size_t);       │
    │      void (*close)(struct transport *);                     │
    │  };                                                         │
    │                                                              │
    │  // Implementations:                                        │
    │  - TCP socket                                               │
    │  - UDP socket                                               │
    │  - Serial port                                              │
    │  - Unix domain socket                                       │
    │  - Mock for testing                                         │
    └─────────────────────────────────────────────────────────────┘

    3. PLUGIN SYSTEM
    ┌─────────────────────────────────────────────────────────────┐
    │  struct plugin_ops {                                        │
    │      const char *name;                                      │
    │      int (*init)(struct plugin *);                          │
    │      int (*process)(struct plugin *, void *data);           │
    │      void (*cleanup)(struct plugin *);                      │
    │  };                                                         │
    │                                                              │
    │  // Loaded from .so files at runtime                        │
    │  void *handle = dlopen("plugin.so", RTLD_NOW);              │
    │  struct plugin_ops *ops = dlsym(handle, "plugin_ops");      │
    └─────────────────────────────────────────────────────────────┘

    4. TEST DOUBLES / MOCKING
    ┌─────────────────────────────────────────────────────────────┐
    │  // Production code                                         │
    │  struct database_ops {                                      │
    │      int (*connect)(struct database *, const char *);       │
    │      int (*query)(struct database *, const char *, ...);    │
    │      void (*disconnect)(struct database *);                 │
    │  };                                                         │
    │                                                              │
    │  // Test mock                                               │
    │  static int mock_query(struct database *db, ...) {          │
    │      return expected_result;  // Controlled by test         │
    │  }                                                          │
    │  static const struct database_ops mock_db_ops = {           │
    │      .query = mock_query,                                   │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- HAL：抽象 GPIO，支持 sysfs、寄存器、USB、模拟后端
- 协议栈：抽象传输，支持 TCP、UDP、串口、Unix socket
- 插件系统：运行时从 .so 加载
- 测试 mock：注入可控返回值用于测试

### Naming Conventions

```
+------------------------------------------------------------------+
|  NAMING CONVENTIONS                                              |
+------------------------------------------------------------------+

    STRUCTURE NAMING:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct xxx           // Base object                        │
    │  struct xxx_ops       // Behavior table                     │
    │  struct xxx_priv      // Private implementation data        │
    │                                                              │
    │  Examples:                                                  │
    │  struct eio_pin       struct eio_pin_ops                    │
    │  struct transport     struct transport_ops                  │
    │  struct gpio_chip     // (kernel embeds ops directly)       │
    └─────────────────────────────────────────────────────────────┘

    FUNCTION NAMING:
    ┌─────────────────────────────────────────────────────────────┐
    │  xxx_register()       // Add to subsystem                   │
    │  xxx_unregister()     // Remove from subsystem              │
    │  xxx_find()           // Lookup by name/id                  │
    │  xxx_get()            // Get reference                      │
    │  xxx_put()            // Release reference                  │
    │                                                              │
    │  xxx_operation()      // Wrapper calling ops->operation     │
    │                                                              │
    │  impl_operation()     // Concrete implementation            │
    │  (e.g., gpio_set_status, i2c_set_status)                    │
    └─────────────────────────────────────────────────────────────┘

    FIELD NAMING:
    ┌─────────────────────────────────────────────────────────────┐
    │  ops         // Pointer to behavior table                   │
    │  priv        // Private data (void*)                        │
    │  user_data   // Alternative name for private data           │
    │  next        // Linked list pointer                         │
    │  list        // struct list_head (kernel style)             │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 结构命名：xxx（基础对象）、xxx_ops（行为表）、xxx_priv（私有数据）
- 函数命名：xxx_register/unregister/find/get/put、xxx_operation（包装器）
- 字段命名：ops（行为表指针）、priv/user_data（私有数据）、next/list（链表）

### Documentation Requirements

```
+------------------------------------------------------------------+
|  DOCUMENTATION REQUIREMENTS                                      |
+------------------------------------------------------------------+

    FOR EACH OPS FUNCTION:
    ┌─────────────────────────────────────────────────────────────┐
    │  /**                                                        │
    │   * set_status - Set the pin output status                  │
    │   * @me: The pin instance                                   │
    │   * @status: true for high, false for low                   │
    │   *                                                         │
    │   * Context: May be called from any context.                │
    │   *          Implementation must not sleep.                 │
    │   *                                                         │
    │   * Return: None. Errors should be logged internally.       │
    │   *                                                         │
    │   * This function is MANDATORY.                             │
    │   */                                                        │
    │  void (*set_status)(eio_pin_t * const me, bool status);     │
    └─────────────────────────────────────────────────────────────┘

    DOCUMENT:
    +----------------------------------------------------------+
    | 1. What each operation does                               |
    | 2. Calling context (process/interrupt, may sleep?)        |
    | 3. Mandatory vs optional                                  |
    | 4. Return value meaning                                   |
    | 5. Error handling expectations                            |
    | 6. Locking requirements                                   |
    +----------------------------------------------------------+
```

**中文解释：**
- 每个 ops 函数需文档：功能、调用上下文、是否强制、返回值、错误处理、锁要求

---

## 9. Minimal, Complete Example

```c
/*
 * eio_pin_demo.c - Complete demonstration of ops-based polymorphism
 * 
 * Compile: gcc -o eio_pin_demo eio_pin_demo.c
 * Run:     ./eio_pin_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/*========================================================================
 * INTERFACE DEFINITION (the contract)
 *========================================================================*/

typedef struct eio_pin eio_pin_t;

typedef struct eio_pin_ops {
    void (*init)(eio_pin_t *me);
    void (*set_mode)(eio_pin_t *me, uint8_t mode);
    bool (*get_status)(eio_pin_t *me);
    void (*set_status)(eio_pin_t *me, bool status);
} eio_pin_ops_t;

struct eio_pin {
    struct eio_pin *next;
    const eio_pin_ops_t *ops;
    void *user_data;
    const char *name;
    uint8_t mode;
    bool status;
};

/*========================================================================
 * FRAMEWORK (subsystem core)
 *========================================================================*/

static eio_pin_t *pin_list = NULL;

void eio_pin_register(eio_pin_t *pin)
{
    if (!pin || !pin->ops) {
        fprintf(stderr, "eio_pin_register: invalid pin\n");
        return;
    }
    
    pin->next = pin_list;
    pin_list = pin;
    
    printf("[Framework] Registered pin: %s\n", pin->name);
    
    if (pin->ops->init)
        pin->ops->init(pin);
}

eio_pin_t *eio_pin_find(const char *name)
{
    for (eio_pin_t *p = pin_list; p; p = p->next) {
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

/* Wrapper functions - hide ops dispatch from users */
void eio_pin_set_status(eio_pin_t *pin, bool status)
{
    if (!pin || !pin->ops || !pin->ops->set_status) {
        fprintf(stderr, "eio_pin_set_status: invalid pin or ops\n");
        return;
    }
    pin->ops->set_status(pin, status);
}

bool eio_pin_get_status(eio_pin_t *pin)
{
    if (!pin || !pin->ops || !pin->ops->get_status) {
        fprintf(stderr, "eio_pin_get_status: invalid pin or ops\n");
        return false;
    }
    return pin->ops->get_status(pin);
}

/*========================================================================
 * IMPLEMENTATION 1: GPIO (memory-mapped hardware simulation)
 *========================================================================*/

struct gpio_priv {
    uint32_t base_addr;
    int pin_number;
};

static void gpio_init(eio_pin_t *me)
{
    struct gpio_priv *priv = me->user_data;
    printf("[GPIO] Init: addr=0x%08X, pin=%d\n", 
           priv->base_addr, priv->pin_number);
}

static void gpio_set_mode(eio_pin_t *me, uint8_t mode)
{
    struct gpio_priv *priv = me->user_data;
    me->mode = mode;
    printf("[GPIO] Set mode: pin=%d, mode=%d\n", priv->pin_number, mode);
}

static bool gpio_get_status(eio_pin_t *me)
{
    struct gpio_priv *priv = me->user_data;
    printf("[GPIO] Get status: pin=%d -> %s\n", 
           priv->pin_number, me->status ? "HIGH" : "LOW");
    return me->status;
}

static void gpio_set_status(eio_pin_t *me, bool status)
{
    struct gpio_priv *priv = me->user_data;
    me->status = status;
    printf("[GPIO] Set status: pin=%d -> %s (write to 0x%08X)\n",
           priv->pin_number, status ? "HIGH" : "LOW", priv->base_addr);
}

static const eio_pin_ops_t gpio_ops = {
    .init = gpio_init,
    .set_mode = gpio_set_mode,
    .get_status = gpio_get_status,
    .set_status = gpio_set_status,
};

/*========================================================================
 * IMPLEMENTATION 2: I2C Expander (different transport)
 *========================================================================*/

struct i2c_priv {
    int bus_number;
    uint8_t device_addr;
    uint8_t pin_mask;
};

static void i2c_init(eio_pin_t *me)
{
    struct i2c_priv *priv = me->user_data;
    printf("[I2C] Init: bus=%d, addr=0x%02X, mask=0x%02X\n",
           priv->bus_number, priv->device_addr, priv->pin_mask);
}

static void i2c_set_mode(eio_pin_t *me, uint8_t mode)
{
    struct i2c_priv *priv = me->user_data;
    me->mode = mode;
    printf("[I2C] Set mode: bus=%d addr=0x%02X mode=%d (send I2C command)\n",
           priv->bus_number, priv->device_addr, mode);
}

static bool i2c_get_status(eio_pin_t *me)
{
    struct i2c_priv *priv = me->user_data;
    printf("[I2C] Get status: bus=%d addr=0x%02X (read I2C register) -> %s\n",
           priv->bus_number, priv->device_addr, me->status ? "HIGH" : "LOW");
    return me->status;
}

static void i2c_set_status(eio_pin_t *me, bool status)
{
    struct i2c_priv *priv = me->user_data;
    me->status = status;
    printf("[I2C] Set status: bus=%d addr=0x%02X -> %s (write I2C register)\n",
           priv->bus_number, priv->device_addr, status ? "HIGH" : "LOW");
}

static const eio_pin_ops_t i2c_ops = {
    .init = i2c_init,
    .set_mode = i2c_set_mode,
    .get_status = i2c_get_status,
    .set_status = i2c_set_status,
};

/*========================================================================
 * MAIN: Demonstrate polymorphic behavior
 *========================================================================*/

int main(void)
{
    /* Create GPIO pin instance */
    static struct gpio_priv gpio_priv = {
        .base_addr = 0x40020000,
        .pin_number = 5,
    };
    static eio_pin_t gpio_pin = {
        .ops = &gpio_ops,
        .user_data = &gpio_priv,
        .name = "LED_STATUS",
    };
    
    /* Create I2C expander pin instance */
    static struct i2c_priv i2c_priv = {
        .bus_number = 1,
        .device_addr = 0x20,
        .pin_mask = 0x01,
    };
    static eio_pin_t i2c_pin = {
        .ops = &i2c_ops,
        .user_data = &i2c_priv,
        .name = "RELAY_CONTROL",
    };
    
    /* Register both pins */
    printf("=== REGISTRATION ===\n");
    eio_pin_register(&gpio_pin);
    eio_pin_register(&i2c_pin);
    
    /* Demonstrate polymorphism: same interface, different behavior */
    printf("\n=== POLYMORPHIC OPERATIONS ===\n");
    
    eio_pin_t *led = eio_pin_find("LED_STATUS");
    eio_pin_t *relay = eio_pin_find("RELAY_CONTROL");
    
    if (led && relay) {
        printf("\n--- Setting both to HIGH ---\n");
        eio_pin_set_status(led, true);    /* Calls gpio_set_status */
        eio_pin_set_status(relay, true);  /* Calls i2c_set_status */
        
        printf("\n--- Reading both ---\n");
        bool led_status = eio_pin_get_status(led);
        bool relay_status = eio_pin_get_status(relay);
        
        printf("\n--- Results ---\n");
        printf("LED: %s, Relay: %s\n",
               led_status ? "ON" : "OFF",
               relay_status ? "ON" : "OFF");
    }
    
    printf("\n=== DEMO COMPLETE ===\n");
    return 0;
}
```

**Output:**
```
=== REGISTRATION ===
[Framework] Registered pin: LED_STATUS
[GPIO] Init: addr=0x40020000, pin=5
[Framework] Registered pin: RELAY_CONTROL
[I2C] Init: bus=1, addr=0x20, mask=0x01

=== POLYMORPHIC OPERATIONS ===

--- Setting both to HIGH ---
[GPIO] Set status: pin=5 -> HIGH (write to 0x40020000)
[I2C] Set status: bus=1 addr=0x20 -> HIGH (write I2C register)

--- Reading both ---
[GPIO] Get status: pin=5 -> HIGH
[I2C] Get status: bus=1 addr=0x20 (read I2C register) -> HIGH

--- Results ---
LED: ON, Relay: ON

=== DEMO COMPLETE ===
```

**中文解释：**
- 完整示例：定义接口、框架、两个实现（GPIO 和 I2C）、主函数演示
- 多态效果：同一 `eio_pin_set_status` 调用，不同实现执行不同硬件操作

---

## 10. Mental Model & Recognition Heuristics

### Recognition Checklist

```
+------------------------------------------------------------------+
|  HOW TO RECOGNIZE THIS PATTERN                                   |
+------------------------------------------------------------------+

    STRUCTURAL SIGNALS:
    ┌─────────────────────────────────────────────────────────────┐
    │ ✓ Struct with pointer to another struct of function ptrs    │
    │     struct xxx { const struct xxx_ops *ops; ... }           │
    │                                                              │
    │ ✓ Ops struct contains only function pointers                │
    │     struct xxx_ops { int (*method1)(...); ... }             │
    │                                                              │
    │ ✓ First parameter of each function is pointer to base      │
    │     int (*read)(struct xxx *me, ...);                       │
    │                                                              │
    │ ✓ void* or container_of for private data                   │
    │     void *user_data; or container_of(me, priv_type, base)   │
    │                                                              │
    │ ✓ Registration function that adds to global list           │
    │     int xxx_register(struct xxx *obj);                      │
    └─────────────────────────────────────────────────────────────┘

    BEHAVIORAL SIGNALS:
    ┌─────────────────────────────────────────────────────────────┐
    │ ✓ Multiple implementations of same ops struct               │
    │ ✓ Call sites use obj->ops->method(obj, ...)                 │
    │ ✓ Implementations are in separate files/modules             │
    │ ✓ High-level code doesn't know concrete types               │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 结构信号：ops 指针、函数指针结构、第一参数是 me、user_data 或 container_of、注册函数
- 行为信号：多实现、调用点用 obj->ops->method、实现在不同文件、高层不知具体类型

### Red Flags (Misuse)

```
+------------------------------------------------------------------+
|  RED FLAGS INDICATING MISUSE                                     |
+------------------------------------------------------------------+

    ⚠️ SINGLE IMPLEMENTATION
    +----------------------------------------------------------+
    | If only one implementation ever exists, the indirection   |
    | is unnecessary overhead.                                  |
    | → Use direct calls instead                                |
    +----------------------------------------------------------+

    ⚠️ CHANGING OPS AT RUNTIME
    +----------------------------------------------------------+
    | obj->ops = different_ops;  // After initialization        |
    |                                                           |
    | This is usually a bug. Type should be fixed at creation.  |
    | If you need runtime behavior change, use state machine.   |
    +----------------------------------------------------------+

    ⚠️ CASTING AWAY CONST
    +----------------------------------------------------------+
    | ((struct xxx_ops*)obj->ops)->method = new_func;           |
    |                                                           |
    | Violates the contract. Ops should be immutable.           |
    +----------------------------------------------------------+

    ⚠️ NO NULL CHECKS
    +----------------------------------------------------------+
    | obj->ops->method(obj);  // What if ops is NULL?           |
    |                                                           |
    | Always check in wrapper functions.                        |
    +----------------------------------------------------------+

    ⚠️ MIXING INSTANCE DATA AND BEHAVIOR
    +----------------------------------------------------------+
    | struct ops { int value; int (*get)(void); }               |
    |                                                           |
    | Ops should be pure behavior (function pointers only).     |
    | Instance data belongs in the base struct.                 |
    +----------------------------------------------------------+

    ⚠️ TOO MANY OPTIONAL METHODS
    +----------------------------------------------------------+
    | If most implementations leave most methods NULL,          |
    | the interface is too broad. Split into focused interfaces.|
    +----------------------------------------------------------+
```

**中文解释：**
- 红旗警告：
  - 只有一个实现 → 直接调用
  - 运行时更改 ops → 用状态机
  - 转换掉 const → 违反契约
  - 无 NULL 检查 → 必须检查
  - 混合数据和行为 → ops 应只有函数指针
  - 太多可选方法 → 拆分接口

### Decision Rule of Thumb

```
+------------------------------------------------------------------+
|  WHEN TO INTRODUCE THIS PATTERN                                  |
+------------------------------------------------------------------+

    INTRODUCE IF:
    ┌─────────────────────────────────────────────────────────────┐
    │ 1. You have (or expect) multiple implementations            │
    │    - Different hardware backends                            │
    │    - Different protocol variants                            │
    │    - Mock implementations for testing                       │
    │                                                             │
    │ 2. Implementations should be decoupled from core logic      │
    │    - Developed by different teams                           │
    │    - Compiled separately                                    │
    │    - Added without modifying core                           │
    │                                                             │
    │ 3. The operations are NOT performance-critical              │
    │    - I/O operations (already slow)                          │
    │    - Initialization/configuration                           │
    │    - Infrequent operations                                  │
    └─────────────────────────────────────────────────────────────┘

    AVOID IF:
    ┌─────────────────────────────────────────────────────────────┐
    │ 1. Only one implementation will ever exist                  │
    │                                                             │
    │ 2. Hot inner loop (cache miss from indirection hurts)       │
    │                                                             │
    │ 3. Simple, stable interface that won't grow                 │
    │                                                             │
    │ 4. Debugging complexity outweighs benefits                  │
    └─────────────────────────────────────────────────────────────┘

    ENGINEERING JUDGMENT:
    +----------------------------------------------------------+
    | "Would I benefit from testing this with a mock?"          |
    |   YES → Pattern is likely appropriate                     |
    |   NO  → Probably overkill                                 |
    +----------------------------------------------------------+
```

**中文解释：**
- 引入条件：多实现、解耦需求、非性能关键
- 避免条件：唯一实现、热内循环、简单稳定接口、调试复杂性超过收益
- 工程判断："测试时需要 mock 吗？" 是 → 适合，否 → 过度

---

## Summary

```
+------------------------------------------------------------------+
|  KEY TAKEAWAYS                                                   |
+------------------------------------------------------------------+

    1. CORE MECHANISM
       - Struct with pointer to function pointer table
       - Explicit "this" (me) pointer passed to each function
       - Private data via void* or container_of

    2. ARCHITECTURAL BENEFITS
       - Open/closed principle: extend without modifying
       - Inversion of control: framework calls implementation
       - Decoupling: high-level doesn't know low-level details

    3. SAFETY REQUIREMENTS
       - NULL-check ops and function pointers
       - Respect lifetime: user_data must outlive registration
       - Use const for ops tables

    4. WHEN TO USE
       - Multiple implementations expected
       - Testing with mocks needed
       - Plugin-like architecture
       - I/O-bound operations (not hot loops)

    5. KERNEL PARALLELS
       - file_operations, net_device_ops, gpio_chip
       - Same pattern, proven at scale
```

**中文解释：**
- 核心机制：函数指针表指针、显式 "this"、私有数据
- 架构优势：开闭原则、控制反转、解耦
- 安全要求：NULL 检查、生命周期、const
- 使用时机：多实现、mock 测试、插件架构、I/O 密集
- 内核对应：file_operations、net_device_ops、gpio_chip

---

## Appendix A: Ops-Based Pattern Code Skeleton

This appendix provides a complete code skeleton for the ops-based pattern, with detailed explanations for each key line.

### A.1 Complete Pattern Skeleton

```c
/*============================================================================
 * OPS-BASED POLYMORPHISM PATTERN — COMPLETE SKELETON
 *============================================================================*/

#include <stddef.h>      /* for offsetof */
#include <stdbool.h>

/*----------------------------------------------------------------------------
 * PART 1: FORWARD DECLARATIONS
 *----------------------------------------------------------------------------*/

/* Forward declare the base object type.
 * This allows ops functions to take pointer to base object. */
typedef struct my_object my_object_t;

/*----------------------------------------------------------------------------
 * PART 2: OPS TABLE DEFINITION (The "Virtual Table")
 *----------------------------------------------------------------------------*/

/**
 * struct my_object_ops - Behavior table for my_object
 * @init:       Initialize the object (OPTIONAL)
 * @destroy:    Clean up the object (OPTIONAL)  
 * @operation1: First polymorphic operation (MANDATORY)
 * @operation2: Second polymorphic operation (MANDATORY)
 *
 * This struct defines the "interface" or "contract".
 * Each implementation provides a static const instance of this struct.
 */
typedef struct my_object_ops {
    /*--------------------------------------------------------------------
     * LIFECYCLE CALLBACKS (typically optional)
     *--------------------------------------------------------------------*/
    
    /* Called during registration/initialization.
     * @me: pointer to base object
     * Return: 0 on success, negative errno on failure */
    int (*init)(my_object_t *me);
    
    /* Called during unregistration/cleanup.
     * @me: pointer to base object */
    void (*destroy)(my_object_t *me);
    
    /*--------------------------------------------------------------------
     * CORE OPERATIONS (typically mandatory)
     *--------------------------------------------------------------------*/
    
    /* First polymorphic operation.
     * @me:    pointer to base object (explicit "this")
     * @param: operation-specific parameter
     * Return: operation result */
    int (*operation1)(my_object_t *me, int param);
    
    /* Second polymorphic operation.
     * @me: pointer to base object (explicit "this")
     * Return: boolean result */
    bool (*operation2)(my_object_t *me);
    
} my_object_ops_t;

/*----------------------------------------------------------------------------
 * PART 3: BASE OBJECT DEFINITION
 *----------------------------------------------------------------------------*/

/**
 * struct my_object - Base object structure
 * @next:      Linked list pointer for registration (subsystem management)
 * @ops:       Pointer to behavior table (POLYMORPHISM ENABLER)
 * @priv:      Pointer to implementation-specific private data
 * @name:      Human-readable identifier
 * @flags:     Common state/configuration flags
 *
 * This is the "base class" equivalent. All instances share this layout,
 * but each implementation can have different ops and private data.
 */
struct my_object {
    /*--------------------------------------------------------------------
     * SUBSYSTEM MANAGEMENT (for registration/lookup)
     *--------------------------------------------------------------------*/
    
    struct my_object *next;         /* Intrusive linked list pointer.
                                     * Enables O(n) lookup without allocation.
                                     * Kernel uses struct list_head instead. */
    
    /*--------------------------------------------------------------------
     * POLYMORPHISM ENABLERS (the core of the pattern)
     *--------------------------------------------------------------------*/
    
    const struct my_object_ops *ops; /* Pointer to behavior table.
                                      * const: behavior is immutable after init.
                                      * Different instances can point to
                                      * different ops tables → polymorphism! */
    
    void *priv;                      /* Type-erased pointer to private data.
                                      * Alternative: use container_of().
                                      * Only implementation knows actual type. */
    
    /*--------------------------------------------------------------------
     * COMMON INSTANCE DATA (shared across all implementations)
     *--------------------------------------------------------------------*/
    
    const char *name;               /* Human-readable name for lookup/debug */
    unsigned int flags;             /* Common status/config flags */
    /* ... more common fields as needed ... */
};

/*----------------------------------------------------------------------------
 * PART 4: FRAMEWORK CORE (Subsystem Management)
 *----------------------------------------------------------------------------*/

/* Global registry head - the subsystem owns this */
static my_object_t *object_list = NULL;

/**
 * my_object_register - Register an object with the subsystem
 * @obj: object to register (caller owns memory)
 *
 * Adds object to global list and calls init callback if provided.
 * Return: 0 on success, negative errno on failure.
 */
int my_object_register(my_object_t *obj)
{
    /* Validate required fields */
    if (!obj || !obj->ops)
        return -EINVAL;                 /* Invalid argument */
    
    /* Validate mandatory callbacks */
    if (!obj->ops->operation1 || !obj->ops->operation2)
        return -EINVAL;                 /* Contract violation */
    
    /* Add to head of list (O(1) insertion) */
    obj->next = object_list;
    object_list = obj;
    
    /* Call implementation's init if provided (control inversion!) */
    if (obj->ops->init) {
        int ret = obj->ops->init(obj);  /* Polymorphic call */
        if (ret < 0) {
            /* Rollback: remove from list */
            object_list = obj->next;
            return ret;
        }
    }
    
    return 0;
}

/**
 * my_object_unregister - Remove an object from the subsystem
 * @obj: object to unregister
 *
 * Removes object from global list and calls destroy callback.
 */
void my_object_unregister(my_object_t *obj)
{
    my_object_t **pp;
    
    if (!obj)
        return;
    
    /* Call implementation's destroy if provided */
    if (obj->ops && obj->ops->destroy)
        obj->ops->destroy(obj);         /* Polymorphic call */
    
    /* Remove from list */
    for (pp = &object_list; *pp; pp = &(*pp)->next) {
        if (*pp == obj) {
            *pp = obj->next;
            break;
        }
    }
    
    obj->next = NULL;                   /* Clear dangling pointer */
}

/**
 * my_object_find - Look up an object by name
 * @name: name to search for
 *
 * Return: pointer to object, or NULL if not found.
 */
my_object_t *my_object_find(const char *name)
{
    my_object_t *obj;
    
    for (obj = object_list; obj; obj = obj->next) {
        if (strcmp(obj->name, name) == 0)
            return obj;                 /* Found! */
    }
    
    return NULL;                        /* Not found */
}

/*----------------------------------------------------------------------------
 * PART 5: WRAPPER FUNCTIONS (Hide ops dispatch from consumers)
 *----------------------------------------------------------------------------*/

/**
 * my_object_operation1 - Execute operation1 on object
 * @obj:   target object
 * @param: operation parameter
 *
 * This wrapper:
 * 1. Validates inputs (defensive programming)
 * 2. Hides the ops-> indirection from callers
 * 3. Provides a stable API even if ops layout changes
 *
 * Return: operation result, or -EINVAL on error.
 */
int my_object_operation1(my_object_t *obj, int param)
{
    /* Defensive null checks */
    if (!obj || !obj->ops || !obj->ops->operation1)
        return -EINVAL;
    
    /* Dispatch to concrete implementation */
    return obj->ops->operation1(obj, param);
}

/**
 * my_object_operation2 - Execute operation2 on object
 * @obj: target object
 *
 * Return: operation result, or false on error.
 */
bool my_object_operation2(my_object_t *obj)
{
    if (!obj || !obj->ops || !obj->ops->operation2)
        return false;
    
    return obj->ops->operation2(obj);
}

/*----------------------------------------------------------------------------
 * PART 6: CONCRETE IMPLEMENTATION EXAMPLE
 *----------------------------------------------------------------------------*/

/* Private data structure - only this implementation knows about it */
struct impl_a_private {
    int internal_state;
    void *hardware_handle;
    /* ... implementation-specific fields ... */
};

/* Implementation functions */
static int impl_a_init(my_object_t *me)
{
    struct impl_a_private *priv = me->priv;
    priv->internal_state = 0;
    /* ... hardware initialization ... */
    return 0;
}

static void impl_a_destroy(my_object_t *me)
{
    struct impl_a_private *priv = me->priv;
    /* ... hardware cleanup ... */
    priv->internal_state = -1;
}

static int impl_a_operation1(my_object_t *me, int param)
{
    struct impl_a_private *priv = me->priv;
    /* Access private data and perform operation */
    priv->internal_state += param;
    return priv->internal_state;
}

static bool impl_a_operation2(my_object_t *me)
{
    struct impl_a_private *priv = me->priv;
    return priv->internal_state > 0;
}

/* Static const ops table - shared by all instances of this implementation */
static const my_object_ops_t impl_a_ops = {
    .init       = impl_a_init,
    .destroy    = impl_a_destroy,
    .operation1 = impl_a_operation1,
    .operation2 = impl_a_operation2,
};

/* Instance creation example */
static struct impl_a_private my_priv_data = { 0 };
static my_object_t my_object_instance = {
    .ops  = &impl_a_ops,       /* Point to our ops table */
    .priv = &my_priv_data,     /* Point to our private data */
    .name = "my_device",
};
```

### A.2 Skeleton Diagram

```
+------------------------------------------------------------------+
|  COMPLETE OPS-BASED PATTERN STRUCTURE                            |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │  SUBSYSTEM CORE (Framework)                                 │
    ├─────────────────────────────────────────────────────────────┤
    │                                                             │
    │  object_list ──▶ [obj_1] ──▶ [obj_2] ──▶ [obj_3] ──▶ NULL  │
    │       │                                                     │
    │       │  my_object_register()   ← Add to list               │
    │       │  my_object_unregister() ← Remove from list          │
    │       │  my_object_find()       ← Lookup by name            │
    │       │  my_object_operation1() ← Wrapper function          │
    │       │  my_object_operation2() ← Wrapper function          │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
                            │
                            │ contains
                            ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  BASE OBJECT (my_object_t)                                  │
    ├─────────────────────────────────────────────────────────────┤
    │                                                             │
    │  struct my_object {                                         │
    │      struct my_object *next;    ← Linked list (subsystem)   │
    │      const my_object_ops_t *ops;← ★ POLYMORPHISM KEY        │
    │      void *priv;                ← Private data pointer      │
    │      const char *name;          ← Common data               │
    │      unsigned int flags;        ← Common data               │
    │  };                                                         │
    │                                                             │
    └──────────────────┬──────────────────┬───────────────────────┘
                       │                  │
              ┌────────┘                  └────────┐
              │ ops points to                      │ priv points to
              ▼                                    ▼
    ┌─────────────────────────┐      ┌─────────────────────────┐
    │  OPS TABLE (static const│      │  PRIVATE DATA           │
    │   my_object_ops_t)      │      │  (implementation-       │
    ├─────────────────────────┤      │   specific struct)      │
    │                         │      ├─────────────────────────┤
    │  .init ────────▶ fn_ptr │      │  .internal_state        │
    │  .destroy ─────▶ fn_ptr │      │  .hardware_handle       │
    │  .operation1 ──▶ fn_ptr │      │  ... other fields ...   │
    │  .operation2 ──▶ fn_ptr │      │                         │
    │                         │      │                         │
    └─────────────────────────┘      └─────────────────────────┘
              │                                    │
              │ Shared by all instances            │ Unique per instance
              │ of same "type"                     │
              └────────────────────────────────────┘


    CALL FLOW:
    
    Consumer                Framework              Implementation
    ────────                ─────────              ──────────────
    
    my_object_operation1(obj, 42)
            │
            ▼
        [null checks]
            │
            ▼
        obj->ops->operation1(obj, 42)
                        │
                        │ (indirect call via function pointer)
                        ▼
                    impl_a_operation1(obj, 42)
                            │
                            ▼
                    priv = obj->priv
                    priv->internal_state += 42
                    return priv->internal_state
```

**中文解释：**
- **基础对象（my_object_t）**：包含 next（链表）、ops（行为表指针）、priv（私有数据指针）、公共字段
- **Ops 表（my_object_ops_t）**：静态 const，包含函数指针，同类型所有实例共享
- **私有数据**：实现特定，每个实例独有
- **框架核心**：管理注册表，提供查找、注册、注销功能
- **包装函数**：隐藏 ops-> 间接调用，提供稳定 API

### A.3 Key Lines Explanation

| Line | Explanation |
|------|-------------|
| `typedef struct my_object my_object_t;` | Forward declaration allows ops to reference base type |
| `const struct my_object_ops *ops;` | `const` ensures behavior is immutable after creation |
| `void *priv;` | Type erasure - only implementation knows actual type |
| `struct my_object *next;` | Intrusive linked list for O(1) insertion, no allocation |
| `static const my_object_ops_t impl_a_ops = {...};` | Static const allows compiler optimization, sharing |
| `obj->ops->operation1(obj, param);` | The polymorphic call - same syntax, different behavior |
| `struct impl_a_private *priv = me->priv;` | Type recovery - cast void* to known type |

---

## Appendix B: 5 Linux Kernel Modules Analysis (Top-to-Down)

This appendix provides detailed analysis of 5 representative Linux kernel v3.2 modules that implement the ops-based pattern.

### B.1 Module 1: miscdevice (Character Device Framework)

```
+------------------------------------------------------------------+
|  MISCDEVICE ARCHITECTURE                                         |
+------------------------------------------------------------------+

    USER SPACE
    ┌─────────────────────────────────────────────────────────────┐
    │  open("/dev/mydevice", O_RDWR)                              │
    │  read(fd, buf, len)                                         │
    │  write(fd, buf, len)                                        │
    │  close(fd)                                                  │
    └────────────────────────────┬────────────────────────────────┘
                                 │ system call
                                 ▼
    KERNEL VFS LAYER
    ┌─────────────────────────────────────────────────────────────┐
    │  sys_open() → misc_open()                                   │
    │  sys_read() → filp->f_op->read()  ← POLYMORPHIC CALL        │
    │  sys_write()→ filp->f_op->write() ← POLYMORPHIC CALL        │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    MISCDEVICE CORE (drivers/char/misc.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  static LIST_HEAD(misc_list);  ← Global registry            │
    │  static DEFINE_MUTEX(misc_mtx);← Concurrency protection     │
    │                                                             │
    │  misc_register(struct miscdevice *misc)                     │
    │  misc_deregister(struct miscdevice *misc)                   │
    │  misc_open() → dispatches to misc->fops->open()             │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    CONCRETE DRIVER (e.g., drivers/char/mem.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  static const struct file_operations mem_fops = {           │
    │      .read  = read_mem,                                     │
    │      .write = write_mem,                                    │
    │      .open  = open_mem,                                     │
    │  };                                                         │
    │  static struct miscdevice mem_dev = {                       │
    │      .minor = MISC_DYNAMIC_MINOR,                           │
    │      .name  = "mem",                                        │
    │      .fops  = &mem_fops,  ← Points to behavior table        │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 用户空间通过系统调用访问设备
- VFS 层将调用分发到 filp->f_op->xxx()（多态调用）
- miscdevice 核心维护全局列表，提供 register/deregister
- 具体驱动提供 file_operations 和 miscdevice 结构

**Key Structure Definitions:**

```c
/* include/linux/miscdevice.h - Lines 49-58 */
struct miscdevice  {
    int minor;                            /* Device minor number */
    const char *name;                     /* Device name in /dev/ */
    const struct file_operations *fops;   /* ★ OPS TABLE POINTER */
    struct list_head list;                /* ★ LINKED LIST NODE */
    struct device *parent;                /* Parent device */
    struct device *this_device;           /* This device */
    const char *nodename;                 /* Node name override */
    mode_t mode;                          /* File permissions */
};

/* include/linux/fs.h - Lines 1583-1600 (partial) */
struct file_operations {
    struct module *owner;                 /* Module ownership */
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    /* ... 20+ more operations ... */
};
```

**Registration Flow (Top-to-Down):**

```c
/* drivers/char/misc.c - Lines 184-230 */
int misc_register(struct miscdevice *misc)
{
    struct miscdevice *c;
    dev_t dev;
    int err = 0;

    /* 1. Initialize list node (for intrusive linked list) */
    INIT_LIST_HEAD(&misc->list);

    /* 2. Lock the registry (concurrency protection) */
    mutex_lock(&misc_mtx);
    
    /* 3. Check for duplicate minor numbers */
    list_for_each_entry(c, &misc_list, list) {
        if (c->minor == misc->minor) {
            mutex_unlock(&misc_mtx);
            return -EBUSY;              /* Already registered */
        }
    }

    /* 4. Assign dynamic minor if requested */
    if (misc->minor == MISC_DYNAMIC_MINOR) {
        int i = find_first_zero_bit(misc_minors, DYNAMIC_MINORS);
        if (i >= DYNAMIC_MINORS) {
            mutex_unlock(&misc_mtx);
            return -EBUSY;              /* No available minors */
        }
        misc->minor = DYNAMIC_MINORS - i - 1;
        set_bit(i, misc_minors);
    }

    /* 5. Create device node in sysfs */
    dev = MKDEV(MISC_MAJOR, misc->minor);
    misc->this_device = device_create(misc_class, misc->parent, dev,
                                      misc, "%s", misc->name);
    if (IS_ERR(misc->this_device)) {
        /* Handle error... */
        goto out;
    }

    /* 6. Add to global list (O(1) insertion at head) */
    list_add(&misc->list, &misc_list);      /* ★ REGISTRATION */
    
out:
    mutex_unlock(&misc_mtx);
    return err;
}
```

**Dispatch Flow (Polymorphic Call):**

```c
/* drivers/char/misc.c - Lines 112-158 */
static int misc_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    struct miscdevice *c;
    int err = -ENODEV;
    const struct file_operations *old_fops, *new_fops = NULL;

    mutex_lock(&misc_mtx);
    
    /* 1. Find matching device by minor number */
    list_for_each_entry(c, &misc_list, list) {
        if (c->minor == minor) {
            new_fops = fops_get(c->fops);  /* ★ GET OPS TABLE */
            break;
        }
    }
        
    if (!new_fops) {
        /* Try to load module on demand... */
        goto fail;
    }

    err = 0;
    old_fops = file->f_op;
    file->f_op = new_fops;                  /* ★ INSTALL OPS */
    
    /* 2. Call device-specific open (POLYMORPHIC CALL) */
    if (file->f_op->open) {
        file->private_data = c;
        err = file->f_op->open(inode, file);  /* ★ DISPATCH */
        if (err) {
            fops_put(file->f_op);
            file->f_op = fops_get(old_fops);
        }
    }
    fops_put(old_fops);
    
fail:
    mutex_unlock(&misc_mtx);
    return err;
}
```

**中文解释：**
- `misc_register()`：初始化链表节点、加锁、检查重复、分配 minor、创建设备节点、添加到全局列表
- `misc_open()`：按 minor 查找设备、获取 fops 表、安装到 file->f_op、调用设备特定 open

---

### B.2 Module 2: net_device (Network Device Framework)

```
+------------------------------------------------------------------+
|  NET_DEVICE ARCHITECTURE                                         |
+------------------------------------------------------------------+

    NETWORK STACK
    ┌─────────────────────────────────────────────────────────────┐
    │  TCP/UDP Layer                                              │
    │       │                                                     │
    │       ▼                                                     │
    │  IP Layer                                                   │
    │       │                                                     │
    │       ▼                                                     │
    │  dev_queue_xmit(skb)                                        │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    NETWORK CORE (net/core/dev.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  struct list_head dev_base_head;  ← Global device list      │
    │                                                             │
    │  register_netdev()                                          │
    │  unregister_netdev()                                        │
    │  dev_queue_xmit() → ops->ndo_start_xmit()  ← POLYMORPHIC    │
    │  dev_open()       → ops->ndo_open()        ← POLYMORPHIC    │
    │  dev_close()      → ops->ndo_stop()        ← POLYMORPHIC    │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    NET DEVICE DRIVER (e.g., drivers/net/ethernet/intel/e1000e)
    ┌─────────────────────────────────────────────────────────────┐
    │  static const struct net_device_ops e1000e_netdev_ops = {   │
    │      .ndo_open       = e1000_open,                          │
    │      .ndo_stop       = e1000_close,                         │
    │      .ndo_start_xmit = e1000_xmit_frame,  ← TX entry point  │
    │      .ndo_get_stats64= e1000e_get_stats64,                  │
    │      .ndo_set_rx_mode= e1000_set_multi,                     │
    │      /* ... 15+ more operations ... */                      │
    │  };                                                         │
    │                                                             │
    │  /* In probe function: */                                   │
    │  netdev->netdev_ops = &e1000e_netdev_ops;  ← INSTALL OPS    │
    │  register_netdev(netdev);                  ← REGISTER       │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 网络栈通过 dev_queue_xmit() 发送数据包
- 网络核心维护全局设备列表，分发调用到 ops->ndo_xxx()
- 设备驱动提供 net_device_ops，在 probe 时安装并注册

**Key Structure Definitions:**

```c
/* include/linux/netdevice.h - Lines 859-951 (partial) */
struct net_device_ops {
    /* Lifecycle */
    int  (*ndo_init)(struct net_device *dev);
    void (*ndo_uninit)(struct net_device *dev);
    int  (*ndo_open)(struct net_device *dev);      /* ifconfig up */
    int  (*ndo_stop)(struct net_device *dev);      /* ifconfig down */
    
    /* Data path - CRITICAL for performance */
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);
    
    /* Configuration */
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    
    /* Statistics */
    struct rtnl_link_stats64* (*ndo_get_stats64)(struct net_device *dev,
                                                  struct rtnl_link_stats64 *storage);
    /* ... 30+ more operations ... */
};

/* include/linux/netdevice.h - Lines 963-1050 (partial) */
struct net_device {
    char name[IFNAMSIZ];                   /* Interface name (eth0) */
    
    struct list_head dev_list;             /* ★ LINKED LIST NODE */
    
    unsigned long state;                   /* Device state flags */
    
    const struct net_device_ops *netdev_ops; /* ★ OPS TABLE POINTER */
    
    void *priv;                            /* Driver private data */
    /* ... 100+ more fields ... */
};
```

**Real Driver Example (e1000e):**

```c
/* drivers/net/ethernet/intel/e1000e/netdev.c - Lines 5882-5900 */
static const struct net_device_ops e1000e_netdev_ops = {
    .ndo_open           = e1000_open,
    .ndo_stop           = e1000_close,
    .ndo_start_xmit     = e1000_xmit_frame,     /* ★ TX function */
    .ndo_get_stats64    = e1000e_get_stats64,
    .ndo_set_rx_mode    = e1000_set_multi,
    .ndo_set_mac_address= e1000_set_mac,
    .ndo_change_mtu     = e1000_change_mtu,
    .ndo_do_ioctl       = e1000_ioctl,
    .ndo_tx_timeout     = e1000_tx_timeout,
    .ndo_validate_addr  = eth_validate_addr,
    .ndo_vlan_rx_add_vid= e1000_vlan_rx_add_vid,
    .ndo_vlan_rx_kill_vid= e1000_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
    .ndo_poll_controller= e1000_netpoll,
#endif
    .ndo_set_features   = e1000_set_features,
};

/* In probe function (simplified): */
static int e1000_probe(struct pci_dev *pdev, ...)
{
    struct net_device *netdev;
    struct e1000_adapter *adapter;
    
    /* Allocate network device with private data */
    netdev = alloc_etherdev(sizeof(struct e1000_adapter));
    
    /* Get private data pointer */
    adapter = netdev_priv(netdev);       /* ★ PRIVATE DATA ACCESS */
    adapter->netdev = netdev;
    adapter->pdev = pdev;
    
    /* Install ops table */
    netdev->netdev_ops = &e1000e_netdev_ops;  /* ★ INSTALL OPS */
    
    /* Initialize hardware... */
    
    /* Register with network subsystem */
    err = register_netdev(netdev);        /* ★ REGISTER */
    
    return 0;
}
```

**TX Path (Polymorphic Call):**

```c
/* net/core/dev.c (simplified) */
int dev_queue_xmit(struct sk_buff *skb)
{
    struct net_device *dev = skb->dev;
    const struct net_device_ops *ops = dev->netdev_ops;
    
    /* ... queueing and scheduling ... */
    
    /* Call driver's transmit function (POLYMORPHIC CALL) */
    rc = ops->ndo_start_xmit(skb, dev);   /* ★ DISPATCH */
    
    return rc;
}
```

**中文解释：**
- `net_device_ops`：定义 30+ 操作，涵盖生命周期、数据路径、配置、统计
- `net_device`：包含 dev_list（链表）、netdev_ops（ops 指针）、priv（私有数据）
- 驱动 probe：分配设备、获取私有数据指针、安装 ops、注册
- TX 路径：`dev_queue_xmit()` 调用 `ops->ndo_start_xmit()` 发送数据包

---

### B.3 Module 3: i2c_adapter (I²C Bus Framework)

```
+------------------------------------------------------------------+
|  I2C ADAPTER ARCHITECTURE                                        |
+------------------------------------------------------------------+

    CLIENT DRIVER (e.g., temperature sensor)
    ┌─────────────────────────────────────────────────────────────┐
    │  i2c_smbus_read_byte_data(client, REG_TEMP);                │
    │  i2c_transfer(adapter, msgs, 2);                            │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    I2C CORE (drivers/i2c/i2c-core.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  i2c_transfer(adap, msgs, num)                              │
    │       │                                                     │
    │       ▼                                                     │
    │  adap->algo->master_xfer(adap, msgs, num)  ← POLYMORPHIC    │
    │                                                             │
    │  i2c_add_adapter()                                          │
    │  i2c_del_adapter()                                          │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    BUS ADAPTER DRIVER (e.g., i2c-designware.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  static const struct i2c_algorithm dw_algo = {              │
    │      .master_xfer  = dw_xfer,                               │
    │      .functionality= dw_func,                               │
    │  };                                                         │
    │                                                             │
    │  /* In probe: */                                            │
    │  dev->adapter.algo = &dw_algo;         ← INSTALL ALGO       │
    │  dev->adapter.algo_data = dev;         ← PRIVATE DATA       │
    │  i2c_add_adapter(&dev->adapter);       ← REGISTER           │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 客户端驱动通过 i2c_transfer() 发送数据
- I2C 核心分发到 adap->algo->master_xfer()（多态调用）
- 总线适配器驱动提供 i2c_algorithm，在 probe 时安装并注册

**Key Structure Definitions:**

```c
/* include/linux/i2c.h - Lines 351-366 */
struct i2c_algorithm {
    /* If adapter can't do I2C-level access, set master_xfer to NULL.
     * If adapter can do SMBus access, set smbus_xfer. 
     * If NULL, SMBus protocol is simulated using I2C messages */
    
    /* master_xfer should return number of messages successfully
     * processed, or negative value on error */
    int (*master_xfer)(struct i2c_adapter *adap,   /* ★ PRIMARY OP */
                       struct i2c_msg *msgs, int num);
    
    int (*smbus_xfer)(struct i2c_adapter *adap, u16 addr,
                      unsigned short flags, char read_write,
                      u8 command, int size, union i2c_smbus_data *data);

    /* To determine what the adapter supports */
    u32 (*functionality)(struct i2c_adapter *);
};

/* include/linux/i2c.h - Lines 372-391 */
struct i2c_adapter {
    struct module *owner;
    unsigned int class;
    
    const struct i2c_algorithm *algo;  /* ★ OPS TABLE POINTER */
    void *algo_data;                   /* ★ PRIVATE DATA POINTER */

    struct rt_mutex bus_lock;          /* Bus-level locking */

    int timeout;                       /* In jiffies */
    int retries;
    struct device dev;                 /* Embedded device */

    int nr;                            /* Adapter number */
    char name[48];
    struct completion dev_released;

    struct mutex userspace_clients_lock;
    struct list_head userspace_clients; /* ★ LINKED LIST */
};
```

**Transfer Path (Polymorphic Call):**

```c
/* drivers/i2c/i2c-core.c (simplified) */
int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    int ret;

    /* Adapter must support master_xfer */
    if (adap->algo->master_xfer) {
        /* Lock the bus */
        rt_mutex_lock(&adap->bus_lock);
        
        /* Call adapter's transfer function (POLYMORPHIC CALL) */
        ret = adap->algo->master_xfer(adap, msgs, num);  /* ★ DISPATCH */
        
        rt_mutex_unlock(&adap->bus_lock);
        return ret;
    }
    
    return -EOPNOTSUPP;
}
EXPORT_SYMBOL(i2c_transfer);
```

**中文解释：**
- `i2c_algorithm`：定义 master_xfer（主传输）、smbus_xfer（SMBus）、functionality（能力查询）
- `i2c_adapter`：包含 algo（算法指针）、algo_data（私有数据）、bus_lock（总线锁）
- `i2c_transfer()`：加锁、调用 algo->master_xfer()、解锁

---

### B.4 Module 4: gpio_chip (GPIO Framework)

```
+------------------------------------------------------------------+
|  GPIO_CHIP ARCHITECTURE                                          |
+------------------------------------------------------------------+

    GPIO CONSUMER (e.g., LED driver)
    ┌─────────────────────────────────────────────────────────────┐
    │  gpio_request(gpio_num, "led");                             │
    │  gpio_direction_output(gpio_num, 0);                        │
    │  gpio_set_value(gpio_num, 1);                               │
    │  gpio_free(gpio_num);                                       │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    GPIOLIB CORE (drivers/gpio/gpiolib.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  static struct gpio_desc gpio_desc[ARCH_NR_GPIOS];          │
    │                                                             │
    │  gpio_set_value(gpio, value)                                │
    │       │                                                     │
    │       ▼                                                     │
    │  chip->set(chip, offset, value)        ← POLYMORPHIC        │
    │                                                             │
    │  gpiochip_add()                                             │
    │  gpiochip_remove()                                          │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    GPIO CONTROLLER DRIVER (e.g., gpio-pl061.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  /* Ops are EMBEDDED directly in gpio_chip! */              │
    │  static int pl061_direction_input(struct gpio_chip *gc,     │
    │                                   unsigned offset) {...}    │
    │  static int pl061_get_value(struct gpio_chip *gc,           │
    │                             unsigned offset) {...}          │
    │  static void pl061_set_value(struct gpio_chip *gc,          │
    │                              unsigned offset, int value)    │
    │  {...}                                                      │
    │                                                             │
    │  /* In probe: */                                            │
    │  chip->gc.direction_input = pl061_direction_input;          │
    │  chip->gc.get = pl061_get_value;                            │
    │  chip->gc.set = pl061_set_value;                            │
    │  gpiochip_add(&chip->gc);              ← REGISTER           │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- GPIO 消费者通过 gpio_xxx() API 操作 GPIO
- gpiolib 核心通过 chip->set() 等分发调用（多态）
- 注意：gpio_chip 的 ops 是**嵌入**的，不是单独的 ops 结构

**Key Structure Definition:**

```c
/* include/asm-generic/gpio.h - Lines 91-134 */
struct gpio_chip {
    const char *label;                 /* Chip label for debug */
    struct device *dev;                /* Parent device */
    struct module *owner;              /* Module owner */

    /* ★ OPS ARE EMBEDDED, NOT SEPARATE STRUCT */
    int (*request)(struct gpio_chip *chip, unsigned offset);
    void (*free)(struct gpio_chip *chip, unsigned offset);
    
    int (*direction_input)(struct gpio_chip *chip, unsigned offset);
    int (*get)(struct gpio_chip *chip, unsigned offset);
    
    int (*direction_output)(struct gpio_chip *chip, 
                            unsigned offset, int value);
    void (*set)(struct gpio_chip *chip, unsigned offset, int value);
    
    int (*to_irq)(struct gpio_chip *chip, unsigned offset);
    void (*dbg_show)(struct seq_file *s, struct gpio_chip *chip);
    
    int base;                          /* First GPIO number */
    u16 ngpio;                         /* Number of GPIOs */
    const char *const *names;          /* GPIO names array */
    unsigned can_sleep:1;              /* May sleep in get/set */
    unsigned exported:1;               /* Exported via sysfs */
};
```

**Dispatch Flow (Polymorphic Call):**

```c
/* drivers/gpio/gpiolib.c (simplified) */
void gpio_set_value(unsigned gpio, int value)
{
    struct gpio_chip *chip;
    unsigned offset;
    
    /* Find chip for this GPIO */
    chip = gpio_to_chip(gpio);
    offset = gpio - chip->base;
    
    /* Call chip's set function (POLYMORPHIC CALL) */
    chip->set(chip, offset, value);    /* ★ DISPATCH */
}

/* Note: gpio_chip uses EMBEDDED ops, not separate ops struct.
 * This is a VARIANT of the pattern - ops fields directly in base struct.
 * 
 * Trade-off:
 * - Saves one pointer indirection (better performance)
 * - But cannot share ops table between instances
 * - Each chip instance has its own function pointers
 */
```

**中文解释：**
- `gpio_chip`：ops 直接嵌入结构，不是单独的 ops 结构
- 这是模式的**变体**：节省一次指针间接，但不能共享 ops 表
- `gpio_set_value()`：找到 chip、计算 offset、调用 chip->set()

---

### B.5 Module 5: uart_port (Serial Port Framework)

```
+------------------------------------------------------------------+
|  UART_PORT ARCHITECTURE                                          |
+------------------------------------------------------------------+

    TTY LAYER
    ┌─────────────────────────────────────────────────────────────┐
    │  User: write(fd, "Hello", 5);                               │
    │       │                                                     │
    │       ▼                                                     │
    │  tty_write() → uart_write()                                 │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    SERIAL CORE (drivers/tty/serial/serial_core.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  uart_write() → port->ops->start_tx(port)  ← POLYMORPHIC    │
    │                                                             │
    │  uart_register_driver()                                     │
    │  uart_add_one_port()                                        │
    │  uart_remove_one_port()                                     │
    └────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
    UART DRIVER (e.g., drivers/tty/serial/amba-pl011.c)
    ┌─────────────────────────────────────────────────────────────┐
    │  static struct uart_ops pl011_ops = {                       │
    │      .tx_empty    = pl011_tx_empty,                         │
    │      .set_mctrl   = pl011_set_mctrl,                        │
    │      .get_mctrl   = pl011_get_mctrl,                        │
    │      .stop_tx     = pl011_stop_tx,                          │
    │      .start_tx    = pl011_start_tx,    ← Start transmission │
    │      .stop_rx     = pl011_stop_rx,                          │
    │      .startup     = pl011_startup,     ← Open port          │
    │      .shutdown    = pl011_shutdown,    ← Close port         │
    │      .set_termios = pl011_set_termios,                      │
    │      .type        = pl011_type,                             │
    │      .release_port= pl011_release_port,                     │
    │      .request_port= pl011_request_port,                     │
    │      .config_port = pl011_config_port,                      │
    │  };                                                         │
    │                                                             │
    │  /* In probe: */                                            │
    │  uap->port.ops = &pl011_ops;           ← INSTALL OPS        │
    │  uart_add_one_port(&pl011_driver, &uap->port); ← REGISTER   │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- TTY 层通过 uart_write() 发送数据
- Serial 核心分发到 port->ops->start_tx()（多态调用）
- UART 驱动提供 uart_ops，在 probe 时安装并注册

**Key Structure Definitions:**

```c
/* include/linux/serial_core.h - Lines 230-273 */
struct uart_ops {
    /* Status check */
    unsigned int (*tx_empty)(struct uart_port *);
    
    /* Modem control */
    void (*set_mctrl)(struct uart_port *, unsigned int mctrl);
    unsigned int (*get_mctrl)(struct uart_port *);
    
    /* TX control */
    void (*stop_tx)(struct uart_port *);
    void (*start_tx)(struct uart_port *);         /* ★ START TX */
    void (*send_xchar)(struct uart_port *, char ch);
    
    /* RX control */
    void (*stop_rx)(struct uart_port *);
    void (*enable_ms)(struct uart_port *);
    
    /* Break control */
    void (*break_ctl)(struct uart_port *, int ctl);
    
    /* Lifecycle */
    int (*startup)(struct uart_port *);           /* ★ OPEN */
    void (*shutdown)(struct uart_port *);         /* ★ CLOSE */
    void (*flush_buffer)(struct uart_port *);
    
    /* Configuration */
    void (*set_termios)(struct uart_port *, struct ktermios *new,
                        struct ktermios *old);
    void (*pm)(struct uart_port *, unsigned int state,
               unsigned int oldstate);
    
    /* Port info */
    const char *(*type)(struct uart_port *);
    void (*release_port)(struct uart_port *);
    int (*request_port)(struct uart_port *);
    void (*config_port)(struct uart_port *, int);
    int (*verify_port)(struct uart_port *, struct serial_struct *);
};

/* include/linux/serial_core.h - Lines 295-350 (partial) */
struct uart_port {
    spinlock_t lock;                   /* Port lock */
    unsigned long iobase;              /* IO base address */
    unsigned char __iomem *membase;    /* Memory base address */
    
    /* Inline ops for fast path (optimization) */
    unsigned int (*serial_in)(struct uart_port *, int);
    void (*serial_out)(struct uart_port *, int, int);
    
    unsigned int irq;                  /* IRQ number */
    unsigned int uartclk;              /* UART clock */
    unsigned int fifosize;             /* TX FIFO size */
    
    struct uart_state *state;          /* Parent state */
    struct uart_icount icount;         /* Statistics */
    
    const struct uart_ops *ops;        /* ★ OPS TABLE POINTER */
    
    /* ... more fields ... */
};
```

**Dispatch Flow (Polymorphic Call):**

```c
/* drivers/tty/serial/serial_core.c (simplified) */
static void uart_start(struct tty_struct *tty)
{
    struct uart_state *state = tty->driver_data;
    struct uart_port *port = state->uart_port;
    unsigned long flags;

    spin_lock_irqsave(&port->lock, flags);
    
    /* Call driver's start_tx function (POLYMORPHIC CALL) */
    port->ops->start_tx(port);          /* ★ DISPATCH */
    
    spin_unlock_irqrestore(&port->lock, flags);
}
```

**中文解释：**
- `uart_ops`：定义 20+ 操作，涵盖状态、TX/RX 控制、生命周期、配置
- `uart_port`：包含 ops（ops 指针）、lock（自旋锁）、硬件寄存器信息
- `uart_start()`：加锁、调用 ops->start_tx()、解锁

---

### B.6 Pattern Comparison Table

```
+------------------------------------------------------------------+
|  5 MODULES COMPARISON                                            |
+------------------------------------------------------------------+

┌──────────────┬───────────────┬───────────────┬───────────────────┐
│ Module       │ Base Object   │ Ops Table     │ Private Data      │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ miscdevice   │ miscdevice    │ file_operations│ file->private_data│
│              │ .list         │ *fops          │ or container_of  │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ net_device   │ net_device    │ net_device_ops│ netdev_priv()     │
│              │ .dev_list     │ *netdev_ops    │                   │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ i2c_adapter  │ i2c_adapter   │ i2c_algorithm │ algo_data         │
│              │ (device list) │ *algo          │ or container_of  │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ gpio_chip    │ gpio_chip     │ (EMBEDDED)    │ container_of      │
│              │ (array index) │ chip->get/set │                   │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ uart_port    │ uart_port     │ uart_ops      │ container_of      │
│              │ (driver list) │ *ops           │                   │
└──────────────┴───────────────┴───────────────┴───────────────────┘

┌──────────────┬───────────────┬───────────────┬───────────────────┐
│ Module       │ Registration  │ Lookup        │ Key Dispatch      │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ miscdevice   │misc_register()│ by minor num  │ filp->f_op->read()│
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ net_device   │register_      │ by name       │ ops->ndo_start_   │
│              │netdev()       │               │ xmit()            │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ i2c_adapter  │i2c_add_       │ by nr         │ algo->master_xfer │
│              │adapter()      │               │                   │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ gpio_chip    │gpiochip_add() │ by gpio num   │ chip->set()       │
├──────────────┼───────────────┼───────────────┼───────────────────┤
│ uart_port    │uart_add_one_  │ by line num   │ ops->start_tx()   │
│              │port()         │               │                   │
└──────────────┴───────────────┴───────────────┴───────────────────┘
```

**中文解释：**
- 所有模块遵循相同模式：基础对象 + ops 表 + 私有数据
- gpio_chip 是变体：ops 嵌入基础对象，不是单独结构
- 每个模块有自己的注册/查找机制和关键分发点

### B.7 Lessons for Real Projects

```
+------------------------------------------------------------------+
|  KEY LESSONS FROM KERNEL MODULES                                 |
+------------------------------------------------------------------+

    1. NAMING CONSISTENCY
    ┌─────────────────────────────────────────────────────────────┐
    │  struct xxx         → Base object                           │
    │  struct xxx_ops     → Behavior table (or embedded ops)      │
    │  xxx_register()     → Add to subsystem                      │
    │  xxx_unregister()   → Remove from subsystem                 │
    │                                                              │
    │  Examples:                                                  │
    │  - miscdevice / file_operations / misc_register             │
    │  - net_device / net_device_ops / register_netdev            │
    │  - i2c_adapter / i2c_algorithm / i2c_add_adapter            │
    └─────────────────────────────────────────────────────────────┘

    2. CONST OPS TABLES
    ┌─────────────────────────────────────────────────────────────┐
    │  static const struct xxx_ops my_ops = { ... };              │
    │                                                              │
    │  - Shared by all instances of same type                     │
    │  - Can be placed in .rodata (read-only, cacheable)          │
    │  - Immutable after initialization                           │
    └─────────────────────────────────────────────────────────────┘

    3. WRAPPER FUNCTIONS
    ┌─────────────────────────────────────────────────────────────┐
    │  /* Don't expose raw ops dispatch to consumers */           │
    │  int gpio_set_value(unsigned gpio, int value)               │
    │  {                                                          │
    │      chip = gpio_to_chip(gpio);                             │
    │      chip->set(chip, gpio - chip->base, value);             │
    │  }                                                          │
    │                                                              │
    │  - Hides complexity                                         │
    │  - Adds validation                                          │
    │  - Provides stable API                                      │
    └─────────────────────────────────────────────────────────────┘

    4. CONCURRENCY PROTECTION
    ┌─────────────────────────────────────────────────────────────┐
    │  - mutex for registration (misc_mtx, bus_lock)              │
    │  - spinlock for data path (port->lock)                      │
    │  - RCU for read-heavy lookups                               │
    │                                                              │
    │  Rule: Ops calls should specify context requirements:       │
    │  - "May sleep" vs "Must be atomic"                          │
    └─────────────────────────────────────────────────────────────┘

    5. PRIVATE DATA ACCESS
    ┌─────────────────────────────────────────────────────────────┐
    │  Two patterns in kernel:                                    │
    │                                                              │
    │  1. Void pointer:                                           │
    │     adapter->algo_data = driver_private;                    │
    │     struct my_priv *priv = adapter->algo_data;              │
    │                                                              │
    │  2. Container_of (preferred):                               │
    │     struct my_driver {                                      │
    │         struct i2c_adapter adapter;  /* EMBEDDED */         │
    │         /* private fields... */                             │
    │     };                                                       │
    │     struct my_driver *drv = container_of(adap,              │
    │                             struct my_driver, adapter);     │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- **命名一致性**：struct xxx、struct xxx_ops、xxx_register
- **const ops 表**：静态 const，所有同类型实例共享
- **包装函数**：隐藏复杂性、添加验证、提供稳定 API
- **并发保护**：mutex（注册）、spinlock（数据路径）、RCU（读多写少）
- **私有数据访问**：void 指针或 container_of（首选）

