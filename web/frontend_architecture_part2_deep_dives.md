# Part 4 — Technology Deep Dives

*Continued from [Part 1 — Foundations](frontend_architecture_part1_foundations.md)*

Each technology is examined through a systems lens: what problem it solves, how it works internally, minimal code examples, and the engineering tradeoffs.

---

## 4.1 Vite — The Build System

### Why Vite Exists

Webpack dominated frontend build tooling for a decade. It was designed in an era before browsers supported ES modules natively. Webpack's architecture bundles everything into a single graph before serving — even in development.

The problem: as projects grew to thousands of modules, Webpack dev server startup times reached 30-60+ seconds. Every file change triggered a partial re-bundle.

Vite exploits two key platform changes:

1. **Browsers now support native ES modules** — no bundling needed in dev mode
2. **esbuild (written in Go) is 10-100x faster** than JavaScript-based transpilers

### Architecture

```
+-------------------------------------------------------------------+
|                        Vite Architecture                           |
|-------------------------------------------------------------------|
|                                                                    |
|  +---------------------+     +---------------------+              |
|  |    Dev Server        |     |   Prod Builder      |              |
|  |---------------------|     |---------------------|              |
|  | HTTP server          |     | Rollup bundler      |              |
|  | WebSocket (HMR)      |     | Code splitting      |              |
|  | On-demand transforms |     | Tree-shaking        |              |
|  | esbuild transpiler   |     | Minification        |              |
|  | Native ESM serving   |     | Asset optimization  |              |
|  +---------------------+     +---------------------+              |
|           |                            |                           |
|           v                            v                           |
|  +---------------------+     +---------------------+              |
|  | Dependency           |     | Output               |             |
|  | Pre-bundler          |     |                     |              |
|  |---------------------|     | dist/                |              |
|  | esbuild             |     |   index.html        |              |
|  | CJS -> ESM          |     |   assets/            |              |
|  | Flatten deep imports|     |     app.a1b2c3.js   |              |
|  +---------------------+     |     style.d4e5f6.css|              |
|                               +---------------------+              |
|                                                                    |
|  +---------------------+                                           |
|  | Plugin System        |                                          |
|  |---------------------|                                           |
|  | Rollup-compatible    |                                          |
|  | Transform hooks      |                                          |
|  | Virtual modules      |                                          |
|  +---------------------+                                           |
|                                                                    |
+-------------------------------------------------------------------+
```

### Minimal Configuration

```typescript
// vite.config.ts
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    proxy: {
      '/api': 'http://localhost:8080',
    },
  },
  build: {
    target: 'es2020',
    sourcemap: true,
  },
})
```

### Why Vite Replaced Webpack

| Dimension        | Webpack                    | Vite                        |
|------------------|----------------------------|-----------------------------|
| Dev startup      | Bundle everything first    | Serve on demand             |
| Transpiler       | babel (JS, slow)           | esbuild (Go, fast)         |
| Dev architecture | Full bundle graph          | Native ES modules          |
| HMR speed        | Slower as project grows    | Constant time (module-level)|
| Prod bundler     | webpack                    | Rollup                     |
| Config           | Complex, verbose           | Minimal, convention-based  |
| Cold start       | 10-60s for large projects  | <1s typically              |

### Tradeoffs

**Pros**:
- Orders of magnitude faster dev experience
- Rollup's mature plugin ecosystem for production
- Clean, minimal configuration

**Cons**:
- Dev/prod use different bundlers (esbuild vs Rollup) — rare behavior differences
- Less battle-tested than Webpack for extreme edge cases
- Plugin system slightly different between dev and prod

### Alternatives

| Tool       | Architecture                              | When to Use                        |
|------------|-------------------------------------------|------------------------------------|
| Webpack    | Bundles everything, mature ecosystem      | Legacy projects, complex configs   |
| Turbopack  | Rust-based, incremental computation       | Next.js projects (Vercel stack)    |
| esbuild    | Go-based, extremely fast, low-level       | When you need raw speed, less config |
| Parcel     | Zero-config, automatic transforms         | Simpler projects, rapid prototyping |

---

## 4.2 React — The UI Engine

### The Core Problem

Manually manipulating the DOM is like writing raw assembly — powerful but error-prone and unscalable. Consider updating a user list:

```javascript
// Without React: imperative DOM manipulation
// Like writing to raw framebuffer memory
const list = document.getElementById('user-list');
list.innerHTML = '';
users.forEach(user => {
  const li = document.createElement('li');
  li.textContent = user.name;
  li.onclick = () => selectUser(user.id);
  list.appendChild(li);
});
```

This doesn't scale. Every state change requires you to manually compute DOM diffs. React inverts this — you declare what the UI should look like, and React computes the mutations.

### The Component Model

A React component is a **pure function** from state to UI:

```
Component: (Props, State) -> Virtual DOM Tree
```

This is analogous to a render function in a game engine: given the current world state, produce the frame. React calls your component function, gets back a tree description, diffs it, and patches the DOM.

