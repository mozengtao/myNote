# 示例二：智能家居设备控制

## 场景

`SmartHome` 管理若干 `Room`，每个 `Room` 管理若干 `Device`（`Light` / `Thermostat`）。可以对单个设备操作，也可以一键关闭某个房间的所有设备。

## 对应的心智模型要点

对照 [`complex_obj_organization.md`](../../complex_obj_organization.md) 第十七节 Checklist：

| Checklist 原则 | 本示例中的体现 |
|---|---|
| Composition（Has-A） | `SmartHome` has `Room`，`Room` has `Device`；`Light` / `Thermostat` 各自独立实现开关逻辑，而不是被塞进一个巨大的基类 |
| Hide Structure | `home.turn_off_all("客厅")` 隐藏了"客厅内部到底有几个设备、用什么容器存储"这些细节 |
| Single Responsibility | `Light` 只关心亮度，`Thermostat` 只关心温度，`Room` 只负责管理设备集合，`SmartHome` 只负责管理房间集合 |
| Encapsulation | 设备的开关状态（`_is_on`）只能通过 `turn_on()` / `turn_off()` 修改，不允许外部直接赋值 |

## 运行方式

```bash
cd python/complex_obj_organization_examples/smart_home_control
python3 main.py
```
