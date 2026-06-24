# DNF 包管理工具日常使用指南（心智模型 + 最佳实践）

## 一、DNF 的心智模型

可以把 DNF 看成 Linux 系统的软件应用商店：

```
                ┌────────────────────┐
                │   Remote Repos     │
                │ (软件仓库/镜像源)    │
                └─────────┬──────────┘
                          │
                    makecache/update
                          │
                          ▼
                ┌────────────────────┐
                │   DNF Metadata     │
                │ (本地软件包索引)     │
                └─────────┬──────────┘
                          │
       ┌──────────────────┼───────────────────┐
       │                  │                   │
       ▼                  ▼                   ▼
  search/info        install/remove      upgrade/update
       │                  │                   │
       └──────────────────┴───────────────────┘
                          │
                          ▼
                ┌────────────────────┐
                │ Installed Packages │
                │   已安装软件数据库    │
                └────────────────────┘
```

理解 DNF 只需要掌握四个对象：

|对象|作用|
|---|---|
|Repository（仓库）|软件来源|
|Metadata（元数据）|仓库的软件索引|
|Package（软件包）|实际安装的软件|
|Dependencies（依赖）|软件运行所需的其他包|

---

# 二、日常工作流

```
搜索软件
    ↓
查看详情
    ↓
安装
    ↓
使用
    ↓
更新
    ↓
卸载
    ↓
清理
```

---

# 三、软件查询

## 1. 搜索软件包

作用：

- 不知道准确包名时使用
- 查看系统仓库是否提供该软件

语法：

```bash
dnf search <keyword>
```

示例：

```bash
dnf search tig
dnf search ncurses
dnf search readline
dnf search git
```

---

## 2. 查看软件详细信息

作用：

- 查看版本
- 查看描述
- 查看来源仓库

语法：

```bash
dnf info <package>
```

示例：

```bash
dnf info tig
dnf info git
dnf info ncurses-dev
```

---

## 3. 查看是否已安装

语法：

```bash
dnf list installed
```

查看指定软件：

```bash
dnf list installed | grep tig
dnf list installed | grep git
```

或者：

```bash
rpm -q tig
rpm -q git
```

---

## 4. 查看软件提供了哪些文件

语法：

```bash
rpm -ql <package>
```

示例：

```bash
rpm -ql tig

rpm -ql ncurses-dev
```

输出：

```text
/usr/bin/tig
/usr/share/man/man1/tig.1.gz
...
```

---

# 四、安装软件

## 1. 安装普通软件

语法：

```bash
sudo dnf install -y <package>
```

示例：

```bash
sudo dnf install -y git
sudo dnf install -y tig
sudo dnf install -y vim
```

---

## 2. 安装多个软件

```bash
sudo dnf install -y git wget curl
```

---

## 3. 安装开发库

作用：

源码编译时提供：

- 头文件 (.h)
- 动态链接库 (.so)
- pkg-config 信息

vcmos 系统通常命名：

```text
xxx-dev
```

示例：

```bash
sudo dnf install -y ncurses-dev
sudo dnf install -y libreadline-dev
sudo dnf install -y libpcre-dev
```

---

## 4. 安装静态库

作用：

生成完全静态的二进制。

提供：

```text
*.a
```

语法：

```bash
sudo dnf install -y <package>-staticdev
```

示例：

```bash
sudo dnf install -y ncurses-staticdev
sudo dnf install -y libreadline-staticdev
```

---

## 5. 仅下载不安装

语法：

```bash
dnf download <package>
```

示例：

```bash
dnf download tig
```

下载得到：

```text
tig-*.rpm
```

---

# 五、源码编译场景

## 典型流程

```
发现缺少依赖
      ↓
搜索开发库
      ↓
安装 xxx-dev
      ↓
./configure
      ↓
make
      ↓
make install
```

例如编译 tig：

```bash
sudo dnf install -y \
    ncurses-dev \
    libreadline-dev \
    libpcre-dev
```

然后：

```bash
make configure
./configure
make

sudo make install
```

---

# 六、更新系统

## 1. 查看可更新的软件

```bash
dnf check-update
```

---

## 2. 更新全部软件

```bash
sudo dnf update
```

或者：

```bash
sudo dnf upgrade
```

---

## 3. 更新指定软件

```bash
sudo dnf upgrade tig
```

---

# 七、卸载软件

