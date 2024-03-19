1.编译和运行程序
```go
go build main.go
go run main.go
```

2.格式化打印
```go
fmt.Println("go" + "lang")
fmt.Println(true)
fmt.Println(!true)
```

3.变量声明与初始化
```go
var b, c int = 1, 2
:= 用来简化同时进行变量的声明和初始化,例如：
f := "apple"
等同于
var f string = "apple"
注意：:= 语法只在函数内部适用
```

4. 常量
```go
const s string = "constant"
常量语句可以在任何var语句出现的位置出现
```

5. for语句
```go
#1
i := 1  // inside a function
for i <= 3 {
    fmr.Println(i)
    i = i + 1
}

#2
for j := 7; j <= 9; j++ {
    fmt.Println(j)
}

#3
for {
    fmt.Println("loop")
    break
}

#4
for n := 0; n <= 5; n++ {
    if n%2 == 0 {
        continue
    }
    fmt.Println(n)
}
```


6. if/else
```go
if num := 9; num < 0 {
    fmt.Println(num, "is negative")
} else if num < 10 {
    fmt.Println(num, "has 1 digit")
} else {
    fmt.Println(num, "has multiple digits")
}
```

7. switch
```go
switch time.Now().Weekday() {
case time.Saturday, time.Sunday:
    fmt.Println("It is the weekend")
default:
    fmt.Println("It's a weekday")
}

whatAmI := func(i interface{}) {
    switch t := i.(type) {
    case bool:
        fmt.Println("I'm a bool")
    case int:
        fmt.Println("I'm an int")
    default:
        fmt.Printf("Don't know type %T\n", t)
    }
}
```

8. array
```go
slices在go中更常用，array在某些特殊的场合比较有用
var a [5]int
a[4] = 100

b := [5]int{1, 2, 3, 4, 5}

var twoD [2][3]int

twoD[1][1] = 2
```

9. slices
```go
slices是Go语言中非常重要的数据类型，相比于array它提供了更强大的操作序列的接口
未初始化的slice为nil并且长度为0
var s[]string
fmt.Println("uninit:", s, s == nil, lens(s) == 0)

s = make([]string, 3)
fmt.Println("emp:", s, "len:", len(s), "cap:", cap(s))
s[0] = "a"
s[1] = "b"
s.append(s, "d")
s.append(s, "e", "f")
fmt.Println("apd:", l)

c := make([]string, len(s))
copy(c, s)
fmt.Println("cpy:", c)

l := s[2:4]
fmt.Println("sl1:", l)

l := s[:4]
fmt.Println("sl2:", l)

t := []string{"g", "h", "i"}
fmt.Println("dcl:", t)

t2 := []string{"g", "h", "i"}
if slices.Equal(t, t2) {
    fmt.Println("t == t2")
}

twoD := make([][]int, 3)
for i := 0; i < 3; i++ {
    innerLen := i + 1
    twoD[i] = make([]int, innerLen)
    for j := 0; j < innerLen; j++ {
        twoD[i][j] = i + j
    }
}
fmt.Println("2d:", twoD)
```

10. map
```go
m := make(map[string]int)
m["k1"] = 1
m["k2"] = 2

delete(m, "k2")
clear(m)

n1 := map[string]int{"foo":1, "bar":2}
n2 := map[string]int{"foo":1, "bar":2}
if maps.Equal(n1, n2) {
    fmt.Println("n1 == n2")
}
```

11. range
```go
nums := []int{1, 2, 3}
sum := 0
for _, num := range nums {
    sum += num
}

for i, num = range nums {
    if num == 3 {
        fmt.Println("index:", i)
    }
}

kvs := map[string]string{"a": "apple", "b": "banana"}
for k, v := range kvs {
    fmt.Printf("%s -> %s\n", k, v)
}

for k := kvs {
    fmt.Println("key:", k)
}

for i, c := range "go" {
    fmt.Println(i, c)
}
```

