# DPDK Concepts and Port Types

## 1. Core Architecture Overview

DPDK (Data Plane Development Kit) is a set of user-space libraries and drivers
for fast packet processing. Its key design principle: **bypass the kernel
network stack** and process packets entirely in user space via polling.

```
 +================================================================+
 |                        USER SPACE                               |
 |                                                                 |
 |  +----------------------------------------------------------+   |
 |  |                   DPDK Application                        |  |
 |  |                                                           |  |
 |  |   rte_eth_rx_burst()  /  rte_eth_tx_burst()               |  |
 |  |          |                      |                         |  |
 |  |          v                      v                         |  |
 |  |  +------------------------------------------------+       |  |
 |  |  |          PMD (Poll Mode Driver)                |       |  |
 |  |  |                                                |       |  |
 |  |  |  - Runs in user space (same process)           |       |  |
 |  |  |  - Polls RX/TX descriptor rings directly       |       |  |
 |  |  |  - No interrupts for data path (zero-copy)     |       |  |
 |  |  |  - One PMD per device type (ixgbe, i40e, ...)  |       |  |
 |  |  +------------------------------------------------+       |  |
 |  |          |              |               |                 |  |
 |  |     Memory-mapped    DMA buffers    Hugepage memory       |  |
 |  |     device registers (mbufs)        (mempool)             |  |
 |  +----------------------------------------------------------+   |
 |             |                                                   |
 +================================================================+
               | mmap / ioctl
 +================================================================+
 |                       KERNEL SPACE                              |
 |                                                                 |
 |  +-------------------------+  +-----------------------------+   |
 |  |     UIO Driver          |  |      VFIO Driver             |  |
 |  |                         |  |                              |  |
 |  | - Maps device BARs to   |  | - Maps device BARs to        |  |
 |  |   user space            |  |   user space                 |  |
 |  | - Minimal interrupt     |  | - Full IOMMU isolation       |  |
 |  |   handling              |  | - Secure DMA mapping         |  |
 |  | - No IOMMU support      |  | - SR-IOV support             |  |
 |  | - Simpler, less secure  |  | - Preferred for production   |  |
 |  +-------------------------+  +-----------------------------+   |
 |             |                           |                       |
 +================================================================+
               |                           |
 +================================================================+
 |                        HARDWARE                                 |
 |                                                                 |
 |  +----------------------------------------------------------+   |
 |  |              Physical NIC (PF)                            |  |
 |  |                                                           |  |
 |  |   +----------+  +----------+  +----------+                |  |
 |  |   |  VF 0    |  |  VF 1    |  |  VF N    |  (SR-IOV)      |  |
 |  |   +----------+  +----------+  +----------+                |  |
 |  +----------------------------------------------------------+   |
 |                         |                                       |
 |                     Network / Wire                              |
 +================================================================+
```

## 2. Key Concepts

### 2.1 PMD (Poll Mode Driver)

| Aspect       | Detail                                                      |
|--------------|-------------------------------------------------------------|
| **Runs in**  | **User space** (linked into the DPDK application)           |
| **Mechanism**| Continuously polls RX/TX descriptor rings (no interrupts)   |
| **Purpose**  | Drive a specific device type for packet RX/TX               |
| **Examples** | `ixgbe`, `i40e`, `mlx5`, `net_tap`, `net_bonding`, `net_ring` |

- Each device type has its own PMD implementation.
- PMD is linked at compile time or loaded as a shared library.
- Because it runs in user space with zero-copy and no context switches,
  it achieves very low latency and high throughput.

### 2.2 UIO (Userspace I/O)

| Aspect       | Detail                                                      |
|--------------|-------------------------------------------------------------|
| **Runs in**  | **Kernel space** (kernel module)                            |
| **Mechanism**| Maps PCI device BARs into user-space address space          |
| **Purpose**  | Allow user-space code to directly access device registers   |
| **Modules**  | `igb_uio`, `uio_pci_generic`                               |

