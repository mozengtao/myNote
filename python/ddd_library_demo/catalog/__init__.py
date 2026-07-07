"""
Catalog Bounded Context

Responsibility
--------------
Owns the bibliographic truth of the library: WHICH titles exist, under
which ISBN, by which author, and whether that title is currently active
in the catalog or retired. The Catalog context does NOT know anything
about physical copies, loans, members, or reservations - that is the
Borrowing context's job.

DDD Concept: Bounded Context
-----------------------------
A Bounded Context is a boundary within which a particular domain model
(its vocabulary, rules, and aggregates) applies consistently. The word
"Book" means something different here than it does in `borrowing/`:
- In Catalog, a `Book` is a bibliographic record (title/author/ISBN).
- In Borrowing, a "book" is represented only by its `ISBN` value plus a
  count of copies available for lending (`LoanableBook`) - the Borrowing
  context does not need or want the full bibliographic model.

Keeping these concepts in separate packages, with their own `domain/` and
`application/` layers, is what makes the two contexts independently
understandable, testable, and (in a real system) independently
deployable. They interact only through `catalog.domain.events` -
Borrowing subscribes to `BookRegistered`/`BookRetired`; it never imports
`catalog.domain.entities`.
"""
