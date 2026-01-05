# FRR Management Plane Architecture - Part 2: HOW

## Management Architecture

This document explains the architectural design of FRR's management plane, covering `vtysh` as CLI frontend, daemon backends, and `mgmtd` for YANG-based configuration.

---

## 1. CLI Frontend vs Daemon Backends

```
+------------------------------------------------------------------+
|                 vtysh Client-Daemon Architecture                  |
+------------------------------------------------------------------+

  vtysh (CLI Frontend):
  +----------------------------------------------------------+
  |                                                           |
  |  +-------------------+                                    |
  |  |   User Input      |  readline library                  |
  |  |   (keyboard)      |  history, completion               |
  |  +-------------------+                                    |
  |           |                                               |
  |           v                                               |
  |  +-------------------+                                    |
  |  |  Command Parser   |  lib/command.c                     |
  |  |  (local parse)    |  graph-based matching              |
  |  +-------------------+                                    |
  |           |                                               |
  |           v                                               |
  |  +-------------------+                                    |
  |  |  Daemon Router    |  cmd->daemon flags                 |
  |  |  (which daemon?)  |  VTYSH_BGPD, VTYSH_ZEBRA, etc.     |
  |  +-------------------+                                    |
  |           |                                               |
  |           +--------+--------+--------+                    |
  |           |        |        |        |                    |
  |           v        v        v        v                    |
  |        zebra    bgpd     ospfd    isisd   ...             |
  |        .vty     .vty     .vty     .vty                    |
  |                                                           |
  +----------------------------------------------------------+

  Daemon Backend:
  +----------------------------------------------------------+
  |                                                           |
  |  +-------------------+                                    |
  |  |  VTY Server       |  lib/vty.c                         |
  |  |  (accept conn)    |  Unix domain socket                |
  |  +-------------------+                                    |
  |           |                                               |
  |           v                                               |
  |  +-------------------+                                    |
  |  |  Command Execute  |  daemon-specific                   |
  |  |  (run handler)    |  cmd->func()                       |
  |  +-------------------+                                    |
  |           |                                               |
  |           v                                               |
  |  +-------------------+                                    |
  |  |  Output Response  |  vty_out()                         |
  |  |  (send result)    |  back to vtysh                     |
  |  +-------------------+                                    |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`vtysh`是FRR的CLI前端，它使用readline库提供命令行编辑、历史记录和Tab补全功能。当用户输入命令时，`vtysh`首先在本地解析命令，确定命令应该发送到哪个守护进程（通过`cmd->daemon`标志）。然后通过Unix域套接字将命令发送到对应的守护进程。守护进程端有VTY服务器监听连接，接收命令后执行对应的处理函数，然后将结果返回给`vtysh`显示给用户。

---

## 2. Unix Domain Socket Communication

```
+------------------------------------------------------------------+
|               Unix Domain Socket IPC Mechanism                    |
+------------------------------------------------------------------+

  Socket Path Convention:
  +----------------------------------------------------------+
  |                                                           |
  |  /var/run/frr/               (default vty directory)      |
  |     |                                                     |
  |     +-- zebra.vty            (zebra daemon socket)        |
  |     +-- bgpd.vty             (bgpd daemon socket)         |
  |     +-- ospfd.vty            (ospfd daemon socket)        |
  |     +-- ospfd-1.vty          (ospfd instance 1)           |
  |     +-- isisd.vty            (isisd daemon socket)        |
  |     +-- mgmtd.vty            (mgmtd daemon socket)        |
  |     +-- ...                                               |
  |                                                           |
  +----------------------------------------------------------+

  Connection Flow:
  +----------------------------------------------------------+
  |                                                           |
  |  vtysh startup:                                           |
  |                                                           |
  |    for each daemon in vtysh_client[]:                     |
  |        |                                                  |
  |        +-> socket(AF_UNIX, SOCK_STREAM, 0)                |
  |        |                                                  |
  |        +-> connect("/var/run/frr/{daemon}.vty")           |
  |        |                                                  |
  |        +-> if success: vclient->fd = sock                 |
  |        |                                                  |
  |        +-> if fail: vclient->fd = -1 (daemon not running) |
  |                                                           |
  +----------------------------------------------------------+

  Message Protocol:
  +----------------------------------------------------------+
  |                                                           |
  |  Request (vtysh -> daemon):                               |
  |  +--------------------------------------------------+     |
  |  | command string | NUL terminator |                |     |
  |  +--------------------------------------------------+     |
  |  Example: "show ip route\0"                               |
  |                                                           |
  |  Response (daemon -> vtysh):                              |
  |  +--------------------------------------------------+     |
  |  | output text | NUL | NUL | NUL | return code |    |     |
  |  +--------------------------------------------------+     |
  |  Terminator: 3 NUL bytes followed by return code          |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR使用Unix域套接字进行进程间通信。每个守护进程创建一个以`.vty`结尾的套接字文件（如`/var/run/frr/bgpd.vty`）。当`vtysh`启动时，它会尝试连接到所有已知守护进程的套接字。通信协议很简单：请求是以NUL字符结尾的命令字符串，响应是输出文本后跟三个NUL字节和返回码。这种简单的协议使得调试和实现都很容易。

