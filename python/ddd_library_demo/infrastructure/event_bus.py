"""
Infrastructure / In-memory Event Bus

DDD Concept: Event Bus as a replaceable Infrastructure adapter
-------------------------------------------------------------------
This class implements the `shared.domain_event.EventBus` Protocol using
nothing more than a `dict` of subscriber lists and a direct Python
function call to "deliver" an event - no threads, no queue, no network.
That is intentional: the spec explicitly asks for in-memory dispatch, not
Kafka/Celery/etc. Because Application Services depend only on the
`EventBus` Protocol (not on this class), a future
`KafkaEventBus`/`RedisEventBus` could be dropped in here without changing
a single line in `catalog/` or `borrowing/`.
"""

from __future__ import annotations

import logging
from collections import defaultdict

from shared.domain_event import DomainEvent, EventHandler

logger = logging.getLogger(__name__)


class InMemoryEventBus:
    """Synchronous, in-process publish/subscribe implementation of `EventBus`."""

    def __init__(self) -> None:
        self._subscribers: dict[type[DomainEvent], list[EventHandler]] = defaultdict(list)

    def subscribe(self, event_type: type[DomainEvent], handler: EventHandler) -> None:
        self._subscribers[event_type].append(handler)

    def publish(self, event: DomainEvent) -> None:
        handlers = self._subscribers.get(type(event), [])
        logger.info(
            "Publishing %s to %d subscriber(s)", type(event).__name__, len(handlers)
        )
        # Dispatch is synchronous and in-order: every handler for this
        # event type runs, one after another, before `publish` returns.
        # This keeps the demo's console output deterministic and easy to
        # follow - a real message-queue-backed bus would likely be async.
        for handler in handlers:
            handler(event)
