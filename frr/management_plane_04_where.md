# FRR Management Plane Architecture - Part 4: WHERE

## Source Guide

This document provides a guided tour of the FRR management plane source code, covering `vtysh/`, `mgmtd/`, and shared management headers.

---

## 1. Directory Structure

```
+------------------------------------------------------------------+
|                 FRR Management Plane Source Tree                  |
+------------------------------------------------------------------+

  frr/
    |
    +-- vtysh/                    (CLI frontend)
    |     |
    |     +-- vtysh.c             Main CLI implementation
    |     +-- vtysh.h             Header and client definitions
    |     +-- vtysh_main.c        Entry point and readline setup
    |     +-- vtysh_config.c      Configuration merge logic
    |     +-- vtysh_user.c        User authentication
    |     +-- daemons.pl          Perl script for daemon list
    |     +-- subdir.am           Build configuration
    |
    +-- mgmtd/                    (Management daemon)
    |     |
    |     +-- mgmt_main.c         Entry point
    |     +-- mgmt.c              Core initialization
    |     +-- mgmt.h              Header definitions
    |     +-- mgmt_fe_adapter.c   Frontend adapter (vtysh, NETCONF)
    |     +-- mgmt_be_adapter.c   Backend adapter (zebra, ripd)
    |     +-- mgmt_ds.c           Datastore management
    |     +-- mgmt_txn.c          Transaction handling
    |     +-- mgmt_history.c      Configuration history
    |     +-- mgmt_vty.c          VTY commands for mgmtd
    |
    +-- lib/                      (Shared libraries)
          |
          +-- command.c           Command parsing engine
          +-- command.h           Command macros and structures
          +-- vty.c               VTY server implementation
          +-- vty.h               VTY structures
          +-- mgmt_fe_client.c    Frontend client library
          +-- mgmt_be_client.c    Backend client library
          +-- mgmt_msg.c          Message handling
          +-- northbound.c        YANG northbound integration
          +-- yang.c              YANG utilities
```

**中文解释：**

FRR管理平面的源代码分布在三个主要目录中：`vtysh/`包含CLI前端的实现，`mgmtd/`包含管理守护进程的实现，`lib/`包含共享的库代码（如命令解析引擎、VTY服务器、管理客户端库等）。理解这些目录的组织结构是阅读源代码的第一步。

---

## 2. vtysh/ Directory Deep Dive

