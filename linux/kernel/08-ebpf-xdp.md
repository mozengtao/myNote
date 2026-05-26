# eBPF & XDP 深度解析

## 🧩 1. 宏观架构图

### eBPF 生态架构

```
User Space              Kernel Space                     Hardware
┌─────────────┐          ┌─────────────────────────────┐   ┌─────────┐
│ Application │          │       eBPF Programs         │   │   NIC   │
│ ┌─────────┐ │          │ ┌─────────┬─────────┬──────┐ │   │ ┌─────┐ │
│ │clang/llc│ │          │ │ Socket  │ TC/XDP  │Kprobe│ │   │ │ RX  │ │
│ └────┬────┘ │          │ │ Filter  │Programs │/Trace│ │   │ │Queue│ │
│      │      │          │ └─────────┴─────────┴──────┘ │   │ └──┬──┘ │
│ ┌────▼────┐ │          │                             │   │    │   │
│ │eBPF     │ │          │    eBPF VM & JIT            │   │    │   │
│ │Bytecode │ │◄────────►│ ┌─────────────────────────┐ │   │    │   │
│ └─────────┘ │ syscall  │ │ Verifier → JIT Compiler │ │   │    │   │
│             │ bpf()    │ └─────────────────────────┘ │   │    │   │
│ ┌─────────┐ │          │                             │   │    │   │
│ │ eBPF    │ │          │       eBPF Maps             │   │    │   │
│ │ Maps    │◄┼──────────┼─► ┌─────────────────────┐   │   │    │   │
│ │(userspace)│          │  │Array│Hash│Stack│...  │   │   │    │   │
│ └─────────┘ │          │  └─────────────────────┘   │   │    │   │
└─────────────┘          └──────────────┬──────────────┘   │    │   │
                                        │                  │    │   │
                           ┌────────────▼──────────────┐   │    │   │
                           │  Kernel Network Stack     │   │    │   │
                           │                           │   │    │   │
     XDP Hook Point:       │ Driver RX → XDP Program   │◄──┘    │   │
     ┌─────────────────────┤         ↓                 │        │   │
     │ XDP_DROP            │ Generic Netdev Layer      │        │   │
     │ XDP_PASS            │         ↓                 │        │   │
     │ XDP_TX              │ TC (Traffic Control)      │        │   │
     │ XDP_REDIRECT        │         ↓                 │        │   │
     └─────────────────────┤ Netfilter/iptables        │        │   │
                           │         ↓                 │        │   │
                           │ Socket Layer              │        │   │
                           └───────────────────────────┘        │   │
                                        │                      │   │
                           ┌────────────▼──────────────┐       │   │
                           │   User Application        │       │   │
                           └───────────────────────────┘       │   │
                                                               │   │
                           ┌───────────────────────────────────▼───▼─┐
                           │         Packet Flow                     │
                           │ NIC → Driver → XDP → Stack → Socket     │
                           └───────────────────────────────────────┘
```

*图表说明：eBPF 生态从用户态通过 bpf() 系统调用加载字节码，经 Verifier 验证和 JIT 编译后在 XDP/TC/Kprobe 等挂载点运行；XDP 在网卡驱动层最早拦截数据包，可 DROP/PASS/TX/REDIRECT，绕过或加速常规网络栈处理。*

### eBPF 程序类型与挂载点

```
┌─────────────────────────────────────────────────────────────────┐
│              eBPF Program Type Ecosystem                        │
├─────────────────┬───────────────────┬───────────────────────────┤
│   Networking    │  System Tracing   │   Security & Others       │
├─────────────────┼───────────────────┼───────────────────────────┤
│ XDP             │ Kprobe/Kretprobe  │ LSM (Linux Security Module)│
│ TC (qdisc)      │ Tracepoint        │ Cgroup                    │
│ Socket Filter   │ Perf Event        │ Seccomp                   │
│ cgroup/skb      │ Raw Tracepoint    │ Device Control            │
│ lwt (Lightweight│ Uprobe/Uretprobe  │ Sysctl                    │
│  Tunnel)        │ USDT              │                           │
│ sk_reuseport    │ Hardware PMU      │                           │
└─────────────────┴───────────────────┴───────────────────────────┘
```

*图表说明：eBPF 程序按功能分为网络（XDP/TC/Socket Filter）、系统跟踪（Kprobe/Tracepoint/Uprobe）和安全（LSM/Seccomp/Cgroup）三大类，可在不同内核挂载点运行用户定义逻辑。*

---

## 🔬 2. 内核执行路径

### 2.1 eBPF 程序加载全流程

```
User Program calls bpf() Syscall
         ↓
    sys_bpf()               (kernel/bpf/syscall.c)
         ↓
    bpf_prog_load()         Validate attr Parameters
         ↓
    bpf_check()             (kernel/bpf/verifier.c)
    ┌────────────────────────────────────┐
    │ eBPF Verifier Core Flow            │
    │ 1. Control Flow Graph (CFG) Analysis│
    │ 2. Data Flow Tracking              │
    │ 3. Instruction Legality Check      │
    │ 4. Memory Access Safety Validation │
    │ 5. Loop Detection (Prevent Infinite)│
    │ 6. Return Value Type Check         │
    └────────────────────────────────────┘
         ↓
    bpf_prog_select_runtime()
         ↓
┌────────────────┐    ┌─────────────────┐
│  Interpreter   │ OR │   JIT Compile   │
│ bpf_prog_run() │    │ bpf_jit_compile()│
└────────────────┘    └─────────────────┘
         ↓                      ↓
    Allocate Program Memory    Generate Native Machine Code
         ↓                      ↓
    Install to Kernel Hook     Direct CPU Execution
```

