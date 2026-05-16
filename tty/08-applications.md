# SSH/Docker/Terminal 应用详解

## 🎯 学习目标
综合运用前面学到的TTY知识，深入理解SSH、Docker、Terminal Emulator等实际应用中的TTY使用，掌握PTY在远程连接和容器化中的关键作用。

---

## 📊 TTY在不同应用中的架构

```
应用场景架构对比:
┌─────────────────────────────────────────────────────────────────────────────┐
│                           本地终端场景                                       │
│                                                                             │
│  ┌─────────────┐    键盘/显示    ┌─────────────┐    PTY     ┌─────────────┐  │
│  │   用户交互   │◀─────────────▶│  Terminal   │◀─────────▶│    Shell    │  │
│  │            │                │  Emulator   │            │   (bash)    │  │
│  │  键盘输入   │                │  (xterm)    │            │             │  │
│  │  屏幕显示   │                └─────────────┘            └─────────────┘  │
│  └─────────────┘                      │                         │           │
│                                       │ /dev/ptmx              │           │
│                                       ▼                         ▼           │
│                                ┌─────────────────────────────────────────┐  │
│                                │            Kernel                      │  │
│                                │     PTY Driver + N_TTY                 │  │
│                                └─────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                            SSH远程场景                                       │
│                                                                             │
│  Client Machine:                          Server Machine:                  │
│  ┌─────────────┐    ┌─────────────┐      ┌─────────────┐    ┌─────────────┐ │
│  │  Terminal   │◀──▶│ SSH Client  │◀────▶│  SSH Server │◀──▶│    Shell    │ │
│  │  Emulator   │    │   (ssh)     │ Network │  (sshd)   │    │   (bash)    │ │
│  └─────────────┘    └─────────────┘      └─────────────┘    └─────────────┘ │
│        │                   │                    │                   │       │
│        │ 本地PTY            │ TCP连接           │ 服务器PTY          │       │
│        ▼                   ▼                    ▼                   ▼       │
│  ┌─────────────┐    ┌─────────────────────────────────┐    ┌─────────────┐ │
│  │ Local PTY   │    │      Network Stack              │    │ Remote PTY  │ │
│  │Master/Slave │    │   TCP/IP + SSH Protocol        │    │Master/Slave │ │
│  └─────────────┘    └─────────────────────────────────┘    └─────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                           Docker容器场景                                     │
│                                                                             │
│  Host:                                     Container:                       │
│  ┌─────────────┐    ┌─────────────────────────────────┐    ┌─────────────┐ │
│  │  Terminal   │◀──▶│        Docker Engine           │◀──▶│    Shell    │ │
│  │  (docker    │    │                                 │    │   (bash)    │ │
│  │   attach)   │    │  ┌─────────────┐ ┌─────────────┐│    │             │ │
│  └─────────────┘    │  │ containerd  │ │  runc       ││    └─────────────┘ │
│                     │  └─────────────┘ └─────────────┘│           │         │
│                     └─────────────────────────────────┘           │         │
│        │                         │                               │         │
│        │ 宿主机PTY                │ Container PTY                 │         │
│        ▼                         ▼                               ▼         │
│  ┌─────────────┐         ┌─────────────────────────────────────────────┐   │
│  │ Host PTY    │◀───────▶│          Container Namespace                │   │
│  │Master/Slave │         │        PTY + Process + Network              │   │
│  └─────────────┘         └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                      IDE集成终端场景                                         │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                          IDE (VSCode)                              │   │
│  │                                                                     │   │
│  │  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────────┐  │   │
│  │  │   Editor    │    │  Terminal   │    │      Extensions        │  │   │
│  │  │   Pane      │    │    Panel    │    │   (Language Servers)   │  │   │
│  │  └─────────────┘    └─────────────┘    └─────────────────────────┘  │   │
│  │                           │                                         │   │
│  │                           ▼                                         │   │
│  │                  ┌─────────────────┐                                │   │
│  │                  │  PTY Process    │                                │   │
│  │                  │  (node-pty)     │                                │   │
│  │                  └─────────────────┘                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                             │                                               │
│                             ▼                                               │
│                    ┌─────────────────┐                                      │
│                    │   Shell Process │                                      │
│                    │     (bash)      │                                      │
│                    └─────────────────┘                                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 🏗️ SSH中的PTY实现

### SSH客户端PTY分配

```c
// SSH客户端中PTY的创建和管理 (简化版本)

struct ssh_client {
    int sock_fd;                    // 网络连接
    int pty_master;                 // 本地PTY master
    char *pty_slave_name;           // PTY slave设备名
    pid_t shell_pid;                // 本地shell进程ID
    bool pty_allocated;             // PTY是否已分配
};

// SSH客户端请求服务器分配PTY
int ssh_client_request_pty(struct ssh_client *client, 
                          struct winsize *ws) {
    SSH_Packet packet;
    
    /* 构建PTY请求包 */
    ssh_packet_init(&packet, SSH_MSG_CHANNEL_REQUEST);
    ssh_packet_put_string(&packet, "pty-req");
    ssh_packet_put_bool(&packet, 1);  /* want_reply */
    
    /* 终端类型 */
    char *term = getenv("TERM");
    ssh_packet_put_string(&packet, term ? term : "xterm");
    
    /* 终端尺寸 */
    ssh_packet_put_uint32(&packet, ws->ws_col);   /* 列数 */
    ssh_packet_put_uint32(&packet, ws->ws_row);   /* 行数 */
    ssh_packet_put_uint32(&packet, ws->ws_xpixel); /* X像素 */
    ssh_packet_put_uint32(&packet, ws->ws_ypixel); /* Y像素 */
    
    /* 终端模式 (termios设置) */
    struct termios tios;
    tcgetattr(STDIN_FILENO, &tios);
    ssh_packet_put_termios(&packet, &tios);
    
    /* 发送请求 */
    return ssh_send_packet(client->sock_fd, &packet);
}

