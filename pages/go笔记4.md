# 面向对象
- 结构体
    > 将多个任意类型的变量组合在一起的聚合数据类型
    ```go
    # 定义结构体
    type xxx  struct {
        name type
        name type
        ...
    }

    // 示例
    type Profile struct {
        name    string
        age     int
        gender  string
        mother  *Profile
        father  *Profile
    }

    或者

    type Profile struct {
        name,gender    string   // 相邻属性为同一类型，可以合并写在一起
        age     int
        mother  *Profile
        father  *Profile
    }

    关于结构体定义组合字面量的若个规则：
    1. 当最后一个字段和结果不在同一行时，, 不可省略
    xm := Profile {
        name: "xiaoming",
        age: 18,
        gender: "male",
    }
    或者
    xm := Profile {
        name: "xiaoming",
        age: 18,
        gender: "male"}
    2. 初始化结构体时，字段名要么全写，要么全不写
    3. 初始化结构体时，未赋值的字段会自动赋值为其类型对应的零值 （需要注意的是:只有通过指定字段名才可以赋值部分字段）

    # 绑定方法
    // Go 语言中，我们无法在结构体内定义方法, 通过使用组合函数的方式来定义结构体方法
    // 示例
    func (person Profile) FmtProfile {
        fmt.Printf("name: %s\n", person.name)
        fmt.Printf("age: %d\n", person.age)
        fmt.Printf("gender: %s\n", person.gender)
    }
    // FmtProfile 是方法名
    // (person Profile) 表示将 FmtProfile 方法与 Profile 实例绑定，Profile 称为方法的接收者，person 表示实例本身，相当于python中的self, 在方法内可以使用person.XXX来访问实例属性

    # 方法的参数传递方式
    1. 按值传递
    2. 按指针传递(推荐使用)
        需要在方法内改变实例的属性时，必须使用指针作为方法的接收者
    func (person *Profile) increase_age {
        person.age += 1
    }

    # 实现类似于"继承"的效果
    type company struct {
        companyName string
        companyAddr string
    }

    type staff struct {
        name    string
        age     int
        gender  string
        position    string
        company     // 匿名字段
    }

    // 结构体初始化
    myCom := company {
        companyName: "Tecent",
        companyAddr: "Shenzhen",
    }

    staffInfo := staff {
        name:   "xiaoming",
        age:    23,
        gender: "Male",
        position:   "Developer",
        company:    myCom,
    }

    # 内部方法与外部方法
    // 方法的首字母大写时，该方法对于所有包都是public，其他包可以调用该方法
    // 方法的首字母小写时，该方法是private，其他包无法调用该方法

    # 实例化方法
    // 1
    xm := Profile {
        name:   "xiaoming",
        age:    18,
        gender: "male",
    }
    // 2
    xm := new(Profile)
    xm.name = "xiaoming"
    xm.age = 18
    xm.gender = "male"
    // 3
    var xm *Profile = &Profile{}

    xm.name = "xiaoming"
    // (*xm).name = "xiaoming"
    xm.age = 18
    // (*xm).age = 18
    xm.gender = "male"
    // (*xm).gender = "male"
    ```

- 接口与多态
    > 接口指定了对象应该做什么(接口就是方法签名（Method Signature）的集合)，至于如何实现这个行为（即实现细节），则由对象本身去确定
    ```go
    // 接口指定了一个类型应该具有的方法，并由该类型决定如何实现这些方法
    // 当一个类型定义了接口中的所有方法，我们称它实现了该接口
    // Go 语言通过接口实现多态
    # 使用 type 关键字定义接口
    type Phone interface {
        call()
    }

    // 实现接口
    type Nokia struct {
        name    string
    }

    func (phone Nokia) call() {     // 接收者为 Nokia
        fmt.Println("xxx")
    }

    // 示例
    // 定义接口
    type Good interface {
        settleAccount() int
        orderInfo() string
    }

    // 定义 Phone 结构体
    type Phone struct {
        name string
        quantity int
        price int
    }

    // 实现 Good interface定义的接口
    func (phone Phone) settleAccount() int {
        return phone.quantity * phone.price
    }

    funct (phone Phone) orderInfo() string {
        return "you buyed " + strconv(phone.quantity) + phone.name + strconv(phone.settleAccount())
    }

    // 定义 FreeGift 结构体
    type FreeGift struct {
        name string
        quantity int
        price int
    }

    // 实现 Good interface定义的接口
    funct (gift FreeGift) setttleAccount int {
        return 0
    }

    funct (gift FreeGift) orderInfo() string {
        return strconv.Itoa(gift.quantity) + gift.name + strconv(gift.settleAccount())
    }

    // 利用接口参数实现计算不同种类的商品的总计价格
    func calculateAllPrice(goods []Good) int {
        var allPrice int
        for _, good := range goods {
            fmt.Println(good.orderInfo())
            allPrice += good.settleAccount()
        }
        return allPrice
    }

    iPhone := Phone {
        name:   "iPhone",
        quantity:   1,
        price:      8000,
    }

    earphones := FreeGift {
        name:   "earphone",
        quantity:   1,
        price:      200,
    }

    goods := []Good{iPhone, earphones}
    allPrice := caculateAllPrice(goods)
    fmt.Printf("Total paid: %d\n", allPrice)
    ```
