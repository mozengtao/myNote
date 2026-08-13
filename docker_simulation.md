## 手动创建简易容器，加深对docker的理解
```bash
# # 启动Nginx 容器
morrism@PC24036 ~> docker run -d --name web -p 80:80 --memory="256m" --cpus="1" nginx
3008a577eea180983edcda3b6c14c8e7c64832633742998d8ebdb86d73ce17b9

# 获取容器在宿主机上的 PID
morrism@PC24036 ~> docker inspect --format '{{.State.Pid}}' web
11392

## Namespaces
# Pid Namespace
morrism@PC24036 ~> ps aux | grep nginx
root       11392  0.1  0.0  11412  7764 ?        Ss   09:42   0:00 nginx: master process nginx -g daemon off;
systemd+   11491  0.0  0.0  11880  3012 ?        S    09:42   0:00 nginx: worker process
...

morrism@PC24036 ~> docker exec -it web bash
root@3008a577eea1:/# ps aux
USER         PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
root           1  0.0  0.0  11412  7764 ?        Ss   01:42   0:00 nginx: master process nginx -g daemon off;
nginx         29  0.0  0.0  11880  3012 ?        S    01:42   0:00 nginx: worker process
...

# Mount Namespace
morrism@PC24036 ~> sudo ls /proc/11392/root/
bin  boot  dev  docker-entrypoint.d  docker-entrypoint.sh  etc  home  lib  lib64  media  mnt  opt  proc  root  run  sbin  srv  sys  tmp  usr  var

morrism@PC24036 ~> docker exec -it web bash
root@3008a577eea1:/# ls /
bin  boot  dev  docker-entrypoint.d  docker-entrypoint.sh  etc  home  lib  lib64  media  mnt  opt  proc  root  run  sbin  srv  sys  tmp  usr  var

morrism@PC24036 ~> cat /proc/11392/mountinfo | head -n 3
1160 703 0:98 / / rw,relatime - overlay overlay rw,lowerdir=/var/lib/docker/overlay2/l/66ZM72GOIYLYDVVCXECCFNYMDH:/var/lib/docker/overlay2/l/TJYQVMVV4FJKLM5OBTC7YSI3TK:/var/lib/docker/overlay2/l/XAREPOH5A5OPE5FDCI3TYOGFMP:/var/lib/docker/overlay2/l/WU4KROALNRXO3DXYTMQ6O7SSTZ:/var/lib/docker/overlay2/l/PQ3EVTIS5PQISSMRCHD7OV4CJD:/var/lib/docker/overlay2/l/PO2FBJFWZROYHKUL5ZWO344RA2:/var/lib/docker/overlay2/l/SOZTNNVE6PEO34QVIVEUPDX5KI:/var/lib/docker/overlay2/l/UAAZXQJZRSNAOPV7RNMO5HWZWE,upperdir=/var/lib/docker/overlay2/b7dd9caa82a26bae084f818323fa5768156f97ba77cb0dc332d19b0594664692/diff,workdir=/var/lib/docker/overlay2/b7dd9caa82a26bae084f818323fa5768156f97ba77cb0dc332d19b0594664692/work
1186 1160 0:115 / /proc rw,nosuid,nodev,noexec,relatime - proc proc rw
1195 1160 0:116 / /dev rw,nosuid - tmpfs tmpfs rw,size=65536k,mode=755

# Network Namespace
morrism@PC24036 ~> sudo ls -l /proc/11392/ns/
total 0
lrwxrwxrwx 1 root root 0 Aug  3 09:42 cgroup -> 'cgroup:[4026532269]'
lrwxrwxrwx 1 root root 0 Aug  3 09:42 ipc -> 'ipc:[4026532239]'
lrwxrwxrwx 1 root root 0 Aug  3 09:42 mnt -> 'mnt:[4026532234]'
lrwxrwxrwx 1 root root 0 Aug  3 09:42 net -> 'net:[4026532270]'
lrwxrwxrwx 1 root root 0 Aug  3 09:42 pid -> 'pid:[4026532268]'
lrwxrwxrwx 1 root root 0 Aug  3 09:49 pid_for_children -> 'pid:[4026532268]'
lrwxrwxrwx 1 root root 0 Aug  3 09:49 time -> 'time:[4026531834]'
lrwxrwxrwx 1 root root 0 Aug  3 09:49 time_for_children -> 'time:[4026531834]'
lrwxrwxrwx 1 root root 0 Aug  3 09:49 user -> 'user:[4026531837]'
lrwxrwxrwx 1 root root 0 Aug  3 09:42 uts -> 'uts:[4026532236]'

## Cgroups
morrism@PC24036 ~> cat /proc/11392/cgroup
0::/system.slice/docker-3008a577eea180983edcda3b6c14c8e7c64832633742998d8ebdb86d73ce17b9.scope
morrism@PC24036 ~> cat /sys/fs/cgroup/system.slice/docker-3008a577eea180983edcda3b6c14c8e7c64832633742998d8ebdb86d73ce17b9.scope/memory.max
268435456
morrism@PC24036 ~> cat /sys/fs/cgroup/system.slice/docker-3008a577eea180983edcda3b6c14c8e7c64832633742998d8ebdb86d73ce17b9.scope/memory.current
140959744

## Capabilities
morrism@PC24036 ~> grep Cap /proc/11392/status
CapInh: 0000000000000000
CapPrm: 00000000a80425fb
CapEff: 00000000a80425fb
CapBnd: 00000000a80425fb
CapAmb: 0000000000000000
morrism@PC24036 ~> capsh --decode=00000000a80425fb
0x00000000a80425fb=cap_chown,cap_dac_override,cap_fowner,cap_fsetid,cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_net_bind_service,cap_net_raw,cap_sys_chroot,cap_mknod,cap_audit_write,cap_setfcap
```