// 本地PTY创建和shell启动
int ssh_client_setup_local_pty(struct ssh_client *client) {
    /* 创建PTY对 */
    client->pty_master = posix_openpt(O_RDWR);
    if (client->pty_master == -1) {
        perror("posix_openpt");
        return -1;
    }
    
    if (grantpt(client->pty_master) == -1 ||
        unlockpt(client->pty_master) == -1) {
        close(client->pty_master);
        return -1;
    }
    
    client->pty_slave_name = strdup(ptsname(client->pty_master));
    
    /* Fork子进程运行本地shell */
    client->shell_pid = fork();
    if (client->shell_pid == 0) {
        /* 子进程：设置PTY slave并执行shell */
        int slave_fd = open(client->pty_slave_name, O_RDWR);
        
        /* 成为会话leader */
        setsid();
        
        /* 设置控制终端 */
        ioctl(slave_fd, TIOCSCTTY, 1);
        
        /* 重定向标准输入输出 */
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);
        
        /* 执行本地shell */
        execl("/bin/bash", "bash", NULL);
        exit(1);
    } else if (client->shell_pid > 0) {
        client->pty_allocated = true;
        return 0;
    } else {
        perror("fork");
        close(client->pty_master);
        return -1;
    }
}

// SSH客户端数据转发循环
int ssh_client_data_loop(struct ssh_client *client) {
    fd_set readfds;
    char buffer[8192];
    ssize_t n;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(client->sock_fd, &readfds);        /* SSH连接 */
        FD_SET(client->pty_master, &readfds);     /* 本地PTY */
        
        int maxfd = (client->sock_fd > client->pty_master) ? 
                    client->sock_fd : client->pty_master;
        
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }
        
        /* 从SSH连接读取数据，转发到本地PTY */
        if (FD_ISSET(client->sock_fd, &readfds)) {
            n = ssh_read_channel_data(client->sock_fd, buffer, sizeof(buffer));
            if (n > 0) {
                write(client->pty_master, buffer, n);
            } else if (n == 0) {
                printf("SSH连接关闭\n");
                break;
            }
        }
        
        /* 从本地PTY读取数据，转发到SSH连接 */
        if (FD_ISSET(client->pty_master, &readfds)) {
            n = read(client->pty_master, buffer, sizeof(buffer));
            if (n > 0) {
                ssh_send_channel_data(client->sock_fd, buffer, n);
            } else if (n == 0) {
                printf("本地shell退出\n");
                break;
            }
        }
    }
    
    return 0;
}
```

### SSH服务器PTY处理

```c
// SSH服务器端PTY处理 (简化版本)

struct ssh_session {
    int client_fd;                  // 客户端连接
    int pty_master;                 // PTY master
    char *pty_slave_name;           // PTY slave设备名
    pid_t shell_pid;                // shell进程ID
    struct winsize term_size;       // 终端尺寸
    bool pty_requested;             // 客户端是否请求PTY
};

// 处理客户端PTY请求
int sshd_handle_pty_request(struct ssh_session *session, SSH_Packet *packet) {
    char *term_type;
    uint32_t cols, rows, xpixel, ypixel;
    
    /* 解析PTY请求包 */
    term_type = ssh_packet_get_string(packet);
    cols = ssh_packet_get_uint32(packet);
    rows = ssh_packet_get_uint32(packet);
    xpixel = ssh_packet_get_uint32(packet);
    ypixel = ssh_packet_get_uint32(packet);
    
    /* 设置环境变量 */
    setenv("TERM", term_type, 1);
    
    /* 记录终端尺寸 */
    session->term_size.ws_col = cols;
    session->term_size.ws_row = rows;
    session->term_size.ws_xpixel = xpixel;
    session->term_size.ws_ypixel = ypixel;
    
    /* 创建PTY */
    session->pty_master = posix_openpt(O_RDWR);
    if (session->pty_master == -1) {
        ssh_send_channel_failure(session->client_fd);
        return -1;
    }
    
    if (grantpt(session->pty_master) == -1 ||
        unlockpt(session->pty_master) == -1) {
        close(session->pty_master);
        ssh_send_channel_failure(session->client_fd);
        return -1;
    }
    
    session->pty_slave_name = strdup(ptsname(session->pty_master));
    session->pty_requested = true;
    
    /* 发送成功响应 */
    ssh_send_channel_success(session->client_fd);
    
    free(term_type);
    return 0;
}

// 启动shell会话
int sshd_start_shell(struct ssh_session *session, const char *command) {
    int slave_fd;
    
    session->shell_pid = fork();
    if (session->shell_pid == 0) {
        /* 子进程：设置PTY并执行shell */
        
        /* 打开PTY slave */
        slave_fd = open(session->pty_slave_name, O_RDWR);
        if (slave_fd == -1) {
            perror("open pty slave");
            exit(1);
        }
        
        /* 创建新会话 */
        if (setsid() == -1) {
            perror("setsid");
            exit(1);
        }
        
        /* 设置控制终端 */
        if (ioctl(slave_fd, TIOCSCTTY, 1) == -1) {
            perror("TIOCSCTTY");
            exit(1);
        }
        
        /* 设置终端尺寸 */
        ioctl(slave_fd, TIOCSWINSZ, &session->term_size);
        
        /* 重定向标准I/O */
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > STDERR_FILENO)
            close(slave_fd);
            
        /* 设置环境变量 */
        setup_shell_environment(session);
        
        /* 执行命令或shell */
        if (command && *command) {
            execl("/bin/sh", "sh", "-c", command, NULL);
        } else {
            char *shell = getenv("SHELL");
            if (!shell) shell = "/bin/bash";
            execl(shell, shell, NULL);
        }
        
        exit(1);
    } else if (session->shell_pid > 0) {
        /* 父进程：关闭不需要的文件描述符 */
        return 0;
    } else {
        perror("fork");
        return -1;
    }
}

// SSH服务器数据转发
int sshd_session_loop(struct ssh_session *session) {
    fd_set readfds;
    char buffer[8192];
    ssize_t n;
    int status;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(session->client_fd, &readfds);     /* SSH客户端 */
        FD_SET(session->pty_master, &readfds);    /* PTY master */
        
        int maxfd = (session->client_fd > session->pty_master) ? 
                    session->client_fd : session->pty_master;
        
        /* 非阻塞检查子进程状态 */
        if (waitpid(session->shell_pid, &status, WNOHANG) > 0) {
            printf("Shell进程退出，状态: %d\n", status);
            break;
        }
        
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }
        
        /* 从客户端读取数据，转发到PTY */
        if (FD_ISSET(session->client_fd, &readfds)) {
            n = ssh_read_channel_data(session->client_fd, buffer, sizeof(buffer));
            if (n > 0) {
                write(session->pty_master, buffer, n);
            } else if (n == 0) {
                printf("客户端连接关闭\n");
                break;
            }
        }
        
        /* 从PTY读取数据，转发到客户端 */
        if (FD_ISSET(session->pty_master, &readfds)) {
            n = read(session->pty_master, buffer, sizeof(buffer));
            if (n > 0) {
                ssh_send_channel_data(session->client_fd, buffer, n);
            } else if (n == 0) {
                printf("PTY关闭\n");
                break;
            }
        }
    }
    
    return 0;
}
```

## 🐳 Docker中的TTY实现

### Docker容器TTY分配

```c
// Docker容器中TTY的实现机制