*图表说明：eBPF 程序加载流程——bpf() 系统调用触发 Verifier 对控制流、内存访问和循环安全性进行静态验证，通过后选择解释器或 JIT 编译模式，最终安装到内核钩子点执行。*

### 2.2 XDP 数据包处理路径

```
NIC Hardware Receives Packet
         ↓
NIC Driver netdev_rx()       e.g. ixgbe_poll()
         ↓
Allocate sk_buff or Use Page Pool
         ↓
Call XDP Hook:               bpf_prog_run_xdp()
┌──────────────────────────────────────────┐
│         XDP Program Execution            │
│ ctx = xdp_md {                          │
│   data,      // Packet Start Pointer    │
│   data_end,  // Packet End Pointer      │
│   data_meta, // Metadata Region         │
│   ingress_ifindex  // Ingress NIC       │
│ }                                       │
│                                         │
│ Program Can Access:                     │
│ - Ethernet Header (struct ethhdr)       │
│ - IP Header (struct iphdr)              │
│ - TCP/UDP Headers                       │
│ - Modify Packet Content                 │
│ - Query/Update eBPF Maps                │
└──────────────────────────────────────────┘
         ↓
XDP Program Returns Action Code:
┌─────────────┬──────────────────────────────┐
│ XDP_DROP    │ Drop Packet (Earliest Filter)│
│ XDP_PASS    │ Continue to Network Stack    │
│ XDP_TX      │ Reflect Back on Same Interface │
│ XDP_REDIRECT│ Redirect to Other NIC or CPU │
│ XDP_ABORTED │ Program Error, Drop Packet   │
└─────────────┴──────────────────────────────┘
         ↓ (if XDP_PASS)
    Build Full sk_buff
         ↓
    Enter Regular Network Stack (netif_receive_skb)
```

*图表说明：XDP 在网卡驱动层最早执行，通过 xdp_md 上下文直接访问和修改数据包；返回 XDP_DROP/PASS/TX/REDIRECT 等动作码决定数据包命运，PASS 时才构建完整 sk_buff 进入常规网络栈。*

### 2.3 eBPF Maps 操作路径

```
Userspace calls bpf_map_lookup_elem()
         ↓
    sys_bpf(BPF_MAP_LOOKUP_ELEM)
         ↓
    map_lookup_elem()          (kernel/bpf/syscall.c)
         ↓
    map->ops->map_lookup_elem()
         ↓
Dispatch by Map Type:
┌─────────────────┬─────────────────────────────┐
│ BPF_MAP_TYPE_   │      Implementation         │
├─────────────────┼─────────────────────────────┤
│ ARRAY           │ array_map_lookup_elem()     │
│ HASH            │ htab_map_lookup_elem()      │
│ PERCPU_HASH     │ htab_percpu_map_lookup_elem()│
│ LRU_HASH        │ htab_lru_map_lookup_elem()  │
│ STACK           │ stack_map_lookup_elem()     │
│ QUEUE           │ queue_map_lookup_elem()     │
│ RINGBUF         │ ringbuf_map_lookup_elem()   │
└─────────────────┴─────────────────────────────┘
         ↓
    Access Underlying Data Structure (Hash/Array/RCU)
         ↓
    Copy Data to Userspace Buffer
```

*图表说明：eBPF Maps 提供内核与用户态共享数据结构，bpf_map_lookup_elem 根据 Map 类型（Array/Hash/Stack/Queue/Ringbuf 等）分发到对应实现函数，完成键值查找和数据拷贝。*

---

## 🧱 3. 核心数据结构

### 3.1 eBPF 程序描述符