```
+------------------------------------------------------------------+
|                        vtysh/ Source Files                        |
+------------------------------------------------------------------+

  vtysh_main.c - Entry Point:
  +----------------------------------------------------------+
  |                                                           |
  |  Key Functions:                                           |
  |                                                           |
  |  main()                                                   |
  |    |                                                      |
  |    +-> frr_preinit()          FRR library init            |
  |    +-> vtysh_signal_init()    Signal handlers             |
  |    +-> vtysh_init_vty()       Command registration        |
  |    +-> vtysh_config_init()    Config merge init           |
  |    +-> vtysh_connect_all()    Connect to daemons          |
  |    +-> vtysh_rl_run()         Main readline loop          |
  |                                                           |
  |  vtysh_rl_run()                                           |
  |    Main event loop using readline                         |
  |    Handles user input and dispatches commands             |
  |                                                           |
  +----------------------------------------------------------+

  vtysh.c - Core Implementation:
  +----------------------------------------------------------+
  |                                                           |
  |  Client Management:                                       |
  |  - vtysh_client[] array                                   |
  |  - vtysh_connect()           Connect to one daemon        |
  |  - vtysh_connect_all()       Connect to all daemons       |
  |  - vtysh_reconnect()         Reconnect on failure         |
  |                                                           |
  |  Command Execution:                                       |
  |  - vtysh_execute()           Execute with pager           |
  |  - vtysh_execute_no_pager()  Execute without pager        |
  |  - vtysh_execute_func()      Core execution logic         |
  |  - vtysh_client_run()        Send command to daemon       |
  |  - vtysh_client_execute()    Execute on all instances     |
  |                                                           |
  |  Command Nodes:                                           |
  |  - bgp_node, ospf_node, etc. Node definitions             |
  |  - vtysh_init_vty()          Install all nodes/commands   |
  |                                                           |
  |  Tab Completion:                                          |
  |  - command_generator()       Generate completions         |
  |  - new_completion()          Readline completion hook     |
  |  - vtysh_rl_describe()       Handle '?' for help          |
  |                                                           |
  +----------------------------------------------------------+

  vtysh_config.c - Configuration Management:
  +----------------------------------------------------------+
  |                                                           |
  |  Data Structures:                                         |
  |  - struct config              Configuration block         |
  |  - configvec                  Vector of configs by node   |
  |  - config_top                 Top-level config list       |
  |                                                           |
  |  Key Functions:                                           |
  |  - vtysh_config_parse_line() Parse one config line        |
  |  - vtysh_config_dump()       Output merged config         |
  |  - vtysh_read_config()       Read from file               |
  |  - vtysh_apply_config()      Apply config to daemons      |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`vtysh_main.c`是程序入口点，负责初始化、信号处理、连接守护进程和运行主循环。`vtysh.c`是核心实现，包含客户端管理、命令执行、节点定义和Tab补全功能。`vtysh_config.c`处理配置管理，包括解析配置行、合并多个守护进程的配置、读写配置文件。

---

## 3. mgmtd/ Directory Deep Dive

```
+------------------------------------------------------------------+
|                       mgmtd/ Source Files                         |
+------------------------------------------------------------------+

  mgmt_main.c - Entry Point:
  +----------------------------------------------------------+
  |                                                           |
  |  main()                                                   |
  |    |                                                      |
  |    +-> frr_preinit()          FRR library init            |
  |    +-> mgmt_master_init()     Initialize master struct    |
  |    +-> vrf_cmd_init()         VRF command registration    |
  |    +-> if_cmd_init()          Interface cmd registration  |
  |    +-> mgmt_init()            Core mgmtd initialization   |
  |    +-> frr_config_fork()      Fork for config handling    |
  |    +-> frr_run()              Enter main event loop       |
  |                                                           |
  +----------------------------------------------------------+

  mgmt.c - Core Initialization:
  +----------------------------------------------------------+
  |                                                           |
  |  mgmt_init()                                              |
  |    |                                                      |
  |    +-> debug_install()        Install debug commands      |
  |    +-> mgmt_ds_init()         Initialize datastores       |
  |    +-> mgmt_history_init()    Initialize history          |
  |    +-> mgmt_txn_init()        Initialize transactions     |
  |    +-> mgmt_fe_adapter_init() Start frontend adapter      |
  |    +-> vty_init_mgmt_fe()     VTY frontend client         |
  |    +-> mgmt_vty_init()        mgmtd VTY commands          |
  |    +-> mgmt_be_adapter_init() Start backend adapter       |
  |                                                           |
  +----------------------------------------------------------+

  mgmt_fe_adapter.c - Frontend Adapter:
  +----------------------------------------------------------+
  |                                                           |
  |  Purpose: Handle connections from CLI clients             |
  |                                                           |
  |  Key Structures:                                          |
  |  - mgmt_fe_session_ctx       Session state                |
  |  - mgmt_fe_client_adapter    Client connection            |
  |                                                           |
  |  Key Functions:                                           |
  |  - mgmt_fe_adapter_init()    Start server                 |
  |  - mgmt_fe_create_session()  Create client session        |
  |  - mgmt_fe_session_write_lock_ds() Lock datastore         |
  |  - mgmt_fe_session_cfg_txn_cleanup() Cleanup on error     |
  |                                                           |
  +----------------------------------------------------------+

  mgmt_be_adapter.c - Backend Adapter:
  +----------------------------------------------------------+
  |                                                           |
  |  Purpose: Push config to routing daemons                  |
  |                                                           |
  |  XPATH Mapping:                                           |
  |  - zebra_config_xpaths[]     XPaths for zebra             |
  |  - ripd_config_xpaths[]      XPaths for ripd              |
  |  - staticd_config_xpaths[]   XPaths for staticd           |
  |                                                           |
  |  Key Functions:                                           |
  |  - mgmt_be_adapter_init()    Start BE server              |
  |  - mgmt_be_send_cfgdata_req() Send config to backend      |
  |  - mgmt_be_get_adapter_by_xpath() Find backend for xpath  |
  |                                                           |
  +----------------------------------------------------------+

  mgmt_txn.c - Transaction Management:
  +----------------------------------------------------------+
  |                                                           |
  |  Transaction Phases:                                      |
  |  - MGMTD_COMMIT_PHASE_PREPARE_CFG                         |
  |  - MGMTD_COMMIT_PHASE_TXN_CREATE                          |
  |  - MGMTD_COMMIT_PHASE_APPLY_CFG                           |
  |  - MGMTD_COMMIT_PHASE_TXN_DELETE                          |
  |                                                           |
  |  Key Functions:                                           |
  |  - mgmt_txn_create()         Create transaction           |
  |  - mgmt_txn_send_be_txn_create() Create BE transaction    |
  |  - mgmt_txn_cfg_prepare()    Prepare config changes       |
  |  - mgmt_txn_cfg_commit()     Commit changes               |
  |  - mgmt_destroy_txn()        Cleanup transaction          |
  |                                                           |
  +----------------------------------------------------------+

  mgmt_ds.c - Datastore Management:
  +----------------------------------------------------------+
  |                                                           |
  |  Datastores:                                              |
  |  - MGMTD_DS_CANDIDATE        Edit buffer                  |
  |  - MGMTD_DS_RUNNING          Active config                |
  |  - MGMTD_DS_OPERATIONAL      Read-only state              |
  |                                                           |
  |  Key Functions:                                           |
  |  - mgmt_ds_init()            Initialize datastores        |
  |  - mgmt_ds_lock()            Lock datastore               |
  |  - mgmt_ds_copy_dss()        Copy between datastores      |
  |  - mgmt_ds_get_ctx_by_id()   Get datastore context        |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`mgmt_main.c`是`mgmtd`的入口点，初始化各个子系统。`mgmt.c`是核心初始化逻辑，按顺序初始化数据存储、历史记录、事务、前端适配器和后端适配器。`mgmt_fe_adapter.c`处理来自`vtysh`等客户端的连接，`mgmt_be_adapter.c`将配置推送到路由守护进程。`mgmt_txn.c`管理事务的生命周期，`mgmt_ds.c`管理数据存储（候选、运行、操作）。

