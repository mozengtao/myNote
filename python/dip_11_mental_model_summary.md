[← 返回目录](dip_00_index.md) | [上一篇: Part 10 最佳实践](dip_10_best_practices.md)

# Part 11 - 最终心智模型：16 张图 + 《Python DIP 心智模型总结》

本部分统一以 [Part 8 的 network-automation 案例项目](dip_08_real_project_case_study.md)
为背景，从 16 个不同的图示角度，把 DIP 在一个真实项目里的方方面面串成一套完整心智模型。

## 图 1：对象图（Object Diagram，某一时刻的实例快照）

```
       +--------------------------------+
       | workflow: DeviceUpgradeWorkflow|
       |   onboarding -> (points below) |
       |   inspection -> (points below) |
       +--------------------------------+
                    |              |
                    v              v
+------------------------+  +------------------------+
| onboarding:            |  | inspection:            |
| DeviceOnboardingService|  | DeviceInspectionService|
|   repo -> sqlite_repo  |  |   repo -> sqlite_repo  |
+------------------------+  +------------------------+
             |                          |
             +-----------+--------------+
                         v
        +-------------------------------------+
        | sqlite_repo: SqliteDeviceRepository |
        +-------------------------------------+
```

**中文说明：** 对象图展示运行时某一时刻的实例快照，显示具体对象之间的引用关系。工作流对象持有两个服务实例的引用，而两个服务都共享同一个 SQLite 仓储实例。

对象图展示的是"运行时某一刻，具体是哪些对象实例互相持有引用"，与类图（描述类型关系）不同。

## 图 2：依赖图（Dependency Graph，源代码 import 方向）

```
api  ---->  workflows  ---->  application  ---->  domain (Port)
                                                       ^
                                                       |
                                                infrastructure
```

**中文说明：** 依赖图显示源代码 import 方向。注意 infrastructure 层反向依赖 domain 层的抽象接口，这体现了依赖倒置原则的核心思想。

## 图 3：调用图（Call Graph，运行时方法调用链）

```
cli.run_upgrade_command()
   -> DeviceUpgradeWorkflow.run()
        -> DeviceOnboardingService.onboard()
             -> FileConfigLoader.load()
             -> SqliteDeviceRepository.save()
             -> EmailNotifier.notify()
        -> DeviceInspectionService.inspect()
             -> SqliteDeviceRepository.get()
             -> SshConnector.fetch_state()
             -> SqliteDeviceRepository.save()
```

**中文说明：** 调用图展示运行时方法调用链的执行顺序。从 CLI 命令开始，通过工作流协调两个服务的执行，每个服务调用相应的基础设施组件完成具体功能。

## 图 4：生命周期图（Object Lifetime）

```
+----------------------------------------------------------------------+
| Singleton   : SqliteDeviceRepository / ConsoleLogger                 |
|               (One instance throughout application lifecycle)        |
| Transient   : SshConnector                                           |
|               (New instance created every time build_workflow runs)  |
| Scoped      : Not used in this case study                            |
|               (If Web API added, one instance per HTTP request       |
|                sharing same UnitOfWork/DB transaction)               |
+----------------------------------------------------------------------+
```

**中文说明：** 生命周期图展示不同对象的生存期管理策略。Singleton 对象在整个应用生命周期中只有一个实例；Transient 对象每次使用都创建新实例；Scoped 对象在特定范围内共享同一实例。

## 图 5：模块图（Module Diagram）

```
domain/          -> models.py, device_connector.py, device_repository.py,
                    notifier.py, config_loader.py, logger.py
application/     -> device_onboarding_service.py, device_inspection_service.py
workflows/       -> device_upgrade_workflow.py
infrastructure/  -> ssh_connector.py, netconf_connector.py, sqlite_repository.py,
                    in_memory_repository.py, file_config_loader.py,
                    email_notifier.py, console_logger.py
api/             -> cli.py
main.py          -> Composition Root
```

**中文说明：** 模块图显示各个包及其包含的 Python 模块文件。domain 层定义抽象接口和领域模型；application 层实现业务服务；infrastructure 层提供具体实现；api 层处理用户界面；main.py 作为组合根负责依赖组装。

