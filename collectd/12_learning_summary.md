# collectd Architecture Learning Summary

## Part 12: Comprehensive Learning Outcome Checklist

### 12.1 Architecture Overview

**What you should now be able to explain:**
- [ ] collectd as a time-series data processing pipeline
- [ ] The separation between read plugins, core dispatch, and write plugins
- [ ] Why collectd uses a daemon model instead of a library
- [ ] The pull-based (interval-driven) collection model

**What code you should be able to navigate:**
- [ ] `src/daemon/collectd.c` - main entry point and event loop
- [ ] `src/daemon/plugin.c` - plugin management infrastructure
- [ ] `src/daemon/plugin.h` - public API and data structures

**What design trade-offs you should understand:**
- [ ] Static memory allocation vs. dynamic flexibility
- [ ] C implementation vs. safer languages
- [ ] Daemon isolation vs. library embeddability

---

### 12.2 Process Model

**What you should now be able to explain:**
- [ ] The `main()` initialization sequence
- [ ] Configuration file parsing and plugin loading
- [ ] Thread pool creation for read and write workers
- [ ] The main event loop structure

**What code you should be able to navigate:**
- [ ] `configure_collectd()` in `src/daemon/collectd.c`
- [ ] `do_init()`, `do_loop()`, `do_shutdown()` functions
- [ ] Signal handler setup

**What design trade-offs you should understand:**
- [ ] Separate initialization phases for deterministic startup
- [ ] Pre-allocated thread pools vs. dynamic scaling
- [ ] Synchronous shutdown for clean termination

---

### 12.3 Plugin Architecture

**What you should now be able to explain:**
- [ ] Plugin types: read, write, flush, shutdown, config, notification
- [ ] Function pointer ABI for plugin callbacks
- [ ] The `module_register()` entry point convention
- [ ] Plugin context and interval inheritance

**What code you should be able to navigate:**
- [ ] `plugin_register_*()` functions in `plugin.c`
- [ ] `plugin_load_file()` for dynamic loading
- [ ] Callback list structures (`list_init`, `list_write`, etc.)

**What design trade-offs you should understand:**
- [ ] Simple function pointers vs. object-oriented interfaces
- [ ] Global registration vs. instance management
- [ ] Convention-based ABI vs. explicit versioning

---

### 12.4 Scheduling and Intervals

**What you should now be able to explain:**
- [ ] How `read_heap` prioritizes plugin execution
- [ ] Per-plugin interval overrides
- [ ] The relationship between intervals and RRD storage
- [ ] Jitter and timing compensation

**What code you should be able to navigate:**
- [ ] `plugin_read_thread()` in `plugin.c`
- [ ] `read_func_t` structure and heap operations
- [ ] `cdtime_t` arithmetic in `utils_time.c`

**What design trade-offs you should understand:**
- [ ] Priority queue vs. round-robin scheduling
- [ ] Best-effort timing vs. real-time guarantees
- [ ] Coarse intervals for efficiency vs. fine-grained precision

---

### 12.5 Core Data Model

**What you should now be able to explain:**
- [ ] `value_t` union for GAUGE, COUNTER, DERIVE, ABSOLUTE
- [ ] `value_list_t` structure and field semantics
- [ ] `data_set_t` as type schema from `types.db`
- [ ] Identifier format: host/plugin-plugin_instance/type-type_instance

**What code you should be able to navigate:**
- [ ] Data structure definitions in `plugin.h`
- [ ] `types_list.c` for types.db parsing
- [ ] `format_name()` for identifier string generation

**What design trade-offs you should understand:**
- [ ] Fixed-size fields vs. dynamic allocation
- [ ] Five-part identifier vs. arbitrary labels
- [ ] Union type vs. separate structures

---

### 12.6 Dispatch Path

**What you should now be able to explain:**
- [ ] Flow from read plugin through dispatch to write plugins
- [ ] Value cache update and rate calculation
- [ ] Filter chain processing (pre-cache and post-cache)
- [ ] Write queue and thread handoff

**What code you should be able to navigate:**
- [ ] `plugin_dispatch_values()` and `plugin_dispatch_values_internal()`
- [ ] `filter_chain.c` rule processing
- [ ] `utils_cache.c` update logic

**What design trade-offs you should understand:**
- [ ] Synchronous filter processing vs. async queueing
- [ ] Centralized cache vs. per-plugin state
- [ ] Queue-based decoupling vs. direct callbacks

---

### 12.7 Threading Model

**What you should now be able to explain:**
- [ ] Read thread pool and heap-based work distribution
- [ ] Write thread pool and queue consumption
- [ ] Cache mutex and callback serialization
- [ ] Thread-local plugin context

**What code you should be able to navigate:**
- [ ] `plugin_read_thread()` and `plugin_write_thread()`
- [ ] Thread creation in `plugin_init_all()`
- [ ] `pthread_key_t` for thread-local storage

