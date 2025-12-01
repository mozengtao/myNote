## DPDK Packet Processing Pipeline – Overview

DPDK (Data Plane Development Kit) is a set of userspace libraries and drivers for fast packet processing.  
Instead of sending packets through the kernel networking stack, DPDK lets an application **directly receive and transmit packets in user space**, typically using **polling I/O on NIC queues** and **hugepage-backed memory** for high performance.

Key ideas:

- **User-space drivers** (e.g. `vfio-pci`, `igb_uio`) give the process direct access to NIC queues and DMA-able memory.
- **Huge pages + preallocated mempools** provide large, physically-contiguous, cache‑friendly buffers.
- **Polling on dedicated lcores** avoids interrupts and per-packet system calls.

---

## High-level Architecture

```text
          +-------------------------+
          |  DPDK App (User Space)  |
          |  - lcore 0,1,...        |
          |  - RX/TX loops          |
          +------------+------------+
                       |
                       | DPDK EAL, PMD (Poll Mode Driver)
                       v
+----------------------+---------------------+
|     Userspace NIC driver (vfio/uio)       |
|     - Maps NIC BARs & queues to userspace |
|     - Programs NIC RX/TX rings            |
+----------------------+--------------------+
                       |
                       | PCIe DMA to hugepage memory
                       v
             +---------+---------+
             |  Hugepage Memory  |
             |  - mbuf pools     |
             |  - rings, tables  |
             +---------+---------+
                       |
                       v
                    NIC Hardware
```

The **kernel networking stack is bypassed** for data plane traffic; the kernel still participates in control plane (device management, driver binding, routing config, etc.).

---

## Why DPDK Can Process Packets Directly in User Space

### 1. Userspace NIC Drivers and DMA

Traditionally:

- Packets arrive at NIC → interrupt → kernel driver → kernel networking stack → socket layer → user process (`recv()`).
- Each hop involves context switches, copies, and protocol processing.

With DPDK:

- The NIC is bound to a **userspace‑friendly driver** (`vfio-pci` or `igb_uio`).
- DPDK’s PMD (Poll Mode Driver) uses **MMIO registers** and **ring descriptors** directly from user space:
  - RX/TX descriptor rings are mapped into the process via hugepages.
  - The NIC performs **DMA directly into hugepage-backed mbufs**.
  - The DPDK app polls the RX rings (no kernel Rx path, no interrupts in the hot path).

Result: the DPDK app **owns the RX/TX queues** and **talks directly to the NIC** via PCIe and MMIO, so packets never traverse the kernel’s protocol stack for the fast path.

### 2. Environment Abstraction Layer (EAL)

EAL (`rte_eal_init()`) prepares the process:

- Reserves and maps **hugepage** memory.
- Discovers **CPU cores (lcores)**, NUMA topology.
- Probes **PCI devices**, binds them to DPDK drivers, and creates `rte_eth_dev` objects.
- Initializes timers, logging, and shared memory for multi‑process.

Once EAL is initialized, the application can configure NIC ports and queues and start polling loops.

---

## Detailed Packet Processing Flow

### 1. Initialization Phase

1. **Parse EAL arguments & init EAL**
   - `rte_eal_init(argc, argv);`
   - Sets up hugepages, lcore mapping, PCI probing.

2. **Create mbuf pools (packet buffers)**
   - `rte_pktmbuf_pool_create(...)` allocates a mempool in hugepage memory.
   - Each mbuf is:
     - Physically contiguous (for DMA).
     - Cache‑friendly (aligned).
     - Reusable (no per-packet `malloc`/`free`).

3. **Configure and start NIC ports**
   - `rte_eth_dev_configure(port, nb_rx_q, nb_tx_q, &port_conf);`
   - For each RX/TX queue:
     - `rte_eth_rx_queue_setup(...)`
     - `rte_eth_tx_queue_setup(...)`
   - `rte_eth_dev_start(port);`

4. **Launch lcores (worker threads)**
   - `rte_eal_mp_remote_launch(lcore_main, arg, CALL_MASTER);`
   - Each lcore pins to a specific CPU core and runs `lcore_main()` or similar.

### 2. Runtime RX/TX Loops

Typical single-core loop:

```c
while (likely(!force_quit)) {
    // 1. Receive bursts of packets
    nb_rx = rte_eth_rx_burst(port_id, rx_queue_id, rx_pkts, MAX_BURST);

    for (i = 0; i < nb_rx; i++) {
        struct rte_mbuf *m = rx_pkts[i];

        // 2. Parse headers (Ethernet/IP/TCP/UDP or custom)
        // 3. Apply user logic: ACL, routing, load balancing, filtering, metering...

        // Example: forward out same port
        tx_pkts[nb_tx++] = m;
    }

    // 4. Transmit in bursts
    if (nb_tx > 0) {
        sent = rte_eth_tx_burst(port_id, tx_queue_id, tx_pkts, nb_tx);
        // unsent mbufs must be freed to avoid leaks
        for (i = sent; i < nb_tx; i++)
            rte_pktmbuf_free(tx_pkts[i]);
        nb_tx = 0;
    }
}
```

