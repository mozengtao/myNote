# Node.js Tutorial — Part 2: Building Things

> Continues from [Part 1](nodejs_tutorial_part1.md) (Fundamentals, Globals, Modules, Async, Event Loop).

---

## Table of Contents — Part 2

6. [File System Basics](#6-file-system-basics)
7. [Building a Simple HTTP Server](#7-building-a-simple-http-server)
8. [Node Package System](#8-node-package-system)
9. [Environment Variables](#9-environment-variables)
10. [Mini Project — REST API Server](#10-mini-project--rest-api-server)

---

## 6. File System Basics

The `fs` module is Node's equivalent of `<stdio.h>` + POSIX file operations. Almost every function comes in three flavors:

| Flavor | Example | Behavior |
|---|---|---|
| Synchronous | `fs.readFileSync()` | Blocks the event loop. Like `fread()` in C. |
| Callback | `fs.readFile()` | Non-blocking. Calls your function when done. |
| Promise | `fs.promises.readFile()` | Non-blocking. Returns a Promise. |

**Rule of thumb**: Use the Promise API (`fs.promises`) for application code. Use Sync versions only for startup/configuration scripts.

### Read File

```javascript
const fs = require('fs');
const fsp = require('fs').promises;

// Synchronous — blocks
const data = fs.readFileSync('config.txt', 'utf8');
console.log(data);

// Callback — non-blocking
fs.readFile('config.txt', 'utf8', (err, data) => {
    if (err) {
        console.error(err.message);
        return;
    }
    console.log(data);
});

// async/await — non-blocking, clean syntax
async function readConfig() {
    const data = await fsp.readFile('config.txt', 'utf8');
    console.log(data);
}
readConfig();
```

> **Encoding**: Without `'utf8'`, `readFile` returns a raw `Buffer` (like `unsigned char[]` in C). Always pass the encoding for text files.

### Write File

Overwrites the file if it exists, creates it if it doesn't (like `fopen("w")`).

```javascript
const fs = require('fs');
const fsp = require('fs').promises;

// Synchronous
fs.writeFileSync('output.txt', 'Hello, World!\n');

// Callback
fs.writeFile('output.txt', 'Hello, World!\n', (err) => {
    if (err) throw err;
    console.log('Written');
});

// async/await
async function write() {
    await fsp.writeFile('output.txt', 'Hello, World!\n');
    console.log('Written');
}
write();
```

### Append File

Adds to the end of a file (like `fopen("a")`).

```javascript
const fs = require('fs');
const fsp = require('fs').promises;

// Synchronous
fs.appendFileSync('log.txt', `[${new Date().toISOString()}] Server started\n`);

// Callback
fs.appendFile('log.txt', 'New entry\n', (err) => {
    if (err) throw err;
});

// async/await
async function appendLog(message) {
    await fsp.appendFile('log.txt', `[${new Date().toISOString()}] ${message}\n`);
}
appendLog('Request received');
```

### Other Useful Operations

```javascript
const fs = require('fs').promises;

async function demo() {
    // Check if file exists
    try {
        await fs.access('myfile.txt');
        console.log('File exists');
    } catch {
        console.log('File does not exist');
    }

    // Get file info — like stat() in C
    const stats = await fs.stat('myfile.txt');
    console.log('Size:', stats.size, 'bytes');
    console.log('Is directory:', stats.isDirectory());
    console.log('Modified:', stats.mtime);

    // List directory — like opendir()/readdir() in C
    const files = await fs.readdir('.');
    console.log('Files:', files);

    // Create directory — like mkdir()
    await fs.mkdir('new_dir', { recursive: true });

    // Delete file — like unlink()
    await fs.unlink('temp.txt');

    // Rename/move — like rename()
    await fs.rename('old.txt', 'new.txt');
}
```

### Exercise 6.1

Create `file_ops.js`:

```javascript
const fs = require('fs').promises;
const path = require('path');

async function main() {
    const dir = path.join(__dirname, 'test_output');

    // Create directory
    await fs.mkdir(dir, { recursive: true });

    // Write three files
    for (let i = 1; i <= 3; i++) {
        const file = path.join(dir, `file${i}.txt`);
        await fs.writeFile(file, `This is file number ${i}\n`);
    }

    // Read all files and concatenate
    const files = await fs.readdir(dir);
    let combined = '';
    for (const file of files) {
        const content = await fs.readFile(path.join(dir, file), 'utf8');
        combined += content;
    }

    // Write combined output
    await fs.writeFile(path.join(dir, 'combined.txt'), combined);
    console.log('Combined contents:\n' + combined);
}

main().catch(console.error);
```

---

## 7. Building a Simple HTTP Server

The `http` module provides low-level HTTP server and client functionality. No external dependencies needed.

### Minimal Server

```javascript
const http = require('http');

const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('Hello Node.js\n');
});

server.listen(3000, () => {
    console.log('Server running at http://localhost:3000');
});
```

**C analogy**: This is like `socket()` → `bind()` → `listen()` → `accept()` in a loop, but Node handles the loop and connection management for you.

The callback `(req, res)` fires for every incoming request:
- `req` — `http.IncomingMessage` — the request (method, URL, headers, body stream)
- `res` — `http.ServerResponse` — the response you build and send

### Handling Different Routes

```javascript
const http = require('http');

const server = http.createServer((req, res) => {
    // Set default header
    res.setHeader('Content-Type', 'application/json');

    if (req.method === 'GET' && req.url === '/') {
        res.writeHead(200);
        res.end(JSON.stringify({ message: 'Welcome to the API' }));

    } else if (req.method === 'GET' && req.url === '/health') {
        res.writeHead(200);
        res.end(JSON.stringify({ status: 'ok', uptime: process.uptime() }));

    } else if (req.method === 'GET' && req.url === '/time') {
        res.writeHead(200);
        res.end(JSON.stringify({ time: new Date().toISOString() }));

    } else {
        res.writeHead(404);
        res.end(JSON.stringify({ error: 'Not Found' }));
    }
});

server.listen(3000, () => {
    console.log('API server running on http://localhost:3000');
});
```

Test with `curl`:

```bash
curl http://localhost:3000/
curl http://localhost:3000/health
curl http://localhost:3000/time
curl http://localhost:3000/nonexistent
```

### Reading Request Body (POST)

The request body comes as a stream. You must collect the chunks:

```javascript
const http = require('http');

const server = http.createServer((req, res) => {
    if (req.method === 'POST' && req.url === '/echo') {
        let body = '';

        // Data comes in chunks (like read() returning partial data)
        req.on('data', chunk => {
            body += chunk.toString();
        });

        // All data received
        req.on('end', () => {
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ received: body }));
        });

    } else {
        res.writeHead(404);
        res.end('Not Found\n');
    }
});

server.listen(3000, () => console.log('Listening on 3000'));
```

Test:

```bash
curl -X POST -d '{"name":"Alice"}' http://localhost:3000/echo
```

### Helper: Parse Request Body as JSON

```javascript
function parseBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => { body += chunk.toString(); });
        req.on('end', () => {
            try {
                resolve(JSON.parse(body));
            } catch (err) {
                reject(new Error('Invalid JSON'));
            }
        });
        req.on('error', reject);
    });
}

// Usage inside request handler:
// const data = await parseBody(req);
```

### Exercise 7.1

Build a server with these routes:

| Method | Path | Response |
|---|---|---|
| GET | `/` | `{ "message": "Hello" }` |
| GET | `/info` | `{ "nodeVersion": "...", "platform": "...", "pid": ... }` |
| POST | `/reverse` | Accepts `{ "text": "hello" }`, returns `{ "reversed": "olleh" }` |
| * | * | `404 Not Found` |

---

## 8. Node Package System

### npm — Node Package Manager

npm is to Node.js what `apt`/`yum` is to Linux or `pip` is to Python. It manages third-party libraries from the [npm registry](https://www.npmjs.com/) (over 2 million packages).

npm ships with Node.js — no separate install needed.

### Initialize a Project

```bash
mkdir my-project && cd my-project
npm init -y    # creates package.json with defaults
```

This creates `package.json` — the project manifest (like a `Makefile` + dependency list combined):

```json
{
  "name": "my-project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "start": "node index.js",
    "test": "echo \"Error: no test specified\" && exit 1"
  },
  "keywords": [],
  "author": "",
  "license": "ISC"
}
```

### Install Packages

```bash
# Install a runtime dependency (goes into "dependencies")
npm install express           # or: npm i express

# Install a dev-only dependency (goes into "devDependencies")
npm install --save-dev nodemon  # or: npm i -D nodemon

# Install globally (CLI tools available system-wide)
npm install -g typescript
```

#### dependencies vs devDependencies

| Type | Purpose | Example | Deployed to production? |
|---|---|---|---|
| `dependencies` | Required at runtime | `express`, `pg`, `axios` | Yes |
| `devDependencies` | Needed only during development | `nodemon`, `jest`, `eslint` | No |

After installing, a `node_modules/` directory appears (like a local `/usr/lib/` for your project) and a `package-lock.json` (exact dependency versions — commit this to git).

### Using an Installed Package

```bash
npm install lodash
```

```javascript
const _ = require('lodash');

console.log(_.chunk([1, 2, 3, 4, 5, 6], 2));
// [[1, 2], [3, 4], [5, 6]]

console.log(_.uniq([1, 1, 2, 3, 3, 3]));
// [1, 2, 3]
```

### npm Scripts

The `"scripts"` section in `package.json` defines named commands:

```json
{
  "scripts": {
    "start": "node server.js",
    "dev": "nodemon server.js",
    "test": "jest",
    "lint": "eslint ."
  }
}
```

Run them with:

```bash
npm start         # special: no "run" needed for start/test
npm test          # special: no "run" needed
npm run dev       # custom scripts need "run"
npm run lint
```

### Key npm Commands Cheat Sheet

```bash
npm init -y              # create package.json
npm install              # install all deps from package.json
npm install <pkg>        # add a dependency
npm install -D <pkg>     # add a dev dependency
npm uninstall <pkg>      # remove a package
npm update               # update packages to latest compatible
npm list                 # show installed packages
npm list --depth=0       # top-level packages only
npm outdated             # check for newer versions
npm audit                # check for security vulnerabilities
npm run <script>         # run a script from package.json
```

### .gitignore for Node Projects

Always ignore `node_modules/` (it can be regenerated from `package-lock.json`):

```
node_modules/
.env
```

### Exercise 8.1

```bash
mkdir npm-demo && cd npm-demo
npm init -y
npm install chalk@5       # terminal colors (ESM-only in v5)
```

Create `index.mjs`:

```javascript
import chalk from 'chalk';

console.log(chalk.green('Success!'));
console.log(chalk.red.bold('Error!'));
console.log(chalk.blue.underline('Info'));
```

Add a script to `package.json`:

```json
"scripts": {
    "start": "node index.mjs"
}
```

Run: `npm start`

---

## 9. Environment Variables

### process.env

Environment variables are key-value pairs passed to your process by the OS. In C, you access them with `getenv("HOME")`. In Node.js, it's `process.env.HOME`.

```javascript
// Access standard env vars
console.log(process.env.HOME);
console.log(process.env.USER);
console.log(process.env.PATH);

// Access custom env vars
const port = process.env.PORT || 3000;
const dbUrl = process.env.DATABASE_URL || 'localhost:5432/mydb';
```

### Setting Environment Variables

```bash
# Inline (for one command)
PORT=8080 node server.js

# Export (for the whole shell session)
export PORT=8080
node server.js
```

### The .env File Pattern

For local development, use a `.env` file with the `dotenv` package:

```bash
npm install dotenv
```

Create `.env`:

```
PORT=8080
DB_HOST=localhost
DB_PORT=5432
DB_NAME=myapp
SECRET_KEY=supersecret123
```

Load it at the top of your entry point:

```javascript
require('dotenv').config();

// Now process.env.PORT === '8080' (always strings!)
const port = parseInt(process.env.PORT, 10) || 3000;
const dbHost = process.env.DB_HOST;

console.log(`Server will use port ${port}`);
console.log(`Database at ${dbHost}`);
```

> **Security**: NEVER commit `.env` to git. Add it to `.gitignore`.

### Configuration Pattern

A clean pattern is to centralize config:

```javascript
// config.js
require('dotenv').config();

const config = {
    port: parseInt(process.env.PORT, 10) || 3000,
    db: {
        host: process.env.DB_HOST || 'localhost',
        port: parseInt(process.env.DB_PORT, 10) || 5432,
        name: process.env.DB_NAME || 'myapp',
    },
    secretKey: process.env.SECRET_KEY || 'dev-secret',
    isDev: process.env.NODE_ENV !== 'production',
};

module.exports = config;
```

```javascript
// server.js
const config = require('./config');
console.log(`Starting server on port ${config.port}`);
```

### Exercise 9.1

Create a server that reads its port from `PORT` environment variable:

```javascript
const http = require('http');
const port = parseInt(process.env.PORT, 10) || 3000;

const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end(`Running on port ${port}\n`);
});

server.listen(port, () => {
    console.log(`Server listening on http://localhost:${port}`);
});
```

Test with different ports:

```bash
node server.js                  # uses default 3000
PORT=4000 node server.js        # uses 4000
PORT=9999 node server.js        # uses 9999
```

---

## 10. Mini Project — REST API Server

Build a TODO API that reads/writes a JSON file, uses modules, and demonstrates async/await.

### Project Structure

```
todo-api/
├── package.json
├── server.js          ← entry point
├── routes.js          ← request routing
├── store.js           ← JSON file read/write
└── data/
    └── todos.json     ← persistent storage
```

### Step 1: Initialize

```bash
mkdir todo-api && cd todo-api
npm init -y
mkdir data
echo '[]' > data/todos.json
```

### Step 2: The Store Module (`store.js`)

This module handles all file I/O. It reads and writes a JSON file as the "database".

```javascript
const fs = require('fs').promises;
const path = require('path');

const DATA_FILE = path.join(__dirname, 'data', 'todos.json');

async function readTodos() {
    try {
        const raw = await fs.readFile(DATA_FILE, 'utf8');
        return JSON.parse(raw);
    } catch (err) {
        if (err.code === 'ENOENT') {
            // File doesn't exist yet — return empty array
            return [];
        }
        throw err;
    }
}

async function writeTodos(todos) {
    const json = JSON.stringify(todos, null, 2);
    await fs.writeFile(DATA_FILE, json, 'utf8');
}

async function addTodo(text) {
    const todos = await readTodos();
    const todo = {
        id: Date.now(),
        text,
        done: false,
        createdAt: new Date().toISOString(),
    };
    todos.push(todo);
    await writeTodos(todos);
    return todo;
}

async function getAllTodos() {
    return readTodos();
}

async function toggleTodo(id) {
    const todos = await readTodos();
    const todo = todos.find(t => t.id === id);
    if (!todo) return null;
    todo.done = !todo.done;
    await writeTodos(todos);
    return todo;
}

async function deleteTodo(id) {
    const todos = await readTodos();
    const index = todos.findIndex(t => t.id === id);
    if (index === -1) return null;
    const [removed] = todos.splice(index, 1);
    await writeTodos(todos);
    return removed;
}

module.exports = { getAllTodos, addTodo, toggleTodo, deleteTodo };
```

### Step 3: The Router Module (`routes.js`)

Parses request body and routes requests to the store.

```javascript
const store = require('./store');

function parseBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => { body += chunk.toString(); });
        req.on('end', () => {
            if (!body) return resolve({});
            try {
                resolve(JSON.parse(body));
            } catch {
                reject(new Error('Invalid JSON'));
            }
        });
        req.on('error', reject);
    });
}