**What design trade-offs you should understand:**
- [ ] Fixed thread pools vs. dynamic scaling
- [ ] Coarse-grained locking vs. fine-grained concurrency
- [ ] Callback isolation vs. shared state

---

### 12.8 Write Plugins

**What you should now be able to explain:**
- [ ] Push vs. pull write models
- [ ] Stateful vs. stateless write plugins
- [ ] RRD interval alignment requirements
- [ ] Network plugin batching and buffering

**What code you should be able to navigate:**
- [ ] `write_rrd.c` file management and caching
- [ ] `write_prometheus.c` metric exposition
- [ ] `network.c` binary protocol

**What design trade-offs you should understand:**
- [ ] Write latency vs. batching efficiency
- [ ] Local storage vs. network transport
- [ ] Format flexibility vs. performance

---

### 12.9 Configuration System

**What you should now be able to explain:**
- [ ] `liboconfig` tree structure
- [ ] Global options vs. plugin blocks
- [ ] Complex config callbacks and `oconfig_item_t`
- [ ] TypesDB and PluginDir handling

**What code you should be able to navigate:**
- [ ] `configfile.c` dispatch functions
- [ ] `cf_util_get_*()` helper functions
- [ ] Plugin config callback registration

**What design trade-offs you should understand:**
- [ ] Declarative config vs. programmatic setup
- [ ] Static parsing vs. dynamic validation
- [ ] Hierarchical blocks vs. flat key-value

---

### 12.10 Error Handling and Logging

**What you should now be able to explain:**
- [ ] Return value conventions (0 success, non-zero error)
- [ ] Logging levels and macros (ERROR, WARNING, NOTICE, INFO, DEBUG)
- [ ] Log plugin architecture
- [ ] Error propagation patterns

**What code you should be able to navigate:**
- [ ] Logging macros in `common.h` and `plugin.h`
- [ ] `plugin_log()` implementation
- [ ] Error handling in example plugins

**What design trade-offs you should understand:**
- [ ] Simple return codes vs. rich error types
- [ ] Centralized logging vs. structured logs
- [ ] Fail-fast vs. graceful degradation

---

### 12.11 Plugin Development

**What you should now be able to explain:**
- [ ] Read plugin lifecycle and data submission
- [ ] Write plugin callback contract
- [ ] Configuration handling patterns
- [ ] Memory ownership and cleanup

**What code you should be able to navigate:**
- [ ] `src/load.c` - simple read plugin
- [ ] `src/cpu.c` - complex read plugin
- [ ] `src/write_log.c` - write plugin example

**What design trade-offs you should understand:**
- [ ] Simplicity vs. feature completeness
- [ ] Platform abstraction vs. native optimization
- [ ] Plugin autonomy vs. framework integration

---

### 12.12 Architecture Mapping

**What you should now be able to explain:**
- [ ] Mapping collectd patterns to generic C designs
- [ ] Plugin system as callback registry pattern
- [ ] Time-series pipeline as producer-consumer
- [ ] Configuration as typed tree structure

**What concepts you should be able to reuse:**
- [ ] Heap-based task scheduling
- [ ] Write queue decoupling
- [ ] Function pointer callback ABI
- [ ] Thread pool work distribution

**What patterns you should recognize:**
- [ ] Global singleton with initialization guards
- [ ] AVL tree for keyed lookup
- [ ] Linked list for ordered traversal
- [ ] Pthread-based concurrency

---

### 12.13 Limitations Understanding

**What you should now be able to explain:**
- [ ] Scaling limits (throughput, cardinality, latency)
- [ ] When Prometheus is a better choice
- [ ] Why collectd can't be embedded as a library
- [ ] High-cardinality metric limitations

**What scenarios you should recognize as unsuitable:**
- [ ] Dynamic container environments
- [ ] Sub-second alerting requirements
- [ ] High-cardinality label dimensions
- [ ] Embedded agent use cases

**What alternatives you should consider:**
- [ ] Prometheus for cloud-native monitoring
- [ ] OpenTelemetry for application instrumentation
- [ ] InfluxDB for high-cardinality time series
- [ ] Custom solutions for specialized requirements

---

## 12.2 Quick Reference Card

### Data Structure Sizes

| Structure | Size | Notes |
|-----------|------|-------|
| `identifier_t` | ~320 bytes | 5 × 64 byte strings |
| `value_t` | 8 bytes | Union of numeric types |
| `value_list_t` | ~400 bytes | Identifier + metadata |
| `data_source_t` | ~128 bytes | Name + type + range |
| `data_set_t` | variable | Array of data sources |

### Key Functions

