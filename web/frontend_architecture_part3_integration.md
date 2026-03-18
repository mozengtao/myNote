# Part 5–9 — Integration, Structure, and Mental Models

*Continued from [Part 2 — Technology Deep Dives](frontend_architecture_part2_deep_dives.md)*

---

# Part 5 — How Everything Works Together

## The Full Request Lifecycle

Let's trace a complete user interaction through every layer of the stack. This is the frontend equivalent of tracing a syscall from userspace through the kernel to hardware and back.

### Scenario: User clicks "Load Users" on a dashboard page

```
+-------------------------------------------------------------------+
|  PHASE 1: User Interaction                                         |
|-------------------------------------------------------------------|
|                                                                    |
|  User clicks "Load Users" button                                   |
|       |                                                            |
|       v                                                            |
|  Browser dispatches 'click' DOM event                              |
|       |                                                            |
|       v                                                            |
|  React's synthetic event system captures the event                 |
|  (React uses event delegation on the root — single listener)       |
|       |                                                            |
|       v                                                            |
|  onClick handler fires in <DashboardPage> component                |
|                                                                    |
+-------------------------------------------------------------------+
         |
         v
+-------------------------------------------------------------------+
|  PHASE 2: Navigation (React Router)                                |
|-------------------------------------------------------------------|
|                                                                    |
|  Handler calls: navigate('/dashboard/users')                       |
|       |                                                            |
|       v                                                            |
|  React Router:                                                     |
|    1. Calls history.pushState() — updates URL bar                  |
|       (no HTTP request — browser API only)                         |
|    2. Matches new URL to route tree                                |
|    3. Determines: render <DashboardLayout> -> <UsersPage>          |
|    4. Triggers React re-render with new route context              |
|                                                                    |
+-------------------------------------------------------------------+
         |
         v
+-------------------------------------------------------------------+
|  PHASE 3: Data Fetching (TanStack Query)                           |
|-------------------------------------------------------------------|
|                                                                    |
|  <UsersPage> mounts and calls:                                     |
|    useQuery({ queryKey: ['users'], queryFn: fetchUsers })          |
|       |                                                            |
|       v                                                            |
|  TanStack Query checks cache:                                      |
|                                                                    |
|    Cache hit (fresh)?  --YES-->  Return cached data immediately     |
|         |                        (skip network, go to Phase 5)      |
|         NO                                                         |
|         |                                                          |
|    Cache hit (stale)? --YES-->   Return stale data immediately      |
|         |                        AND trigger background refetch     |
|         NO                                                         |
|         |                                                          |
|    Cache miss:                                                     |
|         |                                                          |
|         v                                                          |
|    Set status = 'loading'                                          |
|    Execute queryFn:                                                |
|      fetch('https://api.example.com/users')                        |
|         |                                                          |
|         v                                                          |
|    Browser's fetch API -> HTTP GET request over network            |
|                                                                    |
+-------------------------------------------------------------------+
         |
         v
+-------------------------------------------------------------------+
|  PHASE 4: Server Response                                          |
|-------------------------------------------------------------------|
|                                                                    |
|  Backend API receives GET /api/users                               |
|       |                                                            |
|       v                                                            |
|  Backend queries database, returns JSON:                           |
|  [{ "id": 1, "name": "Alice" }, { "id": 2, "name": "Bob" }]      |
|       |                                                            |
|       v                                                            |
|  Browser receives HTTP response                                    |
|       |                                                            |
|       v                                                            |
|  TanStack Query:                                                   |
|    1. Deserializes JSON response                                   |
|    2. Stores in cache: key=['users'], data=User[]                  |
|    3. Sets status = 'success'                                      |
|    4. Sets staleTime countdown (e.g., 30 seconds)                  |
|    5. Notifies all subscribed components                           |
|                                                                    |
+-------------------------------------------------------------------+
         |
         v
+-------------------------------------------------------------------+
|  PHASE 5: React Re-render                                          |
|-------------------------------------------------------------------|
|                                                                    |
|  TanStack Query's state change triggers React re-render:           |
|                                                                    |
|  1. RENDER phase (pure computation):                               |
|     - Call UsersPage() function                                    |
|     - useQuery returns { data: User[], isLoading: false }          |
|     - Component returns new Virtual DOM tree                       |
|       with <UserTable> containing user rows                        |
|                                                                    |
|  2. RECONCILIATION phase (diffing):                                |
|     - Compare new VDOM with previous VDOM                          |
|     - Previous: <Spinner /> (loading state)                        |
|     - New: <table> with <tr> for each user                         |
|     - Compute patch: remove Spinner, insert table                  |
|                                                                    |
|  3. COMMIT phase (DOM mutations):                                  |
|     - Apply minimal DOM patches                                    |
|     - Tailwind CSS classes take effect (browser applies styles)    |
|     - User sees the data table                                     |
|                                                                    |
+-------------------------------------------------------------------+
         |
         v
+-------------------------------------------------------------------+
|  PHASE 6: UI Update Complete                                       |
|-------------------------------------------------------------------|
|                                                                    |
|  DOM is updated. Browser repaints.                                 |
|  User sees the users table with data.                              |
|                                                                    |
|  Total time breakdown (typical):                                   |
|    Click -> Router match:        ~1ms   (synchronous)              |
|    React re-render (loading):    ~5ms   (show spinner)             |
|    Network request:              ~100-500ms (API call)              |
|    React re-render (data):       ~5-20ms (render table)            |
|    Browser paint:                ~1-5ms  (display update)          |
|                                                                    |
+-------------------------------------------------------------------+
```

