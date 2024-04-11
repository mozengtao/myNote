```js
JavaScript 是一种轻量级的脚本语言，它不具备开发操作系统的能力，而是只用来编写控制其他大型应用程序（比如浏览器）的“脚本”
JavaScript 也是一种嵌入式（embedded）语言，它本身提供的核心语法不算很多，JavaScript 本身不提供任何与 I/O（输入/输出）相关的 API，都要靠宿主环境（host）提供，所以 JavaScript 只合适嵌入更大型的应用程序环境，去调用宿主环境提供的底层 API

JavaScript 的核心语法包括两个部分：基本的语法构造 和 标准库，除此之外，各种宿主环境提供额外的 API（即只能在该环境使用的接口），以便 JavaScript 调用。
以浏览器为例，它提供的额外 API 可以分成三大类:
1.浏览器控制类：操作浏览器
2.DOM 类：操作网页的各种元素
3.Web 类：实现互联网的各种功能


实验环境:
Chrome 浏览器的“控制台”
打开方式：Ctrl + Shift + J
（按Enter键，代码就会执行。按Shift + Enter键，就是代码换行，不会触发执行）


JavaScript 的基本语法
1.语句(statement) 和 表达式(expression)
语句（statement）是为了完成某种任务而进行的操作
表达式是为了得到返回值，一定会返回一个值。凡是 JavaScript 语言中预期为值的地方，都可以使用表达式

var a = 1 + 2;  // 赋值语句；其中 1 + 2 是表达式
;;;             // 3 条空语句

语句以分号结尾，一个分号就表示一个语句结束。多个语句可以写在一行内
表达式结尾没有分号

2.变量
变量是对“值”的具名引用

var a = 1;      // 声明变量a，然后在变量a与数值1之间建立引用关系，称为将数值1“赋值”给变量a
等价于
var a;      // 变量声明
a = 1;      // 变量赋值

var a, b;   // 声明多个变量

JavaScript 的变量名区分大小写，A和a是两个不同的变量
JavaScript 是一种动态类型语言，也就是说，变量的类型没有限制，变量可以随时更改类型
var a = 1;
a = "hello";


使用var重新声明一个已经存在的变量，是无效的
var x = 1;
var x;  // 声明无效
x       // 1

如果第二次声明的时候还进行了赋值，则会覆盖掉前面的值
var x = 1;
var x = 2;

等价于
var x = 1;
var x;
x = 2;

2.1 变量提升
JavaScript 引擎的工作方式是，先解析代码，获取所有被声明的变量，然后再一行一行地运行。这造成的结果，就是所有的变量的声明语句，都会被提升到代码的头部，这就叫做变量提升（hoisting）
console.log(a);
var a = 1;

3. 标识符(identifier)

4. 注释
// 单行注释

/*
多行注释
*/

5. 区块
JavaScript 使用大括号，将多个相关的语句组合在一起，称为“区块”（block）
对于var命令来说，JavaScript 的区块不构成单独的作用域（scope）
{
    a = 1;
}

a   // 1

6. 条件语句
if (m === 0) {
  // ...
} else if (m === 1) {
  // ...
} else if (m === 2) {
  // ...
} else {
  // ...
}

相等运算符 ==
严格相等运算符 ===

switch (fruit) {
  case "banana":
    // ...
    break;
  case "apple":
    // ...
    break;
  default:
    // ...
}

三元运算符
var even = (n % 2 === 0) ? true : false;
等价于
var even;
if (n % 2 === 0) {
  even = true;
} else {
  even = false;
}

7. 循环语句
// 1
var i = 0;

while (i < 100) {
  console.log('i 当前为：' + i);
  i = i + 1;
}

// 2
var x = 3;
for (var i = 0; i < x; i++) {
  console.log(i);
}

//
for ( ; ; ){
  console.log('Hello World');
}

// 3
var x = 3;
var i = 0;

do {
  console.log(i);
  i++;
} while(i < x);


break 语句和 continue 语句 


标签（label）
// 1
top:
  for (var i = 0; i < 3; i++){
    for (var j = 0; j < 3; j++){
      if (i === 1 && j === 1) break top;
      console.log('i=' + i + ', j=' + j);
    }
  }

// 2
foo: {
  console.log(1);
  break foo;
  console.log('本行不会输出');
}
console.log(2);

// 3
top:
  for (var i = 0; i < 3; i++){
    for (var j = 0; j < 3; j++){
      if (i === 1 && j === 1) continue top;
      console.log('i=' + i + ', j=' + j);
    }
  }


数据类型
primitive type:
number  整数和小数
string  文本
boolean

special type:
undefined    “未定义”或不存在
null    空值

complex type:
object  各种值组成的集合
1.狭义的对象 object
2.数组 array
3.函数 function


typeof 运算符
typeof 123 // "number"
typeof '123' // "string"
typeof false // "boolean"

typeof {} // "object"
typeof [] // "object"

typeof null // "object" (历史原因)

if (typeof v === "undefined") {
  // ...
}

var o = {};
var a = [];
o instanceof Array // false
a instanceof Array // true



null和undefined
null表示空值，即该处的值现在为空。调用函数时，某个参数未设置任何值，这时就可以传入null，表示该参数为空
undefined表示“未定义”，返回undefined的典型场景:
// 变量声明了，但没有赋值
var i;
i // undefined

// 调用函数时，应该提供的参数没有提供，该参数等于 undefined
function f(x) {
  return x;
}
f() // undefined

// 对象没有赋值的属性
var  o = new Object();
o.p // undefined

// 函数没有返回值时，默认返回 undefined
function f() {}
f() // undefined


布尔值
除了下面六个值被转为false，其他值都视为true
undefined
null
false
0
NaN
""或''（空字符串）


注意，空数组（[]）和空对象（{}）对应的布尔值，都是true


- 数值
JavaScript 内部，所有数字都是以64位浮点数形式储存，即使整数也是如此
1 === 1.0 // true

0.1 + 0.2 === 0.3   // false

0.3 / 0.1           // 2.9999999999999996

(0.3 - 0.2) === (0.2 - 0.1) // false


数值的进制
0xff // 255
0o377 // 255
0b11 // 3


有前导0的数值会被视为八进制，但是如果前导0后面有数字8和9，则该数值被视为十进制
0888 // 888
0777 // 511

正零和负零
-0 === +0 // true
0 === -0 // true
0 === +0 // true

+0 // 0
-0 // 0
(-0).toString() // '0'
(+0).toString() // '0'


唯一有区别的场合是，+0或-0当作分母，返回的值是不相等的
(1 / +0) === (1 / -0) // false
除以正零得到+Infinity，除以负零得到-Infinity


NaN (Not a Number)
5 - 'x' // NaN

Math.acos(2) // NaN
Math.log(-1) // NaN
Math.sqrt(-1) // NaN

0 / 0 // NaN

typeof NaN // 'number'


NaN不等于任何值，包括它本身
NaN === NaN // false

数组的indexOf方法内部使用的是严格相等运算符
NaN].indexOf(NaN) // -1


NaN + 32 // NaN
NaN - 32 // NaN
NaN * 32 // NaN
NaN / 32 // NaN



Infinity 

// 场景一
Math.pow(2, 1024)
// Infinity

// 场景二
0 / 0 // NaN
1 / 0 // Infinity

与数值相关的全局方法
将字符串转为整数
// 1 基本用法
parseInt('123') // 123
parseInt('   81') // 81

parseInt(1.23) // 1
// 等同于
parseInt('1.23') // 1

parseInt('8a') // 8
parseInt('12**') // 12
parseInt('12.34') // 12
parseInt('15e2') // 15
parseInt('15px') // 15

parseInt('abc') // NaN
parseInt('.3') // NaN
parseInt('') // NaN
parseInt('+') // NaN
parseInt('+1') // 1

parseInt('0x10') // 16
parseInt('011') // 11

parseInt(1000000000000000000000.5) // 1
// 等同于
parseInt('1e+21') // 1

parseInt(0.0000008) // 8
// 等同于
parseInt('8e-7') // 


// 2 进制转换
parseInt('1000') // 1000
// 等同于
parseInt('1000', 10) // 1000

parseInt('1000', 2) // 8
parseInt('1000', 6) // 216
parseInt('1000', 8) // 512

parseInt('10', 37) // NaN
parseInt('10', 1) // NaN
parseInt('10', 0) // 10
parseInt('10', null) // 10
parseInt('10', undefined) // 10

parseInt('1546', 2) // 1
parseInt('546', 2) // NaN


// 1
parseInt(0x11, 36) // 43
parseInt(0x11, 2) // 1

等同于
parseInt(String(0x11), 36)
parseInt(String(0x11), 2)

等同于
parseInt('17', 36)
parseInt('17', 2)

// 2
parseInt(011, 2) // NaN

等同于
parseInt(String(011), 2)

等同于
parseInt(String(9), 2)



parseFloat方法用于将一个字符串转为浮点数

parseFloat('3.14') // 3.14

parseFloat('314e-2') // 3.14
parseFloat('0.0314E+2') // 3.14

parseFloat('3.14more non-digit characters') // 3.14

parseFloat('\t\v\r12.34\n ') // 12.34


parseFloat([1.23]) // 1.23
// 等同于
parseFloat(String([1.23])) // 1.23


如果字符串的第一个字符不能转化为浮点数，则返回NaN
parseFloat([]) // NaN
parseFloat('FF2') // NaN
parseFloat('') // NaN


isNaN方法可以用来判断一个值是否为NaN
isNaN(NaN) // true
isNaN(123) // false

isNaN只对数值有效，如果传入其他值，会被先转成数值
isNaN('Hello') // true
// 相当于
isNaN(Number('Hello')) // true

isNaN({}) // true
// 等同于
isNaN(Number({})) // true

isNaN(['xzy']) // true
// 等同于
isNaN(Number(['xzy'])) // true

对于空数组和只有一个数值成员的数组，isNaN返回false
isNaN([]) // false
isNaN([123]) // false
isNaN(['123']) // false


使用isNaN之前，最好判断一下数据类型
function myIsNaN(value) {
  return typeof value === 'number' && isNaN(value);
}

判断NaN更可靠的方法是，利用NaN为唯一不等于自身的值的这个特点，进行判断
function myIsNaN(value) {
  return value !== value;
}


isFinite方法返回一个布尔值，表示某个值是否为正常的数值
isFinite(Infinity) // false
isFinite(-Infinity) // false
isFinite(NaN) // false
isFinite(undefined) // false
isFinite(null) // true
isFinite(-1) // true

除了Infinity、-Infinity、NaN和undefined这几个值会返回false，isFinite对于其他的数值都会返回true



- 字符串
字符串就是零个或多个排在一起的字符，放在单引号或双引号之中

单引号字符串的内部，可以使用双引号。双引号字符串的内部，可以使用单引号
'key = "value"'
"It's a long journey"

var longString = 'Long \
long \
long \
string';

longString  // "Long long long string"

连接运算符（+）可以连接多个单行字符串
var longString = 'Long '
  + 'long '
  + 'long '
  + 'string';

转义
'\251' // "©"
'\xA9' // "©"
'\u00A9' // "©"

'\172' === 'z' // true
'\x7A' === 'z' // true
'\u007A' === 'z' // true


字符串与数组
字符串可以被视为字符数组

var s = 'hello';
s[0] // "h"
s[1] // "e"
s[4] // "o"

// 直接对字符串使用方括号运算符
'hello'[1] // "e"

'abc'[3] // undefined
'abc'[-1] // undefined
'abc'['x'] // undefined

无法改变字符串之中的单个字符
var s = 'hello';

delete s[0];
s // "hello"

s[1] = 'a';
s // "hello"

s[5] = '!';
s // "hello"


length属性返回字符串的长度，该属性也是无法改变的
var s = 'hello';
s.length // 5

s.length = 3;
s.length // 5

s.length = 7;
s.length // 5

JavaScript 引擎内部，所有字符都用 Unicode 表示


Base64 转码
文本里面包含一些不可打印的符号，比如 ASCII 码0到31的符号都无法打印出来，这时可以使用 Base64 编码，将它们转成可以打印的字符。另一个场景是，有时需要以文本格式传递二进制数据，那么也可以使用 Base64 编码
btoa()：任意值转为 Base64 编码
atob()：Base64 编码转为原来的值

var string = 'Hello World!';
btoa(string) // "SGVsbG8gV29ybGQh"
atob('SGVsbG8gV29ybGQh') // "Hello World!"

这两个方法不适合非 ASCII 码的字符，会报错
btoa('你好') // 报错

要将非 ASCII 码字符转为 Base64 编码，必须中间插入一个转码环节
function b64Encode(str) {
  return btoa(encodeURIComponent(str));
}

function b64Decode(str) {
  return decodeURIComponent(atob(str));
}

b64Encode('你好') // "JUU0JUJEJUEwJUU1JUE1JUJE"
b64Decode('JUU0JUJEJUEwJUU1JUE1JUJE') // "你好"


对象
对象就是一组“键值对”（key-value）的集合，是一种无序的复合数据集合
var obj = {
  foo: 'Hello',
  bar: 'World'
};

键名：
对象的所有键名都是字符串，所以加不加引号都可以
如果键名是数值，会被自动转为字符串
如果键名不符合标识名的条件（比如第一个字符为数字，或者含有空格或运算符），且也不是数字，则必须加上引号，否则会报错
键名又称为“属性”（property）

键值
键值 可以是任何数据类型
如果一个属性的值为函数，通常把这个属性称为“方法”，它可以像函数那样调用

var obj = {
  p: function (x) {
    return 2 * x;
  }
};

obj.p(1) // 2

如果属性的值还是一个对象，就形成了链式引用
var o1 = {};
var o2 = { bar: 'hello' };

o1.foo = o2;
o1.foo.bar // "hello"

```

