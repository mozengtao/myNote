# DIP Network Automation Case Study

一个完整可运行的网络自动化项目，用来演示 [Python DIP 心智模型教程](../dip_00_index.md)
[Part 8](../dip_08_real_project_case_study.md) 里描述的分层架构如何把 Dependency Inversion
Principle 落到真实的目录结构和模块划分上。

没有任何第三方依赖（不需要 `pip install`），SSH/NETCONF 都是用打印语句模拟的伪实现，突出的是
**依赖方向**而不是协议细节；SQLite 是真实可运行的（用 `:memory:` 数据库）。

## 目录结构与依赖方向

```
dip_network_automation_case_study/
├── main.py                          # Composition Root：唯一知道所有 Detail 的地方
├── domain/                          # Abstraction 所在地：Protocol + 实体，零外部依赖
│   ├── models.py                    #   Device 实体
│   ├── device_connector.py          #   DeviceConnector Port
│   ├── device_repository.py         #   DeviceRepository Port
│   ├── notifier.py                  #   Notifier Port
│   ├── config_loader.py             #   ConfigLoader Port
│   └── logger.py                    #   Logger Port
├── application/                     # High-level Module：只依赖 domain 里的 Port
│   ├── device_onboarding_service.py
│   └── device_inspection_service.py
├── workflows/                       # 编排多个 Application Service 的用例流水线
│   └── device_upgrade_workflow.py
├── api/                             # 对外入口（示例用 CLI 风格函数代表）
│   └── cli.py
├── infrastructure/                  # Low-level Module / Detail：真正实现各个 Port
│   ├── ssh_connector.py             #   DeviceConnector 的 SSH 实现
│   ├── netconf_connector.py         #   DeviceConnector 的 NETCONF 实现
│   ├── sqlite_repository.py         #   DeviceRepository 的真实 SQLite 实现
│   ├── in_memory_repository.py      #   DeviceRepository 的内存实现（测试用）
│   ├── file_config_loader.py        #   ConfigLoader 的本地 JSON 文件实现
│   ├── email_notifier.py            #   Notifier 的邮件实现
│   └── console_logger.py            #   Logger 的终端实现
└── configs/                         # FileConfigLoader 读取的示例设备出厂配置
    ├── RPD-01.json
    └── RPD-02.json
```

依赖方向（源代码 import 方向）：

```
api/ --> workflows/ --> application/ --> domain/ (Port)
                                            ^
                                            |
                                     infrastructure/
```

`domain/` 是唯一不 import 任何其他本项目模块的目录；`infrastructure/` 是唯一同时"认识"
domain 契约和第三方协议/数据库细节的目录；`main.py` 是唯一同时 import `infrastructure/`
和 `application/`/`workflows/`/`api/` 的地方（Composition Root）。

## 如何运行

```bash
cd python/dip_network_automation_case_study
python3 main.py
```

预期输出：设备 RPD-01 走 SSH 协议完成"入网 + 巡检"，设备 RPD-02 走 NETCONF 协议完成同样的
流程——`application/`、`workflows/`、`api/` 目录下的代码完全相同，仅在 `main.py` 里替换了
注入的 `DeviceConnector` 实现。

## 如何替换实现（验证 DIP 的核心价值）

- 把 `SqliteDeviceRepository()` 换成 `InMemoryDeviceRepository()`：`application/` 和
  `workflows/` 代码零改动。
- 新增一种协议（比如 gNMI）：只需要在 `infrastructure/` 下新增一个
  `GnmiConnector`（实现 `fetch_state(host) -> dict`），`main.py` 里多加一行调用即可，
  不需要修改 `domain/`、`application/`、`workflows/`、`api/` 任何一行代码。
- 新增一种通知渠道（比如 Slack）：同理，只需要在 `infrastructure/` 下新增一个满足
  `Notifier.notify(message)` 契约的类。
