- ```go
  #包(package)
  每个 Go 文件都属于且仅属于一个包。一个包可以由许多以 .go 为扩展名的源文件组成,必须在源文件中非注释的第一行
  指明这个文件属于哪个包，如：package main, package main表示一个可独立执行的程序，每个 Go 应用程序都包含一
  个名为 main 的包, 所有的包名都应该使用小写字母
  
  Go 的标准库包含了大量的包（如：fmt 和 os），用户也可以创建自己的包, 应用程序的包的依赖关系决定了其构建顺序。
  属于同一个包的源文件必须全部被一起编译，一个包即是编译时的一个单元，因此根据惯例，每个目录都只包含一个包。
  如果对一个包进行更改或重新编译，所有引用了这个包的客户端程序都必须全部重新编译。
  
  一个 Go 程序是通过 import 关键字将一组包链接在一起
  #1
  import "fmt"
  import "os"
  
  #2
  import (
     "fmt"
     "os"
  )
  
  #3
  import fm "fmt" // alias3
  
  在导入一个外部包后，能够且只能够访问该包中导出的对象
  当标识符（包括常量、变量、类型、函数名、结构字段等等）以一个大写字母开头，这种形式的标识符的对象就可以被
  外部包的代码所使用（客户端程序需要先导入这个包），这被称为导出（像面向对象语言中的 public）；标识符如果
  以小写字母开头，则对包外是不可见的，但是他们在整个包的内部是可见并且可用的（像面向对象语言中的 private ）
  如果你导入了一个包却没有使用它，则会在构建程序时引发错误
  
  # 函数
  func functionName(parameter_list) (return_value_list) {
     …
  }
  左大括号 { 必须与方法的声明放在同一行，这是编译器的强制规定
  parameter_list 的形式为 (param1 type1, param2 type2, …)
  return_value_list 的形式为 (ret1 type1, ret2 type2, …)
  
  # 注释
  #类型
  变量（或常量）包含数据，这些数据可以有不同的数据类型，简称类型，使用 var 声明的变量的值会自动初始化为该类型的零值。类型定义了某个变量的值的集合与可对其进行操作的集合。
  类型可以是基本类型，如：int、float、bool、string；结构化的（复合的），如：struct、array、slice、map、channel；只描述类型的行为的，如：interface
  
  使用 type 关键字可以定义你自己的类型
  如果你有多个类型需要定义，可以使用因式分解关键字的方式
  type (
     IZ int
     FZ float64
     STR string
  )
  
  # Go 程序的一般结构
  1.在完成包的 import 之后，开始对常量、变量和类型的定义或声明。
  2.如果存在 init 函数的话，则对该函数进行定义（这是一个特殊的函数，每个含有该函数的包都会首先执行这个函数）。
  3.如果当前包是 main 包，则定义 main 函数。
  4.然后定义其余的函数，首先是类型的方法，接着是按照 main 函数中先后调用的顺序来定义相关函数，如果有很多函数，则可以按照字母顺序来进行排序。
  
  package main
  
  import (
     "fmt"
  )
  
  const c = "C"
  
  var v int = 5
  
  type T struct{}
  
  func init() { // initialization of package
  }
  
  func main() {
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
  
  Go 程序的执行（程序启动）顺序如下：
  1.按顺序导入所有被 main 包引用的其它包，然后在每个包中执行如下流程：
  2.如果该包又导入了其它的包，则从第一步开始递归执行，但是每个包只会被导入一次。
  3.然后以相反的顺序在每个包中初始化常量和变量，如果该包含有 init 函数的话，则调用该函数。
  4.在完成这一切之后，main 也执行同样的过程，最后调用 main 函数开始执行程序
  
  
  # 类型转换
  Go 语言不存在隐式类型转换，因此所有的转换都必须显式说明
  valueOfTypeB = typeB(valueOfTypeA)
  
  //声明并定义变量（类型转换）
  a := 5.0
  b := int(a)
  
  #Go 命名规范
  通过 gofmt 来强制实现统一的代码风格
  
  #常量
  存储在常量中的数据类型只可以是布尔型、数字型（整数型、浮点型和复数）和字符串型
  const identifier [type] = value
  const Pi = 3.14159
  在 Go 语言中，你可以省略类型说明符 [type]，因为编译器可以根据变量的值来推断其类型
  显式类型定义： const b string = "abc"
  隐式类型定义： const b = "abc"
  未定义类型的常量会在必要时刻根据上下文来获得相关类型
  常量也允许使用并行赋值的形式
  
  iota 可以被用作枚举值：
  // 赋值一个常量时，之后没赋值的常量都会应用上一行的赋值表达式
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
  第一个 iota 等于 0，每当 iota 在新的一行被使用时，它的值都会自动加 1，并且没有赋值的常量默认会应用上一行
  的赋值表达式
  
  #变量
  声明变量：
  var identifier type
  
  var a, b *int
  #1
  var a int
  var b bool
  var str string
  
  #2(因式分解关键字的写法一般用于声明全局变量)
  var (
  	a int
  	b bool
  	str string
  )
  当一个变量被声明之后，系统自动赋予它该类型的零值: int 为 0，float 为 0.0，bool 为 false，string 为空字
  符串，指针为 nil
  如果你的全局变量希望能够被外部包所使用，则需要将首个单词的首字母也大写(可见性规则)
  
  #作用域
  变量在函数体外声明，则被认为是全局变量，可以在整个包甚至外部包（被导出后）使用，不管你声明在哪个源文件里或
  在哪个源文件里调用该变量
  在函数体内声明的变量称之为局部变量，它们的作用域只在函数体内，参数和返回值变量也是局部变量。
  在某个代码块的内层代码块中使用相同名称的变量，则此时外部的同名变量将会暂时隐藏
  
  变量的声明与赋值
  var identifier [type] = value
  var a int = 15
  var i = 5
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
  这种写法主要用于声明包级别的全局变量
  在函数体内声明局部变量时，应使用简短声明语法 :=，例如 a := 1
  
  
  Go 语言中的引用类型： 指针，slices，maps，channel，被引用的变量会存储在堆中，以便进行垃圾回收，且比栈拥
  有更大的内存空间
  
  函数 Printf 可以在 fmt 包外部使用，这是因为它以大写字母 P 开头，该函数主要用于打印输出到控制台
  函数 fmt.Sprintf 与 Printf 的作用是完全相同的，不过前者将格式化后的字符串以返回值的形式返回给调用者
  
  := 赋值操作符(初始化声明)
  a := 50 或 b := false，a 和 b 的类型（int 和 bool）将由编译器自动推断
  这是使用变量的首选形式，但是它只能被用在函数体内，而不可以用于全局变量的声明与赋值。使用操作符 := 可以高效
  地创建一个新的变量，称之为初始化声明
  
  
  空白标识符 _ 也被用于抛弃值，如值 5 在：_, b = 5, 7 中被抛弃
  _ 实际上是一个只写变量，你不能得到它的值。这样做是因为 Go 语言中你必须使用所有被声明的变量，但有时你并不需
  要使用从一个函数得到的所有返回值
  并行赋值也被用于当一个函数返回多个返回值时，比如这里的 val 和错误 err 是通过调用 Func1 函数同时
  得到：val, err = Func1(var1)
  
  # init函数
  init 函数是一类非常特殊的函数，它不能够被人为调用，而是在每个包完成初始化后自动执行，并且执行优先级比 main 函数高，每个源文件都只能包含一个 init 函数。初始化总是以单线程执行，并且按照包的依赖关系顺序执行，一个可能的用途是在开始执行程序之前对数据进行检验或修复，以保证程序状态的正确性，init 函数也经常被用在当一个程序开始之前调用后台执行的 goroutine
  
  #布尔类型 bool
  布尔型的值只可以是常量 true 或者 false
  两个类型相同的值可以使用相等 == 或者不等 != 运算符来进行比较并获得一个布尔型的值
  布尔型的常量和变量也可以通过和逻辑运算符（非 !、和 &&、或 ||）结合来产生另外一个布尔值
  对于布尔值的好的命名能够很好地提升代码的可读性，例如以 is 或者 Is 开头的 isSorted、isFinished、isVisible
  
  #数字类型
  整数：
  int8（-128 -> 127）
  int16（-32768 -> 32767）
  int32（-2,147,483,648 -> 2,147,483,647）
  int64（-9,223,372,036,854,775,808 -> 9,223,372,036,854,775,807）
  
  无符号整数：
  uint8（0 -> 255）
  uint16（0 -> 65,535）
  uint32（0 -> 4,294,967,295）
  uint64（0 -> 18,446,744,073,709,551,615）
  
  浮点型（IEEE-754 标准）：
  float32（+- 1e-45 -> +- 3.4 * 1e38）
  float64（+- 5 * 1e-324 -> 107 * 1e308）
  
  格式化说明符
  %d 用于格式化整数（%x 和 %X 用于格式化 16 进制表示的数字）
  %g 用于格式化浮点型（%f 输出浮点数，%e 输出科学计数表示法）
  %0nd 用于规定输出长度为n的整数，其中开头的数字 0 是必须的
  %n.mg 用于表示数字 n 并精确到小数点后 m 位，除了使用 g 之外，还可以使用 e 或者 f
  
  #复数
  Go 拥有以下复数类型：
  complex64 (32 位实数和虚数)
  complex128 (64 位实数和虚数)
  
  #位运算
  按位与 &
  按位或 |
  按位异或 ^
  位清除 &^
  按位补足 ^
  位左移 <<
  位右移 >>
  
  使用位左移与 iota 计数配合可优雅地实现存储单位的常量枚举：
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
  在通讯中使用位左移表示标识的用例
  type BitFlag int
  const (
  	Active BitFlag = 1 << iota // 1 << 0 == 1
  	Send // 1 << 1 == 2
  	Receive // 1 << 2 == 4
  )
  
  flag := Active | Send // == 3
  
  
  # 随机数
  函数 rand.Float32 和 rand.Float64 返回介于 [0.0, 1.0) 之间的伪随机数，其中包括 0.0 但不包括 1.0。函数 
  rand.Intn 返回介于 [0, n) 之间的伪随机数
  rand.Seed(value) 函数来提供伪随机数的生成种子
  
  #类型别名
  例如：
  type TZ int
  
  包 unicode 包含了一些针对测试字符的非常有用的函数（其中 ch 代表字符）：
  判断是否为字母：unicode.IsLetter(ch)
  判断是否为数字：unicode.IsDigit(ch)
  判断是否为空白符号：unicode.IsSpace(ch)
  
  #字符串
  Go 支持以下 2 种形式的字面值：
  解释字符串：
  该类字符串使用双引号括起来，其中的相关的转义字符将被替换，这些转义字符包括
  \n：换行符
  \r：回车符
  \t：tab 键
  \u 或 \U：Unicode 字符
  \\：反斜杠自身
  
  非解释字符串
  该类字符串使用反引号括起来，支持换行，例如
  
  和 C/C++不一样，Go 中的字符串是根据长度限定，而非特殊字符\0
  
  字符串的内容（纯字节）可以通过标准索引法来获取，在中括号 [] 内写入索引，索引从 0 开始计数，需要注意的是，这种转换方案只对纯 ASCII 码的字符串有效
  
  # 字符串拼接符 +
  str := "Beginning of the string " +
  	"second part of the string"
  
  s := "hel" + "lo,"
  s += "world!"
  fmt.Println(s) //输出 “hello, world!”
  
  在循环中使用加号 + 拼接字符串并不是最高效的做法，更好的办法是使用函数 strings.Join()，更好的办法是使用字节缓冲（bytes.Buffer）拼接
  
  
  #strings 和 strconv 包
  Go 中使用 strings 包来完成对字符串的主要操作
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
  
  # 字符串与其它类型的转换
  与字符串相关的类型转换都是通过 strconv 包实现的
  获取程序运行的操作系统平台下 int 类型所占的位数:strconv.IntSize
  数字类型转换到字符串:
  strconv.Itoa(i int) string
  strconv.FormatFloat(f float64, fmt byte, prec int, bitSize int) string
  从字符串类型转换为数字类型
  strconv.Atoi(s string) (i int, err error)
  strconv.ParseFloat(s string, bitSize int) (f float64, err error)
  
  #时间和日期
  pkg time
  
  time.Now()
  ...
  
  #指针
  Go 语言为程序员提供了控制数据结构的指针的能力，但是不能进行指针运算
  
  var i1 = 5
  fmt.Printf("An integer: %d, it's location in memory: %p\n", i1, &i1)
  
  var intP *int
  
  一个指针变量可以指向任何一个值的内存地址 它指向那个值的内存地址，在 32 位机器上占用 4 个字节，在 64 位机器上占用 8 个字节，并且与它所指向的值的大小无关，不能获取字面量或常量的地址
  当一个指针被定义后没有分配到任何变量时，它的值为 nil
  在书写表达式类似 var p *type 时，切记在 * 号和指针名称间留有一个空格
  
  指针的一个高级应用是你可以传递一个变量的引用（如函数的参数），这样不会传递变量的拷贝
  对一个空指针的反向引用是不合法的，并且会使程序崩溃
  
  #控制结构
  #if
  if condition1 {
  	// do something	
  } else if condition2 {
  	// do something else	
  } else {
  	// catch-all or default
  }
  Go 语言的函数经常使用两个返回值来表示执行是否成功：返回某个值以及 true 表示成功；返回零值（或 nil）和 false 表示失败
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
  
  switch result := calculate(); {
  case result < 0:
  	...
  case result > 0:
  	...
  default:
  	// 0
  }
  
  switch a, b := x[i], y[j]; {
  	case a < b: t = -1
  	case a == b: t = 0
  	case a > b: t = 1
  }
  
  #for
  for 初始化语句; 条件语句; 修饰语句 {}
  for i := 0; i < 5; i++ {
  	fmt.Printf("This is the %d iteration\n", i)
  }
  
  for i, j := 0, N; i < j; i, j = i+1, j-1 {}
  
  
  # ex1
  var i int = 5
  
  for i >= 0 {
  	i = i - 1
  	fmt.Printf("The variable i is now: %d\n", i)
  }
  
  #ex2 无限循环
  for t, err = p.Token(); err == nil; t, err = p.Token() {
  	...
  }
  
  #for-range 结构
  它可以迭代任何一个集合(包括数组和 map)
  for pos, char := range str {
  	...
  }
  
  
  #break与continue
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
  
  #标签与 goto
  for、switch 或 select 语句都可以配合标签（label）形式的标识符使用，即某一行第一个以冒号（:）结尾的单词（gofmt 会将后续代码自动移至下一行）
  （标签的名称是大小写敏感的，为了提升可读性，一般建议使用全部大写字母）
  LABEL1:
  	for i := 0; i <= 5; i++ {
  		for j := 0; j <= 5; j++ {
  			if j == 4 {
  				continue LABEL1
  			}
  			fmt.Printf("i is: %d, and j is: %d\n", i, j)
  		}
  	}
  特别注意 使用标签和 goto 语句是不被鼓励的：它们会很快导致非常糟糕的程序设计，而且总有更加可读的替代方案来实现相同的需求
  
  #函数（function）
  函数是基本的代码块
  Go是编译型语言，所以函数编写的顺序是无关紧要的；鉴于可读性的需求，最好把 main() 函数写在文件的前面，其他函数按照一定逻辑顺序进行编写（例如函数被调用的顺序）
  Go 里面有三种类型的函数：
  1.普通的带有名字的函数
  2.匿名函数或者lambda函数
  3.方法
  除了main()、init()函数外，其它所有类型的函数都可以有参数与返回值。函数参数、返回值以及它们的类型被统称为函数签名。
  
  不正确的 Go 代码：
  func g()
  {
  }
  正确的 Go 代码：
  func g() {
  }
  
  函数被调用的基本格式：
  pack1.Function(arg1, arg2, …, argn)
  函数可以将其他函数调用作为它的参数，只要这个被调用函数的返回值个数、返回值类型和返回值的顺序与调用函数所需求的实参是一致的
  假设 f1 需要 3 个参数 f1(a, b, c int)，同时 f2 返回 3 个参数 f2(a, b int) (int, int, int)，就可以这样调用 f1：f1(f2(a, b))
  函数重载（function overloading）指的是可以编写多个同名函数，只要它们拥有不同的形参与/或者不同的返回值，在 Go 里面函数重载是不被允许的。这将导致一个编译错误
  
  如果需要申明一个在外部定义的函数，你只需要给出函数名与函数签名，不需要给出函数体：
  func flushICache(begin, end uintptr) // implemented externally
  
  函数也可以以申明的方式被使用，作为一个函数类型，就像：
  type binOp func(int, int) int	//在这里，不需要函数体 {}
  函数是一等值（first-class value）：它们可以赋值给变量，就像 add := binOp 一样
  函数值（functions value）之间可以相互比较：如果它们引用的是相同的函数或者都是 nil 的话，则认为它们是相同的函数
  函数不能在其它函数里面声明（不能嵌套），不过我们可以通过使用匿名函数来破除这个限制
  
  #函数参数与返回值
  函数能够接收参数供自己使用，也可以返回零个或多个值（我们通常把返回多个值称为返回一组值）
  函数定义时，它的形参一般是有名字的，不过我们也可以定义没有形参名的函数，只有相应的形参类型，就像这样：func f(int, int, float64)，没有参数的函数通常被称为 niladic 函数（niladic function），就像 main.main()
  
  #按值传递（call by value） 按引用传递（call by reference）
  指针也是变量类型，有自己的地址和值，通常指针的值指向一个变量的地址。所以，按引用传递也是按值传递
  在函数调用时，像切片（slice）、字典（map）、接口（interface）、通道（channel）这样的引用类型都是默认使用引用传递（即使没有显式的指出指针）
  
  如果一个函数需要返回四到五个值，我们可以传递一个切片给函数（如果返回值具有相同类型）或者是传递一个结构体（如果返回值具有不同的类型）。因为传递一个指针允许直接修改变量的值，消耗也更少
  
  任何一个非命名返回值（使用非命名返回值是很糟的编程习惯）在 return 语句里面都要明确指出包含返回值的变量或是一个可计算的值
  尽量使用命名返回值：会使代码更清晰、更简短，同时更加容易读懂
  
  #空白符（blank identifier）
  空白符用来匹配一些不需要的值，然后丢弃掉
  #ex1
  func main() {
  	var i1 int
  	var f1 float32
  	i1, _, f1 = ThreeValues()
  	fmt.Printf("The int: %d, the float: %f \n", i1, f1)
      }
      
      func ThreeValues() (int, int, float32) {
  	return 5, 6, 7.5
      }
  
  #改变外部变量（outside variable）
  传递指针给函数不但可以节省内存（因为没有复制变量的值），而且赋予了函数直接修改外部变量的能力，所以被修改的变量不再需要使用 return 返回
  当需要在函数内改变一个占用内存比较大的变量时，性能优势就更加明显了,然而，如果不小心使用的话，传递一个指针很容易引发一些不确定的事
  
  #传递变长参数
  如果函数的最后一个参数是采用 ...type 的形式，那么这个函数就可以处理一个变长的参数，这个长度可以为 0，这样的函数称为变参函数
  func myFunc(a, b, arg ...int) {}
  这个函数接受一个类似某个类型的 slice 的参数,
  该参数可以通过第 5.4.4 节中提到的 for 循环结构迭代
  示例：
  func Greeting(prefix string, who ...string)
  Greeting("hello:", "Joe", "Anna", "Eileen")
  在 Greeting 函数中，变量 who 的值为 []string{"Joe", "Anna", "Eileen"}
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
  
  #defer 和追踪
  关键字 defer 允许我们推迟到函数返回之前（或任意位置执行 return 语句之后）一刻才执行某个语句或函数（为什么要在返回之后才执行这些语句？因为 return 语句同样可以包含一些操作，而不是单纯地返回某个值）
  关键字 defer 的用法类似于面向对象编程语言 Java 和 C# 的 finally 语句块，它一般用于释放某些已分配的资源
  
  func function1() {
  	fmt.Printf("In function1 at the top\n")
  	defer function2()
  	fmt.Printf("In function1 at the bottom!\n")
  }
  
  func function2() {
  	fmt.Printf("Function2: Deferred until the end of the calling function!")
  }
  
  output:
  In Function1 at the top
  In Function1 at the bottom!
  Function2: Deferred until the end of the calling function!
  当有多个 defer 行为被注册时，它们会以逆序执行（类似栈，即后进先出）
  
  关键字 defer 允许我们进行一些函数执行完成后的收尾工作，例如：
  1.关闭文件流
  defer file.Close()
  2.解锁一个加锁的资源 
  mu.Lock()  
  defer mu.Unlock() 
  3.打印最终报告
  printHeader()  
  defer printFooter()
  4.关闭数据库链接
  // open a database connection  
  defer disconnectFromDB()
  合理使用 defer 语句能够使得代码更加简洁
  
  #ex1
  package main
  
  import "fmt"
  
  func main() {
  	doDBOperations()
  }
  
  func connectToDB() {
  	fmt.Println("ok, connected to db")
  }
  
  func disconnectFromDB() {
  	fmt.Println("ok, disconnected from db")
  }
  
  func doDBOperations() {
  	connectToDB()
  	fmt.Println("Defering the database disconnect.")
  	defer disconnectFromDB() //function called here with defer
  	fmt.Println("Doing some DB operations ...")
  	fmt.Println("Oops! some crash or network error ...")
  	fmt.Println("Returning from function here!")
  	return //terminate the program
  	// deferred function executed here just before actually returning, even if
  	// there is a return or abnormal termination before
  }
  
  
  使用 defer 语句实现代码追踪
  一个基础但十分实用的实现代码执行追踪的方案就是在进入和离开某个函数打印相关的消息，即可以提炼为下面两个函数
  func trace(s string) { fmt.Println("entering:", s) }
  func untrace(s string) { fmt.Println("leaving:", s) }
  
  #ex1
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
  更加简便的版本:
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
  
  使用 defer 语句来记录函数的参数与返回值
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
  
  #内置函数
  Go 语言拥有一些不需要进行导入操作就可以使用的内置函数。
  close	用于管道通信
  len、cap	len 用于返回某个类型的长度或数量（字符串、数组、切片、map 和管道）；cap 是容量的意思，用于返回某个类型的最大容量（只能用于切片和 map）
  new、make	new 和 make 均是用于分配内存：new 用于值类型和用户定义的类型，如自定义结构，make 用于内置引用类型（切片、map 和管道）。它们的用法就像是函数，但是将类型作为参数：new(type)、make(type)。new(T) 分配类型 T 的零值并返回其地址，也就是指向类型 T 的指针（详见第 10.1 节）。它也可以被用于基本类型：v := new(int)。make(T) 返回类型 T 的初始化之后的值，因此它比 new 进行更多的工作（详见第 7.2.3/4 节、第 8.1.1 节和第 14.2.1 节）new() 是一个函数，不要忘记它的括号
  copy、append	用于复制和连接切片
  panic、recover	两者均用于错误处理机制
  print、println	底层打印函数（详见第 4.2 节），在部署环境中建议使用 fmt 包
  complex、real imag	用于创建和操作复数（详见第 4.5.2.2 节）
  
  #递归函数
  当一个函数在其函数体内调用自身，则称之为递归
  func fibonacci(n int) (res int) {
  	if n <= 1 {
  		res = 1
  	} else {
  		res = fibonacci(n-1) + fibonacci(n-2)
  	}
  	return
  }
  在使用递归函数时经常会遇到的一个重要问题就是栈溢出：一般出现在大量的递归调用导致的程序栈内存分配耗尽。这个问题可以通过一个名为懒惰求值 (opens new window)的技术解决，在 Go 语言中，我们可以使用管道（channel）和 goroutine（详见第 14.8 节）来实现
  
  Go 语言中也可以使用相互调用的递归函数：多个函数之间相互调用形成闭环。因为 Go 语言编译器的特殊性，这些函数的声明顺序可以是任意的
  func even(nr int) bool {
  	if nr == 0 {
  		return true
  	}
  	return odd(RevSign(nr) - 1)
  }
  
  func odd(nr int) bool {
  	if nr == 0 {
  		return false
  	}
  	return even(RevSign(nr) - 1)
  }
  
  func RevSign(nr int) int {
  	if nr < 0 {
  		return -nr
  	}
  	return nr
  }
  
  #将函数作为参数
  函数可以作为其它函数的参数进行传递，然后在其它函数内调用执行，一般称之为回调
  func main() {
  	callback(1, Add)
  }
  
  func Add(a, b int) {
  	fmt.Printf("The sum of %d and %d is: %d\n", a, b, a+b)
  }
  
  func callback(y int, f func(int, int)) {
  	f(y, 2) // this becomes Add(1, 2)
  }
  
  #闭包
  当我们不希望给函数起名字的时候，可以使用匿名函数，这样的一个函数不能够独立存在，但可以被赋值于某个变量，即保存函数的地址到变量中：fplus := func(x, y int) int { return x + y }，然后通过变量名对函数进行调用：fplus(3,4)，可以直接对匿名函数进行调用：func(x, y int) int { return x + y } (3, 4)
  计算从 1 到 1 百万整数的总和的匿名函数：
  func() {
  	sum := 0
  	for i := 1; i <= 1e6; i++ {
  		sum += i
  	}
  }()
  
  #ex1
  func f() {
  	for i := 0; i < 4; i++ {
  		g := func(i int) { fmt.Printf("%d ", i) } //此例子中只是为了演示匿名函数可分配不同的内存地址，在现实开发中，不应该把该部分信息放置到循环中。
  		g(i)
  		fmt.Printf(" - g is of type %T and has value %v\n", g, g)
  	}
  }
  
  #defer 语句和匿名函数
  关键字 defer （详见第 6.4 节）经常配合匿名函数使用，它可以用于改变函数的命名返回值
  匿名函数还可以配合 go 关键字来作为 goroutine 使用
  匿名函数同样被称之为闭包（函数式语言的术语）：它们被允许调用定义在其它环境下的变量。闭包可使得某个函数捕捉到一些外部状态，例如：函数被创建时的状态。另一种表示方式为：一个闭包继承了函数所声明时的作用域。这种状态（作用域内的变量）都被共享到闭包的环境中，因此这些变量可以在闭包中被操作，直到被销毁，详见第 6.9 节中的示例。闭包经常被用作包装函数：它们会预先定义好 1 个或多个参数以用于包装，详见下一节中的示例。另一个不错的应用就是使用闭包来完成更加简洁的错误检查（详见第 16.10.2 节）。
  
  #应用闭包：将函数作为返回值
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
  
  #使用闭包调试
  当您在分析和调试复杂的程序时，无数个函数在不同的代码文件中相互调用，如果这时候能够准确地知道哪个文件中的具体哪个函数正在执行，对于调试是十分有帮助的。您可以使用 runtime 或 log 包中的特殊函数来实现这样的功能
  包 runtime 中的函数 Caller() 提供了相应的信息，因此可以在需要的时候实现一个 where() 闭包函数来打印函数执行的位置:
  where := func() {
  	_, file, line, _ := runtime.Caller(1)
  	log.Printf("%s:%d", file, line)
  }
  where()
  // some code
  where()
  // some more code
  where()
  也可以设置 log 包中的 flag 参数来实现：
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
  
  #计算函数执行时间
  用 time 包中的 Now() 和 Sub 函数：
  start := time.Now()
  longCalculation()
  end := time.Now()
  delta := end.Sub(start)
  fmt.Printf("longCalculation took this amount of time: %s\n", delta)
  
  #通过内存缓存来提升性能
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
  
  #数组与切片
  容器是可以包含大量条目（item）的数据结构, 例如数组、切片和 map
  数组是具有相同 唯一类型 的一组已编号且长度固定的数据项序列（这是一种同构的数据结构）；这种类型可以是任意的原始类型例如整型、字符串或者自定义类型。数组长度必须是一个常量表达式，并且必须是一个非负整数。数组长度也是数组类型的一部分
  注意事项 如果我们想让数组元素类型为任意类型的话可以使用空接口作为类型，当使用值时我们必须先做一个类型判断
  数组元素可以通过 索引（位置）来读取（或者修改），索引从 0 开始，第一个元素索引为 0，第二个索引为 1，以此类推
  
  声明的格式：
  var identifier [len]type
  
  #数组常量
  通过 数组常量 的方法来初始化数组
  第一种变化：var arrAge = [5]int{18, 20, 15, 22, 16}
  第二种变化：var arrLazy = [...]int{5, 6, 7, 8, 22}
  第三种变化：var arrKeyValue = [5]string{3: "Chris", 4: "Ron"}
  示例：
  type Vector3D [3]float32
  var vec Vector3D
  
  #多维数组
  示例：
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
  
  #将数组传递给函数
  把一个大数组传递给函数会消耗很多内存。有两种方法可以避免这种现象：
  1.传递数组的指针
  2.使用数组的切片
  示例：
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
  但这在 Go 中并不常用，通常使用切片
  
  #切片(https://go.timpaik.top/07.2.html#_7-2-%E5%88%87%E7%89%87)
  切片（slice）是对数组一个连续片段的引用（该数组我们称之为相关数组，通常是匿名的），所以切片是一个引用类型
  这个片段可以是整个数组，或者是由起始和终止索引标识的一些项的子集。需要注意的是，终止索引标识的项不包括在切片内。切片提供了一个相关数组的动态窗口。
  切片是可索引的，并且可以由 len() 函数获取长度
  和数组不同的是，切片的长度可以在运行时修改，最小为 0 最大为相关数组的长度：切片是一个 长度可变的数组。
  切片提供了计算容量的函数 cap() 可以测量切片最长可以达到多少：它等于切片的长度 + 数组除切片之外的长度。如果 s 是一个切片，cap(s) 就是从 s[0] 到数组末尾的数组长度。切片的长度永远不会超过它的容量，所以对于 切片 s 来说该不等式永远成立：0 <= len(s) <= cap(s)
  多个切片如果表示同一个数组的片段，它们可以共享数据；因此一个切片和相关数组的其他切片是共享存储的，相反，不同的数组总是代表不同的存储。数组实际上是切片的构建块。
  优点 因为切片是引用，所以它们不需要使用额外的内存并且比使用数组更有效率，所以在 Go 代码中 切片比数组更常用。
  声明切片的格式是： var identifier []type（不需要说明长度）
  切片的初始化格式是：var slice1 []type = arr1[start:end]
  arr1[2:] 和 arr1[2:len(arr1)] 相同，都包含了数组从第三个到最后的所有元素。
  arr1[:3] 和 arr1[0:3] 相同，包含了从第一个到第三个元素（不包括第四个）。
  如果你想去掉 slice1 的最后一个元素，只要 slice1 = slice1[:len(slice1)-1]。
  
  对于每一个切片（包括 string），以下状态总是成立的：
  s == s[:i] + s[i:] // i是一个整数且: 0 <= i <= len(s)
  len(s) <= cap(s)
  
  切片在内存中的组织方式实际上是一个有 3 个域的结构体：指向相关数组的指针，切片长度以及切片容量。
  
  #将切片传递给函数
  如果你有一个函数需要对数组做操作，你可能总是需要把参数声明为切片。当你调用该函数时，把数组分片，创建为一个 切片引用并传递给该函数。
  示例：
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
  
  #用 make() 创建一个切片
  当相关数组还没有定义时，我们可以使用 make() 函数来创建一个切片 同时创建好相关数组：var slice1 []type = make([]type, len)
  也可以简写为 slice1 := make([]type, len)，这里 len 是数组的长度并且也是 slice 的初始长度
  所以定义 s2 := make([]int, 10)，那么 cap(s2) == len(s2) == 10
  make 接受 2 个参数：元素的类型以及切片的元素个数
  
  
  # new() 和 make() 的区别
  都在堆上分配内存，但是它们的行为不同，适用于不同的类型
  1.new(T) 为每个新的类型T分配一片内存，初始化为 0 并且返回类型为*T的内存地址：这种方法 返回一个指向类型为 T，值为 0 的地址的指针，它适用于值类型如数组和结构体（参见第 10 章）；它相当于 &T{}。
  2.make(T) 返回一个类型为 T 的初始值，它只适用于3种内建的引用类型：切片、map 和 channel（参见第 8 章，第 13 章）。
  换言之，new 函数分配内存，make 函数初始化；
  
  #多维 切片
  # bytes 包
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
  
  #For-range 结构
  可以应用于数组和切片:
  for ix, value := range slice1 {
  	...
  }
  多维切片下的 for-range：
  for row := range screen {
  	for column := range screen[row] {
  		screen[row][column] = 1
  	}
  }
  
  #切片重组（reslice）
  slice1 := make([]type, start_length, capacity)
  这么做的好处是我们的切片在达到容量上限后可以扩容。改变切片长度的过程称之为切片重组 reslicing，做法如下：slice1 = slice1[0:end]，其中 end 是新的末尾索引（即长度）
  将切片扩展 1 位可以这么做：
  sl = sl[0:len(sl)+1]
  
  #切片的复制与追加
  如果想增加切片的容量，我们必须创建一个新的更大的切片并把原分片的内容都拷贝过来。
  
  #字符串、数组和切片的应用
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
  
  
  #获取字符串的某一部分
  使用 substr := str[start:end] 可以从字符串 str 获取到从索引 start 开始到 end-1 位置的子字符串。同样的，str[start:] 则表示获取从 start 开始到 len(str)-1 位置的子字符串。而 str[:end] 表示获取从 0 开始到 end-1 的子字符串
  
  #字符串和切片的内存结构
  在内存中，一个字符串实际上是一个双字结构，即一个指向实际数据的指针和记录字符串长度的整数，因为指针对用户来说是完全不可见，因此我们可以依旧把字符串看做是一个值类型，也就是一个字符数组。
  
  #修改字符串中的某个字符
  Go 语言中的字符串是不可变的，也就是说 str[index] 这样的表达式是不可以被放在等号左侧的
  因此，您必须先将字符串转换成字节数组，然后再通过修改数组中的元素值来达到修改字符串的目的，最后将字节数组转换回字符串格式。
  例如，将字符串 "hello" 转换为 "cello"：
  s := "hello"
  c := []byte(s)
  c[0] = 'c'
  s2 := string(c) // s2 == "cello"
  
  #字节数组对比函数
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
  
  #搜索及排序切片和数组
  #append 函数常见操作
  将切片 b 的元素追加到切片 a 之后：a = append(a, b...)
  
  复制切片 a 的元素到新的切片 b 上：
  删除位于索引 i 的元素：a = append(a[:i], a[i+1:]...)
  
  切除切片 a 中从索引 i 至 j 位置的元素：a = append(a[:i], a[j:]...)
  
  为切片 a 扩展 j 个元素长度：a = append(a, make([]T, j)...)
  
  在索引 i 的位置插入元素 x：a = append(a[:i], append([]T{x}, a[i:]...)...)
  
  在索引 i 的位置插入长度为 j 的新切片：a = append(a[:i], append(make([]T, j), a[i:]...)...)
  
  在索引 i 的位置插入切片 b 的所有元素：a = append(a[:i], append(b, a[i:]...)...)
  
  取出位于切片 a 最末尾的元素 x：x, a = a[len(a)-1], a[:len(a)-1]
  
  将元素 x 追加到切片 a：a = append(a, x)
  可以使用切片和 append 操作来表示任意可变长度的序列，从数学的角度来看，切片相当于向量，如果需要的话可以定义一个向量作为切片的别名来进行操作
  #切片和垃圾回收
  切片的底层指向一个数组，该数组的实际容量可能要大于切片所定义的容量。只有在没有任何切片指向的时候，底层的数组内存才会被释放，这种特性有时会导致程序占用多余的内存
  
  # Map
  map 是一种特殊的数据结构：一种元素对（pair）的无序集合，pair 的一个元素是 key，对应的另一个元素是 value，所以这个结构也称为关联数组或字典
  map 是引用类型，可以使用如下声明：
  var map1 map[keytype]valuetype
  var map1 map[string]int
  
  在声明的时候不需要知道 map 的长度，map 是可以动态增长的。未初始化的 map 的值是 nil
  key 可以是任意可以用 == 或者 != 操作符比较的类型，比如 string、int、float。所以数组、切片和结构体不能作为 key，但是指针和接口类型可以。如果要用结构体作为 key 可以提供 Key() 和 Hash() 方法，这样可以通过结构体的域计算出唯一的数字或者字符串的 key
  value 可以是任意类型的；通过使用空接口类型（详见第 11.9 节），我们可以存储任意值，但是使用这种类型作为值时需要先做一次类型断言
  map 传递给函数的代价很小：在 32 位机器上占 4 个字节，64 位机器上占 8 个字节，无论实际上存储了多少数据。通过 key 在 map 中寻找值是很快的，比线性查找快得多，但是仍然比从数组和切片的索引中直接读取要慢 100 倍；所以如果你很在乎性能的话还是建议用切片来解决问题。
  map 也可以用函数作为自己的值，这样就可以用来做分支结构（详见第 5 章）：key 用来选择要执行的函数。
  v := map1[key1] 可以将 key1 对应的值赋值给 v；如果 map 中没有 key1 存在，那么 v 将被赋值为 map1 的值类型的空值
  常用的 len(map1) 方法可以获得 map 中的 pair 数目，这个数目是可以伸缩的，因为 map-pairs 在运行时可以动态添加和删除
  map literals 的使用方法： map 可以用 {key1: val1, key2: val2} 的描述方法来初始化，就像数组和结构体一样
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
  
  map 是 引用类型 的： 内存用 make 方法来分配
  
  map 的初始化：var map1 = make(map[keytype]valuetype)
  简写为：map1 := make(map[keytype]valuetype)
  
  不要使用 new，永远用 make 来构造 map
  
  注意 如果你错误的使用 new() 分配了一个引用对象，你会获得一个空引用的指针，相当于声明了一个未初始化的变量并且取了它的地址：
  
  #map 容量
  和数组不同，map 可以根据新增的 key-value 对动态的伸缩，因此它不存在固定长度或者最大限制。但是你也可以选择标明 map 的初始容量 capacity，就像这样：make(map[keytype]valuetype, cap)
  例如：map2 := make(map[string]float32, 100)
  
  当 map 增长到容量上限的时候，如果再增加新的 key-value 对，map 的大小会自动加 1。所以出于性能的考虑，对于大的 map 或者会快速扩张的 map，即使只是大概知道容量，也最好先标明。
  一个 map 的具体例子，即将音阶和对应的音频映射起来：
  noteFrequency := map[string]float32 {
  	"C0": 16.35, "D0": 18.35, "E0": 20.60, "F0": 21.83,
  	"G0": 24.50, "A0": 27.50, "B0": 30.87, "A4": 440}
  ```