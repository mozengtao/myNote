[Game Programming Patterns](https://gameprogrammingpatterns.com/contents.html)  
[Java Design Patterns](https://java-design-patterns.com/patterns/)  
[14 Patterns to Ace Any Coding Interview Question](https://hackernoon.com/14-patterns-to-ace-any-coding-interview-question-c5bb3357f6ed)  
[Design Patterns](https://sourcemaking.com/design_patterns)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  

## DDD
![领域驱动设计](./design_patterns/Domain_Driven_Design_in_C.md)  
![]()  
![]()  
1. 什么是 DDD
    DDD 核心理念：让代码"说业务语言"，业务驱动而非技术驱动
2. DDD 核心概念
    实体、值对象、聚合、聚合根、仓储、领域服务、领域事件
3. 传统方式 vs DDD 对比
    以数据为中心 vs 以领域为中心的代码对比

- DDD 核心概念
```
┌─────────────────────────────────────────────────────────┐
│                    Aggregate (聚合)                     │
│                                                         │
│   ┌───────────────────────────────────────────────┐     │
│   │        Aggregate Root (聚合根)                │     │
│   │        Order (订单)                           │     │
│   │                                               │     │
│   │   + confirm()   (业务方法)                    │     │
│   │   + cancel()                                  │     │
│   │   + add_item()                                │     │
│   └───────────────────────────────────────────────┘     │
│                         │                               │
│                         ▼                               │
│   ┌───────────────────────────────────────────────┐     │
│   │        Entity (实体) - OrderItem              │     │
│   │        Value Object (值对象) - Money          │     │
│   └───────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────┘
```

- 示例代码结构
```
order_system/
├── domain/              # 领域层 - 核心业务逻辑
│   ├── order.h/c        # 订单聚合根
│   ├── order_item.h/c   # 订单项实体
│   ├── money.h/c        # 金额值对象
│   └── order_events.h   # 领域事件
├── repository/          # 仓储层 - 持久化
│   └── order_repository.h/c
├── service/             # 应用服务层
│   └── order_service.h/c
└── main.c               # 演示程序
```

- DDD 的 5 大好处
好处	        说明
业务逻辑集中	 所有规则在领域对象中，修改只需改一处
代码即文档	     order_confirm() 比 UPDATE status=1 更易懂
易于测试	    不依赖数据库，毫秒级单元测试
技术解耦	    换数据库不改业务代码
统一语言	    业务人员和开发人员使用相同术语