# Python Dependency Inversion Principle (DIP) 心智模型教程

这是一套系统化掌握 **Dependency Inversion Principle（依赖倒置原则）** 的教程，面向已经掌握 Python
基础语法、希望建立完整心智模型的读者。全部示例采用 Python 3.12 风格，取材于 DDD / Clean
Architecture / 网络自动化 / Web Backend 的真实工程场景，而非教材式 Hello World。

> 本教程不是零散知识点，而是一条完整的学习路径：先建立本质认知，再逐一掌握 12 种 Python 实现
> 方式，再理解它与经典设计模式、真实项目结构的关系，最后收敛成一套可以指导真实工程设计的思维框架。

## 如何阅读

按顺序阅读 Part 1 → Part 8；如果你已经熟悉某一部分，可以直接跳到对应文件。每个技巧 / 示例都遵循
统一的五段式结构：**完整代码 → 对象关系图 → Object Flow → Dependency Graph → 为什么符合 DIP →
如果违反 DIP 会怎样**，方便对照学习和检索。

## 目录

### 第一部分：DIP 的本质

- [Part 1 - DIP 本质与依赖方向反转](dip_01_essence_and_direction.md)
  High-level/Low-level Module、Abstraction、Detail 的定义；为什么叫 *Inversion* 而不是
  *Injection*；依赖方向反转的多张对比图。

### 第二部分：Python 中实现 DIP 的 12 种方式

- [Part 2 - 类型系统类：abc.ABC / typing.Protocol / Duck Typing](dip_02_type_system_techniques.md)
- [Part 3 - 注入方式类：Constructor / Setter / Method Injection](dip_03_injection_techniques.md)
- [Part 4 - 创建型类：Factory+DIP / Strategy / Callback Injection / Functional DI](dip_04_creational_techniques.md)
- [Part 5 - 架构类：Plugin Architecture / IoC Container](dip_05_architecture_techniques.md)

每种方式都包含原理讲解（为什么能实现 DIP、依赖方向、适合规模、优缺点）与至少 3 个完整可运行的
独立示例。

### 第三部分：实现方式比较

- [Part 6 - 12 种实现方式对比表](dip_06_comparison_table.md)

### 第四部分：DIP 与设计模式

- [Part 7 - DIP 与 Factory / Strategy / Bridge / Adapter / Facade / Command / Repository /
  Service Layer / Hexagonal / Ports & Adapters / Clean Architecture / DDD](dip_07_design_patterns.md)

### 第五部分：真实 Python 项目案例

- [Part 8 - network-automation 项目架构分析 + 可运行案例项目](dip_08_real_project_case_study.md)
- 配套可运行代码：[python/dip_network_automation_case_study/](dip_network_automation_case_study/README.md)

### 第六部分：Pythonic 的 DIP

- [Part 9 - 什么时候用 Protocol / ABC / Duck Typing / 完全不用接口](dip_09_pythonic_dip.md)

### 第七部分：大型项目最佳实践

- [Part 10 - 企业级 Python 项目组织与对象生命周期管理](dip_10_best_practices.md)

### 第八部分：最终心智模型

- [Part 11 - 15+ 张图的完整心智模型 + 《Python DIP 心智模型总结》](dip_11_mental_model_summary.md)

## 快速心智地图

```
+----------------------------------------------------------------+
|                     Python DIP 心智模型教程                     |
+----------------------------------------------------------------+
| Part 1  本质        : Module / Abstraction / Detail / 方向反转  |
| Part 2-5 实现方式    : ABC/Protocol/Duck/注入/工厂/策略/插件/IoC |
| Part 6  横向比较     : 12 种方式全维度对比表                     |
| Part 7  设计模式关系 : DIP 是几乎所有解耦模式的底层原理           |
| Part 8  真实项目     : network-automation 分层落地               |
| Part 9  Pythonic     : 什么时候不需要接口                        |
| Part 10 最佳实践     : 大型项目组织 + 对象生命周期                |
| Part 11 心智模型     : 15+ 图 + 十条最终总结                     |
+----------------------------------------------------------------+
```
