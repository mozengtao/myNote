# 第 10 课：五个代表例（两台扁平机协作）

主例「一台 ISM + 一台 NSM」的完整实现在 `code/`。表驱动是第 08 课的；跨机事件是本课的。

不要把邻居画进接口方框里。两台机是并列的。

---

## 例 1：NSM 主路径（主例）

**问题**：单邻居从 `DOWN` 走到 `FULL`，再被 `KILL_NBR` 拉回 `DOWN`。省略 Attempt、Exchange、Loading。

**状态图**：见 `LESSON.md` 本课 NSM 图。

**状态表**：五态；`NEGOTIATION_DONE` 直接进 `FULL`。

**实现**：`code/` 三语言；看带 `NSM:` 前缀的行。

**复杂度**：5 个扁平状态。

**工业用途**：邻接形成与拆除。

**常见缺陷**：`KILL_NBR` 不走 `nsm_dispatch`，邻居表的动作被跳过。

**重构**：加回 Exchange / Loading 只改 NSM 表。

---

## 例 2：ISM

**问题**：接口自己的局面：没启用、点对点已启用、广播上等待、教学桩 DR。

```
DOWN --IF_UP--> POINT_TO_POINT --IF_DOWN--> DOWN
DOWN --IF_UP_BCAST--> WAITING --WAIT_TIMER--> DR
```

主演示走点对点。广播路径证明 ISM 可以单独跑：哪怕 NSM 一直是 `DOWN`，接口仍然能进 `WAITING` / `DR`。这正是「两台机字段独立」的证据。

**复杂度**：4 个扁平状态。

**工业用途**：接口启用、点对点 vs 广播。

**常见缺陷**：为了邻居 Full 而把接口状态改名为 `FULL`——Full 是邻居机的词。

**重构**：Loopback 只是 ISM 多一行，不要因此把 NSM 嵌进去。

---

## 例 3：DR 选举（图 / 文字，代码是桩）

**问题**：广播多路访问网上，必须选出 DR / BDR，普通邻居只保持 2-Way，不与每一个人对出 Full 邻接。

RFC 思路（简化图）：

```
+---------+  WaitTimer / BackupSeen
| WAITING | --------------------------------+
+---------+                                 |
                                            v
                 +--------+    +--------+    +-----------+
                 |   DR   |    | Backup |    | DR Other  |
                 +--------+    +--------+    +-----------+
```

比较的是 Router Priority，同分再比 Router ID。Wait Timer 防止后上线的路由器立刻抢 DR。

本课代码：`WAIT_TIMER` 与 `BACKUP_SEEN` **都** 进入 `DR`，不比较 ID，也没有 Backup / DR Other。轨迹里会打印 `become DR (election stub)`。

**复杂度**：真实选举远大于本课一格。

**工业用途**：以太网 OSPF。

**常见缺陷**：在教学程序里认真实现优先级比较，却仍把 NSM 省略掉——选举服务的是「对谁 AdjOK」，没有邻居机就没有意义。本课桩足够。

**重构**：选举结果变成对每个邻居喂或不喂 `ADJ_OK`，仍是事件，不是第三台「选举状态机」套在 ISM 里。若你单独做一台选举小机，也让它向 NSM **投递事件**，保持扁平。

---

## 例 4：AdjOK

**问题**：双向 Hello 之后，并不总是要形成邻接（ExStart 以降）。RFC 用 AdjOK? 决定。

| 接口类型 | 典型答案 |
|----------|----------|
| 点对点 | 要，对唯一邻居 AdjOK |
| 广播 | 只对 DR / BDR AdjOK；DR Other 之间停在 2-Way |
| NBMA | 类似广播，另有 Attempt |

本课把答案外置：`main` 喂 `ADJ_OK` 就进入 `EXSTART`。没有监护函数。点对点主演示里总是喂。

```
TWOWAY --ADJ_OK / start DD stub--> EXSTART --NEGOTIATION_DONE--> FULL
```

若永远不喂 `ADJ_OK`，邻居停在 `TWOWAY`，接口仍可以是 `POINT_TO_POINT` 或 `DR`。两台机再次独立。

**复杂度**：一格。策略在表外。

**工业用途**：控制邻接数量。

**常见缺陷**：广播网上对所有邻居自动 AdjOK，控制面邻接爆炸。

**重构**：把「是否 DR」做成 NSM 查表前的监护也可以；本课坚持监护外置为事件，以免二维表长出第三维。

---

## 例 5：多邻居（一台 ISM，每邻居一台 NSM）

**问题**：同一接口三个邻居。不要复制三份接口状态。

```
                    +-----------+
                    | ISM (1)   |
                    | p2p or DR |
                    +-----------+
                       |  |  |
            KILL all   |  |  | Hello demux by neighbor id
                       v  v  v
                 +------+ +------+ +------+
                 | NSM0 | | NSM1 | | NSM2 |
                 +------+ +------+ +------+
```

接口 `IF_DOWN` 时对 NSM0/1/2 各投一次 `KILL_NBR`。某个邻居 DeadInterval 到期，只向 **那一台** NSM 投递 Kill，ISM 可以仍是 `POINT_TO_POINT`。

本课代码只有一个 `nsm` 字段。练习「中等」要求你加第二个。

**复杂度**：ISM 1 + NSM N，不是 1×N 乘积枚举。

**工业用途**：广播网、多点对等。

**常见缺陷**：三个邻居就起三份 ISM；或一个 `nsm_state` 无法表达「邻居 A Full、邻居 B Init」。

**重构**：邻居表用数组或哈希，键是 Router ID。查找失败等于那台 NSM 不存在，不要因此改 ISM。