## 图 6：包依赖图（Package-level Dependency）

```
+--------+     +-------------+     +-------------+
|  api   | --> |  workflows  | --> | application |
+--------+     +-------------+     +-------------+
                                          |
                                          v
                                    +--------+
                                    | domain |
                                    +--------+
                                          ^
                                          |
                                 +----------------+
                                 | infrastructure |
                                 +----------------+
```

**中文说明：** 包级依赖图展示模块间的依赖关系。从上到下的依赖流向体现了分层架构，而 infrastructure 向 domain 的反向依赖箭头体现了依赖倒置原则的应用。

## 图 7：Layer Diagram（分层架构）

```
+-----------------------------------------------------------+
| Presentation / API Layer     (api/)                       |
+-----------------------------------------------------------+
| Application Layer            (application/, workflows/)   |
+-----------------------------------------------------------+
| Domain Layer                 (domain/)                    |
+-----------------------------------------------------------+
| Infrastructure Layer         (infrastructure/)            |
+-----------------------------------------------------------+
```

**中文说明：** 分层架构图显示了四层结构的组织方式。依赖方向：上层依赖下层的"抽象"，但 Infrastructure 层反向依赖 Domain 层的抽象。这是"洋葱式"依赖，Infrastructure 虽然画在最下面，但源代码依赖箭头是向上指向 Domain 的，体现了依赖倒置。

## 图 8：Hexagonal Diagram（六边形架构）

```
                    +-----------------------------+
                    |        api/ (Driving)       |
                    +---------------+-------------+
                                    |
                    +---------------v-------------+
                    |                             |
                    |   domain/ + application/    |
                    |     (Application Core)      |
                    |                             |
                    +---------------+-------------+
                                    |
                    +---------------v-------------+
                    |   infrastructure/ (Driven)  |
                    +-----------------------------+
```

**中文说明：** 六边形架构图展示了端口适配器模式。Driving 适配器（如 API）驱动应用核心执行业务逻辑，Driven 适配器（如基础设施）被应用核心驱动来完成外部操作。应用核心通过端口与外部世界交互。

## 图 9：Clean Architecture Diagram（同心圆）

```
+-----------------------------------------------------------+
|  Frameworks (main.py, infrastructure/)                    |
|  +-----------------------------------------------------+  |
|  |  Interface Adapters (api/)                          |  |
|  |  +------------------------------------------------+ |  |
|  |  |  Use Cases (application/, workflows/)          | |  |
|  |  |  +-------------------------------------------+ | |  |
|  |  |  |  Entities (domain/models.py)              | | |  |
|  |  |  +-------------------------------------------+ | |  |
|  |  +------------------------------------------------+ |  |
|  +-----------------------------------------------------+  |
+-----------------------------------------------------------+
```

**中文说明：** 整洁架构同心圆图展示了四层结构的依赖关系。最内层是实体（领域模型），向外依次是用例层（应用服务）、接口适配器层（API）、框架层（基础设施）。依赖只能从外向内，内层不知道外层的存在。

## 图 10：Object Flow Diagram（对象创建与传递流程）

```
main()
  |
  v
create SqliteDeviceRepository() / ConsoleLogger() / FileConfigLoader() / EmailNotifier()
  |
  v
create SshConnector() or NetconfConnector()
  |
  v
inject all dependencies -> DeviceOnboardingService / DeviceInspectionService
  |
  v
inject both services -> DeviceUpgradeWorkflow
  |
  v
run_upgrade_command(workflow, device_id, host)
```

**中文说明：** 对象流程图展示了对象创建与传递的完整流程。从 main() 函数开始，依次创建基础设施对象、连接器对象，然后将它们注入到服务中，最后将服务注入到工作流中，完成整个对象图的组装。

## 图 11：Control Flow（控制流）

```
run() 
 -> if onboarding fails -> raise exception -> workflow stops
 -> else continue to inspect()
      -> if repo.get() returns None -> raise ValueError -> workflow stops
      -> else continue to connector.fetch_state()
```

**中文说明：** 控制流图展示了工作流的执行分支逻辑。工作流按顺序执行，任何步骤失败都会抛出异常并中止整个流程，只有所有步骤都成功才能完成整个工作流。

