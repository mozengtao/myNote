# Go Learning Guide - Complete Index

> A comprehensive guide to mastering Go for experienced systems programmers

```
    ╔══════════════════════════════════════════════════════════════════╗
    ║                    GO LEARNING GUIDE                             ║
    ║                                                                  ║
    ║   From C/C++ Systems Programming to Production Go                ║
    ║                                                                  ║
    ║   57 Topics • 11 Sections • Complete Coverage                    ║
    ╚══════════════════════════════════════════════════════════════════╝
```

---

## Quick Navigation

| Section | Topics | Focus Area |
|---------|--------|------------|
| [I. Philosophy](#i-go-philosophy--execution-model) | 1-4 | Why Go exists, design principles |
| [II. Type System](#ii-type-system--data-model) | 5-10 | Types, structs, interfaces, methods |
| [III. Memory](#iii-memory-management--lifetime) | 11-15 | GC, escape analysis, ownership |
| [IV. Errors](#iv-error-handling--control-flow) | 16-20 | Error values, panic/recover, defer |
| [V. Concurrency](#v-concurrency-model) | 21-27 | Goroutines, channels, sync |
| [VI. Packages](#vi-packages-modules--project-structure) | 28-32 | Modules, layout, API design |
| [VII. Stdlib](#vii-standard-library-mastery) | 33-37 | net/http, json, io, os, time |
| [VIII. Testing](#viii-testing-debugging--tooling) | 38-42 | Tests, benchmarks, profiling |
| [IX. Performance](#ix-performance--optimization) | 43-47 | Fast/slow patterns, allocation |
| [X. Architecture](#x-architecture--best-practices) | 48-52 | DI, interfaces, layered design |
| [XI. Real Projects](#xi-from-learning-to-real-projects) | 53-57 | Traps, reviews, projects |

---

## I. Go Philosophy & Execution Model

*Understanding why Go exists and how it thinks*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Why Go Was Created | [01-why-go-was-created.md](01-philosophy/01-why-go-was-created.md) | Google scale, compilation speed, simplicity |
| 2 | Simplicity, Readability, Convention | [02-simplicity-readability-convention.md](01-philosophy/02-simplicity-readability-convention.md) | gofmt, naming, less is more |
| 3 | Compiled, Statically-Linked Execution Model | [03-execution-model.md](01-philosophy/03-execution-model.md) | Single binary, cross-compilation |
| 4 | Go Toolchain Overview | [04-toolchain-overview.md](01-philosophy/04-toolchain-overview.md) | go build, go run, go mod, go vet |

---

## II. Type System & Data Model

*Go's approach to types, composition, and polymorphism*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 5 | Basic Types and Zero Values | [05-basic-types-zero-values.md](02-type-system/05-basic-types-zero-values.md) | int, string, bool, zero initialization |
| 6 | Structs and Composition | [06-structs-composition.md](02-type-system/06-structs-composition.md) | Embedding, composition over inheritance |
| 7 | Interfaces | [07-interfaces.md](02-type-system/07-interfaces.md) | Implicit satisfaction, small interfaces |
| 8 | Value vs Reference Semantics | [08-value-vs-reference-semantics.md](02-type-system/08-value-vs-reference-semantics.md) | Copy behavior, slice/map internals |
| 9 | Pointers | [09-pointers.md](02-type-system/09-pointers.md) | &, *, no arithmetic, nil safety |
| 10 | Methods and Receivers | [10-methods-and-receivers.md](02-type-system/10-methods-and-receivers.md) | Value vs pointer receivers |

---

## III. Memory Management & Lifetime

*How Go handles memory without manual management*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 11 | Garbage Collection in Go | [11-garbage-collection.md](03-memory/11-garbage-collection.md) | Concurrent GC, low latency, tuning |
| 12 | Escape Analysis | [12-escape-analysis.md](03-memory/12-escape-analysis.md) | Stack vs heap, -gcflags="-m" |
| 13 | Object Lifetime and Ownership | [13-object-lifetime-ownership.md](03-memory/13-object-lifetime-ownership.md) | No ownership system, GC roots |
| 14 | Memory Leaks in Go | [14-memory-leaks-in-go.md](03-memory/14-memory-leaks-in-go.md) | Goroutine leaks, reference retention |
| 15 | GC vs Manual Memory | [15-gc-vs-manual-memory.md](03-memory/15-gc-vs-manual-memory.md) | Trade-offs, when GC wins/loses |

---

## IV. Error Handling & Control Flow

*Go's explicit, value-based error handling*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 16 | Error Values (not Exceptions) | [16-error-values.md](04-errors/16-error-values.md) | error interface, errors.Is/As |
| 17 | panic and recover | [17-panic-recover.md](04-errors/17-panic-recover.md) | When to panic, recovery patterns |
| 18 | defer | [18-defer.md](04-errors/18-defer.md) | LIFO, cleanup, capturing variables |
| 19 | Designing Error-Friendly APIs | [19-error-friendly-apis.md](04-errors/19-error-friendly-apis.md) | Wrapping, sentinel errors, types |
| 20 | Error Handling Anti-Patterns | [20-error-antipatterns.md](04-errors/20-error-antipatterns.md) | Ignoring errors, over-wrapping |

---

## V. Concurrency Model

*CSP-style concurrency: goroutines, channels, and synchronization*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 21 | Goroutines | [21-goroutines.md](05-concurrency/21-goroutines.md) | Lightweight threads, M:N scheduling |
| 22 | Channels | [22-channels.md](05-concurrency/22-channels.md) | Typed, buffered/unbuffered, ownership |
| 23 | select Statement | [23-select.md](05-concurrency/23-select.md) | Multiplexing, timeouts, non-blocking |
| 24 | sync Primitives | [24-sync-primitives.md](05-concurrency/24-sync-primitives.md) | Mutex, RWMutex, WaitGroup, Once |
| 25 | context Package | [25-context.md](05-concurrency/25-context.md) | Cancellation, deadlines, values |
| 26 | Data Races and Race Detector | [26-data-races.md](05-concurrency/26-data-races.md) | -race flag, happens-before |
| 27 | Go Memory Model | [27-memory-model.md](05-concurrency/27-memory-model.md) | Synchronization guarantees |

---

## VI. Packages, Modules & Project Structure

*Organizing Go code at scale*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 28 | Package System and Visibility | [28-package-system.md](06-packages/28-package-system.md) | Exported/unexported, package naming |
| 29 | Go Modules | [29-go-modules.md](06-packages/29-go-modules.md) | go.mod, versioning, vendoring |
| 30 | Project Layout Conventions | [30-project-layout.md](06-packages/30-project-layout.md) | cmd/, internal/, pkg/, flat |
| 31 | Designing Public APIs | [31-public-api-design.md](06-packages/31-public-api-design.md) | Stability, documentation, options |
| 32 | Avoiding Cyclic Dependencies | [32-cyclic-dependencies.md](06-packages/32-cyclic-dependencies.md) | Interface extraction, restructuring |

---

## VII. Standard Library Mastery

*The batteries-included Go standard library*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 33 | net/http for Servers and Clients | [33-net-http.md](07-stdlib/33-net-http.md) | Handlers, middleware, timeouts |
| 34 | encoding/json | [34-encoding-json.md](07-stdlib/34-encoding-json.md) | Struct tags, Marshal/Unmarshal |
| 35 | io.Reader and io.Writer | [35-io-interfaces.md](07-stdlib/35-io-interfaces.md) | Composable I/O, streaming |
| 36 | os and Filesystem | [36-os-filesystem.md](07-stdlib/36-os-filesystem.md) | Files, directories, permissions |
| 37 | time and Context Patterns | [37-time-context.md](07-stdlib/37-time-context.md) | Timers, tickers, context integration |

---

## VIII. Testing, Debugging & Tooling

*Go's built-in testing and analysis tools*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 38 | go test and Table-Driven Tests | [38-go-test.md](08-testing/38-go-test.md) | Test functions, subtests, coverage |
| 39 | Benchmarking | [39-benchmarks.md](08-testing/39-benchmarks.md) | b.N, memory stats, comparison |
| 40 | Race Detector | [40-race-detector.md](08-testing/40-race-detector.md) | -race in tests and production |
| 41 | Profiling (pprof) | [41-profiling.md](08-testing/41-profiling.md) | CPU, memory, goroutine profiles |
| 42 | Linting and Formatting | [42-linting-formatting.md](08-testing/42-linting-formatting.md) | gofmt, go vet, staticcheck |

---

## IX. Performance & Optimization

*Understanding Go's performance characteristics*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 43 | When Go is Fast | [43-when-go-is-fast.md](09-performance/43-when-go-is-fast.md) | I/O bound, concurrency, simplicity |
| 44 | When Go is Slow | [44-when-go-is-slow.md](09-performance/44-when-go-is-slow.md) | Allocations, GC pressure, bounds |
| 45 | Allocation-Aware Programming | [45-allocation-aware.md](09-performance/45-allocation-aware.md) | sync.Pool, pre-allocation, reuse |
| 46 | Avoiding Premature Optimization | [46-premature-optimization.md](09-performance/46-premature-optimization.md) | Profile first, clarity over speed |
| 47 | Go vs C/C++ Performance | [47-go-vs-c-performance.md](09-performance/47-go-vs-c-performance.md) | Trade-offs, when to use each |

---

## X. Architecture & Best Practices

*Building maintainable Go systems*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 48 | Dependency Injection | [48-dependency-injection.md](10-architecture/48-dependency-injection.md) | Constructor injection, no frameworks |
| 49 | Interface-Driven Design | [49-interface-driven-design.md](10-architecture/49-interface-driven-design.md) | Accept interfaces, return structs |
| 50 | Layered Architecture | [50-layered-architecture.md](10-architecture/50-layered-architecture.md) | Handler → Service → Repository |
| 51 | Maintainable Services | [51-maintainable-services.md](10-architecture/51-maintainable-services.md) | Configuration, logging, metrics |
| 52 | When NOT to Use Go | [52-when-not-go.md](10-architecture/52-when-not-go.md) | GUI, ML, scripting, real-time |

---

## XI. From Learning to Real Projects

*Bridging theory and practice*

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 53 | Common Beginner Traps | [53-beginner-traps.md](11-real-projects/53-beginner-traps.md) | Loop variable capture, nil interfaces |
| 54 | Code Review Checklist | [54-code-review-checklist.md](11-real-projects/54-code-review-checklist.md) | What to look for in Go code |
| 55 | Reading Open Source Go | [55-reading-go-projects.md](11-real-projects/55-reading-go-projects.md) | Recommended projects to study |
| 56 | Recommended Learning Projects | [56-learning-projects.md](11-real-projects/56-learning-projects.md) | CLI tools, servers, libraries |
| 57 | **Summary and Readiness Checklist** | [57-summary-and-checklist.md](11-real-projects/57-summary-and-checklist.md) | Final checklist, feature map |

---

## Learning Paths

### 🚀 Fast Track (Core Essentials)

For those with limited time, focus on these 20 critical topics:

```
Philosophy:     1 → 3
Type System:    5 → 6 → 7 → 9 → 10
Memory:         11 → 12
Errors:         16 → 18
Concurrency:    21 → 22 → 25 → 26
Packages:       28 → 29
Testing:        38 → 40
Summary:        57
```

### 📚 Complete Path (All 57 Topics)

Follow the numbered sequence from 1 to 57 for comprehensive understanding.

### 🔧 C/C++ Developer Path

If coming from C/C++, pay special attention to:

| Topic | Why It Matters |
|-------|---------------|
| 7 - Interfaces | No classes, no inheritance |
| 11 - GC | No malloc/free |
| 12 - Escape Analysis | Understanding allocations |
| 16 - Error Values | No exceptions |
| 21 - Goroutines | Not pthreads |
| 27 - Memory Model | Different guarantees |
| 47 - Performance | Trade-offs vs C |

---

## Topic Categories

### Must Know Before Production

```
Critical:    1, 5, 6, 7, 9, 11, 16, 18, 21, 22, 25, 26, 28, 38, 40
Important:   2, 3, 8, 10, 12, 17, 23, 24, 29, 30, 33, 35
```

### Learn As Needed

```
Specialized: 14, 15, 19, 20, 27, 31, 32, 34, 36, 37, 39, 41-52
Reference:   53-57
```

---

## File Statistics

| Section | Files | Topics |
|---------|-------|--------|
| 01-philosophy | 4 | Philosophy & Execution |
| 02-type-system | 6 | Types & Data Model |
| 03-memory | 5 | Memory Management |
| 04-errors | 5 | Error Handling |
| 05-concurrency | 7 | Concurrency Model |
| 06-packages | 5 | Packages & Modules |
| 07-stdlib | 5 | Standard Library |
| 08-testing | 5 | Testing & Tooling |
| 09-performance | 5 | Performance |
| 10-architecture | 5 | Architecture |
| 11-real-projects | 5 | Real Projects |
| **Total** | **57** | **Complete Guide** |

---

## Quick Reference

### Go Commands Cheat Sheet

```bash
go build ./...          # Build all packages
go test ./...           # Test all packages
go test -race ./...     # Test with race detector
go mod tidy             # Clean up dependencies
go vet ./...            # Static analysis
go fmt ./...            # Format code
go run main.go          # Build and run
go install              # Build and install binary
```

### Common Patterns Index

| Pattern | Topics |
|---------|--------|
| Error handling | 16, 17, 18, 19, 20 |
| Concurrency | 21, 22, 23, 24, 25 |
| Testing | 38, 39, 40 |
| HTTP services | 33, 50, 51 |
| JSON APIs | 34, 33 |
| Dependency injection | 48, 49 |

---

## Related Resources

- **[README.md](README.md)** - Project overview and introduction
- **[57-summary-and-checklist.md](11-real-projects/57-summary-and-checklist.md)** - Final readiness checklist

---

*Generated for the Go Learning Guide - 57 Topics for Systems Programmers*

