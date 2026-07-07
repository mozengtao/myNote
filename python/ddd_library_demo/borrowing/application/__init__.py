"""
Borrowing / Application layer

See `catalog/application/__init__.py` for the general Application Service
rules (orchestration only, no business rules, no direct Infrastructure
imports). The same rules apply here: every service below loads a
`Library` via `LibraryRepository`, delegates to one of its business
methods, saves it, and publishes whatever events were recorded.
"""