- 结构体 Tag 的用法
    > 结构体字段可以添加额外的属性，用反引号包含的字符串，称为 Tag, 即标签
    ```go
    type Person struct {
        Name string `json:"name"`
        Age int     `json:"age"`
        Addr string `json:"addr,omitempty`
    }

    # Tag 的格式
    // Tag 由反引号包含，由一对或几对的键值对组成，通过空格来分割键值
    `key01:"value01" key02:"value02" key03:"value03"`

    # 获取 Tag 的3个步骤
    1.获取字段 field
    2.获取标签 Tag
    3.获取键值对 key:value

    // 获取 field 的 3 中方法
    field := reflect.TypeOf(obj).FieldByName("Name")
    filed := reflect.ValueOf(obj).Type().Field(i)   // i 表示第几个字段
    filed := reflect.ValueOf(&obj).Elem().Type().Field(i)   // i 表示第几个字段

    // 获取 Tag
    tag := field.Tag

    // 获取键值对
    labelValue := tag.Get("label")      // Get 只是对 Lookup 的简单封装而已
    labelValue, ok := tag.Lookup("label")

    // 示例: 美化打印
    type Person struct {
        Name    string  `label:"Name is: "`
        Age     int     `label:"Age is: "`
        Gender  string  `label:"Gender is: " default: "unknown"`
    }

    func Print(obj interface{}) error {
        // 取 value
        v := reflect.ValueOf(obj)

        // 解析字段
        for i := 0; i < v.NumField(); i++ {
            // 取 Tag
            field := v.Type().Field(i)
            tag := field.Tag

            // 解析 Label 和 Default
            label := tag.Get("label")
            defaultValue := tag.Get("default")

            value := fmt.Sprintf("%v", v.Field(i))
            if value == "" {
                value = defaultValue
            }

            fmt.Println(label + value)
        }

        return nil
    }
    ```
- 类型断言 Type Assertion
    > 类型断言的用途： 1.检查 i 是否为 nil  2. 检查 i 存储的值是否为某个类型
    ```go
    # 使用方式
    // 方式 1
    t := i.(T)  // 断言接口对象 i 不为 nil，并且接口对象 i 存储的值的类型是 T, 断言成功返回 i 的值，失败触发 panic
    // 方式 2
    t, ok := i.(T)  // 和方式1的区别是，断言失败不会触发 panic，而是将 ok 的值设置为 false

    # Type Switch
    // 用来区分多种类型
    func findType(i interface{}) {
        switch x := i.(type) {
            case int:
                fmt.Println(x, "is int")
            case string:
                fmt.Println(x, "is string")
            case nil:
                fmt.Println(x, "is nil")
            default:
                fmt.Println(x, "no type matched")
        }
    }
    注意：
    1. 类型断言，仅能对静态类型为空接口（interface{}）的对象进行断言，否则会抛出错误
    2. 类型断言完成后，实际上会返回静态类型为你断言的类型的对象，而要清楚原来的静态类型为空接口类型（interface{}），这是 Go 的隐式转换
    ```
- 空接口
    > 空接口是特殊形式的接口类型，普通接口都有方法，空接口没有定义任何方法，因此我们可以说所有类型都实现了空接口
    ```go
    每个接口都包含两个属性，一个是值，一个是类型，对于空接口来说，值和类型都是nil

    # 如何使用空接口
    // 1 使用 interface{} 作为类型声明一个实例，这个实例可以承载任意类型的值
    var i interface{}

    i = 1
    fmt.Println(i)
    i = "hello"
    i = false

    // 2 如果希望函数可以接收任意类型的参数，可以使用空接口
    func myfunc(iface interface{}) {
        fmt.Pritnln(iface)
    }

    a := 10
    b := "hello"
    c := true

    myfunc(a)
    myfunc(b)
    myfunc(c)

    // 接收任意个任意类型的值
    func myfunc(ifaces ...interface{}) {
        for _, iface := range ifaces {
            fmt.Println(iface)
        }
    }

    a := 10
    b := "hello"
    c := true

    myfunc(a, b, c)

    // 3 定义接收任意类型的 array, slice, map, struct
    any := make([]interface{}, 5)
    any[0] = 10
    any[1] = "hello"
    any[2] = []int{1, 2, 3}

    for _, value := range any {
        fmt.Println(value)
    }

    # 使用空接口的注意事项
    1. 空接口可以承载任意值，但不代表任意类型就可以承接空接口类型的值（从实现的角度看，任意类型的值都满足空接口，因此空接口类型可以保存任何值，也可以从空接口中取出原值，但是不允许将空接口类型的对象赋值给固定类型的对象）
    2. 当空接口承载数组和切片后，该对象无法再进行切片，如下会报错
    sli := []int{1, 2, 3}

    var i interface{}
    i = sli

    g := i[1:3] // 会报错
    3. 当使用空接口接收任意类型的参数时，它的静态类型是 interface{} ，但动态类型我们并不知道，因此需要使用类型断言
    func mayfunc(i interface{}) {
        switch i.(type) {
            case int:
                fmt.Println("int")
            case string:
                fmt.Println("string")
        }
    }
    ```
- 接口的3个"潜规则"
    ```go
    1. 对方法调用的限制
        接口是一组固定的方法集，由于静态类型的限制，接口变量有时仅能调用其中特定的一些方法
    type Phone interface {
        call()
    }

    type iPhone struct {
        name string
    }

    func (phone iPhone) call() {

    }

    func (phone iPhone) send_msg() {

    }

    var phone Phone
    phone = iPhone{name: "mi"}
    phone.call()
    phone.send_msg()    // error: phone.send_msg undefined (type Phone has no field or method send_msg)
    // 出错的原因是phone对象显式声明为Phone接口类型，因此phone调用的方法会受到此接口的限制
    # 解决方法
    phone := iPhone{name: "mi"}
    phone.call()
    phone.send_msg()

    2. 调用函数时的隐式转换
    // Go 语言中的函数调用都是按值传递的，变量会在方法调用前进行类型转换，如下代码会报错
    a := 10

    switch a.(type) {
        case int:
            fmt.Println("int")
        case string:
            fmt.Println("string")
    }
    // error: cannot type switch on non-interface value a (type int)
    // 出错原因是 Go 会将传入的参数值的类型转换为 interfacee{} 类型
    # 解决方法： 将变量的静态类型转换为 interface{} 类型
    a := 10

    switch interface{}(a).(type) {
        case int:
            fmt.Println("int")
        case string:
            fmt.Println("string")
    }

    3. 类型断言中的隐式转换
    // 只有静态类型为接口类型的对象才可以进行类型断言，当类型断言完成后，会返回一个静态类型为你断言的类型的对象，即当使用类型断言，Go 实际上又会默认为我们进行了一次隐式的类型转换
    // 验证方法：使用完一次类型断言后，对返回的对象再一次使用类型断言，Goland 立马就会提示我们新对象 b 不是一个接口类型的对象，不允许进行类型断言
    ```
- 反射
    > 反射可以用来获取对象的类型，属性及方法等
    ```go
    # 反射世界里的两种类型
    1. reflect.Type
    2. reflect.Value

    type Type interface {
        Align() int
        FieldAlign() int
        Method(int) Method
        MethodByName(string) (Method, bool)
        ...
    }

    type Value struct {
        typ *rtype
        ptr unsafe.Pointer
        flag
    }
    // Value 同时接收了很多方法

    // 接口变量，实际上是由一 pair (type和data)组合而成的，pair 对中记录着实际变量的值和类型，也就是说在真实世界里，type 和 value 是合并在一起组成 接口变量的
    // 在反射的世界里，type 和 data 却是分开的，他们分别由 reflect.Type 和 reflect.Value 来表现

    # 反射的三大定律
    1.反射可以将接口类型变量 转换为“反射类型对象”；
    2.反射可以将 “反射类型对象”转换为 接口类型变量；
    3.如果要修改 “反射类型对象” 其类型必须是 可写的；

    
    // 1 实现从接口变量到反射对象的转换
    reflect.TypeOf(i)   获取接口值的类型
    relfect.ValueOf(i)  获取接口值的值
    // 这两个方法返回的对象，我们称之为反射对象：Type object 和 Value object

    var age interface{} = 25

    fmt.Printf("original type: %T, value: %v \n", age, age)

    t := reflect.TypeOf(age)
    v := reflect.ValueOf(age)

    fmt.Printf("reflect type: %T \n", t)
    fmt.Printf("reflect value: %v \n", v)

    // 2 实现反射对象到接口变量的转换
    // 通过源码可知， reflect.Value 的结构体会接收 Interface 方法，返回了一个 interface{} 类型的变量 (注意：只有 Value 才能逆向转换，而 Type 则不行)
    func (v value) Interface() (i interface{}) {
        return valueInterface(v, true)
    }

    var age interface{} = 25

    fmt.Printf("original type: %T, value: %v \n", age, age)

    // 从接口变量到反射对象
    t := reflect.TypeOf(age)
    v := reflect.ValueOf(age)

    fmt.Printf("reflect type: %T \n", t)
    fmt.Printf("reflect value: %v \n", v)

    // 从反射对象到接口变量
    i := v.Interface()
    fmt.Printf("new type: %T, new value: %v \n", age, age)

    // 3 如果要修改 “反射类型对象” 其类型必须是 可写的
    // Go 语言里的函数都是值传递，只要你传递的不是变量的指针，你在函数内部对变量的修改是不会影响到原始的变量的
    // 对于反射，当使用 reflect.Typeof 和 reflect.Valueof 的时候，如果传递的不是接口变量的指针，反射世界里的变量值始终将只是真实世界里的一个拷贝，你对该反射对象进行修改，并不能反映到真实世界里
    注意对于反射：
    1. 不是接收变量指针创建的反射对象，是不具备『可写性』的
    2. 是否具备『可写性』，可使用 CanSet() 来获取得知
    3. 对不具备『可写性』的对象进行修改，是没有意义的，也认为是不合法的，因此会报错

    var name string = "hello"
    v1 := reflect.ValueOf(&name)
    fmt.Println(v1.CanSet())    // false

    v2 := v1.Elem()
    fmt.Println(v2.CanSet())    // true

    // 如果对象具有可写性后，可以使用各种 SetXXX 方法对其进行修改
    var name string = "hello"

    v1 := reflect.ValueOf(&name)
    v2 := v1.Elem()

    v2.SetString("world")
    fmt.Println(name)

    # 获取类别
    // Type 对象和 Value 对象可以通过 Kind() 方法返回对应的接口变量的基础类型
    reflect.TypeOf(m).Kind()
    reflect.ValueOf(m).Kind()
    // Kind 和 Type 是有区别的，Kind 表示的是 Go 原生的基本类型（共一下25种的合法类型）
    type Kind uint

    const {
        Invalid Kind = itoa // 非法类型
        Bool                // 布尔型
        Int                 // 有符号整型
        Int8                // 有符号8位整型
        Int16               // 有符号16位整型
        Int32               // 有符号32位整型
        Int64               // 有符号64位整型
        Uint                // 无符号整型
        Uint8               // 无符号8位整型
        Uint16              // 无符号16位整型
        Uint32              // 无符号32位整型
        Uint64              // 无符号64位整型
        Uintptr             // 指针
        Float32             // 单精度浮点数
        Float64             // 双精度浮点数
        Complex64           // 64位复数类型
        Complex128          // 128位复数类型
        Array               // 数组
        Chan                // 通道
        Func                // 函数
        Interface           // 接口
        Map                 // 映射
        Ptr                 // 指针
        Slice               // 切片
        String              // 字符串
        Struct              // 结构体
        UnsafePointer       // 底层指针
    }

    # Kind 函数的使用
    // 1
    type Profile struct {

    }

    m := Profile{}

    t := reflect.TypeOf(m)
    fmt.Println("Type: ", t)
    fmt.Println("Kind: ", t.Kind())

    // 2
    m := Profile()

    t := reflect.TypeOf(&m)

    fmt.Println("&m Type: ", t)
    fmt.Println("&m Kind: ", t.Kind())

    fmt.Println("m Type: ", t.Elem())
    fmt.Println("m Kind: ", t.Elem().Kind())

    // 使用 ValueOf 方法
    m := Profile{}

    v := reflect.ValueOf(&m)

    fmt.Println("&m Type: ", v.Type())
    fmt.Println("&m Kind: ", v.Kind())

    fmt.Println("m Type: ", v.Elem().Type())
    fmt.Println("m Kind: ", v.Elem().Kind())

    # 类型转换
    // 1
    var age int = 25

    v1 := reflect.ValueOf(age)
    fmt.Println("before convert, type: %T, value: %v", v1, v1)
    v2 := v1.Int()
    fmt.Println("after convert, type: %T, value: %v", v2, v2)

    // 2
    var score float64 = 99.5

    v1 := reflect.ValueOf(score)
    fmt.Println("before convert, type: %T, value: %v", v1, v1)
    v2 := v1.Float()
    fmt.Println("after convert, type: %T, value: %v", v2, v2)

    // 3
    var name string = 99.5

    v1 := reflect.ValueOf(name)
    fmt.Println("before convert, type: %T, value: %v", v1, v1)
    v2 := v1.String()
    fmt.Println("after convert, type: %T, value: %v", v2, v2)

    // ...
    Bool()
    Pointer()
    Interface()

    # 对切片的操作
    Slice() 函数与 Int() 等类型转换函数**不一样**，它返回的还是 reflect.Value 发射对象，而不是我们所想的真实世界里的切片对象

    var numList []int = []int{1, 2}

    v1 := reflect.ValueOf(numList)
    fmt.Println("before convert, type: %T, value: %v", v1, v1)

    // Slice 函数接收两个参数
    v2 := v1.Slice(0, 2)
    fmt.Println("after convert, type: %T, value: %v", v2, v2)

    // Slice3(): 对切片再切片
    // Set() 和 Append(): 更新切片
    func appendToSlice(arrPtr interface{}) {
        valuePtr := reflect.ValueOf(arrPtr)
        value := valuePtr.Elem()

        value.Set(reflect.Append(value, reflect.ValueOf(3)))

        fmt.Println(value)
        fmt.Println(value.Len())
    }

    arr := []int{1, 2}

    appendToSlice(&arr)

    fmt.Println(arr)

    # 对属性的操作
    // NumField() 和 Field()
    type Person struct {
        name    string
        age     int
        gender  string
    }

    p := Person{"developer", 27, "male"}

    v := reflect(ValueOf(p))

    for i := 0; i < v.NumField(); i++ {
        fmt.Println("The %d field: %v", i+1, v.Field(i))
    }

    # 对方法的操作
    // NumMethod() 和 Method()  (要获取Name，使用Typeof)
    type Person struct {
        name    string
        age     int
        gender  string
    }

    func (p Person) SayBye() {
        fmt.Println("bye")
    }

    func (p Person) SayHello() {
        fmt.Println("hello")
    }

    p := Person{"developer", 27, "male"}

    t := reflect.TypeOf(p)

    for i := 0; i < v.NumMethod(); i++ {
        fmt.Println("The %d field: %v", i+1, v.Method(i).Name)
    }

    # 动态调用函数（要调用Call，使用ValueOf）
    // 1 (使用索引)
    type Person struct {
        name    string
        age     int
        gender  string
    }

    func (p Person) SayBye() {
        fmt.Println("bye")
    }

    func (p Person) SayHello() {
        fmt.Println("hello")
    }

    p := &Person{"developer", 27, "male"}

    t := reflect.TypeOf(p)
    v := reflect.ValueOf(p)

    for i := 0; i < v.NumMethod(); i++ {
        fmt.Println("The %d method: %v, call result: %v", i+1, t.Method(i).name, v.Elem().Method(i).call(nil))
    }

    
    // 2 (使用函数名)
    type Person struct {
        name    string
        age     int
        gender  string
    }

    func (p Person) SayBye() {
        fmt.Println("bye")
    }

    func (p Person) SayHello() {
        fmt.Println("hello")
    }

    p := &Person{"developer", 27, "male"}

    v := reflect.ValueOf(p)

    v.MethodByName("SayHello").Call(nil)
    v.MethodByName("SayBye").Call(nil)

    // 3 (使用函数且有参数)
    type Person struct {

    }

    func (p Person) SelfIntroduction(name string, age int) {
        fmt.Printf("Hello, my name is %s and I'm %d years old.\n", name, age)
    }

    p := &Person{}

    //t := reflect.TypeOf(p)
    v := reflect.ValueOf(p)

    name := reflect.ValueOf("xiaoming")
    age := reflect.ValueOf(27)
    intput := []reflect.Value{name, age}
    v.MethodByName("SelfIntroduction").Call(intput)

    注意：
    1.有 reflect 的代码一般都较难理解，使用时请注意适当
    2.Golang 的反射很慢，这个和它的 API 设计有关
    3.反射是一个高级知识点，内容很多，不容易掌握，应该小心谨慎的使用它
    4.不到不得不用的地步，能避免使用反射就不用
    ```
- 静态类型与动态类型
    - 静态类型，是变量声明时的类型
    - 具体类型(concrete type)，是程序运行时变量的具体类型
    ```go
    // 静态类型
    var age int
    var name string

    // 具体类型 是 程序运行时的变量类型
    var i interface{}   // i 的静态类型是 interface{}

    i = 18              // i 的静态类型是 interface{}，i 的具体类型是 int
    i := "hello"        // i 的静态类型是 interface{}，i 的具体类型是 string

    // 接口变量 是由type和data的一个pair组合而成，pair中记录了变量的实际的值和类型
    // 定义变量
    age := (int)(25)
    或者
    age := (interface{})(25)

    # 接口细分
    1.iface
        表示带有一组方法的接口
    2.eface
        表示不带有方法的接口
    
    // iface 源码
    // runtime/runtime2.go
    // 非空接口
    type iface struct {
        tab  *itab
        data unsafe.Pointer
    }

    // 非空接口的类型信息
    type itab struct {
        inter  *interfacetype  // 接口定义的类型信息
        _type  *_type      // 接口实际指向值的类型信息
        link   *itab
        bad    int32
        inhash int32
        fun    [1]uintptr   // 接口方法实现列表，即函数地址列表，按字典序排序
    }

    // runtime/type.go
    // 非空接口类型，接口定义，包路径等。
    type interfacetype struct {
    typ     _type
    pkgpath name
    mhdr    []imethod      // 接口方法声明列表，按字典序排序
    }
    // 接口的方法声明
    type imethod struct {
    name nameOff          // 方法名
    ityp typeOff                // 描述方法参数返回值等细节
    }

    // eface 源码
    // src/runtime/runtime2.go
    // 空接口
    type eface struct {
        _type *_type
        data  unsafe.Pointer
    }
    ```
- make 和 new
    ```go
    # new
    // The new built-in function allocates memory. The first argument is a type,
    // not a value, and the value returned is a pointer to a newly
    // allocated zero value of that type.
    func new(Type) *Type    // new 只能传递一个参数，该参数为一个任意类型（内件类型或者自定义类型）

    // new 做的事情
    1.分配内存
    2.设置零值
    3.返回指针

    a := new(int)
    *a = 1
    等价于
    a := 1

    # make
    //The make built-in function allocates and initializes an object
    //of type slice, map, or chan (only). Like new, the first argument is
    // a type, not a value. Unlike new, make's return type is the same as
    // the type of its argument, not a pointer to it.

    func make(t Type, size ...IntegerType) Type
    // make 只能用来为 slice, map 或者 chan 这3中引用类型（注意：也只能用在这3中类型上）分配内存和初始化对象
    // make 返回类型本身而不是指针，返回值依赖于具体传入的类型，因为这3中类型都是引用类型，因此没必要返回指针
    注意：因为这3中类型是引用类型，所以必须得初始化（size和cap），但不是置为零值，这是和new是不一样的
    // slice
    a := make([]int, 2, 10)
    // dictionary
    b := make(map[string]int)
    // chan
    c := makek(chan int, 10)
    ```
- 空结构体
    > 空结构体，和正常的结构体一样，可以接收方法函数，空结构体是一个不占用空间的对象
    ```go
    // 在一些特殊的场合之下，可以用做占位符使用，合理的使用空结构体，会减小程序的内存占用空间
    // 比如在使用信道(channel)控制并发时，我们只是需要一个信号，但并不需要传递值，这个时候，也可以使用 struct{} 代替
    func main() {
        ch := make(chan struct{}, 1)
        go func() {
            <-ch
            // do something
        }()
        ch <- struct{}{}
        // ...
    }
    ```
- 包管理
    > Go 语言中，一个包可以包含多个 .go 文件（这些文件必须在同一级文件夹下），只要这些 .go 文件的头部都使用 package 关键字声明了同一个包
    ```go
    # 单行导入与多行导入
    // 1
    import "fmt"
    import "sync"

    // 2
    import (
        "fmt"
        "sync"
    )

    # 使用别名
    // 在一些场景下需要对导入的包进行重新命名
    // 1
    import (
        "crypto/rand"
        mrand "math/rand"   // 将名称替换为 mrand 避免冲突
    )
    // 2
    import hw "helloworldtestmodule"    // 避免过长的包名
    // 3
    import pathpkg "path"               // 防止导入的包名和本地的变量发生冲突

    # 使用 点 操作
    import . "fmt"
    Println("hello")
    // 这种用法，会有一定的隐患，就是导入的包里可能有函数，会和我们自己的函数发生冲突

    # 包的初始化
        每个包都允许有一个 init 函数，当这个包被导入时，会执行该包的这个 init 函数，做一些初始化任务
        1.init 函数优先于 main 函数执行
        2.在一个包引用链中，包的初始化是深度优先的
        3.同一个包甚至同一个源文件，可以有多个 init 函数
        4.init 函数不能有入参和返回值
        5.init 函数不能被其他函数调用
        6.同一个包内的多个 init 顺序是不受保证的
        7.在 init 之前，其实会先初始化包作用域的常量和变量（常量优先于变量）
    # 包的匿名导入
    // 当我们导入一个包时，如果这个包没有被使用到，在编译时，是会报错的
    // 有些情况下，我们导入一个包，只想执行包里的 init 函数，来运行一些初始化任务，此时可以使用匿名导入
    import _ "image/png"
    // 由于导入时，会执行init函数，所以编译时，仍然会将这个包编译到可执行文件中

    # 导入的是路径还是包
    import "testmodule/foo"
    // 导入时，是按照目录导入。导入目录后，可以使用这个目录下的所有包
    // 出于习惯，包名和目录名通常会设置成一样，所以会让你有一种你导入的是包的错觉

    # 相对导入和绝对导入
    绝对导入：从 $GOPATH/src 或 $GOROOT 或者 $GOPATH/pkg/mod 目录下搜索包并导入
        import "app/utilset"
    相对导入：从当前目录中搜索包并开始导入
        import "./utilset"
    注意：
    1.项目不要放在 $GOPATH/src 下，否则会报错
    2.Go Modules 不支持相对导入，在你开启 GO111MODULE 后，无法使用相对导入
    3.使用相对导入的方式，项目可读性会大打折扣，不利用开发者理清整个引用关系
    4.一般更推荐使用绝对引用的方式。使用绝对引用的话，又要谈及优先级了

    # 包导入路径优先级
    // 如果使用 govendor
    1.先从项目根目录的 vendor 目录中查找
    2.最后从 $GOROOT/src 目录下查找
    3.然后从 $GOPATH/src 目录下查找
    4.都找不到的话，就报错

    // 如果使用 go modules
    1.导入的包如果有域名，都会先在 $GOPATH/pkg/mod 下查找，找不到就连网去该网站上寻找，找不到或者找到的不是一个包，则报错
    2.如果导入的包没有域名（比如 “fmt”这种），就只会到 $GOROOT 里查找
    3.当项目下有 vendor 目录时，不管包有没有域名，都只会在 vendor 目录中想找

    # Go Modules 应用
    Go 语言的的包依赖管理
    1.GOPATH
    2.go vendor 
    3.go mod    // after v1.11
    // cmds
    go env
    go mod init xxx
    go install

    go modules 的核心:
    1.go.mod
    2.go.sum

    go mod 命令：
    go mod init
    go mod download
    go mod graph
    go mod tidy
    go mod verify
    go mod why
    go mod vendor
    go mod edit
    go list -m -json all

    如何给项目添加依赖（写进 go.mod）
    1.只要在项目中有 import，然后 go build 就会 go module 就会自动下载并添加
    2.自己手工使用 go get 下载安装后，会自动写入 go.mod

    # 如何开源自己写的包给别人用
    ......

    # Go 编码规范
    // 文件命名
    文件名应一律使用小写
    不同单词之间用下划线分词，不要使用驼峰式命名
    如果是测试文件，可以以 _test.go 结尾
    文件若具有平台特性，应以 文件名_平台.go 命名，如utils_ windows.go，utils_linux.go
    一般情况下应用的主入口应为 main.go，或者以应用的全小写形式命名。比如MyBlog 的入口可以为 myblog.go

    // 常量命名
    1.驼峰命名法，比如 appVersion
    2.使用全大写且用下划线分词，比如 APP_VERSION (推荐)
    // 定义多个变量
    const (
        APP_VERSION = "0.10"
        CONF_PATH   = "/etc/xxx.conf"
    )

    // 变量命名
    统一使用 驼峰命名法
    1.在相对简单的环境（对象数量少、针对性强）中，可以将完整单词简写为单个字母，例如：user写为u
    2.若该变量为 bool 类型，则名称应以 Has, Is, Can 或 Allow 开头。例如：isExist ，hasConflict
    3.其他一般情况下首单词全小写，其后各单词首字母大写。例如：numShips 和 startDate
    4.若变量中有特有名词（以下列出），且变量为私有，则首单词还是使用全小写，如 apiClient
    5.若变量中有特有名词（以下列出），但变量不是私有，那首单词就要变成全大写。例如：APIClient，URLString

    常见的特有名词:
    // A GonicMapper that contains a list of common initialisms taken from golang/lint
    var LintGonicMapper = GonicMapper{
        "API":   true,
        "ASCII": true,
        "CPU":   true,
        "CSS":   true,
        "DNS":   true,
        "EOF":   true,
        "GUID":  true,
        "HTML":  true,
        "HTTP":  true,
        "HTTPS": true,
        "ID":    true,
        "IP":    true,
        "JSON":  true,
        "LHS":   true,
        "QPS":   true,
        "RAM":   true,
        "RHS":   true,
        "RPC":   true,
        "SLA":   true,
        "SMTP":  true,
        "SSH":   true,
        "TLS":   true,
        "TTL":   true,
        "UI":    true,
        "UID":   true,
        "UUID":  true,
        "URI":   true,
        "URL":   true,
        "UTF8":  true,
        "VM":    true,
        "XML":   true,
        "XSRF":  true,
        "XSS":   true,
    }

    // 函数命名
    1.函数名还是使用 驼峰命名法
    2.在 Golang 中是用大小写来控制函数的可见性，因此当你需要在包外访问，需要使用 大写字母开头
    3.当你不需要在包外访问，使用小写字母开头

    // 函数内部的参数的排列顺序也有几点原则
    1.参数的重要程度越高，应排在越前面
    2.简单的类型应优先复杂类型
    3.尽可能将同种类型的参数放在相邻位置，则只需写一次类型

    // 接口命名
    使用驼峰命名法，可以用 type alias 来定义大写开头的 type 给包外访问
    type helloWorld interface {
        func Hello()
    }

    type sayHello hellWorld

    // 当接口只有一个函数时，接口名通常以er为后缀
    type Reader interface {
        Read(p []byte) (n int, err error)
    }

    // 注释规范
    // 包注释
    位于 package 之前，如果一个包有多个文件，只需要在一个文件中编写即可
    如果你想在每个文件中的头部加上注释，需要在版权注释和 Package前面加一个空行，否则版权注释会作为Package的注释
    如果是特别复杂的包，可单独创建 doc.go 文件说明
    // Copyright 2009 The Go Authors. All rights reserved.
    // Use of this source code is governed by a BSD-style
    // license that can be found in the LICENSE file.
    package net

    // 代码注释
    // 单行注释

    /*
    多
    行
    注
    释
    */

    // 代码注释的一些更加苛刻的要求
    所有导出对象都需要注释说明其用途；非导出对象根据情况进行注释。
    如果对象可数且无明确指定数量的情况下，一律使用单数形式和一般进行时描述；否则使用复数形式。
    包、函数、方法和类型的注释说明都是一个完整的句子。
    句子类型的注释首字母均需大写；短语类型的注释首字母需小写。
    注释的单行长度不能超过 80 个字符。
    类型的定义一般都以单数形式描述：
    // Request represents a request to run a command.  type Request struct { ...
    如果为接口，则一般以以下形式描述：
    // FileInfo is the interface that describes a file and is returned by Stat and Lstat.
    type FileInfo interface { ...
    函数与方法的注释需以函数或方法的名称作为开头：
    // Post returns *BeegoHttpRequest with POST method.
    如果一句话不足以说明全部问题，则可换行继续进行更加细致的描述：
    // Copy copies file from source to target path.
    // It returns false and error when error occurs in underlying function calls.
    若函数或方法为判断类型（返回值主要为 bool 类型），则以 <name> returns true if 开头：
    // HasPrefix returns true if name has any string in given slice as prefix.
    func HasPrefix(name string, prefixes []string) bool { ...

    # 特别注释
    TODO：提醒维护人员此部分代码待完成
    FIXME：提醒维护人员此处有BUG待修复
    NOTE：维护人员要关注的一些问题说明

    # 包的导入
    标准库排最前面，第三方包次之、项目内的其它包和当前包的子包排最后，每种分类以一空行分隔
    尽量不要使用相对路径来导入包
    import (
        "fmt"
        "html/template"
        "net/http"
        "os"

        "github.com/codegangsta/cli"
        "gopkg.in/macaron.v1"

        "github.com/gogits/git"
        "github.com/gogits/gfm"

        "github.com/gogits/gogs/routers"
        "github.com/gogits/gogs/routers/repo"
        "github.com/gogits/gogs/routers/user"
    )

    # 善用 gofmt
    使用 tab 进行缩进
    一行最长不要超过 80 个字符

    # 结合 Makefile 简化编译过程
    // 利用 -ldflags 动态往程序中注入信息
    BINARY="demo"
    VERSION=0.0.1
    BUILD=`date +%F`
    SHELL := /bin/bash

    versionDir="github.com/iswbm/demo/utils"
    gitTag=$(shell git log --pretty=format:'%h' -n 1)
    gitBranch=$(shell git rev-parse --abbrev-ref HEAD)
    buildDate=$(shell TZ=Asia/Shanghai date +%FT%T%z)
    gitCommit=$(shell git rev-parse --short HEAD)

    ldflags="-s -w -X ${versionDir}.version=${VERSION} -X ${versionDir}.gitBranch=${gitBranch} -X '${versionDir}.gitTag=${gitTag}' -X '${versionDir}.gitCommit=${gitCommit}' -X '${versionDir}.buildDate=${buildDate}'"

    default:
        @echo "build the ${BINARY}"
        @GOOS=linux GOARCH=amd64 go build -ldflags ${ldflags} -o  build/${BINARY}.linux  -tags=jsoniter
        @go build -ldflags ${ldflags} -o  build/${BINARY}.mac  -tags=jsoniter
        @echo "build done."
    
    # 依赖管理：好用的工作区模式
    // 正常 Go 项目中引用的包，都需要在对应代码托管网站上有该包，才能编译及运行
    // (在 $GOPATH/src 目录下创建 github.com/iswbm/demo 及 github.com/iswbm/util 两个空的 go 包) 但如果 demo 引用 util 项目的包，而 util 本身也还在自己的本地上开发，并没有上传到 github，那么 demo 包在调试过程中肯定是无法找到 util 包的
    // Go 1.18 提供的工作区模式，就可以优雅的解决如上出现的问题
    ```
- 并发编程
    ```go
    # 函数
    函数是基于功能或 逻辑进行封装的可复用的代码结构，主要是为了提高代码可读性和可维护性
    Go语言是编译型语言，所以函数编写的顺序是无关紧要的
    1.普通函数
    2.匿名函数

    # 函数声明
    func 函数名称(形参列表) (返回值列表) {
        函数体
    }
    // 形式参数列表描述了函数的参数名以及参数类型，这些参数作为局部变量，其值由函数调用者提供
    // 返回值列表描述了函数返回值的变量名以及类型，如果函数返回一个无名变量或者没有返回值，返回值列表的括号是可以省略的
    func sum(a int, b int) (int) {
        return a + b
    }
    func main() {
        fmt.Println(sum(1, 2))
    }

    # 可变参数
    1.多个类型一致的参数
    2.多个类型不一致的参数
    // 使用 ...int 表示一个元素为 int 类型的切片，用来接收调用者传入的参数
    func sum(args ...int) int {
        var sum int
        for _, v := range args {
            sum += v
        }
        return sum
    }

    fmt.Println(sum(1, 2, 3))
    // ... 是 Go 语言为了方便程序员写代码而实现的语法糖，如果该函数下有多个类型的参数，这个语法糖必须得是最后一个参数
    // ... 只能在定义函数时使用

    func MyPrintf(args ...interface{}) {
        for _, arg := range args {
            switch arg.(type) {
                case int:
                    fmt.Println("int")
                case string:
                    fmt.Println("string")
                case int64:
                    fmt.Println("int64")
                default:
                    fmt.Println("unknown")
            }
        }
    }

    var v1 int = 1
    var v2 int64 = 12
    var v3 string = "hello"
    var v4 float32 = 1.23
    MyPrintf(v1, v2, v3, v4)

    # 多个可变参数函数传递参数
    ... 除了用来接收多个参数，还可以用来解序列，将函数的可变参数(一个切片)一个一个的取出来，传递给另一个可变参数的函数，而不是传递可变参数变量本身，这个用法，也只能在给函数传递参数里使用
    func sum(args ...int) int {
        var result int
        for _, v := range args {
            result += v
        }
        return result
    }

    func Sum(args ...int) int {
        //
        result := sum(args...)
        return result
    }
    
    fmt.Println(Sum(1, 2, 3))

    # 函数的返回值
    当没有指明返回值的类型时, 函数体可以用 return 来结束函数的运行，但 return 后不能跟任何一个对象
    Go 支持一个函数返回多个值
    Go支持返回带有变量名的值
    func double(a int) (b int) {
        // 不能使用 := ，因为在返回值那里已经声明返回值的类型为int
        b = a * 2
        // 不需要指明返回哪个命令，因为返回值那里已经指定了变量名
        return
    }

    # 方法与函数
    方法，是一种特殊的函数。当你一个函数和对象/结构体进行绑定的时候，我们就称这个函数是一个方法

    # 匿名函数
    匿名函数，就是没有名字的函数，它只有函数逻辑体，而没有函数名
    匿名函数只有拥有短暂的生命，一般都是定义后立即使用
    func(data int) {
        fmt.Println("hello", data)
    }()

    // 作为回调函数使用
    // 第2个参数为函数
    func visit(list []int, f func(int)) {
        for _, v := range list {
            // 执行回调函数
            f(v)
        }
    }

    // 使用匿名函数作为参数
    visit([]int{1, 2, 3}, func(v int) {
        fmt.Println(v)
    })

    # 函数类型
    函数类型表示表示着所有拥有同样的入参类型和返回值类型的函数集合
    // Greeting function type
    type Greeting func(name string) string

    func (g Greeting) say(n string) {
        fmt.Println(g(n))
    }

    func english(name string) string {
        return "Hello, " + name
    }

    greet := Greeting(english)
    或者
    var greet Greeting = english

    // greet 作为 Greeting 类型的对象，因此也拥有 Greeting 类型的所有方法
    greet.say("World")
    ```
- goroutine
    在普通函数调用前加一个关键字 go, 就开启了一个 goroutine
    ```go
    // 开启一个协程执行函数
    go func()

    Go 程序的入口通常是 main 函数, main 函数最先运行，我们称之为 main goroutine
    在 main 中或者其下调用的代码中才可以使用 go + func() 的方法来启动协程
    main 函数作为main goroutine, 执行完成后，其下运行的所有协程会自动退出

    # 信道 chan
    chan 提供了 goroutine 之间传输信息的通道, chan 是一种队列式的数据结构，遵循先入先出的规则

    // 每个 chan 只能传递一种数据类型的数据，声明时必须指定类型
    var 信道实例 chan 信道类型
    信道实例 = make(chan 信道类型) // 声明后的信道，其零值是nil，无法直接使用，必须配合make函进行初始化

    或者

    信道实例 := make(chan 信道类型)

    # 信道的操作：发送数据和读取数据
    pipeline := make(chan int)  // 定义 chan
    pipeline<- 200              // 向信道发送数据
    mydata := <-pipeline        // 从信道读取数据，并赋值给mydata

    信道用完后，可以对其进行关闭，避免有人一直在等待
    关闭信道后，接收方仍然可以从信道中读取到数据，只是读取的值是0
    close(pipeline)
    对一个已关闭的信道再关闭，会报错
    x, ok := <-pipeline
    // 第二个返回值 ok 表示信道是否被关闭，如果已经被关闭，ok 为 false，若还没被关闭，ok 为true

    # 信道容量与长度
    使用 make 创建信道，make 接收两个参数
    1.信道类型
    2.信道容量：默认为0，表示可以缓存的数据个数
    // 容量为 0 时， 信道中不能存放数据，在发送数据时，必须要求立马被接收，否则会报错，此信道称为 无缓冲信道
    // 容量为 1 时， 信道只能缓存一个数据，若信道中已有一个数据，此时再向信道发送数据，会造成程序阻塞，此信道可用来做锁
    // 容量大于 1 时，信道中可以存放多个数据，可以用于多个协程之间的通信管道，共享资源
    获取信道容量：cap
    获取信道长度：len

    pipeline := make(chan int, 10)
    fmt.Printf("pipeline chan capacity: %d\n", cap(pipeline))
    pipeline<- 1
    fmt.Printf("pipeline current length: %d\n", len(pipeline))

    # 缓冲信道 与 无缓冲信道
    // 缓冲信道 允许信道里存储一个或多个数据，意味着，设置了缓冲区后，发送端和接收端可以处于异步状态
    pipline := make(chan int, 10)
    // 无缓冲信道
    在信道里无法存储数据，意味着，接收端必须先于发送端准备好，以确保你发送完数据后，有人立马接收数据，否则发送端将造成阻塞，即发送端和接收端是同步运行的。
    pipline := make(chan int)
    
    // 或者
    pipline := make(chat int, 0)

    # 双向信道与单向信道
    // 默认情况下信道都是双向的
    单向信道分为：只读信道 和 只写信道
    // 只读信道
    var pipline = make(chan int)
    type Receiver = <-chan int  // 关键代码：定义别名类型，<-chan 表示只能 chan 里发出数据，即对程序来说就是只读
    var receiver Receiver = pipline
    // 只写信道
    var pipline := make(chan int)
    type Sender = chan<- int    // 关键代码：定义别名类型，chan<- 表示只能 chan 里接收数据，即对程序来说就是只写
    var sender Sender = pipline

    // 示例
    // 定义只写信道
    type Sender = chan<- int
    // 定义只读信道
    type Receiver = <-chan int

    func main() {
        var pipline = make(chan int)

        go func() {
            var sender Sender = pipline
            fmt.Println("Sending 100")
            sender <- 100
        }()

        go func() {
            var receiver Receiver = pipline
            num := <-receiver
            fmt.Println("Received %d", num)
        }()

        // main goroutine sleep to leave chance for goroutine to run
        time.Sleep(time.Second)
    }

    // 遍历信道
    注意：遍历信道时，需要确保信道处于关闭状态，否则循环会阻塞
    func fibonacci(mychan chan int) {
        n := cap(mychan)
        x, y := 1, 1
        for i := 0; i < n; i++ {
            mychan <- x
            x, y = y, x+y
        }

        // remember to close chan
        close(mychan)
    }

    func main() {
        pipline := make(chan int, 10)

        go fibonacci(pipline)

        for k := range pipline {
            fmt.Println(k)
        }
    }

    # 用信道来做锁
    当信道里的数据量已经达到设定的的容量时，此时再往信道里发送数据会阻塞整个程序

    // 示例
    func increment(ch chan bool, x *int) {  // x=x+1不是原子操作，使用容量为1的信道达到锁的效果
        ch <- true
        *x = *x + 1
        <- ch
    }

    func main() {
        pipline := make(chan bool, 1)

        var x int
        for x := 0; i < 1000; i++ {
            go increment(pipline, &x)
        }

        time.Sleep(time.Second)
        fmt.Println("x value: ", x)
    }

    # 信道传递是深拷贝吗
    值类型：String, Array, Int, Struct, Float, Bool     (深拷贝)
    引用类型：Slice, Map                                (浅拷贝)
    对于信道而言，是否是深拷贝，取决于你传入的值是值类型，还是引用类型

    注意事项：
    1.关闭一个未初始化的 channel 会产生 panic
    2.重复关闭同一个 channel 会产生 panic
    3.向一个已关闭的 channel 发送消息会产生 panic
    4.从已关闭的 channel 读取消息不会产生 panic，且能读出 channel 中还未被读取的消息，若消息均已被读取，则会读取到该类型的零值。
    5.从已关闭的 channel 读取消息永远不会阻塞，并且会返回一个为 false 的值，用以判断该 channel 是否已关闭（x,ok := <- ch）
    6.关闭 channel 会产生一个广播机制，所有向 channel 读取消息的 goroutine 都会收到消息
    7.chan 在 Golang 中是一等公民，它是线程安全的，面对并发问题，应首先想到 channel

    # WaitGroup
    Goroutine 之间的通信机制
    1.使用信道来标记完成
    // “不要通过共享内存来通信，要通过通信来共享内存”
    func main() {
        done := make(chan bool)
        go func() {
            for i:= 0; i < 5; i++ {
                fmt.Println(i)
            }
            done <- true
        }()

        <-done
    }

    2.使用 WaitGroup
    var 实例名 sync.WaitGroup   // 实例化 WaitGroup
    实例化后 WaitGroup 的使用
        Add: 初始值为0，你传入的值会往计数器上加，这里直接传入你子协程的数量
        Done: 当某个子协程完成后，可调用此方法，会从计数器上减一，通常可以使用 defer 来调用
        Wait: 阻塞当前协程，直到实例里的计数器归零
    
    func worker(x int, wg *sync.WaitGroup) {
        defer wg.Done()
        for i := 0; i < 5; i++ {
            fmt.Printf("worker %d: %d\n", x, i)
        }
    }

    func main() {
        var wg sync.WaitGroup

        wg.Add(2)
        go worker(1, &wg)
        go worker(2, &wg)

        wg.Wait()
    }
    ```
- 互斥锁和读写锁
	Mutex 可以用来实现互斥锁
	RWMutex	可用来实现读写锁
    ```go
	互斥锁 Mutex(Mutual Exclusion) 是为了保护一个资源不会因为并发操作而引起冲突导致数据不准确
	Mutex 锁的两种定义方法
	// 1
	var lock *sync.Mutex
	lock = new(sync.Mutex)
	// 2
	lock := &sync.Mutex{}

	func add(count *int, wg *sync.WaitGroup, lock *sync.Mutex) {
		defer wg.Done

		for i := 0; i < 1000; i++ {
			lock.Lock()
			*count = *count + 1
			lock.Unlock()
		}
	}

	func main() {
		var wg sync.WaitGroup
		lock := &sync.Mutex()
		count := 0

		wg.Add(3)
		go add(&count, &wg, lock)
		go add(&count, &wg, lock)
		go add(&count, &wg, lock)

		wg.Wait()
		fmt.Println(count)
	}

	注意：
	1.同一协程里，不要在尚未解锁时再次使加锁
	2.同一协程里，不要对已解锁的锁再次解锁
	3.加了锁后，别忘了解锁，必要时使用 defer 语句

	# RWMutex 将程序对资源的访问分为读操作和写操作
	为了保证数据的安全，它规定了当有人还在读取数据（即读锁占用）时，不允计有人更新这个数据（即写锁会阻塞）
	为了保证程序的效率，多个人（线程）读取数据（拥有读锁）时，互不影响不会造成阻塞，它不会像 Mutex 那样只允许有一个人（线程）读取同一个数据

	实例化 RWMutex 锁
	// 1
	var lock *sync.RWMutex
	lock = new(sync.RWMutex)

	// 2
	lock := &sync.RWMutex{}

	RWMutex 里提供了两种锁，每种锁分别对应两个方法，为了避免死锁，两个方法应成对出现，必要时请使用 defer

	读锁：调用 RLock 方法开启锁，调用 RUnlock 释放锁
	写锁：调用 Lock 放开开启锁，调用 Unlock 释放锁

	func main() {
		lock := &sync.RWMutex{}
		lock.Lock()

		for i := 0; i < 4; i++ {
			go func(i int) {
				fmt.Printf("The %d goroute is going to run\n", i);
				lock.RLock()
				fmt.Printf("The %d goroutine gets the read lock, after 1s, it will release the read lock\n", i)
				time.Sleep(time.Second)
				lock.RUnlock()
			}(i)
		}

		time.Sleep(time.Second * 2)

		fmt.Println("The write lock will be released, the read lock will not be blocked")
		lock.Unlock()

		// 由于会等到读锁全部释放，才能获得写锁，因此这里一定会在上面 4 个协程全部完成才能往下走
		lock.Lock()
		fmt.Println("The program is going to exit")
		lock.Unlock()
	}
    ```
- Goroutine 信道死锁案例
    ```go
    // 1
    func main() {
        pipline := make(chan string)
        pipline <- "hello"      // 对于无缓冲信道，在接收者未准备好之前，发送操作是阻塞的
        fmt.Println(<-pipline)
    }
    // fatal error: all goroutines are asleep - deadlock!

    // fix 1 method 1(not work)
    func main() {
        pipline := make(chan string)
        fmt.Println(<-pipline)  // 虽然保证了接收代码在发送代码之前执行，但是接收者一直在等待数据而处于阻塞状态，所以无法执行到发送数据的语句，仍然会造成死锁
        pipline <- "hello"
    }
    // fix 1， method 1
    func hello(pipline chan string) {
        <-pipline
    }

    func main() {
        pipline := make(chan string)
        go hello(pipline)   // 将接收者代码写在另一个协程里，并保证在发送者之前执行
        pipline <- "hello"
    }

    // fix 1, method 2
    pipline := make(chan string, 1) // 接收者代码必须在发送者代码之前 执行，这是针对无缓冲信道才有的约束，将其改为缓冲信道摆脱了该约束
    pipline <- "hello"
    fmt.Println(<-pipline)

    // 2
    func main() {
        ch1 := make(chan string, 1)

        ch1 <- "hello"
        ch1 <- "world"  // 信道容量为1，却向信道中写入2条数据，会造成死锁

        fmt.Println(<-ch1)
    }

    // 3
    func main() {
        pipline := make(chan string)
        go func() {
            pipline <- "hello"
            pipline <- "world"
            // close(pipline)
        }()
        for data := range pipline {
            fmt.Println(data)   // for 接收了2条消息后，由于再也没有向信道发送数据，因此无法收到数据，陷入死循环，造成死锁
        }
    }

    // fix 3
    func main() {
        pipline := make(chan string)
        go func() {
            pipline <- "hello"
            pipline <- "world"
            close(pipline)      // 发送完数据后，手动关闭信道，告诉 range 信道已经关闭，无需等待
        }()
        for data := range pipline {
            fmt.Println(data)
        }
    }
    ```
- 如何实现一个协程池
    goroutine 是一个轻量级的线程，他的创建、调度都是在用户态进行，并不需要进入内核，这意味着创建销毁协程带来的开销是非常小的，大多数情况下，开发人员是不太需要使用协程池的
    ```go
    // 定义协程池
    type Pool struct {
        work chan func()        // 用于接收 task 任务
        sem  chan struct{}      // 用于设置协程池大小，即同时可执行的协程数量
    }

    // 创建协程池实例
    func New(size int) *Pool {
        return &Pool {
            work: make(chan func()),
            sem:  make(chan struct{}, size),
        }
    }

    // 往协程池中添加任务
    // 第一次调用 NewTask 添加任务的时候，由于 work 是无缓冲通道，所以会一定会走第二个 case 的分支：使用 go worker 开启一个协程
    func (p *Pool) NewTask(task func()) {
        select {
            case p.work <- task:
            case p.sem <- struct{}{}:
                go p.worker(task)
        }
    }

    // 执行任务
    // 为了能够实现协程的复用，这个使用了 for 无限循环，使这个协程在执行完任务后，也不退出，而是一直在接收新的任务
    func (p *pool) worker(task func()) {
        defer func() { <-p.sem }()
        for {
            task()
            task = <-p.work
        }
    }
    // 如果设定的协程池数大于 2，此时第二次传入往 NewTask 传入task，select case 的时候，如果第一个协程还在运行中，就一定会走第二个case，重新创建一个协程执行task
    // 如果传入的任务数大于设定的协程池数，并且此时所有的任务都还在运行中，那此时再调用 NewTask 传入 task ，这两个 case 都不会命中，会一直阻塞直到有任务执行完成，worker 函数里的 work 通道才能接收到新的任务，继续执行

    // 协程池的使用
    func main() {
        pool := New(2)

        for i := 1; i < 5; i++ {
            pool.NewTask(func() {
                time.Sleep(2 * time.Second)
                fmt.Println(time.Now())
            })
        }

        time.Sleep(5 * time.Second)
    }
    // 总共 4 个任务，由于协程池大小为 2，所以 4 个任务分两批执行（从打印的时间可以看出）
    ```
- Goroutine: Context
    Context 用来更好的管理(关闭)协程
    ```go
    // Context的接口定义
    type Context interface {
        Deadline() (deadline time.Time, ok bool)
        Done() <-chan struct{}
        Err() error
        Value(key interface{}) interface{}
    }
    // Deadline: 到截止时间deadline，Context 会自动触发 Cancel 动作。返回的第二个值是 一个布尔值，true 表示设置了截止时间，false 表示没有设置截止时间，如果没有设置截止时间，就要手动调用 cancel 函数取消 Context
    // Done：返回一个只读的通道（只有在被cancel后才会返回），类型为 struct{}。当这个通道可读时，意味着parent context已经发起了取消请求，根据这个信号，开发者就可以做一些清理动作，退出goroutine
    // Err：返回 context 被 cancel 的原因
    // Value：返回被绑定到 Context 的值，是一个键值对，所以要通过一个Key才可以获取对应的值，这个值一般是线程安全的

    // 为什么需要 Context
    当一个协程（goroutine）开启后，我们是无法强制关闭它的
    常见的关闭协程的原因：
    1.goroutine 自己跑完结束退出            // 正常关闭
    2.主进程crash退出，goroutine 被迫退出   // 异常关闭，需要fix
    3.通过 chan 发送信号，引导协程的关闭       // 开发者可以手动控制协程的方法

    // 不使用 Context
    func monitor(ch chan bool, number int) {
        for {
            select {
                case v := <-ch:
                    // 仅当 ch 通道被 close，或者有数据发过来(无论是true还是false)才会走到这个分支
                    fmt.Printf("监控器%v，接收到通道值为：%v，监控结束。\n", number,v)
                    return
                default:
                    fmt.Printf("监控器%v，正在监控中...\n", number)
                    time.Sleep(2 * time.Second)
            }
        }
    }

    func main() {
        stopSignal := make(chan bool)

        for i := 1; i <= 5; i++ {
            go monitor(stopSignal, i)
        }

        time.Sleep(time.Second)
        // 关闭所有 goroutine
        close(stopSignal)

        // 等待5s，若此时屏幕没有输出 <正在监控中> 就说明所有的goroutine都已经关闭
        time.Sleep(5 * time.Second)

        fmt.Println("主程序退出！")
    }
    // 当我们定义一个无缓冲通道时，如果要对所有的 goroutine 进行关闭
    // 可以使用 close 关闭通道，然后在所有的 goroutine 里不断检查通道是否关闭(前提你得约定好
    // ，该通道你只会进行 close 而不会发送其他数据，否则发送一次数据就会关闭一个goroutine
    // 这样会不符合咱们的预期，所以最好你对这个通道再做一层封装做个限制)来决定是否结束 goroutine

    // 使用 Context
    func monitor(ctx context.Context, number int) {
        for {
            select {
                // 其实可以写成 case <- ctx.Done()
                // 这里仅是为了让你看到 Done 返回的内容
                case v :=<- ctx.Done(): // 在所有的goroutine 里利用 for + select 搭配来不断检查 ctx.Done() 是否可读，可读就说明该 context 已经取消，你可以清理 goroutine 并退出了
                    fmt.Printf("监控器%v，接收到通道值为：%v，监控结束。\n", number,v)
                    return
                default:
                    fmt.Printf("监控器%v，正在监控中...\n", number)
                    time.Sleep(2 * time.Second)
            }
        }
    }

    func main() {
        ctx, cancel := context.WithCancel(context.Background()) // 以 context.Background() 为 parent context 定义一个可取消的 context

        for i := 1; i <= 5; i++ {
            go monitor(ctx, i)
        }

        time.Sleep(1 * time.Second)
        // 关闭所有 goroutine
        cancel()    // 当你想到取消 context 的时候，只要调用一下 cancel 方法即可。这个 cancel 就是我们在创建 ctx 的时候返回的第二个值

        // 等待5s，若此时屏幕没有输出 <正在监控中> 就说明所有的goroutine都已经关闭
        time.Sleep(5 * time.Second)

        fmt.Println("主程序退出！！")
    }

    # 根 Context
    Go 已经实现了2个，我们代码中最开始都是以这两个内置的context作为最顶层的parent context，衍生出更多的子Context
    var (
        background = new(emptyCtx)
        todo       = new(emptyCtx)
    )

    func Background() Context {
        return background
    }

    func TODO() Context {
        return todo
    }

    // Background，主要用于main函数、初始化以及测试代码中，作为Context这个树结构的最顶层的Context，也就是根Context，它不能被取消
    // TODO，如果我们不知道该使用什么Context的时候，可以使用这个，但是实际应用中，暂时还没有使用过这个TODO
    // 它们两个本质上都是emptyCtx结构体类型，是一个不可取消，没有设置截止时间，没有携带任何值的Context

    # Context 的继承衍生
    context 包的几个 With 系列的函数
    func WithCancel(parent Context) (ctx Context, cancel CancelFunc)
    func WithDeadline(parent Context, deadline time.Time) (Context, CancelFunc)
    func WithTimeout(parent Context, timeout time.Duration) (Context, CancelFunc)
    func WithValue(parent Context, key, val interface{}) Context
    // 第一个参是 父context
    // 通过一次继承，就多实现了一个功能，比如使用 WithCancel 函数传入 根context ，就创建出了一个子 context，该子context 相比 父context，就多了一个 cancel context 的功能

    // WithDeadline
    func monitor(ctx context.Context, number int)  {
        for {
            select {
            case <- ctx.Done():
                fmt.Printf("监控器%v，监控结束。\n", number)
                return
            default:
                fmt.Printf("监控器%v，正在监控中...\n", number)
                time.Sleep(2 * time.Second)
            }
        }
    }

    func main() {
        ctx01, cancel := context.WithCancel(context.Background())
        ctx02, cancel := context.WithDeadline(ctx01, time.Now().Add(1 * time.Second))

        defer cancel()

        for i :=1 ; i <= 5; i++ {
            go monitor(ctx02, i)
        }

        time.Sleep(5  * time.Second)
        if ctx02.Err() != nil {
            fmt.Println("监控器取消的原因: ", ctx02.Err())
        }

        fmt.Println("主程序退出！！")
    }

    // WithTimeout
    func monitor(ctx context.Context, number int)  {
        for {
            select {
            case <- ctx.Done():
                fmt.Printf("监控器%v，监控结束。\n", number)
                return
            default:
                fmt.Printf("监控器%v，正在监控中...\n", number)
                time.Sleep(2 * time.Second)
            }
        }
    }

    func main() {
        ctx01, cancel := context.WithCancel(context.Background())

        // 相比例子1，仅有这一行改动
        ctx02, cancel := context.WithTimeout(ctx01, 1* time.Second)

        defer cancel()

        for i :=1 ; i <= 5; i++ {
            go monitor(ctx02, i)
        }

        time.Sleep(5  * time.Second)
        if ctx02.Err() != nil {
            fmt.Println("监控器取消的原因: ", ctx02.Err())
        }

        fmt.Println("主程序退出！！")
    }

    // WithValue
    func monitor(ctx context.Context, number int)  {
        for {
            select {
            case <- ctx.Done():
                fmt.Printf("监控器%v，监控结束。\n", number)
                return
            default:
                // 获取 item 的值
                value := ctx.Value("item")
                fmt.Printf("监控器%v，正在监控 %v \n", number, value)
                time.Sleep(2 * time.Second)
            }
        }
    }

    func main() {
        ctx01, cancel := context.WithCancel(context.Background())
        ctx02, cancel := context.WithTimeout(ctx01, 1* time.Second)
        ctx03 := context.WithValue(ctx02, "item", "CPU")

        defer cancel()

        for i :=1 ; i <= 5; i++ {
            go monitor(ctx03, i)
        }

        time.Sleep(5  * time.Second)
        if ctx02.Err() != nil {
            fmt.Println("监控器取消的原因: ", ctx02.Err())
        }

        fmt.Println("主程序退出！！")
    }

    # Context 使用注意事项
    1.通常 Context 都是做为函数的第一个参数进行传递（规范性做法），并且变量名建议统一叫 ctx
    2.Context 是线程安全的，可以放心地在多个 goroutine 中使用。
    3.当你把 Context 传递给多个 goroutine 使用时，只要执行一次 cancel 操作，所有的 goroutine 就可以收到 取消的信号
    4.不要把原本可以由函数参数来传递的变量，交给 Context 的 Value 来传递。
    5.当一个函数需要接收一个 Context 时，但是此时你还不知道要传递什么 Context 时，可以先用 context.TODO 来代替，而不要选择传递一个 nil。
    6.当一个 Context 被 cancel 时，继承自该 Context 的所有 子 Context 都会被 cancel
    ```
- Go 协程：万能的通道模型
    对于 chan，要极力避免下面两种情况：
    1. 对一个已关闭的通道，进行关闭
    2. 对一个已关闭的通道，写入数据
    上述两种操作，都会异常程序触发 panic
    Go 语言并没有提供一个内置的函数来判断一个通道是否关闭
    ```go
    # 通道编程模型
    思路:
    1.（发送者）对一个已关闭的通道，写入数据  Not OK
    2.（接收者）对一个已关闭的通道，读取数据  OK
    所以根本问题是如果保证发送者本身知道 chan 是关闭的

    不同场景（无论哪种场景，都会有数据竞争的问题）
    1.一个发送者，N 个接收者
    2.多个发送者
        一个接收者
        多个接收者
    
    解决方案：
    增加一个 “管理角色” 的通道
    将通道分为两种
    1.业务通道：承载数据，用于多个协程间共享数据
    2.管理通道：仅为了标记业务通道是否关闭而存在

    管理通道需要满足两个条件：
    1.具备广播功能
        只能是无缓冲通道（关闭后，所有 read 该通道的所有协程，都能明确的知道该通道已关闭）
        当该管理通道关闭了，说明业务通道也关闭了
        当该管理通道阻塞了，说明业务通道还没关闭
    2.有唯一发送者
        对于多个发送者，一个接收者的场景，业务通道的这个接收者，就可以充当管理通道的 唯一发送者
        对于多个发送者，多个接收者的场景，就需要再单独开启一个媒介协程做 唯一发送者

    // N个发送者，一个接收者
    func main() {
        rand.Seed(time.Now().UnixNano())

        const Max = 100000
        const NumSenders = 1000

        wg := sync.WaitGroup{}
        wg.Add(1)

        // 业务通道
        dataCh := make(chan int)

        // 管理通道：必须是无缓冲通道
        // 其发送者是 业务通道的接收者。
        // 其接收者是 业务通道的发送者。
        stopCh := make(chan struct{})

        // 业务通道的发送者
        for i := 0; i < NumSenders; i++ {
            go func() {
                for {
                    // 提前检查管理通道是否关闭
                    // 让业务通道发送者早尽量退出
                    select {
                    case <- stopCh:
                        return
                    default:
                    }

                    select {
                    case <- stopCh:
                        return
                    case dataCh <- rand.Intn(Max):
                    }
                }
            }()
        }

        // 业务通道的接收者，亦充当管理通道的发送者
        go func() {
            defer wg.Done()

            for value := range dataCh {
                if value == 6666 {
                    // 当达到某个条件时
                    // 通过关闭管理通道来广播给所有业务通道的发送者
                    close(stopCh)
                    return
                }
            }
        }()

        wg.Wait()
    }

    // N个发送者，N个接收者
    func main() {
        rand.Seed(time.Now().UnixNano())

        const Max = 100000
        const NumReceivers = 10
        const NumSenders = 1000

        wg := sync.WaitGroup{}
        wg.Add(NumReceivers)

        // 1. 业务通道
        dataCh := make(chan int)

        // 2. 管理通道：必须是无缓冲通道
        // 其发送者是：额外启动的管理协程
        // 其接收者是：所有业务通道的发送者。
        stopCh := make(chan struct{})

        // 3. 媒介通道：必须是缓冲通道
        // 其发送者是：业务通道的所有发送者和接收者
        // 其接收者是：媒介协程（唯一）
        toStop := make(chan string, 1)

        var stoppedBy string

        // 媒介协程
        go func() {
            stoppedBy = <-toStop
            close(stopCh)
        }()

        // 业务通道发送者
        for i := 0; i < NumSenders; i++ {
            go func(id string) {
                for {
                    // 提前检查管理通道是否关闭
                    // 让业务通道发送者早尽量退出
                    select {
                    case <- stopCh:
                        return
                    default:
                    }

                    value := rand.Intn(Max)
                    select {
                    case <-stopCh:
                        return
                    case dataCh <- value:
                    }
                }
            }(strconv.Itoa(i))
        }

        // 业务通道的接收者
        for i := 0; i < NumReceivers; i++ {
            go func(id string) {
                defer wg.Done()

                for {
                    // 提前检查管理通道是否关闭
                    // 让业务通道接收者早尽量退出
                    select {
                    case <- stopCh:
                        return
                    default:
                    }

                    select {
                    case <- stopCh:
                        return
                    case value := <-dataCh:
                        // 一旦满足某个条件，就通过媒介通道发消息给媒介协程
                        // 以关闭管理通道的形式，广播给所有业务通道的协程退出
                        if value == 6666 {
                            // 务必使用 select，两个目的：
                            // 1、防止协程阻塞
                            // 2、防止向已关闭的通道发送数据导致panic
                            select {
                            case toStop <- "接收者#" + id:
                            default:
                            }
                            return
                        }

                    }
                }
            }(strconv.Itoa(i))
        }

        wg.Wait()
        fmt.Println("被" + stoppedBy + "终止了")
    }

    // 为什么业务通道没有关闭呢？
    // 我们的最终目的其实不是关闭业务通道，而是让业务通道相关的协程能够正常退出。业务通道其实并不都要去关闭它，多关闭一个就多一分风险，何必呢？一旦所有的协程正常退出了，Go 的垃圾回收自然会清理掉，这样是不是更省事呢？

    万能的通道编程模型 总结：
    1.当只有一个发送者时，无论有多少接收者，业务通道都应由唯一发送者关闭。
    2.当有多个发送者，一个接收者时，应借助管理通道，由业务通道唯一接收者充当管理通道的发送者，其他业务通道的发送者充当接收者
    3.当有多个发送者，多个接收者时，这是最复杂的，不仅要管理通道，还要另起一个专门的媒介协程，新增一个媒介通道，但核心逻辑都是一样
    ```
- fmt.Printf 函数
    打印函数
    ```go
    1.fmt.Print: 正常打印字符串和变量，不会进行格式化，不会自动换行，需要手动添加 \n 进行换行，多个变量值之间不会添加空格
    2.fmt.Println：正常打印字符串和变量，不会进行格式化，多个变量值之间会添加空格，并且在每个变量值后面会进行自动换行
    3.fmt.Printf：可以按照自己需求对变量进行格式化打印。需要手动添加 \n 进行换行

    fmt.Print("hello", "world\n")   // helloworld
    fmt.Prntln("hello", "world")    // hello world
    fmt.Printf("hello world\n")     // hello world

    // fmt.Printf 函数
    func Printf(format string, a ...interface{}) (n int, err error) {
        return Fprintf(os.Stdout, format, a...)
    }

    # Printf 的占位符

    %v：以值的默认格式打印
    %+v：类似%v，但输出结构体时会添加字段名
    %#v：值的Go语法表示
    %T：打印值的类型
    %%： 打印百分号本身
    
    // 整型值
    %b：以二进制打印
    %d：以十进制打印
    %o：以八进制打印
    %x：以十六进制打印，使用小写：a-f
    %X：以十六进制打印，使用大写：A-F
    %c：打印对应的的unicode码值
    %q：该值对应的单引号括起来的go语法字符字面值，必要时会采用安全的转义表示
    %U：该值对应的 Unicode格式：U+1234，等价于”U+%04X

    // 字符串
    %s：输出字符串表示（string类型或[]byte)
    %q：双引号围绕的字符串，由Go语法安全地转义
    %x：十六进制，小写字母，每字节两个字符
    %X：十六进制，大写字母，每字节两个字符

    // 浮点数
    %e：科学计数法，如-1234.456e+78
    %E：科学计数法，如-1234.456E+78
    %f：有小数部分但无指数部分，如123.456
    %F：等价于%f
    %g：根据实际情况采用%e或%f格式（以获得更简洁、准确的输出）
    %G：根据实际情况采用%E或%F格式（以获得更简洁、准确的输出）

    type Profile struct {
        name string
        gender string
        age int
    }

    func main() {
        n := 1024
        fmt.Printf("%b\n")      // 10000000000
        fmt.Printf("%o\n")      // 2000
        fmt.Printf("%d\n")      // 1024
        fmt.Printf("%x\n")      // 400

        // 根据 Unicode码值打印字符
        fmt.Printf("%c\n", 65)  // A
        // 根据 Unicode 编码打印字符
        fmt.Printf("%c \n", 0x4E2D) // 中
        // 打印 raw 字符时
        fmt.Printf("%q \n", 0x4E2D) // '中'
        // 打印 Unicode 编码
        fmt.Printf("%U \n", '中') // U+4E2D

        var person = Profile(name:"xiaoming", gender:"male", age:18)

        fmt.Printf("%v \n", person)     // {xiaoming male 18}
        fmt.Printf("%T \n", person)     // main.Profile
        fmt.Printf("%#v \n", person)    // main.Profile{name:"xiaoming", gender:"male", age:18}
        fmt.Printf("%+v \n", person)    // {name:"xiaoming", gender:"male", age:18}
        fmt.Printf("%% \n", person)     // %%

        // 指针打印
        fmt.Printf("%p \n", &person)    // %%

        // 布尔值
        fmt.Printf("%t \n", true)       // true
        fmt.Printf("%t \n", false)       // false

        // 字符串
        fmt.Printf("%s \n", []byte("hello world"))      // hello world
        fmt.Printf("%s \n", "hello world")              // hello world

        fmt.Printf("%q \n", []byte("hello world"))      // "hello world"
        fmt.Printf("%q \n", "hello world")              // "hello world"
        fmt.Printf("%q \n", `hello \r\n world`)         // "hello \\r\\n world"

        fmt.Printf("%x \n", "hello world")              // 68656c6c6f20776f726c64
        fmt.Printf("%X \n", "hello world")              // 68656c6c6F20776F726C64

        f := 12.34
        fmt.Printf("%b\n", f)       // 6946802425218990p-49
        fmt.Printf("%e\n", f)       // 1.234000e+01
        fmt.Printf("%E\n", f)       // 1.234000E+01
        fmt.Printf("%f\n", f)       // 12.340000
        fmt.Printf("%g\n", f)       // 12.34
        fmt.Printf("%G\n", f)       // 12.34
    }

    # 宽度标识符
    %[宽度.精度]标识符
    如果未指定精度，会使用默认精度；如果点号后没有跟数字，表示精度为0
	fmt.Printf("%f\n", f)    // 以默认精度打印
	fmt.Printf("%9f\n", f)   // 宽度为9，默认精度
	fmt.Printf("%.2f\n", f)  // 默认宽度，精度2
	fmt.Printf("%9.2f\n", f) //宽度9，精度2
	fmt.Printf("%9.f\n", f)  // 宽度9，精度0
    Output:
    10.240000
    10.240000
    10.24
        10.24
        10
    ```