# FRR Management Plane Architecture - Part 6: REUSE

## Applying to Real Projects

This document covers how to apply FRR's management plane concepts to your own systems, including building custom control CLIs, using FRR as a routing backend, and avoiding CLI-driven logic pitfalls.

---

## 1. Building Your Own Control CLI

```
+------------------------------------------------------------------+
|                Building a Custom CLI System                       |
+------------------------------------------------------------------+

  FRR CLI Architecture Patterns to Reuse:
  +----------------------------------------------------------+
  |                                                           |
  |  1. Command Definition Macros                             |
  |     - Define commands declaratively                       |
  |     - Separate command string from implementation         |
  |     - Auto-generate help text                             |
  |                                                           |
  |  2. Node-Based Hierarchy                                  |
  |     - Organize commands by context                        |
  |     - Parent-child relationships                          |
  |     - Context-sensitive command availability              |
  |                                                           |
  |  3. Graph-Based Parsing                                   |
  |     - Tokenize input                                      |
  |     - Match against command patterns                      |
  |     - Support abbreviations and alternatives              |
  |                                                           |
  |  4. Tab Completion Engine                                 |
  |     - Context-aware suggestions                           |
  |     - Dynamic completions from data                       |
  |                                                           |
  +----------------------------------------------------------+

  Minimal CLI Framework Example (C):
  +----------------------------------------------------------+
  |                                                           |
  |  // Command node definition                               |
  |  enum node_type {                                         |
  |      ROOT_NODE,                                           |
  |      CONFIG_NODE,                                         |
  |      INTERFACE_NODE,                                      |
  |  };                                                       |
  |                                                           |
  |  struct cli_node {                                        |
  |      const char *name;                                    |
  |      enum node_type type;                                 |
  |      enum node_type parent;                               |
  |      const char *prompt;                                  |
  |  };                                                       |
  |                                                           |
  |  // Command definition                                    |
  |  struct cli_command {                                     |
  |      const char *pattern;      /* e.g., "show status" */  |
  |      const char *help;                                    |
  |      enum node_type node;                                 |
  |      int (*handler)(int argc, char **argv);               |
  |  };                                                       |
  |                                                           |
  |  // Command registration                                  |
  |  void register_command(struct cli_command *cmd);          |
  |                                                           |
  |  // Example commands                                      |
  |  CLI_COMMAND(show_status,                                 |
  |      "show status",                                       |
  |      "Display system status",                             |
  |      ROOT_NODE)                                           |
  |  {                                                        |
  |      printf("System is operational\n");                   |
  |      return 0;                                            |
  |  }                                                        |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR的CLI架构有几个值得借鉴的设计模式：使用宏声明式定义命令，将命令字符串与实现分离；使用节点层次结构组织命令；使用图匹配进行命令解析；提供上下文感知的Tab补全。示例代码展示了如何构建一个最小的CLI框架：定义节点类型、命令结构，并使用宏注册命令。

---

## 2. Using FRR as a Routing Backend

```
+------------------------------------------------------------------+
|                   FRR as Routing Backend                          |
+------------------------------------------------------------------+

  Integration Architecture:
  +----------------------------------------------------------+
  |                                                           |
  |  Your Application                                         |
  |       |                                                   |
  |       |  (control)                                        |
  |       v                                                   |
  |  +-------------------+     +-------------------+          |
  |  |  Control Plane    |     |  FRR Daemons      |          |
  |  |  (your logic)     |<--->|  (routing engine) |          |
  |  +-------------------+     +-------------------+          |
  |                                   |                       |
  |                                   v                       |
  |                            +-------------+                |
  |                            |   Kernel    |                |
  |                            |   (FIB)     |                |
  |                            +-------------+                |
  |                                                           |
  +----------------------------------------------------------+

  Integration Patterns:
  +----------------------------------------------------------+
  |                                                           |
  |  Pattern 1: Configuration Injection                       |
  |  +------------------------------------------------------+ |
  |  |                                                      | |
  |  |  Your App --> vtysh -c --> FRR daemons               | |
  |  |                                                      | |
  |  |  - Generate configuration dynamically                | |
  |  |  - Push via vtysh or Unix socket                     | |
  |  |  - Let FRR handle protocol details                   | |
  |  |                                                      | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  Pattern 2: Route Redistribution                          |
  |  +------------------------------------------------------+ |
  |  |                                                      | |
  |  |  Your App --> kernel routes --> zebra --> protocols  | |
  |  |                                                      | |
  |  |  - Add routes directly to kernel                     | |
  |  |  - Configure zebra to redistribute                   | |
  |  |  - FRR advertises to network                         | |
  |  |                                                      | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  Pattern 3: Northbound API                                |
  |  +------------------------------------------------------+ |
  |  |                                                      | |
  |  |  Your App --> NETCONF/gRPC --> mgmtd --> daemons     | |
  |  |                                                      | |
  |  |  - Model-driven configuration                        | |
  |  |  - Transactional changes                             | |
  |  |  - Industry-standard interface                       | |
  |  |                                                      | |
  |  +------------------------------------------------------+ |
  |                                                           |
  +----------------------------------------------------------+

  SDN Controller Integration:
  +----------------------------------------------------------+
  |                                                           |
  |  +-------------------+                                    |
  |  |  SDN Controller   |                                    |
  |  |  (OpenDaylight,   |                                    |
  |  |   ONOS, etc.)     |                                    |
  |  +-------------------+                                    |
  |           |                                               |
  |           | NETCONF/gRPC/REST                             |
  |           v                                               |
  |  +-------------------+                                    |
  |  |  FRR (mgmtd)      |                                    |
  |  +-------------------+                                    |
  |           |                                               |
  |           | Routing protocols                             |
  |           v                                               |
  |  +-------------------+                                    |
  |  |  Network Fabric   |                                    |
  |  +-------------------+                                    |
  |                                                           |
  |  Benefits:                                                |
  |  - Leverage existing SDN infrastructure                   |
  |  - Standard interfaces (YANG models)                      |
  |  - Hybrid SDN/traditional networking                      |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR可以作为应用程序的路由后端使用。三种常见的集成模式：配置注入（通过`vtysh`或Unix套接字推送配置）、路由重分发（在内核中添加路由，让FRR重分发到协议）、北向API（使用NETCONF/gRPC进行模型驱动配置）。对于SDN控制器集成，FRR的`mgmtd`提供标准接口，支持混合SDN/传统网络架构。