struct container_config {
    bool allocate_tty;              // -t参数
    bool interactive;               // -i参数  
    char *entrypoint;               // 入口命令
    char **cmd;                     // 命令参数
    struct winsize term_size;       // 终端尺寸
};

struct container_runtime {
    int container_fd;               // 容器进程文件描述符
    int pty_master;                 // PTY master (宿主机)
    pid_t container_pid;            // 容器主进程PID
    int stdin_fd, stdout_fd, stderr_fd; // 标准I/O
};

// Docker容器创建时的TTY设置
int docker_create_container_with_tty(struct container_config *config,
                                    struct container_runtime *runtime) {
    int pty_master = -1, pty_slave = -1;
    
    if (config->allocate_tty) {
        /* 创建PTY对 */
        pty_master = posix_openpt(O_RDWR | O_NOCTTY);
        if (pty_master == -1) {
            perror("posix_openpt");
            return -1;
        }
        
        if (grantpt(pty_master) == -1 || unlockpt(pty_master) == -1) {
            close(pty_master);
            return -1;
        }
        
        char *slave_name = ptsname(pty_master);
        pty_slave = open(slave_name, O_RDWR | O_NOCTTY);
        if (pty_slave == -1) {
            close(pty_master);
            return -1;
        }
        
        runtime->pty_master = pty_master;
    }
    
    /* 创建容器进程 */
    runtime->container_pid = fork();
    if (runtime->container_pid == 0) {
        /* 容器进程 */
        
        /* 进入容器命名空间 */
        if (unshare(CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | 
                   CLONE_NEWUTS | CLONE_NEWIPC) == -1) {
            perror("unshare");
            exit(1);
        }
        
        if (config->allocate_tty) {
            /* 设置PTY slave */
            if (setsid() == -1) {
                perror("setsid");
                exit(1);
            }
            
            /* 设置控制终端 */
            if (ioctl(pty_slave, TIOCSCTTY, 1) == -1) {
                perror("TIOCSCTTY");
                exit(1);
            }
            
            /* 设置终端尺寸 */
            ioctl(pty_slave, TIOCSWINSZ, &config->term_size);
            
            /* 重定向标准I/O到PTY */
            dup2(pty_slave, STDIN_FILENO);
            dup2(pty_slave, STDOUT_FILENO);
            dup2(pty_slave, STDERR_FILENO);
            close(pty_slave);
            close(pty_master);
        }
        
        /* 设置容器环境 */
        setup_container_environment(config);
        
        /* 执行容器命令 */
        if (config->cmd && config->cmd[0]) {
            execvp(config->cmd[0], config->cmd);
        } else {
            execl("/bin/bash", "bash", NULL);
        }
        
        exit(1);
    } else if (runtime->container_pid > 0) {
        /* 宿主机进程 */
        if (config->allocate_tty) {
            close(pty_slave);  /* 关闭slave，只保留master */
        }
        return 0;
    } else {
        perror("fork");
        if (pty_master != -1) close(pty_master);
        if (pty_slave != -1) close(pty_slave);
        return -1;
    }
}

// Docker attach实现
int docker_attach_container(struct container_runtime *runtime) {
    fd_set readfds;
    char buffer[8192];
    ssize_t n;
    
    /* 设置本地终端为原始模式 */
    struct termios orig_termios, raw_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    
    raw_termios = orig_termios;
    cfmakeraw(&raw_termios);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
    
    printf("连接到容器...（按 Ctrl+P, Ctrl+Q 断开连接）\n");
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);           /* 用户输入 */
        FD_SET(runtime->pty_master, &readfds);    /* 容器输出 */
        
        int maxfd = (STDIN_FILENO > runtime->pty_master) ? 
                    STDIN_FILENO : runtime->pty_master;
        
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        
        /* 用户输入 -> 容器 */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            n = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (n > 0) {
                /* 检查断开连接序列 (Ctrl+P, Ctrl+Q) */
                if (n >= 2 && buffer[0] == 0x10 && buffer[1] == 0x11) {
                    printf("\n断开连接序列检测到，退出attach\n");
                    break;
                }
                write(runtime->pty_master, buffer, n);
            } else if (n == 0) {
                break;
            }
        }
        
        /* 容器输出 -> 用户 */
        if (FD_ISSET(runtime->pty_master, &readfds)) {
            n = read(runtime->pty_master, buffer, sizeof(buffer));
            if (n > 0) {
                write(STDOUT_FILENO, buffer, n);
            } else if (n == 0) {
                printf("\n容器已退出\n");
                break;
            }
        }
    }
    
    /* 恢复终端设置 */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    
    return 0;
}
```

### Docker exec实现

```c
// docker exec实现
int docker_exec_command(const char *container_id, 
                       const char *command,
                       bool allocate_tty, 
                       bool interactive) {
    pid_t container_pid;
    int pty_master = -1;
    
    /* 查找容器主进程PID */
    container_pid = find_container_pid(container_id);
    if (container_pid <= 0) {
        fprintf(stderr, "容器未找到或未运行: %s\n", container_id);
        return -1;
    }
    
    /* 如果需要TTY，创建PTY */
    if (allocate_tty) {
        pty_master = posix_openpt(O_RDWR | O_NOCTTY);
        if (pty_master == -1) {
            perror("posix_openpt");
            return -1;
        }
        
        if (grantpt(pty_master) == -1 || unlockpt(pty_master) == -1) {
            close(pty_master);
            return -1;
        }
    }
    
    pid_t exec_pid = fork();
    if (exec_pid == 0) {
        /* 子进程：进入容器命名空间 */
        char ns_path[256];
        
        /* 进入容器的各个命名空间 */
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/mnt", container_pid);
        int mnt_fd = open(ns_path, O_RDONLY);
        setns(mnt_fd, CLONE_NEWNS);
        close(mnt_fd);
        
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/pid", container_pid);
        int pid_fd = open(ns_path, O_RDONLY);
        setns(pid_fd, CLONE_NEWPID);  
        close(pid_fd);
        
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/net", container_pid);
        int net_fd = open(ns_path, O_RDONLY);
        setns(net_fd, CLONE_NEWNET);
        close(net_fd);
        
        /* 其他命名空间... */
        
        if (allocate_tty) {
            /* 设置PTY slave */
            char *slave_name = ptsname(pty_master);
            int slave_fd = open(slave_name, O_RDWR);
            
            setsid();
            ioctl(slave_fd, TIOCSCTTY, 1);
            
            dup2(slave_fd, STDIN_FILENO);
            dup2(slave_fd, STDOUT_FILENO);
            dup2(slave_fd, STDERR_FILENO);
            close(slave_fd);
            close(pty_master);
        }
        
        /* 执行命令 */
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(1);
    } else if (exec_pid > 0) {
        /* 父进程：处理I/O */
        if (allocate_tty && interactive) {
            /* 设置为交互模式 */
            struct termios orig_termios;
            tcgetattr(STDIN_FILENO, &orig_termios);
            
            struct termios raw_termios = orig_termios;
            cfmakeraw(&raw_termios);
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
            
            /* I/O循环 */
            fd_set readfds;
            char buffer[4096];
            
            while (1) {
                FD_ZERO(&readfds);
                FD_SET(STDIN_FILENO, &readfds);
                FD_SET(pty_master, &readfds);
                
                int maxfd = (STDIN_FILENO > pty_master) ? STDIN_FILENO : pty_master;
                
                if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0)
                    break;
                    
                if (FD_ISSET(STDIN_FILENO, &readfds)) {
                    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
                    if (n > 0) {
                        write(pty_master, buffer, n);
                    } else break;
                }
                
                if (FD_ISSET(pty_master, &readfds)) {
                    ssize_t n = read(pty_master, buffer, sizeof(buffer));
                    if (n > 0) {
                        write(STDOUT_FILENO, buffer, n);
                    } else break;
                }
            }
            
            /* 恢复终端设置 */
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
        
        /* 等待命令完成 */
        int status;
        waitpid(exec_pid, &status, 0);
        
        if (pty_master != -1)
            close(pty_master);
            
        return WEXITSTATUS(status);
    } else {
        perror("fork");
        if (pty_master != -1)
            close(pty_master);
        return -1;
    }
}
```

## 💻 Terminal Emulator实现

### 基于PTY的终端模拟器

```c
// 简化的终端模拟器实现

