# scrcpy：Windows 11 控制 Android 手机的完整心智模型

## 1. 一句话理解

scrcpy 是一个通过 **ADB（Android Debug Bridge）连接 Android 手机，并把 Android 屏幕实时显示到 Windows，同时把 Windows 的鼠标/键盘操作转发到 Android** 的工具。

最核心的理解：

> **scrcpy = Android Screen Mirroring + Input Forwarding，而 ADB 是它与 Android 沟通的控制通道。**

可以把它理解成：

```text
                 Windows 11
┌──────────────────────────────────────┐
│                                      │
│  Mouse / Keyboard                    │
│        │                             │
│        ▼                             │
│     scrcpy client                    │
│        │                             │
│        │ ADB                         │
└────────┼─────────────────────────────┘
         │
         │ USB / Wi-Fi
         ▼
┌──────────────────────────────────────┐
│            Android Phone             │
│                                      │
│       scrcpy server                  │
│          │        │                  │
│          │        └── Input          │
│          │                           │
│          └── Screen Capture          │
│                                      │
│          Android System              │
│               │                      │
│               ▼                      │
│          Android Apps               │
└──────────────────────────────────────┘
```

## 2. 为什么需要 scrcpy？

如果没有 scrcpy：

```text
Windows
   │
   │ USB
   ▼
Android Phone
```

人必须低头操作手机

有了 scrcpy：

```text
Windows
   │
   │ scrcpy
   ▼
Android Phone
```

```text
人在 Windows 上：
    ↓
看手机屏幕
    ↓
移动鼠标
    ↓
点击
    ↓
输入文字
    ↓
操作 Android App
```

所以 scrcpy 解决的是：

如何把“人在手机上操作 Android”的交互过程搬到 Windows 上。

## 3. scrcpy 和 ADB 是什么关系？

这是最重要的关系之一。

不要把：

scrcpy
ADB

理解成两个互相独立的工具。

更准确的模型是：

```text
                 Windows
                    │
          ┌─────────┴─────────┐
          │                   │
       scrcpy                 ADB
          │                   │
          └─────────┬─────────┘
                    │
                    ▼
                Android
```

其中：

ADB

ADB = Android Debug Bridge。

主要负责：

```text
Windows
   │
   │ 命令 / 数据
   ▼
Android
```

例如：

adb devices
adb shell
adb shell pm list packages
adb shell input tap 100 200

ADB 是：

Windows 与 Android 之间的基础控制通道。

scrcpy

scrcpy 建立在 ADB 之上：

```text
scrcpy
    │
    ▼
   ADB
    │
    ▼
 Android
```

scrcpy 主要解决：

```text
Android Screen
      ↓
实时视频
      ↓
Windows Window
```

```text
Windows Mouse / Keyboard
      ↓
Input Event
      ↓
Android
```

所以可以记成：

ADB
 ↓
连接 / 控制 Android

scrcpy
 ↓
利用 ADB 实现“可视化 + 交互式控制”

## 4. 安装方式

当前使用：

winget install --exact Genymobile.scrcpy

这个安装方式会安装 scrcpy，并带上 ADB。

验证：

scrcpy --version

以及：

adb --version

例如：

Android Debug Bridge version 1.0.41
Version 37.x.x

可以进一步确认：

where.exe scrcpy
where.exe adb

## 5. 整体 Architecture

完整的运行架构可以理解成：

```text
                           Windows 11
┌────────────────────────────────────────────────────────┐
│                                                        │
│   User                                                │
│    │                                                   │
│    ├── Mouse                                           │
│    └── Keyboard                                        │
│          │                                             │
│          ▼                                             │
│      scrcpy client                                     │
│          │                                             │
│          │ ADB                                        │
│          ▼                                             │
└──────────┼─────────────────────────────────────────────┘
           │
           │ USB / TCP/IP
           ▼
┌─────────────────────────────────────────────────────────┐
│                     Android                             │
│                                                         │
│                scrcpy server                            │
│                    │                                    │
│          ┌─────────┴─────────┐                          │
│          │                   │                          │
│          ▼                   ▼                          │
│   Screen Capture       Input Injection                 │
│          │                   │                          │
│          ▼                   ▲                          │
│     Video Stream        Touch / Key Event               │
│          │                   │                          │
│          ▼                   │                          │
│    Android Display      Android Input System             │
│                              │                          │
│                              ▼                          │
│                         Android Apps                    │
└─────────────────────────────────────────────────────────┘
```