| Function | Purpose |
|----------|---------|
| `plugin_register_read()` | Register periodic read callback |
| `plugin_register_write()` | Register value consumer |
| `plugin_dispatch_values()` | Submit metrics to core |
| `cf_register_complex()` | Register config callback |
| `uc_update()` | Update value cache |
| `fc_process_chain()` | Process filter rules |

### Thread Safety

| Resource | Protection |
|----------|------------|
| `cache_tree` | `cache_lock` mutex |
| `read_heap` | `read_lock` mutex |
| `write_queue` | `write_lock` mutex |
| Plugin context | `pthread_key_t` TLS |
| Callback lists | `plugins_lock` mutex |

### Return Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| -1 | General error |
| -ENOENT | Not found |
| -ENOMEM | Allocation failed |

---

## 12.3 Files Index

### Core Daemon

| File | Description |
|------|-------------|
| `src/daemon/collectd.c` | Main entry point, event loop |
| `src/daemon/plugin.c` | Plugin management |
| `src/daemon/plugin.h` | Public API, data structures |
| `src/daemon/configfile.c` | Configuration parsing |
| `src/daemon/filter_chain.c` | Match and target processing |
| `src/daemon/utils_cache.c` | Value cache |
| `src/daemon/types_list.c` | Types.db parsing |

### Example Plugins

| File | Type | Description |
|------|------|-------------|
| `src/load.c` | Read | System load average |
| `src/cpu.c` | Read | CPU utilization |
| `src/df.c` | Read | Disk space |
| `src/write_log.c` | Write | Log output |
| `src/write_graphite.c` | Write | Graphite protocol |
| `src/network.c` | Read/Write | Binary network protocol |

---

## 12.4 ASCII Architecture Diagram

```
                    COLLECTD ARCHITECTURE
                    =====================

  +------------------+     Configuration
  | collectd.conf    |-------------------+
  +------------------+                   |
         |                               |
         v                               v
  +------+------+              +------------------+
  | types.db    |              | cf_read()        |
  +--------------+              +------------------+
         |                               |
         v                               v
  +--------------+              +------------------+
  | data_set_t   |              | dispatch_load()  |
  | registry     |              +------------------+
  +--------------+                       |
                                         v
                              +--------------------+
                              | plugin_load_file() |
                              +--------------------+
                                         |
                                         v
  +-------------------------------------------------------------------+
  |                         PLUGIN INFRASTRUCTURE                      |
  |                                                                   |
  |  +-----------+    +-----------+    +-----------+    +-----------+ |
  |  | Read      |    | Write     |    | Flush     |    | Shutdown  | |
  |  | Callbacks |    | Callbacks |    | Callbacks |    | Callbacks | |
  |  +-----------+    +-----------+    +-----------+    +-----------+ |
  +-------------------------------------------------------------------+
         |                   ^
         |                   |
         v                   |
  +-------------+     +-------------+
  | read_heap   |     | write_queue |
  +-------------+     +-------------+
         |                   ^
         v                   |
  +-------------+     +-------------+
  | Read Thread |---->| value_list_t|
  | Pool        |     +-------------+
  +-------------+            |
                             v
  +-------------------------------------------------------------------+
  |                      DISPATCH PIPELINE                            |
  |                                                                   |
  |  +---------------+    +---------------+    +---------------+      |
  |  | PreCache      |--->| uc_update()   |--->| PostCache     |      |
  |  | FilterChain   |    | (rate calc)   |    | FilterChain   |      |
  |  +---------------+    +---------------+    +---------------+      |
  +-------------------------------------------------------------------+
                             |
                             v
                      +-------------+
                      | write_queue |
                      +-------------+
                             |
                             v
                      +-------------+
                      | Write Thread|
                      | Pool        |
                      +-------------+
                             |
              +--------------+--------------+
              |              |              |
              v              v              v
        +---------+    +---------+    +---------+
        | RRD     |    | Network |    | Graphite|
        | Files   |    | Socket  |    | Server  |
        +---------+    +---------+    +---------+
```

---

## 12.5 Further Reading

### Official Resources
- collectd Wiki: https://collectd.org/wiki/
- Plugin documentation: https://collectd.org/documentation/manpages/

### Source Code Exploration
1. Start with `src/load.c` for simple plugin pattern
2. Study `src/cpu.c` for complex configuration
3. Examine `src/network.c` for protocol implementation
4. Review `src/daemon/plugin.c` for infrastructure

### Design Patterns
- Observer pattern: callback registration
- Strategy pattern: pluggable components
- Producer-consumer: read/write decoupling
- Singleton: global configuration state

---

## Acknowledgments

This documentation was created through systematic analysis of the collectd 5.x source code. The architecture represents over 15 years of development and production deployment experience in systems monitoring.

**Key Contributors to collectd:**
- Florian Forster (lead developer)
- Marc Fournier
- Sebastian Harl
- Many community contributors

The patterns documented here represent battle-tested solutions for building reliable, efficient monitoring infrastructure in C.