```typescript
// A function component — a pure function from props to UI
interface UserCardProps {
  name: string;
  email: string;
  role: 'admin' | 'user';
  onPromote: () => void;
}

function UserCard({ name, email, role, onPromote }: UserCardProps) {
  // This function is called on every render.
  // It returns a DESCRIPTION of the UI (Virtual DOM), not actual DOM nodes.
  return (
    <div className="rounded-lg border p-4 shadow-sm">
      <h3 className="text-lg font-semibold">{name}</h3>
      <p className="text-gray-600">{email}</p>
      <span className="text-sm">{role}</span>
      {role === 'user' && (
        <button onClick={onPromote} className="mt-2 rounded bg-blue-500 px-3 py-1 text-white">
          Promote to Admin
        </button>
      )}
    </div>
  );
}
```

### Virtual DOM and Reconciliation

```
          Component Function Call                  Real DOM
                    |                                 |
                    v                                 |
          +-------------------+                      |
          | Virtual DOM Tree  |                      |
          | (JS object tree)  |                      |
          +-------------------+                      |
                    |                                 |
                    v                                 |
          +-------------------+                      |
          | Reconciler (diff) |                      |
          | Old VDOM vs New   |                      |
          +-------------------+                      |
                    |                                 |
                    v                                 |
          +-------------------+                      |
          | Minimal Patch Set |  ---- apply ----->   |
          | (mutations only)  |                      |
          +-------------------+                      |
```

The Virtual DOM is a plain JavaScript object tree:

```javascript
// JSX:     <div className="card"><h3>Alice</h3></div>
// Becomes:
{
  type: 'div',
  props: {
    className: 'card',
    children: {
      type: 'h3',
      props: { children: 'Alice' }
    }
  }
}
```

React's reconciliation algorithm uses heuristics to achieve O(n) diffing (a general tree diff is O(n^3)):

1. **Different types** → destroy and rebuild (like realloc vs. in-place mutation)
2. **Same type** → update props, recurse into children
3. **Lists with keys** → reorder, insert, delete (like a stable sort with identity)

### State and Hooks

React hooks are the API for component-local state and side effects:

```typescript
import { useState, useEffect, useCallback } from 'react';

function UserList() {
  // useState: allocate a state slot (like a local variable that persists across renders)
  const [filter, setFilter] = useState('');
  const [page, setPage] = useState(1);

  // useEffect: register a side effect (like a deferred callback / cleanup handler)
  useEffect(() => {
    document.title = `Users - Page ${page}`;

    // Cleanup function — runs before next effect or unmount
    // Analogous to a destructor or defer statement
    return () => {
      document.title = 'App';
    };
  }, [page]); // Dependency array — only re-run when `page` changes

  // useCallback: memoize a function reference (avoid unnecessary re-renders of children)
  const handleFilter = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    setFilter(e.target.value);
    setPage(1);
  }, []);

  return (
    <div>
      <input value={filter} onChange={handleFilter} placeholder="Filter users..." />
      <UserTable filter={filter} page={page} />
      <button onClick={() => setPage(p => p + 1)}>Next Page</button>
    </div>
  );
}
```

**Mental model for hooks**: Think of hooks as **registers in a virtual machine**. Each `useState` allocates a register. React maintains a linked list of hook values per component instance, indexed by call order. This is why hooks must be called in the same order every render (no conditional hooks) — it's a positional addressing scheme.

### Tradeoffs

**Pros**:
- Declarative — describe the "what", not the "how"
- Composable component model scales well
- Massive ecosystem and community
- Concurrent rendering (React 18+) enables priority scheduling

**Cons**:
- Virtual DOM diffing has overhead vs direct DOM manipulation
- Re-render cascades can cause performance issues without memoization
- Hooks model has a learning curve (stale closures, dependency arrays)
- Large bundle size compared to alternatives (~40KB minified+gzipped)

### Alternatives

| Library   | Architecture                | Bundle Size | When to Use                       |
|-----------|-----------------------------|-------------|-----------------------------------|
| Vue       | Reactive proxy system       | ~33KB       | Prefer reactivity over VDOM       |
| Svelte    | Compile-time reactivity     | ~2KB        | Minimal runtime, max performance  |
| SolidJS   | Fine-grained reactivity     | ~7KB        | React-like API, no VDOM overhead  |
| Preact    | React API, smaller runtime  | ~3KB        | Drop-in React replacement, smaller|

---

## 4.3 TypeScript — The Type System

### Why TypeScript Exists

JavaScript has no static type system. In a large codebase, this leads to the same class of bugs that C programmers experience without a linter — type mismatches, undefined field access, wrong argument counts.

TypeScript adds a **compile-time type layer** that is completely erased before execution. This is architecturally identical to how Rust's lifetimes exist only at compile time.

```
+-------------------+     +-------------------+     +-------------------+
|   TypeScript      | --> | Type Checker      | --> |   JavaScript      |
|   Source (.tsx)    |     | (tsc / esbuild)   |     |   Output (.js)    |
|                   |     |                   |     |                   |
| Types, interfaces |     | Validates types   |     | All types erased  |
| Generic params    |     | Reports errors    |     | Runtime identical |
| Enums, unions     |     | at compile time   |     | to hand-written JS|
+-------------------+     +-------------------+     +-------------------+
```

