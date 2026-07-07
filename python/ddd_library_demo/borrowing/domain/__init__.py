"""
Borrowing / Domain layer

Dependency Rule
---------------
This package may import from `shared/` only - never from
`borrowing/application`, `infrastructure/`, or `catalog/`. See
`catalog/domain/__init__.py` for the full explanation of this rule; it
applies identically here.
"""