---

## 4. lib/ Shared Code

```
+------------------------------------------------------------------+
|                    lib/ Management-Related Files                  |
+------------------------------------------------------------------+

  command.c / command.h - Command Engine:
  +----------------------------------------------------------+
  |                                                           |
  |  Core Structures:                                         |
  |  - struct cmd_element        Command definition           |
  |  - struct cmd_node           Node in CLI hierarchy        |
  |  - struct vty                VTY session state            |
  |                                                           |
  |  Macros:                                                  |
  |  - DEFUN, DEFPY, DEFUNSH     Command definition macros    |
  |  - ALIAS                     Command alias                |
  |  - install_element()         Register command in node     |
  |  - install_node()            Register node                |
  |                                                           |
  |  Parsing:                                                 |
  |  - cmd_make_strvec()         Tokenize command line        |
  |  - cmd_execute_command()     Execute parsed command       |
  |  - cmd_describe_command()    Generate help text           |
  |  - cmd_complete_command()    Generate completions         |
  |                                                           |
  +----------------------------------------------------------+

  vty.c / vty.h - VTY Server:
  +----------------------------------------------------------+
  |                                                           |
  |  VTY Types:                                               |
  |  - VTY_TERM                  Terminal session             |
  |  - VTY_FILE                  File-based input             |
  |  - VTY_SHELL                 Shell client (vtysh)         |
  |                                                           |
  |  Key Functions:                                           |
  |  - vty_init()                Initialize VTY subsystem     |
  |  - vty_serv_un()             Create Unix socket server    |
  |  - vty_read()                Read from VTY                |
  |  - vty_out()                 Write to VTY                 |
  |  - vty_prompt()              Generate prompt              |
  |                                                           |
  +----------------------------------------------------------+

  mgmt_fe_client.c - Frontend Client Library:
  +----------------------------------------------------------+
  |                                                           |
  |  Used by: vtysh, NETCONF, gRPC frontends                  |
  |                                                           |
  |  Key Functions:                                           |
  |  - mgmt_fe_client_create()   Create FE client             |
  |  - mgmt_fe_send_setcfg_req() Send config set request      |
  |  - mgmt_fe_send_commitcfg_req() Send commit request       |
  |  - mgmt_fe_send_get_req()    Send get data request        |
  |                                                           |
  +----------------------------------------------------------+

  mgmt_be_client.c - Backend Client Library:
  +----------------------------------------------------------+
  |                                                           |
  |  Used by: zebra, ripd, staticd                            |
  |                                                           |
  |  Key Functions:                                           |
  |  - mgmt_be_client_create()   Create BE client             |
  |  - mgmt_be_client_handle_msg() Process mgmtd message      |
  |  - mgmt_be_send_cfgdata_create_reply() Reply to config    |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`lib/`目录包含管理平面使用的共享代码。`command.c`是命令解析引擎，提供命令定义宏、节点注册和命令执行功能。`vty.c`实现VTY服务器，处理终端会话、文件输入和shell客户端。`mgmt_fe_client.c`和`mgmt_be_client.c`分别提供前端和后端客户端库，供`vtysh`等前端和`zebra`等后端使用。

---

## 5. Key Data Structures

```
+------------------------------------------------------------------+
|                   Key Management Plane Structures                 |
+------------------------------------------------------------------+

  struct cmd_element (lib/command.h):
  +----------------------------------------------------------+
  |                                                           |
  |  struct cmd_element {                                     |
  |      const char *string;      /* Command string */        |
  |      const char *doc;         /* Documentation */         |
  |      int daemon;              /* Target daemon flags */   |
  |      uint8_t attr;            /* Command attributes */    |
  |      int (*func)(...);        /* Handler function */      |
  |  };                                                       |
  |                                                           |
  +----------------------------------------------------------+

  struct cmd_node (lib/command.h):
  +----------------------------------------------------------+
  |                                                           |
  |  struct cmd_node {                                        |
  |      const char *name;        /* Node name */             |
  |      enum node_type node;     /* Node type enum */        |
  |      enum node_type parent;   /* Parent node */           |
  |      const char *prompt;      /* CLI prompt format */     |
  |      int (*func)(struct vty *); /* Config write func */   |
  |  };                                                       |
  |                                                           |
  +----------------------------------------------------------+

  struct vty (lib/vty.h):
  +----------------------------------------------------------+
  |                                                           |
  |  struct vty {                                             |
  |      int fd;                  /* Socket file descriptor */|
  |      enum vty_type type;      /* VTY type */              |
  |      enum node_type node;     /* Current CLI node */      |
  |      char buf[VTY_BUFSIZ];    /* Input buffer */          |
  |      FILE *of;                /* Output file */           |
  |      int lineno;              /* Line number (file) */    |
  |      ...                                                  |
  |  };                                                       |
  |                                                           |
  +----------------------------------------------------------+

  struct vtysh_client (vtysh/vtysh.h):
  +----------------------------------------------------------+
  |                                                           |
  |  struct vtysh_client {                                    |
  |      int fd;                  /* Socket to daemon */      |
  |      const char *name;        /* Daemon name */           |
  |      int flag;                /* VTYSH_* flag */          |
  |      char path[MAXPATHLEN];   /* Socket path */           |
  |      struct vtysh_client *next; /* Instance list */       |
  |  };                                                       |
  |                                                           |
  +----------------------------------------------------------+

  struct mgmt_master (mgmtd/mgmt.h):
  +----------------------------------------------------------+
  |                                                           |
  |  struct mgmt_master {                                     |
  |      struct event_loop *master; /* Event loop */          |
  |      bool terminating;        /* Shutdown flag */         |
  |      int socket_buffer;       /* Socket buffer size */    |
  |      struct mgmt_ds_ctx *running_ds;   /* Running DS */   |
  |      struct mgmt_ds_ctx *candidate_ds; /* Candidate DS */ |
  |      struct mgmt_ds_ctx *oper_ds;      /* Oper DS */      |
  |      ...                                                  |
  |  };                                                       |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