### Typed React Components

```typescript
// Define the shape of data — like a C struct or protobuf message
interface User {
  id: number;
  name: string;
  email: string;
  role: 'admin' | 'editor' | 'viewer';  // Discriminated union (like an enum)
  lastLogin: Date;
}

// Props interface — defines the component's public API
// Analogous to a function signature in a header file
interface UserProfileProps {
  user: User;
  onRoleChange: (userId: number, newRole: User['role']) => void;
  isEditable?: boolean;  // Optional prop (like a nullable parameter)
}

function UserProfile({ user, onRoleChange, isEditable = false }: UserProfileProps) {
  return (
    <div>
      <h2>{user.name}</h2>
      <p>{user.email}</p>
      {isEditable && (
        <select
          value={user.role}
          onChange={(e) => onRoleChange(user.id, e.target.value as User['role'])}
        >
          <option value="viewer">Viewer</option>
          <option value="editor">Editor</option>
          <option value="admin">Admin</option>
        </select>
      )}
    </div>
  );
}
```

### Generics — Parameterized Types

```typescript
// Generic data fetching hook — like a C++ template or Rust generic
// T is the type parameter, constrained to be at minimum an object with an `id`
function useApiData<T extends { id: number }>(
  endpoint: string,
  transform?: (raw: unknown) => T[]
): {
  data: T[];
  loading: boolean;
  error: Error | null;
} {
  const [data, setData] = useState<T[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<Error | null>(null);

  useEffect(() => {
    fetch(endpoint)
      .then(res => res.json())
      .then(raw => {
        const result = transform ? transform(raw) : raw as T[];
        setData(result);
      })
      .catch(setError)
      .finally(() => setLoading(false));
  }, [endpoint, transform]);

  return { data, loading, error };
}

// Usage — TypeScript infers the return type
const { data: users } = useApiData<User>('/api/users');
// `users` is typed as User[] — full autocomplete and type safety
```

### Discriminated Unions — Type-Safe State Machines

This is TypeScript's most powerful pattern for systems engineers. It maps directly to tagged unions / algebraic data types:

```typescript
// Like a Rust enum with associated data
type RequestState<T> =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'success'; data: T }
  | { status: 'error'; error: Error };

function renderState<T>(state: RequestState<T>, renderData: (data: T) => JSX.Element) {
  switch (state.status) {
    case 'idle':
      return <p>Ready to fetch</p>;
    case 'loading':
      return <p>Loading...</p>;
    case 'success':
      // TypeScript narrows: `state.data` is available here
      return renderData(state.data);
    case 'error':
      // TypeScript narrows: `state.error` is available here
      return <p>Error: {state.error.message}</p>;
  }
}
```

### Tradeoffs

**Pros**:
- Catches type errors at compile time (prevents entire categories of runtime bugs)
- Excellent IDE support (autocomplete, refactoring, go-to-definition)
- Gradual adoption — you can mix `.js` and `.ts` files
- Zero runtime cost — all types are erased

**Cons**:
- Build step required (though esbuild makes this near-instant)
- Some type gymnastics for complex generic patterns
- `any` type is an escape hatch that can undermine safety
- Third-party library type definitions may lag behind

---

## 4.4 Tailwind CSS — The Styling Engine

### The Problem with Traditional CSS

CSS has a global namespace. Every class name exists in one flat scope. In a large application, this creates the same problems as global variables in C:

```
+----------------------------------+----------------------------------+
|     Traditional CSS              |     Tailwind CSS                 |
|----------------------------------|----------------------------------|
|                                  |                                  |
|  Global namespace                |  Utility-first, scoped by usage  |
|  Specificity wars                |  No specificity conflicts        |
|  Dead CSS accumulates            |  Only used classes in output     |
|  Hard to refactor safely         |  Delete component = delete styles|
|  .btn-primary { ... }            |  className="bg-blue-500 px-4"    |
|  .card-header { ... }            |                                  |
|  .card-header .btn { ... }       |  Build-time scanning:            |
|                                  |  Source -> extract classes ->     |
|  Runtime: browser parses         |    generate minimal CSS          |
|  entire CSS file                 |                                  |
|                                  |                                  |
+----------------------------------+----------------------------------+
```

### How Tailwind Works

Tailwind is a **build-time code generator**, not a runtime library:

```
Build Time:

  Source Files (.tsx, .html)
        |
        v
  +---------------------+
  | Tailwind Scanner     |
  |---------------------|
  | Regex-based scan of  |
  | all source files     |
  | Extract class names: |
  |   "flex", "p-4",     |
  |   "bg-blue-500",     |
  |   "hover:bg-blue-600"|
  +----------+----------+
             |
             v
  +---------------------+
  | CSS Generator        |
  |---------------------|
  | For each used class, |
  | emit the CSS rule:   |
  |                      |
  | .flex {              |
  |   display: flex;     |
  | }                    |
  | .p-4 {               |
  |   padding: 1rem;     |
  | }                    |
  | .bg-blue-500 {       |
  |   background-color:  |
  |     #3b82f6;         |
  | }                    |
  +----------+----------+
             |
             v
  output.css (only used styles, typically 5-15KB)
```

