# FRR Management Plane Architecture - Part 5: API

## External Control

This document covers the APIs available for external programmatic control of FRR, including command-line execution, northbound interfaces, and automation hooks.

---

## 1. Programmatic Configuration Methods

```
+------------------------------------------------------------------+
|              FRR External Control Interfaces                      |
+------------------------------------------------------------------+

  Available Interfaces:
  +----------------------------------------------------------+
  |                                                           |
  |  +-------------------+                                    |
  |  |   vtysh -c        |  Command-line execution            |
  |  |   (shell script)  |  Simple, widely compatible         |
  |  +-------------------+                                    |
  |           |                                               |
  |           v                                               |
  |  +-------------------+                                    |
  |  |   Unix Socket     |  Direct socket communication       |
  |  |   (programmatic)  |  Language-agnostic                 |
  |  +-------------------+                                    |
  |           |                                               |
  |           v                                               |
  |  +-------------------+                                    |
  |  |   NETCONF/YANG    |  Model-driven configuration        |
  |  |   (mgmtd)         |  Industry standard                 |
  |  +-------------------+                                    |
  |           |                                               |
  |           v                                               |
  |  +-------------------+                                    |
  |  |   gRPC            |  High-performance RPC              |
  |  |   (mgmtd)         |  Protobuf messages                 |
  |  +-------------------+                                    |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR提供多种外部控制接口。最简单的是通过`vtysh -c`执行命令，适合shell脚本使用。更高级的方法包括直接通过Unix套接字通信、使用NETCONF/YANG进行模型驱动配置、以及使用gRPC进行高性能RPC调用。选择哪种接口取决于具体需求：简单脚本使用`vtysh -c`，复杂自动化使用NETCONF或gRPC。

---

## 2. vtysh Command-Line Execution

```
+------------------------------------------------------------------+
|                  vtysh -c Command Interface                       |
+------------------------------------------------------------------+

  Basic Usage:
  +----------------------------------------------------------+
  |                                                           |
  |  # Single command                                         |
  |  vtysh -c "show ip route"                                 |
  |                                                           |
  |  # Multiple commands                                      |
  |  vtysh -c "conf t" -c "router bgp 65000" -c "end"         |
  |                                                           |
  |  # With daemon selection                                  |
  |  vtysh -d bgpd -c "show ip bgp summary"                   |
  |                                                           |
  |  # JSON output (where supported)                          |
  |  vtysh -c "show ip route json"                            |
  |                                                           |
  +----------------------------------------------------------+

  Script Integration:
  +----------------------------------------------------------+
  |                                                           |
  |  #!/bin/bash                                              |
  |                                                           |
  |  # Get BGP neighbor count                                 |
  |  count=$(vtysh -c "show ip bgp summary json" | \          |
  |          jq '.ipv4Unicast.totalPeers')                    |
  |                                                           |
  |  # Configure a static route                               |
  |  vtysh << EOF                                             |
  |  configure terminal                                       |
  |  ip route 10.0.0.0/8 192.168.1.1                          |
  |  end                                                      |
  |  write memory                                             |
  |  EOF                                                      |
  |                                                           |
  |  # Check command success                                  |
  |  if vtysh -c "show ip route 10.0.0.0/8" | grep -q "via";  |
  |  then                                                     |
  |      echo "Route installed successfully"                  |
  |  fi                                                       |
  |                                                           |
  +----------------------------------------------------------+

  Exit Codes:
  +----------------------------------------------------------+
  |                                                           |
  |  0   CMD_SUCCESS           Command executed successfully  |
  |  1   CMD_WARNING           Warning during execution       |
  |  2   CMD_ERR_NO_MATCH      Unknown command                |
  |  3   CMD_ERR_AMBIGUOUS     Ambiguous command              |
  |  4   CMD_ERR_INCOMPLETE    Incomplete command             |
  |  5   CMD_ERR_NO_DAEMON     Target daemon not running      |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

`vtysh -c`是最简单的程序化配置方法。可以执行单个命令、多个命令序列，或选择特定守护进程。支持JSON输出的命令可以方便地与`jq`等工具集成。脚本可以使用heredoc语法输入多行配置。退出码用于判断命令是否成功执行。

---

## 3. Direct Unix Socket Communication

