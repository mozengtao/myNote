## High-Level Architecture
```
Client
  |
TCP Connection
  |
Redis Server
  |
In-Memory Data Structures (RAM)
  |
Persistence (RDB(Redis Database Snapshotting) / AOF(Append-Only File))
```

- [Redis Docs](https://redis.io/docs/latest/)
    - Redis CLI
- [redis](https://github.com/redis/redis)
- [Redis设计与实现](https://github.com/wonter/learning-distributed-storage/blob/master/sources/ebooks)
- [**Develop with Redis**](https://redis.io/docs/latest/develop/)
- [A Crash Course in Redis](https://blog.bytebytego.com/p/a-crash-course-in-redis)
- [Redis Explained](https://architecturenotes.co/p/redis)