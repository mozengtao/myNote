# Linux Networking Subsystem: Packet-Centric Architecture (sk_buff)

## 1. Why Packet Metadata and Payload Are Separated

```
+------------------------------------------------------------------+
|  sk_buff DESIGN PHILOSOPHY                                       |
+------------------------------------------------------------------+

    PROBLEM: Packet Processing Requires Two Different Views
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  METADATA (changes at every layer):                        │
    │    • Which device received/sends this packet?              │
    │    • What protocol is it? (Ethernet? IP? TCP?)             │
    │    • Checksums computed? Valid?                            │
    │    • Where do headers begin?                               │
    │    • Fragmentation info, routing decision, etc.            │
    │                                                              │
    │  PAYLOAD (mostly unchanged):                               │
    │    • The actual packet bytes                               │
    │    • Headers prepended/stripped at each layer              │
    │    • Often megabytes of data (jumbo frames, TSO)           │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: Separate sk_buff (metadata) from Data Area
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    struct sk_buff (~240 bytes)                             │
    │    ┌───────────────────────────────────────┐               │
    │    │  Metadata:                           │               │
    │    │    • Protocol info                   │               │
    │    │    • Device pointers                 │               │
    │    │    • Header offsets                  │               │
    │    │    • Checksums                       │               │
    │    │    • Timestamps                      │               │
    │    │    • Layer-private control buffer    │               │
    │    ├───────────────────────────────────────┤               │
    │    │  Pointers to data:                   │               │
    │    │    • head, data, tail, end           │               │
    │    └───────────────────────────────────────┘               │
    │                    │                                        │
    │                    │  points to                            │
    │                    ▼                                        │
    │    ┌───────────────────────────────────────┐               │
    │    │  Data Area (variable size)           │               │
    │    │    • Headroom                        │               │
    │    │    • Packet headers + payload        │               │
    │    │    • Tailroom                        │               │
    │    │    • skb_shared_info                 │               │
    │    └───────────────────────────────────────┘               │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    BENEFITS:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Metadata manipulation is cheap (small struct)          │
    │  2. Data can be shared between clones (copy metadata only) │
    │  3. Paged data for jumbo frames (scatter-gather)           │
    │  4. Headroom allows prepending headers without copy        │
    │  5. Layer-specific info in control buffer (cb[48])         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Headroom and Tailroom Design

```
+------------------------------------------------------------------+
|  HOW HEADROOM/TAILROOM ENABLE ZERO-COPY LAYERING                 |
+------------------------------------------------------------------+

    MEMORY LAYOUT:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    head ──► ┌─────────────────────────────────────┐        │
    │             │                                     │        │
    │             │          HEADROOM                   │        │
    │             │   (reserved for lower layer hdrs)   │        │
    │             │                                     │        │
    │    data ──► ├─────────────────────────────────────┤        │
    │             │  ┌───────────────────────────────┐  │        │
    │             │  │      Ethernet Header (14B)   │  │        │
    │             │  ├───────────────────────────────┤  │        │
    │             │  │      IP Header (20-60B)      │  │        │
    │             │  ├───────────────────────────────┤  │        │
    │             │  │      TCP Header (20-60B)     │  │        │
    │             │  ├───────────────────────────────┤  │        │
    │             │  │                               │  │        │
    │             │  │         PAYLOAD               │  │        │
    │             │  │                               │  │        │
    │             │  └───────────────────────────────┘  │        │
    │    tail ──► ├─────────────────────────────────────┤        │
    │             │                                     │        │
    │             │          TAILROOM                   │        │
    │             │   (reserved for trailers/padding)   │        │
    │             │                                     │        │
    │     end ──► ├─────────────────────────────────────┤        │
    │             │      skb_shared_info                │        │
    │             └─────────────────────────────────────┘        │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    TRANSMIT PATH (adding headers):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Application provides payload:                              │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │  1. alloc_skb(size, GFP_KERNEL) with headroom          ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │    HEADROOM           │  payload        │        ││
    │  │     │                       ▲                  │        ││
    │  │     └───────────────────────┼──────────────────┘        ││
    │  │                             │                            ││
    │  │                           data                           ││
    │  │                                                          ││
    │  │  2. TCP adds header (skb_push):                         ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │  HEADROOM  │ TCP │    payload           │        ││
    │  │     │            ▲                             │        ││
    │  │     └────────────┼─────────────────────────────┘        ││
    │  │                  │                                       ││
    │  │                data (moved left)                         ││
    │  │                                                          ││
    │  │  3. IP adds header (skb_push):                          ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │ HEADROOM │IP│TCP│    payload            │        ││
    │  │     │          ▲                               │        ││
    │  │     └──────────┼───────────────────────────────┘        ││
    │  │                │                                         ││
    │  │              data                                        ││
    │  │                                                          ││
    │  │  4. Ethernet adds header (skb_push):                    ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │ HEAD │ETH│IP│TCP│    payload            │        ││
    │  │     │      ▲                                   │        ││
    │  │     └──────┼───────────────────────────────────┘        ││
    │  │            │                                             ││
    │  │          data                                            ││
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │  NO DATA COPYING! Just pointer manipulation.               │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    RECEIVE PATH (stripping headers):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  NIC delivers complete frame:                               │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │  1. Driver receives:                                    ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │ETH│IP│TCP│    payload                   │        ││
    │  │     ▲                                          │        ││
    │  │     └──────────────────────────────────────────┘        ││
    │  │     │                                                    ││
    │  │   data                                                   ││
    │  │                                                          ││
    │  │  2. Ethernet processed (skb_pull):                      ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │ETH│IP│TCP│    payload                   │        ││
    │  │         ▲                                      │        ││
    │  │     └───┼──────────────────────────────────────┘        ││
    │  │         │                                                ││
    │  │       data (moved right)                                 ││
    │  │                                                          ││
    │  │  3. IP processed (skb_pull):                            ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │ETH│IP│TCP│    payload                   │        ││
    │  │            ▲                                   │        ││
    │  │     └──────┼───────────────────────────────────┘        ││
    │  │            │                                             ││
    │  │          data                                            ││
    │  │                                                          ││
    │  │  4. TCP delivers payload to socket:                     ││
    │  │     ┌──────────────────────────────────────────┐        ││
    │  │     │ETH│IP│TCP│    payload                   │        ││
    │  │                ▲                               │        ││
    │  │     └──────────┼───────────────────────────────┘        ││
    │  │                │                                         ││
    │  │              data                                        ││
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │  Headers still accessible via mac_header, network_header!  │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    KEY FUNCTIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  skb_push(skb, len)    → Prepend space, move data left     │
    │  skb_pull(skb, len)    → Remove from head, move data right │
    │  skb_put(skb, len)     → Append space, move tail right     │
    │  skb_trim(skb, len)    → Trim to length, move tail left    │
    │                                                              │
    │  skb_reserve(skb, len) → Reserve headroom (at alloc time)  │
    │  skb_headroom(skb)     → Available headroom                │
    │  skb_tailroom(skb)     → Available tailroom                │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. SKB Cloning and Sharing