- Minimal kernel driver; does NOT process packets.
- Handles basic interrupt forwarding (link state, etc.).
- **No IOMMU support** — less secure, requires root.

### 2.3 VFIO (Virtual Function I/O) — Deep Dive

| Aspect       | Detail                                                      |
|--------------|-------------------------------------------------------------|
| **Runs in**  | **Kernel space** (kernel subsystem)                         |
| **Mechanism**| Maps device to user space with IOMMU-backed DMA isolation   |
| **Purpose**  | Secure, isolated user-space device access                   |
| **Module**   | `vfio-pci`                                                  |

- Preferred over UIO for production: provides DMA isolation via IOMMU.
- Supports SR-IOV (VF passthrough to VMs/containers).
- Can work without root (via group permissions).

#### 2.3.1 VFIO Architecture: Container / Group / Device

VFIO organizes access through a three-level hierarchy:

```
  +-----------------------------------------------------------+
  |                   VFIO Container                           |
  |                   /dev/vfio/vfio                           |
  |                                                            |
  |   Owns the IOMMU domain (one virtual address space         |
  |   for all DMA). All groups in a container share the        |
  |   same IOMMU page table.                                   |
  |                                                            |
  |   +-------------------------+  +-------------------------+ |
  |   |     VFIO Group          |  |     VFIO Group          | |
  |   |     /dev/vfio/26        |  |     /dev/vfio/27        | |
  |   |                         |  |                         | |
  |   |  An IOMMU group: set    |  |  Another IOMMU group    | |
  |   |  of devices that share  |  |                         | |
  |   |  IOMMU isolation.       |  |                         | |
  |   |                         |  |                         | |
  |   |  +--------+ +--------+  |  |  +--------+             | |
  |   |  |Device 0| |Device 1|  |  |  |Device 2|             | |
  |   |  |0000:03:| |0000:03:|  |  |  |0000:04:|             | |
  |   |  |00.0    | |00.1    |  |  |  |00.0    |             | |
  |   |  +--------+ +--------+  |  |  +--------+             | |
  |   +-------------------------+  +-------------------------+ |
  +-----------------------------------------------------------+
```

| Level         | What It Is                                                     |
|---------------|----------------------------------------------------------------|
| **Container** | Top-level object. Holds the IOMMU context (DMA address space). Opened via `/dev/vfio/vfio`. |
| **Group**     | Matches an IOMMU group in hardware. All devices in a group share isolation boundaries — they must all be bound to VFIO together. Opened via `/dev/vfio/<group_id>`. |
| **Device**    | A single PCI function (PF or VF). Obtained from the group via `ioctl(VFIO_GROUP_GET_DEVICE_FD)`. Provides access to BARs, config space, and interrupts. |

#### 2.3.2 What IOMMU Does for VFIO

```
  Without IOMMU (UIO):                            With IOMMU (VFIO):

  +----------+     DMA any addr                  +----------+    DMA
  |  Device   |--------------------+             |  Device   |--------+
  +----------+                     |             +----------+         |
                                   v                          v
                            +-----------+                    +--------------+
                            | Physical  |                    |    IOMMU     |
                            | Memory    |                    | (translates) |
                            | (ALL)     |                    +--------------+
                            +-----------+                             |
                                                                      v
                                                             +--------------+
                                                             | Physical Mem |
                                                             | (ONLY mapped |
                                                             |  regions)    |
                                                             +--------------+
```

- **Without IOMMU** (UIO): the device can DMA to **any** physical address.
  A buggy or malicious device/driver can corrupt arbitrary memory.
- **With IOMMU** (VFIO): the device's DMA goes through the IOMMU, which
  translates I/O virtual addresses (IOVA) to physical addresses. Only
  explicitly mapped regions are accessible — everything else faults.

This is why VFIO is considered **secure**: the device is sandboxed at the
hardware level by the IOMMU.

#### 2.3.3 Device Binding Flow

How a PCI device gets bound to VFIO and used by DPDK:

```
  Step 1: Unbind from kernel driver
  +-----------+                         +-----------+
  | ixgbe /   |  echo PCI_ADDR >        | (no       |
  | i40e /    |  /sys/.../unbind        |  driver)  |
  | ice       | ----------------------> |           |
  +-----------+                         +-----------+

  Step 2: Bind to vfio-pci
  +-----------+                         +-----------+
  | (no       |  echo PCI_ADDR >        | vfio-pci  |
  |  driver)  |  /sys/.../bind          | (kernel)  |
  |           | ----------------------> |           |
  +-----------+                         +-----------+

  Step 3: DPDK opens VFIO handles
  +------------------------------------+
  | DPDK EAL init                      |
  |                                    |
  |  1. open(/dev/vfio/vfio)           |   --> container fd
  |  2. open(/dev/vfio/<group>)        |   --> group fd
  |  3. ioctl(SET_IOMMU_TYPE)          |   --> configure IOMMU
  |  4. ioctl(GET_DEVICE_FD, pci_addr) |   --> device fd
  |  5. ioctl(GET_REGION_INFO)         |   --> BAR addresses & sizes
  |  6. mmap(device_fd, BAR_offset)    |   --> map BARs to user space
  |  7. ioctl(DMA_MAP, hugepage_mem)   |   --> map DMA buffers in IOMMU
  +------------------------------------+

  Step 4: PMD uses mapped memory
  +-------------------------------------------------+
  | PMD (user space)                                 |
  |                                                  |
  |  - Reads/writes device registers via mmap'd BARs |
  |  - DMA buffers (mbufs on hugepages) are IOMMU-   |
  |    mapped so device can read/write them safely   |
  |  - Polls descriptor rings for RX/TX              |
  +-------------------------------------------------+
```

In practice, DPDK provides `dpdk-devbind.py` (or `usertools/dpdk-devbind.py`)
which automates steps 1 and 2:

```
  dpdk-devbind.py --bind=vfio-pci 0000:03:00.0
```

#### 2.3.4 DMA Mapping in Detail

```
  +------------------+       +-------------------+       +-----------------+
  | DPDK Application |       |      IOMMU        |       | Physical Memory |
  |                  |       |   (Page Table)    |       |                 |
  |  hugepage buf    |       |                   |       |                 |
  |  IOVA: 0x1000    |------>| 0x1000 -> 0x7F000 |------>| phys: 0x7F000   |
  |                  |       |                   |       |  (hugepage)     |
  |  hugepage buf    |       |                   |       |                 |
  |  IOVA: 0x3000    |------>| 0x3000 -> 0xAB000 |------>| phys: 0xAB000   |
  |                  |       |                   |       |  (hugepage)     |
  +------------------+       +-------------------+       +-----------------+

  Device DMA:
    NIC writes packet to IOVA 0x1000
      --> IOMMU translates to phys 0x7F000
      --> data lands in hugepage buffer
      --> PMD reads it from user-space mmap
```

- DPDK allocates packet buffers (mbufs) from **hugepage memory**.
- These hugepage regions are registered with the IOMMU via
  `ioctl(VFIO_IOMMU_MAP_DMA)`, creating IOVA-to-physical mappings.
- The NIC uses IOVA addresses in its descriptor rings.
- The IOMMU translates IOVA to physical on every DMA access.
- Any DMA to an **unmapped** IOVA causes an IOMMU fault (blocked).

#### 2.3.5 Interrupt Handling

VFIO supports multiple interrupt modes for non-data-path events:

```
  +----------+     MSI-X / INTx      +-----------+     eventfd      +----------+
  |   NIC    | --------------------> | VFIO      | ---------------> | DPDK App |
  |          |     (hardware IRQ)    | (kernel)  |  (user-space     | (epoll/  |
  +----------+                       +-----------+   notification)  |  poll)   |
                                                                    +----------+
```