## 利用 Linux 原生工具复刻 Docker 的核心功能
```bash
# ==========================================================
# 终极实验：手搓一个“高仿 Docker”
# 目标：利用 Linux 原生工具复刻 Docker 的核心功能
# 涵盖：Namespace隔离、Cgroups限制、网络配置、安全加固
# ==========================================================

# ----------------------------------------------------------
# 阶段一：准备根文件系统 (RootFS)
# ----------------------------------------------------------
mkdir -p ~/mycontainer/rootfs
cd ~/mycontainer

# 下载静态编译的 BusyBox (无需依赖库)
wget https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox
chmod +x busybox

# 安装至根目录
cd rootfs
../busybox --install .
mkdir -p proc sys dev etc home tmp
# 拷贝 DNS 解析配置以便容器内联网
cp /etc/resolv.conf etc/
cd ..

# ----------------------------------------------------------
# 阶段二：配置网络 (Network Namespace)
# Docker 原理：创建 veth pair，一端进容器，一端连网桥
# ----------------------------------------------------------
# 1. 创建网络命名空间
sudo ip netns add ns-demo

# 2. 创建 veth pair (虚拟网卡对)
sudo ip link add veth0 type veth peer name veth1

# 3. 将 veth1 移入命名空间
sudo ip link set veth1 netns ns-demo

# 4. 配置宿主机端 (veth0)
sudo ip addr add 10.1.1.1/24 dev veth0
sudo ip link set veth0 up
# (可选) 开启 IP 转发，允许容器访问外网
# echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward
# sudo iptables -t nat -A POSTROUTING -s 10.1.1.0/24 -o eth0 -j MASQUERADE

# 5. 配置容器端 (veth1)
sudo ip netns exec ns-demo ip addr add 10.1.1.2/24 dev veth1
sudo ip netns exec ns-demo ip link set veth1 up
sudo ip netns exec ns-demo ip link set lo up
sudo ip netns exec ns-demo ip route add default via 10.1.1.1

# ----------------------------------------------------------
# 阶段三：配置资源限制 (Cgroups v2)
# Docker 原理：--memory 和 --cpus 参数最终写入 cgroup 文件
# ----------------------------------------------------------
# 1. 创建控制组
sudo mkdir -p /sys/fs/cgroup/mycontainer

# 2. 限制内存为 100MB
echo "100M" | sudo tee /sys/fs/cgroup/mycontainer/memory.max

# 3. 限制 CPU (每 100ms 周期最多使用 50ms，即 0.5 核)
echo "50000 100000" | sudo tee /sys/fs/cgroup/mycontainer/cpu.max

# ----------------------------------------------------------
# 阶段四：启动“容器” (Unshare + Chroot)
# 核心：将进程隔离进新的 Namespace 和 Cgroup
# ----------------------------------------------------------
sudo unshare \
  --fork \
  --pid \           # PID 隔离 (ps 只看自己)
  --mount \         # Mount 隔离 (独立 /proc)
  --uts \           # UTS 隔离 (独立 hostname)
  --cgroup \        # Cgroup 隔离 (继承当前 cgroup)
  --net=/var/run/netns/ns-demo \ # 加入刚才创建的网络命名空间
  --kill-child \    # 退出时清理子进程
  chroot rootfs /bin/sh

# ----------------------------------------------------------
# 阶段五：容器内验证 (此时已进入新 Shell)
# ----------------------------------------------------------
# 1. 挂载伪文件系统 (必须，否则 ps 等命令数据错误)
mount -t proc none /proc
mount -t sysfs none /sys

# 2. 验证隔离性
hostname container-demo    # 修改主机名
ps aux                     # 应该只能看到 PID 1 (sh) 和 ps 自己
ip addr show               # 应该看到 veth1 网卡

# 3. 验证网络连通性
ping -c 3 10.1.1.1         # Ping 宿主机
# ping -c 3 www.baidu.com # Ping 外网 (需开启 MASQUERADE)

# 4. 验证资源限制 (读取 cgroup 文件)
cat /sys/fs/cgroup/memory.max
cat /sys/fs/cgroup/cpu.max

# 5. 验证权限削减 (Capabilities)
# 尝试加载内核模块 (Docker 默认禁止此操作)
modprobe br_netfilter
# 预期结果: modprobe: ERROR: could not insert 'br_netfilter': Operation not permitted

# ----------------------------------------------------------
# 阶段六：宿主机监控 (模拟 docker stats)
# ----------------------------------------------------------
# 另开一个宿主机终端，观察资源限制是否生效
# 查看当前 cgroup 内的进程列表
cat /sys/fs/cgroup/mycontainer/cgroup.procs
# 查看内存使用情况
cat /sys/fs/cgroup/mycontainer/memory.current
# 查看 CPU 使用时间 (单位：微秒)
cat /sys/fs/cgroup/mycontainer/cpu.stat

# ----------------------------------------------------------
# 阶段七：清理环境
# ----------------------------------------------------------
# 1. 退出容器 Shell
exit

# 2. 删除网络命名空间
sudo ip netns del ns-demo
sudo ip link del veth0 2>/dev/null # 清理宿主机端网卡

# 3. 删除 Cgroups
sudo rmdir /sys/fs/cgroup/mycontainer

# 4. 删除文件
# rm -rf ~/mycontainer
```