- # what is zmq
	- #+BEGIN_QUOTE
	  ZeroMQ is an asynchronous network messaging library known for its high performance. It’s intended use is for distributed systems as well as concurrent systems. In summary, ZMQ allows you to send messages (binary data, serialized data, simple strings, etc.) over the network through various methods like TCP or multicast as well as between processes.  ZeroMQ provides a whole slew of language APIs which run on most operating systems and allows you to communicate seamlessly between all sorts of programs. It also provides a collection of patterns, such as request-reply and publish-subscribe which assist you in creating and structuring your network.
	  #+END_QUOTE
- # zmq基本概念
- #+BEGIN_QUOTE
  ZMQ是一种高速并发消息通信框架
  
  **ZMQ context**
  在使用任何ZMQ的库函数之前，必须使用**zmq_init()**进行ZMQ context的初始化
  Initialise ØMQ context
      zmq_init(3)
  Terminate ØMQ context
      zmq_term(3)
  
  ZQM context是线程安全的，因此zmq的调用者不需要额外的锁机制
  
  **ZMQ message**
  zmq message用于在不同的应用或者同一应用的不同组件之间传递数据，zmq message没有内部的数据结构，zmq message内部数据是不透明的二进制数据流
  Initialise a message
      zmq_msg_init(3) zmq_msg_init_size(3) zmq_msg_init_data(3)
  Release a message
      zmq_msg_close(3)
  Access message content
      zmq_msg_data(3) zmq_msg_size(3)
  Message manipulation
      zmq_msg_copy(3) zmq_msg_move(3)
  
  **ZMQ socket**
  zmq socket是对异步消息队列的抽象，不同的socket的type对应不同的语义
  [socket types](http://api.zeromq.org/2-1:zmq-socket)
  Request-reply pattern（请求-应答模式）
  The request-reply pattern is used for sending requests from a client to one or more instances of a service, and receiving subsequent replies to each request sent.
      ZMQ_REQ
      ZMQ_REP
      ZMQ_DEALER
      ZMQ_ROUTER
  
  Publish-subscribe pattern（发布-订阅模式）
  The publish-subscribe pattern is used for one-to-many distribution of data from a single publisher to multiple subscribers in a fan out fashion.
      ZMQ_PUB
      ZMQ_SUB
  
  Pipeline pattern（分布式处理）
  The pipeline pattern is used for distributing data to nodes arranged in a pipeline. Data always flows down the pipeline, and each stage of the pipeline is connected to at least one node. When a pipeline stage is connected to multiple nodes data is round-robined among all connected nodes.
      ZMQ_PUSH
      ZMQ_PULL
  
  Exclusive pair pattern
  The exclusive pair pattern is used to connect a peer to precisely one other peer. This pattern is used for inter-thread communication across the inproc transport.
      ZMQ_PAIR
  
  Creating a socket
      zmq_socket(3)
  Closing a socket
      zmq_close(3)
  Manipulating socket options
      zmq_getsockopt(3) zmq_setsockopt(3)
  Establishing a message flow
      zmq_bind(3) zmq_connect(3)
  Sending and receiving messages
      zmq_send(3) zmq_recv(3)
  Input/output multiplexing
      zmq_poll(3)
  
  **Transports**
  zmq socket可以使用不同的底层传输机制，每种传输机制适合于特定的用途，有自身的优点和缺点
  Unicast transport using TCP
      zmq_tcp(7)
  Reliable multicast transport using PGM
      zmq_pgm(7)
  Local inter-process communication transport
      zmq_ipc(7)
  Local in-process (inter-thread) communication transport
      zmq_inproc(7)
  
  **Devices**
  ØMQ provides devices, which are building blocks that act as intermediate nodes in complex messaging topologies. Devices can act as brokers that other nodes connect to, proxies that connect through to other nodes, or any mix of these two models.
  zmq_device(3)
  #+END_QUOTE
- # 参考
- [**zguide**](https://zguide.zeromq.org/)
- [Working with Messages](https://zguide.zeromq.org/docs/chapter2/#Working-with-Messages)
- [zmq(7)](https://libzmq.readthedocs.io/en/latest/zmq.html)
- [zmq.h](https://travlr.github.io/libzmq/zmq_8h_source.html#l00225)
- [A Brief Introduction to ZeroMQ](https://intelligentproduct.solutions/technical-software/introduction-to-zeromq/)
- [ZMQ Frequently Asked Questions](http://wiki.zeromq.org/area:faq#toc5)
- [ZMQ 指南](https://wizardforcel.gitbooks.io/zmq-guide/content/chapter1.html)
	- **[zguide](https://github.com/booksbyus/zguide)**
- [zguide](http://zguide2.wikidot.com/)
- [The ZeroMQ Guide - for C Developers](http://zguide2.wdfiles.com/local--files/page:start/zguide-c.pdf)
- [**The ZeroMQ project**](https://github.com/zeromq)
- [Learning ØMQ with pyzmq](https://learning-0mq-with-pyzmq.readthedocs.io/en/latest/index.html)
- [ØMQ - The Guide](https://zguide.zeromq.org/)
- [ØMQ API](http://api.zeromq.org/2-1:_start)
	- [zmq_ipc(7)](http://api.zeromq.org/4-1:zmq-ipc)
	- [unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html)
- [ZeroMQ: Diving into the Wire](https://www.codeproject.com/Articles/863889/ZeroMQ-Diving-into-the-Wire)
- [ZMQ API reference](https://libzmq.readthedocs.io/en/latest/)
- [Frequently Asked Questions](http://wiki.zeromq.org/area:faq)
- [The ZeroMQ Guide - for Python Developers](https://zguide2.wdfiles.com/local--files/page:start/zguide-py.pdf)
- [The ZeroMQ Guide - for C Developers](https://zguide2.wdfiles.com/local--files/page:start/zguide-c.pdf)
- [ZeroMQ RFC](https://rfc.zeromq.org/)
- [ØMQ - The Guide](https://zguide.zeromq.org/)
- [ZMQ 指南](https://github.com/anjuke/zguide-cn/tree/master)
- [zguide examples](https://github.com/booksbyus/zguide/tree/master/examples)
- [ØMQ/2.2 - The Guide](http://zguide2.zeromq.org/)
- [zmq_poll(3)](https://linux.die.net/man/3/zmq_poll)
- [czmq](https://github.com/zeromq/czmq) #github
- [CZMQ - High-level C Binding for ZeroMQ](http://czmq.zeromq.org/)
- [czmq zmsg.c](https://github.com/zeromq/czmq/blob/master/src/zmsg.c)
- [The PyZMQ API](https://pyzmq.readthedocs.io/en/latest/)
- [Using pyZMQ for inter-process communication: Part 1](https://pythonforthelab.com/blog/using-pyzmq-for-inter-process-communication-part-1/)
- [Using pyZMQ for inter-process communication: Part 2](https://pythonforthelab.com/blog/using-pyzmq-for-inter-process-communication-part-2/)