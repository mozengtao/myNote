"""
Tests demonstrating the in-memory Event Bus and Domain Event basics:
publish/subscribe dispatch and event immutability.
"""

from __future__ import annotations

from dataclasses import FrozenInstanceError, dataclass

import pytest

from infrastructure.event_bus import InMemoryEventBus
from shared.domain_event import DomainEvent


@dataclass(frozen=True, kw_only=True)
class SampleEvent(DomainEvent):
    message: str


@dataclass(frozen=True, kw_only=True)
class OtherEvent(DomainEvent):
    payload: int


class TestDomainEvent:
    def test_events_are_immutable(self) -> None:
        event = SampleEvent(message="hello")
        with pytest.raises(FrozenInstanceError):
            event.message = "changed"  # type: ignore[misc]

    def test_events_carry_an_occurred_at_timestamp(self) -> None:
        event = SampleEvent(message="hello")
        assert event.occurred_at is not None


class TestInMemoryEventBus:
    def test_publish_invokes_subscribed_handler(self) -> None:
        bus = InMemoryEventBus()
        received: list[SampleEvent] = []
        bus.subscribe(SampleEvent, received.append)

        bus.publish(SampleEvent(message="hello"))

        assert len(received) == 1
        assert received[0].message == "hello"

    def test_publish_only_notifies_subscribers_of_the_matching_type(self) -> None:
        bus = InMemoryEventBus()
        sample_received: list[SampleEvent] = []
        other_received: list[OtherEvent] = []
        bus.subscribe(SampleEvent, sample_received.append)
        bus.subscribe(OtherEvent, other_received.append)

        bus.publish(SampleEvent(message="hello"))

        assert len(sample_received) == 1
        assert len(other_received) == 0

    def test_multiple_handlers_for_the_same_event_all_run(self) -> None:
        bus = InMemoryEventBus()
        calls: list[str] = []
        bus.subscribe(SampleEvent, lambda e: calls.append("first"))
        bus.subscribe(SampleEvent, lambda e: calls.append("second"))

        bus.publish(SampleEvent(message="hello"))

        assert calls == ["first", "second"]

    def test_publishing_with_no_subscribers_does_not_raise(self) -> None:
        bus = InMemoryEventBus()
        bus.publish(SampleEvent(message="hello"))  # should simply be a no-op
