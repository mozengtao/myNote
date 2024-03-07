- [**Command Documentation**](https://go.dev/doc/cmd)
- [**Go入门指南**](https://go.timpaik.top/)
- [**Go编程时光**](https://golang.iswbm.com/index.html)
- [[Go笔记]]
- [**Effective Go**](https://go.dev/doc/effective_go)
- ```bash
  $ sudo apt install golang-go -y
  ```
- [**go pkg**](https://pkg.go.dev/) #online
  - [strings](https://pkg.go.dev/strings)
    > Package strings implements simple functions to manipulate UTF-8 encoded strings.
  - [fmt](https://pkg.go.dev/fmt)
    > Package fmt implements formatted I/O with functions analogous to C's printf and scanf. The format 'verbs' are derived from C's but are simpler.
  - [os](https://pkg.go.dev/os)
    > Package os provides a platform-independent interface to operating system functionality. The design is Unix-like, although the error handling is Go-like; failing calls return values of type error rather than error numbers. Often, more information is available within the error. For example, if a call that takes a file name fails, such as Open or Stat, the error will include the failing file name when printed and will be of type *PathError, which may be unpacked for more information.
    - [exec](https://pkg.go.dev/os/exec)
      > Package exec runs external commands. It wraps os.StartProcess to make it easier to remap stdin and stdout, connect I/O with pipes, and do other adjustments.
    - [flag](https://pkg.go.dev/flag)
      > Package flag implements command-line flag parsing.
    - [bufio](https://pkg.go.dev/bufio)
      > Package bufio implements buffered I/O. It wraps an io.Reader or io.Writer object, creating another object (Reader or Writer) that also implements the interface but provides buffering and some help for textual I/O.
    - [net](https://pkg.go.dev/net)
      > Package net provides a portable interface for network I/O, including TCP/IP, UDP, domain name resolution, and Unix domain sockets.
      - [http](https://pkg.go.dev/net/http@go1.22.1)
        > Package http provides HTTP client and server implementations.
    - [unsafe](https://pkg.go.dev/unsafe)
      > Package unsafe contains operations that step around the type safety of Go programs.
      ```go
      Sizeof
      Offsetof
      Allignof
      ...
      ```
    - [io](https://pkg.go.dev/io)
      > Package io provides basic interfaces to I/O primitives. Its primary job is to wrap existing implementations of such primitives, such as those in package os, into shared public interfaces that abstract the functionality, plus some other related primitives.
      - [fs](https://pkg.go.dev/io/fs)
        > Package fs defines basic interfaces to a file system. A file system can be provided by the host operating system but also by other packages.
- [project-layout](https://github.com/golang-standards/project-layout) #github
- [**Go by Example**](https://gobyexample.com/)
- [**online compiler with examples**](https://goplay.tools/)
- [runc](https://github.com/opencontainers/runc) #github
- [Go设计模式实战](https://tigerb.cn/go/#/patterns/template)
- [go学习3部曲:入门，进阶，实战](https://www.kancloud.cn/gofor/golang-learn/2571648)
- [Golang tutorial series](https://golangbot.com/learn-golang-series/)
- [Go 开发者路线图](https://github.com/debuginn/golang-developer-roadmap-cn?tab=readme-ov-file)
- [Go 语言进阶之旅](https://golang1.eddycjy.com/)
- [Golang 进阶](https://github.com/weirubo/intermediate_go?tab=readme-ov-file)
- [**Go语言高级编程**](https://chai2010.cn/advanced-go-programming-book/index.html)
- [**Go 语言设计与实现**](https://draveness.me/golang/)
- [GOBook](https://github.com/hapi666/GOBook)
- []()
- []()
- []()
- []()
- []()
- []()