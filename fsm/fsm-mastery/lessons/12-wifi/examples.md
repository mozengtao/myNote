# 第 12 课：五个代表例（会话失败恢复）

主例「关联路径 + 失败回扫描」的完整实现在 `code/`。四次握手、漫游、省电只建立直觉，不进主例代码。认证失败恢复是主例已经跑过的代码路径。

本课状态全部摊平。不要把后三个短例画成 `CONNECTED` 里面再挂一层。

---

## 例 1：关联主路径（主例）

**问题**：站点要找到 AP、认证、关联，然后才能传数据。任一步失败不能把进程打死，也不能假装还在那一步。

**状态图**：见 `LESSON.md`。

**状态表**：`IDLE` → `SCANNING` → `AUTHENTICATING` → `ASSOCIATING` → `CONNECTED`；失败汇入 `DISCONNECTED`。

**实现**：`code/c/wifi.c`、`code/cpp/wifi.cpp`、`code/python/wifi.py`。

**复杂度**：6 个扁平状态；上下文 2 个整数。

**工业用途**：典型 Wi-Fi 请求端程序的会话循环。

**常见缺陷**：失败停在 `AUTHENTICATING`；或为每种失败各做一个终点。

**重构**：恢复代码只应出现在 `DISCONNECTED` 的 `RETRY`/`TIMEOUT` 行，不要在三个失败事件上各抄一遍扫描。

---

## 例 2：四次握手（讲义，不进代码）

**问题**：RSN 网络在关联成功后还要安装单播密钥。四拍 EAPOL-Key 任一拍失败，数据面仍不能当已连接。

```
ASSOCIATING --ASSOC_OK / start 4-way--> (规格上的密钥协商，本课不实现)
拍1 AP->STA  ANonce
拍2 STA->AP  SNonce + MIC
拍3 AP->STA  install + MIC
拍4 STA->AP  ACK
成功后才相当于本课的 CONNECTED（受保护）
```

| 本课代码 | 真实 RSN |
|----------|----------|
| ASSOC_OK → CONNECTED | ASSOC_OK → 四拍 → 受保护的 Connected |
| 省略 | 某一拍 TIMEOUT → 通常拆密钥、回扫描或再关联 |

**为什么不写进主例**：四拍会再加四个局面，把「失败回扫描」挤出注意力。主例在 `ASSOC_OK` 打印 `assoc ok, connected`，并在注释里写明省略。

**复杂度（若真做）**：约 +4 个扁平状态，失败仍应汇入本课的 `DISCONNECTED`。

**工业用途**：WPA2/WPA3 个人。

**常见缺陷**：关联成功就向用户报告 Connected，DHCP 在未装密钥时失败。

**重构**：可以是关联机结束后启动另一台很小的四拍机（第 10 课多机）。不要把四拍焊进关联枚举交差。本课两者都不实现。

---

## 例 3：漫游（讲义，不进代码）

**问题**：已经关联的站点走到另一个 AP 的覆盖。理想情况是先关联新 AP，再断开旧 AP，语音不中断。

教学只建立图，不编码：

```
+------------+   信号差（策略）    +---------------+
| CONNECTED  | ------------------> | 扫描邻居      |
| (old AP)   |                     | （不要做成     |
+------------+                     |  CONNECTED 的 |
                                   |  内部名字）   |
                                   +---------------+
                                          |
                                          | reassoc ok
                                          v
                                   +------------+
                                   | CONNECTED  |
                                   | (new AP)   |
                                   +------------+
```

本课代码的 `LOST` 是 **先断再扫**：简单、会断流。漫游是另一条策略，不是本课新概念。

若硬做成扁平机，至少增加 `ROAM_SCAN`、`REASSOCIATING`。主路径变胖，恢复点却仍应是「扫描」。所以省略。

更不要写成 `CONNECTED` 下面再挂一个扫描局面——套起来的写法留到第 13 课。现在用扁平已经能讲清「失败回熟知状态」。

**复杂度（若真做）**：+2 左右扁平状态，或第 13 课再讨论如何套。

**工业用途**：手机在楼里走动、企业无缝漫游。

**常见缺陷**：漫游走 `LOST` 路径，通话先掉再连；或对着旧 BSSID 重试关联而不扫描。

**重构**：BSSID 是上下文，不是状态。`CONNECTED` 只有一个名字，当前 AP 放在 `ctx`。

---

## 例 4：省电（讲义，不进代码）

**问题**：已关联的站点可以睡觉，AP 帮它缓冲单播。醒来收 TIM，有数据再取。

错误画法（本课禁止）：

```
CONNECTED     POWER_SAVE     SLEEP     AWAKE
```

四个兄弟状态会让人以为睡觉时 **不再关联**。睡觉时仍是已关联站点，只是射频关了。

正确的直觉（文字，本课不画套圈实现）：

> 省电是「已连接」这个局面里的附加行为。扁平六状态没有地方干净地放下它。第 13 课才会把状态套起来。现在不要发明 `CONNECTED_SLEEP` 这种假扁平。

本课代码只有 `DATA_OK` 自环，表示链路还活着。没有睡眠事件。

**复杂度**：0（本课）；套起来之后也不应增加「是否关联」的新顶层状态。

**工业用途**：手机待机、物联网周期性上线。

**常见缺陷**：把睡眠做成与 `CONNECTED` 互斥的顶层状态，唤醒后还要重新关联。

**重构**：等到第 13 课再改这张图。本课完成扁平恢复即可。

---

## 例 5：认证失败恢复（代码路径）

**问题**：认证被拒或超时。不要停在 `AUTHENTICATING`。进入 `DISCONNECTED`，还有预算则 `RETRY`/`TIMEOUT` 回 `SCANNING`，再走认证、关联。

```
AUTHENTICATING --AUTH_FAIL / retries-- --> DISCONNECTED
DISCONNECTED --RETRY [retries left] / retry scan--> SCANNING
DISCONNECTED --RETRY [retries==0] / give up--> IDLE
```

| Current State | Event | Guard | Action | Next State |
|---------------|-------|-------|--------|------------|
| AUTHENTICATING | AUTH_FAIL | — | session fail | DISCONNECTED |
| DISCONNECTED | RETRY | retries_left > 0 | retry scan | SCANNING |
| DISCONNECTED | RETRY | retries_left == 0 | give up | IDLE |

`main` 第 2 段：失败一次再成功。第 3 段：关联失败直到放弃（同一恢复骨架，失败事件换成 `ASSOC_FAIL`）。第 5 段：扫描超时用尽预算。

这就是本课新概念的代码形状。四次握手失败若将来要接，也应汇入同一张表的 `DISCONNECTED` 行，而不是新发明 `PTK_FAILED` 终点。

**复杂度**：0 个新状态；复用主例表。

**工业用途**：信号差、AP 忙、一时收不到 Auth。

**常见缺陷**：密码错误也走满三次扫描。加监护：不可恢复原因直接 `IDLE`（中等练习）。

**重构**：`action_session_fail` 必须共用。复制粘贴三份「retries--; state = DISCONNECTED」会在某一次忘记减预算。
