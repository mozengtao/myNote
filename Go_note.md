## slice
[How Slices Work in Go](https://dev.to/jpoly1219/how-slices-work-in-go-47nc)  
```go
// array (Arrays are basically containers with fixed sizes)

myArray := [3]int{0, 1, 2}
myArray := [...]string{"apple", "banana"}

// slice (Slices are implemented using arrays much but more powerful than arrays, because of their dynamic nature)

// 1 use make() and specify type, length, and capacity
mySlice := make([]int, 4, 4)
mySlice[0] = 0

// 2 declare a struct literal
mySlice := []int{0, 1, 2, 3}

// 3 create an empty slice and append to it
mySlice := []int{}
mySlice = append(mySlice, 0, 1, 2, 3)

// how slices work
a slice is a header that contains a pointer to an underlying array

type SliceHeader struct {
    Data uintptr
    Len int
    Cap int
}

// how slices "grow"
/*
1. It will check that the current length is equal to the capacity.
2. If appending over-capacity, a new slice with double the original slice's capacity will be created.
3. The original slice will be copied over to the new slice.
4. The new element will be appended at the end.
5. The resulting slice will be returned.
*/

// how slicing works
/*
1. It will point to a new location in the same underlying array.
2. Length and capacity will be adjusted.
*/

// 变长参数 (函数的变长参数（variadic parameters）在内部是通过 切片（slice） 来保存的)
func sum(nums ...int) int {
    total := 0
    for _, num := range nums {
        total += num
    }
    return total
}

fmt.Println(sum(1, 2, 3))

nums := []int{1, 2, 3}
fmt.Println(sum(nums...))
```

```go
  // package
  每个 Go 文件都属于且仅属于一个包。一个包可以由许多以 .go 为扩展名的源文件组成,必须在源文件中非注释的第一行指明这个文件属于哪个包，如：package main, package main表示一个可独立执行的程序，每个 Go 应用程序都包含一个名为 main 的包, 所有的包名都应该使用小写字母
  
  Go 的标准库包含了大量的包（如：fmt 和 os），用户也可以创建自己的包, 应用程序的包的依赖关系决定了其构建顺序。属于同一个包的源文件必须全部被一起编译，一个包即是编译时的一个单元，因此根据惯例，每个目录都只包含一个包。如果对一个包进行更改或重新编译，所有引用了这个包的客户端程序都必须全部重新编译。
  
  一个 Go 程序是通过 import 关键字将一组包链接在一起
  // 1
  import "fmt"
  import "os"
  
  // 2
  import (
     "fmt"
     "os"
  )
  
  // 3 (alias)
  import fm "fmt"
  
  在导入一个外部包后，能够且只能够访问该包中导出的对象
  当标识符（包括常量、变量、类型、函数名、结构字段等等）以一个大写字母开头，这种形式的标识符的对象就可以被外部包的代码所使用（客户端程序需要先导入这个包），这被称为导出（像面向对象语言中的 public）
  标识符如果以小写字母开头，则对包外是不可见的，但是他们在整个包的内部是可见并且可用的（像面向对象语言中的 private ）如果你导入了一个包却没有使用它，则会在构建程序时引发错误
  
  //  函数
  func functionName(parameter_list) (return_value_list) {   // { 必须和函数声明在同一行
     …
  }
  
  // 注释
  // 类型
  基本类型：int、float、bool、string
  复合类型：struct、array、slice、map、channel
  描述类型的行为的：interface
  
  // type (自定义类型)
  type (
     IZ int
     FZ float64
     STR string
  )
  
  //  Go 程序的一般结构
  package main  // 1
  
  import (      // 2
     "fmt"
  )
  
  const c = "C" // 3
  
  var v int = 5
  
  type T struct{}
  
  func init() { // 4 initialization of package
  }
  
  func main() { // 5
     var a int
     Func1()
     // ...
     fmt.Println(a)
  }
  
  func (t T) Method1() {
     //...
  }
  
  func Func1() { // exported function Func1
     //...
  }
  
  //  类型转换，Go 语言不存在隐式类型转换，因此所有的转换都必须显式说明
  valueOfTypeB = typeB(valueOfTypeA)
  
  a := 5.0
  b := int(a)
  
  // gofmt
  
  // 常量
  常量的数据类型只可以是布尔型、数字型（整数型、浮点型和复数）和字符串型
  const identifier [type] = value
  const Pi = 3.14159
  const b string = "abc"
  const b = "abc"   // type inference
  
  // itoa (赋值一个常量时，之后没赋值的常量都会应用上一行的赋值表达式)
  const (
  	a = iota  // a = 0
  	b         // b = 1
  	c         // c = 2
  	d = 5     // d = 5   
  	e         // e = 5
  )
  
  // 赋值两个常量，iota 只会增长一次，而不会因为使用了两次就增长两次
  const (
  	Apple, Banana = iota + 1, iota + 2 // Apple=1 Banana=2
  	Cherimoya, Durian                  // Cherimoya=2 Durian=3
  	Elderberry, Fig                    // Elderberry=3, Fig=4
  
  )
  
  // 使用 iota 结合 位运算 表示资源状态的使用案例
  const (
  	Open = 1 << iota  // 0001
  	Close             // 0010
  	Pending           // 0100
  )
  
  const (
  	_           = iota             // 使用 _ 忽略不需要的 iota
  	KB = 1 << (10 * iota)          // 1 << (10*1)
  	MB                             // 1 << (10*2)
  	GB                             // 1 << (10*3)
  	TB                             // 1 << (10*4)
  	PB                             // 1 << (10*5)
  	EB                             // 1 << (10*6)
  	ZB                             // 1 << (10*7)
  	YB                             // 1 << (10*8)
  )
  
  // 变量
  var identifier type   // declaration
  
  var a, b *int
  // 1
  var a int
  var b bool
  var str string
  
  // 2(一般用于声明全局变量)
  var (
  	a int   // default: 0
  	b bool  // default: false
  	str string  // default: nil
  )
  
  // 作用域
  var identifier [type] = value
  var a int = 15
  var i = 5 // type inference
  var b bool = false
  var str string = "Go says hello to the world!"
  
  var (
  	a = 15
  	b = false
  	str = "Go says hello to the world!"
  	numShips = 50
  	city string
  )
  
  var (
  	HOME = os.Getenv("HOME")
  	USER = os.Getenv("USER")
  	GOROOT = os.Getenv("GOROOT")
  )

  // 初始化声明: 函数体内局部变量的简短声明语法，只能被用在函数体内，不可以用于全局变量的声明与赋值
  a := 1
  
  // 只写变量 _
  _, b = 5, 7
  
  
  //  init函数 在包完成初始化后自动执行，并且执行优先级比 main 函数高，每个源文件只能包含一个 init 函数，init 函数也经常被用在当一个程序开始之前调用后台执行的 goroutine

  // 布尔类型 bool
  
  // 数字类型
  int8（-128 -> 127）
  int16（-32768 -> 32767）
  int32（-2,147,483,648 -> 2,147,483,647）
  int64（-9,223,372,036,854,775,808 -> 9,223,372,036,854,775,807）
  
  uint8（0 -> 255）
  uint16（0 -> 65,535）
  uint32（0 -> 4,294,967,295）
  uint64（0 -> 18,446,744,073,709,551,615）
  
  float32（+- 1e-45 -> +- 3.4 * 1e38）
  float64（+- 5 * 1e-324 -> 107 * 1e308）
  
  // 复数
  complex64 (32 位实数和虚数)
  complex128 (64 位实数和虚数)
  
  // 位运算
  &, |, ^, &^, ^, <<, >>
  
  //
  type ByteSize float64
  const (
  	_ = iota // 通过赋值给空白标识符来忽略值
  	KB ByteSize = 1<<(10*iota)
  	MB
  	GB
  	TB
  	PB
  	EB
  	ZB
  	YB
  )
  
  // e.g
  type BitFlag int
  const (
  	Active BitFlag = 1 << iota // 1 << 0 == 1
  	Send // 1 << 1 == 2
  	Receive // 1 << 2 == 4
  )
  
  flag := Active | Send // == 3
  
  
  //  随机数
  rand.Float32
  rand.Float64
  rand.Intn
  rand.Seed(value)
  
  // 自定义类型(类型别名)
  type TZ int
  
  // unicode package
  unicode.IsLetter(ch)
  unicode.IsDigit(ch)
  unicode.IsSpace(ch)
  
  // 字符串
  Go 中的字符串是根据长度限定，而非特殊字符\0
  
  //  字符串拼接
  str := "Beginning of the string " +
  	"second part of the string"
  
  s := "hel" + "lo,"
  s += "world!"
  fmt.Println(s) //输出 “hello, world!”
  
  // 拼接字符串更高效的方法
  strings.Join()
  bytes.Buffer
  
  
  // string operations
  strings.HasPrefix(s, prefix string) bool
  strings.HasSuffix(s, suffix string) bool
  strings.Contains(s, substr string) bool
  strings.Index(s, str string) int
  strings.LastIndex(s, str string) int
  strings.IndexRune(s string, r rune) int  //查询非 ASCII 编码的字符在父字符串中的位置
  trings.Replace(str, old, new, n) string
  strings.Count(s, str string) int
  strings.Repeat(s, count int) string
  strings.ToLower(s) string
  strings.ToUpper(s) string
  strings.TrimSpace(s)
  strings.Trim(s, "cut")
  strings.TrimLeft (s, "cut")
  strings.TrimRight(s, "cut")
  strings.Fields(s)
  strings.Split(s, sep)
  strings.Join(sl []string, sep string) string
  函数 strings.NewReader(str) 用于生成一个 Reader 并读取字符串中的内容，然后返回指向该 Reader 的指针
  从其它类型读取内容的函数还有：
  Read() 从 []byte 中读取内容
  ReadByte() 和 ReadRune() 从字符串中读取下一个 byte 或者 rune
  
  //  字符串转换为其它类型 strconv
  strconv.IntSize  // 平台下 int 类型所占的位数
  // type to string
  strconv.Itoa(i int) string
  strconv.FormatFloat(f float64, fmt byte, prec int, bitSize int) string
  // string to other type
  strconv.Atoi(s string) (i int, err error)
  strconv.ParseFloat(s string, bitSize int) (f float64, err error)
  
  // time and date
  pkg time
  time.Now()
  
  // pointer
  var i1 = 5
  fmt.Printf("An integer: %d, it's location in memory: %p\n", i1, &i1)
  
  var intP *int
  
  // if
  if condition1 {
  	// do something	
  } else if condition2 {
  	// do something else	
  } else {
  	// catch-all or default
  }

  // switch
  switch var1 {
  	case val1:
  		...
  	case val2:
  		...
  	default:
  		...
  }
  
  switch i {
  	case 0: fallthrough
  	case 1:
  		f() // 当 i == 0 时函数也会被调用
  }
  
  //
  package main
  
  import "fmt"
  
  func main() {
  	var num1 int = 100
  
  	switch num1 {
  	case 98, 99:
  		fmt.Println("It's equal to 98")
  	case 100: 
  		fmt.Println("It's equal to 100")
  	default:
  		fmt.Println("It's not equal to 98 or 100")
  	}
  }
  
  // 1
  switch result := calculate(); {
  case result < 0:
  	...
  case result > 0:
  	...
  default:
  	// 0
  }
  
  // 2
  switch a, b := x[i], y[j]; {
  	case a < b: t = -1
  	case a == b: t = 0
  	case a > b: t = 1
  }
  
  // for
  for i := 0; i < 5; i++ {
  	fmt.Printf("This is the %d iteration\n", i)
  }
  
  for i, j := 0, N; i < j; i, j = i+1, j-1 {}
  
  
  // 1
  var i int = 5
  
  for i >= 0 {
  	i = i - 1
  	fmt.Printf("The variable i is now: %d\n", i)
  }
  
  // 2
  for t, err = p.Token(); err == nil; t, err = p.Token() {
  	...
  }
  
  // for ... range
  for pos, char := range str {
  	...
  }
  
  // break, continue
  for {
  	i = i - 1
  	fmt.Printf("The variable i is now: %d\n", i)
  	if i < 0 {
  		break
  	}
  }
  
  for i := 0; i < 10; i++ {
  	if i == 5 {
  		continue
  	}
  	print(i)
  	print(" ")
  }
  
  // label: 以冒号（:）结尾的标识符，一般使用全部大写字母
  for, switch 或 select 语句都可以配合标签（label）形式的标识符使用
  LABEL1:
  	for i := 0; i <= 5; i++ {
  		for j := 0; j <= 5; j++ {
  			if j == 4 {
  				continue LABEL1
  			}
  			fmt.Printf("i is: %d, and j is: %d\n", i, j)
  		}
  	}
  
  // 函数（function）
  pack1.Function(arg1, arg2, …, argn)
  Go 语言的函数重载是不被允许的。这将导致一个编译错误
  
  // function signature
  func flushICache(begin, end uintptr) // implemented externally
  
  // function type
  type binOp func(int, int) int	//在这里，不需要函数体 {}
  // function is first-class value
  add := binOp
  // 函数值的比较：如果它们引用的是相同的函数或者都是 nil 的话，则认为它们是相同的函数
  // 普通函数的生命不可嵌套，匿名函数可以嵌套在普通函数中进行声明
  
  // 函数可以返回零个或多个值
  没有参数的函数通常被称为 niladic 函数（niladic function），就像 main.main()
  
  // call by value vs call by reference
  
  // 空白符（blank identifier）

  // ex1
  func main() {
  	var i1 int
  	var f1 float32
  	i1, _, f1 = ThreeValues()
  	fmt.Printf("The int: %d, the float: %f \n", i1, f1)
      
    func ThreeValues() (int, int, float32) {
  	    return 5, 6, 7.5
    }
  
  
  // 变长参数
  func myFunc(a, b, arg ...int) {}

  func Greeting(prefix string, who ...string)
  Greeting("hello:", "Joe", "Anna", "Eileen")   // who 的值为 切片 []string{"Joe", "Anna", "Eileen"}
  如果参数被存储在一个 slice 类型的变量 slice 中，则可以通过 slice... 的形式来传递参数，调用变参函数
  
  func min(s ...int) int {
  	if len(s)==0 {
  		return 0
  	}
  	min := s[0]
  	for _, v := range s {
  		if v < min {
  			min = v
  		}
  	}
  	return min
  }
  
  // defer 和追踪
  关键字 defer 允许我们推迟到函数返回之前（或任意位置执行 return 语句之后）一刻才执行某个语句或函数
  关键字 defer 的用法类似于面向对象编程语言 Java 和 C 的 finally 语句块，它一般用于释放某些已分配的资源
  多个 defer 行为被注册时，它们会以逆序执行（类似栈，即后进先出）
  
  func function1() {
  	fmt.Printf("In function1 at the top\n")
  	defer function2()
  	fmt.Printf("In function1 at the bottom!\n")
  }
  
  func function2() {
  	fmt.Printf("Function2: Deferred until the end of the calling function!")
  }
  
  关键字 defer 允许我们进行一些函数执行完成后的收尾工作，例如：
  defer file.Close()    // 关闭文件流
  
  mu.Lock()  
  defer mu.Unlock()     // 解锁一个加锁的资源
  
  printHeader()         // 打印最终报告
  defer printFooter()
  
  // open a database connection  
  defer disconnectFromDB()  // 关闭数据库链接
  
  
  // e.g
  func trace(s string) { fmt.Println("entering:", s) }
  func untrace(s string) { fmt.Println("leaving:", s) }
  // ex1
  package main
  
  import "fmt"
  
  func trace(s string)   { fmt.Println("entering:", s) }
  func untrace(s string) { fmt.Println("leaving:", s) }
  
  func a() {
  	trace("a")
  	defer untrace("a")
  	fmt.Println("in a")
  }
  
  func b() {
  	trace("b")
  	defer untrace("b")
  	fmt.Println("in b")
  	a()
  }
  
  func main() {
  	b()
  }

  // e.g improved
  package main
  
  import "fmt"
  
  func trace(s string) string {
  	fmt.Println("entering:", s)
  	return s
  }
  
  func un(s string) {
  	fmt.Println("leaving:", s)
  }
  
  func a() {
  	defer un(trace("a"))
  	fmt.Println("in a")
  }
  
  func b() {
  	defer un(trace("b"))
  	fmt.Println("in b")
  	a()
  }
  
  func main() {
  	b()
  }
  
  // 使用 defer 语句来记录函数的参数与返回值
  package main
  
  import (
  	"io"
  	"log"
  )
  
  func func1(s string) (n int, err error) {
  	defer func() {
  		log.Printf("func1(%q) = %d, %v", s, n, err)
  	}()
  	return 7, io.EOF
  }
  
  func main() {
  	func1("Go")
  }
  
  // 内置函数
  close
  len、cap	len
  new、make	new 和 make 用于分配内存
  copy、append	用于复制和连接切片
  panic、recover
  print、println
  complex、real imag
  
  // 递归函数
  
  // 将函数作为参数

  // 闭包
  fplus := func(x, y int) int { return x + y }
  fplus(3,4)
  
  func(x, y int) int { return x + y } (3, 4)  // 直接对匿名函数进行调用
  
  // 应用闭包：将函数作为返回值
  func Add2() func(b int) int {
  	return func(b int) int {
  		return b + 2
  	}
  }
  
  func Adder(a int) func(b int) int {
  	return func(b int) int {
  		return a + b
  	}
  }
  
  // make an Add2 function, give it a name p2, and call it:
  p2 := Add2()
  fmt.Printf("Call Add2 for 3 gives: %v\n", p2(3))
  // make a special Adder function, a gets value 2:
  TwoAdder := Adder(2)
  fmt.Printf("The result is: %v\n", TwoAdder(3))
  
  略微不同的实现:
  func Adder() func(int) int {
  	var x int
  	return func(delta int) int {
  		x += delta
  		return x
  	}
  }
  
  var f = Adder()
  fmt.Print(f(1), " - ")
  fmt.Print(f(20), " - ")
  fmt.Print(f(300))
  
  // 使用闭包调试
  where := func() {
  	_, file, line, _ := runtime.Caller(1)
  	log.Printf("%s:%d", file, line)
  }
  where()
  // some code
  where()
  // some more code
  where()

  // 通过设置 log 包中的 flag 参数来实现
  log.SetFlags(log.Llongfile)
  log.Print("")
  更加简短版本的 where 函数:
  var where = log.Print
  func func1() {
  where()
  ... some code
  where()
  ... some code
  where()
  }
  
  // 计算函数执行时间
  用 time 包中的 Now() 和 Sub 函数：
  start := time.Now()
  longCalculation()
  end := time.Now()
  delta := end.Sub(start)
  fmt.Printf("longCalculation took this amount of time: %s\n", delta)
  
  // 通过内存缓存来提升性能
  当在进行大量的计算时，提升性能最直接有效的一种方式就是避免重复计算。通过在内存中缓存和重复利用相同计算的结果，称之为内存缓存。
  内存缓存的技术在使用计算成本相对昂贵的函数时非常有用（不仅限于例子中的递归），譬如大量进行相同参数的运算。这种技术还可以应用于纯函数中，即相同输入必定获得相同输出的函数
  package main
  
  import (
  	"fmt"
  	"time"
  )
  
  const LIM = 41
  
  var fibs [LIM]uint64
  
  func main() {
  	var result uint64 = 0
  	start := time.Now()
  	for i := 0; i < LIM; i++ {
  		result = fibonacci(i)
  		fmt.Printf("fibonacci(%d) is: %d\n", i, result)
  	}
  	end := time.Now()
  	delta := end.Sub(start)
  	fmt.Printf("longCalculation took this amount of time: %s\n", delta)
  }
  func fibonacci(n int) (res uint64) {
  	// memoization: check if fibonacci(n) is already known in array:
  	if fibs[n] != 0 {
  		res = fibs[n]
  		return
  	}
  	if n <= 1 {
  		res = 1
  	} else {
  		res = fibonacci(n-1) + fibonacci(n-2)
  	}
  	fibs[n] = res
  	return
  }
  
  // 数组与切片
  var identifier [len]type
  
  // 通过数组常量的方法来初始化数组
  var arrAge = [5]int{18, 20, 15, 22, 16}   // 1
  var arrLazy = [...]int{5, 6, 7, 8, 22}    // 2
  var arrKeyValue = [5]string{3: "Chris", 4: "Ron"} // 3
  
  // e.g
  type Vector3D [3]float32
  var vec Vector3D
  
  // e.g
  package main
  const (
  	WIDTH  = 1920
  	HEIGHT = 1080
  )
  
  type pixel int
  var screen [WIDTH][HEIGHT]pixel
  
  func main() {
  	for y := 0; y < HEIGHT; y++ {
  		for x := 0; x < WIDTH; x++ {
  			screen[x][y] = 0
  		}
  	}
  }
  
  // 将数组传递给函数: 1.传递数组的指针 2.使用切片(更通用)
  package main
  import "fmt"
  
  func main() {
  	array := [3]float64{7.0, 8.5, 9.1}
  	x := Sum(&array) // Note the explicit address-of operator
  	// to pass a pointer to the array
  	fmt.Printf("The sum of the array is: %f", x)
  }
  
  func Sum(a *[3]float64) (sum float64) {
  	for _, v := range a { // derefencing *a to get back to the array is not necessary!
  		sum += v
  	}
  	return
  }
  
  // 切片(https://go.timpaik.top/07.2.html// _7-2-%E5%88%87%E7%89%87)
  var identifier []type
  
  var slice1 []type = arr1[start:end]
  
  arr1[2:]
  arr1[2:len(arr1)]

  arr1[:3]
  arr1[0:3]

  slice1 = slice1[:len(slice1)-1]。
  
  s == s[:i] + s[i:] // i是一个整数且: 0 <= i <= len(s)
  len(s) <= cap(s)
  
  
  // 切片作为函数参数
  func sum(a []int) int {
  	s := 0
  	for i := 0; i < len(a); i++ {
  		s += a[i]
  	}
  	return s
  }
  
  func main() {
  	var arr = [5]int{0, 1, 2, 3, 4}
  	sum(arr[:])
  }
  
  // 创建切片
  var slice1 []type = make([]type, len)
  slice1 := make([]type, len)

  s2 := make([]int, 10) // cap(s2) == len(s2) == 10
  
  
  //  new() 和 make() 的区别
  都在堆上分配内存，但是它们的行为不同，适用于不同的类型
  1.new(T) 适用于值类型如数组和结构体，相当于 &T{}。
  2.make(T) 返回一个类型为 T 的初始值，适用于3种内建的引用类型：切片、map 和 channel
  换言之，new 函数分配内存，make 函数初始化；
  
  // 多维 切片
  //  bytes 包
  类型 []byte 的切片十分常见，Go 语言有一个 bytes 包专门用来解决这种类型的操作方法
  bytes 包和字符串包十分类似。而且它还包含一个十分有用的类型 Buffer:
  import "bytes"
  
  type Buffer struct {
  	...
  }
  这是一个长度可变的 bytes 的 buffer，提供 Read 和 Write 方法，因为读写长度未知的 bytes 最好使用 buffer。
  
  Buffer 可以这样定义：var buffer bytes.Buffer。
  或者使用 new 获得一个指针：var r *bytes.Buffer = new(bytes.Buffer)。
  或者通过函数：func NewBuffer(buf []byte) *Buffer，创建一个 Buffer 对象并且用 buf 初始化好；NewBuffer 最好用在从 buf 读取的时候使用。
  
  通过 buffer 串联字符串
  示例：创建一个 buffer，通过 buffer.WriteString(s) 方法将字符串 s 追加到后面，最后再通过 buffer.String() 方法转换为 string：
  var buffer bytes.Buffer
  for {
  	if s, ok := getNextString(); ok { //method getNextString() not shown here
  		buffer.WriteString(s)
  	} else {
  		break
  	}
  }
  fmt.Print(buffer.String(), "\n")
  这种实现方式比使用 += 要更节省内存和 CPU，尤其是要串联的字符串数目特别多的时候
  
  // for-range 结构
  // 应用于数组和切片
  for ix, value := range slice1 {
  	...
  }

  // 多维切片下的 for-range
  for row := range screen {
  	for column := range screen[row] {
  		screen[row][column] = 1
  	}
  }
  
  // 切片重组（reslice）
  slice1 := make([]type, start_length, capacity)
  这么做的好处是我们的切片在达到容量上限后可以扩容。改变切片长度的过程称之为切片重组 reslicing，做法如下：slice1 = slice1[0:end]，其中 end 是新的末尾索引（即长度）
  将切片扩展 1 位可以这么做：
  sl = sl[0:len(sl)+1]
  
  // 切片的复制与追加
  如果想增加切片的容量，我们必须创建一个新的更大的切片并把原分片的内容都拷贝过来。
  
  // 字符串、数组和切片的应用
  从字符串生成字节切片
  假设 s 是一个字符串（本质上是一个字节数组），那么就可以直接通过 c := []byte(s) 来获取一个字节的切片 c。另外，您还可以通过 copy 函数来达到相同的目的：copy(dst []byte, src string)
  同样的，还可以使用 for-range 来获得每个元素:
  package main
  
  import "fmt"
  
  func main() {
      s := "\u00ff\u754c"
      for i, c := range s {
          fmt.Printf("%d:%c ", i, c)
      }
  }
  
  // 获取字符串的某一部分
  substr := str[start:end]
  str[start:]
  str[:end]
  
  // 字符串和切片的内存结构：指针 + 长度
  
  // 修改字符串中的某个字符
  Go 语言中的字符串是不可变的
  需要先将字符串转换成字节数组，再修改数组中的元素值，最后将字节数组转换回字符串
  s := "hello"
  c := []byte(s)
  c[0] = 'c'
  s2 := string(c) // s2 == "cello"
  
  // 字节数组对比函数
  示例：Compare 函数会返回两个字节数组字典顺序的整数对比结果
  func Compare(a, b[]byte) int {
  	for i:=0; i < len(a) && i < len(b); i++ {
  	    switch {
  	    case a[i] > b[i]:
  		return 1
  	    case a[i] < b[i]:
  		return -1
  	    }
  	}
  	// 数组的长度可能不同
  	switch {
  	case len(a) < len(b):
  	    return -1
  	case len(a) > len(b):
  	    return 1
  	}
  	return 0 // 数组相等
  }
  
  // 搜索及排序切片和数组
  // append 函数常见操作
  a = append(a, b...) // 将切片 b 的元素追加到切片 a 之后
  a = append(a[:i], a[i+1:]...)   // 删除位于索引 i 的元素
  a = append(a[:i], a[j:]...) // 切除切片 a 中从索引 i 至 j 位置的元素
  a = append(a, make([]T, j)...)  // 为切片 a 扩展 j 个元素长度
  a = append(a[:i], append([]T{x}, a[i:]...)...)  // 在索引 i 的位置插入元素 x
  a = append(a[:i], append(make([]T, j), a[i:]...)...)    // 在索引 i 的位置插入长度为 j 的新切片
  a = append(a[:i], append(b, a[i:]...)...)   // 在索引 i 的位置插入切片 b 的所有元素
  x, a = a[len(a)-1], a[:len(a)-1]    // 取出位于切片 a 最末尾的元素 x
  a = append(a, x)    // 将元素 x 追加到切片 a
  
  //  Map
  map 是引用类型，是元素对的无序集合，也称为关联数组或字典,内存用 make 方法来分配
  var map1 map[keytype]valuetype
  var map1 map[string]int
  
  keytype 是任意可以用 == 或者 != 操作符比较的类型 string、int、float、指针、接口类型。如果要用结构体作为 key 可以提供 Key() 和 Hash() 方法，通过结构体的域计算出唯一的数字或者字符串的 key
  value 可以是任意类型的；通过使用空接口类型可以存储任意值，但是使用时需要先做一次类型断言
  map 传递给函数的代价很小，通过 key 在 map 中寻找值是很快的，比线性查找快得多，但是仍然比从数组和切片的索引中直接读取要慢 100 倍
  map 也可以用函数作为自己的值，即通过 key 用来选择要执行的函数
  v := map1[key1] // 将 key1 对应的值赋值给 v；如果 map 中没有 key1 存在，那么 v 将被赋值为 map1 的值类型的空值
  package main
  import "fmt"
  
  func main() {
  	var mapLit map[string]int
  	//var mapCreated map[string]float32
  	var mapAssigned map[string]int
  
  	mapLit = map[string]int{"one": 1, "two": 2}
  	mapCreated := make(map[string]float32)
  	mapAssigned = mapLit
  
  	mapCreated["key1"] = 4.5
  	mapCreated["key2"] = 3.14159
  	mapAssigned["two"] = 3
  
  	fmt.Printf("Map literal at \"one\" is: %d\n", mapLit["one"])
  	fmt.Printf("Map created at \"key2\" is: %f\n", mapCreated["key2"])
  	fmt.Printf("Map assigned at \"two\" is: %d\n", mapLit["two"])
  	fmt.Printf("Map literal at \"ten\" is: %d\n", mapLit["ten"])
  }
  
  var map1 = make(map[keytype]valuetype)
  map1 := make(map[keytype]valuetype)   // 永远用 make 来构造 map，如果你错误的使用 new() 分配了一个引用对象，你会获得一个空引用的指针，相当于声明了一个未初始化的变量并且取了它的地址
    
  // map 容量的动态增长
  make(map[keytype]valuetype, cap)
  map2 := make(map[string]float32, 100) // 出于性能的考虑，即使只是大概知道容量，也最好先标明
  ```