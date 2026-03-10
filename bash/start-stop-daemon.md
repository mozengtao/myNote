# `start-stop-daemon` 命令总结

`start-stop-daemon` 是一个用于管理守护进程（daemon）的 Linux 命令行工具，主要在基于 Debian 的系统中（如 Ubuntu）的 `init.d` 脚本中广泛使用。它的核心作用是标准化地启动、停止和重启系统服务，确保服务能在后台正确运行，并以指定的用户身份执行。

## 常用选项及其含义

| 选项             | 含义                                                               |
| :--------------- | :----------------------------------------------------------------- |
| `--start`        | 尝试启动一个守护进程。如果进程已运行，则不会重复启动。             |
| `--stop`         | 尝试停止一个守护进程。通过 PID 文件、名称、UID 等方式识别进程。    |
| `--restart`      | 停止现有进程（如果存在），然后启动新进程。                         |
| `--status`       | 检查守护进程是否正在运行，并返回相应的退出状态码。                 |
| `--pidfile <PID文件>` | 指定包含守护进程 PID 的文件路径。用于识别和管理进程。            |
| `--exec <可执行文件>`  | 指定要执行的程序路径。                                             |
| `--name <进程名称>`    | 指定进程的名称（通常是可执行文件的 basename）。用于识别进程。      |
| `--user <用户/UID>`    | 以指定用户身份运行（`--start`）或查找（`--stop`）进程。            |
| `--group <组/GID>`     | 以指定组身份运行（`--start`）进程。                                |
| `--chuid <用户/UID>`   | 启动后将进程的用户 ID 更改为指定用户。                             |
| `--chgrp <组/GID>`     | 启动后将进程的组 ID 更改为指定组。                                 |
| `--background`   | 将守护进程从控制终端分离，使其在后台运行。                         |
| `--signal <信号>`      | 发送指定的信号来停止进程（如 `TERM` 或 `KILL`）。默认为 `TERM`。 |
| `--retry <秒数>` | 在发送信号后等待指定秒数，检查进程是否已停止。可重复发送信号。     |
| `--oknodo`       | 在停止操作时，如果进程未运行，则返回成功状态码（0），不报错。      |
| `--no-close`     | 不关闭标准输入/输出/错误文件描述符。通常用于调试。                 |
| `--make-pidfile` | 创建 PID 文件（如果 `--pidfile` 选项已指定）。                    |

## 10 个典型的应用示例

1.  **启动一个简单的 Python Web 服务器并后台运行**
    ```bash
    start-stop-daemon --start --pidfile /var/run/mywebapp.pid --chuid www-data --exec /usr/bin/python3 -- /var/www/mywebapp/app.py --port 80 --background
    ```

2.  **停止一个指定 PID 文件的服务**
    ```bash
    start-stop-daemon --stop --pidfile /var/run/mywebapp.pid --signal TERM --retry 5
    ```

3.  **重启一个服务**
    ```bash
    start-stop-daemon --stop --pidfile /var/run/mywebapp.pid --signal TERM --retry 5 --start --pidfile /var/run/mywebapp.pid --chuid www-data --exec /usr/bin/python3 -- /var/www/mywebapp/app.py --port 80 --background
    ```

4.  **检查一个服务是否正在运行**
    ```bash
    start-stop-daemon --status --pidfile /var/run/mywebapp.pid --name app.py
    ```

5.  **以特定用户启动一个 NodeJS 应用**
    ```bash
    start-stop-daemon --start --pidfile /var/run/mynodeapp.pid --chuid nodeuser --exec /usr/bin/node -- /opt/mynodeapp/server.js --background
    ```

6.  **强制停止一个无响应的服务**
    ```bash
    start-stop-daemon --stop --pidfile /var/run/stuckapp.pid --signal KILL
    ```

7.  **启动一个服务，如果不存在 PID 文件则创建**
    ```bash
    start-stop-daemon --start --pidfile /var/run/anothersvc.pid --exec /usr/local/bin/anothersvc --make-pidfile --background
    ```