function sendJSON(res, statusCode, data) {
    res.writeHead(statusCode, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(data, null, 2));
}

async function handleRequest(req, res) {
    const { method, url } = req;

    try {
        // GET /todos — list all todos
        if (method === 'GET' && url === '/todos') {
            const todos = await store.getAllTodos();
            sendJSON(res, 200, todos);
            return;
        }

        // POST /todos — add a new todo
        if (method === 'POST' && url === '/todos') {
            const { text } = await parseBody(req);
            if (!text || typeof text !== 'string') {
                sendJSON(res, 400, { error: 'Missing "text" field' });
                return;
            }
            const todo = await store.addTodo(text.trim());
            sendJSON(res, 201, todo);
            return;
        }

        // PATCH /todos/:id/toggle — toggle done status
        const toggleMatch = url.match(/^\/todos\/(\d+)\/toggle$/);
        if (method === 'PATCH' && toggleMatch) {
            const id = parseInt(toggleMatch[1], 10);
            const todo = await store.toggleTodo(id);
            if (!todo) {
                sendJSON(res, 404, { error: 'Todo not found' });
                return;
            }
            sendJSON(res, 200, todo);
            return;
        }

        // DELETE /todos/:id — delete a todo
        const deleteMatch = url.match(/^\/todos\/(\d+)$/);
        if (method === 'DELETE' && deleteMatch) {
            const id = parseInt(deleteMatch[1], 10);
            const todo = await store.deleteTodo(id);
            if (!todo) {
                sendJSON(res, 404, { error: 'Todo not found' });
                return;
            }
            sendJSON(res, 200, { deleted: todo });
            return;
        }

        // Fallback — 404
        sendJSON(res, 404, { error: 'Not Found' });

    } catch (err) {
        console.error('Request error:', err);
        sendJSON(res, 500, { error: 'Internal Server Error' });
    }
}