| Interrupt Type | Use Case                                             |
|----------------|------------------------------------------------------|
| **MSI-X**      | Per-queue interrupts; used for RX interrupt mode     |
| **INTx**       | Legacy PCI interrupts (fallback)                     |
| **eventfd**    | VFIO delivers interrupts to user space via eventfd   |

- In DPDK's default **poll mode**, interrupts are typically disabled for
  RX/TX (the PMD polls continuously).
- Interrupts are still used for: link-state changes, error notifications,
  and power-saving interrupt-driven RX mode (`rte_eth_dev_rx_intr_*`).

#### 2.3.6 VFIO Modes

| Mode              | Description                                          |
|-------------------|------------------------------------------------------|
| **VFIO Type1**    | Standard mode with hardware IOMMU (Intel VT-d / AMD-Vi). Requires IOMMU enabled in BIOS and kernel (`intel_iommu=on`). |
| **VFIO No-IOMMU** | Runs VFIO without IOMMU. Less secure (similar to UIO) but provides the VFIO API. Enabled via `enable_unsafe_noiommu_mode=1`. Used when hardware has no IOMMU. |

```
  VFIO Type1 (with IOMMU):              VFIO No-IOMMU:
  +--------+    +-------+    +-----+    +--------+    +-----+
  | Device |--->| IOMMU |--->| RAM |    | Device |--->| RAM |
  +--------+    +-------+    +-----+    +--------+    +-----+
                (isolated)               (no isolation, unsafe)
```

#### 2.3.7 Per-Device Scope: UIO and VFIO Can Coexist

- UIO and VFIO are **not** system-wide mutually exclusive.
- **Per device**: each PCI device can only be bound to **one** driver at a time
  (either `vfio-pci`, `igb_uio`, `uio_pci_generic`, or a kernel NIC driver).
- **Across devices**: different devices on the same system can use different
  drivers simultaneously. DPDK EAL handles both.

```
  Device A (0000:03:00.0) ---bound-to---> vfio-pci   (VFIO)
  Device B (0000:04:00.0) ---bound-to---> igb_uio    (UIO)
  Device C (0000:05:00.0) ---bound-to---> ixgbe      (kernel, not DPDK)
```

### 2.4 UIO vs VFIO Comparison

```
                  UIO                              VFIO
           +----------------+              +------------------+
           | Kernel Module  |              | Kernel Subsystem |
           |                |              |                  |
  Security | No IOMMU       |              | IOMMU isolation  |
           | Root required  |              | Group-based perms|
           |                |              |                  |
     DMA   | Direct phys    |              | IOMMU-mapped     |
           | (no isolation) |              | (secure)         |
           |                |              |                  |
   SR-IOV  | Limited        |              | Full support     |
           |                |              |                  |
    Use    | Dev/test       |              | Production       |
           +----------------+              +------------------+
```

### 2.5 Other Key Concepts

| Concept              | Description                                                |
|----------------------|------------------------------------------------------------|
| **EAL**              | Environment Abstraction Layer — DPDK init: hugepages, cores, PCI scan, memory |
| **Hugepages**        | 2MB/1GB pages to reduce TLB misses for large packet buffers |
| **mbuf**             | Memory buffer — the packet data structure in DPDK           |
| **Mempool**          | Pre-allocated pool of mbufs for zero-alloc packet handling  |
| **RX/TX Queue**      | Per-port descriptor rings for receiving/transmitting packets |
| **lcore**            | Logical core — a CPU core dedicated to a DPDK thread        |
| **SR-IOV**           | Single Root I/O Virtualization — one PF creates many VFs    |
| **PF**               | Physical Function — the "real" NIC device                   |
| **VF**               | Virtual Function — a lightweight virtual NIC from SR-IOV    |

## 3. DPDK Port Types

All port types present the same `rte_eth_dev` API to the application.
The difference is the underlying device/transport.

### 3.1 Port Type Summary

