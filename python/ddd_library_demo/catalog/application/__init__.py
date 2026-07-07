"""
Catalog / Application layer

Responsibility
--------------
Orchestrates Catalog use cases (register a book, retire a book) by
coordinating the `Book` aggregate, its `BookRepository`, and the
`EventBus`. Application Services deliberately contain NO business rules
of their own - every rule (ISBN validity, "cannot retire twice", ...)
lives in `catalog/domain/`. If you find yourself writing an `if` statement
here that encodes a business rule rather than a workflow step, it belongs
in the Domain layer instead.

Dependency Rule
---------------
This layer may import `catalog/domain/` and `shared/`. It must never
import `infrastructure/` directly - dependencies on repositories/event
buses are received via constructor injection using the `Protocol`
interfaces defined in `catalog/domain/repository.py` and
`shared/domain_event.py`.
"""