管理平面的关键数据结构包括：`cmd_element`表示一个CLI命令，包含命令字符串、文档、目标守护进程标志和处理函数。`cmd_node`表示CLI层次结构中的节点，包含名称、类型、父节点和提示符。`vty`表示VTY会话状态，包含套接字、类型、当前节点和缓冲区。`vtysh_client`表示`vtysh`到守护进程的连接。`mgmt_master`是`mgmtd`的全局状态，包含事件循环和数据存储上下文。

---

## 6. Reading Order Recommendation

```
+------------------------------------------------------------------+
|                     Recommended Reading Order                     |
+------------------------------------------------------------------+

  Phase 1: Command Infrastructure (lib/)
  +----------------------------------------------------------+
  |                                                           |
  |  1. lib/command.h           Understand DEFUN macros       |
  |  2. lib/command.c           Command parsing engine        |
  |  3. lib/vty.h               VTY structures                |
  |  4. lib/vty.c               VTY server implementation     |
  |                                                           |
  |  Goal: Understand how commands are defined and executed   |
  |                                                           |
  +----------------------------------------------------------+

  Phase 2: CLI Frontend (vtysh/)
  +----------------------------------------------------------+
  |                                                           |
  |  1. vtysh/vtysh.h           Client structure definitions  |
  |  2. vtysh/vtysh_main.c      Entry point and main loop     |
  |  3. vtysh/vtysh.c:115-138   Client array definition       |
  |  4. vtysh/vtysh.c:4630-4707 vtysh_connect() function      |
  |  5. vtysh/vtysh.c:242-408   vtysh_client_run() function   |
  |  6. vtysh/vtysh.c:505-662   vtysh_execute_func()          |
  |  7. vtysh/vtysh_config.c    Config merge logic            |
  |                                                           |
  |  Goal: Understand CLI to daemon communication             |
  |                                                           |
  +----------------------------------------------------------+

  Phase 3: Management Daemon (mgmtd/)
  +----------------------------------------------------------+
  |                                                           |
  |  1. mgmtd/mgmt.h            Core structures               |
  |  2. mgmtd/mgmt_main.c       Entry point                   |
  |  3. mgmtd/mgmt.c            Initialization sequence       |
  |  4. mgmtd/mgmt_ds.c         Datastore management          |
  |  5. mgmtd/mgmt_fe_adapter.c Frontend handling             |
  |  6. mgmtd/mgmt_be_adapter.c Backend dispatch              |
  |  7. mgmtd/mgmt_txn.c        Transaction logic             |
  |                                                           |
  |  Goal: Understand YANG-based configuration flow           |
  |                                                           |
  +----------------------------------------------------------+

  Phase 4: Client Libraries (lib/)
  +----------------------------------------------------------+
  |                                                           |
  |  1. lib/mgmt_fe_client.c    Frontend client library       |
  |  2. lib/mgmt_be_client.c    Backend client library        |
  |  3. lib/mgmt_msg.c          Message protocol              |
  |                                                           |
  |  Goal: Understand mgmtd IPC protocols                     |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

推荐的阅读顺序分为四个阶段：首先阅读`lib/`目录中的命令基础设施代码，理解命令如何定义和执行。然后阅读`vtysh/`目录，理解CLI前端如何与守护进程通信。接着阅读`mgmtd/`目录，理解基于YANG的配置流程。最后阅读客户端库代码，理解`mgmtd`的IPC协议。

---

## Summary

| Directory | Focus | Key Files |
|-----------|-------|-----------|
| vtysh/ | CLI frontend | vtysh.c, vtysh_main.c, vtysh_config.c |
| mgmtd/ | YANG management | mgmt.c, mgmt_fe_adapter.c, mgmt_be_adapter.c |
| lib/ | Shared infrastructure | command.c, vty.c, mgmt_*_client.c |

---

**Previous:** [Part 3: WHAT - Key Components](management_plane_03_what.md)

**Next:** [Part 5: API - External Control](management_plane_05_api.md)