struct terminal_emulator {
    /* PTY相关 */
    int pty_master;                 // PTY master文件描述符
    char *pty_slave_name;           // PTY slave设备名
    pid_t shell_pid;                // shell进程PID
    
    /* 终端状态 */
    int rows, cols;                 // 终端尺寸
    char **screen;                  // 屏幕缓冲区
    int cursor_x, cursor_y;         // 光标位置
    
    /* 终端属性 */
    struct termios termios;         // 终端属性
    bool echo_mode;                 // 回显模式
    bool cursor_visible;            // 光标可见性
    
    /* GUI相关 */
    void *window;                   // 窗口句柄
    void *font;                     // 字体
    int char_width, char_height;    // 字符尺寸
};

// 初始化终端模拟器
int terminal_init(struct terminal_emulator *term, int rows, int cols) {
    term->rows = rows;
    term->cols = cols;
    term->cursor_x = term->cursor_y = 0;
    term->echo_mode = true;
    term->cursor_visible = true;
    
    /* 分配屏幕缓冲区 */
    term->screen = calloc(rows, sizeof(char*));
    for (int i = 0; i < rows; i++) {
        term->screen[i] = calloc(cols + 1, sizeof(char));
        memset(term->screen[i], ' ', cols);
    }
    
    /* 创建PTY */
    term->pty_master = posix_openpt(O_RDWR);
    if (term->pty_master == -1) {
        perror("posix_openpt");
        return -1;
    }
    
    if (grantpt(term->pty_master) == -1 || unlockpt(term->pty_master) == -1) {
        close(term->pty_master);
        return -1;
    }
    
    term->pty_slave_name = strdup(ptsname(term->pty_master));
    
    /* 设置PTY的termios */
    struct termios tios;
    tcgetattr(term->pty_master, &tios);
    
    /* 设置基本的终端属性 */
    tios.c_lflag |= ECHO | ICANON | ISIG;
    tios.c_iflag |= ICRNL;
    tios.c_oflag |= ONLCR;
    
    tcsetattr(term->pty_master, TCSAFLUSH, &tios);
    term->termios = tios;
    
    /* 设置窗口大小 */
    struct winsize ws;
    ws.ws_row = rows;
    ws.ws_col = cols;
    ws.ws_xpixel = cols * term->char_width;
    ws.ws_ypixel = rows * term->char_height;
    ioctl(term->pty_master, TIOCSWINSZ, &ws);
    
    return 0;
}

// 启动shell进程
int terminal_start_shell(struct terminal_emulator *term) {
    term->shell_pid = fork();
    
    if (term->shell_pid == 0) {
        /* 子进程：运行shell */
        int slave_fd = open(term->pty_slave_name, O_RDWR);
        
        /* 创建新会话 */
        setsid();
        
        /* 设置控制终端 */
        ioctl(slave_fd, TIOCSCTTY, 1);
        
        /* 重定向标准I/O */
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);
        close(term->pty_master);
        
        /* 设置环境变量 */
        setenv("TERM", "xterm-256color", 1);
        
        /* 执行shell */
        char *shell = getenv("SHELL");
        if (!shell) shell = "/bin/bash";
        execl(shell, shell, NULL);
        
        exit(1);
    } else if (term->shell_pid < 0) {
        perror("fork");
        return -1;
    }
    
    return 0;
}

