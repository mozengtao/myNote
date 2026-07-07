"""
Catalog / Domain layer

Dependency Rule
---------------
This package (and every module inside it) may import from `shared/` only.
It must NEVER import from `catalog/application`, `infrastructure/`, or
`borrowing/`. The Domain layer is the innermost layer of the architecture;
everything else depends on it, it depends on nothing project-specific.

    Application
        |
        v
    Domain   <-- you are here
        ^
        |
    Infrastructure
"""