```c
struct bpf_prog {
    u16                     pages;          /* 程序占用的页数 */
    u16                     jited:1,        /* JIT 编译标志 */
                            jit_requested:1,
                            gpl_compatible:1,
                            cb_access:1,
                            dst_needed:1,
                            blinded:1,
                            is_func:1,
                            kprobe_override:1;
    enum bpf_prog_type      type;           /* 程序类型 */
    enum bpf_attach_type    expected_attach_type;
    u32                     len;            /* 程序指令数 */
    u32                     jited_len;      /* JIT后代码长度 */
    u8                      tag[BPF_TAG_SIZE];
    struct bpf_prog_aux     *aux;           /* 辅助信息 */
    struct sock_fprog_kern  *orig_prog;     /* 原始 BPF */
    unsigned int            (*bpf_func)(const void *ctx,
                                       const struct bpf_insn *insn);
    /* JIT编译后的函数指针 */
    union {
        struct sock_filter  insns[0];
        struct bpf_insn     insnsi[0];      /* eBPF指令数组 */
    };
};

struct bpf_prog_aux {
    atomic64_t              refcnt;         /* 引用计数 */
    u32                     used_map_cnt;   /* 使用的 map 数量 */
    u32                     max_ctx_offset; /* ctx 最大偏移 */
    u32                     max_pkt_offset; /* 数据包最大偏移 */
    u32                     stack_depth;    /* 栈深度 */
    u32                     id;             /* 程序 ID */
    u32                     func_cnt;       /* 函数数量 */
    u32                     func_idx;       /* 函数索引 */
    struct bpf_prog         **func;         /* 子程序数组 */
    void                    *jit_data;      /* JIT 私有数据 */
    struct bpf_map          **used_maps;    /* 使用的 maps */
    struct bpf_prog         *prog;          /* 指向主程序 */
    struct user_struct      *user;          /* 所属用户 */
    const char              *name;          /* 程序名称 */
    void                    *security;      /* 安全上下文 */
    struct bpf_prog_offload_ops *offload;   /* 硬件卸载 */
    struct btf              *btf;           /* BTF 类型信息 */
    struct bpf_func_info    *func_info;     /* 函数信息 */
    struct bpf_line_info    *linfo;         /* 行号信息 */
    void                    **jited_linfo;  /* JIT行号信息 */
    u32                     func_info_cnt;
    u32                     nr_linfo;
    u32                     linfo_idx;
};
```

### 3.2 XDP 上下文结构

```c
/* 传递给 XDP 程序的上下文 */
struct xdp_md {
    __u32 data;             /* 数据包起始位置 */
    __u32 data_end;         /* 数据包结束位置 */
    __u32 data_meta;        /* 元数据起始位置 */
    __u32 ingress_ifindex;  /* 入口网络接口索引 */
    __u32 rx_queue_index;   /* 接收队列索引 */
};

/* 内核中的 XDP 缓冲区 */
struct xdp_buff {
    void *data;             /* 数据包数据指针 */
    void *data_end;         /* 数据包结束指针 */
    void *data_meta;        /* 元数据指针 */
    void *data_hard_start;  /* 缓冲区硬起始地址 */
    unsigned long handle;   /* 缓冲区句柄 */
    struct xdp_rxq_info *rxq; /* 接收队列信息 */
};

struct xdp_rxq_info {
    struct net_device *dev;    /* 网络设备 */
    u32 queue_index;           /* 队列索引 */
    u32 reg_state;             /* 注册状态 */
    struct xdp_mem_info mem;   /* 内存信息 */
} ____cacheline_aligned;
```

### 3.3 eBPF Map 通用结构

```c
struct bpf_map {
    /* 第一个缓存行 - 热路径数据 */
    const struct bpf_map_ops *ops ____cacheline_aligned;
    struct bpf_map *inner_map_meta;
    void *security;
    enum bpf_map_type map_type;
    u32 key_size;              /* 键大小 */
    u32 value_size;            /* 值大小 */
    u32 max_entries;           /* 最大条目数 */
    u32 map_flags;             /* Map 标志 */
    int spin_lock_off;         /* 自旋锁偏移 */
    u32 id;                    /* Map ID */
    int numa_node;             /* NUMA 节点 */
    u32 btf_key_type_id;       /* BTF 键类型 ID */
    u32 btf_value_type_id;     /* BTF 值类型 ID */
    struct btf *btf;           /* BTF 类型信息 */
    struct bpf_map_memory memory; /* 内存管理 */
    char name[BPF_OBJ_NAME_LEN]; /* Map 名称 */
    bool bypass_spec_v1;       /* 绕过投机执行缓解 */
    bool frozen;               /* 冻结标志 */
    /* 第二个缓存行 */
    atomic64_t refcnt ____cacheline_aligned;
    atomic64_t usercnt;        /* 用户引用计数 */
    struct work_struct work;   /* 延迟释放工作 */
    struct mutex freeze_mutex; /* 冻结互斥锁 */
    u64 writecnt;              /* 写入计数 */
};

/* Hash Map 特定实现 */
struct bpf_htab {
    struct bpf_map map;
    struct bucket *buckets;         /* 哈希桶数组 */
    void *elems;                   /* 元素存储区 */
    union {
        struct pcpu_freelist freelist;
        struct bpf_lru lru;        /* LRU 淘汰策略 */
    };
    struct htab_elem *__percpu *extra_elems;
    atomic_t count;                /* 当前元素数量 */
    u32 n_buckets;                 /* 桶数量 */
    u32 elem_size;                 /* 元素大小 */
    u32 hashrnd;                   /* 哈希随机种子 */
};
```

---

## ⚙️ 4. 最小可运行实验

### 4.1 XDP 丢包程序 (DDoS 防护示例)

