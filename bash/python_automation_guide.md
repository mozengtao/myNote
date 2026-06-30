# Python 自动化项目代码组织与心智模式（完整示例）

> **目标**
>
> 建立一种可以长期维护、容易扩展的自动化项目组织方式，而不是"把 Shell 翻译成 Python"。

---

# 一、Python 自动化 = Workflow + Object + Service

Shell 自动化：

```
Command
    │
Pipeline
    │
Command
```

Python 自动化：

```
Object
    │
Workflow
    │
Service
```

Shell 组织的是：

> Command Stream

Python 组织的是：

> Object Flow

因此：

```
Shell：
    Data(Stream)
        │
        ▼
    Command
        │
        ▼
    stdout

Python：
    Object
        │
        ▼
    Function
        │
        ▼
    Object
```

所以：

**Python 不应该大量拼接字符串。**

Python 应该组织：

```
Object
↓

Workflow

↓

Object

↓

Workflow

↓

Result
```

---

# 二、完整案例

假设需要实现下面的需求：

```
收集多台设备信息

↓

解析输出

↓

分析

↓

生成 Markdown 报告
```

例如：

```
Host1
Host2
Host3

↓

SSH

↓

show version

↓

show interface

↓

统计

↓

report.md
```

---

# 三、推荐目录结构

```
network_audit/
│
├── main.py
│
├── config/
│      ├── config.yaml
│      └── loader.py
│
├── workflow/
│      └── audit.py
│
├── services/
│      └── device_service.py
│
├── infra/
│      └── ssh.py
│
├── models/
│      ├── device.py
│      └── report.py
│
├── parsers/
│      └── parser.py
│
├── reporter/
│      └── markdown.py
│
└── utils/
       └── logger.py
```

职责：

```
config
    配置

workflow
    做什么

services
    业务能力

infra
    如何访问外部世界

models
    数据模型

reporter
    输出结果
```

---

# 四、项目整体数据流

```
             config.yaml
                  │
                  ▼
            Configuration
                  │
                  ▼
             Workflow
                  │
                  ▼
          DeviceService
                  │
                  ▼
            SSH Client
                  │
                  ▼
          Network Device
                  │
                  ▼
             CLI Output
                  │
                  ▼
              Parser
                  │
                  ▼
           Device Object
                  │
                  ▼
             Analyzer
                  │
                  ▼
            Report Object
                  │
                  ▼
         Markdown Reporter
                  │
                  ▼
             report.md
```

---

# 五、第一层：Configuration

config.yaml

```yaml
hosts:
  - sw1
  - sw2
  - sw3

username: admin

timeout: 10
```

loader.py

```python
config = load_yaml(...)
```

程序其它地方：

```
config.hosts

config.timeout
```

而不是：

```
hosts=[...]

timeout=10
```

原则：

> 配置永远来自外部。

---

# 六、第二层：Model（最重要）

不要：

```
dict
```

应该：

```python
@dataclass
class Device:

    hostname: str

    version: str

    interfaces: list[str]
```

那么：

```
CLI

↓

Parser

↓

Device
```

以后整个程序都是：

```
Device
```

而不是：

```
dict
```

例如：

```
device.hostname

device.version

device.interfaces
```

而不是：

```
data["hostname"]
```

---

# 七、第三层：Infrastructure

这里只有：

```
SSH

HTTP

Kafka

Filesystem

subprocess
```

例如：

```python
class SSHClient:

    def execute(self, command):
        ...
```

这一层：

不知道什么是：

```
Device

Report

Workflow
```

它只负责：

```
发送命令

返回文本
```

---

# 八、第四层：Service

Service 开始有业务含义。

例如：

```python
class DeviceService:

    def collect_version(...)

    def collect_interfaces(...)
```

Workflow：

```
Workflow

↓

DeviceService
```

而不是：

```
Workflow

↓

SSH

↓

CLI
```

Workflow 不应该知道：

```
SSH

Paramiko

subprocess
```

---

# 九、第五层：Parser

例如：

```
CLI

↓

Parser

↓

Model
```

代码：

```python
parse_version()

↓

Device.version
```

不要：

```
Workflow

↓

split()

↓

regex()

↓

split()
```

解析应该独立。

---