## 图 12：Data Flow（数据流）

```
JSON config file --load()--> dict --construct--> Device entity --save()--> SQLite table
                                                                    |
Real/Mock device --fetch_state()--> dict(state) --print/return------+
```

**中文说明：** 数据流图展示了数据在系统中的转换过程。配置数据从 JSON 文件加载为字典，然后构造成领域实体，最终保存到 SQLite 数据库；设备状态数据从真实或模拟设备获取，转换为字典格式后输出。

## 图 13：Plugin Architecture Diagram

```
+------------------+ inherit+register  +---------------------------+
| SshConnector /   | -------------->   | DeviceConnector._registry |
| NetconfConnector |                   | (if refactored to Part5   |
+------------------+                   | plugin approach)          |
                                       +---------------------------+
                                                | create(protocol)
                                                v
                                       +-------------------------+
                                       | DeviceInspectionService |
                                       +-------------------------+
```

**中文说明：** 插件架构图展示了可扩展的连接器设计。不同协议的连接器通过继承和自注册机制加入注册表，服务通过工厂方法根据协议类型创建相应的连接器实例。这种设计支持动态扩展新的设备协议。

> 本案例项目为了保持简单，采用的是 Constructor Injection 而非完整插件注册表；如果协议数量
> 会持续增长，应参考 [Part 5 示例 2](dip_05_architecture_techniques.md) 把
> `DeviceConnector` 改造成支持 `__init_subclass__` 自注册的插件体系。

## 图 14：IoC Diagram

```
+---------------------------+
| main.py (acts as simple   |
|        Container)         |
+-------------+-------------+
              | declaratively knows "who needs whom"
              v
+--------------------------------------------------------+
| SqliteDeviceRepository -> Onboarding/Inspection        |
|                          Service -> Workflow           |
+--------------------------------------------------------+
```

**中文说明：** IoC（控制反转）图展示了依赖注入容器的概念。main.py 作为简化的容器，声明式地管理对象间的依赖关系，负责创建对象并将依赖注入到需要的地方，实现了控制反转。

> 若项目继续增长到几十个组件，应参考 [Part 5 示例 1-3](dip_05_architecture_techniques.md)
> 引入 `dependency_injector` 做声明式对象图管理。

## 图 15：Composition Root Diagram

```
+-------------------------------------------------------------------+
| main.py is the ONLY place that imports ALL of the following:      |
|   - domain/ abstract types (for type annotations only)            |
|   - application/, workflows/, api/ (High-level)                   |
|   - infrastructure/ (Low-level / Detail)                          |
| No other file is allowed to know both High-level and              |
| Low-level sides simultaneously                                    |
+-------------------------------------------------------------------+
```

**中文说明：** 组合根图阐明了依赖管理的关键原则。只有 main.py 被允许同时了解高层抽象和底层实现，负责将两者连接起来。其他所有文件都不能同时依赖高层和低层，确保了清晰的分层边界。

## 图 16：Testing / Mock Diagram

```
Production:  DeviceInspectionService(SshConnector(), SqliteDeviceRepository(), ConsoleLogger())
Testing:     DeviceInspectionService(FakeConnector(), InMemoryDeviceRepository(), NoOpLogger())
                                          ^                     ^                     ^
                              All replaced with test doubles, DeviceInspectionService code unchanged
```

**中文说明：** 测试替身图展示了依赖注入在测试中的价值。生产环境使用真实的实现，测试环境使用测试替身（Mock/Fake/Stub），而业务服务的源代码完全不需要修改，体现了依赖倒置的测试友好性。

---

# 《Python Dependency Inversion Principle (DIP) 心智模型总结》

## 1. 一句话理解 DIP

**让"重要且稳定"的代码依赖抽象，让"易变且具体"的代码也依赖同一个抽象，而不是让重要的代码
依赖易变的代码。**

## 2. 依赖方向为什么倒置

