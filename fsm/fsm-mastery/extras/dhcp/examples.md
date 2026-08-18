# 附录 DHCP：四个短例

主例在 `code/`。本文件只对照，不增加新状态层。

---

## 例 1：拿到租约（主例）

**问题**：主机没有地址，要走完 DISCOVER → OFFER → REQUEST → ACK。

```
INIT --DISCOVER_SENT--> SELECTING --OFFER--> REQUESTING --ACK--> BOUND
```

`REQUEST_SENT` 是 `REQUESTING` 上的自环，把「发出」和「ACK」分开。

**实现**：`code/c/dhcp.c`、`code/cpp/dhcp.cpp`、`code/python/dhcp.py`。

**常见缺陷**：`OFFER` 直接进 `BOUND`，跳过 REQUEST，规范不允许。

---

## 例 2：T1 续租

**问题**：租约过半，向原服务器单播 REQUEST，ACK 后继续用同一地址。

| Current State | Event | Action | Next |
|---------------|-------|--------|------|
| BOUND | T1 | send renew | RENEWING |
| RENEWING | ACK | renewed | BOUND |

**常见缺陷**：T1 到期却广播 DISCOVER，把已有租约丢掉。

---

## 例 3：NAK 与租约到期

NAK 表示服务器拒绝。到期表示时钟走完。两条都回 `INIT`，下一轮必须重新 DISCOVER。

```
REQUESTING --NAK--> INIT
BOUND --LEASE_EXPIRED--> INIT
```

**常见缺陷**：NAK 后停在 `REQUESTING` 空等；或到期后仍对外宣称 bound。

---

## 例 4：REBINDING 与 DHCPv6（只讲，不实现）

RFC 2131：T2 之后进入 `REBINDING`，向任意服务器广播续租。本附录 `T2 → INIT`，故意缩短。

DHCPv6 用 Solicit / Advertise / Request / Reply，IA 上另有 T1/T2。不要把本附录五态改名后当成 v6。

**常见缺陷**：在 DHCPv4 教学机上加 `SOLICIT` 事件，两张图焊死。
