# Go Advanced Engineering Guide

> **A comprehensive Go guide for experienced systems programmers**
> 
> Not a syntax tour. Not a beginner tutorial. This is an engineering-focused deep dive into Go's design philosophy, concurrency model, and production patterns.

```
╔═══════════════════════════════════════════════════════════════════════════╗
║                         GO ADVANCED GUIDE                                 ║
║                                                                           ║
║   Target Audience: C/C++/Java engineers moving to Go                      ║
║   Focus: Engineering problems and production-grade solutions              ║
║   Style: Why it works, not just how to use it                             ║
║                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════╝
```

---

## Structure

Each topic follows a consistent 7-part structure:

1. **Engineering Problem** - What real-world problem does this solve?
2. **Core Mental Model** - How Go expects you to think
3. **Language Mechanism** - The exact Go features involved
4. **Idiomatic Usage** - When and how to use it properly
5. **Common Pitfalls** - Bugs, edge cases, production traps
6. **Complete Example** - Realistic, compilable code
7. **Design Takeaways** - Rules of thumb for architecture

---

## Section I: Go Execution & Concurrency Model ✅

This is the most critical section. Go's concurrency model is its defining feature.

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Goroutines | [01-goroutines.md](01-concurrency/01-goroutines.md) | Lightweight execution units, M:N scheduling |
| 2 | Go Scheduler | [02-go-scheduler.md](01-concurrency/02-go-scheduler.md) | GMP model, GOMAXPROCS, scheduling points |
| 3 | Channels | [03-channels.md](01-concurrency/03-channels.md) | Typed conduits, buffered vs unbuffered |
| 4 | select Statement | [04-select.md](01-concurrency/04-select.md) | Channel multiplexing, timeouts |
| 5 | Goroutine Lifetime | [05-goroutine-lifetime.md](01-concurrency/05-goroutine-lifetime.md) | Lifecycle management, shutdown |
| 6 | Goroutine Leaks | [06-goroutine-leaks.md](01-concurrency/06-goroutine-leaks.md) | Detection, prevention |
| 7 | context.Context | [07-context.md](01-concurrency/07-context.md) | Cancellation, request scoping |
| 8 | sync Primitives | [08-sync-primitives.md](01-concurrency/08-sync-primitives.md) | Mutex, WaitGroup, Once, Pool |
| 9 | Data Races | [09-data-races.md](01-concurrency/09-data-races.md) | Race detector, prevention |
| 10 | Go Memory Model | [10-memory-model.md](01-concurrency/10-memory-model.md) | Happens-before guarantees |

---

## Section II: Type System & Program Structure ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Structs & Composition | [01-structs-composition.md](02-type-system/01-structs-composition.md) | Composition over inheritance |
| 2 | Embedded Structs | [02-embedding.md](02-type-system/02-embedding.md) | Promotion, not inheritance |
| 3 | Interfaces | [03-interfaces.md](02-type-system/03-interfaces.md) | Implicit implementation |
| 4 | Value vs Pointer | [04-value-vs-pointer.md](02-type-system/04-value-vs-pointer.md) | Semantics, copying |
| 5 | Method Receivers | [05-method-receivers.md](02-type-system/05-method-receivers.md) | T vs *T |
| 6 | Zero Values | [06-zero-values.md](02-type-system/06-zero-values.md) | Zero-cost initialization |

---

## Section III: Memory & Resource Management ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Garbage Collection | [01-garbage-collection.md](03-memory/01-garbage-collection.md) | Concurrent GC, GOGC |
| 2 | Escape Analysis | [02-escape-analysis.md](03-memory/02-escape-analysis.md) | Stack vs heap |
| 3 | Allocation Behavior | [03-allocation-behavior.md](03-memory/03-allocation-behavior.md) | Cost, patterns |
| 4 | Object Lifetime | [04-object-lifetime.md](03-memory/04-object-lifetime.md) | References, cleanup |
| 5 | defer | [05-defer.md](03-memory/05-defer.md) | Controlled RAII |

---

## Section IV: Error Handling & Reliability ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Error Values | [01-error-values.md](04-errors/01-error-values.md) | error interface, wrapping |
| 2 | Sentinel Errors | [02-sentinel-errors.md](04-errors/02-sentinel-errors.md) | Package-level errors |
| 3 | Error Wrapping | [03-error-wrapping.md](04-errors/03-error-wrapping.md) | errors.Is, errors.As |
| 4 | Error Boundaries | [04-error-boundaries.md](04-errors/04-error-boundaries.md) | API design |
| 5 | panic vs error | [05-panic-vs-error.md](04-errors/05-panic-vs-error.md) | When to use each |
| 6 | recover | [06-recover.md](04-errors/06-recover.md) | Crash containment |

---

## Section V: Standard Library Architecture ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | net/http | [01-net-http.md](05-stdlib/01-net-http.md) | Handler model, middleware |
| 2 | io.Reader/Writer | [02-io-reader-writer.md](05-stdlib/02-io-reader-writer.md) | Composable I/O |
| 3 | encoding/json | [03-encoding-json.md](05-stdlib/03-encoding-json.md) | Struct tags |
| 4 | os and os/exec | [04-os-exec.md](05-stdlib/04-os-exec.md) | System interaction |
| 5 | Signal Handling | [05-signal-handling.md](05-stdlib/05-signal-handling.md) | Graceful shutdown |
| 6 | time Package | [06-time-package.md](05-stdlib/06-time-package.md) | Timeouts, tickers |

---