因为原本"自然"的依赖方向是"业务需要什么功能，就直接调用提供这个功能的具体模块"——即
Business → Database。这个方向的问题在于：越"重要、越想保持稳定"的代码（业务规则），反而被
越"易变、越想随意替换"的代码（技术细节）用一条硬编码的 import 死死绑住。DIP 通过引入一个
双方都依赖的抽象，把这条箭头拆成两条都指向抽象的箭头，让"谁依赖谁"服从"谁更稳定、谁更重要"，
而不是服从"谁在调用谁"这一运行时表面现象。

## 3. Python 最推荐的实现方式

对于中大型项目的默认选择：**`typing.Protocol` + Constructor Injection**。理由：Protocol
零侵入、天然支持第三方对象、享受完整类型检查，是最 Pythonic 的契约表达方式；Constructor
Injection 保证对象构造完成即完整可用，是最容易推理、最不容易出 bug 的注入时机。只有在需要
"实例化即强校验""共享默认实现""子类注册表"时才升级到 `abc.ABC`；对象图复杂到手写组装
吃力时再引入 IoC 容器；需要支持未知数量第三方扩展时再上升到 Plugin Architecture。

## 4. 什么时候不要使用 DIP

- 只有一个实现、且没有任何迹象表明未来会有第二个（参见 [Part 9](dip_09_pythonic_dip.md)）。
- 一次性脚本、原型代码、生命周期以天/小时计的临时工具。
- 纯数据结构/值对象，没有"行为"需要被替换。
- 模块内部的私有实现细节，从未跨越过任何架构边界。
- 团队规模很小（1-2人）、项目复杂度低，过度抽象反而拖慢迭代速度。

## 5. 什么时候必须使用 DIP

- 依赖的技术选型大概率会变化或已知需要支持多种（数据库、支付渠道、设备协议）。
- 需要为业务逻辑写不依赖外部系统（网络、数据库、文件）的快速单元测试。
- 多团队协作，"业务团队"和"基础设施团队"需要能独立开发、独立测试、独立发布。
- 需要支持插件/扩展生态，核心代码不应该因为新增一种扩展而被迫修改。
- 构建 DDD/Clean Architecture/Hexagonal 风格的系统，DIP 是这些架构风格的地基。

## 6. 大型 Python 项目的最佳实践

参见 [Part 10](dip_10_best_practices.md) 的完整 Checklist；核心一句话：**domain/ 永远
最干净，infrastructure/ 永远反向依赖 domain/，只有 Composition Root 允许同时认识两边。**

## 7. 学习路线

```
Step 1: Master Part 1 (four terms + geometric intuition of direction inversion)
Step 2: Proficiently use typing.Protocol + Constructor Injection (Part 2④/③⑤⑥)
Step 3: Learn to use Factory and Strategy for "selection logic" and 
        "replaceable algorithms" (Part 4)
Step 4: Practice layered architecture in a real project (implement Part 8 
        case study yourself)
Step 5: Learn advanced abc.ABC usage (Template Method, subclass registry)
        (Part 2①, Part 5⑪)
Step 6: Learn IoC container and plugin architecture when needed (Part 5)
Step 7: Understand relationship between DIP and design patterns/architectural 
        styles (Part 7), draw analogies
```

**中文说明：** 学习路线图提供了掌握 DIP 的渐进式学习路径。从基础概念开始，逐步掌握实现技术，通过真实项目实践，最后理解高级模式和架构风格的关系，形成完整的知识体系。

## 8. 容易踩的坑

```
+-----------------------------------------------------------------------+
| Pitfall 1: Using DI (Dependency Injection) and thinking DIP is       |
|            achieved - see Part 1.2 counter-examples                  |
| Pitfall 2: Defining abstractions in infrastructure/ instead of       |
|            domain/, direction seems right but domain/ actually        |
|            imports interfaces from infrastructure/                    |
| Pitfall 3: Forcibly defining interfaces for single implementations    |
|            (over-design)                                              |
| Pitfall 4: Defining Protocol method signatures without using type    |
|            checkers (mypy/pyright), making "contracts" meaningless,  |
|            finding signature mismatches only at runtime              |
| Pitfall 5: Stuffing business logic into Factory/Container, making    |
|            Factory a new coupling point                              |
| Pitfall 6: Using Setter Injection but forgetting default values,    |
|            objects exist in "constructed but not usable" state      |
| Pitfall 7: Storing request-scoped mutable state in global singletons,|
|            causing pollution in concurrent scenarios                  |
| Pitfall 8: Plugin registration mechanism too obscure (too much black |
|            magic), new team members can't locate "where is this class|
|            loaded/called from"                                        |
+-----------------------------------------------------------------------+
```

