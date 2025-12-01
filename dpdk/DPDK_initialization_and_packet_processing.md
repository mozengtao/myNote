# DPDK Initialization and Packet Processing Guide

## Table of Contents

1. [DPDK Application Startup Flow](#1-dpdk-application-startup-flow)
2. [EAL Initialization Details](#2-eal-initialization-details)
3. [Memory Pool Initialization](#3-memory-pool-initialization)
4. [Network Port Initialization](#4-network-port-initialization)
5. [Thread Model and Packet Processing](#5-thread-model-and-packet-processing)
6. [Complete Packet Processing Flow](#6-complete-packet-processing-flow)
7. [Core Code Snippets](#7-core-code-snippets)

---

## 1. DPDK Application Startup Flow

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                        DPDK Application Startup Flow                                    │
└─────────────────────────────────────────────────────────────────────────────────────────┘

                                    ┌─────────────┐
                                    │   main()    │
                                    └──────┬──────┘
                                           │
                    ┌──────────────────────┼──────────────────────┐
                    │                      │                      │
                    ▼                      ▼                      ▼
           ┌────────────────┐    ┌────────────────┐    ┌────────────────┐
           │app_config_init │    │app_config_args │    │app_config_parse│
           │   (defaults)   │    │    _parse()    │    │  (.cfg file)   │
           └────────────────┘    └────────────────┘    └────────────────┘
                    │                      │                      │
                    └──────────────────────┼──────────────────────┘
                                           │
                                           ▼
                                  ┌────────────────┐
                                  │   app_init()   │
                                  └────────┬───────┘
                                           │
        ┌──────────────────────────────────┼──────────────────────────────────┐
        │                                  │                                  │
        ▼                                  ▼                                  ▼
┌───────────────────┐            ┌───────────────────┐            ┌───────────────────┐
│ app_common_init() │            │  thread_type_    │             │ app_common_links_ │
│                   │            │    register()    │             │     start()       │
└─────────┬─────────┘            └───────────────────┘            └───────────────────┘
          │
          │  ┌─────────────────────────────────────────────────────────────┐
          │  │                   app_common_init()                         │
          │  └─────────────────────────────────────────────────────────────┘
          │
          ├─────────────► core_map_init()     ──► CPU topology detection
          │
          ├─────────────► core_mask_init()    ──► Build CPU core mask
          │
          ├─────────────► eal_init()          ──► rte_eal_init() [CRITICAL]
          │                    │
          │                    ├──► Parse EAL arguments
          │                    ├──► Initialize huge pages
          │                    ├──► Initialize memory zones
          │                    ├──► Probe PCI/vdev devices
          │                    └──► Initialize timer subsystem
          │
          ├─────────────► mempools_init()     ──► rte_mempool_create()
          │
          └─────────────► links_init()        ──► rte_eth_dev_configure()
                               │
                               ├──► rte_eth_rx_queue_setup()
                               └──► rte_eth_tx_queue_setup()

                                           │
                                           ▼
                                  ┌────────────────┐
                                  │   app_run()    │
                                  └────────┬───────┘
                                           │
                                           ▼
                              ┌────────────────────────┐
                              │rte_eal_mp_remote_launch│
                              │   (thread_run)         │
                              └────────────┬───────────┘
                                           │
                    ┌──────────────────────┼──────────────────────┐
                    ▼                      ▼                      ▼
              ┌──────────┐           ┌──────────┐           ┌──────────┐
              │ lcore 0  │           │ lcore 1  │           │ lcore N  │
              │ (main)   │           │ (worker) │           │ (worker) │
              └──────────┘           └──────────┘           └──────────┘
                    │                      │                      │
                    ▼                      ▼                      ▼
              ┌──────────┐           ┌──────────┐           ┌──────────┐
              │  while   │           │  while   │           │  while   │
              │(!force_  │           │(!force_  │           │(!force_  │
              │  quit)   │           │  quit)   │           │  quit)   │
              │ process  │           │ process  │           │ process  │
              │ packets  │           │ packets  │           │ packets  │
              └──────────┘           └──────────┘           └──────────┘
```

**说明**：
- DPDK 应用程序启动分为三个主要阶段：配置解析、初始化、运行
- `app_config_init()` 设置默认配置参数
- `app_config_args_parse()` 解析命令行参数
- `app_config_parse()` 解析 `.cfg` 配置文件
- `app_init()` 是核心初始化入口，调用 `app_common_init()` 完成 DPDK 环境初始化
- `app_run()` 启动所有工作线程进入数据包处理主循环

### 1.1 Main Function Code

```c
/* main.c */
int
main(int argc, char **argv)
{
    int lcoreid = 0;

    memset(&app, 0, sizeof(struct app_params));

    /* Init app config to defaults */
    app_config_init(&app);

    /* Parse command line arguments */
    app_config_args_parse(&app, argc, argv);

    /* Parse application config file, .cfg file */
    app_config_parse(&app, app.config_file);

    /* Set global debug level */
    rte_log_set_global_level(app.log_level);

    /* Initialize the application */
    app_init(&app);

    /* Run all the application's threads */
    app_run(&app);

    /* ..and wait and sleep here */
    while (!force_quit)
        sleep(1);

    /* Wait for threads to finish */
    RTE_LCORE_FOREACH_WORKER(lcoreid) {
        if (rte_eal_wait_lcore(lcoreid) < 0)
            return -1;
    }

    app_shutdown(&app);
    return 0;
}
```

---

## 2. EAL Initialization Details

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         EAL (Environment Abstraction Layer) Init                        │
└─────────────────────────────────────────────────────────────────────────────────────────┘

                                  ┌──────────────┐
                                  │  eal_init()  │
                                  └───────┬──────┘
                                          │
                    ┌─────────────────────┼─────────────────────┐
                    │                     │                     │
                    ▼                     ▼                     ▼
        ┌───────────────────┐  ┌───────────────────┐  ┌───────────────────┐
        │  Build EAL Args   │  │  Configure Cores  │  │  Configure Memory │
        └─────────┬─────────┘  └─────────┬─────────┘  └─────────┬─────────┘
                  │                      │                      │
                  ▼                      ▼                      ▼
        ┌───────────────────┐  ┌───────────────────┐  ┌───────────────────┐
        │ --lcores=0@1,1@2  │  │ --main-lcore=1    │  │ --socket-mem=1024 │
        │ -c <core_mask>    │  │ -n <channels>     │  │ --huge-dir=/mnt/  │
        └───────────────────┘  └───────────────────┘  └───────────────────┘
                  │                      │                      │
                  └──────────────────────┼──────────────────────┘
                                         │
                                         ▼
                              ┌─────────────────────┐
                              │   rte_eal_init()    │
                              └──────────┬──────────┘
                                         │
          ┌──────────────────────────────┼──────────────────────────────┐
          │                              │                              │
          ▼                              ▼                              ▼
┌─────────────────────┐      ┌─────────────────────┐      ┌─────────────────────┐
│   Memory Init       │      │   Device Probe      │      │   Thread Init       │
├─────────────────────┤      ├─────────────────────┤      ├─────────────────────┤
│ - Huge page mapping │      │ - PCI bus scan      │      │ - lcore mapping     │
│ - Memory zones      │      │ - Driver binding    │      │ - Thread affinity   │
│ - Heap setup        │      │ - vdev creation     │      │ - TLS setup         │
└─────────────────────┘      └─────────────────────┘      └─────────────────────┘


                    ┌─────────────────────────────────────────────────────┐
                    │              EAL Arguments Example                  │
                    ├─────────────────────────────────────────────────────┤
                    │                                                     │
                    │  ./vcmtsd                                           │
                    │      --lcores='0@23,1@24,2@25'                       │
                    │      --main-lcore=1                                 │
                    │      -n 4                                           │
                    │      --socket-mem=4096                              │
                    │      --huge-dir=/dev/hugepages                      │
                    │      --file-prefix=vcmts                            │
                    │      --allow=0000:3b:00.0                           │
                    │      --vdev=net_tap0,iface=rphy_tap                 │
                    │      --log-level=7                                  │
                    │      --                                             │
                    │                                                     │
                    └─────────────────────────────────────────────────────┘
```

**说明**：
- EAL 是 DPDK 的环境抽象层，负责底层资源初始化
- `--lcores` 参数指定逻辑核心到物理核心的映射关系
- `--socket-mem` 指定每个 NUMA 节点分配的大页内存大小（MB）
- `--allow` 参数指定允许 DPDK 使用的 PCI 设备地址
- `--vdev` 参数创建虚拟设备（如 TAP 接口）
- `rte_eal_init()` 返回后，DPDK 环境准备就绪

### 2.1 EAL Init Code

```c
/* app_common.c - eal_init() */
static void
eal_init(void)
{
    char buffer[256];
    uint32_t n_args = 0;

    /* Build application name */
    eal_argv[n_args++] = strdup(common_params->app_name);

    /* Build --lcores argument */
    snprintf(buffer, sizeof(buffer), "--lcores=%s", p->lcores);
    eal_argv[n_args++] = strdup(buffer);

    /* Build --main-lcore argument */
    snprintf(buffer, sizeof(buffer), "--main-lcore=1");
    eal_argv[n_args++] = strdup(buffer);

    /* Build memory channels argument */
    if (p->channels_present) {
        snprintf(buffer, sizeof(buffer), "-n%" PRIu32, p->channels);
        eal_argv[n_args++] = strdup(buffer);
    }

    /* Build socket memory argument */
    if (p->socket_mem) {
        snprintf(buffer, sizeof(buffer), "--socket-mem=%s", p->socket_mem);
        eal_argv[n_args++] = strdup(buffer);
    }

    /* Build huge page directory argument */
    if (p->huge_dir) {
        snprintf(buffer, sizeof(buffer), "--huge-dir=%s", p->huge_dir);
        eal_argv[n_args++] = strdup(buffer);
    }

    /* Build PCI allow list */
    for (i = 0; i < APP_COMMON_MAX_LINKS; i++) {
        if (p->pci_allow[i] == NULL)
            break;
        snprintf(buffer, sizeof(buffer), "--allow=%s", p->pci_allow[i]);
        eal_argv[n_args++] = strdup(buffer);
    }

    /* Build virtual devices */
    for (i = 0; i < APP_COMMON_MAX_LINKS; i++) {
        if (p->vdev[i] == NULL)
            break;
        snprintf(buffer, sizeof(buffer), "--vdev=%s", p->vdev[i]);
        eal_argv[n_args++] = strdup(buffer);
    }

    /* Terminate argument list */
    snprintf(buffer, sizeof(buffer), "--");
    eal_argv[n_args++] = strdup(buffer);
    eal_argc = n_args;

    /* Call DPDK EAL initialization */
    status = rte_eal_init(eal_argc, eal_argv);
    if (status < 0)
        rte_panic("EAL init error\n");

    /* Initialize timer subsystem */
    rte_timer_subsystem_init();
}
```

---

## 3. Memory Pool Initialization

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                           Memory Pool (Mempool) Initialization                          │
└─────────────────────────────────────────────────────────────────────────────────────────┘

                              ┌─────────────────────┐
                              │   mempools_init()   │
                              └──────────┬──────────┘
                                         │
                                         ▼
                    ┌────────────────────────────────────────┐
                    │     For each mempool in config:        │
                    │                                        │
                    │  ┌──────────────────────────────────┐  │
                    │  │  mempool_params[i]               │  │
                    │  │  - name: "MEMPOOL0"              │  │
                    │  │  - pool_size: 8192               │  │
                    │  │  - buffer_size: 2048             │  │
                    │  │  - cache_size: 256               │  │
                    │  │  - cpu_socket_id: 0              │  │
                    │  └──────────────────────────────────┘  │
                    └────────────────────┬───────────────────┘
                                         │
                    ┌────────────────────┴────────────────────┐
                    │                                         │
                    ▼                                         ▼
        ┌───────────────────────────┐           ┌───────────────────────────┐
        │   RING Type Mempool       │           │   STACK Type Mempool      │
        │   rte_mempool_create()    │           │   rte_mempool_create_     │
        │                           │           │   empty() + set_ops       │
        └─────────────┬─────────────┘           └─────────────┬─────────────┘
                      │                                       │
                      └───────────────────┬───────────────────┘
                                          │
                                          ▼
                          ┌───────────────────────────────┐
                          │      Mempool Structure        │
                          ├───────────────────────────────┤
                          │                               │
                          │   ┌─────────────────────────┐ │
                          │   │     Mempool Header      │ │
                          │   │  - name, size, flags    │ │
                          │   └─────────────────────────┘ │
                          │              │                │
                          │              ▼                │
                          │   ┌─────────────────────────┐ │
                          │   │    Per-lcore Cache      │ │
                          │   │  (fast local access)    │ │
                          │   └─────────────────────────┘ │
                          │              │                │
                          │              ▼                │
                          │   ┌─────────────────────────┐ │
                          │   │     Ring/Stack          │ │
                          │   │  (shared pool backend)  │ │
                          │   └─────────────────────────┘ │
                          │              │                │
                          │              ▼                │
                          │   ┌─────────────────────────┐ │
                          │   │   mbuf Objects Array    │ │
                          │   │  [mbuf0][mbuf1]...[mbufN]│ │
                          │   └─────────────────────────┘ │
                          │                               │
                          └───────────────────────────────┘


                    ┌─────────────────────────────────────────────────────┐
                    │                mbuf Structure                       │
                    ├─────────────────────────────────────────────────────┤
                    │                                                     │
                    │   ┌─────────────────────────────────────────────┐   │
                    │   │              struct rte_mbuf                │   │
                    │   ├─────────────────────────────────────────────┤   │
                    │   │  buf_addr     ──► pointer to data buffer    │   │
                    │   │  data_off     ──► offset to start of data   │   │
                    │   │  pkt_len      ──► total packet length       │   │
                    │   │  data_len     ──► data length in segment    │   │
                    │   │  nb_segs      ──► number of segments        │   │
                    │   │  port         ──► input port                │   │
                    │   │  ol_flags     ──► offload flags             │   │
                    │   │  next         ──► next segment (chaining)   │   │
                    │   │  pool         ──► originating mempool       │   │
                    │   └─────────────────────────────────────────────┘   │
                    │                         │                           │
                    │                         ▼                           │
                    │   ┌─────────────────────────────────────────────┐   │
                    │   │              Data Buffer                    │   │
                    │   │  [headroom][packet data][tailroom]          │   │
                    │   └─────────────────────────────────────────────┘   │
                    │                                                     │
                    └─────────────────────────────────────────────────────┘
```

**说明**：
- Mempool 是 DPDK 的高性能内存池，用于分配 mbuf（消息缓冲区）
- 每个 lcore 有独立的本地缓存，减少锁竞争
- Ring 类型使用无锁环形队列实现，适合多生产者多消费者场景
- Stack 类型使用栈实现，适合单生产者单消费者场景
- mbuf 是 DPDK 数据包的载体，包含元数据和实际数据缓冲区

### 3.1 Mempool Init Code

```c
/* app_common.c - mempools_init() */
static void
mempools_init(void)
{
    uint32_t i;

    for (i = 0; i < common_params->n_mempools; i++) {
        struct app_common_mempool_params *p =
                    &common_params->mempool_params[i];
        uint32_t full_buffer_size = p->buffer_size +
                        APP_COMMON_MEMPOOL_OVERHEAD;

        RTE_LOG(INFO, APP_COMMON,
            "Initializing %s: pool size %u, buffer size %u, cache size %u\n",
            p->name, p->pool_size, p->buffer_size, p->cache_size);

        if (p->type == APP_COMMON_MEMPOOL_TYPE_RING) {
            /* Ring-based mempool */
            p->mempool = rte_mempool_create(
                    p->name,
                    p->pool_size,
                    full_buffer_size,
                    p->cache_size,
                    sizeof(struct rte_pktmbuf_pool_private),
                    rte_pktmbuf_pool_init,
                    NULL,
                    rte_pktmbuf_init,
                    NULL,
                    p->cpu_socket_id,
                    0);
        } else {
            /* Stack-based mempool */
            p->mempool = rte_mempool_create_empty(
                    p->name,
                    p->pool_size,
                    full_buffer_size,
                    p->cache_size,
                    sizeof(struct rte_pktmbuf_pool_private),
                    p->cpu_socket_id,
                    0);

            rte_mempool_set_ops_byname(p->mempool, "stack", NULL);
            rte_pktmbuf_pool_init(p->mempool, NULL);
            rte_mempool_populate_default(p->mempool);
            rte_mempool_obj_iter(p->mempool, rte_pktmbuf_init, NULL);
        }

        if (p->mempool == NULL)
            rte_panic("%s init error\n", p->name);
    }
}
```

---

## 4. Network Port Initialization

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Network Port (Link) Initialization                              │
└─────────────────────────────────────────────────────────────────────────────────────────┘

                              ┌─────────────────────┐
                              │    links_init()     │
                              └──────────┬──────────┘
                                         │
                    ┌────────────────────┴────────────────────┐
                    │     For each link in config:           │
                    └────────────────────┬────────────────────┘
                                         │
                                         ▼
                          ┌──────────────────────────────┐
                          │   rte_eth_dev_info_get()     │
                          │   (Get device capabilities)  │
                          └──────────────┬───────────────┘
                                         │
                                         ▼
                          ┌──────────────────────────────┐
                          │   rte_eth_dev_configure()    │
                          │   (Configure port)           │
                          │   - n_rxq: number of RX queues│
                          │   - n_txq: number of TX queues│
                          │   - eth_conf: port config    │
                          └──────────────┬───────────────┘
                                         │
                    ┌────────────────────┴────────────────────┐
                    │                                         │
                    ▼                                         ▼
        ┌───────────────────────────┐           ┌───────────────────────────┐
        │  rte_eth_rx_queue_setup() │           │  rte_eth_tx_queue_setup() │
        │  For each RX queue:       │           │  For each TX queue:       │
        │  - queue_id               │           │  - queue_id               │
        │  - nb_desc (ring size)    │           │  - nb_desc (ring size)    │
        │  - socket_id              │           │  - socket_id              │
        │  - rx_conf                │           │  - tx_conf                │
        │  - mempool                │           └───────────────────────────┘
        └───────────────────────────┘
                    │                                         │
                    └────────────────────┬────────────────────┘
                                         │
                                         ▼
                          ┌──────────────────────────────┐
                          │   rte_eth_dev_start()        │
                          │   (Start the port)           │
                          └──────────────────────────────┘


                    ┌─────────────────────────────────────────────────────┐
                    │              Port Configuration                     │
                    ├─────────────────────────────────────────────────────┤
                    │                                                     │
                    │   struct rte_eth_conf {                             │
                    │       .rxmode = {                                   │
                    │           .mq_mode = RTE_ETH_MQ_RX_NONE,            │
                    │           .mtu = 9000,  /* Jumbo frames */          │
                    │       },                                            │
                    │       .txmode = {                                   │
                    │           .mq_mode = RTE_ETH_MQ_TX_NONE,            │
                    │       },                                            │
                    │   };                                                │
                    │                                                     │
                    │   RX Offloads:                                      │
                    │   - RTE_ETH_RX_OFFLOAD_KEEP_CRC                     │
                    │                                                     │
                    │   TX Offloads:                                      │
                    │   - RTE_ETH_TX_OFFLOAD_MULTI_SEGS                   │
                    │   - RTE_ETH_TX_OFFLOAD_IPV4_CKSUM                   │
                    │                                                     │
                    └─────────────────────────────────────────────────────┘


                    ┌─────────────────────────────────────────────────────┐
                    │              Queue Configuration                    │
                    ├─────────────────────────────────────────────────────┤
                    │                                                     │
                    │   RX Queue Config (rte_eth_rxconf):                 │
                    │   - rx_thresh.pthresh = 8                           │
                    │   - rx_thresh.hthresh = 8                           │
                    │   - rx_thresh.wthresh = 4                           │
                    │   - rx_free_thresh = 64                             │
                    │   - rx_drop_en = 0                                  │
                    │                                                     │
                    │   TX Queue Config (rte_eth_txconf):                 │
                    │   - tx_thresh.pthresh = 36                          │
                    │   - tx_thresh.hthresh = 0                           │
                    │   - tx_thresh.wthresh = 0                           │
                    │                                                     │
                    └─────────────────────────────────────────────────────┘
```

**说明**：
- `rte_eth_dev_info_get()` 获取网卡设备信息和能力
- `rte_eth_dev_configure()` 配置端口的队列数量和模式
- `rte_eth_rx_queue_setup()` 设置接收队列，关联 mempool
- `rte_eth_tx_queue_setup()` 设置发送队列
- `rte_eth_dev_start()` 启动端口，开始收发数据包

### 4.1 Link Init Code

```c
/* app_common.c - links_init() */
static void
links_init(int addresses_only)
{
    uint32_t i;

    for (i = 0; i < common_params->n_links; i++) {
        struct rte_eth_dev_info dev_info;
        struct rte_eth_conf eth_conf = default_eth_conf;
        struct app_common_link_params *p_link =
                        &common_params->link_params[i];
        uint32_t n_rxq, n_txq, j;

        if (!p_link->configured)
            continue;

        /* Get device info */
        rte_eth_dev_info_get(p_link->port_id, &dev_info);

        n_rxq = utils_link_n_rxq_get(common_params, p_link);
        n_txq = utils_link_n_txq_get(common_params, p_link);

        /* Configure port */
        eth_conf.rxmode.offloads = rx_offloads_get(p_link, &dev_info);
        eth_conf.txmode.offloads = tx_offloads_get(p_link, &dev_info);

        rte_eth_dev_configure(p_link->port_id, n_rxq, n_txq, &eth_conf);

        /* Setup RX queues */
        for (j = 0; j < common_params->n_rxq; j++) {
            struct app_common_rxq_params *p_rxq =
                        &common_params->rxq_params[j];

            rte_eth_rx_queue_setup(
                    p_link->port_id,
                    rxq_queue_id,
                    p_rxq->size,
                    cpu_socket_id_get(p_link->port_id),
                    &eth_rxconf,
                    common_params->mempool_params[p_rxq->mempool_id].mempool);
        }

        /* Setup TX queues */
        for (j = 0; j < common_params->n_txq; j++) {
            struct app_common_txq_params *p_txq =
                        &common_params->txq_params[j];

            rte_eth_tx_queue_setup(
                    p_link->port_id,
                    txq_queue_id,
                    p_txq->size,
                    cpu_socket_id_get(p_link->port_id),
                    &eth_txconf);

            /* Initialize TX buffer */
            p_txq->tx_buffer = rte_zmalloc_socket("tx_buffer",
                    RTE_ETH_TX_BUFFER_SIZE(p_txq->burst),
                    0,
                    cpu_socket_id_get(p_link->port_id));

            rte_eth_tx_buffer_init(p_txq->tx_buffer, p_txq->burst);
        }
    }
}

/* Start all links */
void
app_common_links_start(void)
{
    for (i = 0; i < common_params->n_links; i++) {
        p_link = &common_params->link_params[i];

        if (!p_link->configured)
            continue;

        /* Start the port */
        rte_eth_dev_start(p_link->port_id);
        p_link->state = 1;
    }
}
```

---

## 5. Thread Model and Packet Processing

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                           DPDK Thread Model                                             │
└─────────────────────────────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────────────────────────────────────┐
                    │                Thread Registration                  │
                    └─────────────────────────────────────────────────────┘

                              ┌─────────────────────┐
                              │ thread_type_register│
                              └──────────┬──────────┘
                                         │
                    ┌────────────────────┴────────────────────┐
                    │                                         │
                    ▼                                         ▼
        ┌───────────────────────────┐           ┌───────────────────────────┐
        │   struct thread_type      │           │   struct thread_ops       │
        │   - name: "US_LMUM"       │           │   - f_init()              │
        │   - thread_ops ──────────────────────►│   - f_run()               │
        │                           │           │   - f_free()              │
        └───────────────────────────┘           └───────────────────────────┘


                    ┌─────────────────────────────────────────────────────┐
                    │                Thread Launch                        │
                    └─────────────────────────────────────────────────────┘

                              ┌─────────────────────┐
                              │ rte_eal_mp_remote_  │
                              │     launch()        │
                              └──────────┬──────────┘
                                         │
                    ┌────────────────────┴────────────────────┐
                    │                                         │
                    ▼                                         ▼
        ┌───────────────────────────┐           ┌───────────────────────────┐
        │   Main lcore (lcore 0)    │           │   Worker lcores           │
        │   - Control plane         │           │   - Data plane            │
        │   - Management tasks      │           │   - Packet processing     │
        │   - sleep() in main loop  │           │   - Polling mode          │
        └───────────────────────────┘           └───────────────────────────┘


                    ┌─────────────────────────────────────────────────────┐
                    │              Thread Lifecycle                       │
                    ├─────────────────────────────────────────────────────┤
                    │                                                     │
                    │   ┌─────────────┐                                   │
                    │   │   f_init()  │  ◄── Initialize thread context   │
                    │   └──────┬──────┘       - Setup RX/TX ports        │
                    │          │              - Allocate resources        │
                    │          ▼                                          │
                    │   ┌─────────────┐                                   │
                    │   │   f_run()   │  ◄── Main processing loop        │
                    │   │             │       while (!force_quit) {       │
                    │   │  ┌───────┐  │           process_packets();      │
                    │   │  │ loop  │  │       }                           │
                    │   │  └───────┘  │                                   │
                    │   └──────┬──────┘                                   │
                    │          │                                          │
                    │          ▼                                          │
                    │   ┌─────────────┐                                   │
                    │   │   f_free()  │  ◄── Cleanup and shutdown        │
                    │   └─────────────┘       - Release resources         │
                    │                         - Flush TX buffers          │
                    │                                                     │
                    └─────────────────────────────────────────────────────┘
```

**说明**：
- DPDK 使用 lcore（逻辑核心）模型，每个线程绑定到特定 CPU 核心
- 线程类型通过 `thread_type_register()` 注册到框架
- 每个线程类型定义三个回调函数：`f_init()`、`f_run()`、`f_free()`
- `rte_eal_mp_remote_launch()` 在所有 lcore 上启动线程
- 数据面线程使用轮询模式（Polling Mode）处理数据包，避免中断开销

### 5.1 Thread Launch Code

```c
/* app_common.c - threads_launch_no_wait() */
static void
threads_launch_no_wait(void *cb_arg, void *fun)
{
    rte_eal_mp_remote_launch(fun, cb_arg, CALL_MAIN);
}

/* thread_run() - dispatches to registered thread handlers */
static int
thread_run(void *cb_arg)
{
    uint32_t t_id;
    unsigned int lcore_id;

    lcore_id = rte_lcore_id();

    for (t_id = 0; t_id < common_params->n_threads; t_id++) {
        struct app_common_thread_params *params =
                    &common_params->thread_params[t_id];
        struct app_common_thread_type *ttype;

        ttype = thread_type_find(params->type);
        if (ttype == NULL)
            continue;

        if (lcore_id == params->lcore_id)
            ttype->thread_ops->f_run(cb_arg);
    }

    return 0;
}
```

---

## 6. Complete Packet Processing Flow

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Complete Packet Processing Flow                                 │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                              NETWORK                                                │
    │                                 │                                                   │
    │                                 ▼                                                   │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                             NIC                                             │   │
    │   │   ┌─────────────────────────────────────────────────────────────────────┐   │   │
    │   │   │                    Hardware RX Queues                               │   │   │
    │   │   │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │   │
    │   │   │   │  RXQ 0   │  │  RXQ 1   │  │  RXQ 2   │  │  RXQ N   │            │   │   │
    │   │   │   │  (Ring)  │  │  (Ring)  │  │  (Ring)  │  │  (Ring)  │            │   │   │
    │   │   │   └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘            │   │   │
    │   │   │        │             │             │             │                  │   │   │
    │   │   └────────┼─────────────┼─────────────┼─────────────┼──────────────────┘   │   │
    │   └────────────┼─────────────┼─────────────┼─────────────┼──────────────────────┘   │
    └────────────────┼─────────────┼─────────────┼─────────────┼──────────────────────────┘
                     │             │             │             │
                     │   (DMA to Huge Pages - Zero Copy)       │
                     │             │             │             │
                     ▼             ▼             ▼             ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                           HUGE PAGES MEMORY                                         │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                         MEMPOOL                                             │   │
    │   │   ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐           │   │
    │   │   │  mbuf   │  │  mbuf   │  │  mbuf   │  │  mbuf   │  │  mbuf   │  ...      │   │
    │   │   │[pkt data]│ │[pkt data]│ │[pkt data]│ │[pkt data]│ │[pkt data]│          │   │
    │   │   └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘           │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                     │             │             │             │
                     │   (Polling - No Interrupt)              │
                     ▼             ▼             ▼             ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                           USER SPACE APPLICATION                                    │
    │                                                                                     │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                    Worker Thread (lcore N)                                  │   │
    │   │                                                                             │   │
    │   │   ┌─────────────────────────────────────────────────────────────────────┐   │   │
    │   │   │                    Main Loop                                        │   │   │
    │   │   │                                                                     │   │   │
    │   │   │   while (!force_quit) {                                             │   │   │
    │   │   │                                                                     │   │   │
    │   │   │       /* 1. Receive packets from RX queue */                        │   │   │
    │   │   │       nb_rx = rte_eth_rx_burst(port_id, queue_id,                   │   │   │
    │   │   │                                rx_pkts, MAX_PKT_BURST);             │   │   │
    │   │   │                                                                     │   │   │
    │   │   │       /* 2. Process each packet */                                  │   │   │
    │   │   │       for (i = 0; i < nb_rx; i++) {                                 │   │   │
    │   │   │           mbuf = rx_pkts[i];                                        │   │   │
    │   │   │                                                                     │   │   │
    │   │   │           /* Parse headers */                                       │   │   │
    │   │   │           eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *); │   │   │
    │   │   │           ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);            │   │   │
    │   │   │                                                                     │   │   │
    │   │   │           /* Application logic */                                   │   │   │
    │   │   │           process_packet(mbuf);                                     │   │   │
    │   │   │                                                                     │   │   │
    │   │   │           /* Prepare for TX */                                      │   │   │
    │   │   │           tx_pkts[tx_count++] = mbuf;                               │   │   │
    │   │   │       }                                                             │   │   │
    │   │   │                                                                     │   │   │
    │   │   │       /* 3. Transmit packets */                                     │   │   │
    │   │   │       nb_tx = rte_eth_tx_burst(port_id, queue_id,                   │   │   │
    │   │   │                                tx_pkts, tx_count);                  │   │   │
    │   │   │                                                                     │   │   │
    │   │   │       /* 4. Free unsent packets */                                  │   │   │
    │   │   │       for (i = nb_tx; i < tx_count; i++)                            │   │   │
    │   │   │           rte_pktmbuf_free(tx_pkts[i]);                             │   │   │
    │   │   │   }                                                                 │   │   │
    │   │   │                                                                     │   │   │
    │   │   └─────────────────────────────────────────────────────────────────────┘   │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                     │             │             │             │
                     │   (DMA from Huge Pages - Zero Copy)     │
                     ▼             ▼             ▼             ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                             NIC                                                     │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                    Hardware TX Queues                                       │   │
    │   │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐                    │   │
    │   │   │  TXQ 0   │  │  TXQ 1   │  │  TXQ 2   │  │  TXQ N   │                    │   │
    │   │   │  (Ring)  │  │  (Ring)  │  │  (Ring)  │  │  (Ring)  │                    │   │
    │   │   └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘                    │   │
    │   │        │             │             │             │                          │   │
    │   └────────┼─────────────┼─────────────┼─────────────┼──────────────────────────┘   │
    └────────────┼─────────────┼─────────────┼─────────────┼──────────────────────────────┘
                 │             │             │             │
                 ▼             ▼             ▼             ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                              NETWORK                                                │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **网卡 DMA**：网卡通过 DMA 直接将数据包写入大页内存中的 mbuf
- **零拷贝**：数据包在整个处理过程中不需要内存拷贝
- **轮询模式**：应用程序主动轮询 RX 队列，无需中断
- **批量处理**：`rte_eth_rx_burst()` 和 `rte_eth_tx_burst()` 批量收发数据包
- **用户空间处理**：所有数据包处理在用户空间完成，避免内核上下文切换

---

## 7. Core Code Snippets

### 7.1 Packet Receive (RX Burst)

```c
/* Receive a burst of packets from RX queue */
#define MAX_PKT_BURST 32

struct rte_mbuf *rx_pkts[MAX_PKT_BURST];
uint16_t nb_rx;

/* Poll for received packets */
nb_rx = rte_eth_rx_burst(
    port_id,      /* Port ID */
    queue_id,     /* Queue ID */
    rx_pkts,      /* Array to store received mbufs */
    MAX_PKT_BURST /* Maximum number of packets to receive */
);

if (nb_rx == 0)
    return; /* No packets received */

/* Process received packets */
for (uint16_t i = 0; i < nb_rx; i++) {
    struct rte_mbuf *mbuf = rx_pkts[i];
    
    /* Access packet data */
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    
    /* Get packet length */
    uint32_t pkt_len = rte_pktmbuf_pkt_len(mbuf);
    
    /* Process packet... */
}
```

### 7.2 Packet Transmit (TX Burst)

```c
/* Transmit a burst of packets to TX queue */
struct rte_mbuf *tx_pkts[MAX_PKT_BURST];
uint16_t nb_tx, tx_count = 0;

/* Prepare packets for transmission */
for (uint16_t i = 0; i < nb_rx; i++) {
    struct rte_mbuf *mbuf = rx_pkts[i];
    
    /* Modify packet if needed... */
    
    tx_pkts[tx_count++] = mbuf;
}

/* Send packets */
nb_tx = rte_eth_tx_burst(
    port_id,      /* Port ID */
    queue_id,     /* Queue ID */
    tx_pkts,      /* Array of mbufs to send */
    tx_count      /* Number of packets to send */
);

/* Free unsent packets */
if (unlikely(nb_tx < tx_count)) {
    for (uint16_t i = nb_tx; i < tx_count; i++) {
        rte_pktmbuf_free(tx_pkts[i]);
    }
}
```

### 7.3 Buffered TX (with TX Buffer)

```c
/* Using TX buffer for automatic batching and flushing */
struct rte_eth_dev_tx_buffer *tx_buffer;

/* Initialize TX buffer */
tx_buffer = rte_zmalloc_socket("tx_buffer",
                RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST),
                0, socket_id);
rte_eth_tx_buffer_init(tx_buffer, MAX_PKT_BURST);

/* In processing loop - buffer packets */
for (uint16_t i = 0; i < nb_rx; i++) {
    struct rte_mbuf *mbuf = rx_pkts[i];
    
    /* Buffer the packet - automatically sends when buffer is full */
    rte_eth_tx_buffer(port_id, queue_id, tx_buffer, mbuf);
}

/* Flush remaining packets in buffer */
rte_eth_tx_buffer_flush(port_id, queue_id, tx_buffer);
```

### 7.4 Complete Main Loop Example

```c
/* thread_upstream.c - thread_upstream_lmum_run() */
static int
thread_upstream_lmum_run(__rte_unused void *arg)
{
    char *name, *type;
    unsigned int lcore_id, socket_id;
    uint32_t pkts_num;

    name = tp_us_lmum->name;
    type = tp_us_lmum->type;

    lcore_id = rte_lcore_id();
    socket_id = rte_socket_id();

    VCMTS_LOG_SAFE(INFO, VCMTS_US,
        "%s (%s): Entering main loop on lcore %u (socket %u)\n",
        name, type, lcore_id, socket_id);

    /* Enter main processing loop */
    while (likely(!force_quit)) {
        /* Lower MAC processing - receive and decode */
        pkts_num = us_lower_mac_run();
        
        /* Upper MAC processing - forward and transmit */
        us_upper_mac_run(pkts_num);
    }

    /* Cleanup - flush remaining TX buffers */
    rte_eth_tx_buffer_flush(RTE_PER_LCORE(tx_port_id),
                RTE_PER_LCORE(tx_queue_id),
                RTE_PER_LCORE(tx_eth_buf));

    return 0;
}
```

### 7.5 mbuf Operations

```c
/* Common mbuf operations */

/* Allocate mbuf from mempool */
struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mempool);

/* Get pointer to packet data */
void *data = rte_pktmbuf_mtod(mbuf, void *);

/* Get packet data with offset */
struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod_offset(mbuf, 
                            struct rte_ipv4_hdr *,
                            sizeof(struct rte_ether_hdr));

/* Get packet length */
uint32_t pkt_len = rte_pktmbuf_pkt_len(mbuf);
uint16_t data_len = rte_pktmbuf_data_len(mbuf);

/* Prepend data (add header) */
char *new_data = rte_pktmbuf_prepend(mbuf, header_len);

/* Append data (add trailer) */
char *tail = rte_pktmbuf_append(mbuf, trailer_len);

/* Adjust data offset */
char *adj = rte_pktmbuf_adj(mbuf, bytes_to_skip);

/* Trim data from end */
int ret = rte_pktmbuf_trim(mbuf, bytes_to_remove);

/* Clone mbuf (reference copy) */
struct rte_mbuf *clone = rte_pktmbuf_clone(mbuf, clone_pool);

/* Free mbuf back to mempool */
rte_pktmbuf_free(mbuf);
```

---

## 8. Summary: DPDK Performance Advantages

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                     DPDK vs Traditional Kernel Stack                                    │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Traditional Kernel Network Stack:
    
    ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
    │   NIC    │────►│  Driver  │────►│  Kernel  │────►│   App    │
    │          │     │(interrupt)│    │  Stack   │     │(syscall) │
    └──────────┘     └──────────┘     └──────────┘     └──────────┘
         │                │                │                │
         │   Hardware     │   Context      │   Memory       │   System
         │   Interrupt    │   Switch       │   Copy         │   Call
         │                │                │                │
         └────────────────┴────────────────┴────────────────┘
                          OVERHEAD SOURCES


    DPDK User Space Processing:
    
    ┌──────────┐     ┌──────────────────────────────────────────┐
    │   NIC    │────►│              User Space App              │
    │  (PMD)   │     │  (Polling Mode - No Interrupt)           │
    └──────────┘     └──────────────────────────────────────────┘
         │                              │
         │   DMA to                     │   Zero Copy
         │   Huge Pages                 │   Processing
         │                              │
         └──────────────────────────────┘
                   MINIMAL OVERHEAD


    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   DPDK Key Optimizations:                                                           │
    │                                                                                     │
    │   1. Polling Mode Driver (PMD)                                                      │
    │      - No interrupt overhead                                                        │
    │      - Dedicated CPU cores for packet processing                                    │
    │                                                                                     │
    │   2. Huge Pages                                                                     │
    │      - Reduced TLB misses                                                           │
    │      - Contiguous physical memory                                                   │
    │                                                                                     │
    │   3. Zero Copy                                                                      │
    │      - DMA directly to/from user space buffers                                      │
    │      - No kernel buffer copies                                                      │
    │                                                                                     │
    │   4. Lockless Data Structures                                                       │
    │      - Ring buffers for queues                                                      │
    │      - Per-lcore caches for mempools                                                │
    │                                                                                     │
    │   5. Batch Processing                                                               │
    │      - Process multiple packets per function call                                   │
    │      - Amortize function call overhead                                              │
    │                                                                                     │
    │   6. CPU Affinity                                                                   │
    │      - Threads pinned to specific cores                                             │
    │      - No context switching                                                         │
    │      - Better cache utilization                                                     │
    │                                                                                     │
    │   7. NUMA Awareness                                                                 │
    │      - Memory allocated on local NUMA node                                          │
    │      - Reduced memory access latency                                                │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **轮询模式驱动 (PMD)**：消除中断开销，CPU 专用于数据包处理
- **大页内存**：减少 TLB 未命中，提供连续物理内存
- **零拷贝**：DMA 直接访问用户空间缓冲区，无内核缓冲区拷贝
- **无锁数据结构**：使用环形缓冲区和每核心缓存减少锁竞争
- **批量处理**：每次函数调用处理多个数据包，分摊开销
- **CPU 亲和性**：线程绑定到特定核心，无上下文切换，更好的缓存利用
- **NUMA 感知**：内存分配在本地 NUMA 节点，减少内存访问延迟

---

## 9. Configuration Example

```ini
; vcmts.cfg - DPDK Application Configuration Example

[EAL]
lcores = 0@23,1@24,2@25
n = 4                           ; Memory channels
socket_mem = 4096               ; Memory per NUMA node (MB)
huge_dir = /dev/hugepages
file_prefix = vcmts
log_level = 7

[MEMPOOL0]
pool_size = 8192
buffer_size = 2048
cache_size = 256
cpu = 0

[LINK0]
; RPHY VF
promisc = no
rx_offloads_override = 0x0

[LINK1]
; NSI VF
promisc = no

[RXQ0.0]
size = 1024
burst = 32
mempool = MEMPOOL0

[TXQ0.0]
size = 1024
burst = 32
dropless = no

[THREAD0]
type = US_LMUM
core = s0c0
pktq_in = RXQ0.0 RXQ1.0 RXQ2.0
pktq_out = TXQ1.0 TXQ0.0 TXQ2.0 TXQ4.0
```

**说明**：
- `[EAL]` 部分配置 DPDK 环境抽象层参数
- `[MEMPOOL]` 部分配置内存池
- `[LINK]` 部分配置网络端口
- `[RXQ]` 和 `[TXQ]` 部分配置收发队列
- `[THREAD]` 部分配置工作线程及其输入输出队列