---

## 3. Configuration Persistence

```
+------------------------------------------------------------------+
|               Configuration File Management                       |
+------------------------------------------------------------------+

  File Hierarchy:
  +----------------------------------------------------------+
  |                                                           |
  |  /etc/frr/                                                |
  |     |                                                     |
  |     +-- frr.conf             (integrated config)          |
  |     |   [contains all daemon configs]                     |
  |     |                                                     |
  |     +-- vtysh.conf           (vtysh-specific settings)    |
  |     |   [username, service integrated-config]             |
  |     |                                                     |
  |     +-- daemons              (which daemons to start)     |
  |     |   [zebra=yes, bgpd=yes, ospfd=no, ...]              |
  |     |                                                     |
  |     +-- zebra.conf           (per-daemon config, legacy)  |
  |     +-- bgpd.conf            (per-daemon config, legacy)  |
  |     +-- ...                                               |
  |                                                           |
  +----------------------------------------------------------+

  Integrated vs Split Configuration:
  +----------------------------------------------------------+
  |                                                           |
  |  Integrated Mode (frr.conf):                              |
  |  +------------------------------------------------------+ |
  |  | ! FRR Configuration                                  | |
  |  |                                                      | |
  |  | hostname router1                                     | |
  |  | !                                                    | |
  |  | interface eth0                                       | |
  |  |  ip address 10.0.0.1/24                              | |
  |  | !                                                    | |
  |  | router bgp 65000                                     | |
  |  |  neighbor 10.0.0.2 remote-as 65001                   | |
  |  | !                                                    | |
  |  | router ospf                                          | |
  |  |  network 10.0.0.0/24 area 0                          | |
  |  | !                                                    | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  Split Mode (per-daemon files):                           |
  |  bgpd.conf:                                               |
  |  +------------------------------------------------------+ |
  |  | router bgp 65000                                     | |
  |  |  neighbor 10.0.0.2 remote-as 65001                   | |
  |  +------------------------------------------------------+ |
  |                                                           |
  +----------------------------------------------------------+

  Write Memory Flow:
  +----------------------------------------------------------+
  |                                                           |
  |  "write memory" command:                                  |
  |                                                           |
  |    1. vtysh sends "show running-config" to each daemon    |
  |    2. Each daemon returns its configuration section       |
  |    3. vtysh collects and merges all sections              |
  |    4. vtysh writes merged config to frr.conf              |
  |                                                           |
  |  Daemon 1 ----+                                           |
  |               |                                           |
  |  Daemon 2 ----+----> vtysh -----> /etc/frr/frr.conf       |
  |               |      (merge)                              |
  |  Daemon 3 ----+                                           |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR支持两种配置模式：集成模式和分离模式。在集成模式下，所有守护进程的配置都保存在一个`frr.conf`文件中；在分离模式下，每个守护进程有自己的配置文件。当执行"write memory"命令时，`vtysh`会向每个守护进程发送"show running-config"命令，收集各自的配置片段，然后合并写入配置文件。这个过程确保了配置文件始终反映当前运行状态。

---

## 4. Command Registration and Dispatch

```
+------------------------------------------------------------------+
|               Command Registration Architecture                   |
+------------------------------------------------------------------+

  Command Definition Macro:
  +----------------------------------------------------------+
  |                                                           |
  |  DEFUN (command_name,                                     |
  |         command_name_cmd,                                 |
  |         "show ip route [json]",      <- command string    |
  |         SHOW_STR                     <- help strings      |
  |         IP_STR                                            |
  |         "Routing table\n"                                 |
  |         JSON_STR)                                         |
  |  {                                                        |
  |      /* command implementation */                         |
  |      return CMD_SUCCESS;                                  |
  |  }                                                        |
  |                                                           |
  +----------------------------------------------------------+

  DEFUNSH - Shell-only Command (vtysh handles locally):
  +----------------------------------------------------------+
  |                                                           |
  |  DEFUNSH (VTYSH_ALL,           <- daemon flags            |
  |           vtysh_exit,                                     |
  |           vtysh_exit_cmd,                                 |
  |           "exit",                                         |
  |           "Exit current mode\n")                          |
  |  {                                                        |
  |      /* handled in vtysh, not sent to daemons */          |
  |      return vtysh_exit(vty);                              |
  |  }                                                        |
  |                                                           |
  +----------------------------------------------------------+

  Command Node Hierarchy:
  +----------------------------------------------------------+
  |                                                           |
  |  VIEW_NODE (unprivileged)                                 |
  |      |                                                    |
  |      +-- "enable" --> ENABLE_NODE                         |
  |                           |                               |
  |                           +-- "configure" --> CONFIG_NODE |
  |                                                   |       |
  |                   +--------+--------+--------+----+       |
  |                   |        |        |        |            |
  |                   v        v        v        v            |
  |              INTERFACE  BGP_NODE  OSPF_NODE  VRF_NODE     |
  |              _NODE                                        |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR使用宏来定义命令。`DEFUN`宏定义守护进程内部的命令，`DEFUNSH`宏定义`vtysh`本地处理的命令。每个命令都关联一个或多个守护进程标志（如`VTYSH_BGPD`），`vtysh`根据这些标志决定将命令发送到哪些守护进程。命令按节点组织，形成层次结构：从VIEW_NODE（非特权模式）到ENABLE_NODE（特权模式），再到CONFIG_NODE（配置模式），然后进入各种子节点（如BGP_NODE、OSPF_NODE等）。