```c
/* xdp_drop_demo.c - 基于源 IP 的 DDoS 防护 */
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

/* 定义一个 Hash Map 存储黑名单 IP */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, __u32);                /* IP 地址 */
    __type(value, __u64);              /* 丢包计数 */
    __uint(max_entries, 1000000);      /* 最大 100 万条目 */
    __uint(map_flags, BPF_F_NO_PREALLOC);
} blacklist_ips SEC(".maps");

/* XDP 程序 */
SEC("xdp")
int xdp_drop_ddos(struct xdp_md *ctx)
{
    /* 解析以太网头 */
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;
    struct ethhdr *eth = data;
    
    /* 边界检查 */
    if ((void*)eth + sizeof(*eth) > data_end)
        return XDP_PASS;
    
    /* 只处理 IPv4 */
    if (bpf_ntohs(eth->h_proto) != ETH_P_IP)
        return XDP_PASS;
        
    /* 解析 IP 头 */
    struct iphdr *ip = data + sizeof(*eth);
    if ((void*)ip + sizeof(*ip) > data_end)
        return XDP_PASS;
    
    /* 检查源 IP 是否在黑名单中 */
    __u32 src_ip = ip->saddr;
    __u64 *drop_count = bpf_map_lookup_elem(&blacklist_ips, &src_ip);
    
    if (drop_count) {
        /* 更新丢包计数 */
        __sync_fetch_and_add(drop_count, 1);
        
        /* 丢弃数据包 */
        return XDP_DROP;
    }
    
    /* 允许正常流量通过 */
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
```

### 4.2 用户态控制程序

```c
/* xdp_drop_user.c - 用户态控制程序 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

int main(int argc, char **argv)
{
    const char *ifname = "eth0";
    const char *filename = "xdp_drop_demo.o";
    int prog_fd, map_fd;
    struct bpf_object *obj;
    struct bpf_program *prog;
    int ifindex;
    
    if (argc > 1)
        ifname = argv[1];
    
    /* 加载 eBPF 程序 */
    obj = bpf_object__open_file(filename, NULL);
    if (libbpf_get_error(obj)) {
        fprintf(stderr, "ERROR: opening BPF object file failed\n");
        return 1;
    }
    
    /* 加载到内核 */
    if (bpf_object__load(obj)) {
        fprintf(stderr, "ERROR: loading BPF object file failed\n");
        goto cleanup;
    }
    
    /* 获取程序 fd */
    prog = bpf_object__find_program_by_name(obj, "xdp_drop_ddos");
    if (!prog) {
        fprintf(stderr, "ERROR: finding XDP program failed\n");
        goto cleanup;
    }
    prog_fd = bpf_program__fd(prog);
    
    /* 获取 map fd */
    map_fd = bpf_object__find_map_fd_by_name(obj, "blacklist_ips");
    if (map_fd < 0) {
        fprintf(stderr, "ERROR: finding blacklist map failed\n");
        goto cleanup;
    }
    
    /* 获取网卡接口索引 */
    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        fprintf(stderr, "ERROR: interface %s not found\n", ifname);
        goto cleanup;
    }
    
    /* 附加 XDP 程序到网卡 */
    if (bpf_set_link_xdp_fd(ifindex, prog_fd, XDP_FLAGS_UPDATE_IF_NOEXIST) < 0) {
        fprintf(stderr, "ERROR: attaching XDP program to %s failed\n", ifname);
        goto cleanup;
    }
    
    printf("XDP program attached to %s successfully\n", ifname);
    printf("Map fd: %d\n", map_fd);
    
    /* 添加一些黑名单 IP 示例 */
    __u32 bad_ips[] = {
        inet_addr("192.168.1.100"),
        inet_addr("10.0.0.50"),
        inet_addr("172.16.1.200")
    };
    
    for (int i = 0; i < sizeof(bad_ips)/sizeof(__u32); i++) {
        __u64 init_count = 0;
        if (bpf_map_update_elem(map_fd, &bad_ips[i], &init_count, BPF_ANY)) {
            fprintf(stderr, "ERROR: updating blacklist failed\n");
        } else {
            struct in_addr addr;
            addr.s_addr = bad_ips[i];
            printf("Added %s to blacklist\n", inet_ntoa(addr));
        }
    }
    
    printf("Press Ctrl+C to exit...\n");
    
    /* 统计循环 */
    while (1) {
        sleep(5);
        
        printf("\n=== Drop Statistics ===\n");
        for (int i = 0; i < sizeof(bad_ips)/sizeof(__u32); i++) {
            __u64 count;
            if (bpf_map_lookup_elem(map_fd, &bad_ips[i], &count) == 0) {
                struct in_addr addr;
                addr.s_addr = bad_ips[i];
                printf("%s: %llu drops\n", inet_ntoa(addr), count);
            }
        }
    }
    
cleanup:
    bpf_object__close(obj);
    return 0;
}
```

### 4.3 编译与运行脚本

```bash
#!/bin/bash
# build_xdp_demo.sh

echo "Building XDP DDoS protection demo..."

# 编译 eBPF 程序
clang -O2 -target bpf -c xdp_drop_demo.c -o xdp_drop_demo.o

# 编译用户态程序
gcc -o xdp_drop_user xdp_drop_user.c -lbpf

echo "Build completed!"
echo ""
echo "Usage:"
echo "  sudo ./xdp_drop_user [interface_name]"
echo ""
echo "Example:"
echo "  sudo ./xdp_drop_user eth0"
echo ""
echo "Test with:"
echo "  # 从黑名单 IP 发包测试"
echo "  ping -I 192.168.1.100 target_host"
echo ""
echo "Monitor with:"
echo "  sudo tcpdump -i eth0 -c 10"
echo "  ip -s link show eth0"
```