module.exports = { handleRequest };
```

### Step 4: The Server (`server.js`)

```javascript
const http = require('http');
const { handleRequest } = require('./routes');

const PORT = parseInt(process.env.PORT, 10) || 3000;

const server = http.createServer(handleRequest);

server.listen(PORT, () => {
    console.log(`TODO API server running at http://localhost:${PORT}`);
    console.log('');
    console.log('Endpoints:');
    console.log('  GET    /todos              — list all todos');
    console.log('  POST   /todos              — add todo (body: {"text": "..."})');
    console.log('  PATCH  /todos/:id/toggle   — toggle done status');
    console.log('  DELETE /todos/:id          — delete a todo');
});
```

### Step 5: Add npm Scripts

Update `package.json`:

```json
{
  "name": "todo-api",
  "version": "1.0.0",
  "scripts": {
    "start": "node server.js"
  }
}
```

### Step 6: Test It

Start the server:

```bash
npm start
```

In another terminal, use `curl`:

```bash
# Add todos
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"text":"Learn Node.js"}' http://localhost:3000/todos | jq

curl -s -X POST -H "Content-Type: application/json" \
  -d '{"text":"Build a REST API"}' http://localhost:3000/todos | jq

curl -s -X POST -H "Content-Type: application/json" \
  -d '{"text":"Deploy to production"}' http://localhost:3000/todos | jq

