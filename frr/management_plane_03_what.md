# FRR Management Plane Architecture - Part 3: WHAT

## Key Components

This document details the key components of FRR's management plane, including command definition macros, configuration nodes, runtime vs startup config, and IPC between mgmtd and daemons.

---

## 1. Command Definition Macros

```
+------------------------------------------------------------------+
|                    Command Definition System                      |
+------------------------------------------------------------------+

  DEFUN - Standard Command Definition:
  +----------------------------------------------------------+
  |                                                           |
  |  DEFUN (function_name,         <- C function name         |
  |         cmd_element_name,      <- struct cmd_element      |
  |         "command string",      <- CLI syntax              |
  |         "Help string 1\n"      <- Per-token help          |
  |         "Help string 2\n")                                |
  |  {                                                        |
  |      /* argc = number of arguments */                     |
  |      /* argv = array of cmd_token pointers */             |
  |      vty_out(vty, "Output text\n");                       |
  |      return CMD_SUCCESS;                                  |
  |  }                                                        |
  |                                                           |
  +----------------------------------------------------------+

  Macro Variants:
  +----------------------------------------------------------+
  |                                                           |
  |  DEFUN      - Full command with implementation            |
  |  DEFUNSH    - Shell (vtysh) only command                  |
  |  DEFPY      - Python-style argument extraction            |
  |  DEFUN_YANG - YANG-integrated command (for mgmtd)         |
  |  ALIAS      - Command alias (shares implementation)       |
  |  DEFSH      - Shell command without function body         |
  |                                                           |
  +----------------------------------------------------------+

  DEFPY Example (Modern Style):
  +----------------------------------------------------------+
  |                                                           |
  |  DEFPY (bgp_neighbor_remote_as,                           |
  |         bgp_neighbor_remote_as_cmd,                       |
  |         "neighbor <A.B.C.D|X:X::X:X|WORD>$neighbor "      |
  |         "remote-as <(1-4294967295)$as|internal|external>",|
  |         NEIGHBOR_STR                                      |
  |         NEIGHBOR_ADDR_STR                                 |
  |         "Specify a BGP neighbor\n"                        |
  |         AS_STR                                            |
  |         "Internal BGP peer\n"                             |
  |         "External BGP peer\n")                            |
  |  {                                                        |
  |      /* Variables automatically extracted:                |
  |       * const char *neighbor (from $neighbor)             |
  |       * long as (from $as, or 0 if internal/external)     |
  |       */                                                  |
  |      return bgp_vty_return(vty,                           |
  |                  peer_remote_as(bgp, neighbor, as, ...)); |
  |  }                                                        |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR使用一系列宏来定义CLI命令。`DEFUN`是最基本的命令定义宏，它创建一个C函数和对应的`cmd_element`结构。`DEFPY`是现代风格的宏，支持自动参数提取：在命令字符串中使用`$name`语法，参数会自动提取为同名的C变量。`DEFUNSH`定义仅在`vtysh`中处理的命令，不会发送到守护进程。`DEFUN_YANG`用于YANG集成的命令，会生成对应的YANG数据路径。

---

## 2. Configuration Nodes

```
+------------------------------------------------------------------+
|                   Configuration Node Hierarchy                    |
+------------------------------------------------------------------+

  Node Tree Structure:
  +----------------------------------------------------------+
  |                                                           |
  |  AUTH_NODE                  (login authentication)        |
  |      |                                                    |
  |      v                                                    |
  |  VIEW_NODE ----------------+  (unprivileged mode)         |
  |      |                     |                              |
  |      | "enable"            | restricted commands only     |
  |      v                     |                              |
  |  ENABLE_NODE --------------+  (privileged mode)           |
  |      |                                                    |
  |      | "configure terminal"                               |
  |      v                                                    |
  |  CONFIG_NODE                  (global configuration)      |
  |      |                                                    |
  |      +-- "interface eth0" ------> INTERFACE_NODE          |
  |      |                                                    |
  |      +-- "router bgp 65000" ----> BGP_NODE                |
  |      |       |                                            |
  |      |       +-- "address-family ipv4" -> BGP_IPV4_NODE   |
  |      |       +-- "neighbor X" ---------> BGP_PEER_NODE    |
  |      |                                                    |
  |      +-- "router ospf" ---------> OSPF_NODE               |
  |      |       |                                            |
  |      |       +-- "area 0" ---------> OSPF_AREA_NODE       |
  |      |                                                    |
  |      +-- "vrf NAME" ------------> VRF_NODE                |
  |      |                                                    |
  |      +-- "route-map NAME" ------> RMAP_NODE               |
  |      |                                                    |
  |      +-- "key chain NAME" ------> KEYCHAIN_NODE           |
  |                                                           |
  +----------------------------------------------------------+

  Node Definition:
  +----------------------------------------------------------+
  |                                                           |
  |  static struct cmd_node bgp_node = {                      |
  |      .name = "bgp",                                       |
  |      .node = BGP_NODE,           <- enum node_type        |
  |      .parent_node = CONFIG_NODE, <- parent in hierarchy   |
  |      .prompt = "%s(config-router)# ",                     |
  |  };                                                       |
  |                                                           |
  |  /* In daemon initialization: */                          |
  |  install_node(&bgp_node, bgp_config_write);               |
  |                                                           |
  |  /* Install commands under this node: */                  |
  |  install_element(BGP_NODE, &neighbor_remote_as_cmd);      |
  |                                                           |
  +----------------------------------------------------------+

  vtysh Node Mirroring:
  +----------------------------------------------------------+
  |                                                           |
  |  vtysh maintains its own node definitions:                |
  |                                                           |
  |  static struct cmd_node bgp_node = {                      |
  |      .name = "bgp",                                       |
  |      .node = BGP_NODE,                                    |
  |      .parent_node = CONFIG_NODE,                          |
  |      .prompt = "%s(config-router)# ",                     |
  |  };                                                       |
  |                                                           |
  |  /* vtysh needs matching nodes to track CLI context       |
  |   * even though actual commands run in daemons */         |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR的命令按节点组织。节点形成树状层次结构，从AUTH_NODE（认证）开始，经过VIEW_NODE（非特权）、ENABLE_NODE（特权）、CONFIG_NODE（配置），然后分支到各种特定配置节点。每个节点定义包括名称、枚举值、父节点和命令提示符。`vtysh`维护与守护进程相同的节点定义，以便正确跟踪CLI上下文，尽管实际命令在守护进程中执行。

