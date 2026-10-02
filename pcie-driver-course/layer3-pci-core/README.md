# Layer 3 — Linux PCI Core 与 Driver Matching

## 1. Mental Model

```
                  PCI Core (pci_bus_type)

           +-----------------+-----------------+
           |                                   |
     struct pci_dev                     struct pci_driver
     (内核枚举创建的硬件对象)           (开发者编写的软件驱动)
     - vendor: 0x1234                   - name: "my_pci_driver"
     - device: 0x11e8                   - id_table: [1234:11e8, ...]
           |                                   |
           |                                   |
           +------------ match ----------------+
                               |
                               | (pci_bus_match 检查 id_table)
                               v
                       my_probe(pci_dev, id)
```

## 2. Why

硬件已经枚举好了（`struct pci_dev` 已经存在），现在我们需要写一个驱动把它“接管”过来。
Linux 绝不允许驱动直接去盲扫硬件。驱动必须向 PCI Core 注册一个 `struct pci_driver`，并声明一个 **`id_table`**。
PCI Core 会拿这个表和系统里所有的 `struct pci_dev` 逐一比对，只要比对成功，就会把对应的 `pci_dev` 传入驱动的 `probe()` 函数。

## 3. 核心对象与生命周期

### 3.1 `struct pci_driver`
```c
struct pci_driver {
    const char *name;                      /* 驱动在 sysfs 中的名称 */
    const struct pci_device_id *id_table;  /* 驱动所支持的硬件 ID 数组，以全 0 结尾 */
    int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
    void (*remove)(struct pci_dev *dev);
    /* 电源管理、错误恢复等回调 */
};
```

### 3.2 模块与驱动的生命周期串联
```
module_init()
      |
      v
pci_register_driver(&my_driver)
      |
      v
PCI Core 遍历 bus 上已有设备，执行 matching
      |
      +---> 匹配成功 -> 触发 probe() -> probe 成功，绑定完成
      |
      +---> 没匹配上 -> 什么都不做，静静等待有新设备热插拔
      |
(系统运行中...)
      |
rmmod
      |
      v
pci_unregister_driver(&my_driver)
      |
      v
PCI Core 对每一个已绑定的 pci_dev 调用 remove()
      |
      v
module_exit() 完成
```

## Layer 3 实验列表

| Experiment | 目标 | 目录 |
|---|---|---|
| 3.1 | 编写针对 QEMU edu (1234:11e8) 的最小 PCI 驱动骨架，验证 probe() | [exp3.1-minimal-pci-driver/](exp3.1-minimal-pci-driver/) |
| 3.2 | 注入 ID 不匹配的故障，彻底验证 "insmod 成功不等于 probe 发生" | [exp3.2-matching-failure/](exp3.2-matching-failure/) |

## Checkpoint

- `struct pci_device_id` 数组为什么必须以全 0 结构体作为末尾？
- 当执行 `insmod my_pci.ko` 时，是谁在什么时候调用了 `my_probe()`？
- 如果系统中插入了两个相同型号的 PCI 网卡，`my_probe()` 会被调用几次？传入的参数有何不同？
- 为什么在驱动卸载时，`remove()` 会在 `module_exit()` 内部被联动触发？
