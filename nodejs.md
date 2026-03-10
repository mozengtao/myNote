[**nodejs**](https://nodejs.org/en)  
[**nodejs modules**](https://nodejs.org/docs/latest/api/modules.html)  
[Node.js v20.12.0 文档](https://nodejs.cn/api/)
[nodejs modules](https://nodejs.org/docs/latest/api/)
[]()  

[**The Modern JavaScript Tutorial**](https://javascript.info/)  
[Solve Hard Problems Faster with Node.js](https://heynode.com/)  
[]()  


[CommonJS](./node/CommonJS.md)  
[Electron](./node/electron.md)  


[Node.js Tutorial — Part 1: Core Concepts](./node/docs/nodejs_tutorial_part1.md)  
[Node.js Tutorial — Part 2: Building Things](./node/docs/nodejs_tutorial_part2.md)  
[Node.js 深度剖析 — Part 1: 架构概览与事件循环](./node/docs/nodejs_deep_dive_part1.md)  
[Node.js 深度剖析 — Part 2: 异步编程范式与 V8 引擎](./node/docs/nodejs_deep_dive_part2.md)  
[Node.js 深度剖析 — Part 3: Libuv 详解与综合应用](./node/docs/nodejs_deep_dive_part3.md)  


- Promise
[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)  
[Promises, async/await](https://javascript.info/async)  
[Discover Promises in Node.js](https://nodejs.org/en/learn/asynchronous-work/discover-promises-in-nodejs)  
[Understanding async/await in Node.js](https://blog.postman.com/understanding-async-await-in-node-js/)  
[Mastering Promises in Node.js: A Comprehensive Guide](https://medium.com/@ayushnandanwar003/mastering-promises-in-node-js-a-comprehensive-guide-04dd7558a8d4)  
[Understanding Promises in Node.js](https://heynode.com/tutorial/what-are-promises/)  
[]()  
[]()  
[]()  
[]()  


[Introduction to Node.js](https://nodejs.org/en/learn/getting-started/introduction-to-nodejs)  
[Beginning Node.jS](https://edu.anarcho-copy.org/Programming%20Languages/Node/Beginning%20Nodejs.pdf)  
[Node.js Design Patterns](https://github.com/PacktPublishing/Node.js-Design-Patterns-Fourth-Edition)  
[nodebeginner](https://github.com/manuelkiessling/nodebeginner.org)  
[]()  

[How to Get Started with NodeJS](https://www.freecodecamp.org/news/get-started-with-nodejs/)  
[Node.js Tutorials: Tutorials for Programmers of All Levels](https://stackify.com/learn-nodejs-tutorials/)  
[]()  



[nodejs playground](https://stackblitz.com/edit/node-rski7n?file=index.js)  
[七天学会NodeJS](https://nqdeng.github.io/7-days-nodejs/)  

- Node.js Internal Architecture
```
      +-------------------------------------------------------+
      |                  Node.js Standard Library             |
      |          (fs, http, path, crypto, streams, etc.)      |
      +-------------------------------------------------------+
      |                  Node.js C++ Bindings                 |
      |    (Glue code connecting JS to underlying C++ libs)   |
      +-------------------------------------------------------+
      |            V8 Engine           |         Libuv        |
      |   (Google's JS Engine:         |  (Event Loop,        |
      |    Compiles JS to Machine Code)|   Thread Pool, I/O)  |
      +--------------------------------+----------------------+
      |          Low-Level Libs (c-ares, llhttp, OpenSSL)     |
      +-------------------------------------------------------+
      |                     Operating System                  |
      |                (Kernel, Network, File System)         |
      +-------------------------------------------------------+
```

- V8 Engine
    Developed by Google for Chrome, V8 is the heart of Node. It converts your JavaScript code into Machine Code that the processor can actually execute. Without V8, the computer wouldn't understand a single line of your .js files.

- Libuv
    If V8 is the heart, Libuv is the soul. It is a C library that manages the Event Loop and the Worker Pool.
        - Non-blocking I/O: Libuv handles network requests or file reading by offloading them to the OS kernel whenever possible.
        - The Thread Pool: For tasks that the OS can't do asynchronously (like heavy cryptography or complex file system tasks), Libuv manages a pool of threads (usually 4 by default) to run them in the background.

- The Event Loop
    This is the mechanism that allows Node.js to perform non-blocking I/O operations despite being single-threaded. It constantly checks if the "call stack" is empty and then processes callbacks from the "task queue."

- JavaScript Execution Flow
```
JS SOURCE CODE
        |
        v
 +-------------------------------------------------------------+
 |                         V8 ENGINE                           |
 |  +-------------------+             +---------------------+  |
 |  |    Memory Heap    |             |     Call Stack      |  |
 |  |  (Allocates mem)  |             |  (Executes frames)  |  |
 |  +-------------------+             +----------+----------+  |
 +-----------------------------------------------|-------------+
        |                                        |
        |   Non-blocking API Call                |
        |   (e.g., setTimeout, fs, fetch)        |
        v                                        v
 +-------------------------------------------------------------+
 |                         LIBUV / OS                          |
 |  +-------------------------------------------------------+  |
 |  |      Event Loop (Phases: Poll, Check, Close, etc.)    |  |
 |  +---------------------------^---------------------------+  |
 |                              |                              |
 |  +---------------------------+---------------------------+  |
 |  |    Worker Thread Pool     |      OS Kernel Tasks      |  |
 |  |    (File I/O, Crypto)     |      (Network, TCP)       |  |
 |  +---------------------------+---------------------------+  |
 +------------------------------|------------------------------+
                                |
        +-----------------------+
        |  Task Completion -> Callback sent to Queue
        v
 +----------------------------+       +------------------------+
 |      Callback Queue        | ----> |      Event Loop        |
 | (Task / Microtask Queues)  | <---- | (Checks if Stack empty)|
 +----------------------------+       +------------------------+
```

- Key Components of Execution
1. The Call Stack (LIFO)
This is where V8 tracks where we are in the program. When you call a function, it’s "pushed" onto the stack. When it returns, it’s "popped" off.
    The Single Thread: JavaScript can only do one thing at a time because it only has one Call Stack.

2. The Memory Heap
A large, mostly unstructured region of memory where objects, variables, and data are stored. V8’s Garbage Collector monitors this area to free up space.

3. Task Queues (The Waiting Room)
Once an asynchronous operation (handled by Libuv) finishes, the result doesn't just jump back into your code. It enters a queue:
    Microtask Queue: High-priority (Promises, process.nextTick). These are processed immediately after the current stack empties, before the next event loop tick.
    Task Queue (MacroTask): Standard-priority (setTimeout, I/O, setImmediate).

4. The Bridge (Node.js Bindings)
Since V8 is written in C++ and your code is JavaScript, they can't talk directly. Node.js provides Bindings (wrappers) that allow a JavaScript function call to trigger a C++ function inside Libuv.

- How a Request Flows
1. JavaScript Execution: Your code runs on the main thread (V8).
2. Offloading: When an asynchronous task is called (e.g., fs.readFile), Node.js passes the request to Libuv.
3. Background Work: Libuv uses the OS Kernel or its internal Thread Pool to complete the task.
4. Callback Queue: Once the task is finished, the result is placed in a queue.
5. Event Loop: The loop picks up the result and pushes the callback back to the main thread for V8 to execute.

Note: Because the main thread is single-threaded, if you run a massive for loop that takes 10 seconds, you "block" the event loop, and no other requests can be handled during that time. This is why we say "don't block the loop!"