**Backend analogy**: Tailwind is like Protocol Buffers or code generation from a schema. You write declarations in your source (class names), and the build tool generates the minimal output (CSS rules). Unused utilities are never emitted — dead code elimination at the CSS level.

### Component with Tailwind

```typescript
interface AlertProps {
  type: 'info' | 'warning' | 'error';
  title: string;
  message: string;
  onDismiss: () => void;
}

const alertStyles = {
  info: 'border-blue-200 bg-blue-50 text-blue-800',
  warning: 'border-yellow-200 bg-yellow-50 text-yellow-800',
  error: 'border-red-200 bg-red-50 text-red-800',
} as const;

function Alert({ type, title, message, onDismiss }: AlertProps) {
  return (
    <div className={`flex items-start gap-3 rounded-lg border p-4 ${alertStyles[type]}`}>
      <div className="flex-1">
        <h4 className="font-semibold">{title}</h4>
        <p className="mt-1 text-sm opacity-80">{message}</p>
      </div>
      <button
        onClick={onDismiss}
        className="rounded p-1 opacity-60 transition-opacity hover:opacity-100"
      >
        ✕
      </button>
    </div>
  );
}
```

### Tailwind's Design System

Tailwind enforces a constrained design system through its spacing/color/typography scales:

```
Spacing scale (rem-based, like a type-safe enum):
  p-0   -> 0rem
  p-1   -> 0.25rem
  p-2   -> 0.5rem
  p-3   -> 0.75rem
  p-4   -> 1rem       (16px at default font size)
  p-6   -> 1.5rem
  p-8   -> 2rem

Color scale (design tokens):
  blue-50   -> #eff6ff   (lightest)
  blue-100  -> #dbeafe
  blue-500  -> #3b82f6   (primary)
  blue-900  -> #1e3a8a   (darkest)

Responsive breakpoints (mobile-first):
  sm:   -> @media (min-width: 640px)
  md:   -> @media (min-width: 768px)
  lg:   -> @media (min-width: 1024px)
  xl:   -> @media (min-width: 1280px)
```

### Tradeoffs

**Pros**:
- No dead CSS — output contains only what's used
- No naming collisions — no global CSS class names to manage
- Consistent design system enforced by constrained utility scales
- Fast iteration — style directly in markup, no file switching