### 4.4 TCP 连接跟踪程序

```c
/* tcp_tracker.c - 跟踪 TCP 连接建立过程 */
#include <linux/bpf.h>
#include <linux/ptrace.h>
#include <linux/version.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct tcp_event {
    __u32 pid;
    __u32 src_ip;
    __u32 dst_ip;
    __u16 src_port;
    __u16 dst_port;
    char comm[16];
    __u8 event_type;  /* 0=connect, 1=accept, 2=close */
};

/* Ring Buffer 用于向用户态发送事件 */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

/* 跟踪 tcp_connect() 函数 */
SEC("kprobe/tcp_connect")
int trace_tcp_connect(struct pt_regs *ctx)
{
    struct tcp_event *event;
    struct sock *sk = (struct sock *)PT_REGS_PARM1(ctx);
    
    /* 从 ring buffer 预留空间 */
    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (!event)
        return 0;
    
    /* 获取进程信息 */
    event->pid = bpf_get_current_pid_tgid() >> 32;
    bpf_get_current_comm(&event->comm, sizeof(event->comm));
    event->event_type = 0; /* connect */
    
    /* 读取 socket 信息 */
    bpf_probe_read_kernel(&event->src_ip, sizeof(event->src_ip), 
                         &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read_kernel(&event->dst_ip, sizeof(event->dst_ip), 
                         &sk->__sk_common.skc_daddr);
    bpf_probe_read_kernel(&event->src_port, sizeof(event->src_port), 
                         &sk->__sk_common.skc_num);
    bpf_probe_read_kernel(&event->dst_port, sizeof(event->dst_port), 
                         &sk->__sk_common.skc_dport);
    
    /* 提交事件到用户空间 */
    bpf_ringbuf_submit(event, 0);
    
    return 0;
}

/* 跟踪 inet_csk_accept() 函数 */
SEC("kretprobe/inet_csk_accept")
int trace_tcp_accept(struct pt_regs *ctx)
{
    struct tcp_event *event;
    struct sock *sk = (struct sock *)PT_REGS_RC(ctx);
    
    if (!sk)
        return 0;
        
    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (!event)
        return 0;
    
    event->pid = bpf_get_current_pid_tgid() >> 32;
    bpf_get_current_comm(&event->comm, sizeof(event->comm));
    event->event_type = 1; /* accept */
    
    bpf_probe_read_kernel(&event->src_ip, sizeof(event->src_ip), 
                         &sk->__sk_common.skc_rcv_saddr);
    bpf_probe_read_kernel(&event->dst_ip, sizeof(event->dst_ip), 
                         &sk->__sk_common.skc_daddr);
    bpf_probe_read_kernel(&event->src_port, sizeof(event->src_port), 
                         &sk->__sk_common.skc_num);
    bpf_probe_read_kernel(&event->dst_port, sizeof(event->dst_port), 
                         &sk->__sk_common.skc_dport);
    
    bpf_ringbuf_submit(event, 0);
    
    return 0;
}

char _license[] SEC("license") = "GPL";
```

---

## 🔍 5. 可观测性 & Debug

### 5.1 eBPF 程序状态查看

```bash
# 查看已加载的 eBPF 程序
sudo bpftool prog list

# 详细信息
sudo bpftool prog show id <prog_id> --pretty

# 查看 JIT 汇编代码  
sudo bpftool prog dump jited id <prog_id>

# 查看 eBPF 字节码
sudo bpftool prog dump xlated id <prog_id>

# 查看程序统计信息
sudo bpftool prog show id <prog_id> --json | jq '.run_cnt, .run_time_ns'
```

### 5.2 eBPF Maps 调试

```bash
# 列出所有 Maps
sudo bpftool map list

# 查看 Map 内容
sudo bpftool map dump id <map_id>

# 实时监控 Map 变化
sudo bpftool map event-pipe id <map_id>

# 查看 Map 统计
sudo bpftool map show id <map_id> --pretty

# 手动更新 Map(测试用)
sudo bpftool map update id <map_id> key hex 01 02 03 04 value hex 05 06 07 08
```

### 5.3 XDP 性能监控

```bash
# XDP 统计信息
sudo ip -s link show dev eth0
# 查看 xdp_redirect_count, xdp_drop 等计数

# XDP 程序性能
sudo perf record -g -a sleep 10  # 采样 10 秒
sudo perf report --stdio

# 跟踪 XDP 函数
sudo trace-cmd record -p function -l xdp_do_generic_redirect
sudo trace-cmd report

# 使用 bcc 工具
sudo /usr/share/bcc/tools/xdp_drop_count.py

# 自定义跟踪脚本
sudo bpftrace -e '
kprobe:bpf_prog_run_xdp {
    @start[tid] = nsecs;
}
kretprobe:bpf_prog_run_xdp /@start[tid]/ {
    $duration = nsecs - @start[tid];
    @xdp_latency = hist($duration / 1000); /* microseconds */
    delete(@start[tid]);
}'
```

