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
    ```