- 对象
```js
对象的属性之间用逗号分隔，最后一个属性后面可以加逗号（trailing comma），也可以不加
var obj = {
  foo: 'Hello',
  bar: 'World',
};

属性可以动态创建，不必在对象声明时就指定
var obj = {};
obj.foo = 123;
obj.foo // 123

对象的引用
如果不同的变量名指向同一个对象，那么它们都是这个对象的引用，也就是说指向同一个内存地址。修改其中一个变量，会影响到其他所有变量
var o1 = {};
var o2 = o1;

o1.a = 1;
o2.a // 1

o2.b = 2;
o1.b // 2


如果取消某一个变量对于原对象的引用，不会影响到另一个变量（这种引用只局限于对象，如果两个变量指向同一个原始类型的值。那么，变量这时都是值的拷贝）
var o1 = {};
var o2 = o1;

o1 = 1;
o2 // {}


表达式还是语句？
{ foo: 123 }

为了避免这种歧义，JavaScript 引擎的做法是，如果遇到这种情况，无法确定是对象还是代码块，一律解释为代码块
例如：{ console.log(123) } // 123

如果要解释为对象，最好在大括号前加上圆括号。因为圆括号的里面，只能是表达式，所以确保大括号只能解释为对象
({ foo: 123 }) // 正确
({ console.log(123) }) // 报错

这种差异在eval语句（作用是对字符串求值）中反映得最明显
eval('{foo: 123}') // 123
eval('({foo: 123})') // {foo: 123}
上面代码中，如果没有圆括号，eval将其理解为一个代码块；加上圆括号以后，就理解成一个对象


属性的操作

属性的读取：
1.使用点运算符
2.使用方括号运算符

var obj = {
  p: 'Hello World'
};

obj.p // "Hello World"
obj['p'] // "Hello World


注意，如果使用方括号运算符，键名必须放在引号里面，否则会被当作变量处理
var foo = 'bar';

var obj = {
  foo: 1,
  bar: 2
};

obj.foo  // 1
obj[foo]  // 2


方括号运算符内部还可以使用表达式
obj['hello' + ' world']
obj[3 + 3]


数字键可以不加引号，因为会自动转成字符串
var obj = {
  0.7: 'Hello World'
};

obj['0.7'] // "Hello World"
obj[0.7] // "Hello World"


注意，数值键名不能使用点运算符（因为会被当成小数点），只能使用方括号运算符
var obj = {
  123: 'hello world'
};

obj.123 // 报错
obj[123] // "hello world"


属性的赋值：
var obj = {};

obj.foo = 'Hello';
obj['bar'] = 'World';


```