---

## 3. Avoiding CLI-Driven Logic Pitfalls

```
+------------------------------------------------------------------+
|                 Anti-Patterns to Avoid                            |
+------------------------------------------------------------------+

  Anti-Pattern 1: Business Logic in CLI Handlers
  +----------------------------------------------------------+
  |                                                           |
  |  WRONG:                                                   |
  |  +------------------------------------------------------+ |
  |  |  DEFUN(configure_service, ...)                       | |
  |  |  {                                                   | |
  |  |      // Complex business logic here                  | |
  |  |      validate_license();                             | |
  |  |      check_resource_availability();                  | |
  |  |      provision_network_resources();                  | |
  |  |      notify_billing_system();                        | |
  |  |      update_database();                              | |
  |  |      return CMD_SUCCESS;                             | |
  |  |  }                                                   | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  CORRECT:                                                 |
  |  +------------------------------------------------------+ |
  |  |  DEFUN(configure_service, ...)                       | |
  |  |  {                                                   | |
  |  |      // CLI layer: parse and delegate                | |
  |  |      struct service_config cfg;                      | |
  |  |      parse_service_args(&cfg, argc, argv);           | |
  |  |      return service_configure(&cfg);  // API call    | |
  |  |  }                                                   | |
  |  |                                                      | |
  |  |  // Business logic in separate module                | |
  |  |  int service_configure(struct service_config *cfg)   | |
  |  |  {                                                   | |
  |  |      // All business logic here                      | |
  |  |      // Can be called from CLI, API, or other        | |
  |  |  }                                                   | |
  |  +------------------------------------------------------+ |
  |                                                           |
  +----------------------------------------------------------+

  Anti-Pattern 2: Parsing CLI Output for State
  +----------------------------------------------------------+
  |                                                           |
  |  WRONG:                                                   |
  |  +------------------------------------------------------+ |
  |  |  # Don't do this!                                    | |
  |  |  output = vtysh -c "show ip route"                   | |
  |  |  routes = parse_text_output(output)  # fragile!      | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  CORRECT:                                                 |
  |  +------------------------------------------------------+ |
  |  |  # Use structured output                             | |
  |  |  output = vtysh -c "show ip route json"              | |
  |  |  routes = json.loads(output)  # robust               | |
  |  |                                                      | |
  |  |  # Or use proper API                                 | |
  |  |  routes = netconf_get("/routing/routes")             | |
  |  +------------------------------------------------------+ |
  |                                                           |
  +----------------------------------------------------------+

  Anti-Pattern 3: CLI as Only Interface
  +----------------------------------------------------------+
  |                                                           |
  |  WRONG:                                                   |
  |  +------------------------------------------------------+ |
  |  |  Only way to configure system is through CLI         | |
  |  |  No API, no programmatic access                      | |
  |  |  Automation requires screen-scraping                 | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  CORRECT (FRR approach):                                  |
  |  +------------------------------------------------------+ |
  |  |  Multiple interfaces to same functionality:          | |
  |  |  - CLI (vtysh) for operators                         | |
  |  |  - Unix socket for local automation                  | |
  |  |  - NETCONF for network management                    | |
  |  |  - gRPC for modern applications                      | |
  |  |  All share same backend configuration logic          | |
  |  +------------------------------------------------------+ |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

需要避免的反模式：第一，不要在CLI处理函数中放置复杂的业务逻辑，CLI层应该只负责解析参数并委托给业务逻辑模块。第二，不要解析CLI文本输出来获取状态，应该使用JSON输出或proper API。第三，不要只提供CLI接口，应该像FRR一样提供多种接口（CLI、Unix套接字、NETCONF、gRPC），它们共享同一个后端逻辑。

---

## 4. Reusable Design Principles

```
+------------------------------------------------------------------+
|              Design Principles from FRR Management                |
+------------------------------------------------------------------+

  Principle 1: Separation of Concerns
  +----------------------------------------------------------+
  |                                                           |
  |  Layer                 Responsibility                     |
  |  -------------------------------------------------------- |
  |  User Interface        Parsing, formatting, help          |
  |  Configuration Mgmt    Validation, persistence, txns      |
  |  Business Logic        Core functionality                 |
  |  Protocol Engine       Actual work                        |
  |                                                           |
  |  Each layer has clear boundaries                          |
  |  Interfaces between layers are stable                     |
  |                                                           |
  +----------------------------------------------------------+

  Principle 2: Configuration as Data
  +----------------------------------------------------------+
  |                                                           |
  |  Configuration is:                                        |
  |  - Declarative, not imperative                            |
  |  - Serializable (can be saved/loaded)                     |
  |  - Comparable (diff between configs)                      |
  |  - Versionable (history tracking)                         |
  |                                                           |
  |  Configuration is NOT:                                    |
  |  - Procedure execution                                    |
  |  - One-time actions                                       |
  |  - Transient state                                        |
  |                                                           |
  +----------------------------------------------------------+

  Principle 3: Multiple Access Methods
  +----------------------------------------------------------+
  |                                                           |
  |  Same configuration achievable via:                       |
  |  - Interactive CLI                                        |
  |  - Configuration file                                     |
  |  - Programmatic API                                       |
  |                                                           |
  |  All methods go through same validation                   |
  |  All methods produce same end state                       |
  |                                                           |
  +----------------------------------------------------------+

  Principle 4: Graceful Degradation
  +----------------------------------------------------------+
  |                                                           |
  |  Management plane failures should NOT:                    |
  |  - Stop the core functionality                            |
  |  - Corrupt the configuration                              |
  |  - Require full restart                                   |
  |                                                           |
  |  Example: vtysh crashes                                   |
  |  - FRR daemons continue running                           |
  |  - Routing continues unaffected                           |
  |  - vtysh can reconnect when restarted                     |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

