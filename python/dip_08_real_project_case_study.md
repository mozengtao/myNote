[← 返回目录](dip_00_index.md) | [上一篇: Part 7 设计模式](dip_07_design_patterns.md)

# Part 8 - 真实 Python 项目案例：network-automation

本部分分析一个典型的网络自动化项目目录结构，指出每个目录分别对应 DIP 里的哪个角色，然后给出
一份完整可运行的落地实现：[python/dip_network_automation_case_study/](dip_network_automation_case_study/README.md)。

## 项目目录结构

```
network-automation/
├── domain/            # 实体 + 抽象契约（Port）
├── application/        # 用例编排（Service）
├── infrastructure/      # 具体技术实现（Adapter / Detail）
├── api/                # 对外入口（HTTP/CLI/gRPC）
└── workflows/           # 跨多个用例的业务流水线编排
```

## 角色映射

```
+---------------------------------------------------------------------+
|  目录            |  DIP 角色              |  说明                    |
+---------------------------------------------------------------------+
|  domain/          |  Abstraction + Entity  |  Port 接口 + 领域实体，   |
|                    |  (同时也是 High-level  |  零外部依赖，是整个项目   |
|                    |   Module 的一部分)     |  里最稳定的代码            |
+---------------------------------------------------------------------+
|  application/      |  High-level Module     |  只依赖 domain/ 定义的    |
|                    |                        |  Port，编排单个用例        |
+---------------------------------------------------------------------+
|  workflows/        |  High-level Module     |  编排多个 application/    |
|                    |  (更高一层的编排)       |  服务，组成完整业务流程     |
+---------------------------------------------------------------------+
|  infrastructure/    |  Low-level Module      |  真正实现 domain/ 里的     |
|                    |  + Detail              |  Port（SSH/NETCONF/DB等） |
+---------------------------------------------------------------------+
|  api/              |  Interface Adapter     |  把外部请求(HTTP/CLI)转   |
|                    |  (Driving Adapter)     |  换成对 workflows/ 的调用  |
+---------------------------------------------------------------------+
```

## 完整架构图

```
+-------------------------------------------------------------------+
|  api/ (Driving Adapter)                                              |
|  cli.run_upgrade_command(workflow, device_id, host)                  |
+------------------------------+--------------------------------------+
                                | 调用（依赖）
                                v
+-------------------------------------------------------------------+
|  workflows/ (High-level：跨用例编排)                                  |
|  DeviceUpgradeWorkflow(onboarding, inspection, logger)                |
+------------------------------+--------------------------------------+
                                | 调用（依赖）
                                v
+-------------------------------------------------------------------+
|  application/ (High-level：单用例编排)                                |
|  DeviceOnboardingService / DeviceInspectionService                    |
+------------------------------+--------------------------------------+
                                | 依赖（只认识抽象）
                                v
+-------------------------------------------------------------------+
|  domain/ (Abstraction：Port + Entity)                                |
|  DeviceConnector / DeviceRepository / Notifier / ConfigLoader / Logger|
|  Device (Entity)                                                       |
+------------------------------+--------------------------------------+
                                ^ 实现（反向依赖）
                                |
+-------------------------------------------------------------------+
|  infrastructure/ (Low-level Module / Detail)                          |
|  SshConnector / NetconfConnector / SqliteDeviceRepository /            |
|  EmailNotifier / FileConfigLoader / ConsoleLogger                      |
+-------------------------------------------------------------------+
```

## 依赖方向说明

```
源代码依赖方向（箭头 = import 方向）：

api/  ---->  workflows/  ---->  application/  ---->  domain/ (Port)
                                                          ^
                                                          |
                                                    infrastructure/

main.py（Composition Root）同时 import 上面所有层，负责组装完整对象图。
```

关键规则：

1. `domain/` 不 import 项目里任何其他模块——它是最内层的稳定核心。
2. `application/` 和 `workflows/` 只 import `domain/`，绝不 import `infrastructure/`。
3. `infrastructure/` 结构化满足（或显式实现）`domain/` 里的 Port，因此必然要"认识"
   `domain/`，这正是箭头"从 Detail 指向 Abstraction"的体现。
4. 只有 `main.py` 允许同时 import `application/`、`workflows/`、`api/`、`infrastructure/`——
   这是唯一一处"高层和细节在同一个文件里相遇"的地方，因为它的职责就是**组装**，而不是
   **业务逻辑**。

## 完整实现

参见可运行项目：[python/dip_network_automation_case_study/](dip_network_automation_case_study/README.md)，
其中完整实现了：

- **Repository**：`domain/device_repository.py`（Port） + `infrastructure/sqlite_repository.py` /
  `infrastructure/in_memory_repository.py`（Detail）。
- **Device Connector（相当于 SSH Client 的抽象）**：`domain/device_connector.py`（Port） +
  `infrastructure/ssh_connector.py` / `infrastructure/netconf_connector.py`（Detail）。
- **Notification**：`domain/notifier.py`（Port） + `infrastructure/email_notifier.py`（Detail）。
- **Logger**：`domain/logger.py`（Port） + `infrastructure/console_logger.py`（Detail）。
- **Config Loader**：`domain/config_loader.py`（Port） + `infrastructure/file_config_loader.py`（Detail）。

以上全部通过 Constructor Injection 组装于 [main.py](dip_network_automation_case_study/main.py)，
运行方式见项目 README。运行效果摘要：

```
----- 设备 1：走 SSH 协议 -----
[INFO] === starting workflow for RPD-01 ===
[INFO] onboarded RPD-01 (E6000, fw=3.2.1)
[Email via smtp.corp.com -> noc@corp.com] Device RPD-01 onboarded successfully
[SSH user=admin timeout=10] connecting to 10.0.0.1 ...
[INFO] inspected RPD-01: {'protocol': 'ssh', 'host': '10.0.0.1', 'interfaces_up': 24}

----- 设备 2：走 NETCONF 协议（连接器替换，其余代码零改动） -----
[INFO] === starting workflow for RPD-02 ===
[INFO] onboarded RPD-02 (E6000, fw=3.3.0)
[NETCONF] ncclient.manager.connect(10.0.0.2:830)
[INFO] inspected RPD-02: {'protocol': 'netconf', 'host': '10.0.0.2', 'interfaces_up': 24}
```

同一套 `application/`、`workflows/`、`api/` 代码，仅仅通过在 `main.py` 里替换注入的
`DeviceConnector` 实现，就完成了从 SSH 到 NETCONF 的协议切换——这正是本教程从 Part 1 到
Part 7 所有原理最终要在真实项目里达到的效果。

下一步：进入 [Part 9 - Pythonic 的 DIP](dip_09_pythonic_dip.md)，讨论什么时候该用
Protocol、什么时候该用 ABC、什么时候完全不需要接口。