### 5.4 eBPF 验证器调试

```bash
# 启用详细验证日志
echo 1 > /proc/sys/kernel/bpf_stats_enabled

# 验证器日志级别
echo 2 > /proc/sys/net/core/bpf_jit_enable  # JIT + 调试信息

# 程序加载时查看验证器输出
sudo bpftool prog loadall program.o /sys/fs/bpf/ log-level 2 log-size 1048576

# 查看验证器拒绝的原因
dmesg | grep -i bpf | tail -20
```

### 5.5 使用 libbpf 调试接口

```c
/* debug_helper.c - eBPF 程序调试辅助 */
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

/* 启用 libbpf 调试输出 */
static int libbpf_debug_print(enum libbpf_print_level level,
                             const char *format, va_list args)
{
    if (level >= LIBBPF_DEBUG)
        return vfprintf(stderr, format, args);
    return 0;
}

int main()
{
    /* 设置调试回调 */
    libbpf_set_print(libbpf_debug_print);
    
    /* 加载程序时会输出详细信息 */
    struct bpf_object *obj = bpf_object__open("program.o");
    
    /* ... */
    return 0;
}
```

### 5.6 实时性能分析脚本

```python
#!/usr/bin/env python3
# xdp_perf_monitor.py - XDP 性能实时监控

import time
import json
import subprocess
from collections import defaultdict

def get_xdp_stats():
    """获取 XDP 程序统计信息"""
    try:
        # 获取程序列表
        result = subprocess.run(['bpftool', 'prog', 'list', '--json'], 
                              capture_output=True, text=True)
        programs = json.loads(result.stdout)
        
        stats = {}
        for prog in programs:
            if prog.get('type') == 'xdp':
                prog_id = prog['id']
                # 获取详细统计
                result = subprocess.run(['bpftool', 'prog', 'show', 'id', str(prog_id), '--json'],
                                      capture_output=True, text=True)
                detail = json.loads(result.stdout)
                
                stats[prog_id] = {
                    'name': prog.get('name', f'prog_{prog_id}'),
                    'run_cnt': detail.get('run_cnt', 0),
                    'run_time_ns': detail.get('run_time_ns', 0)
                }
        return stats
    except Exception as e:
        print(f"Error getting XDP stats: {e}")
        return {}

def monitor_performance():
    """监控 XDP 程序性能"""
    print("XDP Performance Monitor")
    print("=" * 60)
    
    prev_stats = {}
    
    while True:
        current_stats = get_xdp_stats()
        
        print(f"\n[{time.strftime('%H:%M:%S')}] XDP Performance")
        print("-" * 60)
        print(f"{'ID':<6} {'Name':<20} {'Rate(pps)':<12} {'Latency(us)':<12}")
        print("-" * 60)
        
        for prog_id, stats in current_stats.items():
            if prog_id in prev_stats:
                # 计算差值
                run_cnt_diff = stats['run_cnt'] - prev_stats[prog_id]['run_cnt']
                time_diff = stats['run_time_ns'] - prev_stats[prog_id]['run_time_ns']
                
                # 计算速率和延迟
                if run_cnt_diff > 0:
                    rate = run_cnt_diff  # 每秒数据包数
                    avg_latency = (time_diff / run_cnt_diff) / 1000  # 微秒
                else:
                    rate = 0
                    avg_latency = 0
                
                print(f"{prog_id:<6} {stats['name']:<20} {rate:<12.0f} {avg_latency:<12.2f}")
            else:
                print(f"{prog_id:<6} {stats['name']:<20} {'--':<12} {'--':<12}")
        
        prev_stats = current_stats.copy()
        time.sleep(1)

if __name__ == "__main__":
    try:
        monitor_performance()
    except KeyboardInterrupt:
        print("\nMonitoring stopped.")
```

---

## ⚡ 6. 性能与设计权衡

### 6.1 XDP vs 传统网络处理性能对比

| 处理阶段 | 传统路径延迟 | XDP 路径延迟 | 性能提升 | 说明 |
|----------|-------------|-------------|----------|------|
| **硬件接收** | ~1μs | ~1μs | 相同 | 都需要网卡 DMA |
| **驱动处理** | ~2μs | ~0.5μs | **4x** | XDP 避免 sk_buff 分配 |
| **协议栈** | ~10-50μs | **0** | **∞** | XDP 绕过整个网络栈 |
| **用户态拷贝** | ~5-10μs | **0** | **∞** | XDP 可在内核态直接处理 |
| **总延迟** | ~18-63μs | ~1.5μs | **12-42x** | 在最佳情况下 |

### 6.2 eBPF JIT vs 解释器性能

```
Performance Test Results (Million Instructions/sec):
┌─────────────────┬──────────┬──────────┬──────────┐
│ Instruction Type│Interpreter│ JIT Comp │ Speedup  │
├─────────────────┼──────────┼──────────┼──────────┤
│ Arithmetic      │   45M    │   850M   │  18.9x   │
│ Memory Access   │   30M    │   420M   │  14.0x   │
│ Map Lookup      │   12M    │   180M   │  15.0x   │
│ Function Call   │    8M    │   95M    │  11.9x   │
│ Complex Control │   15M    │   200M   │  13.3x   │
└─────────────────┴──────────┴──────────┴──────────┘

Memory Usage:
- Interpreter: ~4KB (fixed)
- JIT: ~8-32KB (depends on program complexity)
- Compile Time: ~1-10ms (one-time cost)
```