# 十、第六层：Workflow

Workflow 只回答：

> 做什么？

例如：

```python
collect()

↓

analyze()

↓

generate_report()
```

Workflow 不应该：

```
ssh

regex

json

markdown
```

Workflow 只组织流程。

例如：

```
collect

↓

parse

↓

analyze

↓

report
```

---

# 十一、第七层：Reporter

最后一步：

```
Report Object

↓

Markdown

↓

HTML

↓

JSON
```

例如：

```python
MarkdownReporter.write(report)
```

不要：

```
Workflow

↓

open()

↓

write()

↓

format()
```

输出也应该独立。

---

# 十二、完整调用关系

```
                 main.py
                    │
                    ▼
              AuditWorkflow
                    │
      ┌─────────────┼─────────────┐
      ▼                           ▼
 DeviceService             MarkdownReporter
      │
      ▼
 SSHClient
      │
      ▼
Network Device
```

每层职责：

```
main
    调用 Workflow

Workflow
    组织流程

Service
    提供业务能力

Infrastructure
    操作外部资源
```

---

# 十三、整个对象流（Object Flow）

```
Config
    │
    ▼
Host List
    │
    ▼
Device
    │
    ▼
DeviceInfo
    │
    ▼
AnalysisResult
    │
    ▼
Report
    │
    ▼
Markdown
```

注意：

整个过程中：

**没有字符串到处传。**

传递的是：

```
Object

↓

Object

↓

Object
```

---

# 十四、对应 Shell 的思维

Shell：

```
stdout

↓

pipe

↓

stdin
```

Python：

```
Object

↓

Function

↓

Object
```

例如：

Shell：

```
printf

↓

ssh

↓

awk

↓

sort

↓

grep
```

Python：

```
Device

↓

collect()

↓

DeviceInfo

↓

analyze()

↓

Report

↓

Markdown
```

---

# 十五、职责划分总结

| 层 | 职责 | 示例 |
|----|------|------|
| Configuration | 外部配置 | YAML、JSON、环境变量 |
| Model | 描述现实世界 | Device、Interface、Route、Report |
| Infrastructure | 访问外部系统 | SSH、HTTP、Kafka、Filesystem、subprocess |
| Service | 提供领域能力 | DeviceService、NomadService、KafkaService |
| Parser | 文本 → 对象 | CLI Parser、JSON Parser |
| Workflow | 编排业务流程 | AuditWorkflow、BackupWorkflow |
| Reporter | 输出结果 | Markdown、HTML、JSON |

---

# 十六、推荐的统一心智模型

```
                  Configuration
                        │
                        ▼
                 Workflow Layer
        (Collect → Parse → Analyze → Report)
                        │
        ┌───────────────┼────────────────┐
        ▼                                ▼
  Service Layer                  Reporter Layer
        │
        ▼
 Infrastructure Layer
(SSH / REST / Kafka / subprocess)
        │
        ▼
 External Systems
(Network Device / Linux / Database)
```

整个项目的数据流：

```
YAML
 │
 ▼
Configuration
 │
 ▼
Workflow
 │
 ▼
Service
 │
 ▼
Infrastructure
 │
 ▼
CLI / API
 │
 ▼
Parser
 │
 ▼
Model
 │
 ▼
Analyzer
 │
 ▼
Report
 │
 ▼
Markdown
```

---

# 十七、Python 自动化的黄金原则

1. **Workflow（做什么）与 Infrastructure（怎么做）彻底分离。**
2. **一切业务数据都应该建模（Model），避免在各层之间传递裸 `dict` 或字符串。**
3. **Service 封装领域能力，Workflow 只负责流程编排。**
4. **Parser 专门负责文本到对象的转换，Reporter 专门负责对象到输出格式的转换。**
5. **对象（Object）是 Python 自动化中的核心数据载体，就像文本流（Text Stream）是 Shell 自动化中的核心数据载体一样。**
6. **遵循单一职责原则（Single Responsibility Principle），每一层只回答一个问题：配置是什么、要做什么、如何访问外部系统、如何解析数据、如何生成结果。**
7. **把项目看成一条"对象流（Object Flow）"，而不是一系列函数调用。整个系统就是对象在不同层之间不断转换和丰富的过程。**