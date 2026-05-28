# SSH Config 进阶与高级实用 Tricks 全景指南

---

## 第一部分：基础连接与优化

### 1. 全局心跳保活与连接复用（补充）
#### 💡 Trick: 毫秒级免密登录与防超时掉线
SSH 经常因为长时间无操作或网络抖动而断开；此外，在频繁通过工具或脚本多次登录同一服务器时，每次握手和认证都极为耗时。通过连接复用，首个连接建立后在后台保持 Socket，后续连接毫秒级登录。

```ssh-config
Host *
    # 全局开启心跳保活，每 60 秒发一次心跳包，最多 3 次无响应才断开
    ServerAliveInterval 60
    ServerAliveCountMax 3
    # 开启连接复用，首个连接建立后在后台保持 Socket
    ControlMaster auto
    ControlPath ~/.ssh/sockets/%r@%h:%p
    # 退出最后一个终端后，后台进程仍保留 1 小时
    ControlPersist 3600
```
*(注：需提前在本地终端执行 `mkdir -p ~/.ssh/sockets/` 创建目录)*

### 2. 动态主机解析：利用 %h 变量批量映射
#### 💡 Trick: 统一配置千台相同规则的设备
如果你管理的机器有一整套特定的命名规则或端口规则，不需要为每台机写一个 Host。

```ssh-config
# 匹配所有以 vcmts- 开头的别名，如 vcmts-41, vcmts-42
Host vcmts-*
    # %h 代表你输入的 Host 别名。输入 ssh vcmts-41，实际连接的就是 vcmts-41.corp.internal
    HostName %h.corp.internal
    User tier3support
    Port 5617
```


## 第二部分：身份认证与安全

### 3. 动态身份流控：多账号防串线
#### 💡 Trick: IdentitiesOnly 强锁定，拒绝多密钥轮询导致的被封 IP
当本地有 5 个以上私钥时，SSH 登录服务器默认会把所有密钥轮流试一遍。许多高安全级别的服务器（或特定的 Git/Gerrit 系统）在连续错 3 次密钥后会直接断开连接（Too many authentication failures）。

```ssh-config
Host git-secure
    HostName git.secure.corp.com
    User git
    IdentityFile ~/.ssh/id_ed25519_strict
    # 【核心】告诉 SSH 客户端：只准用上面指定的这把钥匙，绝对不准用 ssh-agent 缓存的其他钥匙去试
    IdentitiesOnly yes
```


## 第三部分：内网穿透与高级路由

### 4. 零接触内网穿透：智能 ProxyJump 级联
#### 💡 Trick: 多级跳板机无缝嵌套
当目标服务器在多层深水内网中，你需要先登录外网堡垒机 A，再跳到内网核心机 B，最后才能到达目标机 C。`ProxyJump` 支持逗号分隔的级联语法，一行配置自动完成多级穿透。

```ssh-config
Host internal-target
    HostName 192.168.10.5
    User tier3support
    # 一键穿透双层跳板机：本地 -> bastionA -> bastionB -> 最终目标
    ProxyJump bastionA,bastionB

Host bastionA
    HostName 203.0.113.1
    User admin

Host bastionB
    HostName 10.254.1.10
    User middleman
    ProxyJump bastionA
```

### 5. 经典代理穿透：ProxyCommand（补充）
#### 💡 Trick: 让 SSH 流量包裹进本地 HTTP/Socks5 代理中
公司严格限制网络时，需要通过本机的代理软件去连接外部服务器（如 GitHub），此时 ProxyJump 无法使用，需借助 `nc` 命令进行流量重定向。

```ssh-config
Host external-git
    HostName github.com
    User git
    # 利用 nc (netcat) 命令，让 SSH 流量包裹进本地的 1080 Socks5 代理中
    ProxyCommand nc -x 127.0.0.1:1080 %h %p
```

### 6. 条件自适应分支：Match 动态路由
#### 💡 Trick: 基于本地网络环境自动决定是否使用跳板机
带笔记本电脑移动办公时，在公司内网可以直连测试机（速度快），在公网（家里/咖啡厅）必须挂跳板机。利用 Match exec 动态检测本地网关或网络状态。

```ssh-config
# 1. 定义基础节点配置
Host dev-cluster-*
    User admin
    Port 5617

# 2. 条件判断：如果能 ping 通公司内网网关 (10.254.25.1)，则直连目标 IP
Match Host dev-cluster-* exec "ping -c 1 -W 1 10.254.25.1 >/dev/null"
    HostName 10.254.25.%h

# 3. 条件判断：如果 ping 不通内网网关（在公网），自动强加 ProxyJump 跳板机
Match Host dev-cluster-* !exec "ping -c 1 -W 1 10.254.25.1 >/dev/null"
    HostName 10.254.25.%h
    ProxyJump external-bastion.corp.com
```