---

## 5. mgmtd Architecture

```
+------------------------------------------------------------------+
|                   mgmtd Daemon Architecture                       |
+------------------------------------------------------------------+

  Overall Structure:
  +----------------------------------------------------------+
  |                                                           |
  |                    +-------------------+                  |
  |                    |      mgmtd        |                  |
  |                    +-------------------+                  |
  |                    |                   |                  |
  |   Frontend         |  +-----------+    |    Backend       |
  |   Adapters         |  | Datastore |    |    Adapters      |
  |                    |  | Manager   |    |                  |
  |  +---------+       |  +-----------+    |       +--------+ |
  |  | vtysh   |<----->|       |           |<----->| zebra  | |
  |  +---------+       |       v           |       +--------+ |
  |                    |  +-----------+    |                  |
  |  +---------+       |  | YANG      |    |       +--------+ |
  |  | NETCONF |<----->|  | Models    |    |<----->| ripd   | |
  |  +---------+       |  +-----------+    |       +--------+ |
  |                    |       |           |                  |
  |  +---------+       |       v           |       +--------+ |
  |  | gRPC    |<----->|  +-----------+    |<----->|staticd | |
  |  +---------+       |  |Transaction|    |       +--------+ |
  |                    |  | Manager   |    |                  |
  |                    |  +-----------+    |                  |
  |                    |                   |                  |
  |                    +-------------------+                  |
  |                                                           |
  +----------------------------------------------------------+

  Datastore Model:
  +----------------------------------------------------------+
  |                                                           |
  |  +-------------------+     +-------------------+          |
  |  |    Candidate      |     |     Running       |          |
  |  |    Datastore      |     |    Datastore      |          |
  |  |                   |     |                   |          |
  |  | (edit buffer)     |     | (active config)   |          |
  |  | (uncommitted)     |     | (committed)       |          |
  |  +-------------------+     +-------------------+          |
  |           |                         |                     |
  |           | validate                | apply               |
  |           | commit                  |                     |
  |           v                         v                     |
  |  +-------------------------------------------+            |
  |  |           Backend Clients                 |            |
  |  |  (zebra, ripd, staticd, ...)              |            |
  |  +-------------------------------------------+            |
  |                                                           |
  +----------------------------------------------------------+

  Transaction Flow:
  +----------------------------------------------------------+
  |                                                           |
  |  1. Frontend receives config request                      |
  |     |                                                     |
  |  2. Edit applied to Candidate datastore                   |
  |     |                                                     |
  |  3. User issues "commit"                                  |
  |     |                                                     |
  |  4. mgmtd validates changes against YANG schema           |
  |     |                                                     |
  |  5. mgmtd creates transaction                             |
  |     |                                                     |
  |  6. Changes sent to relevant backend clients              |
  |     |                                                     |
  |  7. Backend clients apply changes                         |
  |     |                                                     |
  |  8. On success: Candidate merged to Running               |
  |     On failure: Candidate rolled back                     |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`mgmtd`是FRR的新一代管理守护进程，基于YANG模型提供事务性配置管理。它包含三个主要组件：前端适配器（处理来自`vtysh`、NETCONF、gRPC等的请求）、数据存储管理器（管理候选和运行数据存储）、后端适配器（与实际的路由守护进程通信）。配置修改首先应用到候选数据存储，经过YANG模式验证后提交到运行数据存储，然后分发给相关的后端客户端。如果任何步骤失败，整个事务会回滚。

---

## 6. Command Dispatch Flow

```
+------------------------------------------------------------------+
|               Complete Command Execution Flow                     |
+------------------------------------------------------------------+

  User types "show ip bgp summary" in vtysh:
  +----------------------------------------------------------+
  |                                                           |
  |  1. Readline captures input                               |
  |     |                                                     |
  |  2. vtysh_execute() called                                |
  |     |                                                     |
  |  3. cmd_make_strvec() tokenizes input                     |
  |     |  ["show", "ip", "bgp", "summary"]                   |
  |     |                                                     |
  |  4. cmd_execute() matches against command graph           |
  |     |  Returns: CMD_SUCCESS_DAEMON                        |
  |     |  cmd->daemon = VTYSH_BGPD                           |
  |     |                                                     |
  |  5. For each daemon matching cmd->daemon:                 |
  |     |                                                     |
  |     5a. vtysh_client_execute(&vtysh_client[bgpd])         |
  |         |                                                 |
  |         5b. write(vclient->fd, "show ip bgp summary\0")   |
  |             |                                             |
  |             5c. bgpd receives command                     |
  |                 |                                         |
  |                 5d. bgpd executes show_ip_bgp_summary()   |
  |                     |                                     |
  |                     5e. bgpd writes output to socket      |
  |                         |                                 |
  |         5f. vtysh reads response                          |
  |             |                                             |
  |  6. vtysh displays output to user                         |
  |                                                           |
  +----------------------------------------------------------+

  Multi-daemon Command Example ("show running-config"):
  +----------------------------------------------------------+
  |                                                           |
  |  cmd->daemon = VTYSH_ALL (all daemons)                    |
  |                                                           |
  |  vtysh iterates through vtysh_client[]:                   |
  |                                                           |
  |    +-> mgmtd  ---> "show running-config" ---> response    |
  |    |                                                      |
  |    +-> zebra  ---> "show running-config" ---> response    |
  |    |                                                      |
  |    +-> bgpd   ---> "show running-config" ---> response    |
  |    |                                                      |
  |    +-> ospfd  ---> "show running-config" ---> response    |
  |    |                                                      |
  |    +-> ... (all connected daemons)                        |
  |                                                           |
  |  vtysh_config_parse_line() merges all responses           |
  |  vtysh_config_dump() outputs merged configuration         |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

命令执行流程从用户输入开始：readline库捕获输入，`vtysh_execute()`被调用，命令被分词并与命令图匹配。根据匹配结果的`cmd->daemon`标志，命令被发送到对应的守护进程。对于多守护进程命令（如"show running-config"），`vtysh`会遍历所有连接的守护进程，收集响应并合并显示。这种设计允许一个命令透明地在多个守护进程上执行。

---

## Summary

| Component | Role | Communication |
|-----------|------|---------------|
| vtysh | CLI frontend, command dispatch | Unix sockets to daemons |
| mgmtd | YANG-based config management | Frontend/Backend adapters |
| Daemons | Protocol implementation | VTY server on Unix socket |
| Config Files | Persistent storage | Read at startup, write on demand |

The management architecture cleanly separates user interface concerns from protocol implementation, enabling flexible configuration management without impacting routing operations.

---

**Previous:** [Part 1: WHY - Why Management Is Separate](management_plane_01_why.md)

**Next:** [Part 3: WHAT - Key Components](management_plane_03_what.md)