---

## 3. Runtime vs Startup Config

```
+------------------------------------------------------------------+
|              Runtime vs Startup Configuration                     |
+------------------------------------------------------------------+

  Configuration States:
  +----------------------------------------------------------+
  |                                                           |
  |  +-------------------+      +-------------------+         |
  |  |  Startup Config   |      |  Running Config   |         |
  |  |  (frr.conf file)  |      |  (in memory)      |         |
  |  +-------------------+      +-------------------+         |
  |           |                          ^                    |
  |           |                          |                    |
  |           | read at daemon start     | "write memory"     |
  |           |                          | "copy run start"   |
  |           v                          |                    |
  |  +-------------------------------------------+            |
  |  |           Running Configuration           |            |
  |  |           (active in daemons)             |            |
  |  +-------------------------------------------+            |
  |                      |                                    |
  |                      | runtime changes via CLI            |
  |                      |                                    |
  |                      v                                    |
  |  +-------------------------------------------+            |
  |  |           Modified Running Config         |            |
  |  |           (unsaved changes)               |            |
  |  +-------------------------------------------+            |
  |                                                           |
  +----------------------------------------------------------+

  Configuration Commands:
  +----------------------------------------------------------+
  |                                                           |
  |  "show running-config"                                    |
  |      Display current active configuration                 |
  |      (what's actually running now)                        |
  |                                                           |
  |  "show startup-config"                                    |
  |      Display saved configuration file                     |
  |      (what will load on next restart)                     |
  |                                                           |
  |  "write memory" / "copy running-config startup-config"    |
  |      Save running config to startup config file           |
  |                                                           |
  |  "copy startup-config running-config"                     |
  |      Reload startup config (dangerous, not recommended)   |
  |                                                           |
  +----------------------------------------------------------+

  mgmtd Datastore Model:
  +----------------------------------------------------------+
  |                                                           |
  |  +-------------------+     +-------------------+          |
  |  |    Candidate      |     |     Running       |          |
  |  |    Datastore      |     |    Datastore      |          |
  |  +-------------------+     +-------------------+          |
  |           |                         |                     |
  |  User edits here          Active configuration            |
  |  (staging area)           (pushed to backends)            |
  |           |                         |                     |
  |           +--- "commit" --->--------+                     |
  |           |                                               |
  |           +--- "discard" --> changes lost                 |
  |                                                           |
  |  +-------------------+                                    |
  |  |    Operational    |                                    |
  |  |    Datastore      |                                    |
  |  +-------------------+                                    |
  |           |                                               |
  |  Read-only state                                          |
  |  (interfaces, counters, etc.)                             |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR区分运行配置和启动配置。启动配置保存在文件中（如`frr.conf`），守护进程启动时读取。运行配置是当前活动的配置，保存在内存中。用户可以在运行时修改配置，但这些更改只有在执行"write memory"后才会保存到启动配置。`mgmtd`引入了更精细的数据存储模型：候选存储（编辑缓冲区）、运行存储（活动配置）和操作存储（只读状态数据）。

---

## 4. IPC Between mgmtd and Daemons

```
+------------------------------------------------------------------+
|                mgmtd Frontend and Backend IPC                     |
+------------------------------------------------------------------+

  Frontend Communication (vtysh <-> mgmtd):
  +----------------------------------------------------------+
  |                                                           |
  |  vtysh                            mgmtd                   |
  |    |                                |                     |
  |    |   vty_mgmt_fe_connected()      |                     |
  |    +------------------------------->|                     |
  |    |                                |                     |
  |    |   FE_SESSION_REQ               |                     |
  |    +------------------------------->|                     |
  |    |                                |                     |
  |    |   FE_SESSION_REPLY             |                     |
  |    |<-------------------------------+                     |
  |    |                                |                     |
  |    |   FE_SETCFG_REQ                |                     |
  |    +------------------------------->|                     |
  |    |   (configuration change)       |                     |
  |    |                                |                     |
  |    |   FE_SETCFG_REPLY              |                     |
  |    |<-------------------------------+                     |
  |    |                                |                     |
  |    |   FE_COMMITCFG_REQ             |                     |
  |    +------------------------------->|                     |
  |    |   (commit changes)             |                     |
  |    |                                |                     |
  |                                                           |
  +----------------------------------------------------------+

  Backend Communication (mgmtd <-> daemons):
  +----------------------------------------------------------+
  |                                                           |
  |  mgmtd                            zebra/ripd/staticd      |
  |    |                                |                     |
  |    |   BE_ADAPTER connects          |                     |
  |    |<-------------------------------+                     |
  |    |                                |                     |
  |    |   BE_TXN_CREATE                |                     |
  |    +------------------------------->|                     |
  |    |                                |                     |
  |    |   BE_TXN_REPLY                 |                     |
  |    |<-------------------------------+                     |
  |    |                                |                     |
  |    |   BE_CFGDATA_CREATE            |                     |
  |    +------------------------------->|                     |
  |    |   (YANG data changes)          |                     |
  |    |                                |                     |
  |    |   BE_CFGDATA_REPLY             |                     |
  |    |<-------------------------------+                     |
  |    |                                |                     |
  |    |   BE_CFGAPPLY                  |                     |
  |    +------------------------------->|                     |
  |    |   (apply configuration)        |                     |
  |    |                                |                     |
  |                                                           |
  +----------------------------------------------------------+

  XPATH Mapping for Backend Dispatch:
  +----------------------------------------------------------+
  |                                                           |
  |  XPath Prefix                    Backend Client           |
  |  ---------------------------------------------------------|
  |  /frr-zebra:zebra                zebra                    |
  |  /frr-interface:lib              zebra                    |
  |  /frr-vrf:lib                    zebra                    |
  |  /frr-ripd:ripd                  ripd                     |
  |  /frr-routing:.../staticd        staticd                  |
  |  /frr-filter:lib                 zebra, ripd, ripngd      |
  |  /frr-route-map:lib              zebra, ripd, ripngd      |
  |                                                           |
  |  mgmtd routes config changes to appropriate backends      |
  |  based on YANG xpath prefix matching                      |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`mgmtd`通过两组接口进行通信：前端适配器处理来自`vtysh`和其他客户端的请求，后端适配器与路由守护进程通信。前端协议包括会话建立、配置设置和提交请求。后端协议包括事务创建、配置数据传输和应用。`mgmtd`根据YANG路径前缀将配置更改路由到适当的后端客户端。例如，`/frr-ripd:ripd`路径的更改会发送到`ripd`，而`/frr-zebra:zebra`路径的更改会发送到`zebra`。