## Section VI: Project Structure & Modules ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Package Boundaries | [01-package-boundaries.md](06-project/01-package-boundaries.md) | Visibility, naming |
| 2 | API Design | [02-api-design.md](06-project/02-api-design.md) | Stable interfaces |
| 3 | Go Modules | [03-go-modules.md](06-project/03-go-modules.md) | go.mod, versioning |
| 4 | Versioning | [04-versioning.md](06-project/04-versioning.md) | Semantic versioning |
| 5 | Project Layouts | [05-project-layouts.md](06-project/05-project-layouts.md) | /cmd, /internal, /pkg |

---

## Section VII: Testing & Tooling ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | go test | [01-go-test.md](07-testing/01-go-test.md) | Test functions |
| 2 | Table-Driven Tests | [02-table-driven-tests.md](07-testing/02-table-driven-tests.md) | Test patterns |
| 3 | Subtests | [03-subtests.md](07-testing/03-subtests.md) | t.Run |
| 4 | Benchmarks | [04-benchmarks.md](07-testing/04-benchmarks.md) | Performance testing |
| 5 | Race Detector | [05-race-detector.md](07-testing/05-race-detector.md) | -race flag |
| 6 | Profiling | [06-profiling.md](07-testing/06-profiling.md) | pprof |

---

## Section VIII: Performance & Production ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Allocation-Aware Coding | [01-allocation-aware.md](08-performance/01-allocation-aware.md) | Reduce GC pressure |
| 2 | Slice & Map Behavior | [02-slice-map-behavior.md](08-performance/02-slice-map-behavior.md) | Internals |
| 3 | Buffer Reuse | [03-buffer-reuse.md](08-performance/03-buffer-reuse.md) | sync.Pool |
| 4 | When to Optimize | [04-when-to-optimize.md](08-performance/04-when-to-optimize.md) | Profile first |
| 5 | When Not to Use Go | [05-when-not-go.md](08-performance/05-when-not-go.md) | Right tool |

---

## Section IX: Architecture & Design Patterns ✅

| # | Topic | File | Key Concepts |
|---|-------|------|--------------|
| 1 | Dependency Injection | [01-dependency-injection.md](09-architecture/01-dependency-injection.md) | Constructor injection |
| 2 | Interface-Driven Design | [02-interface-driven-design.md](09-architecture/02-interface-driven-design.md) | Accept interfaces |
| 3 | Composition Patterns | [03-composition-patterns.md](09-architecture/03-composition-patterns.md) | No inheritance |
| 4 | Concurrency Patterns | [04-concurrency-patterns.md](09-architecture/04-concurrency-patterns.md) | Worker pools, pipelines |
| 5 | Anti-Patterns | [05-anti-patterns.md](09-architecture/05-anti-patterns.md) | What to avoid |

---

## Progress Summary

| Section | Topics | Status |
|---------|--------|--------|
| I. Concurrency | 10/10 | ✅ Complete |
| II. Type System | 6/6 | ✅ Complete |
| III. Memory | 5/5 | ✅ Complete |
| IV. Errors | 6/6 | ✅ Complete |
| V. Stdlib | 6/6 | ✅ Complete |
| VI. Project | 5/5 | ✅ Complete |
| VII. Testing | 6/6 | ✅ Complete |
| VIII. Performance | 5/5 | ✅ Complete |
| IX. Architecture | 5/5 | ✅ Complete |

**Total: 54/54 topics completed**

---

## Learning Path

### Critical Path (Essential for Production)

```
01-concurrency/01-goroutines.md     → Execution model
01-concurrency/03-channels.md       → Synchronization
01-concurrency/07-context.md        → Cancellation
01-concurrency/09-data-races.md     → Run -race always
02-type-system/03-interfaces.md     → Polymorphism
04-errors/01-error-values.md        → Error handling
03-memory/05-defer.md               → Resource cleanup
```

### Deep Dive (Complex Systems)

```
01-concurrency/02-go-scheduler.md   → Scheduling
01-concurrency/08-sync-primitives.md → When channels aren't enough
01-concurrency/10-memory-model.md   → Guarantees
03-memory/01-garbage-collection.md  → GC tuning
09-architecture/04-concurrency-patterns.md → Production patterns
```

---

## Key Takeaways

### Concurrency
- Goroutines are NOT threads - lighter (2KB stack)
- Every goroutine needs a termination condition
- Use channels for communication, mutex for state
- Always run with `-race` in CI

### Type System
- Composition over inheritance
- Embedding promotes methods, doesn't override
- Accept interfaces, return structs
- Keep interfaces small (1-3 methods)

### Errors
- Errors are values, not exceptions
- Use `%w` to wrap, `errors.Is/As` to check
- Don't compare with `==`
- Wrap at package boundaries

### Memory
- GC is concurrent, low-latency
- Pre-allocate slices when size is known
- Use sync.Pool for frequently allocated objects
- Profile before optimizing

### Patterns
- Worker pools for bounded parallelism
- Pipelines for staged processing
- Fan-out/fan-in for parallel aggregation
- Always close output channels

---

## Reference

This guide is informed by the `routermgr` gRPC server code pattern:

```go
var (
    addressMutex sync.Mutex
    routeMutex   sync.Mutex
)

var RouterAddresses map[uint32]map[int]RouterAddress
var VmcAddresses map[uint32]map[string]map[int]VmcAddress
```

Key patterns demonstrated:
- Mutex protection for shared state
- gRPC server implementation
- Route management with VRF support
- Concurrent map access patterns

---

*This guide is designed for engineers who want to understand Go deeply, not just use it superficially.*