*图表说明：eBPF JIT 编译模式相比解释器在各指令类型上均有 12-19 倍性能提升，代价是额外 8-32KB 内存和一次性 1-10ms 编译开销。*

### 6.3 不同 eBPF Map 类型性能特征

| Map 类型 | 查找延迟 | 更新延迟 | 内存效率 | 并发性 | 适用场景 |
|----------|----------|----------|----------|--------|----------|
| **BPF_MAP_TYPE_ARRAY** | **O(1) ~50ns** | **O(1) ~60ns** | 最高 | 读优化 | 固定大小、频繁访问 |
| **BPF_MAP_TYPE_HASH** | O(1) ~200ns | O(1) ~300ns | 中等 | 良好 | 动态键值、通用场景 |
| **BPF_MAP_TYPE_LRU_HASH** | O(1) ~250ns | O(1) ~400ns | 高 | 中等 | 有限内存、热点数据 |
| **BPF_MAP_TYPE_PERCPU_HASH** | O(1) ~180ns | **O(1) ~150ns** | 中等 | **最佳** | 高并发写入 |
| **BPF_MAP_TYPE_STACK** | O(1) ~100ns | O(1) ~120ns | 高 | 中等 | LIFO 数据结构 |
| **BPF_MAP_TYPE_QUEUE** | O(1) ~100ns | O(1) ~120ns | 高 | 中等 | FIFO 数据结构 |
| **BPF_MAP_TYPE_RINGBUF** | N/A | O(1) ~80ns | 最高 | 最佳 | 用户态通信 |

### 6.4 XDP 挂载模式性能权衡

```c
/* XDP 挂载模式对比 */
enum xdp_attach_mode {
    XDP_MODE_SKB,      /* Generic XDP - 兼容性最好，性能最低 */
    XDP_MODE_DRV,      /* Driver XDP - 性能最好，需要驱动支持 */
    XDP_MODE_HW        /* Hardware XDP - 硬件卸载，延迟最低 */
};
```

| 模式 | 性能(Mpps) | CPU占用 | 内存占用 | 兼容性 | 功能限制 |
|------|------------|---------|----------|--------|----------|
| **Generic** | 2-5 | 高(80-90%) | 高 | **100%** | 完整功能 |
| **Driver** | **20-100** | **低(10-30%)** | 低 | ~60% | 部分限制 |
| **Hardware** | **100-200** | **极低(<5%)** | **最低** | ~5% | 严格限制 |

### 6.5 eBPF 程序复杂度限制

```
Kernel Verifier Limits (by Kernel Version):
┌────────────────────┬─────────┬─────────┬─────────┐
│   Resource Type    │ v4.14   │ v5.4    │ v5.15+  │
├────────────────────┼─────────┼─────────┼─────────┤
│ Max Instructions   │  4096   │  4096   │  1M     │
│ Max Stack Depth    │  512B   │  512B   │  512B   │
│ Max Map Count      │   64    │   64    │   64    │
│ Max Tail Call Depth│   32    │   32    │   33    │
│ Max Loop Iterations│   0     │   0     │ 1M(bnd) │
│ Max Function Count │   0     │   256   │  256    │
│ Max Line Info      │   0     │  65536  │  1M     │
└────────────────────┴─────────┴─────────┴─────────┘

Verification Time Complexity: O(n²) where n = instruction count
Actual Verification Time: 1-100ms (depends on program complexity)
```

*图表说明：eBPF Verifier 对程序复杂度有严格限制，内核 5.15+ 大幅放宽最大指令数和循环次数；验证时间复杂度 O(n²)，实际耗时 1-100ms。*

### 6.6 设计权衡总结

#### **性能优先场景**
- 使用 Driver/Hardware XDP
- Array Maps 替代 Hash Maps
- 避免复杂控制流
- 最小化 Map 查找次数
- Per-CPU Maps 减少锁竞争

#### **灵活性优先场景**  
- Generic XDP 保证兼容性
- Hash Maps 支持动态键
- Ring Buffer 用户态通信
- 复杂逻辑使用 Helper 函数

#### **内存效率场景**
- LRU Maps 自动淘汰
- 合理设置 max_entries
- 使用 BPF_F_NO_PREALLOC 按需分配
- Percpu 变量减少共享状态

---

## 🔗 7. 横向对比

### 7.1 eBPF vs 传统内核模块

| 特性维度 | **eBPF** | **内核模块** | **eBPF 优势** |
|----------|----------|-------------|---------------|
| **安全性** | 沙盒执行、验证器保证 | 完全内核权限 | ✅ 崩溃隔离 |
| **开发效率** | 用户态开发、热加载 | 内核态开发、重启 | ✅ 快速迭代 |
| **性能开销** | JIT编译、轻量级 | 原生代码执行 | ❌ 略有开销 |
| **功能限制** | 受验证器限制 | 无限制访问 | ❌ 功能受限 |
| **调试难度** | 工具链完善 | 传统内核调试 | ✅ 调试友好 |
| **部署复杂度** | 用户态程序 | 需要编译安装 | ✅ 简单部署 |

