# Linux Network Interface 分类心智模型（基于 Device Model 视角）

## 一、第一层分类：是否关联到底层 device

```text
                           network interface
                                    │
                 ┌──────────────────┴──────────────────┐
                 │                                     │
                 ▼                                     ▼
         /device 不存在                       /device 存在
                 │                                     │
                 │                                     │
                 ▼                                     ▼
             软件接口                        与硬件设备关联
                                                     │
                                                     │
                                                     ▼
                                            /device/physfn ?
                                                     │
                                    ┌────────────────┴───────────────┐
                                    │                                │
                                    ▼                                ▼
                              physfn 不存在                    physfn 存在
                                    │                                │
                                    │                                │
                                    ▼                                ▼
                            PF / 普通物理NIC                     SR-IOV VF
```

---

# 二、完整分类图

```text
                           network interface
                                    │
                 ┌──────────────────┼──────────────────┐
                 │                  │                  │
                 ▼                  ▼                  ▼
             无 device           device 存在        device 存在
                                                  physfn 存在
                 │                  │                  │
                 │                  │                  │
                 ▼                  ▼                  ▼
              软件接口          PF / 普通物理NIC         VF
            lo/vlan/bond       eth0/ens1f0           ens1f0v0
            veth/bridge        eno1/p2p1             ens1f0v1

                                    │
                                    ▼
                              真正硬件所有者
```

---

# 三、各类 Interface 的典型特征

## 1. 普通物理网卡 (Physical NIC)

### 示例

```text
eth0
eno1
ens1f0
p2p1
```

---

### Device Model

```text
PCI Device
     │
     ▼
network driver
     │
     ▼
net_device
```

---

### sysfs 特征

```text
device                      √
device/physfn               ×
device/sriov_totalvfs       × (通常)
carrier                     √
speed                       √
duplex                      √
```

---

### 心智模型

```text
NIC
 │
 └────► interface
```

---

## 2. SR-IOV Physical Function (PF)

### 示例

```text
ens2f0
ens2f1
```

---

### Device Model

```text
PCI Device (PF)
      │
      ├──── VF0
      ├──── VF1
      └──── VF2
```

---

### sysfs 特征

```text
device                      √
device/physfn               ×

device/sriov_totalvfs       √
device/sriov_numvfs         √
```

---

### 心智模型

```text
             PF
             │
      ┌──────┼──────┐
      │      │      │
     VF0    VF1    VF2
```

---

## 3. SR-IOV Virtual Function (VF)

### 示例

```text
ens2f0v0
ens2f0v1
```

---

### Device Model

```text
VF Device
    │
    ▼
PF Device
```

---

### sysfs 特征

```text
device                  √
device/physfn           √

device/sriov_numvfs     ×
```

---

### 心智模型

```text
VF
 │
 ▼
PF
 │
 ▼
真实硬件
```

---

# 四、软件 Interface 分类

这些接口：

```text
没有自己的真实硬件
```

即：

```text
device     ×
```

---

## 4. Loopback

### 示例

```text
lo
```

---

### 特征

```text
device                  ×
lower_*                 ×
upper_*                 ×
```

---

### 心智模型

```text
应用
 │
 ▼
lo
 │
 ▼
应用
```

---

## 5. VLAN

### 示例

```text
eth0.100
vlan100
```

---

### Device Model

```text
eth0
 │
 ▼
VLAN Interface
```

---

### sysfs 特征

```text
device                  ×

lower_eth0              √
```

---

### 心智模型

```text
eth0
 │
 └────► vlan100
```

---

## 6. Bond

### 示例

```text
bond0
```

---

### Device Model

```text
eth0
  │
  │
bond0
  │
  │
eth1
```

---

### sysfs 特征

```text
device                  ×

bonding/                √

lower_eth0              √
lower_eth1              √
```

---

### 心智模型

```text
         bond0
        /     \
       /       \
   eth0       eth1
```

---

## 7. Bridge

### 示例

```text
br0
```

---

### Device Model

```text
eth0
  │
br0
  │
tap0
```

---

### sysfs 特征

```text
device                  ×

bridge/                 √
brif/                   √
```

---

### 心智模型

```text
             br0
           /  |  \
          /   |   \
      eth0  tap0 veth0
```

---

## 8. Veth

### 示例

```text
vethabc123
```

---

### Device Model

```text
veth pair
```

---

### sysfs 特征

```text
device                  ×

ifindex != iflink       √
```

---

### 心智模型

```text
vethA  ◄────────►  vethB
```

---

## 9. Macvlan

### 示例

```text
macvlan0
```

---

### Device Model

```text
eth0
 │
 ├── macvlan0
 ├── macvlan1
 └── macvlan2
```

---

### sysfs 特征

```text
device                  ×

lower_eth0              √
```

---

### 心智模型

```text
eth0
 │
 ├── macvlan0
 ├── macvlan1
 └── macvlan2
```

---

# 五、从 Device Model 角度的所有权关系

```text
                  谁拥有真实硬件？
                       │
                       ▼
                 device 存在 ?
                       │
           ┌───────────┴────────────┐
           │                        │
          否                        是
           │                        │
           ▼                        ▼
      软件接口                 physfn 存在 ?
                                   │
                      ┌────────────┴───────────┐
                      │                        │
                     否                        是
                      │                        │
                      ▼                        ▼
              PF / 普通物理NIC                  VF
                 (Owner)                   (Consumer)
```

---

# 六、快速识别表

|类型|device|physfn|lower_*|bonding/|bridge/|真正拥有硬件|
|---|---:|---:|---:|---:|---:|---|
|普通NIC|√|×|×|×|×|自己|
|SR-IOV PF|√|×|×|×|×|自己|
|SR-IOV VF|√|√|×|×|×|PF|
|Loopback|×|×|×|×|×|无|
|VLAN|×|×|√|×|×|底层接口|
|Bond|×|×|√|√|×|成员接口|
|Bridge|×|×|√|×|√|成员接口|
|Veth|×|×|×|×|×|另一端 veth|
|Macvlan|×|×|√|×|×|底层接口|

---

# 七、最终心智模型

```text
                       network interface
                                │
               ┌────────────────┴─────────────────┐
               │                                  │
               ▼                                  ▼
         device 不存在                       device 存在
               │                                  │
               │                                  │
               ▼                                  ▼
          软件抽象接口                        硬件相关接口
               │                                  │
     ┌─────────┼──────────┐                       │
     │         │          │                       │
     ▼         ▼          ▼                       ▼
    lo      vlan      bridge/bond           physfn 存在 ?
                 \          /                     │
                  \        /             ┌────────┴────────┐
                   \      /              │                 │
                    ▼    ▼               ▼                 ▼
                  lower device      PF / 普通NIC           VF
                                          │                │
                                          │                ▼
                                          │               PF
                                          │                │
                                          └────────────────┘
                                               真正硬件所有者
```