## 1. 卸载软件

语法：

```bash
sudo dnf remove -y <package>
```

示例：

```bash
sudo dnf remove -y tig
sudo dnf remove -y git
```

---

## 2. 自动清理无用依赖

语法：

```bash
sudo dnf autoremove
```

作用：

删除因安装软件而带来的孤儿依赖。

---

# 八、缓存管理

DNF 会缓存仓库索引和下载的 rpm。

```
Remote Repo
     ↓
makecache
     ↓
Local Metadata Cache
```

---

## 1. 重新生成缓存

```bash
sudo dnf makecache
```

适用于：

```text
搜索不到新包
仓库更新后
```

---

## 2. 清理缓存

```bash
sudo dnf clean all
```

清理：

```text
metadata
packages
dbcache
```

---

## 3. 查看缓存大小

```bash
du -sh /var/cache/dnf
```

---

# 九、依赖分析

## 查看依赖

```bash
dnf repoquery --requires tig
```

---

## 查看谁依赖它

```bash
dnf repoquery --whatrequires tig
```

---

## 查看安装原因

```bash
dnf history userinstalled
```

查看哪些是主动安装的软件。

---

# 十、历史操作管理

DNF 会记录所有事务。

---

## 查看历史

```bash
dnf history
```

示例：

```text
ID | Command line
1  | install git
2  | install tig
3  | remove tig
```

---

## 查看某次操作详情

```bash
dnf history info 3
```

---

## 回滚操作

```bash
sudo dnf history undo 3
```

撤销第 3 次事务。

---

# 十一、仓库管理

## 查看仓库

```bash
dnf repolist
```

显示：

```text
repo id
repo name
状态
```

---

## 查看所有仓库

```bash
dnf repolist all
```

---

## 启用仓库

```bash
sudo dnf config-manager --set-enabled <repo>
```

---

## 禁用仓库

```bash
sudo dnf config-manager --set-disabled <repo>
```

---

# 十二、常见问题排查

## 搜索不到软件

先刷新缓存：

```bash
sudo dnf makecache
```

再搜索：

```bash
dnf search xxx
```

---

## configure 提示找不到库

安装对应开发包：

```text
xxx-dev
```

例如：

```bash
sudo dnf install -y openssl-dev
```

---

## 缺少头文件

例如：

```text
fatal error: readline/readline.h
```

安装：

```bash
sudo dnf install -y libreadline-dev
```

---

## 缺少静态库

例如：

```text
cannot find -lreadline
```

安装：

```bash
sudo dnf install -y libreadline-staticdev
```

---

# 十三、最常用命令速查表

|场景|命令|
|---|---|
|搜索软件|`dnf search xxx`|
|查看详情|`dnf info xxx`|
|查看已安装|`dnf list installed`|
|安装软件|`sudo dnf install -y xxx`|
|安装开发库|`sudo dnf install -y xxx-dev`|
|安装静态库|`sudo dnf install -y xxx-staticdev`|
|更新系统|`sudo dnf update`|
|更新单个软件|`sudo dnf upgrade xxx`|
|卸载软件|`sudo dnf remove -y xxx`|
|清理无用依赖|`sudo dnf autoremove`|
|刷新缓存|`sudo dnf makecache`|
|清理缓存|`sudo dnf clean all`|
|查看历史|`dnf history`|
|回滚事务|`sudo dnf history undo ID`|
|查看仓库|`dnf repolist`|
|查看依赖|`dnf repoquery --requires xxx`|

---

# 十四、DNF 心智模式总结

```
┌────────────┐
│  搜索软件   │ dnf search
└─────┬──────┘
      ↓
┌────────────┐
│ 查看详情    │ dnf info
└─────┬──────┘
      ↓
┌────────────┐
│ 安装软件    │ dnf install
└─────┬──────┘
      ↓
┌────────────┐
│ 使用软件    │
└─────┬──────┘
      ↓
┌────────────┐
│ 更新软件    │ dnf upgrade
└─────┬──────┘
      ↓
┌────────────┐
│ 卸载软件    │ dnf remove
└─────┬──────┘
      ↓
┌────────────┐
│ 清理系统    │ autoremove
│            │ clean all
└────────────┘
```

一句话记忆：

> **search → info → install → use → upgrade → remove → clean**

如果涉及源码编译，再增加一步：

> **install xxx-dev → configure → make → make install**