**中文说明：** 常见陷阱图列举了在应用 DIP 时容易踩的坑。包括概念混淆（DI vs DIP）、架构组织错误、过度设计、工具配合不当、违背单一职责、对象状态管理、并发安全、可维护性等多个方面的问题。

## 9. 面试高频问题

```
Q1: DIP 和 DI 的区别？
A1: DIP 是设计原则（抽象归谁、依赖方向朝哪），DI 是实现手段（依赖如何被传递进来）。
    见 Part 1.2。

Q2: Python 里怎么定义接口？
A2: abc.ABC（显式继承+强制校验）、typing.Protocol（结构化，零侵入）、
    或者干脆用 Duck Typing（约定俗成）。见 Part 2 / Part 9。

Q3: Repository 模式为什么重要？
A3: 它是 DIP 在数据访问场景的经典应用：领域层定义存取契约，基础设施层实现，
    使业务逻辑可以脱离具体数据库进行测试和演化。见 Part 2③ / Part 7。

Q4: Hexagonal Architecture 和 DIP 是什么关系？
A4: Hexagonal Architecture（Ports & Adapters）是把 DIP 系统化应用到整个应用边界的
    架构级实践：Port = Abstraction，Adapter = Detail。见 Part 7。

Q5: 什么时候不该用抽象接口？
A5: 只有一个实现且未来不太可能变化、纯数据对象、私有实现细节、一次性脚本。见 Part 9。

Q6: 举一个"用了依赖注入但违反 DIP"的例子？
A6: 构造函数参数类型标注写死具体类（如 MySQLOrderRepository）而不是抽象类型，
    即便对象是从外部传进来的，源码依赖方向依然没有倒置。见 Part 1.2。

Q7: IoC 容器解决了什么问题？
A7: 当对象图复杂、依赖层级深时，手写组装代码冗长易错；IoC 容器提供声明式的组装方式并
    统一管理对象生命周期（Singleton/Transient/Scoped）。见 Part 5⑫ / Part 10。
```

## 10. 最终心智框架

```
+-------------------------------------------------------------------+
|                Python DIP Ultimate Mental Framework              |
+-------------------------------------------------------------------+
| Layer 1 (What): High-level/Low-level/Abstraction/Detail         |
|                 four elements                                     |
| Layer 2 (Why):  Keep important stable code from being           |
|                 hijacked by volatile technical details           |
| Layer 3 (How):  12 Python implementation approaches, choose     |
|                 based on dependency lifecycle/scale/team        |
|                 collaboration needs (default: Protocol +        |
|                 Constructor Injection)                           |
| Layer 4 (When): Use when crossing module/team boundaries,       |
|                 tech choices will change, need independent       |
|                 testing; Don't use for one-off/single           |
|                 implementation/pure data/private details        |
| Layer 5 (Organization): domain cleanest -> application/workflow |
|                         depends only on domain ->               |
|                         infrastructure reverse-depends on       |
|                         domain -> Composition Root knows all    |
| Layer 6 (Evolution): When requirements get complex, gradually   |
|                      upgrade from Constructor Injection ->      |
|                      Factory -> Plugin Architecture ->          |
|                      IoC Container                              |
+-------------------------------------------------------------------+
```

**中文说明：** 最终心智框架图总结了 Python DIP 的完整知识体系。分为六个层次：基础概念、设计动机、实现方式、使用时机、架构组织、演进路径。这个框架为理解和应用 DIP 提供了系统性的思维模型。

至此，Python Dependency Inversion Principle 心智模型教程全部完成。建议回到
[索引页](dip_00_index.md)，挑选任意薄弱环节重新阅读，并在自己的项目里实际践行一遍
[Part 8 的 network-automation 案例](dip_network_automation_case_study/README.md)。