### 7. 交互黑魔法：RemoteCommand & RequestTTY 别名嵌套
#### 💡 Trick: 解决老旧跳板机或登录后自动进入特定容器/伪终端的问题
当某些环境由于安全策略或 OpenSSH 版本过老无法使用 ProxyJump 时，或者登录机器后必须立刻执行某个脚本（如进入特殊的 TUI 交互界面），可用此策。

```ssh-config
Host rpd-console
    HostName 10.254.25.42
    User root
    # 登录成功后，在远程主机上自动触发二次 ssh 连接，建立交互式反向链路
    RemoteCommand ssh -tt -o StrictHostKeyChecking=no tier3support@65.100.0.92
    # 强制分配伪终端（极其关键，否则无法响应 Ctrl+C，且没有命令提示符）
    RequestTTY yes
```


## 第四部分：端口转发与隧道代理

### 8. 一键隧道化：本地/远程端口转发自动化
#### 💡 Trick: 把复杂的 ssh -L 或 ssh -R 固化为单单词命令
不要再去记长串的 -L 或 -R 命令。直接将内网数据库、Kafka 监控面板（ksqlDB）、Nomad 控制台映射到本地，或者反向映射出去。

```ssh-config
# 场景 A：正向端口转发 (LocalForward) —— 把内网数据库拉到本地
Host db-tunnel
    HostName 203.0.113.1
    User admin
    # 访问本地 13306 端口，等同于通过跳板机访问内网 10.254.25.40:3306
    LocalForward 13306 10.254.25.40:3306
    # 顺便拉取 Nomad 仪表盘
    LocalForward 14646 10.254.25.40:4646

# 场景 B：反向端口转发 (RemoteForward) —— 把本地测试服务推到公网核心机
Host proxy-shuttle
    HostName 135.242.60.169
    User morrism
    # 远程机器访问它自己的 58080 端口时，流量自动原路穿透回你本地的 3128 端口
    RemoteForward 58080 127.0.0.1:3128
```

### 9. 智能动态代理：Socks5 一键上网
#### 💡 Trick: 开启 DynamicForward 作为全能网络代理
如果你需要访问内网网段中的数十个不同的 Web 服务或 API 节点，为每个端口写一个 LocalForward 会疯掉。可以使用动态代理开启一个本地 Socks5 隧道。

```ssh-config
Host vpn-fallback
    HostName 203.0.113.1
    User admin
    # 在本地 1080 端口开启 Socks5 代理服务
    DynamicForward 1080
```
*(使用方法：连接 `ssh vpn-fallback` 后，配置本地浏览器（如 SwitchyOmega 插件）或终端 `export ALL_PROXY=socks5://127.0.0.1:1080`，即可自由访问该跳板机能触达的任何内网资源。)*

### 10. 自动回收站：连接失败立即优雅退出
#### 💡 Trick: 配合脚本或守护进程时，防止因端口冲突引发僵尸进程
在跑反向通道、端口转发脚本或通过 cron 守护 SSH 隧道时，如果远程端口已经被占用，默认情况下 SSH 仍会顽固地保持连接，但这其实是一个“死隧道”。

```ssh-config
Host tunnel-watcher
    HostName 135.242.60.169
    # 如果端口转发失败（比如远程 58080 端口被残留进程占用了），直接报错退出，而不是建立假连接
    ExitOnForwardFailure yes
    # 配合心跳，断线时立即退出进程以便守护脚本（如 systemd）重新拉起
    ServerAliveInterval 10
    ServerAliveCountMax 2
```


## 第五部分：高阶调试与极速传输

### 11. 极速文件传输：针对 SCP / SFTP / Rsync 的局部优化
#### 💡 Trick: 用更快的加密算法加速大文件或局域网内的同步
默认的加密算法（如强化的 AES）非常消耗 CPU。在安全的内部局域网（Lab/VCMTS 测试网）传输数 GB 的 pcap 抓包文件、系统镜像或日志时，可以通过指定低开销算法将速度拉满。

```ssh-config
Host lab-bulk-transfer
    HostName 10.254.25.136
    User root
    # 使用硬件加速极好、开销极低的 chacha20 算法或 aes128-gcm 提升吞吐量
    Ciphers chacha20-poly1305@openssh.com,aes128-gcm@openssh.com
    # 开启压缩（对大文本日志极为有效）
    Compression yes
```

### 12. 极客调试与免警告：严格主机密钥检查关闭
#### 💡 Trick: 针对频繁重装、IP 经常变动的实验虚拟机群关闭 HostKey 警告
在研发和调试阶段，嵌入式设备、RPD 或本地虚拟机经常需要反复擦写系统、频繁重装。每次重装后连接都会报大红字 WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!。

```ssh-config
# 仅针对本地局域网或开发测试网段关闭检查，切勿用于生产环境！
Host 192.168.244.* 10.254.25.*
    # 不把识别到的主机 key 写入 ~/.ssh/known_hosts
    UserKnownHostsFile /dev/null
    # 自动接受新的或发生变动的主机公钥，不再弹窗输入 yes/no
    StrictHostKeyChecking no
```