12. function
```go
Go需要有明确的return语句，Go可以有多个返回值
当函数有多个相同类型的参数时，参数的类型可以省略
func plus(a, b, c int) int {
    return a + b + c
}

func vals() (int, int) {
    return 3, 7
}

a, b := vals()
_, c = vals()

可变参数
func sum(nums ...int) {
    fmt.Print(nums, " ")
    total := 0

    for _, num := range nums {
        total += num
    }

    return total
}

sum(1, 2)
sum(1, 2, 3)

nums := []int{1, 2, 3}
fmt.Println("sum of nums:", sum(nums...))

匿名函数和闭包
func intSeq() func() int {
    i := 0
    return func() int {
        i++
        return i
    }
}

nextInt := intSeq()
fmt.Println(nextInt())  // 1
fmt.Println(nextInt())  // 2

newInts := intSeq()
fmt.Println(newInts())  // 1

intSeq函数返回另外一个函数(在intSeq的函数体内定义的匿名函数)，返回的函数关闭变量i形成闭包

递归函数
func fact(n int) int {
    if n == 0 {
        return
    }

    return n * fact(n - 1)
}

var fib func(n int) int
fib = func(n int) int {
    if n < 2 {
        return n
    }

    return fib(n-1) + fib(n - 2)
}
```

13. pointers
```go
func zeroptr(iptr *int) {
    *iptr = 0
}

i := 1
zeroptr(&i)
fmt.Println("i:", i)
fmt.Println("address of i:", &i)
```

14. strings and runes
```go
A Go string is a read-only slice of bytes. 
In Go, the concept of a character is called a rune - it’s an integer that represents a Unicode code point. 
Go string literals are UTF-8 encoded text.
Since strings are equivalent to []byte, this will produce the length of the raw bytes stored within.

const s = "สวัสดี"
for i, w = 0, 0; i < len(s); i += w {
    runeValue, width := utf8.DecodeRuneInString(s[i:])
    fmt.Printf("%#U starts at %d\n", runeValue, i)
    w = width

    examineRune(runeValue)
}

func examineRune(r rune) {

    if r == 't' {
        fmt.Println("found tee")
    } else if r == 'ส' {
        fmt.Println("found so sua")
    }
}
```
15. struct

```go
type preson struct {
    name string
    age  int
}

func newPerson(name string) *person {

    p := person{name: name}
    p.age = 42
    return &p
}

fmt.Println(person{"Bob", 20})
fmt.Println(person{name: "Alice", age: 30})
fmt.Println(person{name: "Fred"})
fmt.Println(&person{name: "Ann", age: 40})
fmt.Println(newPerson("Jon"))

s := person{name: "Sean", age: 50}
fmt.Println(s.name)

sp := &s
fmt.Println(sp.age)

sp.age = 51
fmt.Println(sp.age)

dog := struct {
    name string
    isGood bool
} {
    "Rex",
    true,
}
fmt.Println(dog)
```

16. methods
```go
Go supports methods defined on struct types.
type rect struct {
    width, height int
}

// This area method has a receiver type of *rect.
// Methods can be defined for either pointer or value receiver types.
func (r *rect) area() int {
    return r.width * r.height
}

func (r rect) perim() int {
    return 2*r.width + 2*r.height
}

r := rect{width: 10, height: 5}
fmt.Println("area: ", r.area())
fmt.Println("perim: ", r.perim())

rp := &r
fmt.Println("area: ", rp.area())
fmt.Println("perim: ", rp.perim())
```

17. interfaces
```go
Interfaces are named collections of method signatures.
type geometry interface {
    area() float64
    perim() float64
}

type rect struct {
    width, height float64
}
type circle struct {
    radius float64
}

func (r rect) area() float64 {
    return r.width * r.height
}
func (r rect) perim() float64 {
    return 2*r.width + 2*r.height
}

func (c circle) area() float64 {
    return math.Pi * c.radius * c.radius
}
func (c circle) perim() float64 {
    return 2 * math.Pi * c.radius
}

func measure(g geometry) {
    fmt.Println(g)
    fmt.Println(g.area())
    fmt.Println(g.perim())
}

r := rect{width: 3, height: 4}
c := circle{radius: 5}

measure(r)
measure(c)
```