```
+------------------------------------------------------------------+
|  SKB CLONING: SHARE DATA, SEPARATE METADATA                      |
+------------------------------------------------------------------+

    WHEN CLONING IS NEEDED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • tcpdump/packet capture (needs copy, original continues) │
    │  • Multicast (same packet to multiple destinations)        │
    │  • Bridging (forward while also processing locally)        │
    │  • Retransmission (keep original while sending copy)       │
    └─────────────────────────────────────────────────────────────┘

    SKB_CLONE (Shallow clone):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    Original                        Clone                    │
    │    ┌────────────────┐              ┌────────────────┐       │
    │    │ struct sk_buff │              │ struct sk_buff │       │
    │    │ users = 1      │              │ users = 1      │       │
    │    │ cloned = 1     │              │ cloned = 1     │       │
    │    │ data ──────────┼──────┐       │ data ──────────┼───┐   │
    │    │ head ──────────┼──────┤       │ head ──────────┼───┤   │
    │    └────────────────┘      │       └────────────────┘   │   │
    │                            │                            │   │
    │                            ▼                            ▼   │
    │                     ┌─────────────────────────────────────┐ │
    │                     │         SHARED DATA AREA            │ │
    │                     │  ┌─────────────────────────────────┐│ │
    │                     │  │   Packet Data                   ││ │
    │                     │  └─────────────────────────────────┘│ │
    │                     │  ┌─────────────────────────────────┐│ │
    │                     │  │   skb_shared_info               ││ │
    │                     │  │   dataref = 2  ◄── Both count   ││ │
    │                     │  └─────────────────────────────────┘│ │
    │                     └─────────────────────────────────────┘ │
    │                                                              │
    │  Semantics:                                                 │
    │    • New sk_buff struct allocated                          │
    │    • Data area SHARED (dataref incremented)                │
    │    • Each sk_buff can have different metadata              │
    │    • Neither can modify shared data (read-only)            │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    PSKB_COPY (Partial copy):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    Original                        Copy                     │
    │    ┌────────────────┐              ┌────────────────┐       │
    │    │ struct sk_buff │              │ struct sk_buff │       │
    │    │ users = 1      │              │ users = 1      │       │
    │    │ data ──────────┼───┐          │ data ──────────┼───┐   │
    │    └────────────────┘   │          └────────────────┘   │   │
    │                         │                               │   │
    │                         ▼                               ▼   │
    │    ┌─────────────────────────┐    ┌─────────────────────┐   │
    │    │ Original Data Area      │    │ New Data Area       │   │
    │    │ ┌─────────────────────┐ │    │ ┌─────────────────┐ │   │
    │    │ │  Linear Headers     │ │    │ │  COPIED Headers │ │   │
    │    │ └─────────────────────┘ │    │ └─────────────────┘ │   │
    │    │ ┌─────────────────────┐ │    │ ┌─────────────────┐ │   │
    │    │ │ skb_shared_info     │ │    │ │ skb_shared_info │ │   │
    │    │ │   frags[] ──────────┼─┼────┼─┼──► SHARED PAGES │ │   │
    │    │ └─────────────────────┘ │    │ └─────────────────┘ │   │
    │    └─────────────────────────┘    └─────────────────────┘   │
    │                                                              │
    │  Semantics:                                                 │
    │    • New sk_buff AND new linear data area                  │
    │    • Headers copied (can be modified)                      │
    │    • Paged data (frags) still shared                       │
    │    • Used when headers need modification                   │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    SKB_COPY (Full deep copy):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    Original                        Copy                     │
    │    ┌────────────────┐              ┌────────────────┐       │
    │    │ struct sk_buff │              │ struct sk_buff │       │
    │    │ data ──────────┼───┐          │ data ──────────┼───┐   │
    │    └────────────────┘   │          └────────────────┘   │   │
    │                         ▼                               ▼   │
    │    ┌─────────────────────┐        ┌─────────────────────┐   │
    │    │ Original Data Area  │        │ COMPLETE COPY       │   │
    │    │ (all data)          │        │ (all data)          │   │
    │    └─────────────────────┘        └─────────────────────┘   │
    │                                                              │
    │  Semantics:                                                 │
    │    • Everything copied                                     │
    │    • No sharing whatsoever                                 │
    │    • Most expensive, but safe                              │
    │    • Rarely needed                                         │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    CHOOSING THE RIGHT COPY:
    ┌─────────────────────────────────────────────────────────────┐
    │  • skb_clone():  Fast, data shared, metadata independent   │
    │                  Use for: capture, multicast delivery      │
    │                                                              │
    │  • pskb_copy(): Moderate, headers copied, payload shared   │
    │                  Use for: NAT, header modification         │
    │                                                              │
    │  • skb_copy():  Slow, everything copied                    │
    │                  Use for: rare cases needing full isolation│
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Full Packet Receive Path

```
+------------------------------------------------------------------+
|  PACKET RECEIVE PATH: NIC → SOCKET                               |
+------------------------------------------------------------------+

    STEP 1: NIC Driver Receives Packet
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Hardware interrupt → Driver's interrupt handler            │
    │                                                              │
    │  /* e1000 driver example */                                 │
    │  skb = netdev_alloc_skb(netdev, length + NET_IP_ALIGN);     │
    │  skb_reserve(skb, NET_IP_ALIGN);  /* Align IP header */     │
    │                                                              │
    │  /* Copy data from NIC buffer via DMA */                    │
    │  memcpy(skb->data, hw_buffer, length);                      │
    │  skb_put(skb, length);                                      │
    │                                                              │
    │  /* Set device and protocol */                              │
    │  skb->dev = netdev;                                         │
    │  skb->protocol = eth_type_trans(skb, netdev);               │
    │                                                              │
    │  /* Schedule for processing */                              │
    │  napi_gro_receive(napi, skb);  /* or netif_receive_skb */   │
    │                                                              │
    │  OWNERSHIP: Driver → Network Core                           │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    STEP 2: netif_receive_skb (Network Core)
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* net/core/dev.c */                                       │
    │  int netif_receive_skb(struct sk_buff *skb)                 │
    │  {                                                          │
    │      /* Check packet type, set pkt_type */                 │
    │      skb_reset_network_header(skb);                        │
    │                                                              │
    │      /* Packet capture (tcpdump) - clone skb */            │
    │      list_for_each_entry_rcu(ptype, &ptype_all, list) {    │
    │          deliver_skb(skb, ptype, orig_dev);                │
    │      }                                                       │
    │                                                              │
    │      /* Find protocol handler by skb->protocol */          │
    │      list_for_each_entry_rcu(ptype, &ptype_base[...]) {    │
    │          if (ptype->type == skb->protocol) {               │
    │              ret = ptype->func(skb, dev, ptype, orig_dev); │
    │              /* e.g., ip_rcv() for IP packets */           │
    │          }                                                   │
    │      }                                                       │
    │  }                                                          │
    │                                                              │
    │  OWNERSHIP: Network Core → Protocol Handler (IP)           │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    STEP 3: IP Layer (ip_rcv)
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* net/ipv4/ip_input.c */                                  │
    │  int ip_rcv(struct sk_buff *skb, ...)                       │
    │  {                                                          │
    │      struct iphdr *iph;                                     │
    │                                                              │
    │      /* Validate IP header */                              │
    │      if (skb->pkt_type == PACKET_OTHERHOST)                │
    │          goto drop;                                         │
    │                                                              │
    │      /* May need to make writable copy */                  │
    │      if (!pskb_may_pull(skb, sizeof(struct iphdr)))        │
    │          goto drop;                                         │
    │                                                              │
    │      iph = ip_hdr(skb);                                    │
    │                                                              │
    │      /* Validate checksum, length, version */              │
    │      if (ip_fast_csum((u8 *)iph, iph->ihl))                │
    │          goto drop;                                         │
    │                                                              │
    │      /* Pass to ip_rcv_finish via netfilter */             │
    │      return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING,     │
    │                     skb, dev, NULL, ip_rcv_finish);         │
    │  }                                                          │
    │                                                              │
    │  ip_rcv_finish:                                             │
    │    → Route lookup (skb_dst_set)                            │
    │    → ip_local_deliver() if for us                          │
    │    → skb_pull() to strip IP header                         │
    │    → Find transport protocol handler                       │
    │                                                              │
    │  OWNERSHIP: IP Layer → Transport Layer (TCP)               │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    STEP 4: TCP Layer (tcp_v4_rcv)
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* net/ipv4/tcp_ipv4.c */                                  │
    │  int tcp_v4_rcv(struct sk_buff *skb)                        │
    │  {                                                          │
    │      struct tcphdr *th;                                     │
    │      struct sock *sk;                                       │
    │                                                              │
    │      /* Pull TCP header into linear area */                │
    │      if (!pskb_may_pull(skb, sizeof(struct tcphdr)))       │
    │          goto discard;                                      │
    │                                                              │
    │      th = tcp_hdr(skb);                                    │
    │                                                              │
    │      /* Find socket by 4-tuple */                          │
    │      sk = __inet_lookup(net, &tcp_hashinfo,                │
    │                         iph->saddr, th->source,             │
    │                         iph->daddr, th->dest, ...);         │
    │                                                              │
    │      if (sk) {                                              │
    │          /* Process TCP: state machine, ACKs, etc. */      │
    │          ret = tcp_v4_do_rcv(sk, skb);                     │
    │          /* Eventually: skb queued to sk->sk_receive_queue */
    │      }                                                       │
    │  }                                                          │
    │                                                              │
    │  tcp_v4_do_rcv:                                             │
    │    → tcp_rcv_established() or tcp_rcv_state_process()      │
    │    → Validate sequence numbers, handle ACKs                │
    │    → tcp_data_queue() to add to receive queue              │
    │    → Wake up waiting reader: sk->sk_data_ready()           │
    │                                                              │
    │  OWNERSHIP: TCP → Socket Receive Queue                     │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    STEP 5: Socket Receive Queue
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* Packet now queued */                                   │
    │  skb_queue_tail(&sk->sk_receive_queue, skb);               │
    │                                                              │
    │  /* Wake up blocked reader */                              │
    │  sk->sk_data_ready(sk, skb_len);                           │
    │                                                              │
    │  /* User calls recvmsg() */                                │
    │  sys_recvmsg()                                             │
    │    → sock_recvmsg()                                        │
    │    → sock->ops->recvmsg()  /* inet_recvmsg */              │
    │    → sk->sk_prot->recvmsg() /* tcp_recvmsg */              │
    │    → skb_recv_datagram() or skb_copy_datagram_iovec()      │
    │    → Copy to user buffer                                   │
    │    → kfree_skb() or consume_skb()                          │
    │                                                              │
    │  OWNERSHIP: Socket Queue → User (freed after copy)         │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. Layer Processing Without Copying

