
[Asynchronous Non-blocking I/O under the hood: poll, select, epoll/kqueue](https://tuhuynh.com/en/posts/nio-under-the-hood/)
[]()
[select, poll 和 epoll比较](./epoll/select_poll_epoll.md)

- [main.c](epoll/main.c)
- [network_utils.c](epoll/network_utils.c)
- [network_utils.h](epoll/network_utils.h)

```bash
gcc -o epoll_server main.c network_utils.c

./epoll_server 8080

# terminal 1
nc localhost 8080

# terminal 2
nc localhost 8080

```
