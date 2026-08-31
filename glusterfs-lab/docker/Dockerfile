FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# glusterfs-server 10.1 来自 Ubuntu 22.04 (jammy) 官方仓库，无需 PPA。
# 额外工具都是为了后面几课做"证据采集"用的：
#   attr       -> getfattr/setfattr，看 Brick 上的扩展属性 (Lesson 2/5/8)
#   tcpdump    -> 抓 RPC 包 (Lesson 4/7/9)
#   strace     -> 观察系统调用/translator 调用链 (Lesson 6)
#   net-tools/iproute2/iputils-ping/dnsutils -> 基本网络诊断
#   fuse       -> client 端 FUSE 挂载 (Lesson 4+)
#   vim/less/jq -> 阅读 volfile/json 输出
RUN apt-get update && apt-get install -y --no-install-recommends \
    glusterfs-server \
    glusterfs-client \
    glusterfs-cli \
    attr \
    tcpdump \
    strace \
    net-tools \
    iproute2 \
    iputils-ping \
    dnsutils \
    fuse \
    rsync \
    vim \
    less \
    jq \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