### Where State Management Fits

```
+-------------------------------------------------------------------+
|              State Management in the Request Lifecycle              |
|-------------------------------------------------------------------|
|                                                                    |
|  User clicks "Load Users"                                          |
|       |                                                            |
|       +--- Zustand: set activeTab = 'users'                        |
|       |    (client state — instant, synchronous)                   |
|       |                                                            |
|       +--- React Router: navigate('/dashboard/users')              |
|       |    (URL state — encoded in address bar)                    |
|       |                                                            |
|       +--- TanStack Query: fetch users from API                    |
|            (server state — async, cached, refetchable)             |
|                                                                    |
|  Three state channels, three tools, zero overlap:                  |
|                                                                    |
|  +------------------+------------------+-------------------+       |
|  | Client State     | URL State        | Server State      |       |
|  | (Zustand)        | (React Router)   | (TanStack Query)  |       |
|  |------------------|------------------|-------------------|       |
|  | theme: 'dark'    | /dashboard/users | users: User[]     |       |
|  | sidebar: open    | ?page=2&sort=name| orders: Order[]   |       |
|  | modal: visible   | #section         | products: [...]   |       |
|  | form draft: ...  |                  |                   |       |
|  +------------------+------------------+-------------------+       |
|         |                    |                   |                  |
|         v                    v                   v                  |
|              All three feed into React re-renders                  |
|                                                                    |
+-------------------------------------------------------------------+
```

### Mutation Flow: Creating a New User

```
User submits form
     |
     v
React form handler:
  mutation.mutate({ name: 'Charlie', email: 'charlie@co.com' })
     |
     v
TanStack Query mutation:
  POST /api/users  -->  Backend  -->  201 Created
     |
     v
onSuccess callback:
  queryClient.invalidateQueries({ queryKey: ['users'] })
     |
     v
TanStack Query:
  Mark ['users'] cache as stale
  Trigger background refetch: GET /api/users
     |
     v
Fresh data arrives:
  Cache updated with Charlie included
     |
     v
React re-renders:
  <UserTable> now includes Charlie
     |
     v
User sees updated list (no manual refresh needed)
```

This is the frontend equivalent of a **write-through cache** pattern. The mutation writes to the server (source of truth), then invalidates the cache, which triggers a refetch to synchronize the client's view.

---

# Part 6 — Project Structure

## A Realistic Modern Frontend Project