核心对象：

| 对象 | 作用 |
| --- | --- |
| Windows | 用户操作环境 |
| scrcpy client | Windows 端控制程序 |
| ADB | Windows ↔ Android 通信桥 |
| scrcpy server | Android 端负责屏幕采集/输入处理的组件 |
| Android Input System | 接收输入事件 |
| Android App | 真正被操作的应用 |

## 6. scrcpy 的核心数据流

scrcpy 实际上有两条非常重要的数据流。

### 6.1 Screen Flow

Android → Windows：

```text
Android App
    ↓
Android Display
    ↓
Screen Capture
    ↓
scrcpy server
    ↓
Video Stream
    ↓
ADB / transport
    ↓
scrcpy client
    ↓
Windows Window
```

可以简化成：

```text
Android Screen
      ↓
  Capture
      ↓
   Encode
      ↓
   Transport
      ↓
   Decode
      ↓
Windows Window
```

## 7. Input Flow

Windows → Android：

```text
Windows Mouse
      │
      ▼
scrcpy client
      │
      ▼
ADB / transport
      │
      ▼
scrcpy server
      │
      ▼
Android Input System
      │
      ▼
Android App
```

例如：

```text
Windows 鼠标左键
        ↓
scrcpy
        ↓
ADB
        ↓
Android Input
        ↓
Touch Event
        ↓
Mobile Tech
```

所以：

scrcpy 并不是“远程桌面”意义上的截图工具，而是建立了一个双向交互通道。

## 8. 第一次使用的完整 Lifecycle

Step 1：Android 开启 Developer Options

一般：

```text
Settings
   ↓
About Phone
   ↓
Build Number
   ↓
连续点击 7 次
   ↓
Developer Options
Step 2：打开 USB Debugging
Developer Options
        │
        └── USB debugging = ON
```

这允许：

```text
Windows
   │
   │ ADB
   ▼
Android
Step 3：USB 连接手机
Windows 11
    │
    │ USB
    ▼
Android
Step 4：确认 ADB
```

执行：

adb devices

第一次可能：

List of devices attached

XXXXXXXX    unauthorized

此时 Android 手机上会出现：

Allow USB debugging?

[Cancel] [Allow]

点击：

Allow

再次：

adb devices

正常结果：

List of devices attached

XXXXXXXX    device

## 9. adb devices 的心智模型

adb devices 是非常重要的诊断命令。

```text
adb devices
       │
       ▼
ADB Server
       │
       ▼
Android Device
```

结果：

device
device

表示：

```text
ADB transport
    ↓
已经建立
    ↓
Android 已授权
```

可以继续使用。

unauthorized
unauthorized

表示：

USB connection     OK
ADB communication  OK
Android permission NOT OK

去手机上允许 USB debugging。

没有设备
List of devices attached

后面没有设备。

优先排查：

```text
USB Cable
   ↓
USB connection
   ↓
Windows Driver
   ↓
USB Debugging
   ↓
ADB Server
```

## 10. 启动 scrcpy

确认：

adb devices

出现：

XXXXXXXX    device

然后：

scrcpy

scrcpy 会：

```text
Windows
   │
   ▼
ADB
   │
   ▼
Android
   │
   ▼
启动 scrcpy server
   │
   ▼
建立视频/控制通道
   │
   ▼
Windows 出现 Android Window
```

## 11. scrcpy Server 的心智模型

不要把它理解成：