18. struct embedding
```go
Go supports embedding of structs and interfaces to express a more seamless composition of types.
An embedding looks like a field without a name.
When creating structs with literals, we have to initialize the embedding explicitly; 
Embedding structs with methods may be used to bestow interface implementations onto other structs.

type base struct {
    num int
}

func (b base) describe() string {
    return fmt.Sprintf("base with num=%v", b.num)
}

type container struct {
    base
    str string
}

co := container {
    base: base {
        num: 1,
    },
    str: "some name"
}

// We can access the base’s fields directly on container(such as co)
fmt.Printf("co={num: %v, str: %v}\n", co.num, co.str)
fmt.Println("also num:", co.base.num)

// Since container embeds base, the methods of base also become methods of a container.
fmt.Println("describe:", co.describe())

// Embedding structs with methods may be used to bestow interface implementations onto other structs. 
// Here we see that a container now implements the describer interface because it embeds base.
type describer interface {
    describe() string
}

var d describer = co
fmt.Println("describer:", d.describe())
```
19. generics
```go
generics, also known as type parameters
func MapKeys[K comparable, V any](m map[K]V) []K {
    r := make([]K, 0, len(m))
    for k := range m {
        r = append(r, k)
    }
    return r
}

type List[T any] struct {
    head, tail *element[T]
}

type element[T any] struct {
    next *element[T]
    val  T
}

func (lst *List[T]) Push(v T) {
    if lst.tail == nil {
        lst.head = &element[T]{val: v}
        lst.tail = lst.head
    } else {
        lst.tail.next = &element[T]{val: v}
        lst.tail = lst.tail.next
    }
}

func (lst *List[T]) GetAll() []T {
    var elems []T
    for e := lst.head; e != nil; e = e.next {
        elems = append(elems, e.val)
    }
    return elems
}

func main() {
    var m = map[int]string{1: "2", 2: "4", 4: "8"}
    fmt.Println("keys:", MapKeys(m))

    lst := List[int]{}
    lst.Push(10)
    lst.Push(13)
    lst.Push(23)
    fmt.Println("list:", lst.GetAll())
}
```
20. erros
```go
In Go it’s idiomatic to communicate errors via an explicit, separate return value.
By convention, errors are the last return value and have type error, a built-in interface.
A nil value in the error position indicates that there was no error.

func f1(arg int) (int, error) {
	if arg == 42 {
		return -1, errors.New("can't work with 42")
	}

	return arg + 3, nil
}

type argError struct {
	arg int
	prob string
}

func (e *argError) Error() string {
	return fmt.Sprintf("%d - %s", e.arg, e.prob)
}

func f2(arg int) (int, error) {
	if arg == 42 {
		return -1, &argError(arg, "cant't work with it")
	}

	return arg + 3, nil
}

for _, i := range []int{7, 42} {
	if r, e := f1(i); e != nil {
		fmt.Println("f1 failed:", e)
	} else {
		fmt.Println("f2 worked:", r)
	}
}

for _, i := range []int{7, 42} {
	if r, e := f2(i); e != nil {
		fmt.Println("f2 failed:", e)
	} else {
		fmt.Println("f2 worked:", r)
	}
}

_, e := f2(42)
if ae, ok := e.(*argError); ok {
	fmt.Println(ae.arg)
	fmt.Println(ae.prob)
}
```

21. goroutine and channels

Channels are the pipes that connect concurrent goroutines.
By default sends and receives block until both the sender and receiver are ready. This property allowed us to wait at the end of our program for the "ping" message without having to use any other synchronization

```go
# goroutine
# f为普通的函数
go f("goroutine")

go func(msg string) {
	fmt.Println(msg)
}("going")

# channels
// Create a new channel with make(chan val-type)
messages := make(chan string)

// Send a value into a channel using the channel <- syntax
go func() { messages <- "ping" }()

// The <-channel syntax receives a value from the channel
msg := <-messages
fmt.Println(msg)

# Channel Buffering
By default channels are unbuffered
Buffered channels accept a limited number of values without a corresponding receiver for those values

// make a channel of strings buffering up to 2 values
messages := make(chan string, 2)
messages <- "buffered"
messages <- "channel"

fmt.Println(<-messages)
fmt.Println(<-messages)

# channel synchronization
We can use channels to synchronize execution across goroutines.

func worker(done chan bool) {
    fmt.Print("working...")
    time.Sleep(time.Second)
    fmt.Println("done")

    done <- true
}

func main() {

    done := make(chan bool, 1)
    // Start a worker goroutine, giving it the channel to notify on
    go worker(done)

    // Block until we receive a notification from the worker on the channel
    // If you removed this line, the program would exit before the worker even started
    <-done
}

# channel directions
When using channels as function parameters, you can specify if a channel is meant to only send or receive values.

// ping function **only accepts** a channel for sending values.
func ping(pings chan<- string, msg string) {
    pings <- msg
}

// pong function accepts one channel for receives and a second for sends
func pong(pings <-chan string, pongs chan<- string) {
    msg := <-pings
    pongs <- msg
}

func main() {
    pings := make(chan string, 1)
    pongs := make(chan string, 1)
    ping(pings, "passed message")
    pong(pings, pongs)
    fmt.Println(<-pongs)
}
```