```
my-app/
|
+-- public/                    # Static assets served as-is (favicon, robots.txt)
|
+-- src/
|   |
|   +-- components/            # Reusable UI components (the "library")
|   |   +-- ui/                # Primitives: Button, Input, Modal, Card
|   |   +-- layout/            # Structural: Header, Sidebar, Footer, PageShell
|   |   +-- feedback/          # User feedback: Toast, Spinner, ErrorBoundary
|   |   +-- data-display/      # Tables, Lists, Charts, Stat cards
|   |   +-- forms/             # Form primitives: FormField, Select, DatePicker
|   |
|   +-- pages/                 # Page-level components (one per route)
|   |   +-- dashboard/
|   |   |   +-- DashboardPage.tsx
|   |   |   +-- components/    # Page-specific components (not reusable)
|   |   +-- users/
|   |   |   +-- UsersPage.tsx
|   |   |   +-- UserDetailPage.tsx
|   |   |   +-- components/
|   |   +-- settings/
|   |       +-- SettingsPage.tsx
|   |
|   +-- router/                # Route definitions and guards
|   |   +-- index.tsx          # Route tree (createBrowserRouter)
|   |   +-- guards.tsx         # Auth guards, role checks
|   |
|   +-- store/                 # Client state (Zustand stores)
|   |   +-- useAppStore.ts     # Global UI state
|   |   +-- useAuthStore.ts    # Authentication state
|   |
|   +-- services/              # API client layer
|   |   +-- api.ts             # Base fetch/axios configuration
|   |   +-- users.ts           # User-related API functions
|   |   +-- orders.ts          # Order-related API functions
|   |
|   +-- hooks/                 # Custom React hooks
|   |   +-- useUsers.ts        # TanStack Query hook for users
|   |   +-- useDebounce.ts     # Utility hook
|   |   +-- useMediaQuery.ts   # Responsive breakpoint hook
|   |
|   +-- types/                 # Shared TypeScript types/interfaces
|   |   +-- user.ts            # User, UserRole, CreateUserInput
|   |   +-- order.ts           # Order, OrderStatus, LineItem
|   |   +-- api.ts             # ApiResponse<T>, PaginatedResult<T>
|   |
|   +-- utils/                 # Pure utility functions
|   |   +-- format.ts          # Date formatting, currency, etc.
|   |   +-- validation.ts      # Input validation helpers
|   |
|   +-- App.tsx                # Root component
|   +-- main.tsx               # Entry point (renders App into DOM)
|
+-- index.html                 # HTML shell (Vite entry point)
+-- vite.config.ts             # Vite configuration
+-- tsconfig.json              # TypeScript configuration
+-- tailwind.config.ts         # Tailwind configuration
+-- package.json               # Dependencies and scripts
```

### Directory Purpose Map

| Directory      | Purpose                                    | Backend Analogy                           |
|----------------|--------------------------------------------|--------------------------------------------|
| `components/`  | Reusable UI building blocks                | Shared library / utility package           |
| `pages/`       | Route-level compositions                   | HTTP handlers / controllers                |
| `router/`      | URL → component mapping                    | HTTP multiplexer / routing table           |
| `store/`       | Global client state                        | In-process state / config store            |
| `services/`    | Backend API communication                  | Service client / gRPC stubs                |
| `hooks/`       | Reusable stateful logic                    | Shared middleware / interceptors           |
| `types/`       | Data structures and interfaces             | Proto definitions / header files           |
| `utils/`       | Pure functions with no dependencies        | Utility library (`pkg/util` in Go)         |

### Dependency Flow (Layered Architecture)

```
pages/  ──────────────> components/
  |                         |
  +-------> hooks/ ---------+
  |           |
  +-------> services/ ──────> (External API)
  |           |
  +-------> store/ ─────────> (Zustand)
  |
  +-------> types/ <──────── (shared by all layers)
  |
  +-------> utils/ <──────── (shared by all layers)
```

**Rule**: Dependencies flow downward. `pages/` can import from `components/`, `hooks/`, `services/`, and `store/`. But `components/` should NOT import from `pages/` or `store/` directly (they receive data via props). This mirrors the dependency rule in Clean Architecture.

---

# Part 7 — Next.js vs Vite + React

## Architectural Comparison

These represent two fundamentally different deployment architectures:

```
+-------------------------------------------------------------------+
|              Vite + React (Client-Side SPA)                        |
|------------------------------------------------------------------- |
|                                                                    |
|  Build Output: Static files (.html + .js + .css)                   |
|  Deployment: CDN / static file server (S3, Cloudflare Pages)       |
|  Server: None required (or simple API-only backend)                |
|                                                                    |
|  Request Flow:                                                     |
|                                                                    |
|  Browser ----GET /----> CDN ----> index.html (empty shell)         |
|  Browser ----GET /app.js---> CDN ----> JavaScript bundle           |
|  Browser: JS executes, React mounts, renders UI                    |
|  Browser ----GET /api/users---> API Server ----> JSON data         |
|  Browser: React re-renders with data                               |
|                                                                    |
|  Timeline:                                                         |
|  [--- blank screen ---][--- loading ---][--- interactive ---]      |
|  0ms                  200ms           500ms                        |
|                                                                    |
+-------------------------------------------------------------------+

+-------------------------------------------------------------------+
|              Next.js (Server-Rendered Fullstack)                   |
|------------------------------------------------------------------- |
|                                                                    |
|  Build Output: Server code + static assets                         |
|  Deployment: Node.js server (Vercel, Docker, AWS)                  |
|  Server: Active participant in rendering                           |
|                                                                    |
|  Request Flow:                                                     |
|                                                                    |
|  Browser ----GET /dashboard----> Next.js Server                    |
|    Server: match route                                             |
|    Server: fetch data from DB/API                                  |
|    Server: render React to HTML                                    |
|    Server: send complete HTML + JS                                 |
|  Browser: shows HTML immediately (content visible)                 |
|  Browser: downloads JS, hydrates (attaches interactivity)          |
|                                                                    |
|  Timeline:                                                         |
|  [--- content visible ---][--- hydrating ---][--- interactive ---] |
|  0ms                     100ms              300ms                  |
|                                                                    |
+-------------------------------------------------------------------+
```

### Decision Matrix

```
+-------------------------------------------------------------------+
|                    When to Use Which                               |
|------------------------------------------------------------------- |
|                                                                    |
|  Use Vite + React when:                                            |
|    [x] Building internal tools / admin dashboards                  |
|    [x] Users are authenticated (no SEO needed)                     |
|    [x] You have a separate backend team/service                    |
|    [x] You want simple deployment (CDN / S3)                       |
|    [x] You want maximum flexibility in architecture                |
|    [x] Team is small, iteration speed matters most                 |
|                                                                    |
|  Use Next.js when:                                                 |
|    [x] SEO is critical (public-facing content)                     |
|    [x] First-load performance matters (e-commerce, marketing)      |
|    [x] You want one codebase for frontend + API                    |
|    [x] You need mixed rendering (some pages static, some dynamic)  |
|    [x] You want React Server Components for smaller bundles        |
|    [x] Your team can manage server infrastructure                  |
|                                                                    |
+-------------------------------------------------------------------+
```

### Architectural Tradeoff Summary

| Dimension              | Vite + React (SPA)         | Next.js (Fullstack)           |
|------------------------|----------------------------|-------------------------------|
| Deployment             | Static hosting (CDN)       | Node.js server required       |
| Infrastructure cost    | Very low                   | Moderate (compute + CDN)      |
| Initial load           | Slower (blank -> load JS)  | Faster (server HTML)          |
| SEO                    | Poor without workarounds   | Excellent                     |
| Complexity             | Lower                      | Higher (server + client)      |
| Flexibility            | Maximum                    | Opinionated framework         |
| Backend coupling       | Decoupled                  | Can be coupled                |
| Build time             | Fast                       | Slower (multiple outputs)     |
| Hosting options        | Anywhere                   | Node.js host required         |

**Backend analogy**: Vite + React is like writing a CLI tool that talks to an API. Next.js is like writing a monolithic web server that renders HTML and serves an API — more powerful but more infrastructure.

---

# Part 8 — Tool Tradeoffs

## Alternative Comparison Matrix

### Build Tools

| Tool       | Language | Dev Strategy          | Prod Strategy    | Speed    | Maturity |
|------------|----------|-----------------------|------------------|----------|----------|
| **Vite**   | TS/JS    | Native ESM, on-demand | Rollup bundling  | Fast     | High     |
| Webpack    | JS       | Bundle everything     | Bundle + optimize| Moderate | Highest  |
| Turbopack  | Rust     | Incremental compute   | (developing)     | Fastest  | Low      |
| esbuild    | Go       | Bundle                | Bundle           | Fastest  | Moderate |
| Parcel     | Rust/JS  | Zero-config           | Bundle           | Fast     | Moderate |