**Cons**:
- Long class strings can reduce readability
- Learning curve for utility names
- Dynamic styles require workarounds (can't do `bg-${color}-500`)
- Harder to share styled components across projects without a component library

### Alternatives

| Approach       | Architecture                         | When to Use                       |
|----------------|--------------------------------------|-----------------------------------|
| CSS Modules    | Scoped CSS files, compiled to unique class names | Team prefers traditional CSS syntax |
| styled-components | CSS-in-JS, runtime style injection | Dynamic styles, theme-driven design |
| Vanilla Extract | Zero-runtime CSS-in-TypeScript     | Type-safe styles, no runtime cost |
| Plain CSS       | Global stylesheets                  | Small projects, simple needs      |

---

## 4.5 React Router — Client-Side Navigation

### The Core Problem

In a traditional server-rendered app, every navigation is a full HTTP request → server renders HTML → browser reloads. This is like calling `fork/exec` for every operation instead of using function calls within a process.

Client-side routing keeps the application loaded and swaps UI sections based on the URL:

```
Server-Side Navigation:                Client-Side Navigation:

  Browser                Server          Browser (SPA)
    |                      |               |
    | GET /users           |               | Click "Users" link
    |--------------------->|               |   |
    |                      |               |   v
    |  Full HTML response  |               | Router intercepts
    |<---------------------|               | pushState('/users')
    |                      |               | Render <UsersPage>
    | Full page reload     |               | (no server request
    | All JS re-parsed     |               |  for the page itself)
    | All state lost       |               | State preserved!
    |                      |               |
```

### Route Tree Architecture

React Router maps URL patterns to component trees:

```typescript
import { createBrowserRouter, RouterProvider } from 'react-router-dom';

const router = createBrowserRouter([
  {
    path: '/',
    element: <RootLayout />,  // Wraps all routes (shell / chrome)
    errorElement: <ErrorPage />,
    children: [
      { index: true, element: <HomePage /> },
      {
        path: 'dashboard',
        element: <DashboardLayout />,  // Nested layout
        children: [
          { index: true, element: <DashboardOverview /> },
          { path: 'users', element: <UsersPage /> },
          { path: 'users/:userId', element: <UserDetail /> },  // Dynamic segment
          { path: 'settings', element: <SettingsPage /> },
        ],
      },
      { path: 'login', element: <LoginPage /> },
      { path: '*', element: <NotFound /> },  // Catch-all (like default case)
    ],
  },
]);

function App() {
  return <RouterProvider router={router} />;
}
```

### Route Matching Diagram

```
URL: /dashboard/users/42

Route Tree Matching:
  /                     -> RootLayout        (matched)
    /dashboard          -> DashboardLayout   (matched)
      /users/:userId    -> UserDetail        (matched, userId=42)

Rendered Component Tree:
  <RootLayout>
    <Header />
    <DashboardLayout>
      <Sidebar />
      <UserDetail userId="42" />    <-- Leaf component renders here
    </DashboardLayout>
    <Footer />
  </RootLayout>
```

**Backend analogy**: This is exactly like nested HTTP middleware in Go/Express. Each layout component is a middleware that wraps inner routes, providing shared UI (headers, sidebars) while delegating the content area to child routes.

### Navigation Patterns

```typescript
import { useNavigate, useParams, useSearchParams } from 'react-router-dom';

function UserDetail() {
  // Extract dynamic route parameters — like path params in an HTTP handler
  const { userId } = useParams<{ userId: string }>();

  // Programmatic navigation — like an HTTP redirect
  const navigate = useNavigate();

  // Query string management — like URL query parameters
  const [searchParams, setSearchParams] = useSearchParams();
  const tab = searchParams.get('tab') ?? 'profile';

  const handleDelete = async () => {
    await deleteUser(userId!);
    navigate('/dashboard/users', { replace: true });
  };

  return (
    <div>
      <h1>User {userId}</h1>
      <nav>
        <button onClick={() => setSearchParams({ tab: 'profile' })}>Profile</button>
        <button onClick={() => setSearchParams({ tab: 'activity' })}>Activity</button>
      </nav>
      {tab === 'profile' && <ProfileTab userId={userId!} />}
      {tab === 'activity' && <ActivityTab userId={userId!} />}
    </div>
  );
}
```

### Tradeoffs

**Pros**:
- Instant navigation (no full page reloads)
- Preserves application state across navigation
- Nested layouts compose naturally
- Deep linking works — URLs are bookmarkable

**Cons**:
- Initial load is heavier (must load routing + component code upfront or lazy-load)
- SEO requires additional work (SSR or pre-rendering) for public-facing pages
- Browser history management can get complex
- Code splitting needed to avoid loading all routes upfront

---

## 4.6 TanStack Query — Server State Management

### The Two-State Problem

Frontend applications manage two fundamentally different kinds of state:

```
+-----------------------------------------------+-----------------------------------------------+
|              Client State                      |              Server State                      |
|-----------------------------------------------|-----------------------------------------------|
|  Origin: created in the browser                |  Origin: owned by the backend / database       |
|  Examples: sidebar open, theme, form input     |  Examples: user list, order data, settings     |
|  Synchronous — always available                |  Asynchronous — requires network fetch         |
|  Single source of truth (this client)          |  Shared — other clients may mutate it          |
|  No staleness problem                          |  Can become stale at any moment                |
|  Simple: just variables                        |  Complex: loading, error, caching, refetching  |
|                                                |                                                |
|  Tool: Zustand / Redux / useState              |  Tool: TanStack Query                          |
+-----------------------------------------------+-----------------------------------------------+
```

Before TanStack Query, developers crammed server state into Redux, writing enormous amounts of boilerplate for loading states, error handling, caching, and synchronization. TanStack Query extracts this into a dedicated subsystem.

### Architecture

```
+-------------------------------------------------------------------+
|                   TanStack Query Architecture                      |
|-------------------------------------------------------------------|
|                                                                    |
|  Component                  Query Client                           |
|  +-----------+             +--------------------------------+      |
|  | useQuery  | -------->   | Query Cache                    |      |
|  | ('users') |             |                                |      |
|  +-----------+             |  Key: ['users', { page: 1 }]  |      |
|       ^                    |  Data: User[]                  |      |
|       |                    |  Status: fresh / stale         |      |
|       |                    |  UpdatedAt: 1710000000000      |      |
|       |                    |  StaleTime: 30000ms            |      |
|       |                    |                                |      |
|       | Subscribe          |  Key: ['user', 42]            |      |
|       | (re-render on      |  Data: User                   |      |
|       |  cache update)     |  Status: stale                |      |
|       |                    |  (background refetching...)    |      |
|       |                    +----------------+---------------+      |
|       |                                     |                      |
|       +--------- notify -------------------+                      |
|                                             |                      |
|                                        fetch()                     |
|                                             |                      |
|                                             v                      |
|                                    +------------------+            |
|                                    |   Backend API    |            |
|                                    +------------------+            |
|                                                                    |
+-------------------------------------------------------------------+
```

### Core API

```typescript
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';

interface User {
  id: number;
  name: string;
  email: string;
}

// Fetching data — declarative data subscription
function UserList() {
  const {
    data: users,
    isLoading,
    isError,
    error,
  } = useQuery<User[]>({
    queryKey: ['users'],           // Cache key (like a cache bucket identifier)
    queryFn: () =>                 // Fetch function (the actual HTTP call)
      fetch('/api/users').then(r => r.json()),
    staleTime: 30_000,             // Data is fresh for 30s (no refetch)
    gcTime: 5 * 60_000,           // Garbage collect after 5min unused
    refetchOnWindowFocus: true,    // Refetch when user returns to tab
  });

  if (isLoading) return <Spinner />;
  if (isError) return <ErrorDisplay error={error} />;

  return (
    <ul>
      {users!.map(user => (
        <li key={user.id}>{user.name}</li>
      ))}
    </ul>
  );
}

// Mutating data — imperative operations with cache invalidation
function CreateUserForm() {
  const queryClient = useQueryClient();

  const mutation = useMutation({
    mutationFn: (newUser: Omit<User, 'id'>) =>
      fetch('/api/users', {
        method: 'POST',
        body: JSON.stringify(newUser),
        headers: { 'Content-Type': 'application/json' },
      }).then(r => r.json()),

    onSuccess: () => {
      // Invalidate the cache — triggers refetch of user list
      // Like sending a cache invalidation message in a distributed system
      queryClient.invalidateQueries({ queryKey: ['users'] });
    },
  });

  return (
    <form onSubmit={(e) => {
      e.preventDefault();
      const formData = new FormData(e.currentTarget);
      mutation.mutate({
        name: formData.get('name') as string,
        email: formData.get('email') as string,
      });
    }}>
      <input name="name" required />
      <input name="email" type="email" required />
      <button type="submit" disabled={mutation.isPending}>
        {mutation.isPending ? 'Creating...' : 'Create User'}
      </button>
    </form>
  );
}
```

### Caching Strategy

```
Time -->
0s        5s        30s       60s       300s
|---------|---------|---------|---------|---------|

[  FRESH  ]                                        staleTime = 30s
           [       STALE (serve from cache,       ]
           [        background refetch on         ]
           [        next access)                  ]
                                         [  GC   ] gcTime = 300s
                                         (remove
                                          from
                                          cache)

Request timeline:

  t=0s:   Component mounts -> fetch from API -> cache result
  t=5s:   Same component re-renders -> serve from cache (FRESH, no fetch)
  t=35s:  Component re-renders -> serve stale data immediately
          -> trigger background refetch -> update cache -> re-render with fresh data
  t=305s: No components using this key -> garbage collect cache entry
```

**Backend analogy**: This is **stale-while-revalidate** caching, the same strategy used by CDNs and HTTP caches. The user sees data instantly (even if stale), while a background process fetches the latest version.

### Tradeoffs

**Pros**:
- Eliminates boilerplate for loading/error/caching states
- Automatic background refetching keeps data fresh
- Deduplication — multiple components using the same query share one request
- Optimistic updates for responsive UIs
- DevTools for inspecting cache state

**Cons**:
- Learning curve for cache invalidation strategies
- Overfetching risk if staleTime is too aggressive
- Not suitable for real-time data (use WebSockets instead)
- Query key management requires discipline in large apps

### Alternatives

| Tool           | Architecture                    | When to Use                       |
|----------------|---------------------------------|-----------------------------------|
| SWR            | Similar stale-while-revalidate  | Simpler API, lighter weight       |
| Apollo Client  | GraphQL-native cache            | GraphQL APIs with normalized cache|
| RTK Query      | Redux-integrated data fetching  | Already using Redux Toolkit       |
| Manual fetch   | useState + useEffect            | One-off fetches, simple needs     |

---

## 4.7 Zustand — Client State Management

### Why Dedicated Client State?

React's built-in `useState` is component-scoped. When multiple components need to share state, you face the "prop drilling" problem — passing state through many layers of components that don't use it, just to reach a descendant.

```
Prop Drilling Problem:

  <App theme="dark">                      |  state defined here
    <Layout theme="dark">                 |  passed through (doesn't use it)
      <Sidebar theme="dark">              |  passed through (doesn't use it)
        <SidebarItem theme="dark">        |  passed through (doesn't use it)
          <Icon theme="dark" />           v  finally used here
```

Zustand provides a global store that any component can subscribe to directly:

```
Zustand Store:

  Store { theme: 'dark', sidebar: 'open' }
       |              |
       v              v
  <Icon />      <Sidebar />         (subscribe directly, no prop drilling)
```

### Architecture

```
+-------------------------------------------------------------------+
|                     Zustand Architecture                           |
|-------------------------------------------------------------------|
|                                                                    |
|  +--------------------+                                            |
|  | Store Definition   |                                            |
|  |--------------------|                                            |
|  | State: { ... }     |                                            |
|  | Actions: { ... }   |  <-- Single object, immutable updates      |
|  +--------+-----------+                                            |
|           |                                                        |
|           v                                                        |
|  +--------------------+                                            |
|  | Internal Store     |                                            |
|  |--------------------|                                            |
|  | state: current     |                                            |
|  | listeners: Set()   |  <-- Components subscribe via selectors    |
|  | setState()         |  <-- Triggers re-render for subscribers    |
|  | getState()         |  <-- Sync read (no re-render)              |
|  +--------+-----------+                                            |
|           |                                                        |
|    subscribe / notify                                              |
|           |                                                        |
|    +------+------+------+                                          |
|    v      v      v      v                                          |
|  Comp_A Comp_B Comp_C  Comp_D  (only re-render if selected        |
|                                  slice changed)                    |
|                                                                    |
+-------------------------------------------------------------------+
```

### Code Example

```typescript
import { create } from 'zustand';

// Define the store — state + actions in one object
// Analogous to defining a state machine with transition functions
interface AppStore {
  // State
  theme: 'light' | 'dark';
  sidebarOpen: boolean;
  notifications: Notification[];

  // Actions (state transitions)
  toggleTheme: () => void;
  toggleSidebar: () => void;
  addNotification: (n: Notification) => void;
  dismissNotification: (id: string) => void;
}

interface Notification {
  id: string;
  message: string;
  type: 'info' | 'warning' | 'error';
}

const useAppStore = create<AppStore>((set) => ({
  theme: 'light',
  sidebarOpen: true,
  notifications: [],

  toggleTheme: () =>
    set((state) => ({
      theme: state.theme === 'light' ? 'dark' : 'light',
    })),

  toggleSidebar: () =>
    set((state) => ({ sidebarOpen: !state.sidebarOpen })),

  addNotification: (n) =>
    set((state) => ({
      notifications: [...state.notifications, n],
    })),

  dismissNotification: (id) =>
    set((state) => ({
      notifications: state.notifications.filter((n) => n.id !== id),
    })),
}));

// Usage in components — subscribe to specific slices
function ThemeToggle() {
  // Only re-renders when `theme` changes (selector-based subscription)
  const theme = useAppStore((s) => s.theme);
  const toggleTheme = useAppStore((s) => s.toggleTheme);

  return (
    <button onClick={toggleTheme}>
      Current: {theme}
    </button>
  );
}

function Sidebar() {
  const isOpen = useAppStore((s) => s.sidebarOpen);
  if (!isOpen) return null;

  return <nav className="w-64 border-r p-4">...</nav>;
}
```

### Zustand vs Redux

```
+------------------------------+------------------------------+
|          Zustand              |          Redux               |
|------------------------------|------------------------------|
|  Minimal API (create + use)  |  Actions, reducers, dispatch |
|  ~1KB bundle size             |  ~2KB + middleware (~5KB+)   |
|  Direct mutation syntax       |  Immutable update patterns   |
|  No boilerplate              |  Significant boilerplate     |
|  No provider/context needed  |  <Provider store={store}>    |
|  Single store or many stores |  Single store (convention)   |
|                              |                              |
|  Best for:                   |  Best for:                   |
|  - Most apps                 |  - Very large teams          |
|  - Simple to medium state    |  - Time-travel debugging     |
|  - Quick setup               |  - Strict action logging     |
+------------------------------+------------------------------+
```

**Backend analogy**: Zustand is like an in-process key-value store (like a `sync.Map` in Go). Redux is like an event-sourced system — every state change is an event (action) that gets logged and replayed through a reducer.

### Alternatives

| Tool    | Architecture                  | Bundle  | When to Use                     |
|---------|-------------------------------|---------|----------------------------------|
| Redux   | Flux/event-sourcing pattern   | ~5KB+   | Large teams, strict conventions  |
| Jotai   | Atomic state (bottom-up)      | ~3KB    | Fine-grained reactive state     |
| Valtio  | Proxy-based reactive state    | ~3KB    | Prefer mutable syntax           |
| Recoil  | Graph-based state (Meta)      | ~20KB   | Complex derived state            |

---

## 4.8 Next.js — The Fullstack Framework

### What Next.js Adds

Everything we've discussed so far is a **client-side SPA** (Single Page Application). Next.js wraps React and adds a server layer:

```
+-------------------------------------------------------------------+
|                        SPA (Vite + React)                          |
|-------------------------------------------------------------------|
|                                                                    |
|  Browser does everything:                                          |
|  1. Loads empty HTML shell                                         |
|  2. Loads JavaScript bundle                                        |
|  3. JS renders the UI (blank screen until JS loads)                |
|  4. JS fetches data from API                                       |
|  5. JS renders data into UI                                        |
|                                                                    |
|  Server only serves static files.                                  |
|                                                                    |
+-------------------------------------------------------------------+

+-------------------------------------------------------------------+
|                     Next.js (Fullstack)                             |
|-------------------------------------------------------------------|
|                                                                    |
|  Server pre-renders pages:                                         |
|  1. Server receives request                                        |
|  2. Server fetches data from database/API                          |
|  3. Server renders React components to HTML                        |
|  4. Server sends complete HTML to browser (instant content)        |
|  5. Browser loads JS and "hydrates" (attaches event handlers)      |
|  6. App becomes interactive                                        |
|                                                                    |
|  Server is an active participant in rendering.                     |
|                                                                    |
+-------------------------------------------------------------------+
```

### Rendering Strategies

Next.js supports multiple rendering strategies — choose per-page:

```
+-------------------------------------------------------------------+
|                  Next.js Rendering Strategies                       |
|-------------------------------------------------------------------|
|                                                                    |
|  1. SSG (Static Site Generation)                                   |
|     Build time: render HTML                                        |
|     Runtime: serve pre-built HTML from CDN                         |
|     Use case: blog posts, docs, marketing pages                    |
|     Analogy: pre-compiled static binary                            |
|                                                                    |
|  2. SSR (Server-Side Rendering)                                    |
|     Every request: server renders HTML                             |
|     Use case: personalized pages, dashboards                       |
|     Analogy: CGI / dynamic server handler                          |
|                                                                    |
|  3. ISR (Incremental Static Regeneration)                          |
|     Serve stale, regenerate in background                          |
|     Use case: e-commerce product pages                             |
|     Analogy: stale-while-revalidate CDN cache                     |
|                                                                    |
|  4. CSR (Client-Side Rendering)                                    |
|     Same as SPA — browser renders everything                       |
|     Use case: authenticated dashboards, internal tools             |
|     Analogy: traditional SPA                                       |
|                                                                    |
|  5. RSC (React Server Components) — Next.js 13+                   |
|     Components execute on the server, send serialized output       |
|     No JS shipped to browser for server components                 |
|     Use case: data-heavy pages, reduce client bundle               |
|     Analogy: server-side templates with client-side islands        |
|                                                                    |
+-------------------------------------------------------------------+
```

### Next.js Architecture (App Router)

```
+-------------------------------------------------------------------+
|                   Next.js App Router Architecture                   |
|-------------------------------------------------------------------|
|                                                                    |
|  app/                                                              |
|    layout.tsx          <-- Root layout (server component)          |
|    page.tsx            <-- Home page  (server component)           |
|    dashboard/                                                      |
|      layout.tsx        <-- Dashboard layout (server component)     |
|      page.tsx          <-- Dashboard page (server component)       |
|      users/                                                        |
|        page.tsx        <-- Users page (server component)           |
|        [id]/                                                       |
|          page.tsx      <-- User detail (server component)          |
|    api/                                                            |
|      users/                                                        |
|        route.ts        <-- API endpoint (like Go HTTP handler)     |
|                                                                    |
+-------------------------------------------------------------------+
|                                                                    |
|  Request flow:                                                     |
|                                                                    |
|  GET /dashboard/users/42                                           |
|       |                                                            |
|       v                                                            |
|  Next.js Server                                                    |
|       |                                                            |
|       +-- Match route: app/dashboard/users/[id]/page.tsx           |
|       |                                                            |
|       +-- Execute server component:                                |
|       |     async function UserPage({ params }) {                  |
|       |       const user = await db.users.find(params.id);         |
|       |       return <UserProfile user={user} />;                  |
|       |     }                                                      |
|       |                                                            |
|       +-- Render to HTML + RSC payload                             |
|       |                                                            |
|       v                                                            |
|  Send to browser:                                                  |
|    - Complete HTML (visible immediately)                           |
|    - RSC payload (for client hydration)                            |
|    - JS bundle (for interactivity)                                 |
|                                                                    |
+-------------------------------------------------------------------+
```

### Minimal Next.js Code

```typescript
// app/dashboard/users/page.tsx — Server Component (runs on server)
// This function executes on the server, not in the browser.
// It can directly access databases, file system, environment variables.
async function UsersPage() {
  const users = await fetch('https://api.example.com/users', {
    next: { revalidate: 60 },  // ISR: revalidate every 60 seconds
  }).then(r => r.json());

  return (
    <div className="p-6">
      <h1 className="text-2xl font-bold">Users</h1>
      <UserTable users={users} />
    </div>
  );
}

// app/api/users/route.ts — API Route (like a Go HTTP handler)
import { NextResponse } from 'next/server';

export async function GET(request: Request) {
  const { searchParams } = new URL(request.url);
  const page = Number(searchParams.get('page') ?? '1');

  const users = await db.users.findMany({
    skip: (page - 1) * 20,
    take: 20,
  });

  return NextResponse.json(users);
}

export async function POST(request: Request) {
  const body = await request.json();
  const user = await db.users.create({ data: body });
  return NextResponse.json(user, { status: 201 });
}
```

### Tradeoffs

**Pros**:
- SEO-friendly (HTML rendered before JS loads)
- Faster initial page load (server-rendered content)
- API routes eliminate need for a separate backend (for simpler apps)
- Automatic code splitting per route
- Server Components reduce client-side JS

**Cons**:
- Requires a Node.js server (not just static hosting)
- More complex deployment (server infrastructure needed)
- Server/client component boundary is a new mental model to learn
- Vendor lock-in to Vercel's ecosystem (though self-hosting works)
- Heavier framework overhead vs. a lean Vite SPA

---

*Continue to [Part 3 — Integration and Mental Models](frontend_architecture_part3_integration.md)*
