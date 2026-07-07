# 示例六：VMC 批量重启自动化（分层架构 / Infrastructure Layer）

## 场景

网络自动化里非常常见的一类任务：通过 SSH 登录一台运维终端，对一批 VMC（虚拟设备）逐个下发交互式重启命令（下发命令后需要应答 `Are you sure?` 确认）。

和前 5 个示例不同，本示例不是"一个 `main.py` 里放所有代码"，而是完整演示一种更贴近真实网络自动化项目的**分层架构**：

```
Application
    |
    v
Workflow
    |
    v
Domain Object / Domain Service
    |
    v
Infrastructure (InteractiveShell)
    |
    v
Infrastructure (SSH / Paramiko)
```

每一层只知道自己下面那一层暴露出来的"接口"，完全不知道再下面几层是怎么实现的。

## 目录结构

```
vmc_reboot_automation/
+-- main.py                          # Application 层：只负责组装对象
+-- infrastructure/
|   +-- ssh_client.py                # 真实 SSH 连接（生产环境，依赖 paramiko）
|   +-- fake_ssh_client.py           # 本地模拟连接（本示例默认使用，无需 paramiko/真实设备）
|   +-- interactive_shell.py         # 通用的交互式 shell：send / recv_until / drain
+-- domain/
|   +-- vmc.py                       # 领域对象 VMC（只是一个名字）
|   +-- vmc_service.py               # 领域服务：VMC 能做什么（reboot）
+-- workflow/
    +-- reboot_workflow.py           # 编排完整的批量重启流程
```

## 六层说明

| 层 | 文件 | 知道什么 | 不知道什么 |
|---|---|---|---|
| Infrastructure（SSH） | `infrastructure/ssh_client.py` | 怎么建立 SSH 连接、怎么打开 shell | VMC 是什么、要发什么命令 |
| Infrastructure（Shell） | `infrastructure/interactive_shell.py` | 怎么 send / 怎么等 marker / 怎么 drain | VMC、reboot 命令的具体内容 |
| Domain Object | `domain/vmc.py` | VMC 有一个 `name` | SSH、channel、命令 |
| Domain Service | `domain/vmc_service.py` | VMC 能被 reboot，需要经过什么确认步骤 | channel、socket、paramiko |
| Workflow | `workflow/reboot_workflow.py` | 重启的完整业务步骤顺序（遍历 -> 创建 VMC -> 调用 Service -> 收集结果） | SSH、paramiko、recv() |
| Application | `main.py` | 怎么把上面这些对象组装起来 | 具体的重启逻辑怎么实现 |

## 对应的心智模型要点

对照 [`complex_obj_organization.md`](../../complex_obj_organization.md) 第十七节 Checklist 与第二十节分层架构：

| Checklist 原则 | 本示例中的体现 |
|---|---|
| Single Responsibility | 每一层只做一件事：SSH 连接、交互式会话、领域对象、领域服务、流程编排、装配，六件事分在六个文件里 |
| Service 无状态 | `VMCService` 不持有 VMC 列表，只在 `reboot(vmc)` 被调用时临时消费传入的 VMC |
| Object Flow | `VMC` 对象从 `RebootWorkflow` 创建后，一路"流"进 `VMCService.reboot()`，从未被转换成 dict |
| Workflow 驱动 Object Flow | `RebootWorkflow.execute()` 里 `遍历 -> 创建 VMC -> reboot -> 收集结果` 就是一条清晰的对象流水线 |
| Infrastructure 可替换 | `fake_ssh_client.py` 与 `ssh_client.py` 对外接口完全一致，替换后 `domain/`、`workflow/` 代码不用改一行 |

## 与前 5 个示例的区别

前 5 个示例（`library_lending_system` 等）重点演示的是**领域对象内部**如何组织（Object Graph、Encapsulation、Repository、Factory、Builder），代码都写在单个 `main.py` 里。

本示例额外演示的是：当项目规模变大、且明确涉及外部系统（SSH/网络设备）时，如何把 **DDD 分层 + Object Flow + Infrastructure Layer** 落到真实的目录结构和模块划分上——这是网络自动化项目里非常常见的组织方式。

## 运行方式

本示例默认使用 `infrastructure/fake_ssh_client.py` 模拟 SSH 交互，**不需要安装 paramiko，也不需要真实的 VMC 设备**：

```bash
cd python/complex_obj_organization_examples/vmc_reboot_automation
python3 main.py
```

如果要接入真实设备，只需要修改 `main.py` 里的一行 import：

```python
# 把这一行
from infrastructure.fake_ssh_client import FakeSSHConnection
# 换成
from infrastructure.ssh_client import SSHConnection
```

并安装 `pip install paramiko`，`domain/` 和 `workflow/` 目录下的代码完全不用改动。