// ANSI转义序列处理
void terminal_process_escape_sequence(struct terminal_emulator *term, 
                                     const char *seq, size_t len) {
    if (len < 2 || seq[0] != '\033')
        return;
        
    switch (seq[1]) {
    case '[':  /* CSI序列 */
        if (len >= 3) {
            switch (seq[len-1]) {  /* 最后一个字符是命令 */
            case 'A':  /* 光标上移 */
                if (term->cursor_y > 0) term->cursor_y--;
                break;
            case 'B':  /* 光标下移 */
                if (term->cursor_y < term->rows - 1) term->cursor_y++;
                break;
            case 'C':  /* 光标右移 */
                if (term->cursor_x < term->cols - 1) term->cursor_x++;
                break;
            case 'D':  /* 光标左移 */
                if (term->cursor_x > 0) term->cursor_x--;
                break;
            case 'H':  /* 光标归位 */
                term->cursor_x = term->cursor_y = 0;
                break;
            case 'J':  /* 清屏 */
                terminal_clear_screen(term);
                break;
            case 'K':  /* 清行 */
                memset(term->screen[term->cursor_y] + term->cursor_x, ' ',
                       term->cols - term->cursor_x);
                break;
            case 'm':  /* 设置显示属性 */
                /* 颜色和样式处理... */
                break;
            }
        }
        break;
        
    case 'c':  /* 重置终端 */
        terminal_reset(term);
        break;
    }
}

// 处理来自shell的输出
void terminal_process_output(struct terminal_emulator *term, 
                           const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = data[i];
        
        if (c == '\033') {  /* 转义序列开始 */
            /* 查找转义序列结束 */
            size_t seq_len = 1;
            while (i + seq_len < len) {
                unsigned char next = data[i + seq_len];
                seq_len++;
                
                /* 转义序列结束条件 */
                if ((next >= 'A' && next <= 'Z') || 
                    (next >= 'a' && next <= 'z')) {
                    break;
                }
            }
            
            terminal_process_escape_sequence(term, data + i, seq_len);
            i += seq_len - 1;
            
        } else if (c == '\n') {  /* 换行 */
            term->cursor_x = 0;
            if (term->cursor_y < term->rows - 1) {
                term->cursor_y++;
            } else {
                /* 滚屏 */
                terminal_scroll_up(term);
            }
            
        } else if (c == '\r') {  /* 回车 */
            term->cursor_x = 0;
            
        } else if (c == '\t') {  /* 制表符 */
            int next_tab = ((term->cursor_x / 8) + 1) * 8;
            if (next_tab < term->cols) {
                term->cursor_x = next_tab;
            }
            
        } else if (c == '\b') {  /* 退格 */
            if (term->cursor_x > 0) {
                term->cursor_x--;
                term->screen[term->cursor_y][term->cursor_x] = ' ';
            }
            
        } else if (c >= 32 && c < 127) {  /* 可打印字符 */
            if (term->cursor_x < term->cols) {
                term->screen[term->cursor_y][term->cursor_x] = c;
                term->cursor_x++;
                
                /* 自动换行 */
                if (term->cursor_x >= term->cols) {
                    term->cursor_x = 0;
                    if (term->cursor_y < term->rows - 1) {
                        term->cursor_y++;
                    } else {
                        terminal_scroll_up(term);
                    }
                }
            }
        }
    }
    
    /* 重绘屏幕 */
    terminal_redraw(term);
}

// 主事件循环
int terminal_main_loop(struct terminal_emulator *term) {
    fd_set readfds;
    char buffer[4096];
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(term->pty_master, &readfds);
        
        /* 这里应该还要处理GUI事件，简化处理 */
        
        struct timeval timeout = {0, 100000};  /* 100ms超时 */
        
        int result = select(term->pty_master + 1, &readfds, NULL, NULL, &timeout);
        
        if (result > 0 && FD_ISSET(term->pty_master, &readfds)) {
            /* 从shell读取输出 */
            ssize_t n = read(term->pty_master, buffer, sizeof(buffer));
            if (n > 0) {
                terminal_process_output(term, buffer, n);
            } else if (n == 0) {
                printf("Shell进程退出\n");
                break;
            }
        }
        
        /* 处理GUI事件（键盘输入、鼠标、窗口调整等） */
        /* 这部分依赖于具体的GUI框架 */
        handle_gui_events(term);
        
        /* 检查子进程状态 */
        int status;
        if (waitpid(term->shell_pid, &status, WNOHANG) > 0) {
            printf("Shell进程结束，状态: %d\n", status);
            break;
        }
    }
    
    return 0;
}

// 处理键盘输入
void terminal_handle_key_input(struct terminal_emulator *term, 
                              int key, int modifiers) {
    char seq[10];
    size_t len = 0;
    
    /* 处理特殊键 */
    switch (key) {
    case KEY_UP:
        strcpy(seq, "\033[A");
        len = 3;
        break;
    case KEY_DOWN:
        strcpy(seq, "\033[B"); 
        len = 3;
        break;
    case KEY_RIGHT:
        strcpy(seq, "\033[C");
        len = 3;
        break;
    case KEY_LEFT:
        strcpy(seq, "\033[D");
        len = 3;
        break;
    case KEY_HOME:
        strcpy(seq, "\033[H");
        len = 3;
        break;
    case KEY_END:
        strcpy(seq, "\033[F");
        len = 3;
        break;
    default:
        if (key >= 32 && key < 127) {  /* 普通字符 */
            seq[0] = key;
            len = 1;
        } else if (key < 32) {  /* 控制字符 */
            seq[0] = key;
            len = 1;
        }
        break;
    }
    
    /* 发送到shell */
    if (len > 0) {
        write(term->pty_master, seq, len);
    }
}
```

## 🧪 综合实验

### 实验1：SSH会话跟踪

```c
// ssh_session_tracer.c - 跟踪SSH会话的PTY使用
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>

struct ssh_tracer {
    FILE *log_file;
    pid_t ssh_pid;
    time_t start_time;
};

