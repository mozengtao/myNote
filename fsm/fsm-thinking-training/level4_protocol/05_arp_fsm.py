"""Lesson 05: ARP (Level 4 - 网络协议) —— 一个"该不该用FSM"的反例

教学用简化模拟,依据 RFC826 简化建模,不是真实协议栈实现。
重点: ARP本质上更接近"请求-缓存-超时"的轻量机制,不太需要复杂FSM建模。

四个版本(刻意保留v1~v3的"重型"FSM实现,用于对比v4的轻量方案):
    v1 ARPCacheEntryV1  - Enum + match-case
    v2 ARPCacheEntryV2  - Transition Table
    v3 ARPCacheEntryV3  - State Pattern(用于对比"为了用模式而用模式"是什么样子)
    v4 ArpCache         - 轻量级替代方案: 缓存字典 + 过期时间戳,不引入任何FSM结构
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable


class ARPState(Enum):
    NO_ENTRY = auto()
    PENDING = auto()
    RESOLVED = auto()


class ARPEvent(Enum):
    NEED_RESOLVE = auto()
    RECEIVED_REPLY_MATCHING_PENDING = auto()
    RETRY = auto()
    RETRIES_EXHAUSTED = auto()
    CACHE_AGEOUT = auto()
    RECEIVED_GRATUITOUS_MATCHING_RESOLVED = auto()


class IllegalTransitionError(RuntimeError):
    pass


MAX_RETRIES = 3


# ---------------------------------------------------------------------------
# v1: Enum + match-case
# ---------------------------------------------------------------------------
class ARPCacheEntryV1:
    def __init__(self, ip_address: str) -> None:
        self.ip_address = ip_address
        self.state: ARPState = ARPState.NO_ENTRY
        self.mac_address: str | None = None
        self.retry_count: int = 0

    def handle(self, event: ARPEvent, mac_address: str = "") -> ARPState:
        match (self.state, event):
            case (ARPState.NO_ENTRY, ARPEvent.NEED_RESOLVE):
                self.retry_count = 0
                self.state = ARPState.PENDING
            case (ARPState.PENDING, ARPEvent.RECEIVED_REPLY_MATCHING_PENDING):
                self.mac_address = mac_address
                self.state = ARPState.RESOLVED
            case (ARPState.PENDING, ARPEvent.RETRY):
                if self.retry_count >= MAX_RETRIES:
                    raise IllegalTransitionError("重试已耗尽,应触发RETRIES_EXHAUSTED而非继续RETRY")
                self.retry_count += 1  # 状态不变,只是计数属性变化
            case (ARPState.PENDING, ARPEvent.RETRIES_EXHAUSTED):
                self.state = ARPState.NO_ENTRY
            case (ARPState.RESOLVED, ARPEvent.CACHE_AGEOUT):
                self.mac_address = None
                self.state = ARPState.NO_ENTRY
            case (ARPState.RESOLVED, ARPEvent.RECEIVED_GRATUITOUS_MATCHING_RESOLVED):
                self.mac_address = mac_address  # 状态不变,只是刷新映射
            case _:
                raise IllegalTransitionError(
                    f"{self.ip_address}: {self.state.name} 不接受事件 {event.name}"
                )
        return self.state


# ---------------------------------------------------------------------------
# v2: Transition Table
# ---------------------------------------------------------------------------
ARP_TRANSITIONS: dict[tuple[ARPState, ARPEvent], tuple[ARPState, str]] = {
    (ARPState.NO_ENTRY, ARPEvent.NEED_RESOLVE): (ARPState.PENDING, "广播ARP请求,重试计数置0"),
    (ARPState.PENDING, ARPEvent.RECEIVED_REPLY_MATCHING_PENDING): (ARPState.RESOLVED, "写入映射,记录缓存有效期"),
    (ARPState.PENDING, ARPEvent.RETRY): (ARPState.PENDING, "重发请求,重试计数+1(自环)"),
    (ARPState.PENDING, ARPEvent.RETRIES_EXHAUSTED): (ARPState.NO_ENTRY, "放弃解析,报告目标不可达"),
    (ARPState.RESOLVED, ARPEvent.CACHE_AGEOUT): (ARPState.NO_ENTRY, "缓存失效,清除映射"),
    (ARPState.RESOLVED, ARPEvent.RECEIVED_GRATUITOUS_MATCHING_RESOLVED): (ARPState.RESOLVED, "刷新映射(自环)"),
}


class ARPCacheEntryV2:
    def __init__(self, ip_address: str) -> None:
        self.ip_address = ip_address
        self.state: ARPState = ARPState.NO_ENTRY
        self.mac_address: str | None = None
        self.retry_count: int = 0

    def handle(self, event: ARPEvent, mac_address: str = "") -> ARPState:
        key = (self.state, event)
        if key not in ARP_TRANSITIONS:
            raise IllegalTransitionError(
                f"{self.ip_address}: {self.state.name} 不接受事件 {event.name}"
            )
        next_state, action = ARP_TRANSITIONS[key]
        marker = "(自环)" if next_state is self.state else ""
        print(f"  [v2:{self.ip_address}] {self.state.name:9s} -> {next_state.name:9s} {marker} ({action})")

        if event is ARPEvent.NEED_RESOLVE:
            self.retry_count = 0
        elif event is ARPEvent.RETRY:
            self.retry_count += 1
        elif event in (ARPEvent.RECEIVED_REPLY_MATCHING_PENDING, ARPEvent.RECEIVED_GRATUITOUS_MATCHING_RESOLVED):
            self.mac_address = mac_address
        elif event is ARPEvent.CACHE_AGEOUT:
            self.mac_address = None

        self.state = next_state
        return self.state


# ---------------------------------------------------------------------------
# v3: State Pattern —— 刻意实现,用于对比"为了用模式而用模式"
# ---------------------------------------------------------------------------
@dataclass
class _ARPStateHandler:
    state: ARPState
    transitions: dict[ARPEvent, ARPState] = field(default_factory=dict)

    def on_event(self, event: ARPEvent) -> ARPState | None:
        return self.transitions.get(event)


def _build_arp_handlers() -> dict[ARPState, _ARPStateHandler]:
    # 注意: 三个状态类的方法体几乎没有任何独特行为需要封装,
    # 这正是Step 7想让读者直接感受到的"过度设计"信号。
    return {
        ARPState.NO_ENTRY: _ARPStateHandler(ARPState.NO_ENTRY, {ARPEvent.NEED_RESOLVE: ARPState.PENDING}),
        ARPState.PENDING: _ARPStateHandler(
            ARPState.PENDING,
            {
                ARPEvent.RECEIVED_REPLY_MATCHING_PENDING: ARPState.RESOLVED,
                ARPEvent.RETRY: ARPState.PENDING,
                ARPEvent.RETRIES_EXHAUSTED: ARPState.NO_ENTRY,
            },
        ),
        ARPState.RESOLVED: _ARPStateHandler(
            ARPState.RESOLVED,
            {
                ARPEvent.CACHE_AGEOUT: ARPState.NO_ENTRY,
                ARPEvent.RECEIVED_GRATUITOUS_MATCHING_RESOLVED: ARPState.RESOLVED,
            },
        ),
    }


class ARPCacheEntryV3:
    """State Pattern版本:三个类为了表达和v1/v2完全等价的极简逻辑而存在,
    对比之下能直观感受到"重型实现"相对"轻量问题"的不匹配。
    """

    def __init__(self, ip_address: str) -> None:
        self.ip_address = ip_address
        self._handlers = _build_arp_handlers()
        self.state: ARPState = ARPState.NO_ENTRY
        self.mac_address: str | None = None
        self.retry_count = 0

    def handle(self, event: ARPEvent, mac_address: str = "") -> ARPState:
        next_state = self._handlers[self.state].on_event(event)
        if next_state is None:
            raise IllegalTransitionError(
                f"{self.ip_address}: {self.state.name} 不接受事件 {event.name}"
            )
        if event is ARPEvent.NEED_RESOLVE:
            self.retry_count = 0
        elif event is ARPEvent.RETRY:
            self.retry_count += 1
        elif event in (ARPEvent.RECEIVED_REPLY_MATCHING_PENDING, ARPEvent.RECEIVED_GRATUITOUS_MATCHING_RESOLVED):
            self.mac_address = mac_address
        elif event is ARPEvent.CACHE_AGEOUT:
            self.mac_address = None
        self.state = next_state
        return self.state


# ---------------------------------------------------------------------------
# v4: 轻量级替代方案 —— 不引入任何FSM结构,只用缓存字典 + 过期时间戳
# ---------------------------------------------------------------------------
@dataclass
class ARPCacheEntry:
    mac_address: str
    expires_at: float


class ArpCache:
    """这一版才是本课真正推荐的写法:ARP的核心诉求是"缓存管理",
    不是"流程编排",一个字典 + 时间戳判断就足够表达"命中/未命中/过期"
    这几种情况,不需要显式的State/Event/Transition结构。
    """

    def __init__(self, cache_ttl: float = 60.0, max_retries: int = MAX_RETRIES) -> None:
        self._entries: dict[str, ARPCacheEntry] = {}
        self._cache_ttl = cache_ttl
        self._max_retries = max_retries

    def resolve(self, ip_address: str, now: float, send_request: Callable[[str], str | None]) -> str | None:
        """查询IP对应的MAC地址,未命中或过期则模拟发起ARP请求解析。

        send_request: 模拟"广播ARP请求并等待应答"的函数,返回应答的MAC地址
        或 None(表示超时无应答,教学模拟中直接判定失败,不做真实重试等待)。
        """
        entry = self._entries.get(ip_address)
        if entry is not None and entry.expires_at > now:
            return entry.mac_address  # 缓存命中且未过期,直接返回

        for attempt in range(1, self._max_retries + 1):
            mac_address = send_request(ip_address)
            if mac_address is not None:
                self._entries[ip_address] = ARPCacheEntry(mac_address, now + self._cache_ttl)
                return mac_address
            print(f"  [v4] 第{attempt}次请求无应答,{'继续重试' if attempt < self._max_retries else '放弃'}")

        self._entries.pop(ip_address, None)
        return None

    def note_gratuitous(self, ip_address: str, mac_address: str, now: float) -> None:
        """收到 Gratuitous ARP:只刷新已存在的缓存条目,不主动创建新条目。"""
        if ip_address in self._entries:
            self._entries[ip_address] = ARPCacheEntry(mac_address, now + self._cache_ttl)


# ---------------------------------------------------------------------------
# 演示
# ---------------------------------------------------------------------------
def _demo_v2_full_cycle() -> None:
    print("=== v2: 正常解析成功 -> 缓存生效 -> 缓存过期 ===")
    entry = ARPCacheEntryV2("192.168.1.1")
    entry.handle(ARPEvent.NEED_RESOLVE)
    entry.handle(ARPEvent.RECEIVED_REPLY_MATCHING_PENDING, mac_address="AA:BB:CC:00:00:01")
    assert entry.state is ARPState.RESOLVED
    entry.handle(ARPEvent.CACHE_AGEOUT)
    assert entry.state is ARPState.NO_ENTRY

    print("\n=== v2: 请求重试两次后仍无应答,最终放弃 ===")
    lost = ARPCacheEntryV2("192.168.1.99")
    lost.handle(ARPEvent.NEED_RESOLVE)
    lost.handle(ARPEvent.RETRY)
    lost.handle(ARPEvent.RETRY)
    lost.handle(ARPEvent.RETRIES_EXHAUSTED)
    assert lost.state is ARPState.NO_ENTRY


def _demo_v4_lightweight_equivalent() -> None:
    print("\n=== v4: 轻量级方案得到完全一致的行为(命中/过期/重试放弃) ===")
    cache = ArpCache(cache_ttl=10.0)

    responses = iter(["AA:BB:CC:00:00:02"])
    mac = cache.resolve("10.0.0.5", now=0.0, send_request=lambda ip: next(responses, None))
    print(f"  首次解析 10.0.0.5 -> {mac}")
    assert mac == "AA:BB:CC:00:00:02"

    cached_mac = cache.resolve("10.0.0.5", now=5.0, send_request=lambda ip: None)
    print(f"  5秒后(未过期)直接命中缓存 -> {cached_mac}")
    assert cached_mac == mac  # 命中缓存,根本不会调用send_request

    expired_mac = cache.resolve("10.0.0.5", now=20.0, send_request=lambda ip: None)
    print(f"  20秒后(已过期,重新请求但无应答) -> {expired_mac}")
    assert expired_mac is None


if __name__ == "__main__":
    print("=== v1: Enum+match-case 手工验证一次基本解析 ===")
    entry_v1 = ARPCacheEntryV1("192.168.1.2")
    entry_v1.handle(ARPEvent.NEED_RESOLVE)
    entry_v1.handle(ARPEvent.RECEIVED_REPLY_MATCHING_PENDING, mac_address="AA:BB:CC:00:00:03")
    print(f"  {entry_v1.ip_address}: state={entry_v1.state.name}, mac={entry_v1.mac_address}\n")

    _demo_v2_full_cycle()
    _demo_v4_lightweight_equivalent()

    print("\n=== v3: State Pattern 版本行为与 v1/v2 完全等价(仅结构不同) ===")
    entry_v3 = ARPCacheEntryV3("192.168.1.3")
    entry_v3.handle(ARPEvent.NEED_RESOLVE)
    entry_v3.handle(ARPEvent.RECEIVED_REPLY_MATCHING_PENDING, mac_address="AA:BB:CC:00:00:04")
    print(f"  {entry_v3.ip_address}: state={entry_v3.state.name}, mac={entry_v3.mac_address}")
    print("  结论: v1/v2/v3三种FSM实现和v4的轻量方案,行为完全一致——"
          "多出来的代码量没有换来额外的表达能力。")