8.  **停止一个由指定用户运行的服务（不使用 PID 文件）**
    ```bash
    start-stop-daemon --stop --user someuser --name myservice --signal TERM
    ```

9.  **启动一个 Java 应用程序（通常使用 `java -jar`）**
    ```bash
    start-stop-daemon --start --pidfile /var/run/javaapp.pid --chuid javauser --exec /usr/bin/java -- -jar /opt/javaapp/myapp.jar --background
    ```

10. **启动一个服务，并指定日志输出到文件**
    ```bash
    start-stop-daemon --start --pidfile /var/run/logsvc.pid --exec /usr/local/bin/logsvc --background --output /var/log/logsvc.log --errorlog /var/log/logsvc_error.log
    ```

这些例子覆盖了 `start-stop-daemon` 的核心功能，展示了如何在不同的场景下有效地管理 Linux 守护进程。

# `start-stop-daemon` init.d 脚本模板

这是一个使用 `start-stop-daemon` 命令来创建标准的 Linux `init.d` 启动脚本的模板。这个模板包含了 `start`, `stop`, `restart`, `status` 和 `force-reload` 等常用动作，方便你以后参考实现自己的服务脚本。

**文件路径示例：** `/etc/init.d/your_service_name`

**使用方法：**

1.  将以下内容保存为 `/etc/init.d/your_service_name`。
2.  修改 `DAEMON`, `DAEMON_ARGS`, `PIDFILE`, `DESC`, `NAME`, `USER` 等变量以适应你的服务。
3.  给予脚本执行权限：`sudo chmod +x /etc/init.d/your_service_name`。
4.  将其添加到系统启动项：`sudo update-rc.d your_service_name defaults`。

```bash
#!/bin/sh

### BEGIN INIT INFO
# Provides:          your_service_name
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start/stop your_service_name daemon
### END INIT INFO

# Change the next 3 lines to suit your needs
DAEMON=/usr/local/bin/your_daemon_executable # 你的服务可执行文件路径
DAEMON_ARGS="--config /etc/your_service/config.conf" # 你的服务启动参数
PIDFILE=/var/run/your_service_name.pid      # 你的服务 PID 文件路径

# Script variables
DESC="Your Service Description"  # 服务描述
NAME="your_service_name"         # 服务名称
USER="youruser"                  # 运行服务的用户
GROUP="yourgroup"                # 运行服务的用户组 (可选，如果和用户同名可省略)

# Load the LSB function library.
. /lib/lsb/init-functions

do_start() {
    log_daemon_msg "Starting $DESC" "$NAME"
    start-stop-daemon --start --quiet --pidfile $PIDFILE --chuid $USER:$GROUP --exec $DAEMON --test > /dev/null \
        || return 1
    start-stop-daemon --start --quiet --pidfile $PIDFILE --chuid $USER:$GROUP --exec $DAEMON --background --make-pidfile -- $DAEMON_ARGS \
        || return 2
    log_end_msg 0
}

do_stop() {
    log_daemon_msg "Stopping $DESC" "$NAME"
    start-stop-daemon --stop --quiet --pidfile $PIDFILE --retry 30 \
        || return 1
    rm -f $PIDFILE
    log_end_msg 0
}

do_reload() {
    log_daemon_msg "Reloading $DESC configuration" "$NAME"
    start-stop-daemon --stop --signal HUP --pidfile $PIDFILE \
        || return 2
    log_end_msg 0
}

case "$1" in
    start)
        do_start
        ;;
    stop)
        do_stop
        ;;
    restart|force-reload)
        do_stop
        do_start
        ;;
    status)
        start-stop-daemon --status --pidfile $PIDFILE --name $NAME
        status_of_proc -p $PIDFILE "$DAEMON" "$NAME" && exit 0 || exit $?
        ;;
    *)
        echo "Usage: /etc/init.d/$NAME {start|stop|restart|force-reload|status}"
        exit 1
        ;;
esac

exit 0
```

这个模板提供了一个完整且健壮的 `init.d` 脚本结构，可以帮助你在 Debian/Ubuntu 系统上规范化地管理自定义服务。