void trace_process_info(struct ssh_tracer *tracer, pid_t pid, const char *stage) {
    char cmd[512];
    FILE *fp;
    
    fprintf(tracer->log_file, "\n=== %s (PID %d) ===\n", stage, pid);
    
    /* 获取进程信息 */
    snprintf(cmd, sizeof(cmd), "ps -o pid,ppid,pgid,sid,tty,stat,cmd -p %d", pid);
    fp = popen(cmd, "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            fprintf(tracer->log_file, "%s", line);
        }
        pclose(fp);
    }
    
    /* 获取文件描述符信息 */
    snprintf(cmd, sizeof(cmd), "ls -l /proc/%d/fd/", pid);
    fp = popen(cmd, "r");
    if (fp) {
        fprintf(tracer->log_file, "\n文件描述符:\n");
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "pts") || strstr(line, "tty")) {
                fprintf(tracer->log_file, "%s", line);
            }
        }
        pclose(fp);
    }
    
    /* 获取终端信息 */
    snprintf(cmd, sizeof(cmd), "cat /proc/%d/stat 2>/dev/null | cut -d' ' -f7", pid);
    fp = popen(cmd, "r");
    if (fp) {
        int tty_nr;
        if (fscanf(fp, "%d", &tty_nr) == 1) {
            fprintf(tracer->log_file, "TTY编号: %d\n", tty_nr);
        }
        pclose(fp);
    }
    
    fprintf(tracer->log_file, "时间戳: %ld\n", time(NULL) - tracer->start_time);
    fflush(tracer->log_file);
}

void signal_handler(int sig) {
    printf("\n接收到信号 %d，正在清理...\n", sig);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("用法: %s <ssh命令和参数>\n", argv[0]);
        printf("例如: %s ssh user@host\n", argv[0]);
        return 1;
    }
    
    struct ssh_tracer tracer;
    tracer.start_time = time(NULL);
    tracer.log_file = fopen("ssh_trace.log", "w");
    if (!tracer.log_file) {
        perror("fopen");
        return 1;
    }
    
    fprintf(tracer.log_file, "SSH会话跟踪开始\n");
    fprintf(tracer.log_file, "命令: ");
    for (int i = 1; i < argc; i++) {
        fprintf(tracer.log_file, "%s ", argv[i]);
    }
    fprintf(tracer.log_file, "\n");
    
    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* 获取初始状态 */
    trace_process_info(&tracer, getpid(), "跟踪程序启动");
    
    /* 启动SSH */
    tracer.ssh_pid = fork();
    if (tracer.ssh_pid == 0) {
        /* 子进程：执行SSH */
        execvp(argv[1], &argv[1]);
        perror("execvp");
        exit(1);
    } else if (tracer.ssh_pid > 0) {
        /* 父进程：跟踪SSH进程 */
        sleep(1);  /* 让SSH启动 */
        
        trace_process_info(&tracer, tracer.ssh_pid, "SSH启动后");
        
        /* 定期跟踪 */
        int status;
        while (waitpid(tracer.ssh_pid, &status, WNOHANG) == 0) {
            sleep(5);
            trace_process_info(&tracer, tracer.ssh_pid, "运行中");
            
            /* 查找SSH的子进程 */
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "pgrep -P %d", tracer.ssh_pid);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                pid_t child_pid;
                while (fscanf(fp, "%d", &child_pid) == 1) {
                    trace_process_info(&tracer, child_pid, "SSH子进程");
                }
                pclose(fp);
            }
        }
        
        fprintf(tracer.log_file, "\nSSH进程退出，状态: %d\n", WEXITSTATUS(status));
        trace_process_info(&tracer, getpid(), "SSH退出后");
        
    } else {
        perror("fork");
        fclose(tracer.log_file);
        return 1;
    }
    
    fclose(tracer.log_file);
    printf("跟踪完成，日志保存在 ssh_trace.log\n");
    return 0;
}
```

### 实验2：Docker容器TTY分析

```bash
#!/bin/bash
# docker_tty_analysis.sh - Docker容器TTY分析

echo "=== Docker容器TTY分析实验 ==="

# 清理可能存在的容器
docker rm -f tty-test-container 2>/dev/null || true

echo -e "\n1. 启动带TTY的容器:"
docker run -dit --name tty-test-container ubuntu:20.04 /bin/bash
container_id=$(docker ps -qf "name=tty-test-container")

echo "容器ID: $container_id"

# 获取容器进程信息
echo -e "\n2. 容器进程分析:"
container_pid=$(docker inspect --format '{{.State.Pid}}' tty-test-container)
echo "容器主进程PID: $container_pid"

echo "进程层次结构:"
ps -eo pid,ppid,pgid,sid,tty,stat,cmd --forest | grep -E "(PID|$container_pid|docker|containerd)"

echo -e "\n3. 容器内PTY设备:"
docker exec tty-test-container ls -la /dev/pts/
docker exec tty-test-container tty

echo -e "\n4. 容器进程的文件描述符:"
ls -la /proc/$container_pid/fd/ | grep -E "(pts|tty)"

echo -e "\n5. 宿主机PTY设备:"
ls -la /dev/pts/ | grep -v total

echo -e "\n6. 测试不带TTY的容器:"
docker run -d --name no-tty-container ubuntu:20.04 sleep 30
no_tty_pid=$(docker inspect --format '{{.State.Pid}}' no-tty-container)

echo "无TTY容器PID: $no_tty_pid"
echo "无TTY容器的文件描述符:"
ls -la /proc/$no_tty_pid/fd/

echo -e "\n7. 测试docker attach:"
echo "启动attach会话（5秒后自动退出）:"
timeout 5 bash -c 'echo "echo Hello from container; exit" | docker attach tty-test-container' || true

echo -e "\n8. 测试docker exec:"
echo "docker exec交互测试:"
docker exec -it tty-test-container bash -c 'tty; ps -o pid,ppid,pgid,sid,tty,cmd'

echo -e "\n9. TTY相关系统调用跟踪:"
echo "跟踪docker attach的系统调用（10秒）:"
timeout 10 strace -e trace=openat,ioctl,read,write -f docker attach tty-test-container 2>&1 | head -20 || true

echo -e "\n10. 清理:"
docker rm -f tty-test-container no-tty-container

echo -e "\n分析完成！"
```

### 实验3：终端模拟器功能验证

```c
// terminal_test.c - 终端模拟器功能测试
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

struct mini_terminal {
    int pty_master;
    pid_t shell_pid;
    struct termios orig_termios;
    int rows, cols;
};

void restore_terminal(struct mini_terminal *term) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term->orig_termios);
}

void signal_handler(int sig) {
    printf("\n收到信号 %d，正在退出...\n", sig);
    exit(0);
}

