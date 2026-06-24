# Linux Network Interface 的 sysfs 与 procfs 特征总结

## 一、整体心智模型

Linux 中的 network interface (`struct net_device`) 会同时出现在：

```text
                    +----------------------+
                    |   struct net_device  |
                    +----------+-----------+
                               |
               +---------------+----------------+
               |                                |
               v                                v
         /sys/class/net                 /proc/net/*
          (对象模型视角)                 (运行状态视角)
```

其中：

- `/sys/class/net`
  - 来自 sysfs
  - 基于 Linux Device Model
  - 描述："这个 interface 是什么"

- `/proc/net`
  - 来自 procfs
  - 基于内核运行时状态
  - 描述："这个 interface 正在干什么"

---

# 二、/sys/class/net 的组织方式

## 总体结构

```text
/sys/class/net/
├── lo
├── eth0
├── ens2f0
├── ens2f0v0
├── bond0
├── br0
├── vlan100
└── veth3f82a1d
```

实际上：

```text
/sys/class/net/<iface>
        ↓
    symbolic link
        ↓
/sys/devices/.../net/<iface>
```

---

# 三、/proc/net 的组织方式

```text
/proc/net/
├── dev
├── route
├── arp
├── tcp
├── udp
├── softnet_stat
├── bonding/
├── vlan/
└── ...
```

特点：

```text
procfs
    ↓
运行时状态
    ↓
所有 interface 共享这些文件
```

例如：

```bash
cat /proc/net/dev
```

输出：

```text
Inter-| Receive | Transmit
 face | bytes packets ...
    lo: ...
  eth0: ...
 bond0: ...
```

---

# 四、不同类型 Interface 的典型特征

---

# 1. Loopback

## 示例

```text
lo
```

---

## sysfs

```text
/ sys/class/net/lo
├── address
├── mtu
├── operstate
├── statistics/
└── ...

无：

device
phydev
lower_*
upper_*
```

---

## procfs

```text
出现在：

/proc/net/dev
/proc/net/route
/proc/net/if_inet6
```

---

## 特征

```text
软件接口
永远存在
没有硬件关联
```

---

# 2. 普通物理网卡

## 示例

```text
eth0
ens1f0
eno1
```

---

## sysfs

```text
eth0
├── device
├── mtu
├── address
├── carrier
├── duplex
├── speed
├── statistics/
└── queues/
```

---

## device

```text
device
├── driver
├── numa_node
├── vendor
├── device
└── ...
```

---

## 特征

```text
device 存在
physfn 不存在
```

---

## procfs

```text
/ proc/net/dev
/ proc/net/route
```

---

---

# 3. SR-IOV PF

## 示例

```text
ens2f0
```

---

## sysfs

```text
device
├── sriov_numvfs
├── sriov_totalvfs
├── sriov_drivers_autoprobe
└── ...
```

---

## 特征

```text
device 存在

device/physfn 不存在

device/sriov_totalvfs 存在
```

---

## 心智模型

```text
PF
 │
 ├─ VF0
 ├─ VF1
 └─ VF2
```

---

# 4. SR-IOV VF

## 示例

```text
ens2f0v0
```

---

## sysfs

```text
device
└── physfn
```

---

## 特征

```text
device 存在

device/physfn 存在
```

---

## 心智模型

```text
VF
 │
 └────► PF
```

---

# 5. VLAN

## 示例

```text
eth0.100
vlan100
```

---

## sysfs

```text
vlan100
├── lower_eth0
└── ...
```

---

## procfs

```text
/proc/net/vlan/
├── config
└── vlan100
```

---

## 特征

```text
device 不存在

lower_* 存在

/proc/net/vlan 存在
```

---

## 心智模型

```text
eth0
  │
  ▼
vlan100
```

---

# 6. Bond

## 示例

```text
bond0
```

---

## sysfs

```text
bond0
├── bonding/
├── lower_eth0
├── lower_eth1
└── ...
```

---

## procfs

```text
/ proc/net/bonding/bond0
```

---

## 特征

```text
device 不存在

bonding/ 存在
```

---

## 心智模型

```text
        bond0
       /     \
      /       \
   eth0      eth1
```

---

# 7. Bridge

## 示例

```text
br0
```

---

## sysfs

```text
br0
├── bridge/
├── brif/
└── ...
```

---

## 特征

```text
device 不存在

bridge/ 存在

brif/ 存在
```

---

## 心智模型

```text
            br0
          /  |  \
         /   |   \
      eth0 veth0 tap0
```

---

# 8. Veth

## 示例

```text
vethXXX
```

---

## sysfs

```text
vethxxx
└── iflink
```

---

## 特征

```text
device 不存在

iflink != ifindex
```

---

## 心智模型

```text
vethA <────────► vethB
```

---

# 9. Macvlan

## 示例

```text
macvlan0
```

---

## sysfs

```text
macvlan0
└── lower_eth0
```

---

## 特征

```text
device 不存在

lower_* 存在
```

---

## 心智模型

```text
eth0
 │
 ├── macvlan0
 ├── macvlan1
 └── macvlan2
```

---

# 五、lower_* / upper_* 的意义

---

## lower_*

表示：

```text
当前接口依赖谁
```

例如：

```text
bond0
 └── lower_eth0
```

表示：

```text
bond0
  ↓
eth0
```

---

## upper_*

表示：

```text
谁依赖当前接口
```

例如：

```text
eth0
 └── upper_bond0
```

表示：

```text
eth0
 ↑
bond0
```

---

## 整体关系

```text
           upper_*
             ↑
             │
         当前接口
             │
             ↓
           lower_*
```

---

# 六、典型识别规则

|类型|device|physfn|bonding|bridge|lower_*|proc特征|
|---|---|---|---|---|---|---|
|lo|×|×|×|×|×|dev|
|普通NIC|√|×|×|×|×|dev|
|SR-IOV PF|√|×|×|×|×|dev|
|SR-IOV VF|√|√|×|×|×|dev|
|Bond|×|×|√|×|√|bonding|
|Bridge|×|×|×|√|√|dev|
|VLAN|×|×|×|×|√|vlan|
|Macvlan|×|×|×|×|√|dev|
|Veth|×|×|×|×|×|dev|

---

# 七、快速分类决策树

```text
interface
    │
    ├─ device ?
    │      │
    │      ├─ YES
    │      │    │
    │      │    ├─ physfn ?
    │      │    │      │
    │      │    │      ├─ YES → VF
    │      │    │      │
    │      │    │      └─ NO
    │      │    │             │
    │      │    │             ├─ sriov_totalvfs ? → PF
    │      │    │             │
    │      │    │             └─ 普通NIC
    │      │
    │      └─ NO
    │
    └─ bonding ?
           │
           ├─ YES → Bond
           │
           └─ NO
                 │
                 ├─ bridge ? → Bridge
                 │
                 ├─ /proc/net/vlan ? → VLAN
                 │
                 ├─ iflink != ifindex ? → Veth
                 │
                 └─ lower_* ? → Macvlan/VLAN/Overlay
```

---

# 八、核心心智模型

```text
                 net_device
                      │
      ┌───────────────┴────────────────┐
      │                                │
      ▼                                ▼
  /sys/class/net                  /proc/net
      │                                │
      │                                │
      ▼                                ▼
"它是什么？"                   "它现在怎样？"

      │                                │
      │                                │
      ▼                                ▼
Device Model                  Runtime State

device/                       statistics
physfn                        routing
lower_*                       ARP
upper_*                       sockets
bridge/                       bonding status
bonding/                      VLAN status
```