### 7.2 XDP vs DPDK vs AF_XDP

| 性能指标 | **XDP** | **DPDK** | **AF_XDP** |
|----------|---------|----------|------------|
| **最大吞吐** | 20-100 Mpps | **100-200 Mpps** | 30-80 Mpps |
| **最低延迟** | 1-2 μs | **0.5-1 μs** | 2-4 μs |
| **CPU占用** | 10-30% | **5-15%** | 15-40% |
| **开发复杂度** | 中等 | **高** | 低 |
| **内核兼容** | **完全兼容** | 绕过内核 | 部分兼容 |
| **应用集成** | 困难 | 重写应用 | **容易** |

**选择建议:**
- **XDP**: 防火墙、负载均衡、DDoS防护
- **DPDK**: 高频交易、电信级路由
- **AF_XDP**: 高性能用户态网络应用

### 7.3 eBPF 程序类型应用对比

```
              eBPF Program Type Ecosystem
                       │
        ┌──────────────┼───────────────┐
        │              │               │
   Networking      System Monitor   Security Control
        │              │               │
┌───────┴───────┐ ┌────┴──────┐ ┌──────┴──────┐
│ XDP           │ │ Kprobes   │ │ LSM         │
│ TC            │ │ Uprobes   │ │ Seccomp-BPF │
│ Socket Filter │ │ Tracepoint│ │ Cgroup      │
│ cgroup/skb    │ │ Perf Event│ │ Capabilities│
└───────────────┘ └───────────┘ └─────────────┘
```

*图表说明：eBPF 程序类型按应用场景分为网络处理（XDP/TC/Socket Filter）、系统监控（Kprobe/Uprobe/Tracepoint）和安全控制（LSM/Seccomp/Cgroup）三大分支。*

**典型使用案例:**

| 程序类型 | 典型应用 | 性能特点 | 复杂度 |
|----------|----------|----------|--------|
| **XDP** | DDoS防护、负载均衡 | 极高吞吐 | 中等 |
| **TC** | QoS、流量整形 | 高吞吐 | 高 |
| **Kprobe** | 性能分析、调试 | 低开销 | 低 |
| **Socket Filter** | 包捕获、监控 | 中等吞吐 | 低 |
| **cgroup** | 容器网络控制 | 中等吞吐 | 中等 |
| **LSM** | 安全策略执行 | 低开销 | 高 |

### 7.4 eBPF vs 其他可编程数据平面

| 技术 | **eBPF/XDP** | **P4** | **NPU** | **SmartNIC** |
|------|-------------|--------|---------|-------------|
| **编程模型** | C-like | P4 DSL | 汇编/C | 多种 |
| **执行位置** | 内核/驱动 | 交换机 | 网络处理器 | 网卡 |
| **性能** | 20-100 Mpps | **Tbps** | **100+ Mpps** | **200+ Mpps** |
| **灵活性** | **很高** | 中等 | 低 | 高 |
| **成本** | **极低** | 高 | 中等 | 中等 |
| **生态** | **成熟** | 发展中 | 专用 | 发展中 |

---

## 🧠 8. 一句话本质总结

**eBPF 的本质**：在内核中运行用户定义的安全代码，实现可编程的数据平面处理。

**XDP 的本质**：在数据包进入网络栈之前的最早处理点，实现线速包过滤和转发。

**核心价值**：用安全、高性能的方式扩展内核功能，无需修改内核源码或重启系统。

**设计哲学**：
- **安全第一** - 验证器保证程序不会崩溃内核
- **性能优先** - JIT编译达到接近原生代码性能  
- **可观测性** - 丰富的调试和监控能力
- **易用性** - 完整的工具链和生态系统

**应用边界**：
- ✅ **适合**：网络处理、监控、简单逻辑
- ❌ **不适合**：复杂业务逻辑、需要大量内核API的场景

**未来趋势**：从网络领域扩展到存储、调度、安全等更多内核子系统，成为内核可编程的统一接口。

---

## 📚 参考资料与进阶学习

### 官方文档
- [Linux eBPF Documentation](https://docs.kernel.org/bpf/)
- [libbpf Documentation](https://libbpf.readthedocs.io/)
- [BPF and XDP Reference Guide](https://docs.cilium.io/en/latest/bpf/)

### 重要工具
- **bpftool** - eBPF 程序和 Map 管理
- **libbpf** - eBPF 程序加载库  
- **bcc** - eBPF 开发框架
- **bpftrace** - 动态跟踪工具

### 学习路径建议
1. **基础**: 完成本文档的实验代码
2. **进阶**: 学习 Cilium、Katran 等开源项目
3. **高级**: 参与 eBPF 内核开发或编写复杂应用

### 相关技术栈
- **网络**: Cilium、Calico、Katran、Cloudflare  
- **监控**: Pixie、Falco、Tracee
- **安全**: Tetragon、Parca、LoxiLB