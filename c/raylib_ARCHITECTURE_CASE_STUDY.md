# Raylib Layered Software Architecture: A Case Study

**Document Type**: Professional Architecture Analysis  
**Target Audience**: Systems programmers, C library maintainers, software architects  
**Date**: December 2025

---

## Table of Contents

1. [Identify the Layers](#step-1--identify-the-layers)
2. [Dependency Direction & Rules](#step-2--dependency-direction--rules)
3. [Public API vs Internal Implementation](#step-3--public-api-vs-internal-implementation)
4. [Platform Abstraction Layer](#step-4--platform-abstraction-layer)
5. [Module-Level Layering](#step-5--module-level-layering)
6. [Evolution & Change Scenarios](#step-6--evolution--change-scenarios)
7. [Architectural Trade-offs](#step-7--architectural-trade-offs)
8. [Reusable Architecture Lessons](#step-8--architecture-lessons-i-can-reuse)

---

## STEP 1 — Identify the Layers

Raylib exhibits a **five-layer architecture** with clear dependency directions and responsibilities:

### Overview: The Onion Model

```
                    +---------------------------------------+
                    |          USER APPLICATION             |
                    |             #include "raylib.h"       |
                    +------------------+--------------------+
                                       |
                    +------------------v--------------------+
                    |     LAYER 5: PUBLIC API (raylib.h)    |
                    |  +---------------------------------+  |
                    |  |   LAYER 4: DOMAIN MODULES       |  |
                    |  |  +---------------------------+  |  |
                    |  |  | LAYER 3: CORE (rcore/rlgl)|  |  |
                    |  |  |  +---------------------+  |  |  |
                    |  |  |  | LAYER 2: PLATFORM   |  |  |  |
                    |  |  |  |  +---------------+  |  |  |  |
                    |  |  |  |  | LAYER 1:      |  |  |  |  |
                    |  |  |  |  | EXTERNAL DEPS |  |  |  |  |
                    |  |  |  |  +---------------+  |  |  |  |
                    |  |  |  +---------------------+  |  |  |
                    |  |  +---------------------------+  |  |
                    |  +---------------------------------+  |
                    +---------------------------------------+
                                       |
                    +------------------v--------------------+
                    |      OPERATING SYSTEM / HARDWARE      |
                    +---------------------------------------+
```

**中文说明**：
上图展示了 raylib 的"洋葱模型"分层架构。用户应用程序只需要包含 raylib.h 这一个头文件，
即可访问所有功能。每一层都包裹着内层，依赖方向严格从外向内。外层（如领域模块）依赖内层
（如核心层），但内层绝不依赖外层。这种设计确保了模块的独立性和可替换性。

### Simplified Layer Stack

```
    +==================================================================+
    |                                                                  |
    |   +----------------------------------------------------------+   |
    |   |  LAYER 5: raylib.h                                       |   |
    |   |  [Public Types] [Function Prototypes] [Constants]        |   |
    |   +----------------------------------------------------------+   |
    |                              ^                                   |
    |                              | implements                        |
    |   +----------------------------------------------------------+   |
    |   |  LAYER 4: Domain Modules                                 |   |
    |   |  rtextures | rtext | rshapes | rmodels | raudio          |   |
    |   +----------------------------------------------------------+   |
    |                              ^                                   |
    |                              | uses                              |
    |   +----------------------------------------------------------+   |
    |   |  LAYER 3: Core Infrastructure                            |   |
    |   |  rcore.c | rlgl.h | raymath.h | utils.c | config.h       |   |
    |   +----------------------------------------------------------+   |
    |                              ^                                   |
    |                              | #include                          |
    |   +----------------------------------------------------------+   |
    |   |  LAYER 2: Platform Abstraction                           |   |
    |   |  rcore_glfw | rcore_sdl | rcore_web | rcore_android ...  |   |
    |   +----------------------------------------------------------+   |
    |                              ^                                   |
    |                              | links                             |
    |   +----------------------------------------------------------+   |
    |   |  LAYER 1: External Dependencies                          |   |
    |   |  GLFW | miniaudio | stb_* | glad | cgltf | RGFW          |   |
    |   +----------------------------------------------------------+   |
    |                              ^                                   |
    |                              | calls                             |
    |   +----------------------------------------------------------+   |
    |   |  OPERATING SYSTEM APIs                                   |   |
    |   |  Win32 | POSIX | Cocoa | Android NDK | Emscripten        |   |
    |   +----------------------------------------------------------+   |
    |                                                                  |
    +==================================================================+
```

**中文说明**：
此图展示了 raylib 五层架构的堆叠关系。箭头方向表示依赖关系：上层依赖下层。
- 第5层（raylib.h）：对外公开的 API 接口，用户唯一需要引入的头文件
- 第4层（领域模块）：实现具体功能（纹理、文字、形状、模型、音频）
- 第3层（核心基础设施）：提供 OpenGL 抽象、数学库、工具函数等
- 第2层（平台抽象）：隔离不同操作系统的差异，编译时只选择一个
- 第1层（外部依赖）：第三方库，全部以源码形式内嵌

### Layer 1: External Dependencies Layer (Vendor/Third-Party)

| Attribute | Description |
|-----------|-------------|
| **Directory(s)** | `src/external/` |
| **Files** | ~128 files including `stb_*.h`, `miniaudio.h`, `cgltf.h`, `RGFW.h`, `glfw/` |
| **Responsibility** | Provide low-level functionality: image decoding, audio playback, 3D model parsing, OpenGL loading, window management backends |
| **Dependencies (allowed)** | Operating system APIs, C standard library |
| **Dependencies (forbidden)** | Any raylib code; these must remain standalone |

**Key Characteristics**:
- All external libraries are **header-only** or **single-file** implementations
- Libraries are vendored directly (no package manager dependency)
- Each library is self-contained with its own memory allocator hooks

### Layer 2: Platform Abstraction Layer (PAL)

| Attribute | Description |
|-----------|-------------|
| **Directory(s)** | `src/platforms/` |
| **Files** | `rcore_desktop_glfw.c`, `rcore_desktop_rgfw.c`, `rcore_desktop_sdl.c`, `rcore_desktop_win32.c`, `rcore_drm.c`, `rcore_android.c`, `rcore_web.c`, `rcore_memory.c`, `rcore_template.c` |
| **Responsibility** | Abstract OS/platform-specific window creation, input handling, timing, graphics context initialization |
| **Dependencies (allowed)** | Layer 1 (external libs), OS APIs, Core state (`CoreData CORE`) |
| **Dependencies (forbidden)** | Higher-level modules (rtext, rtextures, rmodels, raudio) |

**Key Characteristics**:
- Only ONE platform file is compiled per build (compile-time selection)
- Each platform file implements the same function contract: `InitPlatform()`, `ClosePlatform()`, platform-specific `WindowShouldClose()`, `ToggleFullscreen()`, etc.
- Accessed via `extern CoreData CORE` global state

### Layer 3: Core Infrastructure Layer

| Attribute | Description |
|-----------|-------------|
| **Directory(s)** | `src/` (root level) |
| **Files** | `rcore.c`, `utils.c`, `utils.h`, `rlgl.h`, `raymath.h`, `rcamera.h`, `rgestures.h`, `config.h` |
| **Responsibility** | Window lifecycle, main loop, input state, timing, OpenGL abstraction, math utilities, configuration |
| **Dependencies (allowed)** | Layer 1, Layer 2 (via `#include "platforms/rcore_*.c"`) |
| **Dependencies (forbidden)** | Domain modules (except for optional callbacks like `LoadFontDefault()`) |

**Key Characteristics**:
- `rcore.c` is the **orchestrator** — it `#include`s the appropriate platform file
- `rlgl.h` provides the **graphics abstraction** independent of OpenGL version
- `raymath.h` and `rcamera.h` are **standalone** — usable without raylib
- `config.h` controls compile-time feature selection

### Layer 4: Domain/Feature Modules Layer

| Attribute | Description |
|-----------|-------------|
| **Directory(s)** | `src/` (root level) |
| **Files** | `rtextures.c`, `rtext.c`, `rshapes.c`, `rmodels.c`, `raudio.c` |
| **Responsibility** | High-level domain functionality: textures/images, text/fonts, 2D shapes, 3D models, audio |
| **Dependencies (allowed)** | Layer 1 (external libs for file format support), Layer 3 (rlgl, raymath, utils) |
| **Dependencies (forbidden)** | Each other (with minor exceptions), platform layer directly |

**Key Characteristics**:
- Each module is **independently compilable** if `SUPPORT_MODULE_*` is enabled
- Modules communicate through Layer 3 abstractions (rlgl for rendering)
- Audio module (`raudio.c`) is **completely independent** — can be built standalone

### Layer 5: Public API Layer

| Attribute | Description |
|-----------|-------------|
| **Directory(s)** | `src/` |
| **Files** | `raylib.h` |
| **Responsibility** | Define the **single public interface** for all raylib functionality |
| **Dependencies (allowed)** | None (header only defines types and function prototypes) |
| **Dependencies (forbidden)** | Implementation details, internal headers |

**Key Characteristics**:
- **~602 public API functions** exposed via `RLAPI` macro
- Contains all public type definitions (`Vector2`, `Texture2D`, `Color`, etc.)
- Users include ONLY this header

---

## STEP 2 — Dependency Direction & Rules

### Dependency Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Layer 5: PUBLIC API                             │
│                        (raylib.h)                                   │
│   [Types, Function Declarations, Constants, Enums]                  │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ declares
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                  Layer 4: DOMAIN MODULES                            │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│   │rtextures │ │  rtext   │ │ rshapes  │ │ rmodels  │ │  raudio  │  │
│   │   .c     │ │   .c     │ │   .c     │ │   .c     │ │   .c     │  │
│   └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘  │
│        │            │            │            │            │        │
└────────┼────────────┼────────────┼────────────┼────────────┼────────┘
         │            │            │            │            │
         ▼            ▼            ▼            ▼            │
┌─────────────────────────────────────────────────────┐      │
│              Layer 3: CORE INFRASTRUCTURE           │      │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐       │      │
│  │ rcore  │ │  rlgl  │ │raymath │ │ utils  │       │      │
│  │  .c    │ │   .h   │ │   .h   │ │  .c/h  │       │      │
│  └───┬────┘ └────────┘ └────────┘ └────────┘       │      │
│      │                                              │      │
└──────┼──────────────────────────────────────────────┘      │
       │ #include                                            │
       ▼                                                     │
┌─────────────────────────────────────────────────────┐      │
│         Layer 2: PLATFORM ABSTRACTION               │      │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐         │      │
│  │rcore_glfw │ │rcore_sdl  │ │rcore_web  │  ...    │      │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘         │      │
│        │             │             │                │      │
└────────┼─────────────┼─────────────┼────────────────┘      │
         │             │             │                       │
         ▼             ▼             ▼                       ▼
┌─────────────────────────────────────────────────────────────────────┐
│                 Layer 1: EXTERNAL DEPENDENCIES                      │
│  ┌──────┐ ┌──────────┐ ┌─────────┐ ┌──────┐ ┌──────────┐ ┌───────┐ │
│  │ GLFW │ │ miniaudio│ │ stb_*   │ │ glad │ │  cgltf   │ │ RGFW  │ │
│  └──────┘ └──────────┘ └─────────┘ └──────┘ └──────────┘ └───────┘ │
└─────────────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────────┐
│              OPERATING SYSTEM / HARDWARE                            │
│    [Win32 | X11/Wayland | Cocoa | Android NDK | WebGL/Emscripten]   │
└─────────────────────────────────────────────────────────────────────┘
```

### Data Flow: A Typical Frame

```
    USER CODE                    RAYLIB INTERNALS
    =========                    ================

    main() {
        InitWindow()  ─────────────> rcore.c::InitWindow()
            |                              |
            |                              v
            |                        InitPlatform() ──> rcore_glfw.c
            |                              |
            |                              v
            |                        rlglInit() ──> rlgl.h (OpenGL setup)
            |
        while (!WindowShouldClose()) {
            |
            BeginDrawing() ────────> rcore.c::BeginDrawing()
            |                              |
            |                              v
            |                        rlglFramebufferSetup()
            |
            ClearBackground() ─────> rcore.c ──> rlClearColor()
            |
            DrawTexture() ─────────> rtextures.c
            |                              |
            |                              v
            |                        rlgl.h::rlSetTexture()
            |                        rlgl.h::rlBegin(QUADS)
            |                        rlgl.h::rlVertex2f()
            |                        rlgl.h::rlEnd()
            |
            DrawText() ────────────> rtext.c
            |                              |
            |                              v
            |                        DrawTextEx() ──> rlgl.h
            |
            EndDrawing() ──────────> rcore.c::EndDrawing()
            |                              |
            |                              v
            |                        rlDrawRenderBatchActive()
            |                              |
            |                              v
            |                        SwapScreenBuffer() ──> rcore_glfw.c
            |                              |
            |                              v
            |                        PollInputEvents() ──> rcore_glfw.c
        }
            |
        CloseWindow() ─────────────> rcore.c::CloseWindow()
                                           |
                                           v
                                     ClosePlatform() ──> rcore_glfw.c
    }
```

**中文说明**：
此图展示了 raylib 程序一帧的数据流向。用户代码（左侧）调用 raylib 公开 API，
请求流入 raylib 内部（右侧）。关键流程：
1. InitWindow() 初始化窗口，调用平台层的 InitPlatform() 和 rlgl 的 OpenGL 初始化
2. BeginDrawing/EndDrawing 之间是绘制阶段，所有绘制操作最终通过 rlgl.h 转换为 OpenGL 调用
3. EndDrawing() 会自动刷新渲染批次、交换缓冲区、轮询输入事件
4. 平台相关操作（如交换缓冲区）被隔离在 rcore_glfw.c 等平台文件中

### Dependency Rules

#### Which layers depend on platform/OS code?

| Layer | Platform Dependency |
|-------|---------------------|
| Layer 1 (External) | **Direct** — GLFW, RGFW, miniaudio directly call OS APIs |
| Layer 2 (Platform) | **Direct** — Each `rcore_*.c` is platform-specific |
| Layer 3 (Core) | **Indirect** — Via Layer 2 inclusion, plus minor `#ifdef` for paths |
| Layer 4 (Domain) | **None** — Fully platform-agnostic |
| Layer 5 (API) | **None** — Pure interface definitions |

#### Which layers are platform-agnostic?

- **Layer 4 (Domain Modules)**: rtextures, rtext, rshapes, rmodels, raudio
- **Layer 5 (Public API)**: raylib.h
- **Partially Layer 3**: rlgl.h, raymath.h, rcamera.h (standalone headers)

#### Where are OS-specific APIs isolated?

```
src/
├── platforms/           ← ALL platform code lives here
│   ├── rcore_desktop_glfw.c    (Win32, Linux, macOS, BSD)
│   ├── rcore_desktop_rgfw.c    (Alternative: Win32, Linux, macOS)
│   ├── rcore_desktop_sdl.c     (SDL-based platforms)
│   ├── rcore_desktop_win32.c   (Native Win32, no GLFW)
│   ├── rcore_drm.c             (Linux DRM/KMS, Raspberry Pi)
│   ├── rcore_android.c         (Android NDK)
│   ├── rcore_web.c             (Emscripten/WebAssembly)
│   └── rcore_memory.c          (Headless/software rendering)
└── external/
    └── glfw/src/        ← GLFW's own platform isolation
        ├── win32_*.c
        ├── cocoa_*.m
        ├── x11_*.c
        └── wl_*.c       (Wayland)
```

#### How does raylib prevent dependency inversion violations?

**Compile-Time Platform Selection Mechanism**:

```
                        COMPILATION PROCESS
    ================================================================
    
    Source Files:                    Preprocessor Decision:
    
    rcore.c ─────────────────────────────────────────────────────┐
       |                                                         |
       |   #if defined(PLATFORM_DESKTOP_GLFW)                    |
       |       |                                                 |
       |       +──> #include "platforms/rcore_desktop_glfw.c"    |
       |                                                         |
       |   #elif defined(PLATFORM_DESKTOP_SDL)                   |
       |       |                                                 |
       |       +──> #include "platforms/rcore_desktop_sdl.c"     |
       |                                                         |
       |   #elif defined(PLATFORM_WEB)                           |
       |       |                                                 |
       |       +──> #include "platforms/rcore_web.c"             |
       |                                                         |
       |   #elif defined(PLATFORM_ANDROID)                       |
       |       |                                                 |
       |       +──> #include "platforms/rcore_android.c"         |
       |                                                         |
       +─────────────────────────────────────────────────────────┘
                                |
                                v
                        SINGLE OBJECT FILE
                            rcore.o
                                |
                                v
                          FINAL BINARY
                         (platform-specific)
```

**中文说明**：
上图展示了 raylib 的编译时平台选择机制。rcore.c 是核心文件，它根据预处理器宏定义
（如 PLATFORM_DESKTOP_GLFW）来决定 #include 哪个平台实现文件。关键特点：
- 每次编译只会包含一个平台文件
- 平台文件被直接"内联"到 rcore.c 中，形成单一编译单元
- 这种设计避免了链接时的平台切换，简化了构建系统
- 缺点是切换平台需要重新编译

1. **Compile-time selection**: Only one platform file is ever compiled
   ```c
   // In rcore.c (lines 537-557)
   #if defined(PLATFORM_DESKTOP_GLFW)
       #include "platforms/rcore_desktop_glfw.c"
   #elif defined(PLATFORM_DESKTOP_SDL)
       #include "platforms/rcore_desktop_sdl.c"
   // ... etc
   #endif
   ```

2. **Extern declarations instead of includes**: Platform files access core via `extern CoreData CORE`
   ```c
   // In rcore_desktop_glfw.c (line 122)
   extern CoreData CORE;  // Global CORE state context
   ```

3. **Forward function declarations**: Core declares but doesn't implement platform functions
   ```c
   // In rcore.c (lines 492-493)
   extern int InitPlatform(void);   // Implemented by platform file
   extern void ClosePlatform(void);
   ```

4. **No upward includes**: Domain modules never `#include` platform files

5. **Configuration guards**: Modules check `SUPPORT_MODULE_*` flags
   ```c
   #if defined(SUPPORT_MODULE_RTEXTURES)
   // ... module code ...
   #endif
   ```

---

## STEP 3 — Public API vs Internal Implementation

### API Visibility Architecture

```
    +=====================================================================+
    |                        USER'S VIEW                                  |
    |                                                                     |
    |    #include "raylib.h"   <──── ONLY THIS IS NEEDED                  |
    |                                                                     |
    +=====================================================================+
                                     |
                                     | exposes
                                     v
    +=====================================================================+
    |                      PUBLIC SYMBOLS (RLAPI)                         |
    |                                                                     |
    |   RLAPI void InitWindow(...)        RLAPI Texture2D LoadTexture()   |
    |   RLAPI void CloseWindow()          RLAPI void UnloadTexture()      |
    |   RLAPI void BeginDrawing()         RLAPI void DrawTexture()        |
    |   RLAPI void EndDrawing()           RLAPI Image LoadImage()         |
    |   RLAPI bool IsKeyPressed()         RLAPI Font LoadFont()           |
    |                         ... 602 functions ...                       |
    |                                                                     |
    +=====================================================================+
                                     |
                                     | hides
                                     v
    +=====================================================================+
    |                     INTERNAL SYMBOLS (static)                       |
    |                                                                     |
    |   static CoreData CORE = { 0 };      // Global state (hidden)       |
    |   static PlatformData platform;      // Platform state (hidden)     |
    |   static int screenshotCounter;      // Internal counter            |
    |   static void InitTimer(void);       // Internal function           |
    |   static void SetupFramebuffer();    // Internal function           |
    |   static Font LoadFontTTF();         // Internal font loader        |
    |                                                                     |
    +=====================================================================+
    
    
    VISIBILITY MACRO EXPANSION:
    ===========================
    
    Windows DLL Build:      RLAPI = __declspec(dllexport)
    Windows DLL Usage:      RLAPI = __declspec(dllimport)
    Unix Shared Library:    RLAPI = __attribute__((visibility("default")))
    Static Library:         RLAPI = (empty, implicit extern)
```

**中文说明**：
此图展示了 raylib 的 API 可见性架构。用户只需要包含 raylib.h，即可访问所有公开符号。
- 公开符号（RLAPI 标记）：约 602 个函数，对外可见，构成稳定的公开接口
- 内部符号（static 标记）：对外不可见，可以自由修改而不影响用户代码
- RLAPI 宏根据编译配置展开为不同的可见性属性，支持静态库和动态库两种形式
- 这种设计确保了二进制兼容性：只要公开函数签名不变，内部实现可以任意重构

### Public Headers (User-Facing)

| Header | Purpose | User Should Include? |
|--------|---------|---------------------|
| `raylib.h` | Main API (~602 functions, all types) | **YES — Primary header** |
| `raymath.h` | Math library (vectors, matrices, quaternions) | Optional (standalone capable) |
| `rlgl.h` | OpenGL abstraction layer | Optional (advanced users) |
| `rcamera.h` | Camera system | Optional (standalone capable) |
| `rgestures.h` | Gesture detection | Optional (standalone capable) |

### Internal Headers (Implementation)

| Header | Purpose | User Should Include? |
|--------|---------|---------------------|
| `config.h` | Compile-time feature flags | **NO** — Build system only |
| `utils.h` | Internal logging, file I/O wrappers | **NO** — Internal only |

### Symbol Visibility Control

#### The `RLAPI` Macro

```c
// raylib.h (lines 94-114)
#if defined(_WIN32)
    #if defined(BUILD_LIBTYPE_SHARED)
        #define RLAPI __declspec(dllexport)     // Building DLL
    #elif defined(USE_LIBTYPE_SHARED)
        #define RLAPI __declspec(dllimport)     // Using DLL
    #endif
#else
    #if defined(BUILD_LIBTYPE_SHARED)
        #define RLAPI __attribute__((visibility("default")))  // Unix .so/.dylib
    #endif
#endif

#ifndef RLAPI
    #define RLAPI  // Static library: implicit extern
#endif
```

#### Internal Symbol Hiding

Raylib uses **static functions** and **file-scope globals** to hide internals:

```c
// rshapes.c (lines 82-83)
static Texture2D texShapes = { 1, 1, 1, 1, 7 };        // Not exposed
static Rectangle texShapesRec = { 0.0f, 0.0f, 1.0f, 1.0f };

// rcore.c (lines 391-392)
static int screenshotCounter = 0;  // Internal state
```

### Header-Only Library Pattern

```
    +===================================================================+
    |           HEADER-ONLY LIBRARY IMPLEMENTATION PATTERN              |
    +===================================================================+
    
    
    STANDARD USAGE (most common):
    =============================
    
        raylib user's code
        +------------------+
        | game.c           |
        |                  |
        | #include "raylib.h"   <---- Just declarations, no implementation
        |                  |
        | main() {         |
        |   InitWindow();  |
        |   ...            |
        | }                |
        +------------------+
              |
              | links with
              v
        +------------------+
        | libraylib.a      |   <---- Pre-compiled library
        +------------------+
    
    
    STANDALONE MODULE USAGE (rlgl.h as example):
    =============================================
    
        User's OpenGL wrapper
        +-----------------------------+
        | my_renderer.c               |
        |                             |
        | #define RLGL_IMPLEMENTATION |   <---- ENABLES implementation
        | #include "rlgl.h"           |
        |                             |
        | void myRender() {           |
        |   rlBegin(RL_TRIANGLES);    |
        |   rlVertex3f(...);          |
        |   rlEnd();                  |
        | }                           |
        +-----------------------------+
    
    
    HOW IT WORKS INTERNALLY:
    ========================
    
        rlgl.h structure:
        +----------------------------------------------------------+
        |                                                          |
        |  // Always included: declarations                        |
        |  void rlBegin(int mode);                                 |
        |  void rlVertex3f(float x, float y, float z);             |
        |  ...                                                     |
        |                                                          |
        |  #if defined(RLGL_IMPLEMENTATION)                        |
        |                                                          |
        |      // Only included when RLGL_IMPLEMENTATION defined   |
        |      void rlBegin(int mode) {                            |
        |          // actual implementation code                   |
        |      }                                                   |
        |                                                          |
        |      void rlVertex3f(float x, float y, float z) {        |
        |          // actual implementation code                   |
        |      }                                                   |
        |                                                          |
        |  #endif // RLGL_IMPLEMENTATION                           |
        |                                                          |
        +----------------------------------------------------------+
    
    
    RAYLIB BUILD: Who defines RLGL_IMPLEMENTATION?
    ==============================================
    
        rcore.c
        +----------------------------------+
        | #define RLGL_IMPLEMENTATION      |  <-- rcore.c enables it
        | #include "rlgl.h"                |
        |                                  |
        | #define RAYMATH_IMPLEMENTATION   |  <-- Also enables raymath
        | #include "raymath.h"             |
        +----------------------------------+
```

**中文说明**：
此图解释了 raylib 使用的"头文件库"（header-only library）模式：

1. **标准用法**：用户只需 `#include "raylib.h"` 获取声明，然后链接预编译的库

2. **独立模块用法**：如 rlgl.h、raymath.h 可以脱离 raylib 单独使用
   - 定义 `RLGL_IMPLEMENTATION` 宏后包含头文件
   - 宏开启后，头文件中的实现代码会被编译进来

3. **内部工作原理**：
   - 头文件分两部分：声明（总是包含）和实现（条件包含）
   - `#if defined(XXX_IMPLEMENTATION)` 控制实现代码的编译
   - 这种设计只需一个头文件即可完成声明和实现

4. **raylib 构建流程**：
   - rcore.c 定义了 `RLGL_IMPLEMENTATION` 和 `RAYMATH_IMPLEMENTATION`
   - 因此 rlgl 和 raymath 的实现只在编译 rcore.c 时被引入一次

这种模式的优势：简化构建系统、方便模块独立使用、避免重复编译。

#### Type Definition Guards

Prevent redefinition when using modules standalone:

```c
// raylib.h (lines 164-171)
#define RL_COLOR_TYPE
#define RL_RECTANGLE_TYPE
#define RL_VECTOR2_TYPE
#define RL_VECTOR3_TYPE
// ...

// raymath.h (lines 115-122)
#if !defined(RL_VECTOR2_TYPE)
typedef struct Vector2 { float x; float y; } Vector2;
#define RL_VECTOR2_TYPE
#endif
```

### Why This Separation Is Critical

| Goal | How Raylib Achieves It |
|------|------------------------|
| **Maintainability** | Internal changes don't affect users; `utils.c` can be rewritten without API changes |
| **Binary Compatibility** | `RLAPI` + visibility attributes ensure stable symbol tables across versions |
| **Long-term Evolution** | New features added without breaking existing code; deprecated functions marked clearly |
| **Standalone Modules** | `raymath.h`, `rlgl.h` work independently via type guards |

---

## STEP 4 — Platform Abstraction Layer

### Architectural Boundary

```
    +===================================================================+
    |                    PLATFORM ABSTRACTION PATTERN                   |
    +===================================================================+
    
                          rcore.c (Orchestrator)
                    +---------------------------+
                    |                           |
                    |  CoreData CORE = { 0 };   |  <── Shared State
                    |                           |
                    |  InitWindow() {           |
                    |      InitPlatform();  ────┼──────────────────┐
                    |      rlglInit();          |                  |
                    |  }                        |                  |
                    |                           |                  |
                    |  EndDrawing() {           |                  |
                    |      SwapScreenBuffer(); ─┼──────────────────┤
                    |      PollInputEvents(); ──┼──────────────────┤
                    |  }                        |                  |
                    +---------------------------+                  |
                                                                   |
                    ===============================================|===
                            PLATFORM ABSTRACTION BOUNDARY          |
                    ===============================================|===
                                                                   |
                                                                   v
         +------------------+  +------------------+  +------------------+
         | rcore_glfw.c     |  | rcore_sdl.c      |  | rcore_web.c      |
         |------------------|  |------------------|  |------------------|
         | extern CORE;     |  | extern CORE;     |  | extern CORE;     |
         | static platform; |  | static platform; |  | static platform; |
         |                  |  |                  |  |                  |
         | InitPlatform()   |  | InitPlatform()   |  | InitPlatform()   |
         |   glfwInit()     |  |   SDL_Init()     |  |   emscripten_*   |
         |   glfwCreate...  |  |   SDL_Create...  |  |                  |
         |                  |  |                  |  |                  |
         | SwapScreenBuffer |  | SwapScreenBuffer |  | SwapScreenBuffer |
         |   glfwSwapBuf..  |  |   SDL_GL_Swap..  |  |   emscripten_*   |
         |                  |  |                  |  |                  |
         | PollInputEvents  |  | PollInputEvents  |  | PollInputEvents  |
         |   glfwPollEvt..  |  |   SDL_PollEvt..  |  |   (automatic)    |
         +------------------+  +------------------+  +------------------+
                  |                    |                    |
                  v                    v                    v
         +------------------+  +------------------+  +------------------+
         |      GLFW        |  |       SDL        |  |   Emscripten     |
         |  (Win/Lin/Mac)   |  |  (Multi-plat)    |  |    (WebGL)       |
         +------------------+  +------------------+  +------------------+
                  |                    |                    |
                  v                    v                    v
         +------------------+  +------------------+  +------------------+
         |  Win32 / X11 /   |  |  DirectX / X11   |  |   WebGL API /    |
         |  Cocoa / Wayland |  |  / Cocoa / ...   |  |   Browser DOM    |
         +------------------+  +------------------+  +------------------+
```

**中文说明**：
此图展示了 raylib 平台抽象层的架构模式。核心思想是"接口与实现分离"：
- rcore.c 定义了平台无关的业务逻辑，并声明需要平台实现的函数（如 InitPlatform）
- 每个平台文件（rcore_glfw.c 等）实现相同的函数接口，但使用不同的底层 API
- 共享状态通过 extern CoreData CORE 访问，平台私有状态用 static 隔离
- 虚线标示"平台抽象边界"：边界之上是平台无关代码，边界之下是平台特定代码
- 添加新平台只需实现约 50 个函数，无需修改上层代码

The Platform Abstraction Layer (PAL) provides a **contract** between core raylib and platform-specific implementations:

```
┌───────────────────────────────────────────────────────────────────┐
│                      PLATFORM CONTRACT                            │
│                                                                   │
│  Required Functions (implemented by each rcore_*.c):              │
│  ─────────────────────────────────────────────────────────────    │
│  • int InitPlatform(void)        Initialize graphics + input      │
│  • void ClosePlatform(void)      Cleanup resources                │
│  • bool WindowShouldClose(void)  Check exit condition             │
│  • void ToggleFullscreen(void)   Switch display modes             │
│  • void SwapScreenBuffer(void)   Present frame                    │
│  • void PollInputEvents(void)    Process OS events                │
│  • double GetTime(void)          High-resolution timer            │
│  • void SetMousePosition(int,int) Cursor control                  │
│  • ... (~50 more functions)                                       │
│                                                                   │
│  Required State Access:                                           │
│  ─────────────────────────────────────────────────────────────    │
│  • extern CoreData CORE          Shared state structure           │
│  • static PlatformData platform  Platform-specific state          │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

### Platform File Organization

```
platforms/
├── rcore_desktop_glfw.c     # GLFW backend (most common)
│   └── Uses: GLFW, glad/glad_gles2
│   └── Targets: Windows, Linux, macOS, BSD
│
├── rcore_desktop_rgfw.c     # RGFW backend (alternative)
│   └── Uses: RGFW.h (single-header)
│   └── Targets: Windows, Linux, macOS
│
├── rcore_desktop_sdl.c      # SDL backend
│   └── Uses: SDL2/SDL3
│   └── Targets: Any SDL-supported platform
│
├── rcore_desktop_win32.c    # Native Win32 (no GLFW)
│   └── Uses: Win32 API directly
│   └── Targets: Windows only
│
├── rcore_drm.c              # Linux DRM/KMS
│   └── Uses: libdrm, libgbm, EGL
│   └── Targets: Raspberry Pi, embedded Linux
│
├── rcore_android.c          # Android NDK
│   └── Uses: ANativeWindow, EGL, ALooper
│   └── Targets: Android devices
│
├── rcore_web.c              # WebAssembly
│   └── Uses: Emscripten, WebGL
│   └── Targets: Web browsers
│
├── rcore_memory.c           # Headless/software
│   └── Uses: rlsw.h (software renderer)
│   └── Targets: CI testing, servers
│
└── rcore_template.c         # Reference implementation
    └── Purpose: Guide for new ports
```

### Core State Management Pattern

```
    +===================================================================+
    |              COREDATA GLOBAL STATE STRUCTURE                      |
    +===================================================================+
    
    
    CoreData CORE = { 0 };    // Single global instance in rcore.c
    
    
    +---------------------------------------------------------------+
    |                      CoreData CORE                            |
    +---------------------------------------------------------------+
    |                                                               |
    |  +-------------------------+  +--------------------------+    |
    |  |     CORE.Window         |  |     CORE.Storage         |    |
    |  +-------------------------+  +--------------------------+    |
    |  | title                   |  | basePath                 |    |
    |  | flags                   |  +--------------------------+    |
    |  | ready                   |                                  |
    |  | fullscreen              |  +--------------------------+    |
    |  | shouldClose             |  |      CORE.Time           |    |
    |  | display.width/height    |  +--------------------------+    |
    |  | screen.width/height     |  | current                  |    |
    |  | render.width/height     |  | previous                 |    |
    |  | screenScale (Matrix)    |  | update                   |    |
    |  | dropFilepaths[]         |  | draw                     |    |
    |  +-------------------------+  | frame                    |    |
    |                               | target                   |    |
    |  +-------------------------+  | frameCounter             |    |
    |  |     CORE.Input          |  +--------------------------+    |
    |  +-------------------------+                                  |
    |  | Keyboard:               |                                  |
    |  |   currentKeyState[512]  |                                  |
    |  |   previousKeyState[512] |                                  |
    |  |   keyPressedQueue[16]   |                                  |
    |  +-------------------------+                                  |
    |  | Mouse:                  |                                  |
    |  |   currentPosition       |                                  |
    |  |   previousPosition      |                                  |
    |  |   currentButtonState[8] |                                  |
    |  |   currentWheelMove      |                                  |
    |  +-------------------------+                                  |
    |  | Touch:                  |                                  |
    |  |   position[8]           |                                  |
    |  |   currentTouchState[8]  |                                  |
    |  +-------------------------+                                  |
    |  | Gamepad:                |                                  |
    |  |   ready[4]              |                                  |
    |  |   name[4][128]          |                                  |
    |  |   axisState[4][8]       |                                  |
    |  +-------------------------+                                  |
    |                                                               |
    +---------------------------------------------------------------+
    
    
    ACCESS PATTERN:
    ---------------
    
        rcore.c:             CoreData CORE = { 0 };     // Defines
        rcore_glfw.c:        extern CoreData CORE;      // References
        rcore_sdl.c:         extern CoreData CORE;      // References
        rcore_web.c:         extern CoreData CORE;      // References
```

**中文说明**：
此图展示了 raylib 核心状态管理的数据结构 CoreData。它是一个全局单例，包含：
- Window：窗口状态（标题、尺寸、全屏模式、拖放文件等）
- Storage：存储路径信息
- Time：帧时间管理（当前时间、上一帧、目标帧率等）
- Input：所有输入状态
  - Keyboard：键盘按键状态数组
  - Mouse：鼠标位置、按键、滚轮
  - Touch：触摸点位置和状态
  - Gamepad：游戏手柄连接状态和轴数据

访问模式：rcore.c 定义此全局变量，所有平台文件通过 extern 声明访问它。
这种设计的优点是简化 API（无需传递上下文），缺点是不支持多实例。

### Functionality Abstracted

| Category | Functions Abstracted |
|----------|---------------------|
| **Windowing** | `InitWindow`, `CloseWindow`, `WindowShouldClose`, `ToggleFullscreen`, `SetWindowSize`, `GetMonitorCount`, etc. |
| **Input** | `PollInputEvents`, `GetKeyPressed`, `GetMousePosition`, `GetTouchPosition`, `GetGamepadName`, etc. |
| **Timing** | `GetTime`, `WaitTime`, frame timing |
| **Graphics Context** | OpenGL/ES context creation, `SwapScreenBuffer`, extension loading |
| **Clipboard** | `SetClipboardText`, `GetClipboardText`, `GetClipboardImage` |
| **System** | `OpenURL`, native window handles |

### Adding a New Platform (Architectural Steps)

1. **Copy the template**:
   ```bash
   cp src/platforms/rcore_template.c src/platforms/rcore_myplatform.c
   ```

2. **Define platform selection**:
   ```c
   // In rcore.c, add to platform selection:
   #elif defined(PLATFORM_MYPLATFORM)
       #include "platforms/rcore_myplatform.c"
   ```

3. **Implement required functions** (following template structure):
   ```c
   // rcore_myplatform.c
   
   typedef struct {
       // Platform-specific handles
       MyPlatformWindow *window;
       MyPlatformContext *context;
   } PlatformData;
   
   extern CoreData CORE;
   static PlatformData platform = { 0 };
   
   int InitPlatform(void) {
       // Initialize graphics device
       // Set CORE.Window.ready = true;
       // Initialize input system
       // Call rlLoadExtensions()
       // Call InitTimer()
       return 0;  // Success
   }
   
   void ClosePlatform(void) { /* Cleanup */ }
   
   // ... implement ~50 other functions
   ```

4. **Update build system** (Makefile/CMake):
   ```makefile
   ifeq ($(PLATFORM),PLATFORM_MYPLATFORM)
       CFLAGS += -DPLATFORM_MYPLATFORM
       LDLIBS += -lmyplatform_sdk
   endif
   ```

---

## STEP 5 — Module-Level Layering

### Module Dependency Graph

```
    +===================================================================+
    |               RAYLIB MODULE DEPENDENCY GRAPH                      |
    +===================================================================+
    
                              raylib.h
                           (Public API)
                                 |
           +----------+----+-----+-----+----+----------+
           |          |    |           |    |          |
           v          v    v           v    v          v
      +--------+  +------+------+  +------+------+  +--------+
      |rtextures|  |rtext|rshapes|  |rmodels     |  | raudio |
      +----+---+  +--+--+--+----+  +------+------+  +---+----+
           |         |     |              |             |
           |    +----+     |              |             |
           |    |          |              |             |
           +----+----+-----+----+---------+             |
                     |          |                       |
                     v          v                       |
                 +------+  +--------+                   |
                 | rlgl |  |raymath |                   |
                 +--+---+  +--------+                   |
                    |                                   |
                    v                                   v
                +-------+                         +-----------+
                | utils |                         | miniaudio |
                +---+---+                         +-----------+
                    |
                    v
             +--------------+
             | config.h     |
             | (build opts) |
             +--------------+
    
    
    LEGEND:
    -------
        ───────>  Depends on (uses)
        [module]  Domain module (Layer 4)
        (module)  Infrastructure (Layer 3)
        <module>  External dependency (Layer 1)
```

**中文说明**：
此图展示了 raylib 各模块之间的依赖关系。关键观察点：
1. 所有领域模块（rtextures, rtext, rshapes, rmodels）都依赖 rlgl 进行渲染
2. rmodels 同时依赖 raymath（数学计算）和 rlgl（渲染）
3. raudio 是完全独立的——它只依赖 miniaudio，不依赖其他 raylib 模块
4. utils 是底层公共工具，被多个模块引用
5. 模块之间没有循环依赖，依赖方向是单向的（从上到下）

### Module Independence Matrix

```
    +============+========+======+=======+========+=======+=======+
    | DEPENDS ON | rtext  |rshape|rtexture|rmodels| raudio| rlgl  |
    +============+========+======+=======+========+=======+=======+
    | rtextures  |   -    |  -   |   -   |   -    |   -   |  YES  |
    +------------+--------+------+-------+--------+-------+-------+
    | rtext      |   -    |  -   |  YES* |   -    |   -   |  YES  |
    +------------+--------+------+-------+--------+-------+-------+
    | rshapes    |   -    |  -   |   -   |   -    |   -   |  YES  |
    +------------+--------+------+-------+--------+-------+-------+
    | rmodels    |   -    |  -   |   -   |   -    |   -   |  YES  |
    +------------+--------+------+-------+--------+-------+-------+
    | raudio     |   -    |  -   |   -   |   -    |   -   |   -   |
    +------------+--------+------+-------+--------+-------+-------+
    | rlgl       |   -    |  -   |   -   |   -    |   -   |   -   |
    +============+========+======+=======+========+=======+=======+
    
    YES* = Optional dependency (for font texture loading)
    -    = No dependency
```

**中文说明**：
这个矩阵清晰地展示了模块间的依赖关系。表格读法：行模块依赖列模块。
- rlgl 是最底层的渲染抽象，不依赖任何其他 raylib 模块
- raudio 完全独立，可以单独编译使用（RAUDIO_STANDALONE）
- rtext 对 rtextures 有可选依赖（加载字体纹理时需要）
- 这种低耦合设计使得模块可以按需启用或禁用

### Subsystem Analysis

#### 1. Core Module (`rcore.c`)

**Internal Layering**:
```
┌─────────────────────────────────────────────────────────────┐
│                   rcore.c (4365 lines)                      │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 1. State Management Layer                           │    │
│  │    - CoreData CORE global structure                 │    │
│  │    - Window, Input, Storage, Time sub-structures    │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 2. Header Inclusion (Implementation Selection)      │    │
│  │    - #define RLGL_IMPLEMENTATION                    │    │
│  │    - #define RAYMATH_IMPLEMENTATION                 │    │
│  │    - #include "platforms/rcore_*.c"                 │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 3. Platform-Agnostic Functions                      │    │
│  │    - InitWindow(), BeginDrawing(), EndDrawing()     │    │
│  │    - Input state queries (IsKeyPressed, etc.)       │    │
│  │    - File system utilities                          │    │
│  │    - Timing control (SetTargetFPS)                  │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 4. Automation Events Subsystem                      │    │
│  │    - Event recording/playback                       │    │
│  │    - Testing automation support                     │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**Interaction with Platform Layer**:
- `InitWindow()` → calls `InitPlatform()` → platform-specific initialization
- `EndDrawing()` → calls `SwapScreenBuffer()` + `PollInputEvents()`
- All input state stored in `CORE.Input.*` and read by platform files

**Status**: **Mandatory** — Cannot be disabled

#### 2. Audio Module (`raudio.c`)

**Internal Layering**:
```
┌─────────────────────────────────────────────────────────────┐
│                   raudio.c (2896 lines)                     │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 1. Audio Device Layer (miniaudio)                   │    │
│  │    - InitAudioDevice() / CloseAudioDevice()         │    │
│  │    - Device configuration, sample rate, channels    │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 2. Audio Buffer Management                          │    │
│  │    - AudioBuffer pool (MAX_AUDIO_BUFFER_POOL)       │    │
│  │    - Streaming support                              │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 3. File Format Decoders                             │    │
│  │    - WAV (dr_wav.h)                                 │    │
│  │    - OGG (stb_vorbis.c)                             │    │
│  │    - MP3 (dr_mp3.h)                                 │    │
│  │    - FLAC (dr_flac.h)                               │    │
│  │    - XM/MOD (jar_xm.h, jar_mod.h)                   │    │
│  │    - QOA (qoa.h)                                    │    │
│  └─────────────────────────────────────────────────────┘    │
│                           │                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 4. High-Level API                                   │    │
│  │    - Sound / Music loading                          │    │
│  │    - PlaySound() / PlayMusicStream()                │    │
│  │    - Audio processing callbacks                     │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**Interaction with Core**: 
- **Minimal** — only uses `utils.h` for `TRACELOG()` and file loading
- Can be built **standalone** with `RAUDIO_STANDALONE` define

**Status**: **Optional** — Controlled by `SUPPORT_MODULE_RAUDIO`

#### 3. Graphics Pipeline (rlgl + rtextures + rshapes + rmodels)

**Rendering Data Flow**:

```
    +===================================================================+
    |                   RAYLIB RENDERING PIPELINE                       |
    +===================================================================+
    
    
    USER CODE                          RAYLIB INTERNALS
    =========                          ================
    
    BeginDrawing()  ─────────────────> Clear framebuffer
                                       Reset render state
    
    DrawTexture()   ─────────────────> rtextures.c
    DrawRectangle() ─────────────────> rshapes.c
    DrawText()      ─────────────────> rtext.c
    DrawModel()     ─────────────────> rmodels.c
           |                                |
           |                                |
           +──────────> ALL CALL ──────────>+
                                            |
                                            v
                              +---------------------------+
                              |        rlgl.h             |
                              |   BATCHING SYSTEM         |
                              +---------------------------+
                              |                           |
                              |  rlBegin(RL_QUADS)        |
                              |  rlTexCoord2f(u, v)       |
                              |  rlColor4ub(r, g, b, a)   |
                              |  rlVertex3f(x, y, z)      |
                              |  ...                      |
                              |  rlEnd()                  |
                              |                           |
                              |  [Vertices accumulated    |
                              |   in internal buffer]     |
                              |                           |
                              +-------------+-------------+
                                            |
                                            | State change or
                                            | buffer full
                                            v
                              +---------------------------+
                              |  rlDrawRenderBatch()      |
                              +---------------------------+
                              |                           |
                              |  glBindVertexArray()      |
                              |  glBindBuffer()           |
                              |  glBufferSubData()        |
                              |  glDrawElements()         |
                              |                           |
                              +-------------+-------------+
                                            |
                                            v
    EndDrawing()    ─────────────────> Flush remaining batch
                                       SwapScreenBuffer()
                                       PollInputEvents()
                                            |
                                            v
                              +---------------------------+
                              |   DISPLAY / FRAMEBUFFER   |
                              +---------------------------+
```

**中文说明**：
此图展示了 raylib 渲染管线的数据流：

1. **用户绘制调用**：
   - 用户调用 DrawTexture、DrawRectangle、DrawText、DrawModel 等
   - 这些调用分散在不同模块（rtextures, rshapes, rtext, rmodels）

2. **rlgl 批处理系统**：
   - 所有绘制最终都通过 rlgl.h 的即时模式风格 API
   - rlBegin/rlVertex/rlEnd 将顶点数据累积到内部缓冲区
   - 这避免了每个图元一次 OpenGL 调用的开销

3. **批次提交**：
   - 当状态改变（如切换纹理）或缓冲区满时，自动提交批次
   - rlDrawRenderBatch() 将累积的数据发送给 GPU

4. **帧结束**：
   - EndDrawing() 刷新剩余批次
   - 交换前后缓冲区，显示画面
   - 轮询输入事件，准备下一帧

这种批处理设计大大减少了 OpenGL 调用次数，提高了渲染性能。

**Internal Layering**:
```
┌─────────────────────────────────────────────────────────────────────┐
│                     GRAPHICS SUBSYSTEM                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                  rmodels.c (7131 lines)                       │  │
│  │  [3D Models, Meshes, Materials, Animations]                   │  │
│  │  Uses: rlgl, raymath, cgltf, tinyobj, par_shapes              │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                               │                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                  rtext.c (2796 lines)                         │  │
│  │  [Font Loading, Text Rendering, String Utils]                 │  │
│  │  Uses: rlgl, stb_truetype, stb_rect_pack                      │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                               │                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                 rtextures.c (5610 lines)                      │  │
│  │  [Image/Texture Loading, Processing, GPU Upload]              │  │
│  │  Uses: rlgl, stb_image, stb_image_write, stb_image_resize     │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                               │                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                  rshapes.c (2487 lines)                       │  │
│  │  [2D Primitive Drawing, Collision Detection]                  │  │
│  │  Uses: rlgl only                                              │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                               │                                     │
│                               ▼                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                   rlgl.h (5383 lines)                         │  │
│  │  [OpenGL Abstraction Layer]                                   │  │
│  │  Supports: OpenGL 1.1, 2.1, 3.3, 4.3, ES2, ES3               │  │
│  │  Features: Batching, Shaders, Textures, Framebuffers          │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                               │                                     │
│                               ▼                                     │
│                        [OpenGL/ES Driver]                           │
└─────────────────────────────────────────────────────────────────────┘
```

### Stability Classification

| Module | Stability | Volatility | Hot Path |
|--------|-----------|------------|----------|
| `raylib.h` | **Very Stable** | Low (API compatibility) | N/A |
| `rlgl.h` | **Stable** | Low (core rendering) | **Yes** (every frame) |
| `rcore.c` | **Stable** | Medium (new features) | **Yes** (main loop) |
| `raymath.h` | **Very Stable** | Very Low | **Yes** (math ops) |
| `rshapes.c` | **Stable** | Low | **Yes** (2D drawing) |
| `rtextures.c` | **Moderate** | Medium (format support) | Partial |
| `rtext.c` | **Moderate** | Medium | Partial |
| `rmodels.c` | **Moderate** | High (new formats) | Partial |
| `raudio.c` | **Moderate** | Medium | **Yes** (streaming) |
| `platforms/*` | **Volatile** | High (OS changes) | Partial |

---

## STEP 6 — Evolution & Change Scenarios

### Change Impact Visualization

```
    +===================================================================+
    |           ARCHITECTURE CHANGE IMPACT ANALYSIS                     |
    +===================================================================+
    
    SCENARIO: Add Vulkan Backend
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
        Layer 5: raylib.h        [  NO CHANGE  ] ████████████████████
        Layer 4: Domain Modules  [  NO CHANGE  ] ████████████████████
        Layer 3: rlgl.h          [ MAJOR CHANGE] ░░░░░░██████████████
        Layer 2: Platform        [ MINOR CHANGE] ████████░░░░████████
        Layer 1: External        [    ADD      ] ████████████░░░░░░░░
    
    
    SCENARIO: Port to New OS (e.g., Switch)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
        Layer 5: raylib.h        [  NO CHANGE  ] ████████████████████
        Layer 4: Domain Modules  [  NO CHANGE  ] ████████████████████
        Layer 3: Core            [ MINOR CHANGE] ██████████████░░░░██
        Layer 2: Platform        [    ADD      ] ████████████░░░░░░░░
        Layer 1: External        [ MAYBE ADD   ] ████████████████░░░░
    
    
    SCENARIO: Remove Audio Module
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
        Layer 5: raylib.h        [  NO CHANGE  ] ████████████████████
        Layer 4: raudio.c        [   REMOVE    ] ░░░░░░░░░░░░░░░░░░░░
        Layer 4: Other Modules   [  NO CHANGE  ] ████████████████████
        Layer 3: Core            [  NO CHANGE  ] ████████████████████
        Layer 1: miniaudio       [   REMOVE    ] ░░░░░░░░░░░░░░░░░░░░
    
    
    SCENARIO: Rewrite Text Rendering
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
        Layer 5: raylib.h        [  NO CHANGE  ] ████████████████████
        Layer 4: rtext.c         [ FULL REWRITE] ░░░░░░░░░░░░░░░░░░░░
        Layer 4: Other Modules   [  NO CHANGE  ] ████████████████████
        Layer 3: Core            [  NO CHANGE  ] ████████████████████
        Layer 1: stb_truetype    [ MAYBE SWAP  ] ██████████████░░░░░░
    
    
    LEGEND:  ████ = Stable/Unchanged    ░░░░ = Changed/Affected
```

**中文说明**：
此图以可视化方式展示了四种架构变更场景对各层的影响。条形图中：
- 实心方块（████）表示该层稳定，无需修改
- 空心方块（░░░░）表示该层需要修改或受影响

关键洞察：
1. 添加 Vulkan 后端：主要影响 rlgl.h（核心渲染层），上层完全不变
2. 移植到新操作系统：只需添加新的平台文件，领域模块完全不变
3. 移除音频模块：由于 raudio 完全独立，移除它不影响任何其他模块
4. 重写文字渲染：只影响 rtext.c 本身，公开 API 保持不变

这证明了分层架构的核心价值：变更被隔离在特定层内，不会波及其他层。

### Scenario 1: Add a New Rendering Backend (e.g., Vulkan)

**Layers Affected**:
| Layer | Impact | Changes Required |
|-------|--------|-----------------|
| Layer 1 (External) | **Add** | Include Vulkan SDK headers/loader |
| Layer 2 (Platform) | **Minor** | Surface creation changes in `rcore_*.c` |
| Layer 3 (Core) | **Major** | New `rlgl` backend or parallel implementation |
| Layer 4 (Domain) | **None** | Use `rlgl` abstraction unchanged |
| Layer 5 (API) | **None** | Public API unchanged |

**Architectural Support**:
- ✅ `rlgl.h` already supports multiple OpenGL versions via `GRAPHICS_API_*` defines
- ✅ Compile-time backend selection is established pattern
- ⚠️ Challenge: Vulkan's different paradigm (command buffers, pipelines) doesn't map cleanly to rlgl's immediate-mode style

**Implementation Approach**:
```c
// Option A: Extend rlgl with Vulkan backend
#if defined(GRAPHICS_API_VULKAN)
    // Implement rlgl functions using Vulkan
#endif

// Option B: Create parallel rvk.h module
// Requires more changes to domain modules
```

### Scenario 2: Port to a New OS (e.g., Nintendo Switch)

**Layers Affected**:
| Layer | Impact | Changes Required |
|-------|--------|-----------------|
| Layer 1 (External) | **Add** | Platform SDK, possibly custom GLFW replacement |
| Layer 2 (Platform) | **Add** | New `rcore_switch.c` file |
| Layer 3 (Core) | **Minor** | Add platform detection in `rcore.c` |
| Layer 4 (Domain) | **None** | Platform-agnostic |
| Layer 5 (API) | **None** | Unchanged |

**Architectural Support**:
- ✅ Clean platform abstraction via `platforms/` directory
- ✅ Template file (`rcore_template.c`) guides implementation
- ✅ All platform code isolated; no changes to domain modules

**Implementation Approach**:
```c
// 1. Create platforms/rcore_switch.c
typedef struct {
    NWindow *window;
    EGLDisplay display;
    EGLContext context;
    // ...
} PlatformData;

extern CoreData CORE;
static PlatformData platform = { 0 };

int InitPlatform(void) {
    // Use Nintendo SDK to create window/context
    // ...
}

// 2. Add to rcore.c platform selection
#elif defined(PLATFORM_SWITCH)
    #include "platforms/rcore_switch.c"
```

### Scenario 3: Remove a Module Entirely (e.g., Audio)

**Layers Affected**:
| Layer | Impact | Changes Required |
|-------|--------|-----------------|
| Layer 1 (External) | **Remove** | miniaudio, dr_*, jar_*, qoa |
| Layer 4 (Domain) | **Remove** | `raudio.c` |
| Other Layers | **None** | Already optional |

**Architectural Support**:
- ✅ `SUPPORT_MODULE_RAUDIO` already disables compilation
- ✅ No other modules depend on raudio
- ✅ `config.h` controls all module inclusion

**Implementation**:
```c
// In config.h, change:
#define SUPPORT_MODULE_RAUDIO           1
// To:
//#define SUPPORT_MODULE_RAUDIO         1
```

**Build size reduction**: ~200KB+ on typical builds

### Scenario 4: Rewrite a Subsystem Without Breaking Users (e.g., Text Rendering)

**Layers Affected**:
| Layer | Impact | Changes Required |
|-------|--------|-----------------|
| Layer 1 (External) | **Maybe** | Could swap stb_truetype for FreeType |
| Layer 4 (Domain) | **Major** | Complete `rtext.c` rewrite |
| Layer 5 (API) | **None** | Same function signatures |

**Architectural Support**:
- ✅ Public API (`raylib.h`) is stable contract
- ✅ Internal implementation is hidden
- ✅ `RLAPI` functions are the only interface

**Constraints**:
- Function signatures must remain identical
- Type definitions (Font, GlyphInfo) are public and cannot change
- Memory management patterns (Load/Unload) must be preserved

```c
// Public API contract (cannot change):
RLAPI Font LoadFont(const char *fileName);
RLAPI void UnloadFont(Font font);
RLAPI void DrawText(const char *text, int x, int y, int fontSize, Color color);

// Internal implementation (can change freely):
// static Font LoadFontTTF(...) → can be completely rewritten
```

---

## STEP 7 — Architectural Trade-offs

### Trade-off Spectrum Visualization

```
    +===================================================================+
    |              RAYLIB ARCHITECTURAL TRADE-OFFS                      |
    +===================================================================+
    
    
    SIMPLICITY vs FLEXIBILITY
    -------------------------
    
    Simple ◀━━━━━━━━━━●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━▶ Flexible
                      ^
                   RAYLIB
                   (Optimizes for simplicity)
    
    
    BEGINNER-FRIENDLY vs POWER-USER
    --------------------------------
    
    Beginner ◀━━━━━━━━●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━▶ Power User
                      ^
                   RAYLIB
                   (Targets beginners/hobbyists)
    
    
    COMPILE-TIME vs RUNTIME Configuration
    -------------------------------------
    
    Compile-Time ◀━━━●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━▶ Runtime
                     ^
                  RAYLIB
                  (Most config at compile-time)
    
    
    BINARY SIZE vs FEATURE COMPLETENESS
    ------------------------------------
    
    Small Binary ◀━━━━━━━━━━━━━━━━━━━━━━━━━━●━━━━━━━━▶ Full Features
                                            ^
                                         RAYLIB
                                         (Includes most features)
    
    
    PORTABILITY vs PLATFORM OPTIMIZATION
    -------------------------------------
    
    Portable ◀━━━━━━━●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━▶ Optimized
                     ^
                  RAYLIB
                  (Lowest-common-denominator)
    
    
    GLOBAL STATE vs CONTEXT PASSING
    --------------------------------
    
    Global ◀━━━●━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━▶ Context-based
               ^
            RAYLIB
            (Single global CoreData CORE)
```

**中文说明**：
此图以光谱形式展示了 raylib 在各种架构权衡中的定位。每条光谱两端代表极端选择，
raylib 的位置（●）显示了它的设计偏好：

1. 简单性 vs 灵活性：raylib 选择简单性，牺牲一定的灵活性
2. 初学者 vs 高级用户：raylib 明确以初学者和业余爱好者为目标
3. 编译时 vs 运行时配置：raylib 偏好编译时配置，减少运行时开销
4. 二进制大小 vs 功能完整：raylib 选择包含所有功能，接受较大的库体积
5. 可移植性 vs 平台优化：raylib 追求最大可移植性，不做平台特定优化
6. 全局状态 vs 上下文传递：raylib 使用全局状态，简化 API 但限制多实例

这些权衡决定了 raylib 的适用场景：教育、原型开发、游戏 jam、独立游戏——
而非 AAA 游戏引擎或高性能实时渲染系统。

### What Raylib Optimizes For

| Priority | How Achieved | Trade-off |
|----------|--------------|-----------|
| **Simplicity** | Single-header includes, minimal dependencies | Less flexibility, larger binaries |
| **Beginner-friendliness** | Simple C API, comprehensive examples | Advanced features hidden or unavailable |
| **Compilation speed** | Header-only libraries, single compilation unit | All features compiled even if unused |
| **Cross-platform portability** | Lowest-common-denominator feature set | Platform-specific optimizations avoided |
| **No build system complexity** | Vendored dependencies, minimal CMake | Large repository size, version pinning |
| **Ease of binding** | C89-compatible API, POD types | No C++ features, limited ergonomics |

### What Raylib Does NOT Optimize For

| Non-Priority | Evidence | Impact |
|--------------|----------|--------|
| **AAA-game performance** | Single-threaded, no job system, basic culling | Not suitable for large open worlds |
| **Minimal binary size** | All modules typically compiled in | ~2-4MB static library |
| **Runtime extensibility** | No plugin system, compile-time config | Rebuild required for changes |
| **Modern GPU features** | OpenGL abstraction over Vulkan/DX12 | No compute shaders (except GL4.3), no ray tracing |
| **Memory efficiency** | Generous static buffers, no pooling | Higher memory usage for small apps |
| **Multi-window support** | Single window assumed | Cannot create multiple windows easily |

### Explicit Architectural Compromises

#### 1. Global State Over Dependency Injection

```c
CoreData CORE = { 0 };  // Single global state
```

**Rationale**: Simplifies API (no context passing), easier for beginners  
**Cost**: No multi-instance support, harder to test in isolation

#### 2. Compile-Time Over Runtime Configuration

```c
// config.h
#define SUPPORT_FILEFORMAT_PNG      1
//#define SUPPORT_FILEFORMAT_BMP    1  // Disabled at compile time
```

**Rationale**: Smaller binaries, no runtime overhead  
**Cost**: Must rebuild for different configurations

#### 3. Inclusion-Based Modularity Over Separate Libraries

```c
// rcore.c includes platform implementation
#include "platforms/rcore_desktop_glfw.c"
```

**Rationale**: Single compilation unit, simpler builds  
**Cost**: Cannot swap backends at link-time

#### 4. Synchronous API Over Async/Callback Model

```c
Texture2D LoadTexture(const char *fileName);  // Blocking
```

**Rationale**: Predictable, easy to understand  
**Cost**: No background loading, main thread blocked during I/O

#### 5. Software Design Patterns Avoided

| Pattern | Why Avoided |
|---------|-------------|
| Dependency Injection | Too complex for C, adds indirection |
| Factory Pattern | Simple constructors suffice |
| Observer Pattern | Polling model preferred (simpler) |
| Entity-Component-System | Beyond library scope |
| Plugin Architecture | Compile-time selection preferred |

---

## STEP 8 — Architecture Lessons I Can Reuse

### Seven Lessons Overview

```
    +===================================================================+
    |           SEVEN REUSABLE ARCHITECTURE PATTERNS                    |
    +===================================================================+
    
    
      +-------------------+        +-------------------+
      |  1. SINGLE PUBLIC |        |  2. COMPILE-TIME  |
      |     HEADER        |        |     PLATFORM SEL  |
      |                   |        |                   |
      |   [mylib.h]       |        |   #if WINDOWS     |
      |       |           |        |     #include win  |
      |       v           |        |   #elif LINUX     |
      |   ALL PUBLIC API  |        |     #include lin  |
      +-------------------+        +-------------------+
    
    
      +-------------------+        +-------------------+
      |  3. VENDORED      |        |  4. PREPROCESSOR  |
      |     DEPENDENCIES  |        |     CONFIG FLAGS  |
      |                   |        |                   |
      |   external/       |        |   config.h:       |
      |   +-- stb_*.h     |        |   #define FEAT_A 1|
      |   +-- miniaudio.h |        |   //#define FEAT_B|
      +-------------------+        +-------------------+
    
    
      +-------------------+        +-------------------+
      |  5. EXTERN/STATIC |        |  6. TEMPLATE      |
      |     SPLIT         |        |     FILES         |
      |                   |        |                   |
      |   CoreData CORE;  |        |   template.c:     |
      |   extern CORE;    |        |   // TODO: impl   |
      |   static internal;|        |   // all funcs    |
      +-------------------+        +-------------------+
    
    
                  +-------------------+
                  |  7. STANDALONE    |
                  |     SUB-LIBRARIES |
                  |                   |
                  |   #define STANDALONE|
                  |   #include "math.h"|
                  |   // works alone!  |
                  +-------------------+
```

**中文说明**：
此图概括了从 raylib 架构中提取的七个可复用模式：

1. **单一公共头文件**：只暴露一个头文件给用户，降低集成复杂度
2. **编译时平台选择**：通过预处理器 #if 选择平台实现，不依赖运行时检测
3. **内嵌依赖**：将所有第三方库以源码形式放入仓库，保证构建可重现
4. **预处理器配置**：使用 #define 启用/禁用功能，在编译时裁剪二进制
5. **extern/static 分离**：共享状态用 extern，私有状态用 static，实现 C 语言封装
6. **模板文件**：提供参考实现模板，指导新平台/后端的开发
7. **可独立运行的子库**：设计模块时考虑独立使用场景，提高复用性

### Lesson 1: Single Public Header Principle

**Principle**: Expose exactly ONE header file to users, containing all public types and function declarations.

**How Raylib Applies It**:
- `raylib.h` is the only required include
- 602 functions declared with consistent `RLAPI` visibility
- All types defined once with guards (`RL_VECTOR2_TYPE`)

**How to Apply in Your Projects**:
```c
// mylib.h - The ONLY public header
#ifndef MYLIB_H
#define MYLIB_H

#ifdef MYLIB_SHARED
    #ifdef _WIN32
        #define MYLIB_API __declspec(dllexport)
    #else
        #define MYLIB_API __attribute__((visibility("default")))
    #endif
#else
    #define MYLIB_API
#endif

// All public types
typedef struct { float x, y; } MyVec2;

// All public functions
MYLIB_API void MyInit(void);
MYLIB_API void MyShutdown(void);

#endif // MYLIB_H
```

---

### Lesson 2: Platform Abstraction via Compile-Time Inclusion

**Principle**: Isolate platform code in separate files, select at compile-time via `#include`.

**How Raylib Applies It**:
```c
// rcore.c
#if defined(PLATFORM_DESKTOP_GLFW)
    #include "platforms/rcore_desktop_glfw.c"
#elif defined(PLATFORM_WEB)
    #include "platforms/rcore_web.c"
#endif
```

**How to Apply in Your Projects**:
```
mylib/
├── src/
│   ├── mylib.c              # Core logic
│   └── platform/
│       ├── mylib_win32.c    # Windows implementation
│       ├── mylib_posix.c    # Unix implementation
│       └── mylib_template.c # Reference for new ports
├── include/
│   └── mylib.h              # Public header
```

```c
// mylib.c
#if defined(_WIN32)
    #include "platform/mylib_win32.c"
#elif defined(__unix__)
    #include "platform/mylib_posix.c"
#endif
```

---

### Lesson 3: Vendor Your Dependencies

**Principle**: Include all dependencies in your repository as source code.

**How Raylib Applies It**:
- `src/external/` contains ~128 third-party files
- All header-only or single-file implementations
- Pinned versions, no external package manager

**Benefits**:
- Reproducible builds across all environments
- No "dependency hell" or version conflicts
- Works offline, in air-gapped environments

**How to Apply**:
```
mylib/
├── external/
│   ├── stb_image.h          # Vendored, pinned version
│   ├── miniaudio.h          # Vendored
│   └── README.md            # Document versions and licenses
```

---

### Lesson 4: Configuration via Preprocessor Guards

**Principle**: Use `#define` flags for compile-time feature selection.

**How Raylib Applies It**:
```c
// config.h
#define SUPPORT_MODULE_RAUDIO           1
#define SUPPORT_FILEFORMAT_PNG          1
//#define SUPPORT_FILEFORMAT_BMP        1  // Disabled

// raudio.c
#if defined(SUPPORT_MODULE_RAUDIO)
// ... entire module ...
#endif
```

**How to Apply**:
```c
// mylib_config.h
#ifndef MYLIB_CONFIG_H
#define MYLIB_CONFIG_H

// Module selection
#define MYLIB_ENABLE_NETWORKING     1
#define MYLIB_ENABLE_ENCRYPTION     1
//#define MYLIB_ENABLE_COMPRESSION  1  // Disabled by default

// Limits
#define MYLIB_MAX_CONNECTIONS       64
#define MYLIB_BUFFER_SIZE           4096

#endif
```

---

### Lesson 5: Extern + Static for Interface/Implementation Split

**Principle**: Use `extern` for shared state access, `static` for hidden implementation details.

**How Raylib Applies It**:
```c
// rcore.c - Defines the global
CoreData CORE = { 0 };

// rcore_desktop_glfw.c - Accesses it
extern CoreData CORE;

// Also in rcore_desktop_glfw.c - Hidden platform state
static PlatformData platform = { 0 };
static void ErrorCallback(int error, const char *description);  // Internal
```

**How to Apply**:
```c
// mylib_state.c
MyLibState MYLIB_STATE = { 0 };  // Global, visible to implementation files

// mylib_impl.c
extern MyLibState MYLIB_STATE;   // Access global
static int internalCounter = 0;  // Hidden from other files
static void internalHelper(void);  // Hidden function
```

---

### Lesson 6: Template Files for Extensibility

**Principle**: Provide a documented template file for adding new implementations.

**How Raylib Applies It**:
- `platforms/rcore_template.c` serves as guide for new ports
- Contains all required function stubs with TODO comments
- Documents the contract (what must be implemented)

**How to Apply**:
```c
// platform/mylib_template.c
/**************************************************************
 * Template for new platform implementations
 * 
 * Required Functions:
 *   - int PlatformInit(void)
 *   - void PlatformShutdown(void)
 *   - ... (list all required)
 **************************************************************/

int PlatformInit(void) {
    // TODO: Initialize your platform
    return 0;
}

void PlatformShutdown(void) {
    // TODO: Cleanup resources
}
```

---

### Lesson 7: Standalone Sub-Libraries

**Principle**: Design modules that can work independently of the main library.

**How Raylib Applies It**:
```c
// raymath.h can be used standalone
#define RAYMATH_STANDALONE
#include "raymath.h"

// rlgl.h can be used standalone
#define RLGL_IMPLEMENTATION
#include "rlgl.h"

// raudio.c can be used standalone
#define RAUDIO_STANDALONE
// ... compile raudio.c alone
```

**How to Apply**:
```c
// mylib_math.h
#ifndef MYLIB_MATH_H
#define MYLIB_MATH_H

#if !defined(MYLIB_VEC2_TYPE)
typedef struct { float x, y; } Vec2;
#define MYLIB_VEC2_TYPE
#endif

// Functions work without rest of library
Vec2 Vec2Add(Vec2 a, Vec2 b);
Vec2 Vec2Scale(Vec2 v, float s);

#endif
```

---

### Summary Table: Architectural Lessons

| # | Principle | Raylib Example | Benefit |
|---|-----------|----------------|---------|
| 1 | Single public header | `raylib.h` only | Simple integration |
| 2 | Compile-time platform selection | `#include "platforms/..."` | Clean separation |
| 3 | Vendored dependencies | `src/external/` | Reproducible builds |
| 4 | Preprocessor configuration | `config.h` flags | Compile-time customization |
| 5 | Extern/static split | `CoreData CORE` vs `static platform` | Encapsulation in C |
| 6 | Template files | `rcore_template.c` | Guided extensibility |
| 7 | Standalone sub-libraries | `raymath.h`, `rlgl.h` | Modular reuse |

---

## Appendix A: Build Configuration Flow

```
    +===================================================================+
    |              RAYLIB BUILD CONFIGURATION FLOW                      |
    +===================================================================+
    
    
    BUILD SYSTEM                        RAYLIB SOURCE
    ============                        =============
    
    Makefile / CMakeLists.txt
           |
           |  -DPLATFORM_DESKTOP_GLFW
           |  -DGRAPHICS_API_OPENGL_33
           |  -DSUPPORT_MODULE_RAUDIO=1
           v
    +------------------+
    |   COMPILER       |
    | (gcc/clang/msvc) |
    +--------+---------+
             |
             |  Preprocessor defines
             v
    +------------------------------------------+
    |              config.h                    |
    |                                          |
    |  // Module selection                     |
    |  #define SUPPORT_MODULE_RSHAPES     1    |
    |  #define SUPPORT_MODULE_RTEXTURES   1    |
    |  #define SUPPORT_MODULE_RTEXT       1    |
    |  #define SUPPORT_MODULE_RMODELS     1    |
    |  #define SUPPORT_MODULE_RAUDIO      1    |  <-- Can be 0 to disable
    |                                          |
    |  // Feature selection                    |
    |  #define SUPPORT_FILEFORMAT_PNG     1    |
    |  #define SUPPORT_FILEFORMAT_OGG     1    |
    |  //#define SUPPORT_FILEFORMAT_BMP   1    |  <-- Commented = disabled
    |                                          |
    |  // Limits                               |
    |  #define MAX_KEYBOARD_KEYS        512    |
    |  #define MAX_TOUCH_POINTS           8    |
    +------------------------------------------+
             |
             | Read by all modules
             v
    +------------------------------------------+
    |              rcore.c                     |
    |                                          |
    |  #include "config.h"                     |
    |                                          |
    |  #if defined(PLATFORM_DESKTOP_GLFW)      |
    |    #include "platforms/rcore_glfw.c"     |
    |  #endif                                  |
    |                                          |
    +------------------------------------------+
             |
             | And each module checks:
             v
    +------------------------------------------+
    |              raudio.c                    |
    |                                          |
    |  #if defined(SUPPORT_MODULE_RAUDIO)      |
    |      // entire module code               |
    |  #endif                                  |
    |                                          |
    |  #if defined(SUPPORT_FILEFORMAT_OGG)     |
    |      // OGG loading code                 |
    |  #endif                                  |
    +------------------------------------------+
             |
             v
    +------------------------------------------+
    |          FINAL BINARY                    |
    |  (Only enabled features compiled in)     |
    +------------------------------------------+
```

**中文说明**：
此图展示了 raylib 的构建配置流程，从构建系统到最终二进制文件：

1. **构建系统**（Makefile/CMake）传递预处理器宏给编译器
   - `-DPLATFORM_DESKTOP_GLFW`：选择桌面 GLFW 平台
   - `-DGRAPHICS_API_OPENGL_33`：选择 OpenGL 3.3
   - 可以覆盖 config.h 中的默认值

2. **config.h** 是配置中心，定义：
   - 模块选择：哪些模块要编译（如 RAUDIO、RMODELS）
   - 功能选择：支持哪些文件格式（如 PNG、OGG）
   - 限制值：数组大小等常量

3. **各模块检查配置**：
   - 模块整体用 `SUPPORT_MODULE_*` 控制
   - 具体功能用 `SUPPORT_FILEFORMAT_*` 控制
   - 被禁用的代码在编译时完全跳过

4. **最终二进制**：只包含启用的功能，未启用的代码不会进入二进制文件

这种设计允许用户根据需求裁剪 raylib，在二进制大小和功能之间取得平衡。

## Appendix B: Source File Organization

```
    +===================================================================+
    |              RAYLIB SOURCE DIRECTORY STRUCTURE                    |
    +===================================================================+
    
    
    raylib/
    |
    +-- src/                              [MAIN SOURCE DIRECTORY]
    |   |
    |   +-- raylib.h          (1,728 ln)  [PUBLIC API - User includes this]
    |   |
    |   +-- rcore.c           (4,365 ln)  [CORE - Window, input, main loop]
    |   +-- rlgl.h            (5,383 ln)  [GRAPHICS - OpenGL abstraction]
    |   +-- raymath.h         (3,021 ln)  [MATH - Vectors, matrices, quat]
    |   +-- rcamera.h           (598 ln)  [CAMERA - 3D camera systems]
    |   +-- rgestures.h         (530 ln)  [GESTURES - Touch detection]
    |   |
    |   +-- rtextures.c       (5,610 ln)  [TEXTURES - Image loading/proc]
    |   +-- rtext.c           (2,796 ln)  [TEXT - Font rendering]
    |   +-- rshapes.c         (2,487 ln)  [SHAPES - 2D primitives]
    |   +-- rmodels.c         (7,131 ln)  [MODELS - 3D mesh/material]
    |   +-- raudio.c          (2,896 ln)  [AUDIO - Sound playback]
    |   |
    |   +-- utils.c             (510 ln)  [UTILS - Logging, file I/O]
    |   +-- utils.h              (82 ln)  [UTILS - Internal header]
    |   +-- config.h            (298 ln)  [CONFIG - Build options]
    |   |
    |   +-- platforms/                    [PLATFORM IMPLEMENTATIONS]
    |   |   +-- rcore_desktop_glfw.c      [Windows/Linux/macOS via GLFW]
    |   |   +-- rcore_desktop_sdl.c       [Cross-platform via SDL]
    |   |   +-- rcore_desktop_rgfw.c      [Alternative: RGFW backend]
    |   |   +-- rcore_desktop_win32.c     [Native Windows API]
    |   |   +-- rcore_drm.c               [Linux DRM/KMS]
    |   |   +-- rcore_android.c           [Android NDK]
    |   |   +-- rcore_web.c               [Emscripten/WebGL]
    |   |   +-- rcore_memory.c            [Software renderer]
    |   |   +-- rcore_template.c          [Template for new ports]
    |   |
    |   +-- external/                     [VENDORED DEPENDENCIES]
    |       +-- stb_image.h               [Image loading]
    |       +-- stb_truetype.h            [TTF font loading]
    |       +-- miniaudio.h               [Audio playback]
    |       +-- cgltf.h                   [glTF model loading]
    |       +-- glad.h                    [OpenGL loader]
    |       +-- RGFW.h                    [Window management alt]
    |       +-- glfw/                     [GLFW source tree]
    |       +-- ... (~128 files total)
    |
    +-- examples/                         [USAGE EXAMPLES]
    |   +-- core/                         [Window, input, timing]
    |   +-- shapes/                       [2D drawing]
    |   +-- textures/                     [Image handling]
    |   +-- text/                         [Font rendering]
    |   +-- models/                       [3D graphics]
    |   +-- shaders/                      [Shader effects]
    |   +-- audio/                        [Sound examples]
    |
    +-- projects/                         [IDE PROJECT FILES]
        +-- VS2022/                       [Visual Studio]
        +-- VSCode/                       [VS Code config]
        +-- CMake/                        [CMake example]
```

**中文说明**：
此图展示了 raylib 源代码目录结构：

1. **src/ 主源码目录**：
   - **raylib.h**：唯一的公共头文件，用户只需引入这一个
   - **rcore.c**：核心模块，包含窗口、输入、主循环逻辑
   - **rlgl.h / raymath.h**：可独立使用的子库
   - **r*.c 模块**：各领域功能实现（纹理、文字、形状、模型、音频）
   - **utils 和 config**：内部工具和编译配置

2. **platforms/ 目录**：所有平台特定代码的隔离区
   - 每个平台一个文件
   - 编译时只选择一个

3. **external/ 目录**：第三方依赖源码
   - 全部内嵌，无需外部包管理器
   - 约 128 个文件

4. **examples/ 目录**：按功能分类的示例代码

5. **projects/ 目录**：各种 IDE 和构建系统的项目文件

## Appendix C: File Size Reference

| File | Lines | Primary Responsibility |
|------|-------|----------------------|
| `raylib.h` | 1,728 | Public API |
| `rcore.c` | 4,365 | Core + platform orchestration |
| `rlgl.h` | 5,383 | OpenGL abstraction |
| `rtextures.c` | 5,610 | Image/texture handling |
| `rmodels.c` | 7,131 | 3D model loading |
| `rtext.c` | 2,796 | Font/text rendering |
| `raudio.c` | 2,896 | Audio system |
| `rshapes.c` | 2,487 | 2D shape drawing |
| `raymath.h` | 3,021 | Math library |
| **Total core** | ~35,400 | — |

---

## Conclusion

### Complete Architecture Summary

```
+===========================================================================+
|                    RAYLIB COMPLETE ARCHITECTURE MAP                       |
+===========================================================================+

USER APPLICATION
================
     |
     |  #include "raylib.h"
     v
+===========================================================================+
|  LAYER 5: PUBLIC API                                                      |
|  raylib.h (1,728 lines)                                                   |
|  +---------------------------------+------------------------------------+ |
|  | TYPES                           | FUNCTIONS (602 RLAPI)              | |
|  | Vector2, Vector3, Color,        | InitWindow, DrawTexture,           | |
|  | Texture2D, Font, Sound, ...     | LoadImage, PlaySound, ...          | |
|  +---------------------------------+------------------------------------+ |
+===========================================================================+
     |
     | implements
     v
+===========================================================================+
|  LAYER 4: DOMAIN MODULES (Optional, configurable via config.h)            |
|                                                                           |
|  +-------------+ +------------+ +------------+ +------------+ +---------+ |
|  | rtextures.c | | rtext.c    | | rshapes.c  | | rmodels.c  | |raudio.c | |
|  | (5,610 ln)  | | (2,796 ln) | | (2,487 ln) | | (7,131 ln) | |(2,896)  | |
|  |             | |            | |            | |            | |         | |
|  | LoadImage   | | LoadFont   | | DrawCircle | | LoadModel  | |PlaySound| |
|  | LoadTexture | | DrawText   | | DrawRect   | | DrawMesh   | |LoadWAV  | |
|  +------+------+ +-----+------+ +-----+------+ +-----+------+ +----+----+ |
|         |              |              |              |              |     |
+=========|==============|==============|==============|==============|=====+
          |              |              |              |              |
          +-------+------+------+-------+              |              |
                  |             |                      |              |
                  v             v                      |              |
+===========================================================================+
|  LAYER 3: CORE INFRASTRUCTURE                                             |
|                                                                           |
|  +-----------------+  +---------------+  +-------------+  +-------------+ |
|  | rcore.c         |  | rlgl.h        |  | raymath.h   |  | utils.c/h   | |
|  | (4,365 lines)   |  | (5,383 lines) |  | (3,021 ln)  |  | (~500 ln)   | |
|  |                 |  |               |  |             |  |             | |
|  | InitWindow()    |  | rlBegin()     |  | Vector2Add  |  | TRACELOG    | |
|  | BeginDrawing()  |  | rlVertex2f()  |  | MatrixMult  |  | LoadFileData| |
|  | IsKeyPressed()  |  | rlSetTexture  |  | QuatSlerp   |  | SaveFileData| |
|  +--------+--------+  +-------+-------+  +-------------+  +-------------+ |
|           |                   |                                           |
|           |  #include         |                                           |
|           v                   v                                           |
+===========================================================================+
|  LAYER 2: PLATFORM ABSTRACTION (Only ONE compiled per build)              |
|                                                                           |
|  +----------------+ +----------------+ +----------------+ +-------------+  |
|  |rcore_glfw.c    | |rcore_sdl.c     | |rcore_web.c     | |rcore_drm.c  |  |
|  |                | |                | |                | |             |  |
|  | Windows        | | Cross-platform | | WebAssembly    | | Raspberry   |  |
|  | Linux          | | (SDL2/SDL3)    | | (Emscripten)   | | Pi (KMS)    |  |
|  | macOS          | |                | |                | |             |  |
|  | BSD            | |                | |                | |             |  |
|  +-------+--------+ +-------+--------+ +--------+-------+ +------+------+  |
|          |                  |                   |                |         |
+==========|==================|===================|================|=========+
           |                  |                   |                |
           v                  v                   v                v
+===========================================================================+
|  LAYER 1: EXTERNAL DEPENDENCIES (Vendored in src/external/)               |
|                                                                           |
|  +------+ +----------+ +--------+ +------+ +-------+ +------+ +--------+  |
|  | GLFW | | miniaudio| | stb_*  | | glad | | cgltf | | RGFW | |dr_libs |  |
|  +------+ +----------+ +--------+ +------+ +-------+ +------+ +--------+  |
|                                                                           |
+===========================================================================+
           |
           v
+===========================================================================+
|  OPERATING SYSTEM / HARDWARE                                              |
|                                                                           |
|  +----------+ +------------+ +----------+ +-----------+ +---------------+ |
|  | Win32 API| | X11/Wayland| | Cocoa    | |Android NDK| | WebGL/Browser | |
|  +----------+ +------------+ +----------+ +-----------+ +---------------+ |
|                                                                           |
+===========================================================================+
```

**中文说明**：
此图是 raylib 完整架构的总览，从上到下展示了五层结构和关键组件：

1. **用户应用程序**：只需 `#include "raylib.h"` 即可使用全部功能

2. **第5层 - 公共 API**：raylib.h 定义了所有公开类型和 602 个 RLAPI 函数

3. **第4层 - 领域模块**：可选模块，通过 config.h 启用/禁用
   - rtextures: 图像和纹理处理
   - rtext: 字体和文字渲染
   - rshapes: 2D 图元绘制
   - rmodels: 3D 模型加载和渲染
   - raudio: 音频播放（完全独立）

4. **第3层 - 核心基础设施**：
   - rcore.c: 窗口管理、主循环、输入处理
   - rlgl.h: OpenGL 版本抽象（支持 1.1 到 ES3）
   - raymath.h: 向量、矩阵、四元数数学库
   - utils: 日志、文件 I/O 工具

5. **第2层 - 平台抽象**：每次编译只选择一个平台实现

6. **第1层 - 外部依赖**：全部以源码形式内嵌，无需外部包管理器

这种分层设计使 raylib 能够支持 12+ 平台、7 种 OpenGL 版本、600+ 公开函数，
同时保持了代码的可维护性和 12 年以上的持续演进能力。

Raylib demonstrates that a **well-layered C architecture** can remain maintainable across 12+ years of development while supporting:

- 12+ platforms (Windows, Linux, macOS, Android, Web, Raspberry Pi, etc.)
- 7 OpenGL versions (1.1 through ES3)
- 600+ public API functions
- Multiple window backends (GLFW, RGFW, SDL, native Win32)

The architecture succeeds through:
1. **Strict layer boundaries** — Domain modules never touch platform code
2. **Compile-time flexibility** — Configuration without runtime overhead
3. **Single responsibility files** — Each `.c` file has one clear purpose
4. **Defensive type guards** — Modules work standalone or together
5. **Template-based extensibility** — Clear contracts for new implementations

These patterns are directly transferable to any medium-to-large C codebase requiring cross-platform support and long-term maintainability.