int setup_pty(struct mini_terminal *term) {
    /* 创建PTY */
    term->pty_master = posix_openpt(O_RDWR | O_NOCTTY);
    if (term->pty_master == -1) {
        perror("posix_openpt");
        return -1;
    }
    
    if (grantpt(term->pty_master) == -1 || unlockpt(term->pty_master) == -1) {
        close(term->pty_master);
        return -1;
    }
    
    /* 设置PTY大小 */
    struct winsize ws;
    ws.ws_row = term->rows;
    ws.ws_col = term->cols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ioctl(term->pty_master, TIOCSWINSZ, &ws);
    
    /* 启动shell */
    char *slave_name = ptsname(term->pty_master);
    
    term->shell_pid = fork();
    if (term->shell_pid == 0) {
        /* 子进程：shell */
        int slave_fd = open(slave_name, O_RDWR);
        
        setsid();
        ioctl(slave_fd, TIOCSCTTY, 1);
        
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);
        close(term->pty_master);
        
        setenv("TERM", "xterm", 1);
        execl("/bin/bash", "bash", NULL);
        exit(1);
    } else if (term->shell_pid < 0) {
        perror("fork");
        close(term->pty_master);
        return -1;
    }
    
    return 0;
}

void test_ansi_sequences(struct mini_terminal *term) {
    printf("=== ANSI转义序列测试 ===\n");
    
    /* 发送各种ANSI序列到PTY */
    write(term->pty_master, "clear\n", 6);
    sleep(1);
    
    write(term->pty_master, "echo -e '\\033[31m红色文字\\033[0m'\n", 35);
    sleep(1);
    
    write(term->pty_master, "echo -e '\\033[1m粗体\\033[0m'\n", 29);
    sleep(1);
    
    write(term->pty_master, "echo -e '\\033[4m下划线\\033[0m'\n", 31);
    sleep(1);
    
    /* 光标移动 */
    write(term->pty_master, "echo -e '\\033[10;10H光标定位'\n", 32);
    sleep(1);
    
    /* 清屏测试 */
    write(term->pty_master, "echo -e '\\033[2J\\033[H清屏测试'\n", 34);
    sleep(2);
}

void test_window_resize(struct mini_terminal *term) {
    printf("=== 窗口大小调整测试 ===\n");
    
    /* 发送stty size查看当前大小 */
    write(term->pty_master, "stty size\n", 10);
    sleep(1);
    
    /* 改变窗口大小 */
    struct winsize ws;
    ws.ws_row = 30;
    ws.ws_col = 100;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    
    ioctl(term->pty_master, TIOCSWINSZ, &ws);
    term->rows = 30;
    term->cols = 100;
    
    printf("窗口大小已调整为 %dx%d\n", ws.ws_row, ws.ws_col);
    
    /* 再次查看大小 */
    write(term->pty_master, "stty size\n", 10);
    sleep(1);
    
    /* 测试填充整个窗口 */
    write(term->pty_master, "yes | head -n 25 | cat -n\n", 27);
    sleep(2);
}

void test_signal_handling(struct mini_terminal *term) {
    printf("=== 信号处理测试 ===\n");
    
    /* 启动一个长时间运行的命令 */
    write(term->pty_master, "sleep 10 &\n", 11);
    sleep(1);
    
    write(term->pty_master, "jobs\n", 5);
    sleep(1);
    
    /* 发送Ctrl+C到PTY */
    write(term->pty_master, "\003", 1);  /* Ctrl+C */
    sleep(1);
    
    /* 启动另一个进程并暂停它 */
    write(term->pty_master, "cat\n", 4);
    sleep(1);
    
    /* 发送Ctrl+Z */
    write(term->pty_master, "\032", 1);  /* Ctrl+Z */
    sleep(1);
    
    write(term->pty_master, "jobs\n", 5);
    sleep(1);
    
    /* 恢复作业 */
    write(term->pty_master, "fg\n", 3);
    sleep(1);
    
    /* 终止cat */
    write(term->pty_master, "\003", 1);  /* Ctrl+C */
    sleep(1);
}

void interactive_mode(struct mini_terminal *term) {
    printf("=== 交互模式 ===\n");
    printf("现在进入交互模式，输入将直接发送到shell\n");
    printf("按 Ctrl+\\ 退出交互模式\n");
    
    /* 设置原始模式 */
    struct termios raw = term->orig_termios;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    
    fd_set readfds;
    char buffer[1024];
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(term->pty_master, &readfds);
        
        int maxfd = (STDIN_FILENO > term->pty_master) ? 
                    STDIN_FILENO : term->pty_master;
        
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            break;
        }
        
        /* 用户输入 -> PTY */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (n > 0) {
                /* 检查退出序列 Ctrl+\ */
                if (n == 1 && buffer[0] == 0x1C) {
                    break;
                }
                write(term->pty_master, buffer, n);
            }
        }
        
        /* PTY输出 -> 用户 */
        if (FD_ISSET(term->pty_master, &readfds)) {
            ssize_t n = read(term->pty_master, buffer, sizeof(buffer));
            if (n > 0) {
                write(STDOUT_FILENO, buffer, n);
            } else if (n == 0) {
                printf("\nshell进程退出\n");
                break;
            }
        }
    }
    
    /* 恢复终端设置 */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term->orig_termios);
    printf("\n已退出交互模式\n");
}

int main() {
    struct mini_terminal term;
    
    /* 初始化 */
    term.rows = 24;
    term.cols = 80;
    
    /* 保存原始终端设置 */
    tcgetattr(STDIN_FILENO, &term.orig_termios);
    
    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== 简化终端模拟器测试 ===\n");
    
    if (setup_pty(&term) == -1) {
        return 1;
    }
    
    printf("PTY创建成功，shell PID: %d\n", term.shell_pid);
    
    /* 等待shell启动 */
    sleep(2);
    
    /* 读取shell的初始输出 */
    char buffer[1024];
    int flags = fcntl(term.pty_master, F_GETFL);
    fcntl(term.pty_master, F_SETFL, flags | O_NONBLOCK);
    
    ssize_t n = read(term.pty_master, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Shell初始输出:\n%s\n", buffer);
    }
    
    fcntl(term.pty_master, F_SETFL, flags);  /* 恢复阻塞模式 */
    
    /* 运行测试 */
    test_ansi_sequences(&term);
    test_window_resize(&term);
    test_signal_handling(&term);
    
    /* 交互模式 */
    interactive_mode(&term);
    
    /* 清理 */
    write(term.pty_master, "exit\n", 5);
    
    int status;
    waitpid(term.shell_pid, &status, 0);
    close(term.pty_master);
    
    printf("测试完成，shell退出状态: %d\n", WEXITSTATUS(status));
    return 0;
}
```

### 实验4：综合性能对比

```bash
#!/bin/bash
# comprehensive_performance.sh - 综合性能对比测试

