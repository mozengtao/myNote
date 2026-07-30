# 第十一章:组合成 Mini Container

## 1. 学习目标

把第二~十章学到的所有技术(PID/UTS/Mount/Network Namespace、chroot、cgroups)自动化成一个脚本 `scripts/mini-docker.sh`,得到一个能用 `sudo ./mini-docker.sh run <rootfs> <command>` 直接启动"容器"的最小工具。这一章不引入任何新概念,只做"自动化 + 通读代码"。

## 2. 前置知识

- 第九章:Namespace + chroot 组合
- 第十章:cgroups(CPU/Memory/PIDs)
- 基本的 Shell 脚本阅读能力(变量、函数、`trap`)

## 3. 原理简介

到目前为止,你在终端里手工敲的每一条命令,都是在为同一个目标服务:**让一个即将被 `chroot`/`exec` 出来的目标进程,提前具备正确的 Namespace 归属和 cgroup 归属**。

真正的容器运行时(runc)做的事情,本质上和这一章的脚本是同一个套路,只是:

- 配置来源从"你手敲的参数"变成了"OCI 规范定义的 `config.json`"
- 实现语言从 Shell 变成了 Go,直接调用系统调用而不是拼命令行
- 多了大量错误处理、安全加固(User Namespace、Capabilities、Seccomp 等,本课程不深入)

但**核心骨架完全一致**:

```
1. 准备 cgroup,把"待会儿要变成容器"的进程放进去
        |
        v
2. 用 unshare/clone 创建一组 Namespace(PID/UTS/Mount/Network/...)
        |
        v
3. 在新的 Mount Namespace 里,把 /proc 挂载到目标 rootfs 下
        |
        v
4. chroot(或 pivot_root)切换根文件系统到目标 rootfs
        |
        v
5. 在新根、新 Namespace、新 cgroup 里,execve() 用户真正想跑的命令
```

## 4. 脚本代码逐段解读

脚本位于 [`scripts/mini-docker.sh`](scripts/mini-docker.sh),下面逐段解释关键逻辑(完整代码请直接打开该文件阅读,这里只摘录重点片段并解释"为什么这么写")。

### 4.1 cgroup 的创建与"提前占位"

```bash
setup_cgroup() {
  echo "+cpu +memory +pids" > "${CGROUP_ROOT}/cgroup.subtree_control" 2>/dev/null || true
  mkdir -p "${CGROUP_PATH}"
  echo "${CPU_MAX}"  > "${CGROUP_PATH}/cpu.max"
  echo "${MEM_MAX}"  > "${CGROUP_PATH}/memory.max"
  echo "${PIDS_MAX}" > "${CGROUP_PATH}/pids.max"
  echo $$ > "${CGROUP_PATH}/cgroup.procs"
}
```

**为什么这么写**:关键在最后一行——脚本把**自己当前的 PID(`$$`)**写进了 cgroup,而不是等容器进程起来之后再去找它的 PID 塞进去。这是因为 Linux 子进程会自动继承父进程的 cgroup 归属,所以只要脚本自己先"入组",它之后 `fork` 出来的 `unshare`、`unshare` 再 `fork` 出来的 `chroot`、`chroot` 再 `exec` 出来的目标命令,全部自动在这个 cgroup 里,不需要额外一步步去"追踪 PID 再写入"。这跟第十章子实验 C(先把当前 shell 放进 `mini-pids`,再执行 fork 炸弹)是完全一样的技巧。

### 4.2 一行命令拼出所有 Namespace + chroot

```bash
unshare --pid --uts --mount --net --fork \
  --mount-proc="${rootfs}/proc" \
  chroot "${rootfs}" /bin/sh -c "
    /bin/hostname '${HOSTNAME_IN_CONTAINER}' 2>/dev/null
    exec ${cmd[*]}
  "
```

这一整段就是第九章那条命令原封不动的自动化版本:

- `--pid --uts --mount --net --fork`:对应第二、三、四、五章。
- `--mount-proc="${rootfs}/proc"`:对应第二章"必须重新挂载 /proc"+第九章"路径要写换根前的宿主机路径"这两个知识点。
- `chroot "${rootfs}" /bin/sh -c "..."`:对应第八、九章,换根之后先设置 hostname,再用 `exec` 把 `/bin/sh -c` 这个中间层替换成用户真正想跑的命令,避免多一层无用的 shell 进程常驻(这也是很多真实容器镜像的 `ENTRYPOINT`/`CMD` 处理方式里会考虑的细节)。

### 4.3 清理:为什么要"先移出去,再删目录"

```bash
cleanup_cgroup() {
  echo $$ > "${CGROUP_ROOT}/cgroup.procs" 2>/dev/null || true
  rmdir "${CGROUP_PATH}" 2>/dev/null || true
}
```

**为什么这么写**:cgroup v2 不允许删除一个还挂着进程的分组。脚本自己的 PID 从一开始就写进了 `mini-docker-<pid>` 这个分组,所以在删除这个分组之前,必须先把自己移回根 cgroup(`echo $$ > ${CGROUP_ROOT}/cgroup.procs`),这样子分组才会变空,`rmdir` 才能成功。`trap cleanup_cgroup EXIT` 确保不管容器进程是正常退出还是异常退出,脚本自己退出前都会做这次清理,不会在系统里留下一堆用不到的空 cgroup 目录。