手机安装一个普通 App

更适合理解成：

```text
scrcpy.exe
    │
    │ ADB
    ▼
Android
    │
    ▼
scrcpy server
```

server 是为了配合 Windows 端 scrcpy 完成：

Screen Capture
+
Input Handling

因此：

```text
scrcpy client
      │
      │ communication
      ▼
scrcpy server
      │
      ├── Screen
      │
      └── Input
```

## 12. scrcpy 控制 Android 的本质

从系统角度看：

```text
Mouse / Keyboard
       ↓
scrcpy
       ↓
ADB
       ↓
Android Input System
       ↓
Application
```

所以你在 Windows 上：

Mouse Click

本质上不是：

scrcpy 点击自己的窗口

而是：

```text
scrcpy
   ↓
产生对应 Android Input Event
   ↓
Android 接收
   ↓
Android App 收到事件
```

这就是为什么可以真正操作 Android App。

## 13. USB 和 Wi-Fi 的关系

scrcpy 的核心不是 USB。

更准确：

```text
                 scrcpy
                    │
                   ADB
                    │
          ┌─────────┴─────────┐
          │                   │
         USB                TCP/IP
          │                   │
          ▼                   ▼
       Android             Android
```

因此：

```text
scrcpy
   ↓
ADB
   ↓
Transport
   ├── USB
   └── TCP/IP
```

USB 是最简单的第一次连接方式。

## 14. 为什么推荐先 USB？

因为 USB 可以减少变量：

```text
USB
 │
 ├── 不需要 Wi-Fi
 ├── 不需要 IP
 ├── 不需要路由器
 ├── 不需要无线调试
 └── 延迟低
```

所以推荐：

第一阶段：
USB + ADB + scrcpy

第二阶段：
Wireless Debugging + ADB + scrcpy

## 15. 与 Mobile Tech / NSC-200 的关系

对于你的实际使用场景：

```text
                    Windows 11
                        │
                     scrcpy
                        │
                       ADB
                        │
                        ▼
                  OnePlus Phone
                        │
                        ▼
                  Mobile Tech
                        │
                    Bluetooth
                        │
                        ▼
                    NSC-200
                        │
                        ▼
                TCP Throughput
```

这里一定要区分：

```text
scrcpy
   │
   └── 控制 Android
```

```text
Mobile Tech
   │
   └── 控制 NSC-200
```

```text
NSC-200
   │
   └── 执行测试
```

因此：

ADB/scrcpy 不需要理解 NSC-200 协议。

Windows 只是在远程操作 Android 上的 Mobile Tech。

## 16. 对你的场景，最终可以形成三层控制模型

```text
Layer 3 ─ Test Automation
┌───────────────────────────────┐
│ PowerShell / Python            │
│                               │
│ 自动启动 / 配置 / 执行测试     │
└───────────────┬───────────────┘
                │
                ▼
Layer 2 ─ Android Control
┌───────────────────────────────┐
│ ADB / scrcpy                  │
│                               │
│ 启动 App / 点击 / 输入 / 观察   │
└───────────────┬───────────────┘
                │
                ▼
Layer 1 ─ Device Control
┌───────────────────────────────┐
│ Mobile Tech                   │
│                               │
│ Bluetooth → NSC-200           │
│ TCP Throughput                │
└───────────────────────────────┘
```

这比单纯记住：

scrcpy 是投屏工具

更加重要。

## 17. 常用命令

查看版本
scrcpy --version
adb --version
查看设备
adb devices
打开 scrcpy
scrcpy
指定设备

当有多个 Android 设备时：

scrcpy -s 3B663H00U9M00000

ADB：

adb -s 3B663H00U9M00000 shell

## 18. ADB 还能做什么？

scrcpy 主要负责：

Screen + Interactive Control

ADB 本身则提供更底层的 Android 控制能力：

adb shell
adb shell pm list packages
adb shell dumpsys
adb shell input tap 100 200
adb shell input swipe 100 200 500 500
adb shell input text "hello"
adb shell am start ...
adb shell getprop

