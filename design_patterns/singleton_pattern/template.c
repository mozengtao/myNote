/* 通用范式代码（线程安全的懒加载单例）

懒加载：实例仅在首次调用GetInstance()时创建，节省内存；
双重检查锁：第一层无锁检查优化性能，第二层加锁检查保证线程安全；
封装性：实例为static私有变量，外部无法直接修改，仅通过接口操作。
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// 1. 定义单例结构体（核心数据）
typedef struct {
    int max_res;       // 示例：最大资源数
    int used_res;      // 示例：已使用资源数
    pthread_mutex_t lock; // 线程安全锁
} ResourceManager;

// 2. 静态全局变量：保证实例唯一（初始为NULL）
static ResourceManager *singleton_inst = NULL;
// 初始化标记：避免多次加锁（优化性能）
static int singleton_inited = 0;
// 全局锁：保护初始化过程
static pthread_mutex_t singleton_lock = PTHREAD_MUTEX_INITIALIZER;

// 3. 唯一访问接口：获取单例实例（懒加载+线程安全）
ResourceManager* ResourceManager_GetInstance() {
    // 第一层检查：无锁，快速路径（避免频繁加锁）
    if (!singleton_inited) {
        pthread_mutex_lock(&singleton_lock);
        // 第二层检查：加锁后再次确认（防止多线程竞态）
        if (!singleton_inst) {
            // 初始化实例
            singleton_inst = (ResourceManager*)malloc(sizeof(ResourceManager));
            singleton_inst->max_res = 100;
            singleton_inst->used_res = 0;
            pthread_mutex_init(&singleton_inst->lock, NULL);
            printf("单例实例首次初始化完成\n");
        }
        singleton_inited = 1;
        pthread_mutex_unlock(&singleton_lock);
    }
    return singleton_inst;
}

// 4. 业务接口：封装单例的操作（示例：分配资源）
int ResourceManager_Alloc(ResourceManager *inst) {
    if (!inst) return -1;
    pthread_mutex_lock(&inst->lock);
    if (inst->used_res >= inst->max_res) {
        pthread_mutex_unlock(&inst->lock);
        return -1; // 资源耗尽
    }
    inst->used_res++;
    printf("分配资源：当前使用数=%d\n", inst->used_res);
    pthread_mutex_unlock(&inst->lock);
    return 0;
}

// 测试：多线程环境下仅初始化一次
void* thread_func(void *arg) {
    ResourceManager *inst = ResourceManager_GetInstance();
    ResourceManager_Alloc(inst);
    return NULL;
}

int main() {
    pthread_t t1, t2, t3;
    // 创建3个线程，均调用单例接口
    pthread_create(&t1, NULL, thread_func, NULL);
    pthread_create(&t2, NULL, thread_func, NULL);
    pthread_create(&t3, NULL, thread_func, NULL);
    // 等待线程结束
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    return 0;
}