## 5. 实验步骤:实际跑一次

### Step 1:准备一个 rootfs(复用第九章的产物即可)

```bash
mkdir -p ~/mini-ns-lab/rootfs/bin
cp $(which busybox) ~/mini-ns-lab/rootfs/bin/busybox
cd ~/mini-ns-lab/rootfs/bin
for cmd in sh ls ps hostname mount echo cat mkdir sleep top; do
  ln -sf busybox $cmd
done
```

### Step 2:运行 mini-docker.sh

```bash
cd docker/mini-docker-lab
sudo ./scripts/mini-docker.sh run ~/mini-ns-lab/rootfs /bin/sh
```

观察启动时打印的日志(cgroup 路径、限制参数),进入之后照第九章的方式验证 `ps aux`、`hostname`、`ls /`,应该和第九章手工敲命令得到的结果完全一致。

### Step 3:验证资源限制确实生效了

在容器里执行一个吃 CPU 的命令,另一个宿主机终端观察 `top`:

```
# 容器内
/bin/sh -c 'while true; do :; done' &
```

```bash
# 宿主机
top
```

**观察**:这个死循环进程的 CPU 占用被限制在脚本默认的 `cpu.max = "50000 100000"`(约 50%)附近,和第十章子实验 A 的效果一致——因为它继承了脚本创建的 cgroup。

### Step 4:退出容器,确认清理生效

```
exit
```

回到宿主机后执行:

```bash
ls /sys/fs/cgroup/ | grep mini-docker
```

**观察**:没有残留的 `mini-docker-*` 目录,说明 `trap cleanup_cgroup EXIT` 正确执行了。

## 6. 实验结果分析

- 这个脚本没有发明任何新技术,只是把前十章的命令按固定顺序、加上参数化和错误处理,拼装成一个可复用的工具。
- "自动化"本身就是从"实验脚本"到"生产级运行时"最重要的一步转变——`runc` 相比这个脚本,多的主要是**更严谨的默认安全策略**(User Namespace、Capabilities 收敛、Seccomp 过滤)和**遵循标准化的输入格式**(OCI `config.json`),核心骨架并没有本质区别。
- 这个脚本依然没有:镜像格式支持(需要你自己准备 rootfs)、OverlayFS 自动分层(可以自己把 `merged` 目录传给它)、真实联网(net namespace 默认是空的孤岛,需要手工接上第六章的 veth+bridge)。这些正是留给你在真实 Docker/runc 源码里能继续对照阅读的部分(终章会给出具体路径)。

## 7. Docker 对应关系

```
今天实验:

sudo ./mini-docker.sh run <rootfs> <command>

        |
        v

Docker:

docker run <image> <command>

        |
        v

dockerd 把 <image> 解析成 OverlayFS 的 lowerdir 列表 + 新建 upperdir
生成一份 OCI Runtime Bundle(rootfs 目录 + config.json)
调用 containerd -> containerd-shim -> runc create/start
runc 读取 config.json,按里面声明的 namespaces/resources/mounts 字段,
做的事情和 mini-docker.sh 的 setup_cgroup() + unshare + chroot 本质相同
```

## 8. 心智模型

```
Mini Docker
    =
Namespace(视图隔离) + cgroups(资源收口) + chroot(文件系统入口)
按固定顺序自动拼装起来的一个脚本

Docker
    =
同样的骨架
+ 镜像格式(OverlayFS 自动分层)
+ 标准化配置(OCI Runtime Spec)
+ 更完善的安全默认值
+ 客户端/服务端架构(dockerd/containerd/runc 分层,下一章展开)
```

## 9. 思考题

1. 如果要给 `mini-docker.sh` 加上"自动联网"能力(启动时自动创建 veth pair、接上 bridge、退出时自动清理),你会把这部分逻辑加在脚本的哪个阶段(`unshare` 之前、之中,还是之后)?为什么(提示:veth 的一端需要被"移入"目标 Network Namespace,而这个 Namespace 要等 `unshare --net` 真正创建出来才存在)?
2. `mini-docker.sh` 目前是"一次运行、退出即销毁"的模式,不支持类似 `docker exec` 那样"进入一个已经在运行的容器"。要支持这个能力,大概需要用到哪个系统调用/命令(提示:`man 1 nsenter`)?
3. 这个脚本没有做 User Namespace 隔离,意味着容器里的 root 就是宿主机的 root。如果要在不改变现有骨架的前提下加上 User Namespace(`unshare --user --map-root-user`),你预计还需要解决什么新问题(提示:chroot、cgroup 文件的属主权限、rootfs 里文件的属主是否需要重新映射)?

## 10. 延伸阅读

- `man 1 unshare`、`man 1 nsenter`、`man 7 cgroups`(前面几章的延伸阅读合集)
- runc 源码:[`opencontainers/runc`](https://github.com/opencontainers/runc) 的 `libcontainer/` 目录(终章会给出更具体的文件路径)
- Liz Rice 的经典演讲 *"Containers From Scratch"*(用 Go 手写一个几十行的 mini container,和这一章的思路高度一致,建议看完这套课程后再看会有更强的共鸣)

---

下一章:[第十二章:Docker 架构对照分析](12-docker-architecture-mapping.md) —— 回到真实的 `docker run`,看看 dockerd/containerd/runc 各自在这条链路上做了什么。
