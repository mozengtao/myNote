# 第 13c 课：五个代表例（内部 / 外部）

主例「音量内部、停止与缓冲外部」在 `code/`。

---

## 例 1：播放器音量（主例）

**问题**：`PLAYING` 里调音量不能停渲染；缺数据和停止必须真正离开。

**状态图**：见 `LESSON.md`。

**状态表**：`VOLUME` 的 Kind = internal；`DATA_STARVE` / `STOP` = external。

**实现**：`code/c/player_hsm.c`、`code/cpp/player_hsm.cpp`、`code/python/player_hsm.py`。

**复杂度**：13b 的 LCA + 一个 `fsm_internal`。

**工业用途**：播放器 OSD、运行中参数。

**常见缺陷**：`transition(PLAYING)` 当音量，画面闪断。

**重构**：历史是「离开后再回来」，不要和内部转移混谈。

---

## 例 2：心跳保活

**问题**：`CONNECTED` 周期性 `KEEPALIVE`。若做成外部自环，进入动作会重装整个会话或清计数。

| Current | Event | Kind | Action | Entry/Exit |
|---------|-------|------|--------|------------|
| CONNECTED | KEEPALIVE | internal | send ping, bump counter | 无 |
| CONNECTED | LOST | external | — | exit CONNECTED |

**复杂度**：1 个内部事件。

**工业用途**：VPN、SSH、无线保活。

**常见缺陷**：心跳重进，把「已握手」又跑一遍。

**重构**：第 12 课的 `DATA_OK` 自环，在有进入 / 退出之后应改判为内部。

---

## 例 3：页面内改滑条

**问题**：设置页里拖亮度。换页是外部（卸载控件）；拖滑条是内部。

```
SETTINGS 内部: BRIGHTNESS / set
SETTINGS 外部: BACK --> HOME   （exit unmount）
```

**复杂度**：同页两个 Kind。

**工业用途**：嵌入式菜单。

**常见缺陷**：每次拖动都卸载再挂载整页，闪烁且丢焦点。

**重构**：与音量同构。

---

## 例 4：缺数据必须外部

**问题**：有人把 `DATA_STARVE` 写成内部「只显示转圈，仍停在 PLAYING」。渲染退出不跑，解码器继续饿转。

本课立场：缺数据是另一个叶子 `BUFFERING`，必须外部，才能 `stop rendering`。

| 错误 | 本课 |
|------|------|
| PLAYING 内部 STARVE / show spinner | PLAYING --external--> BUFFERING |

**复杂度**：0 行新状态；Kind 选错就会错。

**工业用途**：流媒体卡顿。

**常见缺陷**：用内部转移偷懒，框上的退出形同虚设。

**重构**：转圈属于 `BUFFERING` 的进入（13b 练习），不属于 `PLAYING` 的内部动作。

---

## 例 5：停止必须外部

**问题**：`STOP` 若内部，叶子还是 `PLAYING`，`exit PLAYING` 与 `exit ACTIVE` 都不跑。用户以为停了，渲染器和会话还在。

`STOP` 画在 `ACTIVE` 边上，只能是外部。13a 的「一支箭」加上 13b 的父退出，本课把它标成 Kind=external。

**复杂度**：已有路径，只是定性。

**工业用途**：任何「取消整段活动」。

**常见缺陷**：内部 `STOP` 只把 UI 图标改成方块。

**重构**：不要发明 `STOPPING` 叶子来逃避外部转移；先把退出跑完再进 `STOPPED`。