```
 +------------------------------------------------------------------+
 |                     DPDK eth Port Abstraction                     |
 |                                                                   |
 |  rte_eth_dev_configure()                                          |
 |  rte_eth_rx_burst() / rte_eth_tx_burst()                          |
 |  rte_eth_dev_info_get()                                           |
 |  ... (same API for ALL port types)                                |
 +------------------------------------------------------------------+
        |           |          |          |         |         |
        v           v          v          v         v         v
 +----------+ +----------+ +------+ +--------+ +------+ +--------+
 | Physical | |    VF    | | TAP  | |  Bond  | | Ring | | vhost  |
 |  NIC(PF) | | (SR-IOV) | |      | |        | |      | | -user  |
 +----------+ +----------+ +------+ +--------+ +------+ +--------+
      |            |           |         |         |         |
   wire/NIC    wire/NIC    host OS    multiple   memory     VM /
   (direct)    (virtual)   kernel     ports      queues   container
                           stack     aggregated
```

### 3.2 Detailed Port Types

#### Physical NIC (PF — Physical Function)

```
  DPDK App <--PMD--> PCI NIC (PF) <--wire--> Network
```

- Direct access to a physical Ethernet NIC via PCI.
- Highest performance — bare-metal line rate.
- Driven by hardware-specific PMDs (e.g., `ixgbe`, `i40e`, `ice`, `mlx5`).
- Bound via UIO or VFIO.

#### VF (Virtual Function — SR-IOV)

```
  DPDK App <--PMD--> VF (PCI) --[internal switch]--> PF <--wire--> Network
```

- A lightweight virtual NIC carved out of a physical PF via SR-IOV.
- Each VF has its own PCI address, RX/TX queues, and MAC.
- Near line-rate performance; used in VMs and containers.
- Bound via VFIO (preferred) or UIO.

#### TAP (Virtual Ethernet — Host Interface)

```
  DPDK App <--PMD--> TAP device <---> Host Kernel Network Stack
                                        |
                                    IP / routing / iptables / bridge
```

- Pure software virtual device (no PCI, no hardware).
- Creates a Linux network interface visible to the host OS.
- The host can assign IP, run protocols, apply firewall rules on it.
- Used for: control plane, management, trap/signal traffic.
- MTU typically 1500 (vs 2000+ for hardware ports).
- Created via `--vdev=net_tap0` in EAL arguments.

#### Bond (Link Aggregation)

```
                         +---> VF 0 (member) ---> wire
  DPDK App <--PMD--> Bond Port
                         +---> VF 1 (member) ---> wire
```

- Aggregates multiple ports (typically VFs or PFs) into one logical port.
- Supports modes: round-robin, active-backup, balance (L2/L3+4), 802.3ad LACP.
- Provides redundancy and/or increased throughput.
- Created via `--vdev=net_bonding0,mode=X,slave=Y,...` or API (`rte_eth_bond_create`).

#### Ring (Memory-Based IPC)

```
  DPDK Process A <--PMD--> Ring (shared memory) <--PMD--> DPDK Process B
```

- Lockless memory ring for inter-process or inter-thread packet passing.
- No hardware, no kernel involvement — purely memory-based.
- Used for: multi-process DPDK pipelines, testing, internal forwarding.
- Created via `--vdev=net_ring0`.

#### KNI (Kernel Network Interface)

```
  DPDK App <--PMD--> KNI device <---> Kernel Network Stack
                                        |
                                    existing kernel drivers / protocols
```

- Similar to TAP but with a different mechanism: a kernel module (`rte_kni`)
  creates a virtual interface and exchanges packets with DPDK via FIFOs.
- Can "mirror" a physical port to the kernel for protocol handling.
- **Deprecated in newer DPDK versions** — TAP is now preferred.

#### vhost-user (VM/Container Communication)

```
  DPDK App (host) <--PMD--> vhost-user socket <--virtio--> Guest VM / Container
```

- Provides a high-performance path between a DPDK host application
  and a guest VM (via QEMU/virtio) or container.
- Uses shared memory and a Unix socket for control.
- Used in: NFV, virtual switches (e.g., OVS-DPDK).
- Created via `--vdev=net_vhost0,iface=/tmp/sock0`.

