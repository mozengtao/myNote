# 单例模式 (Singleton Pattern)

## 一、模式定义

单例模式是一种创建型设计模式，确保一个类只有一个实例，并提供一个全局访问点来获取该实例。

## 二、ASCII 图解

```
+------------------------------------------------------------------+
|                      SINGLETON PATTERN                            |
+------------------------------------------------------------------+

    +-------------------+
    |   Application     |
    +-------------------+
            |
            | getInstance()
            v
    +-------------------+         +-------------------+
    |  Singleton Class  |-------->|  Single Instance  |
    +-------------------+         +-------------------+
    | - instance: ptr   |         |  (Only ONE copy   |
    | + getInstance()   |         |   in memory)      |
    | + operation()     |         +-------------------+
    +-------------------+
            ^
            |
    +-------+-------+
    |               |
+-------+       +-------+
|Client1|       |Client2|       ... All clients share
+-------+       +-------+           the SAME instance


    First Call:                    Subsequent Calls:
    +-----------+                  +-----------+
    | instance  |                  | instance  |
    | == NULL?  |                  | == NULL?  |
    +-----+-----+                  +-----+-----+
          |                              |
         YES                            NO
          |                              |
          v                              v
    +-----------+                  +-----------+
    | Create    |                  | Return    |
    | instance  |                  | existing  |
    +-----------+                  | instance  |
          |                        +-----------+
          v
    +-----------+
    | Return    |
    | instance  |
    +-----------+
```

**图解说明：**

上图展示了单例模式的核心机制。在整个应用程序生命周期中，无论有多少个客户端（Client1、Client2等）请求获取实例，系统都只会创建并返回同一个唯一的实例对象。首次调用 `getInstance()` 时会创建实例，后续调用直接返回已创建的实例。

## 三、适用场景

1. **配置管理器**：全局配置信息只需要一份
2. **日志系统**：统一的日志记录器
3. **数据库连接池**：管理数据库连接资源
4. **硬件设备访问**：如打印机、串口等设备的访问控制
5. **缓存系统**：全局缓存管理

## 四、实现方法

### 方法一：简单实现（非线程安全）

```c
#include <stdio.h>
#include <stdlib.h>

// 单例结构体定义
typedef struct {
    int value;
    char name[50];
} Singleton;

// 静态实例指针
static Singleton* instance = NULL;

// 获取单例实例
Singleton* getInstance(void) {
    if (instance == NULL) {
        instance = (Singleton*)malloc(sizeof(Singleton));
        if (instance != NULL) {
            instance->value = 0;
            instance->name[0] = '\0';
            printf("Singleton instance created.\n");
        }
    }
    return instance;
}

// 设置值
void setValue(Singleton* s, int value) {
    if (s != NULL) {
        s->value = value;
    }
}

// 获取值
int getValue(Singleton* s) {
    return (s != NULL) ? s->value : -1;
}

// 销毁单例（程序结束时调用）
void destroySingleton(void) {
    if (instance != NULL) {
        free(instance);
        instance = NULL;
        printf("Singleton instance destroyed.\n");
    }
}

// 测试代码
int main(void) {
    // 获取单例实例
    Singleton* s1 = getInstance();
    Singleton* s2 = getInstance();
    
    // 验证是否为同一实例
    printf("s1 address: %p\n", (void*)s1);
    printf("s2 address: %p\n", (void*)s2);
    printf("s1 == s2: %s\n", (s1 == s2) ? "true" : "false");
    
    // 使用单例
    setValue(s1, 100);
    printf("Value set by s1: %d\n", getValue(s1));
    printf("Value read by s2: %d\n", getValue(s2));
    
    // 清理
    destroySingleton();
    
    return 0;
}
```

### 方法二：线程安全实现（使用互斥锁）

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// 单例结构体定义
typedef struct {
    int data;
    pthread_mutex_t mutex;
} ThreadSafeSingleton;

// 静态实例指针和初始化锁
static ThreadSafeSingleton* ts_instance = NULL;
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

// 获取线程安全的单例实例（双重检查锁定）
ThreadSafeSingleton* getThreadSafeInstance(void) {
    if (ts_instance == NULL) {
        pthread_mutex_lock(&init_mutex);
        
        // 双重检查
        if (ts_instance == NULL) {
            ts_instance = (ThreadSafeSingleton*)malloc(sizeof(ThreadSafeSingleton));
            if (ts_instance != NULL) {
                ts_instance->data = 0;
                pthread_mutex_init(&ts_instance->mutex, NULL);
                printf("Thread-safe singleton created.\n");
            }
        }
        
        pthread_mutex_unlock(&init_mutex);
    }
    return ts_instance;
}

// 线程安全的数据操作
void setData(ThreadSafeSingleton* s, int data) {
    if (s != NULL) {
        pthread_mutex_lock(&s->mutex);
        s->data = data;
        pthread_mutex_unlock(&s->mutex);
    }
}

int getData(ThreadSafeSingleton* s) {
    int result = -1;
    if (s != NULL) {
        pthread_mutex_lock(&s->mutex);
        result = s->data;
        pthread_mutex_unlock(&s->mutex);
    }
    return result;
}

// 销毁单例
void destroyThreadSafeSingleton(void) {
    pthread_mutex_lock(&init_mutex);
    if (ts_instance != NULL) {
        pthread_mutex_destroy(&ts_instance->mutex);
        free(ts_instance);
        ts_instance = NULL;
        printf("Thread-safe singleton destroyed.\n");
    }
    pthread_mutex_unlock(&init_mutex);
}

// 线程函数
void* threadFunc(void* arg) {
    int thread_id = *(int*)arg;
    ThreadSafeSingleton* s = getThreadSafeInstance();
    
    printf("Thread %d got instance at: %p\n", thread_id, (void*)s);
    setData(s, thread_id * 10);
    printf("Thread %d set data to: %d\n", thread_id, thread_id * 10);
    
    return NULL;
}

// 测试代码
int main(void) {
    pthread_t threads[5];
    int thread_ids[5] = {1, 2, 3, 4, 5};
    
    // 创建多个线程同时访问单例
    for (int i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, threadFunc, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 读取最终数据
    ThreadSafeSingleton* s = getThreadSafeInstance();
    printf("Final data value: %d\n", getData(s));
    
    // 清理
    destroyThreadSafeSingleton();
    
    return 0;
}
```

## 五、注意事项

1. **内存管理**：确保在程序结束时正确释放单例实例
2. **线程安全**：多线程环境下需要使用互斥锁保护
3. **懒加载 vs 饿汉式**：根据需求选择延迟初始化或程序启动时初始化
4. **避免滥用**：单例模式本质上是全局状态，过度使用会增加代码耦合度

## 六、输出示例

```
Singleton instance created.
s1 address: 0x55a8b2c01260
s2 address: 0x55a8b2c01260
s1 == s2: true
Value set by s1: 100
Value read by s2: 100
Singleton instance destroyed.
```

