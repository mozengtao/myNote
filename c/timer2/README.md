# 通用时间轮定时器库

基于 DPL 定时器实现的高性能时间轮定时器库，专为网络协议栈和实时系统设计。

## 项目文件

### 核心文件
- `timer.h` - 头文件，包含所有API定义和数据结构
- `timer.c` - 核心实现，时间轮算法和定时器管理
- `main.c` - 演示程序，展示各种使用场景

### 文档
- `timer-design.md` - 详细的中文技术文档，解释设计思想和实现机制
- `timer_api_design.md` - API设计文档
- `README.md` - 本文件

### 构建文件
- `Makefile` - 构建脚本

## 快速开始

### 编译和运行

```bash
# 编译演示程序
make demo

# 运行演示
./timer_demo

# 编译静态库
make library

# 清理构建文件
make clean
```

### 基本使用示例

```c
#include "timer.h"

// 创建定时器上下文
timer_context_t *ctx = timer_context_create();

// 定义回调函数
void my_callback(gtimer_t *timer, void *user_data) {
    printf("Timer expired!\n");
}

// 创建定时器
gtimer_t *timer = timer_create("my_timer", my_callback, NULL, 1);

// 启动定时器 (5秒后触发)
timer_start(ctx, timer, 5 * 1000000);

// 在事件循环中执行
while (running) {
    timer_exec(ctx);
    // 处理其他事件...
}

// 清理
timer_delete(timer);
timer_context_destroy(ctx);
```

## 核心特性

### 性能优势
- **O(1) 插入和删除**：直接计算槽位位置
- **批量处理**：一次处理多个到期定时器  
- **内存高效**：固定大小轮子 + 动态链表
- **缓存友好**：按时间顺序访问内存

### 设计特点
- **简洁API**：易于理解和使用的接口
- **平台无关**：纯C实现，可移植性好
- **微秒精度**：支持高精度时间控制
- **事件循环集成**：与select/poll完美配合

### 适用场景
- 网络协议超时处理
- 连接保活机制
- 重传和重试逻辑
- 周期性维护任务
- 实时系统调度

## 技术参数

- **轮子大小**：8192 个槽位
- **时间粒度**：1秒（1,000,000 微秒）
- **覆盖范围**：直接支持 ~2.3 小时，多轮支持任意时间
- **内存开销**：固定64KB + 每个定时器~80字节

## 架构设计

```
时间轮 (8192槽位)
    │
    ├── 当前指针位置
    ├── 基准时间
    └── 槽位数组[0..8191]
            │
            └── 定时器链表 (按到期时间排序)
```

## API 参考

### 上下文管理
- `timer_context_create()` - 创建时间轮上下文
- `timer_context_destroy()` - 销毁上下文和所有定时器

### 定时器生命周期
- `timer_create()` - 创建定时器对象
- `timer_start()` - 启动定时器
- `timer_stop()` - 停止定时器
- `timer_restart()` - 重启定时器
- `timer_delete()` - 删除定时器

### 执行控制
- `timer_exec()` - 执行到期定时器
- `timer_get_next_timeout()` - 获取下次超时时间

### 工具函数
- `timer_get_time()` - 获取当前时间
- `timer_time_cmp()` - 时间比较

## 最佳实践

### 事件循环集成
```c
while (running) {
    // 获取超时时间
    timer_time_t timeout = timer_get_next_timeout(ctx);
    
    // 设置select超时
    struct timeval tv = {
        .tv_sec = timeout / 1000000,
        .tv_usec = timeout % 1000000
    };
    
    // 等待事件
    select(maxfd, &readfds, NULL, NULL, &tv);
    
    // 执行定时器
    timer_exec(ctx);
    
    // 处理网络事件...
}
```

### 错误处理
- 定时器回调函数应避免长时间阻塞
- 检查API返回值处理错误情况
- 合理管理定时器生命周期

### 性能调优
- 根据应用特点选择合适的时间粒度
- 避免创建过多的短期定时器
- 定时器回调中避免复杂计算

## 编译要求

- C99 兼容编译器 (gcc, clang)
- POSIX 系统 (Linux, macOS, *BSD)
- 标准C库支持

## 许可证

基于原始 DPL 实现的开源版本，供学习和实际使用。

## 相关文档

详细的设计思想和实现机制请参考 `timer-design.md` 文档。