#### AF_XDP (XDP Socket)

```
  DPDK App <--PMD--> AF_XDP socket <---> Kernel NIC driver <---> wire
```

- Uses Linux's AF_XDP (eXpress Data Path) sockets for kernel-bypass-like
  performance while keeping the NIC bound to its kernel driver.
- Does not require UIO/VFIO — the NIC stays under kernel control.
- Lower performance than pure DPDK but useful when full bypass is not possible.

#### Null (Testing/Benchmarking)

```
  DPDK App <--PMD--> /dev/null  (packets dropped or generated)
```

- Drops all TX packets; generates dummy RX packets.
- Used for: benchmarking, pipeline testing without real hardware.
- Created via `--vdev=net_null0`.

#### Pcap (File-Based Capture/Replay)

```
  DPDK App <--PMD--> pcap file  (read packets from / write packets to file)
```

- Reads packets from a `.pcap` file (RX) or writes to a file (TX).
- Used for: offline testing, replay, debugging.
- Created via `--vdev=net_pcap0,rx_pcap=in.pcap,tx_pcap=out.pcap`.

### 3.3 Port Type Comparison Matrix

| Port Type     | Hardware? | Kernel Involved? | Data Path                 | Typical Use Case                |
|---------------|-----------|------------------|---------------------------|---------------------------------|
| **PF**        | Yes       | UIO/VFIO only    | App ↔ NIC ↔ Wire          | Main data plane                 |
| **VF**        | Yes       | UIO/VFIO only    | App ↔ VF ↔ PF ↔ Wire      | VM/container data plane         |
| **TAP**       | No        | Yes (TAP device) | App ↔ Host kernel stack    | Control plane, management       |
| **Bond**      | No (logical) | Via members   | App ↔ Bond ↔ Members ↔ Wire | Redundancy, load balancing     |
| **Ring**      | No        | No               | App ↔ Memory ↔ App         | IPC, internal pipeline          |
| **KNI**       | No        | Yes (KNI module) | App ↔ Kernel stack         | Legacy host integration         |
| **vhost-user**| No        | No (shared mem)  | App ↔ VM/Container         | NFV, virtual switching          |
| **AF_XDP**    | Indirect  | Yes (XDP socket) | App ↔ Kernel driver ↔ Wire | Partial bypass, coexistence     |
| **Null**      | No        | No               | App ↔ /dev/null            | Testing, benchmarking           |
| **Pcap**      | No        | No               | App ↔ File                 | Offline testing, replay         |

## 4. Relationship: Link, Port, and Queue in DPDK

```
  +---------------------------------------------------------------+
  |                      DPDK Application                          |
  |                                                                |
  |   Thread 0 (lcore 0)        Thread 1 (lcore 1)                 |
  |      |                          |                              |
  |      v                          v                              |
  |   +------+                   +------+                          |
  |   | RXQ0 |<-- poll           | RXQ1 |<-- poll                  |
  |   +------+                   +------+                          |
  |      ^                          ^                              |
  +------|--------------------------|------------------------------+
         |                          |
  +------+----+----------+----+-----+---------+
  |  Port 0 (eth_dev)                         |
  |                                           |
  |  RXQ0   RXQ1   ...   TXQ0   TXQ1   ...    |
  |                                           |
  |  Link: UP/DOWN   Speed: 10G/25G/...       |
  |  MAC:  xx:xx:xx:xx:xx:xx                  |
  +-------------------------------------------+
         |
     [PF/VF/TAP/Bond/... depending on port type]
         |
     [Wire / Host / Memory / VM]
```

- **Port** (`rte_eth_dev`): one DPDK network device, identified by `port_id`.
- **Queue** (RXQ/TXQ): each port has one or more RX and TX queues,
  each typically serviced by a dedicated lcore for lock-free operation.
- **Link**: the logical configuration layer (in vCMTS, `LINK0`, `LINK1`, etc.)
  that maps to a port; may be VF, TAP, or Bond depending on its link ID.