从FRR管理平面学到的设计原则：第一，关注点分离，每一层有明确的职责边界。第二，配置作为数据，配置应该是声明式的、可序列化的、可比较的、可版本化的。第三，多种访问方法，相同的配置应该可以通过CLI、配置文件和API实现。第四，优雅降级，管理平面故障不应该影响核心功能。

---

## 5. Implementation Checklist

```
+------------------------------------------------------------------+
|             Management Plane Implementation Checklist             |
+------------------------------------------------------------------+

  CLI Implementation:
  +----------------------------------------------------------+
  |                                                           |
  |  [ ] Command definition framework                         |
  |  [ ] Node hierarchy for contexts                          |
  |  [ ] Tab completion support                               |
  |  [ ] Help text for all commands                           |
  |  [ ] Command history                                      |
  |  [ ] Pager integration for long output                    |
  |  [ ] Consistent command naming conventions                |
  |                                                           |
  +----------------------------------------------------------+

  Configuration Management:
  +----------------------------------------------------------+
  |                                                           |
  |  [ ] Configuration file format defined                    |
  |  [ ] Load configuration at startup                        |
  |  [ ] Save configuration on demand                         |
  |  [ ] Configuration validation before apply                |
  |  [ ] Rollback on failure                                  |
  |  [ ] Configuration diff capability                        |
  |  [ ] Configuration history/versioning                     |
  |                                                           |
  +----------------------------------------------------------+

  API Implementation:
  +----------------------------------------------------------+
  |                                                           |
  |  [ ] Programmatic access method defined                   |
  |  [ ] Structured output format (JSON/protobuf)             |
  |  [ ] Error codes and messages                             |
  |  [ ] Authentication/authorization                         |
  |  [ ] Rate limiting if needed                              |
  |  [ ] Documentation (OpenAPI/protobuf)                     |
  |                                                           |
  +----------------------------------------------------------+

  Operational Concerns:
  +----------------------------------------------------------+
  |                                                           |
  |  [ ] Logging of configuration changes                     |
  |  [ ] Audit trail                                          |
  |  [ ] Backup/restore procedures                            |
  |  [ ] Upgrade path for configuration                       |
  |  [ ] Monitoring hooks                                     |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

实现管理平面时的检查清单：CLI实现需要命令定义框架、节点层次、Tab补全、帮助文本、命令历史和分页器。配置管理需要定义文件格式、启动加载、按需保存、验证、回滚、差异比较和版本历史。API实现需要定义访问方法、结构化输出、错误码、认证授权、速率限制和文档。运维方面需要日志记录、审计追踪、备份恢复、升级路径和监控钩子。

---

## 6. Automation Best Practices

```
+------------------------------------------------------------------+
|                  Automation Best Practices                        |
+------------------------------------------------------------------+

  Configuration as Code:
  +----------------------------------------------------------+
  |                                                           |
  |  # frr_config.yaml (source of truth)                      |
  |  interfaces:                                              |
  |    - name: eth0                                           |
  |      ip: 10.0.0.1/24                                      |
  |      description: WAN                                     |
  |                                                           |
  |  bgp:                                                     |
  |    asn: 65000                                             |
  |    neighbors:                                             |
  |      - ip: 10.0.0.2                                       |
  |        remote_as: 65001                                   |
  |                                                           |
  |  # Generated frr.conf from YAML                           |
  |  # Version controlled                                     |
  |  # Reviewed via pull request                              |
  |  # Applied via CI/CD pipeline                             |
  |                                                           |
  +----------------------------------------------------------+

  Idempotent Operations:
  +----------------------------------------------------------+
  |                                                           |
  |  WRONG (not idempotent):                                  |
  |  +------------------------------------------------------+ |
  |  |  vtysh -c "router bgp 65000"                         | |
  |  |  vtysh -c "neighbor 10.0.0.2 remote-as 65001"        | |
  |  |  # Second run fails if already configured            | |
  |  +------------------------------------------------------+ |
  |                                                           |
  |  CORRECT (idempotent):                                    |
  |  +------------------------------------------------------+ |
  |  |  def ensure_bgp_neighbor(ip, asn):                   | |
  |  |      current = get_bgp_config()                      | |
  |  |      if neighbor_exists(current, ip, asn):           | |
  |  |          return  # Already configured                | |
  |  |      add_neighbor(ip, asn)                           | |
  |  |  # Safe to run multiple times                        | |
  |  +------------------------------------------------------+ |
  |                                                           |
  +----------------------------------------------------------+

  Validation Before Apply:
  +----------------------------------------------------------+
  |                                                           |
  |  Pipeline:                                                |
  |                                                           |
  |  1. Generate config                                       |
  |     |                                                     |
  |  2. Syntax check (vtysh -C -f config)                     |
  |     |                                                     |
  |  3. Diff with running config                              |
  |     |                                                     |
  |  4. Human review (for significant changes)                |
  |     |                                                     |
  |  5. Apply to staging environment                          |
  |     |                                                     |
  |  6. Run integration tests                                 |
  |     |                                                     |
  |  7. Apply to production                                   |
  |     |                                                     |
  |  8. Verify applied state matches expected                 |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

