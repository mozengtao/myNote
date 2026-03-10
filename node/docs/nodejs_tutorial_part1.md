# Node.js Tutorial — Part 1: Core Concepts

> **Prerequisites**: You know C, general programming, and basic terminal usage.
> **Goal**: Learn the critical 20% of Node.js that covers 80% of practical usage.

---

## Table of Contents — Part 1

1. [Node.js Fundamentals](#1-nodejs-fundamentals)
2. [Global Variables and Objects](#2-global-variables-and-objects)
3. [Modules (Core of Node.js)](#3-modules-core-of-nodejs)
4. [Functions and Asynchronous Programming](#4-functions-and-asynchronous-programming)
5. [Event Loop and Non-Blocking I/O](#5-event-loop-and-non-blocking-io)

---

## 1. Node.js Fundamentals

### What Node.js Is

Node.js is a **runtime environment** that executes JavaScript outside the browser. Think of it the way the C runtime (`libc` + OS syscalls) lets your compiled C binary run — Node.js provides the runtime (V8 engine + libuv) that lets JavaScript files run as standalone programs.

```
  Your C code  →  gcc  →  binary  →  libc + OS     →  runs
  Your JS code →        →  .js    →  V8 + libuv    →  runs via `node`
```

### How It Differs from Browser JavaScript

| Aspect | Browser JS | Node.js |
|---|---|---|
| DOM access | `document`, `window` | **None** — no DOM |
| File system | Forbidden (sandboxed) | Full access via `fs` |
| Network | `fetch`, `XMLHttpRequest` | `http`, `net`, `dgram` modules |
| Global object | `window` | `global` (or `globalThis`) |
| Module system | ES Modules (`<script type="module">`) | CommonJS (`require`) + ES Modules |
| Use case | UI rendering, user interaction | Servers, CLI tools, scripting |

### V8 Engine and Runtime Model

**V8** is Google's open-source JavaScript engine (written in C++). It compiles JavaScript directly to machine code via JIT (Just-In-Time) compilation — no interpreter step.

**libuv** is a C library that provides the event loop, async I/O, thread pool, and cross-platform abstractions. It's the equivalent of your `select()`/`epoll()` event loop in C networking code.

```
┌─────────────────────────────────┐
│         Your JavaScript         │
├─────────────────────────────────┤
│     Node.js Bindings (C++)      │
├──────────────┬──────────────────┤
│   V8 Engine  │     libuv        │
│  (JS → x86)  │ (event loop,     │
│              │  async I/O,      │
│              │  thread pool)    │
├──────────────┴──────────────────┤
│        Operating System         │
└─────────────────────────────────┘
```

### Single-Threaded Event Loop

Node.js runs your JavaScript on **one thread**. There is no `pthread_create()` for your JS code. Instead, I/O operations are offloaded to the OS or a thread pool (managed by libuv), and your code is notified via callbacks when results are ready.

**Mental model from C**:

```c
// This is essentially what Node.js does internally
while (1) {
    events = epoll_wait(epfd, ...);  // block until something happens
    for (each event) {
        callback(event);             // run the JS callback
    }
}
```

Your JS code runs in the callbacks. Between callbacks, Node waits for I/O. This is why Node.js is great for I/O-heavy workloads (web servers, APIs) but poor for CPU-heavy computation (image processing, cryptography) — a long computation blocks the single thread.

### Exercise 1.1

Create a file `hello.js`:

```javascript
console.log("Hello from Node.js!");
console.log("Node version:", process.version);
console.log("Platform:", process.platform);
console.log("Architecture:", process.arch);
```

Run it:

```bash
node hello.js
```

---

## 2. Global Variables and Objects

In C, you have globals like `stdin`, `stdout`, `stderr`, `errno`, `environ`. Node.js has its own set of globally available objects.

### `global`

The top-level object (like `window` in browsers). Anything attached to `global` is accessible everywhere.

```javascript
global.myVar = 42;
console.log(myVar); // 42 — accessible without `global.` prefix
```

In practice, **avoid polluting `global`**. Use modules instead (just like you'd avoid global variables in C).

> **Note**: `globalThis` is the standard cross-environment reference (works in both browsers and Node.js).

### `process`

The `process` object is Node's equivalent of what you'd get from `getpid()`, `getenv()`, `argv`, and `exit()` in C.

```javascript
// PID — like getpid()
console.log("PID:", process.pid);

// Command-line arguments — like argv in C's main(int argc, char *argv[])
// process.argv[0] = path to node binary
// process.argv[1] = path to script
// process.argv[2..] = user arguments
console.log("Arguments:", process.argv);

// Environment variables — like getenv() or environ
console.log("HOME:", process.env.HOME);
console.log("PATH:", process.env.PATH);

// Exit — like exit(code)
// process.exit(0);  // success
// process.exit(1);  // failure

// Current working directory — like getcwd()
console.log("CWD:", process.cwd());

// Memory usage — rough equivalent of reading /proc/self/status
console.log("Memory:", process.memoryUsage());
```

### `__dirname` and `__filename`

These give you the absolute path of the current file — useful for resolving paths relative to the source file rather than the working directory.

```javascript
console.log("This file:", __filename);
console.log("This directory:", __dirname);
```

> **Gotcha**: `__dirname` and `__filename` exist in CommonJS modules but **not** in ES Modules. In ES Modules, use:
> ```javascript
> import { fileURLToPath } from 'url';
> import { dirname } from 'path';
> const __filename = fileURLToPath(import.meta.url);
> const __dirname = dirname(__filename);
> ```

### `console`

Works like `printf` / `fprintf`:

```javascript
console.log("stdout message");       // → stdout (like printf)
console.error("stderr message");     // → stderr (like fprintf(stderr, ...))
console.warn("also stderr");         // → stderr
console.time("timer");               // start a named timer
// ... some work ...
console.timeEnd("timer");            // prints elapsed time

console.table([                      // tabular output
  { name: "Alice", age: 30 },
  { name: "Bob", age: 25 }
]);
```

### Exercise 2.1

Create `globals_demo.js`:

```javascript
// Print all environment variables (like running `env` in bash)
for (const [key, value] of Object.entries(process.env)) {
    console.log(`${key}=${value}`);
}
```

### Exercise 2.2

Create `args_demo.js`:

```javascript
// Mimic a simple C program that processes command-line arguments
const args = process.argv.slice(2); // skip node path and script path

if (args.length === 0) {
    console.error("Usage: node args_demo.js <name> [age]");
    process.exit(1);
}

const name = args[0];
const age = args[1] || "unknown";

console.log(`Name: ${name}`);
console.log(`Age: ${age}`);
console.log(`Script location: ${__filename}`);
console.log(`Script directory: ${__dirname}`);
```

Run it:

```bash
node args_demo.js Alice 30
node args_demo.js           # triggers usage error
```

---

## 3. Modules (Core of Node.js)

Modules are Node's answer to C's `#include` and separate compilation. Each `.js` file is a module with its own scope — variables don't leak between files (unlike C's translation units without `static`).

### CommonJS (CJS) — The Original System

CommonJS is the default module system in Node.js. Every file gets wrapped in a function:

```javascript
// What Node.js actually does with your file:
(function(exports, require, module, __filename, __dirname) {
    // YOUR CODE HERE
});
```

This is why `__filename`, `__dirname`, `require`, and `module` are available — they're function parameters, not true globals.

#### Exporting

```javascript
// math.js — a custom module

function add(a, b) {
    return a + b;
}

function multiply(a, b) {
    return a * b;
}

// Export specific functions (like making them non-static in C)
module.exports = { add, multiply };

// Alternative: export individually
// exports.add = add;
// exports.multiply = multiply;
```

#### Importing with `require()`

```javascript
// app.js
const math = require('./math');     // ./ means relative path
// OR destructure:
const { add, multiply } = require('./math');

console.log(add(2, 3));        // 5
console.log(multiply(4, 5));   // 20
```

**Key behaviors of `require()`:**

| Behavior | Detail |
|---|---|
| Caching | A module is loaded **once** and cached. Subsequent `require()` calls return the cached object. Like a singleton. |
| Synchronous | `require()` blocks until the file is read and executed. |
| Resolution | `'./foo'` → relative file. `'fs'` → built-in. `'express'` → `node_modules/`. |
| Extensions | Tries `.js`, `.json`, `.node` (C++ addon) in order. |

#### `module.exports` vs `exports`

```javascript
// exports is a shorthand reference to module.exports
// This works:
exports.foo = 42;

// This BREAKS — you're reassigning the local variable, not module.exports:
exports = { foo: 42 };  // ❌ doesn't work

// Always use module.exports for assigning a whole new object:
module.exports = { foo: 42 };  // ✅ works
```

**C analogy**: Think of `module.exports` as the "public header" of your module. Only what you export is visible to importers.

### ES Modules (ESM) — The Modern Standard

ES Modules use `import`/`export` syntax. To use ESM in Node.js, either:
- Name your file `.mjs`, or
- Add `"type": "module"` to your `package.json`

#### Named exports

```javascript
// math.mjs
export function add(a, b) {
    return a + b;
}

export function multiply(a, b) {
    return a * b;
}
```

#### Default export

```javascript
// logger.mjs
export default function log(msg) {
    console.log(`[LOG] ${msg}`);
}
```

#### Importing

```javascript
// app.mjs
import { add, multiply } from './math.mjs';
import log from './logger.mjs';

log(`2 + 3 = ${add(2, 3)}`);
```

### CJS vs ESM — Quick Comparison

| Feature | CommonJS | ES Modules |
|---|---|---|
| Syntax | `require()` / `module.exports` | `import` / `export` |
| Loading | Synchronous | Asynchronous |
| Top-level await | No | Yes |
| File extension | `.js` (default) | `.mjs` or `"type": "module"` |
| `this` at top level | `exports` object | `undefined` |
| Interop | Can `require()` JSON directly | Needs `assert { type: 'json' }` |

**Recommendation**: New projects should prefer ESM. Legacy codebases use CJS.

### Built-in Modules

Node.js ships with dozens of modules. The most important ones:

#### `fs` — File System

```javascript
const fs = require('fs');

// Read a file synchronously (blocks — like fread in C)
const data = fs.readFileSync('hello.txt', 'utf8');
console.log(data);
```

#### `path` — Path Manipulation

```javascript
const path = require('path');

// Join path segments (handles OS-specific separators)
const full = path.join(__dirname, 'data', 'config.json');
console.log(full);

console.log(path.basename('/home/user/file.txt'));  // 'file.txt'
console.log(path.extname('file.txt'));              // '.txt'
console.log(path.resolve('..', 'other'));           // absolute path
```

#### `http` — HTTP Server/Client

```javascript
const http = require('http');

const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('Hello from Node.js\n');
});

server.listen(3000, () => {
    console.log('Server running on http://localhost:3000');
});
```

### Exercise 3.1 — Create a Custom Module

Create `utils.js`:

```javascript
function capitalize(str) {
    return str.charAt(0).toUpperCase() + str.slice(1);
}

function repeat(str, n) {
    return str.repeat(n);
}

module.exports = { capitalize, repeat };
```

Create `main.js`:

```javascript
const { capitalize, repeat } = require('./utils');
const path = require('path');

const name = "node.js";
console.log(capitalize(name));          // "Node.js"
console.log(repeat("ha", 3));           // "hahaha"
console.log(path.resolve('.'));         // current absolute path
```

### Exercise 3.2 — ES Module Version

Create `utils.mjs`:

```javascript
export function capitalize(str) {
    return str.charAt(0).toUpperCase() + str.slice(1);
}

export function repeat(str, n) {
    return str.repeat(n);
}
```

Create `main.mjs`:

```javascript
import { capitalize, repeat } from './utils.mjs';

console.log(capitalize("hello"));
console.log(repeat("🔥", 5));
```

Run with: `node main.mjs`

---

## 4. Functions and Asynchronous Programming

This is the **most important section**. Node.js is fundamentally asynchronous. In C, you call `read()` and it blocks until data arrives. In Node.js, you initiate a read and provide a function to call when it's done.

### Callbacks — The Original Pattern

A callback is a function passed as an argument, called when the operation completes. Node.js follows the **error-first callback convention**: the first argument is always an error (or `null` on success).

```javascript
const fs = require('fs');

// C equivalent thinking:
// Instead of: char *data = read(fd, buf, n);  // blocks
// You do:     read_async(fd, callback);        // returns immediately

fs.readFile('hello.txt', 'utf8', function(err, data) {
    if (err) {
        console.error("Read failed:", err.message);
        return;
    }
    console.log("File contents:", data);
});

console.log("This prints BEFORE the file contents!");
```

**Output order:**

```
This prints BEFORE the file contents!
File contents: (contents of hello.txt)
```

The `readFile` call returns immediately. The callback runs later, when the I/O completes.

#### Callback Hell

Nesting callbacks leads to deeply indented, hard-to-read code:

```javascript
fs.readFile('a.txt', 'utf8', (err, a) => {
    if (err) throw err;
    fs.readFile('b.txt', 'utf8', (err, b) => {
        if (err) throw err;
        fs.writeFile('c.txt', a + b, (err) => {
            if (err) throw err;
            console.log("Done!");
        });
    });
});
```

This is why Promises and async/await were introduced.

### Promises — Structured Async

A Promise represents a value that will be available in the future. It has three states:

```
pending  →  fulfilled (resolved with a value)
         →  rejected  (failed with an error)
```

**C analogy**: A Promise is like a `future` in C++ or a file descriptor you `poll()` on — it represents a pending result.

```javascript
const fs = require('fs').promises; // Node's promise-based fs API

fs.readFile('hello.txt', 'utf8')
    .then(data => {
        console.log("Contents:", data);
        return data.length;
    })
    .then(length => {
        console.log("Length:", length);
    })
    .catch(err => {
        console.error("Error:", err.message);
    });
```

#### Creating Your Own Promise

```javascript
function delay(ms) {
    return new Promise((resolve, reject) => {
        if (ms < 0) {
            reject(new Error("Delay must be positive"));
            return;
        }
        setTimeout(() => resolve(`Waited ${ms}ms`), ms);
    });
}

delay(1000)
    .then(msg => console.log(msg))     // "Waited 1000ms"
    .catch(err => console.error(err));
```

#### Running Promises in Parallel

```javascript
const fs = require('fs').promises;

// Read multiple files concurrently — like spawning threads for each read
Promise.all([
    fs.readFile('a.txt', 'utf8'),
    fs.readFile('b.txt', 'utf8'),
    fs.readFile('c.txt', 'utf8')
])
.then(([a, b, c]) => {
    console.log("All files read:", a.length, b.length, c.length);
})
.catch(err => {
    console.error("One of the reads failed:", err.message);
});
```

### async/await — Synchronous-Looking Async

`async/await` is syntactic sugar over Promises. It lets you write asynchronous code that reads like synchronous C code.

```javascript
const fs = require('fs').promises;

async function readAndProcess() {
    try {
        const data = await fs.readFile('hello.txt', 'utf8');
        console.log("Contents:", data);

        const upper = data.toUpperCase();
        await fs.writeFile('upper.txt', upper);
        console.log("Wrote uppercase version");

    } catch (err) {
        console.error("Failed:", err.message);
    }
}

readAndProcess();
```

**Rules:**
- `await` can only be used inside an `async` function (or at the top level of an ES Module).
- `await` pauses **that function** — it does NOT block the event loop. Other callbacks can run while it waits.
- An `async` function always returns a Promise.

#### Parallel with async/await

```javascript
async function readAll() {
    // Sequential — slow (waits for each file one at a time)
    const a = await fs.readFile('a.txt', 'utf8');
    const b = await fs.readFile('b.txt', 'utf8');

    // Parallel — fast (fires both reads simultaneously)
    const [x, y] = await Promise.all([
        fs.readFile('a.txt', 'utf8'),
        fs.readFile('b.txt', 'utf8')
    ]);
}
```

### Side-by-Side Comparison

Reading a file three ways:

```javascript
const fs = require('fs');
const fsp = require('fs').promises;

// 1. Callback
fs.readFile('data.txt', 'utf8', (err, data) => {
    if (err) return console.error(err);
    console.log("[callback]", data);
});

// 2. Promise
fsp.readFile('data.txt', 'utf8')
    .then(data => console.log("[promise]", data))
    .catch(err => console.error(err));

// 3. async/await
async function read() {
    try {
        const data = await fsp.readFile('data.txt', 'utf8');
        console.log("[async]", data);
    } catch (err) {
        console.error(err);
    }
}
read();
```

### Exercise 4.1

Create a file `data.txt` with some text, then write `async_demo.js`:

```javascript
const fs = require('fs').promises;

async function main() {
    // Read, transform, write — all async
    const original = await fs.readFile('data.txt', 'utf8');
    const lines = original.split('\n');
    const numbered = lines.map((line, i) => `${i + 1}: ${line}`).join('\n');
    await fs.writeFile('numbered.txt', numbered);
    console.log("Done. Lines processed:", lines.length);
}

main().catch(console.error);
```

---

## 5. Event Loop and Non-Blocking I/O

### What the Event Loop Does

The event loop is Node's central execution mechanism. It's a loop that:

1. Picks up the next event/callback from the queue
2. Executes it to completion (run-to-completion semantics)
3. Goes back to step 1

```
   ┌───────────────────────────┐
┌─>│           timers          │  ← setTimeout, setInterval callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │     pending callbacks     │  ← I/O callbacks deferred from previous cycle
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │       idle, prepare       │  ← internal use
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │           poll            │  ← retrieve new I/O events; execute I/O callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │           check           │  ← setImmediate callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │      close callbacks      │  ← socket.on('close', ...)
│  └─────────────┬─────────────┘
└────────────────┘
```

### Why Node.js Is Efficient for I/O

**C comparison — threaded model (like Apache):**

```
Client 1 → Thread 1 → blocks on read() → wakes up → responds
Client 2 → Thread 2 → blocks on read() → wakes up → responds
Client 3 → Thread 3 → blocks on read() → wakes up → responds
```

10,000 clients = 10,000 threads = high memory overhead + context switching.

**Node.js — event-driven model (like nginx):**

```
Client 1 ─┐
Client 2 ─┤→ Single thread + event loop → non-blocking I/O
Client 3 ─┘
```

10,000 clients = 1 thread + kernel-level I/O multiplexing (epoll/kqueue). Each connection uses minimal memory.

### Blocking vs Non-Blocking

```javascript
const fs = require('fs');

// ❌ BLOCKING — freezes the entire server while reading
const data = fs.readFileSync('big_file.txt', 'utf8');
console.log(data);

// ✅ NON-BLOCKING — event loop stays free
fs.readFile('big_file.txt', 'utf8', (err, data) => {
    console.log(data);
});
console.log("Event loop is free to handle other requests");
```

### Execution Order Demo

This demonstrates how the event loop prioritizes different types of callbacks:

```javascript
console.log("1: synchronous — runs first");

setTimeout(() => {
    console.log("2: setTimeout — timer phase");
}, 0);

setImmediate(() => {
    console.log("3: setImmediate — check phase");
});

process.nextTick(() => {
    console.log("4: nextTick — runs before any I/O phase");
});

Promise.resolve().then(() => {
    console.log("5: Promise.then — microtask, runs after nextTick");
});

console.log("6: synchronous — runs second");
```

**Output:**

```
1: synchronous — runs first
6: synchronous — runs second
4: nextTick — runs before any I/O phase
5: Promise.then — microtask, runs after nextTick
2: setTimeout — timer phase
3: setImmediate — check phase
```

**Priority**: synchronous code > `process.nextTick` > microtasks (Promises) > macrotasks (timers, I/O).

### The Danger of Blocking the Event Loop

```javascript
const http = require('http');

const server = http.createServer((req, res) => {
    if (req.url === '/slow') {
        // ❌ This blocks the ENTIRE server for 5 seconds
        const start = Date.now();
        while (Date.now() - start < 5000) {} // busy wait
        res.end("Slow response\n");
    } else {
        res.end("Fast response\n");
    }
});

server.listen(3000);
// While /slow is processing, NO other requests can be handled.
// In C with threads, other threads would still run.
// In Node.js, you must offload CPU work to worker threads or child processes.
```

### Exercise 5.1

Create `event_order.js` and predict the output before running:

```javascript
const fs = require('fs');

console.log("START");

fs.readFile(__filename, () => {
    console.log("FILE READ COMPLETE");

    setTimeout(() => console.log("timeout inside readFile"), 0);
    setImmediate(() => console.log("immediate inside readFile"));
});

setTimeout(() => console.log("TIMEOUT 1"), 0);
setTimeout(() => console.log("TIMEOUT 2"), 0);
setImmediate(() => console.log("IMMEDIATE 1"));

process.nextTick(() => console.log("NEXT TICK"));
Promise.resolve().then(() => console.log("PROMISE"));

console.log("END");
```

Run it with `node event_order.js` and compare with your prediction.

**Expected output:**

```
START
END
NEXT TICK
PROMISE
TIMEOUT 1
TIMEOUT 2
IMMEDIATE 1
FILE READ COMPLETE
immediate inside readFile
timeout inside readFile
```

> **Note**: Inside an I/O callback, `setImmediate` always runs before `setTimeout(fn, 0)`.

---

**Continue to [Part 2](nodejs_tutorial_part2.md)** — File System, HTTP Server, NPM, Environment Variables, and Mini Project.