echo "=== TTY应用综合性能对比 ==="

# 创建测试数据
echo "准备测试数据..."
dd if=/dev/urandom of=test_data bs=1M count=1 2>/dev/null
echo "Hello World" > small_data

echo -e "\n1. 直接执行 vs SSH执行性能对比:"

echo "本地执行时间:"
time cat test_data > /dev/null

echo "SSH执行时间 (localhost):"
time ssh localhost 'cat /tmp/test_data' > /dev/null 2>/dev/null || echo "SSH未配置或失败"

echo -e "\n2. 不同终端模式的I/O性能:"

echo "Canonical模式:"
time bash -c 'echo "line1\nline2\nline3" | cat'

echo "Raw模式模拟:"
time bash -c 'echo -e "line1\nline2\nline3" | while IFS= read -r line; do echo "$line"; done'

echo -e "\n3. PTY vs 管道性能对比:"

echo "PTY传输:"
time bash -c 'echo "test data" | script -qec "cat" /dev/null'

echo "管道传输:"
time bash -c 'echo "test data" | cat'

echo -e "\n4. 容器化开销测试:"

echo "本地cat执行:"
time cat small_data

if command -v docker >/dev/null; then
    echo "Docker容器cat执行:"
    time docker run --rm -i ubuntu:20.04 cat < small_data 2>/dev/null || echo "Docker不可用"
fi

echo -e "\n5. 终端大小对性能的影响:"

for size in "24x80" "50x132" "100x200"; do
    rows=$(echo $size | cut -d'x' -f1)
    cols=$(echo $size | cut -d'x' -f2)
    
    echo "终端大小 ${size}:"
    LINES=$rows COLUMNS=$cols time bash -c 'for i in {1..100}; do echo "Line $i with some text to fill the width"; done | cat > /dev/null'
done

echo -e "\n6. 信号处理开销:"

echo "无信号处理:"
time bash -c 'for i in {1..1000}; do echo $i; done > /dev/null'

echo "带信号处理:"
time bash -c 'trap "echo caught" TERM; for i in {1..1000}; do echo $i; done > /dev/null'

echo -e "\n7. 不同应用的启动时间:"

apps=("bash" "sh" "cat" "echo test")

for app in "${apps[@]}"; do
    echo "启动 $app:"
    time timeout 1 $app >/dev/null 2>&1 || true
done

echo -e "\n8. TTY缓冲区效率测试:"

echo "小缓冲区 (1字节):"
time dd if=test_data of=/dev/null bs=1 2>/dev/null

echo "大缓冲区 (64KB):"  
time dd if=test_data of=/dev/null bs=64k 2>/dev/null

echo -e "\n9. 并发TTY会话测试:"

echo "单个会话:"
time bash -c 'echo "test" | cat'

echo "10个并发会话:"
time bash -c 'for i in {1..10}; do (echo "test$i" | cat) & done; wait'

echo -e "\n10. 内存使用对比:"

echo "进程内存使用 (RSS):"
echo "bash: $(ps -o pid,rss,cmd -C bash | tail -n +2 | head -1)"

if pgrep sshd >/dev/null; then
    echo "sshd: $(ps -o pid,rss,cmd -C sshd | tail -n +2 | head -1)"
fi

if pgrep docker >/dev/null; then
    echo "docker: $(ps -o pid,rss,cmd -C docker | tail -n +2 | head -1)"
fi

echo -e "\n清理测试文件..."
rm -f test_data small_data

echo "性能测试完成！"

echo -e "\n总结:"
echo "- PTY比直接管道有额外开销"
echo "- SSH增加网络和加密开销"
echo "- 容器化有命名空间切换开销"  
echo "- 终端大小影响缓冲和渲染效率"
echo "- 信号处理增加系统调用开销"
```

## 🎯 总结和最佳实践

### TTY应用设计原则

1. **正确的PTY使用**
   - 理解master/slave的角色
   - 正确设置termios属性
   - 处理窗口大小变化

2. **信号处理**
   - 正确转发终端信号
   - 维护进程组关系
   - 处理会话管理

3. **性能优化**
   - 使用异步I/O
   - 合理的缓冲区大小
   - 避免频繁的系统调用

4. **兼容性考虑**
   - 支持不同的终端类型
   - 处理各种ANSI转义序列
   - 考虑不同操作系统的差异

### 常见应用模式

```c
// 通用PTY应用模式
struct pty_application {
    int master_fd;
    pid_t child_pid;
    struct termios saved_termios;
    // 应用特定状态...
};

int pty_app_init(struct pty_application *app) {
    /* 1. 创建PTY */
    /* 2. 设置终端属性 */
    /* 3. Fork并执行目标程序 */
    /* 4. 设置信号处理 */
    return 0;
}

int pty_app_main_loop(struct pty_application *app) {
    /* 1. 使用select/poll/epoll监听I/O */
    /* 2. 处理用户输入和程序输出 */
    /* 3. 处理信号和状态变化 */
    /* 4. 维护应用状态 */
    return 0;
}

void pty_app_cleanup(struct pty_application *app) {
    /* 1. 终止子进程 */
    /* 2. 关闭文件描述符 */
    /* 3. 恢复终端状态 */
    /* 4. 清理资源 */
}
```

## 🎯 学习检查点

完成本模块后，你应该能够：

1. ✅ 理解SSH中PTY的双端使用模式
2. ✅ 掌握Docker容器中TTY的分配和管理
3. ✅ 理解Terminal Emulator的实现原理
4. ✅ 能够实现基本的PTY应用程序
5. ✅ 会分析和调试TTY相关的应用问题
6. ✅ 理解不同应用场景中的性能考虑
7. ✅ 掌握TTY应用的最佳实践

---

**恭喜！** 你已经完成了Linux TTY子系统的系统性学习，从底层内核机制到高层应用实现，建立了完整的知识体系。现在你应该能够：

- 深入理解TTY/PTY的工作原理
- 调试和解决TTY相关问题
- 设计和实现TTY应用程序
- 优化TTY应用的性能
- 在SSH、Docker、终端模拟器等场景中正确使用TTY

这些知识将帮助你在系统编程、容器技术、远程连接等领域更加专业和深入。