自动化最佳实践：配置即代码，将配置存储在版本控制的YAML或其他格式中，通过模板生成实际配置，经过代码审查后通过CI/CD流水线应用。操作必须幂等，可以安全地多次运行。应用前验证，包括语法检查、差异比较、人工审查（对于重大变更）、在预发布环境测试，最后才应用到生产环境并验证状态。

---

## Summary

| Topic | Key Lesson |
|-------|------------|
| Custom CLI | Use FRR's node/command pattern |
| FRR Backend | Choose appropriate integration pattern |
| Anti-patterns | Separate CLI from business logic |
| Design | Multiple access methods, configuration as data |
| Automation | Idempotent, validated, version-controlled |

The FRR management plane provides a mature, battle-tested architecture that can be adapted for many network management scenarios. By understanding its design principles, you can build robust, maintainable management systems for your own networking projects.

---

**Previous:** [Part 5: API - External Control](management_plane_05_api.md)

---

## Document Index

1. [Part 1: WHY - Why Management Is Separate](management_plane_01_why.md)
2. [Part 2: HOW - Management Architecture](management_plane_02_how.md)
3. [Part 3: WHAT - Key Components](management_plane_03_what.md)
4. [Part 4: WHERE - Source Guide](management_plane_04_where.md)
5. [Part 5: API - External Control](management_plane_05_api.md)
6. **Part 6: REUSE - Applying to Real Projects** (this document)
