"""
Infrastructure Layer

Responsibility
--------------
Provides concrete, replaceable implementations of every interface (
`Protocol`) defined in the Domain/Shared layers: repositories, the event
bus, id generation, and logging setup. Nothing here is framework-specific
or requires a real database/broker - everything is a small, in-memory
stand-in, which is exactly the point: the Domain/Application layers do
not know or care that these are "just" in-memory objects, and swapping
them for a database-backed or message-queue-backed implementation would
require touching ONLY this package.

Dependency Rule
---------------
Infrastructure is the only layer allowed to import from BOTH bounded
contexts' `domain/` packages (to implement their repository Protocols) and
from `shared/`. It must never be imported BY `catalog/domain`,
`catalog/application`, `borrowing/domain`, or `borrowing/application` -
the arrow always points inward, never outward, from Infrastructure.

    Application
        |
        v
    Domain
        ^
        |
    Infrastructure   <-- you are here
"""