因此：

scrcpy
    ↓
适合“人操作”

ADB
    ↓
适合“程序控制 Android”

这一区别对于后续自动化非常重要。

## 19. 从人工操作走向自动化

最开始：

```text
Windows
   ↓
scrcpy
   ↓
人用鼠标
   ↓
Mobile Tech
   ↓
NSC-200
```

进一步：

```text
PowerShell / Python
        ↓
       ADB
        ↓
   Android UI
        ↓
   Mobile Tech
        ↓
     NSC-200
```

最终可以演进成：

```text
                 Windows
                    │
             PowerShell/Python
                    │
             ┌──────┴──────┐
             │             │
            ADB          scrcpy
             │             │
             ▼             ▼
          Android      Human Observe
             │
             ▼
        Mobile Tech
             │
         Bluetooth
             │
             ▼
          NSC-200
             │
             ▼
      TCP Throughput
```

此时：

scrcpy

主要用于：

观察 + 人工操作 + Debug

而：

ADB

可以逐渐承担：

启动 App
自动点击
输入参数
获取状态
执行自动化

## 20. Failure Model：出现问题应该在哪一层排查？

不要看到 scrcpy 不工作就直接重装。

按照链路逐层定位：

```text
Windows
   │
   ▼
ADB
   │
   ▼
Android
   │
   ▼
scrcpy
   │
   ▼
Mobile Tech
   │
   ▼
Bluetooth
   │
   ▼
NSC-200
   │
   ▼
TCP Throughput
```

对应排查：

| 阶段 | 观察 | 常见问题 |
| --- | --- | --- |
| Windows → ADB | `adb --version` | ADB 未安装/PATH |
| ADB → Phone | `adb devices` | USB/Driver/授权 |
| scrcpy | 手机窗口出现 | scrcpy/ADB 通道 |
| Mobile Tech | App 能否启动 | Android App |
| Bluetooth | 能否发现 NSC-200 | 蓝牙权限/设备 |
| NSC-200 | 是否连接 | 设备状态 |
| TCP Throughput | 测试是否启动 | Mobile Tech/NSC-200 配置 |

因此最重要的诊断原则是：

先定位链路断在哪一层，再处理这一层的问题。

## 21. 最核心的 Mental Model

最终可以把整个 scrcpy 理解成下面这张图：

```text
                       USER
                        │
                 Mouse / Keyboard
                        │
                        ▼
              ┌─────────────────┐
              │     scrcpy      │
              │ Windows Client  │
              └────────┬────────┘
                       │
                       │ ADB
                       │
              ┌────────▼────────┐
              │     Android     │
              │ scrcpy Server   │
              └───────┬─────────┘
                      / \
                     /   \
                    ▼     ▼
             Screen       Input
                │           │
                ▼           ▼
             Windows      Android
             Display       Input
                            │
                            ▼
                         Android
                           App
```

可以浓缩成一句话：

ADB 是“桥”，scrcpy 是“把这座桥变成可视化、可交互的 Android 控制台”。

再结合你的 NSC-200 场景：

```text
Windows
   │
   │ scrcpy / ADB
   ▼
OnePlus
   │
   │ Mobile Tech
   ▼
NSC-200
   │
   ▼
TCP Throughput
```

所以你现在已经完成：

```text
Windows
   │
   │ USB + ADB
   ▼
OnePlus
```

下一阶段就是：

```text
OnePlus
   │
   ▼
scrcpy
   │
   ▼
Windows 可视化控制
   │
   ▼
Mobile Tech
   │
   ▼
NSC-200 TCP Throughput
```

然后再进一步研究：

```text
ADB
 ↓
Android UI Automation
 ↓
Mobile Tech Automation
 ↓
NSC-200 Test Automation
```

这才是从“Windows 远程操作手机”逐渐演进到“Windows 自动执行 NSC-200 测试”的完整技术路线