```
+------------------------------------------------------------------+
|  HOW EACH LAYER ADDS/REMOVES MEANING WITHOUT COPYING             |
+------------------------------------------------------------------+

    LAYER SEPARATION VIA POINTERS:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  After receiving at NIC:                                    │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │     │ ETH │  IP  │ TCP  │     PAYLOAD            │     ││
    │  │     ▲                                              ▲    ││
    │  │     │                                              │    ││
    │  │   data                                           tail   ││
    │  │                                                          ││
    │  │   mac_header     = data (set by eth_type_trans)         ││
    │  │   network_header = unset                                ││
    │  │   transport_header = unset                              ││
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │  After Ethernet processing:                                 │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │     │ ETH │  IP  │ TCP  │     PAYLOAD            │     ││
    │  │           ▲                                        ▲    ││
    │  │           │                                        │    ││
    │  │         data                                     tail   ││
    │  │                                                          ││
    │  │   mac_header     = points to ETH (unchanged)            ││
    │  │   network_header = data (set by skb_reset_network_header)│
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │  After IP processing:                                       │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │     │ ETH │  IP  │ TCP  │     PAYLOAD            │     ││
    │  │                   ▲                                ▲    ││
    │  │                   │                                │    ││
    │  │                 data                             tail   ││
    │  │                                                          ││
    │  │   mac_header      = points to ETH                       ││
    │  │   network_header  = points to IP                        ││
    │  │   transport_header = data (set by skb_set_transport_header)│
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │  After TCP processing:                                      │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │     │ ETH │  IP  │ TCP  │     PAYLOAD            │     ││
    │  │                         ▲                          ▲    ││
    │  │                         │                          │    ││
    │  │                       data                       tail   ││
    │  │                                                          ││
    │  │   All headers still accessible via saved offsets!       ││
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │  KEY INSIGHT:                                               │
    │  • data pointer moves, but headers remain in place         │
    │  • Each layer saves its header offset                      │
    │  • Higher layers can still access lower headers            │
    │  • ZERO memory copying for header "stripping"              │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    CONTROL BUFFER (per-layer private data):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  struct sk_buff {                                          │
    │      ...                                                    │
    │      char cb[48] __aligned(8);  /* Control buffer */       │
    │      ...                                                    │
    │  };                                                         │
    │                                                              │
    │  Each layer uses cb[] for its own purposes:                │
    │                                                              │
    │  /* TCP uses it for: */                                    │
    │  struct tcp_skb_cb {                                       │
    │      __u32 seq;            /* Start sequence number */     │
    │      __u32 end_seq;        /* End sequence number */       │
    │      __u32 when;           /* Timestamp */                 │
    │      __u8  tcp_flags;      /* TCP flags */                 │
    │      __u8  sacked;         /* SACK state */                │
    │      /* ... */                                             │
    │  };                                                         │
    │  #define TCP_SKB_CB(skb) ((struct tcp_skb_cb *)&(skb)->cb) │
    │                                                              │
    │  /* IP uses it for: */                                     │
    │  struct inet_skb_parm {                                    │
    │      struct ip_options opt;                                │
    │      int iif;              /* Input interface */           │
    │      __u16 flags;                                          │
    │      /* ... */                                             │
    │  };                                                         │
    │  #define IPCB(skb) ((struct inet_skb_parm *)&(skb)->cb)    │
    │                                                              │
    │  WARNING: cb[] is reused at each layer!                    │
    │  Cloning preserves cb[], but layer ownership changes.      │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  SK_BUFF ARCHITECTURE SUMMARY                                    |
+------------------------------------------------------------------+

    DESIGN PRINCIPLES:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Separate metadata (sk_buff) from data (buffer area)    │
    │  2. Headroom/tailroom for header manipulation              │
    │  3. Pointer manipulation instead of copying                │
    │  4. Saved offsets for header access across layers          │
    │  5. Control buffer for per-layer private data              │
    │  6. Reference counting for sharing                         │
    └─────────────────────────────────────────────────────────────┘

    PERFORMANCE WINS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Zero-copy header manipulation                           │
    │  • Efficient cloning (share data, copy metadata)           │
    │  • Pre-allocated headroom (no reallocation)                │
    │  • Cache-friendly metadata layout                          │
    │  • Per-layer cb[] avoids extra allocations                 │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP TRANSFER:
    ┌─────────────────────────────────────────────────────────────┐
    │  NIC Driver → netif_receive_skb() → Protocol → Socket      │
    │  Each handoff is explicit pointer transfer                 │
    │  Reference counting (users, dataref) for sharing           │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **设计哲学**：元数据(sk_buff)与数据分离，支持高效克隆和共享
- **头部/尾部空间**：预留空间允许无拷贝添加/剥离协议头
- **指针操作**：skb_push/skb_pull只移动指针，不拷贝数据
- **层级偏移保存**：mac_header/network_header/transport_header保存各层头偏移
- **控制缓冲区**：cb[48]供每层存储私有数据，层间复用
- **接收路径**：NIC→netif_receive_skb→IP→TCP→socket队列，所有权明确传递