# List all todos
curl -s http://localhost:3000/todos | jq

# Toggle a todo (replace 1234567890 with an actual ID from the list)
curl -s -X PATCH http://localhost:3000/todos/1234567890/toggle | jq

# Delete a todo
curl -s -X DELETE http://localhost:3000/todos/1234567890 | jq

# Verify the JSON file was updated
cat data/todos.json
```

### What This Project Demonstrates

| Concept | Where Used |
|---|---|
| **Modules** | `store.js`, `routes.js` imported by `server.js` |
| **async/await** | Every store operation and request handler |
| **File I/O** | Reading/writing `todos.json` |
| **HTTP server** | `http.createServer` in `server.js` |
| **JSON parsing** | Request body parsing in `routes.js` |
| **Error handling** | try/catch in handlers, 400/404/500 responses |
| **process.env** | `PORT` configuration |
| **path module** | Resolving the data file path |

### Best Practices Applied

1. **Separation of concerns** — routing logic, data access, and server setup are in separate modules.
2. **Error-first design** — every async operation is wrapped in try/catch.
3. **Input validation** — POST endpoint checks for required fields.
4. **Proper HTTP status codes** — 200 (OK), 201 (Created), 400 (Bad Request), 404 (Not Found), 500 (Server Error).
5. **No blocking calls** — all file operations use the async API.
6. **Configuration via environment** — port is configurable without code changes.

---

## Quick Reference Card

```
Node.js Essentials — Cheat Sheet
─────────────────────────────────

