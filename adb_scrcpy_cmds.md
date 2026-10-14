# ADB & scrcpy 80/20 Cheat Sheet

## 目录

- [1. 总体 Mental Model](#1-总体-mental-model)
- [2. 最重要的分层 Mental Model](#2-最重要的分层-mental-model)
- [3. ADB 第一组：Device Discovery](#3-adb-第一组device-discovery)
- [4. ADB 第二组：Shell](#4-adb-第二组shell)
- [5. ADB 第三组：Package / App](#5-adb-第三组package--app)
- [6. ADB 第四组：启动 App / Activity](#6-adb-第四组启动-app--activity)
- [7. ADB 第五组：Logcat](#7-adb-第五组logcat)
- [8. ADB 第六组：文件传输](#8-adb-第六组文件传输)
- [9. ADB 第七组：Input Automation](#9-adb-第七组input-automation)
- [10. ADB 第八组：系统信息](#10-adb-第八组系统信息)
- [11. ADB 第九组：Reboot](#11-adb-第九组reboot)
- [12. ADB 第十组：多设备](#12-adb-第十组多设备)
- [13. ADB Server](#13-adb-server)
- [14. scrcpy Mental Model](#14-scrcpy-mental-model)
- [15. scrcpy 第一组：基本启动](#15-scrcpy-第一组基本启动)
- [16. scrcpy 第二组：性能控制](#16-scrcpy-第二组性能控制)
- [17. scrcpy 第三组：窗口](#17-scrcpy-第三组窗口)
- [18. scrcpy 第四组：关闭 Android 屏幕](#18-scrcpy-第四组关闭-android-屏幕)
- [19. scrcpy 第五组：保持唤醒](#19-scrcpy-第五组保持唤醒)
- [20. scrcpy 第六组：录制](#20-scrcpy-第六组录制)
- [21. ADB + scrcpy 最典型工作流](#21-adb--scrcpy-最典型工作流)
- [22. 80/20 命令表](#22-8020-命令表)
- [23. 最终 Mental Model](#23-最终-mental-model)

---

## 1. 总体 Mental Model

ADB 和 scrcpy 不应该理解成两个独立工具，而应该理解成两层：

```text
Windows 11
│
├── adb
│   │
│   ├── device discovery
│   ├── shell
│   ├── file transfer
│   ├── APK install/uninstall
│   ├── app/process control
│   ├── logcat
│   ├── input
│   └── reboot
│
└── scrcpy
    │
    ├── screen streaming
    ├── keyboard input
    ├── mouse input
    ├── recording
    └── interactive control
         │
         ▼
      ADB
         │
         ▼
      Android
```

核心关系：

```text
adb
 │
 └── Android 控制基础设施

scrcpy
 │
 └── 基于 ADB 的 Android 图形化交互工具
```

因此：

> ADB 是控制 Android 的"命令行总线"；scrcpy 是建立在这个总线之上的"远程屏幕 + 输入层"。

## 2. 最重要的分层 Mental Model

日常遇到问题时，优先判断属于哪一层：

```text
┌─────────────────────────────┐
│ Application                 │
│ Mobile Tech / Chrome / App  │
└──────────────┬──────────────┘
               │
┌──────────────▼──────────────┐
│ scrcpy                      │
│ Screen + Keyboard + Mouse   │
└──────────────┬──────────────┘
               │
┌──────────────▼──────────────┐
│ adb                         │
│ shell / files / app / logs  │
└──────────────┬──────────────┘
               │
┌──────────────▼──────────────┐
│ ADB Server / Transport      │
│ USB / TCP/IP                │
└──────────────┬──────────────┘
               │
┌──────────────▼──────────────┐
│ Android OS                  │
│ Package / Activity / Input  │
│ File / Process / Logcat     │
└─────────────────────────────┘
```

排障时不要混层：

| 现象 | 所属层 / 排查方向 |
|---|---|
| scrcpy 打不开 | scrcpy layer |
| `adb devices` 看不到手机 | ADB / USB / authorization |
| `adb shell` 正常，但 App 崩溃 | Android Application |
| App 正常，但 Mobile Tech 找不到 NSC-200 | Mobile Tech / Bluetooth / Network |
| Mobile Tech 找到 NSC-200，但测试失败 | NSC-200 / Test configuration / Network |

## 3. ADB 第一组：Device Discovery

### 3.1 查看设备

```bash
adb devices
```

**Mental Model：**

```text
Windows
   │
   │ adb devices
   ▼
ADB Server
   │
   ▼
Device list
```

典型结果：

```text
List of devices attached
3Bxxxxxxxxxxxx    device
```

常见状态：

- `device` → 正常连接
- `unauthorized` → Android 尚未授权这台电脑
- `offline` → ADB transport 存在问题

### 3.2 查看 adb 版本

```bash
adb version
```

### 3.3 找到实际使用的 adb

```bash
where.exe adb
```

**Mental Model：**

```text
PowerShell
   │
   └── PATH
        │
        ├── adb.exe A
        └── adb.exe B
```

如果存在多个 `adb.exe`，需要确认到底使用的是哪一个。

## 4. ADB 第二组：Shell

这是 ADB 最核心的能力之一。

### 4.1 进入 Android Shell

```bash
adb shell
```

**Mental Model：**

```text
PowerShell
    │
    │ adb shell
    ▼
Android Shell
    │
    ├── ls
    ├── ps
    ├── ip
    ├── getprop
    ├── df
    └── ...
```

### 4.2 直接执行一条命令

```bash
adb shell <command>
```

例如：

```bash
adb shell getprop
adb shell ps
adb shell ip addr
adb shell df -h
```

不要进入 shell 也可以直接执行 Android 命令。

## 5. ADB 第三组：Package / App

Android 应用的核心标识是：

> package name

例如：

```text
com.viavisolutions.mobiletech
```

### 5.1 查看所有 Package

```bash
adb shell pm list packages
```

搜索：

```bash
adb shell pm list packages | findstr /i viavi
```

### 5.2 查看 APK 路径

```bash
adb shell pm path <package>
```

例如：

```bash
adb shell pm path com.viavisolutions.mobiletech
```

**Mental Model：**

```text
Package Name
     │
     ▼
Package Manager
     │
     ├── APK
     ├── permissions
     ├── activities
     └── services
```

### 5.3 安装 APK

```bash
adb install app.apk
```

覆盖安装：

```bash
adb install -r app.apk
```

**Mental Model：**

```text
Windows
   │
   │ adb install
   ▼
Android Package Manager
   │
   ▼
APK
   │
   ▼
Installed Package
```

### 5.4 卸载

```bash
adb uninstall <package>
```

## 6. ADB 第四组：启动 App / Activity

### 6.1 启动应用

常见方式：

```bash
adb shell monkey -p <package> 1
```

例如：

```bash
adb shell monkey -p com.viavisolutions.mobiletech 1
```

### 6.2 直接启动 Activity

```bash
adb shell am start -n <package>/<activity>
```

例如：

```bash
adb shell am start -n com.viavisolutions.mobiletech/.MainActivity
```

**Mental Model：**

```text
ADB
 │
 ▼
Activity Manager
 │
 ▼
Activity
 │
 ▼
Application
```

如果你需要自动化 Android App，这是非常重要的一层。

## 7. ADB 第五组：Logcat

### 7.1 查看实时日志

```bash
adb logcat
```

### 7.2 清空旧日志

```bash
adb logcat -c
```

推荐调试流程：

```bash
adb logcat -c
```

然后：

```bash
adb logcat
```

再执行：

```text
启动 App
    ↓
执行操作
    ↓
观察 logcat
```

### 7.3 保存日志

```bash
adb logcat > log.txt
```

### 7.4 搜索关键字

PowerShell：

```powershell
adb logcat | Select-String -Pattern "viavi|exception|crash"
```

或者：

```powershell
adb logcat | findstr /i "FATAL EXCEPTION"
```

**Mental Model：**

```text
Application
    │
    ├── log
    ├── warning
    ├── error
    └── crash
          │
          ▼
       logcat
          │
          ▼
       adb logcat
          │
          ▼
       PowerShell
```

## 8. ADB 第六组：文件传输

### 8.1 Windows → Android

```bash
adb push <local> <remote>
```

例如：

```bash
adb push test.txt /sdcard/test.txt
```

### 8.2 Android → Windows

```bash
adb pull <remote> <local>
```

例如：

```bash
adb pull /sdcard/test.txt .
```

**Mental Model：**

```text
adb push

Windows ───────────────► Android


adb pull

Windows ◄─────────────── Android
```

## 9. ADB 第七组：Input Automation

ADB 可以直接向 Android Input subsystem 注入输入。

### 9.1 点击

```bash
adb shell input tap 500 1000
```

### 9.2 滑动

```bash
adb shell input swipe 500 1500 500 500 500
```

### 9.3 输入文字

```bash
adb shell input text "hello"
```

### 9.4 Android 按键

- 返回：`adb shell input keyevent KEYCODE_BACK`
- Home：`adb shell input keyevent KEYCODE_HOME`
- Power：`adb shell input keyevent KEYCODE_POWER`

**Mental Model：**

```text
PowerShell
    │
    ▼
ADB
    │
    ▼
Android Input
    │
    ├── tap
    ├── swipe
    ├── text
    └── keyevent
```

这就是 Android GUI 自动化的底层入口之一。

## 10. ADB 第八组：系统信息

| 项目 | 命令 |
|---|---|
| Android Version | `adb shell getprop ro.build.version.release` |
| Device Model | `adb shell getprop ro.product.model` |
| CPU | `adb shell cat /proc/cpuinfo` |
| Memory | `adb shell cat /proc/meminfo` |
| Storage | `adb shell df -h` |
| Network（地址） | `adb shell ip addr` |
| Network（路由） | `adb shell ip route` |
| Process | `adb shell ps` |

**Mental Model：**

```text
Android
   │
   ├── /proc
   ├── properties
   ├── filesystem
   ├── network
   └── processes
          │
          ▼
       adb shell
```

## 11. ADB 第九组：Reboot

- 普通重启：`adb reboot`
- Bootloader：`adb reboot bootloader`
- Recovery：`adb reboot recovery`

日常开发最常用的是：

```bash
adb reboot
```

## 12. ADB 第十组：多设备

如果：

```bash
adb devices
```

得到：

```text
device-A    device
device-B    device
```

必须指定设备：

```bash
adb -s device-A shell
```

例如：

```bash
adb -s 3Bxxxxxxxx shell
```

**Mental Model：**

```text
                 adb
                  │
        ┌─────────┼─────────┐
        ▼         ▼         ▼
    device-A  device-B  device-C
```

`-s`：

```text
-s <serial>
    │
    └── 指定目标 Android device
```

## 13. ADB Server

ADB 实际上不是：

```text
adb.exe ───────► Android
```

更准确的模型是：

```text
adb client
     │
     │ localhost:5037
     ▼
 adb server
     │
     ├── USB
     │
     └── TCP/IP
          │
          ▼
       Android
```

启动：

```bash
adb start-server
```

停止：

```bash
adb kill-server
```

经典排障：

```bash
adb kill-server
adb start-server
adb devices
```

**Mental Model：**

> `adb client` 是你输入的命令；`adb server` 才是负责管理 Android transport 的后台进程。

## 14. scrcpy Mental Model

scrcpy 可以理解成：

```text
                    Windows
                       │
                 ┌─────┴─────┐
                 │  scrcpy    │
                 └─────┬─────┘
                       │
                     ADB
                       │
                       ▼
                    Android
                       │
              ┌────────┴────────┐
              │                 │
           Screen             Input
              │                 │
              ▼                 ▼
          video stream      keyboard/mouse
              │                 │
              └────────┬────────┘
                       ▼
                    Windows
```

所以：

- **ADB** = Android 控制基础设施
- **scrcpy** = Android 屏幕 + 输入的交互层

## 15. scrcpy 第一组：基本启动

```bash
scrcpy
```

这是最常用命令。

指定设备：

```bash
scrcpy -s <serial>
```

例如：

```bash
scrcpy -s 3Bxxxxxxxx
```

## 16. scrcpy 第二组：性能控制

限制分辨率：

```bash
scrcpy -m 1280
```

或者：

```bash
scrcpy --max-size 1280
```

**Mental Model：**

```text
Android
1920x1080
    │
    │ --max-size 1280
    ▼
scrcpy video stream
```

用途，降低：

- bandwidth
- CPU
- encoding load
- display load

限制码率：

```bash
scrcpy -b 8M
```

或者：

```bash
scrcpy --video-bit-rate 8M
```

典型组合：

```bash
scrcpy -m 1280 -b 8M
```

## 17. scrcpy 第三组：窗口

全屏：

```bash
scrcpy -f
```

或者：

```bash
scrcpy --fullscreen
```

## 18. scrcpy 第四组：关闭 Android 屏幕

```bash
scrcpy -S
```

或者：

```bash
scrcpy --turn-screen-off
```

**Mental Model：**

```text
Android Screen
      X
      │
      │
scrcpy
      │
      ▼
Windows
```

手机屏幕关闭，但仍然可以从 Windows 控制 Android。

## 19. scrcpy 第五组：保持唤醒

```bash
scrcpy --stay-awake
```

适合长时间运行的测试：

- Mobile Tech
- NSC-200
- TCP Throughput

避免 Android 因为休眠影响操作。

## 20. scrcpy 第六组：录制

```bash
scrcpy --record test.mp4
```

例如：

```bash
scrcpy --record nsc200-test.mp4
```

**Mental Model：**

```text
Android Screen
      │
      ▼
scrcpy video stream
      │
      ├── Windows display
      │
      └── MP4 recording
```

## 21. ADB + scrcpy 最典型工作流

### 日常控制手机

```bash
adb devices
scrcpy
```

### 调试 App

```bash
adb devices
adb logcat -c
scrcpy
```

然后：

```text
scrcpy 中操作 App
        ↓
PowerShell
        ↓
adb logcat
        ↓
观察错误
```

### 安装 + 启动 App

```bash
adb install -r app.apk
adb shell monkey -p com.example.app 1
scrcpy
```

### App 崩溃排查

```bash
adb logcat -c
adb shell monkey -p com.example.app 1
adb logcat
```

重点寻找：

- `FATAL EXCEPTION`
- `AndroidRuntime`
- `Exception`
- `SecurityException`
- `Permission`

### GUI 自动化

```text
PowerShell
    │
    ▼
ADB
    │
    ├── am start
    │
    ├── input tap
    │
    ├── input swipe
    │
    ├── input text
    │
    └── uiautomator
             │
             ▼
         Android App
```

其中：

```bash
adb shell uiautomator dump /sdcard/window.xml
```

可以查看当前 UI hierarchy。

## 22. 80/20 命令表

| 场景 | 命令 |
|---|---|
| 查看设备 | `adb devices` |
| 查看 adb | `adb version` |
| 找 adb | `where.exe adb` |
| Android shell | `adb shell` |
| 执行 Android 命令 | `adb shell <cmd>` |
| 查看 Package | `adb shell pm list packages` |
| 搜索 Package | `adb shell pm list packages \| findstr <name>` |
| APK 路径 | `adb shell pm path <package>` |
| 安装 APK | `adb install app.apk` |
| 覆盖安装 | `adb install -r app.apk` |
| 卸载 | `adb uninstall <package>` |
| 启动 App | `adb shell monkey -p <package> 1` |
| 启动 Activity | `adb shell am start -n <package>/<activity>` |
| 实时日志 | `adb logcat` |
| 清日志 | `adb logcat -c` |
| 保存日志 | `adb logcat > log.txt` |
| Windows → Android | `adb push local remote` |
| Android → Windows | `adb pull remote local` |
| 点击 | `adb shell input tap x y` |
| 输入文字 | `adb shell input text "..."` |
| 返回 | `adb shell input keyevent KEYCODE_BACK` |
| Home | `adb shell input keyevent KEYCODE_HOME` |
| 重启 | `adb reboot` |
| 重启 ADB | `adb kill-server; adb start-server` |
| 指定设备 | `adb -s <serial> <command>` |
| 启动 scrcpy | `scrcpy` |
| 指定设备 | `scrcpy -s <serial>` |
| 限制分辨率 | `scrcpy -m 1280` |
| 限制码率 | `scrcpy -b 8M` |
| 全屏 | `scrcpy -f` |
| 关闭手机屏幕 | `scrcpy -S` |
| 保持唤醒 | `scrcpy --stay-awake` |
| 录制 | `scrcpy --record test.mp4` |
| 查看帮助 | `scrcpy --help` |

## 23. 最终 Mental Model

不要背 30+ 个命令。

记住下面这张图：

```text
                         Windows
                            │
              ┌─────────────┴─────────────┐
              │                           │
             adb                       scrcpy
              │                           │
       Android CLI              Screen + Input
              │                           │
              └─────────────┬─────────────┘
                            │
                       ADB Server
                            │
                     USB / TCP/IP
                            │
                            ▼
                         Android
                            │
          ┌─────────────────┼─────────────────┐
          │                 │                 │
       Package            Shell            Input
          │                 │                 │
       App/ APK          Linux cmds       tap/key/text
          │                 │                 │
          └─────────────────┼─────────────────┘
                            │
                          Logs
                            │
                         logcat
```

因此可以把 80/20 简化成：

| 命令 | 回答的问题 |
|---|---|
| `adb devices` | 我有哪些 Android device？ |
| `adb shell` | 我能进入 Android 做什么？ |
| `adb shell pm ...` | 有哪些 App？ |
| `adb shell am ...` | 如何启动 App / Activity？ |
| `adb install` / `uninstall` | 如何管理 App？ |
| `adb push` / `pull` | 如何搬文件？ |
| `adb logcat` | App 出问题发生了什么？ |
| `adb shell input ...` | 如何从命令行控制 GUI？ |
| `adb -s ...` | 我要控制哪台设备？ |
| `scrcpy` | 如何把 Android GUI 放到 Windows？ |
| `scrcpy -m` / `-b` | 如何控制视频质量和带宽？ |
| `scrcpy -S` / `--stay-awake` | 如何让长时间测试更稳定？ |
| `scrcpy --record` | 如何记录测试过程？ |

### 一句话总结

> ADB 负责"发现设备、执行命令、管理 App、传输文件、获取日志、注入输入"；scrcpy 负责"把 Android 的屏幕和输入交互搬到 Windows"。

对于 Windows 11 → Android → Mobile Tech → NSC-200 场景，最重要的下一层就是：

```text
adb
 │
 ├── shell
 ├── Activity / Intent
 ├── uiautomator
 ├── input
 └── logcat
        │
        ▼
Mobile Tech automation
        │
        ▼
NSC-200
```

这也是从"手工点击 Mobile Tech"逐步演进到"PowerShell 自动化 TCP Throughput 测试"的核心技术路径。