**When to switch from Vite**: If you're using Next.js (uses Turbopack), or if you have extremely complex build requirements that only Webpack plugins support.

### State Management

| Tool       | Model                    | Bundle | Learning Curve | Best For                   |
|------------|--------------------------|--------|----------------|----------------------------|
| **Zustand**| Simple store + selectors | 1KB    | Low            | Most apps, simple to medium|
| Redux (RTK)| Actions + reducers       | 5KB+   | Medium-High    | Large teams, event logging |
| Jotai      | Atomic (bottom-up)       | 3KB    | Low            | Fine-grained reactivity    |
| Valtio     | Proxy-based mutation     | 3KB    | Low            | Mutable-style preference   |
| Recoil     | Graph-based atoms        | 20KB   | Medium         | Complex derived state      |
| MobX       | Observable + reactions   | 15KB   | Medium         | OOP-style reactivity       |

**Decision rule**: Start with Zustand. Move to Redux only if your team needs strict action logging or time-travel debugging. Use Jotai if you find yourself creating many tiny independent state atoms.

### Data Fetching

| Tool            | Protocol    | Caching        | Bundle | Best For                    |
|-----------------|-------------|----------------|--------|-----------------------------|
| **TanStack Query** | REST/any | Stale-while-revalidate | 12KB | REST APIs, general use   |
| SWR             | REST/any    | Stale-while-revalidate | 4KB  | Simpler needs, Vercel stack |
| Apollo Client   | GraphQL     | Normalized cache| 30KB+ | GraphQL APIs               |
| RTK Query       | REST/any    | Redux-integrated| (incl) | Already using Redux         |
| tRPC            | TypeScript RPC| End-to-end types | 5KB  | Full-stack TypeScript      |

**Decision rule**: TanStack Query for REST APIs. Apollo if you use GraphQL. tRPC if you control both frontend and backend in TypeScript and want end-to-end type safety without API schemas.

### Styling

| Approach           | Runtime Cost | Type Safety | Colocation | Best For                |
|--------------------|-------------|-------------|------------|--------------------------|
| **Tailwind CSS**   | Zero        | No          | Inline     | Rapid prototyping, teams |
| CSS Modules        | Zero        | No          | Adjacent   | Traditional CSS fans     |
| styled-components  | Runtime JS  | Partial     | Inline     | Dynamic/theme-heavy      |
| Vanilla Extract    | Zero        | Full        | Adjacent   | Type-safe CSS            |
| Panda CSS          | Zero        | Full        | Inline     | Tailwind + type safety   |

**Decision rule**: Tailwind for most projects. CSS Modules if the team prefers writing real CSS. Vanilla Extract if you want TypeScript-powered zero-runtime styles.

### Frameworks

| Framework    | Rendering        | Language   | Deployment        | Best For               |
|-------------|------------------|-----------|-------------------|------------------------|
| **Next.js**  | SSR/SSG/ISR/CSR  | React/TS  | Node.js / Vercel   | SEO, fullstack        |
| Remix        | SSR, progressive | React/TS  | Any Node.js host   | Web standards purist  |
| Nuxt         | SSR/SSG          | Vue/TS    | Node.js / Nitro    | Vue ecosystem         |
| SvelteKit    | SSR/SSG          | Svelte/TS | Any adapter        | Minimal runtime       |
| Astro        | SSG + islands    | Any       | Static / SSR       | Content-heavy sites   |
| **Vite + React** | CSR only    | React/TS  | Static CDN         | SPAs, internal tools  |

---

# Part 9 — The Complete Mental Model

## The Frontend Platform Stack

Think of the modern frontend as a **platform** with clearly separated concerns, much like a simplified OS:

```
+===================================================================+
|                    THE FRONTEND PLATFORM                           |
+===================================================================+
|                                                                    |
|  Layer 5: FRAMEWORK (optional)                                     |
|  +--------------------------------------------------------------+  |
|  |  Next.js                                                     |  |
|  |  Orchestrates everything: rendering strategy, routing,       |  |
|  |  API, deployment. Like a PaaS for frontend.                  |  |
|  +--------------------------------------------------------------+  |
|                                                                    |
|  Layer 4: APPLICATION SERVICES                                     |
|  +------------------+  +------------------+  +----------------+    |
|  | React Router     |  | TanStack Query   |  | Zustand        |    |
|  | Navigation       |  | Server State     |  | Client State   |    |
|  | URL -> Component |  | Cache + Sync     |  | UI State       |    |
|  +------------------+  +------------------+  +----------------+    |
|                                                                    |
|  Layer 3: UI ENGINE                                                |
|  +--------------------------------------------------------------+  |
|  |  React                                                       |  |
|  |  Component model, Virtual DOM, reconciliation, hooks         |  |
|  |  The core runtime that drives all UI rendering               |  |
|  +--------------------------------------------------------------+  |
|                                                                    |
|  Layer 2: PRESENTATION                                             |
|  +--------------------------------------------------------------+  |
|  |  Tailwind CSS                                                |  |
|  |  Utility classes compiled to minimal CSS at build time       |  |
|  |  No runtime cost — pure stylesheet                           |  |
|  +--------------------------------------------------------------+  |
|                                                                    |
|  Layer 1: BUILD TOOLCHAIN                                          |
|  +--------------------------------------------------------------+  |
|  |  Vite (build) + TypeScript (types) + Tailwind (CSS gen)      |  |
|  |  Source -> Transform -> Bundle -> Optimize                   |  |
|  |  Exists only at build time. Zero runtime presence.           |  |
|  +--------------------------------------------------------------+  |
|                                                                    |
|  Layer 0: BROWSER RUNTIME                                          |
|  +--------------------------------------------------------------+  |
|  |  V8/SpiderMonkey JS engine, DOM API, CSS engine, Network     |  |
|  |  The "operating system" that runs your application           |  |
|  +--------------------------------------------------------------+  |
|                                                                    |
+===================================================================+
```

## Mapping to Backend Concepts

| Frontend Concept          | Backend Equivalent                              |
|---------------------------|--------------------------------------------------|
| Browser                   | Operating system                                 |
| JavaScript engine (V8)    | Process virtual machine / runtime                |
| React                     | GUI toolkit (Qt, GTK) / template engine          |
| Virtual DOM               | Shadow page table / double buffering             |
| React Router              | HTTP multiplexer / request router                |
| TanStack Query            | Caching proxy / read-through cache               |
| Zustand store             | In-process state / configuration store           |
| Tailwind CSS              | Code generation from schema / protocol buffers   |
| Vite (dev mode)           | Interpreted / JIT mode (`go run`, `cargo run`)   |
| Vite (prod mode)          | Optimized compilation (`go build -ldflags`, `-O2`)|
| TypeScript                | Static analysis / type checker (`mypy`, borrow checker) |
| Component                 | Module / class with a render interface           |
| Props                     | Function parameters / struct fields              |
| Hooks                     | Stateful closures / context-local storage        |
| `useEffect`               | Deferred callback / destructor / `defer`         |
| `useState`                | Thread-local variable / register                 |
| Code splitting            | Dynamic linking / lazy loading shared libraries  |
| HMR                       | Hot code reload (Erlang) / live patching         |
| Hydration                 | Attaching signal handlers to pre-allocated structs|
| Next.js SSR               | CGI / dynamic server-side rendering              |
| Next.js SSG               | Ahead-of-time compilation to static binary       |
| API routes                | HTTP handler functions                           |

## The Data Flow Unification

All data in a frontend application flows through this unified model:

```
+-------------------------------------------------------------------+
|                                                                    |
|  SOURCES OF TRUTH                                                  |
|  ================                                                  |
|                                                                    |
|  1. URL (React Router)                                             |
|     The application's "command line arguments"                     |
|     Encodes: current page, filters, pagination                     |
|     Survives: page refresh, sharing, bookmarking                   |
|                                                                    |
|  2. Server (TanStack Query)                                        |
|     The application's "database"                                   |
|     Encodes: business data (users, orders, products)               |
|     Survives: everything (persisted on server)                     |
|     Cached locally with TTL and invalidation                       |
|                                                                    |
|  3. Client Memory (Zustand)                                        |
|     The application's "process memory"                             |
|     Encodes: UI state (theme, sidebar, drafts)                     |
|     Survives: navigation within app                                |
|     Lost on: page refresh (unless persisted to localStorage)       |
|                                                                    |
|  4. Component Local (useState)                                     |
|     The function's "stack variables"                               |
|     Encodes: component-specific state (input value, hover)         |
|     Survives: re-renders of this component                         |
|     Lost on: component unmount                                     |
|                                                                    |
|  SUBSCRIBERS                                                       |
|  ===========                                                       |
|                                                                    |
|  React components subscribe to one or more sources.                |
|  When any source updates, subscribed components re-render.         |
|  The rendering pipeline (VDOM diff -> DOM patch) minimizes work.   |
|                                                                    |
+-------------------------------------------------------------------+
```

## Decision Flowchart: Choosing Your Stack

```
Start here: What are you building?
     |
     +-- Internal tool / dashboard (behind login)?
     |      |
     |      YES --> Vite + React + TanStack Query + Zustand + Tailwind
     |              (SPA, deploy to CDN, simple infrastructure)
     |
     +-- Public-facing with SEO needs?
     |      |
     |      YES --> Next.js (SSR/SSG) + TanStack Query + Tailwind
     |              (Server rendering, deploy to Vercel/Node.js)
     |
     +-- Content-heavy blog / docs site?
     |      |
     |      YES --> Astro (or Next.js SSG)
     |              (Static generation, minimal JS)
     |
     +-- Fullstack TypeScript (own frontend + API)?
            |
            YES --> Next.js (App Router + API routes)
                    or Vite + React + separate backend
```

## Summary: One Sentence Per Tool

```
+-------------------------------------------------------------------+
|                     THE COMPLETE PICTURE                          |
|-------------------------------------------------------------------|
|                                                                   |
|  Vite            Your compiler toolchain — transforms source      |
|                  into optimized browser-ready bundles.            |
|                                                                   |
|  TypeScript      Your static analyzer — catches type errors       |
|                  at compile time, erased before runtime.          |
|                                                                   |
|  React           Your UI engine — declaratively describes UI      |
|                  as functions of state, efficiently patches DOM.  |
|                                                                   |
|  Tailwind CSS    Your style generator — scans source for utility  |
|                  classes, emits minimal CSS at build time.        |
|                                                                   |
|  React Router    Your request router — maps URLs to component     |
|                  trees without server round-trips.                |
|                                                                   |
|  TanStack Query  Your caching proxy — manages server data with    |
|                  stale-while-revalidate semantics.                |
|                                                                   |
|  Zustand         Your process memory — holds client-only UI       |
|                  state in a simple, subscribable store.           |
|                                                                   |
|  Next.js         Your application server — adds SSR, SSG, API     |
|                  routes, and deployment orchestration to React.   |
|                                                                   |
+-------------------------------------------------------------------+

Together, they form a complete platform for building modern
web applications — from source code to deployed product.
```

---

## Further Reading

For systems engineers going deeper:

| Topic                     | Resource                                         |
|---------------------------|--------------------------------------------------|
| React internals           | [React Source Code Overview](https://github.com/facebook/react) |
| Vite architecture         | [Vite docs - Why Vite](https://vitejs.dev/guide/why.html) |
| TanStack Query concepts   | [TanStack Query docs](https://tanstack.com/query/latest) |
| TypeScript handbook        | [TypeScript docs](https://www.typescriptlang.org/docs/) |
| Next.js architecture       | [Next.js docs](https://nextjs.org/docs) |
| Browser rendering pipeline | [How browsers work (web.dev)](https://web.dev/howbrowserswork/) |
| Event loop deep dive       | [Jake Archibald: In The Loop](https://www.youtube.com/watch?v=cCOL7MC4Pl0) |

---

*[Part 1 — Foundations](frontend_architecture_part1_foundations.md) | [Part 2 — Deep Dives](frontend_architecture_part2_deep_dives.md) | Part 3 — Integration (this file)*