Run a file:            node app.js
REPL:                  node         (interactive, like python3)

Globals:               process, global, console, __dirname, __filename
CLI args:              process.argv.slice(2)
Env vars:              process.env.MY_VAR
Exit:                  process.exit(code)

Modules (CJS):         const x = require('./x')
                       module.exports = { fn1, fn2 }

Modules (ESM):         import { fn } from './x.mjs'
                       export function fn() { }

File ops:              const fs = require('fs').promises
                       await fs.readFile(path, 'utf8')
                       await fs.writeFile(path, data)
                       await fs.appendFile(path, data)
                       await fs.readdir(path)
                       await fs.mkdir(path, { recursive: true })
                       await fs.unlink(path)

HTTP server:           const http = require('http')
                       http.createServer((req, res) => { ... }).listen(3000)

npm:                   npm init -y
                       npm install <pkg>
                       npm install -D <pkg>
                       npm run <script>

Async patterns:        callback(err, result)
                       promise.then(ok).catch(err)
                       const result = await asyncFn()

Event loop priority:   sync → nextTick → microtask → timer → I/O → check
```

---

**End of tutorial.** You now have the foundational 20% that covers 80% of what you'll use daily in Node.js. From here, explore:

- **Express.js** — the standard web framework (routes, middleware, templating)
- **Streams** — processing large data without loading it all into memory
- **Worker Threads** — parallel CPU work (`worker_threads` module)
- **Child Processes** — spawning external commands (`child_process` module)
- **Databases** — `pg` (PostgreSQL), `mongoose` (MongoDB), `better-sqlite3`
- **Testing** — `jest` or Node's built-in test runner (`node --test`)