---

## 5. vtysh Client Array

```
+------------------------------------------------------------------+
|                    vtysh Client Management                        |
+------------------------------------------------------------------+

  Client Definition:
  +----------------------------------------------------------+
  |                                                           |
  |  struct vtysh_client {                                    |
  |      int fd;                    /* socket to daemon */    |
  |      const char *name;          /* daemon name */         |
  |      int flag;                  /* VTYSH_* flag */        |
  |      char path[MAXPATHLEN];     /* socket path */         |
  |      struct vtysh_client *next; /* instances list */      |
  |      int log_fd;                /* log socket */          |
  |      struct event *log_reader;  /* log event */           |
  |  };                                                       |
  |                                                           |
  +----------------------------------------------------------+

  Client Array:
  +----------------------------------------------------------+
  |                                                           |
  |  struct vtysh_client vtysh_client[] = {                   |
  |      {.name = "mgmtd",    .flag = VTYSH_MGMTD},           |
  |      {.name = "zebra",    .flag = VTYSH_ZEBRA},           |
  |      {.name = "ripd",     .flag = VTYSH_RIPD},            |
  |      {.name = "ripngd",   .flag = VTYSH_RIPNGD},          |
  |      {.name = "ospfd",    .flag = VTYSH_OSPFD},           |
  |      {.name = "ospf6d",   .flag = VTYSH_OSPF6D},          |
  |      {.name = "ldpd",     .flag = VTYSH_LDPD},            |
  |      {.name = "bgpd",     .flag = VTYSH_BGPD},            |
  |      {.name = "isisd",    .flag = VTYSH_ISISD},           |
  |      {.name = "pimd",     .flag = VTYSH_PIMD},            |
  |      {.name = "nhrpd",    .flag = VTYSH_NHRPD},           |
  |      {.name = "eigrpd",   .flag = VTYSH_EIGRPD},          |
  |      {.name = "babeld",   .flag = VTYSH_BABELD},          |
  |      {.name = "sharpd",   .flag = VTYSH_SHARPD},          |
  |      {.name = "fabricd",  .flag = VTYSH_FABRICD},         |
  |      {.name = "watchfrr", .flag = VTYSH_WATCHFRR},        |
  |      {.name = "pbrd",     .flag = VTYSH_PBRD},            |
  |      {.name = "staticd",  .flag = VTYSH_STATICD},         |
  |      {.name = "bfdd",     .flag = VTYSH_BFDD},            |
  |      {.name = "vrrpd",    .flag = VTYSH_VRRPD},           |
  |      {.name = "pathd",    .flag = VTYSH_PATHD},           |
  |      {.name = "pim6d",    .flag = VTYSH_PIM6D},           |
  |  };                                                       |
  |                                                           |
  +----------------------------------------------------------+

  Daemon Flags Usage:
  +----------------------------------------------------------+
  |                                                           |
  |  /* Command targets specific daemon(s) */                 |
  |  DEFUN (show_ip_bgp,                                      |
  |         show_ip_bgp_cmd,                                  |
  |         "show ip bgp",                                    |
  |         ...)                                              |
  |  /* Installed with: */                                    |
  |  install_element(VIEW_NODE, &show_ip_bgp_cmd);            |
  |  /* cmd->daemon = VTYSH_BGPD */                           |
  |                                                           |
  |  /* Command targets multiple daemons */                   |
  |  DEFUNSH (VTYSH_ZEBRA | VTYSH_BGPD,                       |
  |           show_interface,                                 |
  |           ...)                                            |
  |  /* Sent to both zebra and bgpd */                        |
  |                                                           |
  |  /* Command targets all daemons */                        |
  |  DEFUNSH (VTYSH_ALL,                                      |
  |           show_running_config,                            |
  |           ...)                                            |
  |  /* Sent to every connected daemon */                     |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`vtysh`维护一个客户端数组，每个条目代表一个守护进程。每个客户端结构包含套接字文件描述符、守护进程名称、标志和套接字路径。标志用于确定命令应该发送到哪些守护进程：`VTYSH_BGPD`表示命令只发送到`bgpd`，`VTYSH_ZEBRA | VTYSH_BGPD`表示发送到`zebra`和`bgpd`两个守护进程，`VTYSH_ALL`表示发送到所有连接的守护进程。

---

## 6. Configuration Merge and Output

```
+------------------------------------------------------------------+
|                Configuration Collection and Merge                 |
+------------------------------------------------------------------+

  "show running-config" Flow:
  +----------------------------------------------------------+
  |                                                           |
  |  vtysh sends "show running-config" to each daemon:        |
  |                                                           |
  |  mgmtd responds:                                          |
  |  +------------------------------------------------------+ |
  |  | hostname router1                                     | |
  |  | interface eth0                                       | |
  |  |  ip address 10.0.0.1/24                              | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  bgpd responds:                                           |
  |  +------------------------------------------------------+ |
  |  | router bgp 65000                                     | |
  |  |  neighbor 10.0.0.2 remote-as 65001                   | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  ospfd responds:                                          |
  |  +------------------------------------------------------+ |
  |  | router ospf                                          | |
  |  |  network 10.0.0.0/24 area 0                          | |
  |  +------------------------------------------------------+ |
  |                                                           |
  +----------------------------------------------------------+

  vtysh_config_parse_line() Processing:
  +----------------------------------------------------------+
  |                                                           |
  |  Input line: "router bgp 65000"                           |
  |                                                           |
  |  1. Identify node type: BGP_NODE                          |
  |  2. Get/create config block for this node                 |
  |  3. Subsequent lines with leading spaces:                 |
  |     - Add to current config block                         |
  |  4. When new top-level line appears:                      |
  |     - Close current block                                 |
  |     - Start new block or add to top-level                 |
  |                                                           |
  +----------------------------------------------------------+

  Config Block Organization:
  +----------------------------------------------------------+
  |                                                           |
  |  configvec (vector of configurations by node type):       |
  |                                                           |
  |  [INTERFACE_NODE] -> config{                              |
  |                        name: "interface eth0"             |
  |                        line: ["ip address 10.0.0.1/24"]   |
  |                      }                                    |
  |                                                           |
  |  [BGP_NODE] -> config{                                    |
  |                  name: "router bgp 65000"                 |
  |                  line: ["neighbor 10.0.0.2 remote-as..."] |
  |                }                                          |
  |                                                           |
  |  [OSPF_NODE] -> config{                                   |
  |                   name: "router ospf"                     |
  |                   line: ["network 10.0.0.0/24 area 0"]    |
  |                 }                                         |
  |                                                           |
  +----------------------------------------------------------+

  vtysh_config_dump() Output Order:
  +----------------------------------------------------------+
  |                                                           |
  |  1. Top-level configs (hostname, log, etc.)               |
  |  2. Interface configs                                     |
  |  3. VRF configs                                           |
  |  4. Router configs (bgp, ospf, etc.)                      |
  |  5. Route-map configs                                     |
  |  6. Access-list configs                                   |
  |  7. Line/VTY configs                                      |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

当执行"show running-config"时，`vtysh`向每个守护进程发送命令并收集响应。然后使用`vtysh_config_parse_line()`解析每行：识别节点类型（如BGP_NODE），创建或获取该节点的配置块，将缩进的行添加到当前块。配置按节点类型组织在`configvec`向量中。最后`vtysh_config_dump()`按预定顺序输出合并后的配置：首先是顶层配置，然后是接口、VRF、路由器、路由映射、访问列表等。

---

## Summary

| Component | Purpose | Key Files |
|-----------|---------|-----------|
| Command Macros | Define CLI commands | `lib/command.h` |
| Config Nodes | Organize command hierarchy | Per-daemon + vtysh |
| Runtime Config | Active in-memory state | Daemon data structures |
| Startup Config | Persistent file storage | frr.conf |
| mgmtd IPC | YANG-based config transport | `mgmtd/mgmt_fe_adapter.c` |
| vtysh Clients | Daemon connection management | `vtysh/vtysh.c` |

---

**Previous:** [Part 2: HOW - Management Architecture](management_plane_02_how.md)

**Next:** [Part 4: WHERE - Source Guide](management_plane_04_where.md)