Key properties:

- **Polling instead of interrupts** → avoids per-packet interrupt overhead and wakeups.
- **Burst I/O** (RX/TX bursts) → improves cache and bus efficiency.
- All packet parsing and decision logic is in **user space**, so you can tailor the data plane exactly to your needs.

### 3. Diagram: RX Path with DPDK

```text
          +------------------------------+
          |   DPDK app (user space)      |
          |   while(...) {               |
          |     rte_eth_rx_burst();      |
          |     process packets;         |
          |     rte_eth_tx_burst();      |
          |   }                          |
          +------------------------------+
                        ^
                        | mbuf pointers
                        |
        +---------------+----------------+
        |      RX ring (in hugepages)    |
        |  - array of descriptors        |
        |  - each desc -> mbuf          |
        +---------------+----------------+
                        ^
                        | DMA
                        |
                    +---+---+
                    |  NIC |
                    +---+---+
                        ^
                        | Ethernet line
                        v
                     Incoming
                      packets
```

---

## Why Huge Pages Help DPDK

### Normal pages vs Huge pages

- **Normal page**: typically 4 KB.
- **Huge page**: 2 MB or 1 GB (depending on platform).

Without huge pages:

- Large packet buffers (e.g. hundreds of MB) require **lots of 4 KB pages**.
- Page tables and TLB entries explode in count.
- More **TLB misses**, more page-walks → higher latency and CPU overhead.

With huge pages:

- Same memory covered by **far fewer pages**.
- Fewer TLB entries; more memory per TLB entry → significantly lower TLB miss rate.
- More likely that packet buffers and metadata live in a small, hot working set from the CPU’s perspective.

### Advantages for DPDK

1. **Better TLB and cache behavior**
   - Fewer page table entries → fewer TLB misses.
   - Data structures like mempools and rings become more cache‑friendly.

2. **Easier DMA mapping**
   - NICs need physical addresses for DMA.
   - Huge pages are contiguous and easier to map to NIC DMA regions.

3. **Predictable, pinned memory**
   - Hugepage memory is typically **pinned** (unswappable).
   - Eliminates latency spikes from paging or memory compaction.

---

## DPDK vs. Traditional Kernel Networking Stack

### Traditional stack (Linux kernel, sockets)

```text
NIC → kernel driver → kernel network stack (L2/L3/L4) → socket buffer
    → syscall (recvfrom()/sendto()/poll()) → user process
```

Characteristics:

- **Pros**:
  - Full TCP/IP stack, congestion control, routing, firewall, etc.
  - Easy programming model (sockets).
- **Cons for high-speed data plane**:
  - Multiple context switches per packet (interrupts + syscalls).
  - Copies between kernel and user space (unless special APIs are used).
  - General-purpose stack has lots of branches and overhead not optimized for a single use case.

### DPDK data plane

```text
NIC ↔ userspace driver/DPDK PMD ↔ hugepage mempools ↔ DPDK app logic
```

Characteristics:

- **Pros**:
  - No per‑packet syscalls or context switches.
  - No generic kernel protocol stack overhead (you implement only what you need).
  - Tight loops with burst I/O, pinned cores, and NUMA-aware memory layout.
  - Can reach line rate at 10/25/40/100 Gbps with small packets.
- **Cons**:
  - You must implement or integrate your own L2/L3/L4 logic if needed (or use frameworks on top of DPDK).
  - Polling loops consume CPU even when traffic is low (though this can be mitigated).
  - More complex to integrate with “normal” kernel networking (must handle control-plane separately).

---

## Putting It All Together

1. **EAL initialization** reserves hugepage memory, discovers NICs and cores, and sets up the environment.
2. **Mbuf pools and rings** are created in hugepage memory for DMA and fast allocations.
3. **NIC RX/TX queues** are configured to use these mbufs.
4. **Worker lcores** run tight polling loops (`rte_eth_rx_burst` / `rte_eth_tx_burst`), processing packets fully in user space.
5. **Huge pages + polling + userspace drivers** together eliminate most of the overhead of the traditional kernel stack, enabling very high packet rates and low latency for data plane workloads.

For a real application, you typically:

- Use DPDK for the **data plane** (packet I/O, classification, forwarding, QoS).
- Use kernel or other components for the **control plane** (CLI/REST, routing protocols, configuration management).


