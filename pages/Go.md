
- [**Playground**](https://goplay.tools/)

- [lensm](https://github.com/loov/lensm)
  > Go assembly and source viewer

- [深入Go语言之旅](https://go.cyub.vip/)
- [**Go 语言设计与实现**](https://draveness.me/golang/)

- [Go 程序员宝典](https://shgopher.github.io/GOFamily/)

- [Golang Internals, Part 1](https://www.altoros.com/blog/golang-internals-part-1-main-concepts-and-project-structure/)

- [How to Write Go Code](https://go.dev/doc/code)
- [**Go 学习路线**](https://github.com/rosedblabs/go-learning)
  - [7天用Go从零实现Web框架Gee教程](https://geektutu.com/post/gee.html)
- [Go go-to guide](https://yourbasic.org/golang/)
- [Tutorials](https://go.dev/doc/tutorial/)
- [Tutorials](https://zetcode.com/all/#go)
- [Tutorials](https://tutorialedge.net/course/golang/)
- [Go examples](https://www.dotnetperls.com/s#go)
- [**Documentation**](https://go.dev/doc/)
- [**Command Documentation**](https://go.dev/doc/cmd)
- [**Go入门指南**](https://go.timpaik.top/)
- [**Go编程时光**](https://golang.iswbm.com/index.html)
- [[Go笔记]]
- [**Effective Go**](https://go.dev/doc/effective_go)
- [**The Go Programming Language Specification**](https://go.dev/ref/spec)
- ```bash
  $ sudo apt install golang-go -y
  ```
- [**go pkg**](https://pkg.go.dev/) #online
  - [strings](https://pkg.go.dev/strings)
    > Package strings implements simple functions to manipulate UTF-8 encoded strings.
    - `type Builder`
      > A Builder is used to efficiently build a string using Builder.Write methods. It minimizes memory copying. The zero value is ready to use. Do not copy a non-zero Builder.
  - [fmt](https://pkg.go.dev/fmt)
    > Package fmt implements formatted I/O with functions analogous to C's printf and scanf. The format 'verbs' are derived from C's but are simpler.
  - [runtime](https://pkg.go.dev/runtime)
    > Package runtime contains operations that interact with Go's runtime system, such as functions to control goroutines.
  - [os](https://pkg.go.dev/os)
    > Package os provides a platform-independent interface to operating system functionality. The design is Unix-like, although the error handling is Go-like; failing calls return values of type error rather than error numbers. Often, more information is available within the error. For example, if a call that takes a file name fails, such as Open or Stat, the error will include the failing file name when printed and will be of type *PathError, which may be unpacked for more information.
    - [exec](https://pkg.go.dev/os/exec)
      > Package exec runs external commands. It wraps os.StartProcess to make it easier to remap stdin and stdout, connect I/O with pipes, and do other adjustments.
  - [flag](https://pkg.go.dev/flag)
    > Package flag implements command-line flag parsing.
  - [bufio](https://pkg.go.dev/bufio)
    > Package bufio implements buffered I/O. It wraps an io.Reader or io.Writer object, creating another object (Reader or Writer) that also implements the interface but provides buffering and some help for textual I/O.
    - `type ReadWriter`
      > ReadWriter stores pointers to a Reader and a Writer. It implements io.ReadWriter.
    - `type Reader`
      > Reader implements buffering for an io.Reader object.
    - `type Scanner`
      > Scanner provides a convenient interface for reading data such as a file of newline-delimited lines of text. 
    - `type SplitFunc`
      > SplitFunc is the signature of the split function used to tokenize the input. 
    - `type Writer`
      > Writer implements buffering for an io.Writer object.
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
  - [regexp](https://pkg.go.dev/regexp)
    > Package regexp implements regular expression search.
  - [encoding](https://pkg.go.dev/encoding)
    > Package encoding defines interfaces shared by other packages that convert data to and from byte-level and textual representations.
    ```go
    binary
    csv
    gob
    hex
    json
    xml
    ...
    ```
  - [log](https://pkg.go.dev/log)
    > Package log implements a simple logging package. It defines a type, Logger, with methods for formatting output. 
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
- [GOBook](https://github.com/hapi666/GOBook)
- [Mastering regular expressions in Go](https://www.honeybadger.io/blog/a-definitive-guide-to-regular-expressions-in-go/)
- [The complete guide to dates and times in Go](https://www.honeybadger.io/blog/complete-guide-to-dates-and-times-in-go/)
- [Error handling in Go: defer, panic, and recover](https://www.honeybadger.io/blog/go-exception-handling/)
- [Logging in Go: Choosing a System and Using it](https://www.honeybadger.io/blog/golang-logging/)
- [跟煎鱼学 Go](https://eddycjy.com/)
- []()