```
+------------------------------------------------------------------+
|               Unix Socket Direct Communication                    |
+------------------------------------------------------------------+

  Protocol Overview:
  +----------------------------------------------------------+
  |                                                           |
  |  Client                              Daemon               |
  |    |                                   |                  |
  |    | socket(AF_UNIX, SOCK_STREAM)      |                  |
  |    |---------------------------------->|                  |
  |    |                                   |                  |
  |    | connect("/var/run/frr/bgpd.vty")  |                  |
  |    |---------------------------------->|                  |
  |    |                                   |                  |
  |    | write("show ip bgp\0")            |                  |
  |    |---------------------------------->|                  |
  |    |                                   |                  |
  |    |    read(output + "\0\0\0" + code) |                  |
  |    |<----------------------------------|                  |
  |    |                                   |                  |
  |    | close()                           |                  |
  |    |---------------------------------->|                  |
  |                                                           |
  +----------------------------------------------------------+

  Python Example:
  +----------------------------------------------------------+
  |                                                           |
  |  import socket                                            |
  |  import json                                              |
  |                                                           |
  |  def frr_command(daemon, command):                        |
  |      sock_path = f"/var/run/frr/{daemon}.vty"             |
  |      sock = socket.socket(socket.AF_UNIX,                 |
  |                           socket.SOCK_STREAM)             |
  |      sock.connect(sock_path)                              |
  |                                                           |
  |      # Send command with null terminator                  |
  |      sock.send((command + '\0').encode())                 |
  |                                                           |
  |      # Read response                                      |
  |      response = b''                                       |
  |      while True:                                          |
  |          data = sock.recv(4096)                           |
  |          if not data:                                     |
  |              break                                        |
  |          response += data                                 |
  |          # Check for terminator (3 nulls + return code)   |
  |          if b'\x00\x00\x00' in response:                  |
  |              break                                        |
  |                                                           |
  |      sock.close()                                         |
  |                                                           |
  |      # Parse response (strip terminator)                  |
  |      output = response.split(b'\x00\x00\x00')[0]          |
  |      return_code = response[-1] if response else 0        |
  |      return output.decode(), return_code                  |
  |                                                           |
  |  # Usage                                                  |
  |  output, rc = frr_command("bgpd",                         |
  |                           "show ip bgp summary json")     |
  |  if rc == 0:                                              |
  |      data = json.loads(output)                            |
  |      print(f"Peers: {data['ipv4Unicast']['totalPeers']}") |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

直接使用Unix套接字通信可以绑过`vtysh`，实现更紧密的集成。协议很简单：发送以NUL字符结尾的命令字符串，读取响应直到遇到三个NUL字节加返回码的终止符。Python示例展示了如何创建套接字、发送命令、读取响应并解析结果。这种方法适合需要高效、低开销通信的应用。

---

## 4. NETCONF/YANG Interface

```
+------------------------------------------------------------------+
|                    NETCONF/YANG Configuration                     |
+------------------------------------------------------------------+

  Architecture:
  +----------------------------------------------------------+
  |                                                           |
  |  NETCONF Client                        FRR                |
  |  (e.g., ncclient)                                         |
  |       |                                                   |
  |       |  TCP/SSH                                          |
  |       v                                                   |
  |  +-------------------+                                    |
  |  |  NETCONF Server   |  (external, e.g., sysrepo)         |
  |  +-------------------+                                    |
  |       |                                                   |
  |       |  mgmtd protocol                                   |
  |       v                                                   |
  |  +-------------------+                                    |
  |  |      mgmtd        |                                    |
  |  +-------------------+                                    |
  |       |                                                   |
  |       |  Backend adapters                                 |
  |       v                                                   |
  |  +--------+--------+--------+                             |
  |  | zebra  |  ripd  |staticd |                             |
  |  +--------+--------+--------+                             |
  |                                                           |
  +----------------------------------------------------------+

  YANG Models:
  +----------------------------------------------------------+
  |                                                           |
  |  FRR YANG Modules:                                        |
  |                                                           |
  |  frr-zebra.yang              Zebra configuration          |
  |  frr-interface.yang          Interface configuration      |
  |  frr-vrf.yang                VRF configuration            |
  |  frr-ripd.yang               RIP configuration            |
  |  frr-staticd.yang            Static routes                |
  |  frr-filter.yang             Access lists, prefix lists   |
  |  frr-route-map.yang          Route map configuration      |
  |                                                           |
  |  Location: yang/*.yang                                    |
  |                                                           |
  +----------------------------------------------------------+

  Example NETCONF Operation:
  +----------------------------------------------------------+
  |                                                           |
  |  <!-- Get running config -->                              |
  |  <rpc>                                                    |
  |    <get-config>                                           |
  |      <source><running/></source>                          |
  |      <filter type="subtree">                              |
  |        <lib xmlns="http://frrouting.org/yang/interface">  |
  |          <interface>                                      |
  |            <name>eth0</name>                              |
  |          </interface>                                     |
  |        </lib>                                             |
  |      </filter>                                            |
  |    </get-config>                                          |
  |  </rpc>                                                   |
  |                                                           |
  |  <!-- Edit configuration -->                              |
  |  <rpc>                                                    |
  |    <edit-config>                                          |
  |      <target><candidate/></target>                        |
  |      <config>                                             |
  |        <lib xmlns="http://frrouting.org/yang/interface">  |
  |          <interface>                                      |
  |            <name>eth0</name>                              |
  |            <description>WAN interface</description>       |
  |          </interface>                                     |
  |        </lib>                                             |
  |      </config>                                            |
  |    </edit-config>                                         |
  |  </rpc>                                                   |
  |                                                           |
  |  <!-- Commit changes -->                                  |
  |  <rpc><commit/></rpc>                                     |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

NETCONF/YANG是网络设备配置的行业标准。FRR通过`mgmtd`提供YANG模型支持。NETCONF客户端（如`ncclient`）通过NETCONF服务器（如`sysrepo`）与`mgmtd`通信，`mgmtd`再将配置分发到各个守护进程。FRR提供多个YANG模块，涵盖接口、VRF、RIP、静态路由、访问控制列表和路由映射等配置。

---

## 5. gRPC Interface

```
+------------------------------------------------------------------+
|                      gRPC Control Interface                       |
+------------------------------------------------------------------+

  Architecture:
  +----------------------------------------------------------+
  |                                                           |
  |  gRPC Client                           FRR                |
  |  (Python/Go/etc.)                                         |
  |       |                                                   |
  |       |  gRPC/HTTP2                                       |
  |       v                                                   |
  |  +-------------------+                                    |
  |  |   mgmtd gRPC      |                                    |
  |  |   Frontend        |                                    |
  |  +-------------------+                                    |
  |       |                                                   |
  |       |  Frontend adapter                                 |
  |       v                                                   |
  |  +-------------------+                                    |
  |  |      mgmtd        |                                    |
  |  +-------------------+                                    |
  |                                                           |
  +----------------------------------------------------------+

  Protobuf Definitions:
  +----------------------------------------------------------+
  |                                                           |
  |  Location: lib/mgmt.proto (if enabled)                    |
  |                                                           |
  |  service MgmtFrontend {                                   |
  |      rpc CreateSession(CreateSessionReq)                  |
  |          returns (CreateSessionReply);                    |
  |      rpc DestroySession(DestroySessionReq)                |
  |          returns (DestroySessionReply);                   |
  |      rpc LockDs(LockDsReq)                                |
  |          returns (LockDsReply);                           |
  |      rpc SetConfig(SetConfigReq)                          |
  |          returns (SetConfigReply);                        |
  |      rpc CommitConfig(CommitConfigReq)                    |
  |          returns (CommitConfigReply);                     |
  |      rpc GetData(GetDataReq)                              |
  |          returns (GetDataReply);                          |
  |  }                                                        |
  |                                                           |
  +----------------------------------------------------------+

  Python gRPC Example:
  +----------------------------------------------------------+
  |                                                           |
  |  import grpc                                              |
  |  from mgmt_pb2 import *                                   |
  |  from mgmt_pb2_grpc import MgmtFrontendStub               |
  |                                                           |
  |  # Connect to mgmtd                                       |
  |  channel = grpc.insecure_channel('localhost:50051')       |
  |  stub = MgmtFrontendStub(channel)                         |
  |                                                           |
  |  # Create session                                         |
  |  session = stub.CreateSession(                            |
  |      CreateSessionReq(client_name="my_app"))              |
  |                                                           |
  |  # Set configuration                                      |
  |  stub.SetConfig(SetConfigReq(                             |
  |      session_id=session.session_id,                       |
  |      ds_id=CANDIDATE,                                     |
  |      config_data=[                                        |
  |          ConfigData(                                      |
  |              xpath="/frr-interface:lib/interface[name='eth0']/description",
  |              value="WAN interface"                        |
  |          )                                                |
  |      ]                                                    |
  |  ))                                                       |
  |                                                           |
  |  # Commit                                                 |
  |  stub.CommitConfig(CommitConfigReq(                       |
  |      session_id=session.session_id,                       |
  |      src_ds_id=CANDIDATE,                                 |
  |      dst_ds_id=RUNNING                                    |
  |  ))                                                       |
  |                                                           |
  |  # Cleanup                                                |
  |  stub.DestroySession(DestroySessionReq(                   |
  |      session_id=session.session_id))                      |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

gRPC提供高性能的RPC接口，使用Protobuf进行消息序列化。`mgmtd`可以暴露gRPC前端，允许客户端创建会话、锁定数据存储、设置配置和提交更改。Python示例展示了典型的工作流程：连接服务器、创建会话、设置配置、提交更改和销毁会话。gRPC适合需要低延迟、类型安全通信的应用。

---

## 6. Automation Hooks

```
+------------------------------------------------------------------+
|                     Automation Integration                        |
+------------------------------------------------------------------+

  Event Notification:
  +----------------------------------------------------------+
  |                                                           |
  |  FRR provides hooks for external monitoring:              |
  |                                                           |
  |  1. BFD (Bidirectional Forwarding Detection)              |
  |     - Session state changes                               |
  |     - Integrated with routing protocols                   |
  |                                                           |
  |  2. SNMP Traps                                            |
  |     - BGP peer state changes                              |
  |     - OSPF neighbor changes                               |
  |     - Interface up/down events                            |
  |                                                           |
  |  3. Syslog                                                |
  |     - All daemon log messages                             |
  |     - Configurable log levels                             |
  |                                                           |
  |  4. JSON Output                                           |
  |     - Many "show" commands support JSON                   |
  |     - Easier parsing for automation                       |
  |                                                           |
  +----------------------------------------------------------+

  Ansible Integration:
  +----------------------------------------------------------+
  |                                                           |
  |  # Using command module                                   |
  |  - name: Configure BGP                                    |
  |    command: >                                             |
  |      vtysh -c "conf t"                                    |
  |      -c "router bgp {{ bgp_asn }}"                        |
  |      -c "neighbor {{ peer_ip }} remote-as {{ peer_asn }}" |
  |      -c "end"                                             |
  |                                                           |
  |  # Using template for full config                         |
  |  - name: Deploy FRR config                                |
  |    template:                                              |
  |      src: frr.conf.j2                                     |
  |      dest: /etc/frr/frr.conf                              |
  |    notify: reload frr                                     |
  |                                                           |
  +----------------------------------------------------------+

  Terraform/Pulumi Integration:
  +----------------------------------------------------------+
  |                                                           |
  |  # Using exec provisioner                                 |
  |  resource "null_resource" "frr_config" {                  |
  |    provisioner "remote-exec" {                            |
  |      inline = [                                           |
  |        "vtysh -c 'conf t' -c 'ip route ${cidr} ${gw}'"    |
  |      ]                                                    |
  |    }                                                      |
  |  }                                                        |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

FRR提供多种自动化集成方式。事件通知包括BFD会话状态变化、SNMP陷阱和Syslog消息。许多"show"命令支持JSON输出，便于自动化解析。Ansible可以通过`command`模块执行`vtysh`命令，或使用模板部署完整配置。Terraform/Pulumi可以使用远程执行provisioner配置FRR。这些集成方式覆盖了从简单脚本到企业级基础设施即代码的各种需求。

---

## 7. API Selection Guide

```
+------------------------------------------------------------------+
|                     API Selection Decision Tree                   |
+------------------------------------------------------------------+

  Choose Your Interface:
  +----------------------------------------------------------+
  |                                                           |
  |  Need simple scripting?                                   |
  |    YES --> vtysh -c                                       |
  |                                                           |
  |  Need programmatic control from application?              |
  |    |                                                      |
  |    +-- Low overhead, custom protocol?                     |
  |    |     YES --> Unix socket direct                       |
  |    |                                                      |
  |    +-- Industry standard, model-driven?                   |
  |    |     YES --> NETCONF/YANG                             |
  |    |                                                      |
  |    +-- High performance, type-safe?                       |
  |          YES --> gRPC                                     |
  |                                                           |
  |  Need event-driven integration?                           |
  |    --> SNMP traps, BFD, Syslog                            |
  |                                                           |
  |  Need infrastructure-as-code?                             |
  |    --> Ansible, Terraform with vtysh                      |
  |                                                           |
  +----------------------------------------------------------+

  Interface Comparison:
  +----------------------------------------------------------+
  |                                                           |
  |  Interface    | Complexity | Performance | Standards     |
  |  -------------|------------|-------------|---------------|
  |  vtysh -c     | Low        | Low         | None          |
  |  Unix socket  | Medium     | High        | None          |
  |  NETCONF      | High       | Medium      | RFC 6241      |
  |  gRPC         | Medium     | High        | gRPC          |
  |                                                           |
  +----------------------------------------------------------+
```

**中文解释：**

选择API时需要考虑多个因素。对于简单脚本，`vtysh -c`是最佳选择。对于需要低开销的自定义协议应用，直接使用Unix套接字。对于需要行业标准和模型驱动配置的场景，选择NETCONF/YANG。对于需要高性能和类型安全的应用，选择gRPC。事件驱动集成使用SNMP陷阱、BFD或Syslog。基础设施即代码场景使用Ansible或Terraform配合`vtysh`。

---

## Summary

| Interface | Best For | Complexity | Standard |
|-----------|----------|------------|----------|
| vtysh -c | Shell scripts | Low | N/A |
| Unix Socket | Custom apps | Medium | N/A |
| NETCONF | Enterprise automation | High | RFC 6241 |
| gRPC | Modern microservices | Medium | gRPC |

---

**Previous:** [Part 4: WHERE - Source Guide](management_plane_04_where.md)

**Next:** [Part 6: REUSE - Applying to Real Projects](management_plane_06_reuse.md)
