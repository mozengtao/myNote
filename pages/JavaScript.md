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


属性的查看:
var obj = {
  key1: 1,
  key2: 2
};

Object.keys(obj);
// ['key1', 'key2']


属性的删除:
delete命令用于删除对象的属性，删除成功后返回true
var obj = { p: 1 };
Object.keys(obj) // ["p"]

delete obj.p // true
obj.p // undefined
Object.keys(obj) // []

注意，删除一个不存在的属性，delete不报错，而且返回true
var obj = {};
delete obj.p // true


只有一种情况，delete命令会返回false，那就是该属性存在，且不得删除
var obj = Object.defineProperty({}, 'p', {
  value: 123,
  configurable: false
});

obj.p // 123
delete obj.p // false

需要注意的是，delete命令只能删除对象本身的属性，无法删除继承的属性
var obj = {};
delete obj.toString // true
obj.toString // function toString() { [native code] }
toString是对象obj继承的属性，虽然delete命令返回true，但该属性并没有被删除，依然存在
这个例子还说明，即使delete返回true，该属性依然可能读取到值


属性是否存在:
in运算符用于检查对象是否包含某个属性（注意，检查的是键名，不是键值）
var obj = { p: 1 };
'p' in obj // true
'toString' in obj // true
in运算符的一个问题是，它不能识别哪些属性是对象自身的，哪些属性是继承的

使用对象的hasOwnProperty方法判断一下，是否为对象自身的属性
var obj = {};
if ('toString' in obj) {
  console.log(obj.hasOwnProperty('toString')) // false
}

属性的遍历:
for...in循环用来遍历一个对象的全部属性
var obj = {a: 1, b: 2, c: 3};

for (var i in obj) {
  console.log('键名：', i);
  console.log('键值：', obj[i]);
}

for...in循环使用注意点:
它遍历的是对象所有可遍历（enumerable）的属性，会跳过不可遍历的属性
它不仅遍历对象自身的属性，还遍历继承的属性

var person = { name: '老张' };

for (var key in person) {
  if (person.hasOwnProperty(key)) {
    console.log(key);
  }
}

with语句
作用是操作同一个对象的多个属性时，提供一些书写的方便
// 例一
var obj = {
  p1: 1,
  p2: 2,
};
with (obj) {
  p1 = 4;
  p2 = 5;
}
// 等同于
obj.p1 = 4;
obj.p2 = 5;

// 例二
with (document.links[0]){
  console.log(href);
  console.log(title);
  console.log(style);
}
// 等同于
console.log(document.links[0].href);
console.log(document.links[0].title);
console.log(document.links[0].style);

注意，如果with区块内部有变量的赋值操作，必须是当前对象已经存在的属性，否则会创造一个当前作用域的全局变量
var obj = {};
with (obj) {
  p1 = 4;
  p2 = 5;
}

obj.p1 // undefined
p1 // 4

因为with区块没有改变作用域，它的内部依然是当前作用域。这造成了with语句的一个很大的弊病，就是绑定对象不明确
with (obj) {
  console.log(x);
}
无法判断x到底是全局变量，还是对象obj的一个属性


如下，建议不要使用with语句，可以考虑用一个临时变量代替wit
with(obj1.obj2.obj3) {
  console.log(p1 + p2);
}

// 可以写成
var temp = obj1.obj2.obj3;
console.log(temp.p1 + temp.p2);

```

- 函数
```js
函数声明
// function 命令
function print(s) {
  console.log(s);
}

// 函数表达式 (匿名函数又称函数表达式（Function Expression）)
var print = function(s) {
  console.log(s);
};

var f = function f() {};

// Function 构造函数 (非常不直观，几乎无人使用)
var add = new Function(
  'x',
  'y',
  'return x + y'
);

// 等同于
function add(x, y) {
  return x + y;
}

可以传递任意数量的参数给Function构造函数，只有最后一个参数会被当做函数体，如果只有一个参数，该参数就是函数体

var foo = new Function(
  'return "hello world";'
);

// 等同于
function foo() {
  return 'hello world';
}
Function构造函数可以不使用new命令，返回结果完全一样


函数的重复声明:
如果同一个函数被多次声明，后面的声明就会覆盖前面的声明

function fib(num) {
  if (num === 0) return 0;
  if (num === 1) return 1;
  return fib(num - 2) + fib(num - 1);
}

fib(6) // 8


函数是第一等公民
JavaScript 语言将函数看作一种值，与其它值（数值、字符串、布尔值等等）地位相同。凡是可以使用值的地方，就能使用函数
函数只是一个可以执行的值，此外并无特殊之处
function add(x, y) {
  return x + y;
}

// 将函数赋值给一个变量
var operator = add;

// 将函数作为参数和返回值
function a(op){
  return op;
}
a(add)(1, 1)
// 2


函数的属性和方法：
name 属性：
函数的name属性返回函数的名字
function f1() {}
f1.name // "f1"

var f2 = function () {};
f2.name // "f2"
只有在变量的值是一个匿名函数时才是如此

var f3 = function myName() {};
f3.name // 'myName'
f3.name返回函数表达式的名字。注意，真正的函数名还是f3，而myName这个名字只在函数体内部可用

name属性的一个用处，就是获取参数函数的名字
var myFunc = function () {};

function test(f) {
  console.log(f.name);
}

test(myFunc) // myFunc



length 属性：
函数的length属性返回函数预期传入的参数个数，即函数定义之中的参数个数
function f(a, b) {}
f.length // 2
不管调用时输入了多少个参数，length属性始终等于2


length属性提供了一种机制，判断定义时和调用时参数的差异，以便实现面向对象编程的“方法重载”（overload）





toString()：
函数的toString()方法返回一个字符串，内容是函数的源码(函数内部的注释也可以返回)
function f() {
  a();
  b();
  c();
}

f.toString()


对于那些原生的函数，toString()方法返回function (){[native code]}
Math.sqrt.toString()
// "function sqrt() { [native code] }"


函数作用域

函数参数不是必需的，JavaScript 允许省略参数
function f(a, b) {
  return a;
}

f(1, 2, 3) // 1
f(1) // 1
f() // undefined

f.length // 2


参数传递方式
函数参数如果是原始类型的值（数值、字符串、布尔值），传递方式是传值传递（passes by value）
如果函数参数是复合类型的值（数组、对象、其他函数），传递方式是传址传递（pass by reference）

同名参数
如果有同名的参数，则取最后出现的那个值
// 1
function f(a, a) {
  console.log(a);
}

f(1, 2) // 2

// 2
function f(a, a) {
  console.log(a);
}

f(1) // undefined

// 3
function f(a, a) {
  console.log(arguments[0]);
}

f(1) // 1


arguments 对象:
用来在函数体内部读取所有参数
// 1
var f = function (one) {
  console.log(arguments[0]);
  console.log(arguments[1]);
  console.log(arguments[2]);
}

f(1, 2, 3)
// 1
// 2
// 3

// 2 (arguments对象可以在运行时修改)
var f = function(a, b) {
  arguments[0] = 3;
  arguments[1] = 2;
  return a + b;
}

f(1, 1) // 5

// 3
严格模式下，arguments对象与函数参数不具有联动关系
var f = function(a, b) {
  'use strict'; // 开启严格模式
  arguments[0] = 3;
  arguments[1] = 2;
  return a + b;
}

f(1, 1) // 2


// 4
通过arguments对象的length属性，可以判断函数调用时到底带几个参数
function f() {
  return arguments.length;
}

f(1, 2, 3) // 3
f(1) // 1
f() // 0

虽然arguments很像数组，但它是一个对象。数组专有的方法（比如slice和forEach），不能在arguments对象上直接使用

将arguments转为真正的数组的方法：
1.slice方法
2.逐一填入新数组
var args = Array.prototype.slice.call(arguments);

// 或者
var args = [];
for (var i = 0; i < arguments.length; i++) {
  args.push(arguments[i]);
}


callee 属性：
arguments对象带有一个callee属性，返回它所对应的原函数
var f = function () {
  console.log(arguments.callee === f);
}

f() // true
可以通过arguments.callee，达到调用函数自身的目的。这个属性在严格模式里面是禁用的，因此不建议使用


闭包
起因：
function f1() {
  var n = 999;
  function f2() {
    console.log(n);
  }
  return f2;
}

var result = f1();
result(); // 999

函数f1的返回值就是函数f2，由于f2可以读取f1的内部变量，所以就可以在外部获得f1的内部变量
闭包就是函数f2，即能够读取其他函数内部变量的函数
可以把闭包简单理解成“定义在一个函数内部的函数”


闭包就是将函数内部和函数外部连接起来的一座桥梁
闭包的用处
1.读取外层函数内部的变量
2.让这些变量始终保持在内存中，即闭包可以使得它诞生环境一直存在
3.封装对象的私有属性和私有方法

闭包可以看作是函数内部作用域的一个接口

// 1
function createIncrementor(start) {
  return function () {
    return start++;
  };
}

var inc = createIncrementor(5);

inc() // 5
inc() // 6
inc() // 7

为什么闭包能够返回外层函数的内部变量？
闭包（上例的inc）用到了外层变量（start），导致外层函数（createIncrementor）不能从内存释放。只要闭包没有被垃圾回收机制清除，外层函数提供的运行环境也不会被清除，它的内部变量就始终保存着当前值，供闭包读取


// 封装对象的私有属性和私有方法
function Person(name) {
  var _age;
  function setAge(n) {
    _age = n;
  }
  function getAge() {
    return _age;
  }

  return {
    name: name,
    getAge: getAge,
    setAge: setAge
  };
}

var p1 = Person('张三');
p1.setAge(25);
p1.getAge() // 25

函数Person的内部变量_age，通过闭包getAge和setAge，变成了返回对象p1的私有变量

外层函数每次运行，都会生成一个新的闭包，而这个闭包又会保留外层函数的内部变量，所以内存消耗很大



立即调用的函数表达式
在定义函数之后，立即调用该函数

function(){ /* code */ }();
// SyntaxError: Unexpected token (
产生这个错误的原因是，function这个关键字既可以当作语句，也可以当作表达式

// 语句
function f() {}

// 表达式
var f = function f() {}

当作表达式时，函数可以定义后直接加圆括号调用
var f = function f(){ return 1}();
f // 1

为了避免解析的歧义，JavaScript 规定，如果function关键字出现在行首，一律解释成语句


函数定义后立即调用的解决方法
(function(){ /* code */ }());
// 或者
(function(){ /* code */ })();

以圆括号开头，引擎就会认为后面跟的是一个表达式，而不是函数定义语句，所以就避免了错误。这就叫做“立即调用的函数表达式”（Immediately-Invoked Function Expression），简称 IIFE


任何让解释器以表达式来处理函数定义的方法，都能产生同样的效果

var i = function(){ return 10; }();
true && function(){ /* code */ }();
0, function(){ /* code */ }();

!function () { /* code */ }();
~function () { /* code */ }();
-function () { /* code */ }();
+function () { /* code */ }();

通常情况下，只对匿名函数使用这种“立即执行的函数表达式”，目的如下
1.不必为函数命名，避免了污染全局变量
2.IIFE 内部形成了一个单独的作用域，可以封装一些外部无法读取的私有变量

// 写法一
var tmp = newData;
processData(tmp);
storeData(tmp);

// 写法二 （更好，因为完全避免了污染全局变量）
(function () {
  var tmp = newData;
  processData(tmp);
  storeData(tmp);
}());
```

- eval 命令
```js
eval命令接受一个字符串作为参数，并将这个字符串当作语句执行
如果eval的参数不是字符串，那么会原样返回

// 1
eval('var a = 1;');
a // 1

// 2
eval(123) // 123

eval没有自己的作用域，都在当前作用域内执行，因此可能会修改当前作用域的变量的值，造成安全问题
//
var a = 1;
eval('a = 2');

a // 2

JavaScript 规定，如果使用严格模式，eval内部声明的变量，不会影响到外部作用域
(function f() {
  'use strict';
  eval('var foo = 123');
  console.log(foo);  // ReferenceError: foo is not defined
})()

总之，eval的本质是在当前作用域之中，注入代码。由于安全风险和不利于 JavaScript 引擎优化执行速度，一般不推荐使用。通常情况下，eval最常见的场合是解析 JSON 数据的字符串，不过正确的做法应该是使用原生的JSON.parse方法

```

- 数组
```js
// 1
var arr = ['a', 'b', 'c'];

// 2
var arr = [];

arr[0] = 'a';
arr[1] = 'b';
arr[2] = 'c';

// 3
var arr = [
  {a: 1},
  [1, 2, 3],
  function() {return true;}
];

arr[0] // Object {a: 1}
arr[1] // [1, 2, 3]
arr[2] // function (){return true;}

// 4
var a = [[1, 2], [3, 4]];
a[0][1] // 2
a[1][1] // 4


数组的本质:
本质上，数组属于一种特殊的对象

// 1
typeof [1, 2, 3] // "object"

数组的特殊性体现在，它的键名是按次序排列的一组整数（0，1，2...）
var arr = ['a', 'b', 'c'];

Object.keys(arr)
// ["0", "1", "2"]


对于数值的键名，不能使用点结构
var arr = [1, 2, 3];
arr.0 // SyntaxError

只能使用arr[x]的方式（方括号是运算符，可以接受数值）


数组的length 属性
该属性是一个动态的值，等于键名中的最大整数加上1
// 1
['a', 'b', 'c'].length // 3

// 2
var arr = ['a', 'b'];
arr.length // 2

arr[2] = 'c';
arr.length // 3

arr[9] = 'd';
arr.length // 10

arr[1000] = 'e';
arr.length // 1001


length属性是可写的
var arr = [ 'a', 'b', 'c' ];
arr.length // 3

arr.length = 2;
arr // ["a", "b"]

清空数组的一个有效方法，就是将length属性设为0
var arr = [ 'a', 'b', 'c' ];

arr.length = 0;
arr // []

// 2
var a = ['a'];

a.length = 3;
a[1] // undefined

由于数组本质上是一种对象，所以可以为数组添加属性，但是这不影响length属性的值
var a = [];

a['p'] = 'abc';
a.length // 0

a[2.1] = 'abc';
a.length // 0

如果数组的键名是添加超出范围的数值，该键名会自动转为字符串
var arr = [];
arr[-1] = 'a';
arr[Math.pow(2, 32)] = 'b';

arr.length // 0
arr[-1] // "a"
arr[4294967296] // "b"


in 运算符
// 1
var arr = [ 'a', 'b', 'c' ];
2 in arr  // true
'2' in arr // true
4 in arr // false

注意，如果数组的某个位置是空位，in运算符返回false
// 
var arr = [];
arr[100] = 'a';

100 in arr // true
1 in arr // false

数组遍历
// 1
var a = [1, 2, 3];

for (var i in a) {
  console.log(a[i]);
}


for...in遍历数组也也遍历到了非整数键，所以，不推荐使用for...in遍历数组。
数组的遍历可以考虑使用for循环或while循环
// 1
var a = [1, 2, 3];

// for循环
for(var i = 0; i < a.length; i++) {
  console.log(a[i]);
}

// while循环
var i = 0;
while (i < a.length) {
  console.log(a[i]);
  i++;
}

var l = a.length;
while (l--) {
  console.log(a[l]);
}

// 2
var colors = ['red', 'green', 'blue'];
colors.forEach(function (color) {
  console.log(color);
});
// red
// green
// blue

数组的空位 (hole)
// 1
var a = [1, , 1];
a.length // 3

// 2
var a = [1, 2, 3,];

a.length // 3
a // [1, 2, 3]

// 3 数组的空位是可以读取的，返回undefined
var a = [, , ,];
a[1] // undefined

// 4 使用delete命令删除一个数组成员，会形成空位，并且不会影响length属性
var a = [1, 2, 3];
delete a[1];

a[1] // undefined
a.length // 3

数组的某个位置是空位，与某个位置是undefined，是不一样的。如果是空位，使用数组的forEach方法、for...in结构、以及Object.keys方法进行遍历，空位都会被跳过

// 1
var a = [, , ,];

a.forEach(function (x, i) {
  console.log(i + '. ' + x);
})
// 不产生任何输出

for (var i in a) {
  console.log(i);
}
// 不产生任何输出

Object.keys(a)
// []


如果某个位置是undefined，遍历的时候就不会被跳过
// 1
var a = [undefined, undefined, undefined];

a.forEach(function (x, i) {
  console.log(i + '. ' + x);
});
// 0. undefined
// 1. undefined
// 2. undefined

for (var i in a) {
  console.log(i);
}
// 0
// 1
// 2

Object.keys(a)
// ['0', '1', '2']

空位就是数组没有这个元素，所以不会被遍历到，而undefined则表示数组有这个元素，值是undefined，所以遍历不会跳过


类似数组的对象
如果一个对象的所有键名都是正整数或零，并且有length属性，那么这个对象就很像数组，语法上称为“类似数组的对象”（array-like object）

“类似数组的对象”的根本特征，就是具有length属性。只要有length属性，就可以认为这个对象类似于数组。但是有一个问题，这种length属性不是动态值，不会随着成员的变化而变化

// 1
var obj = {
  length: 0
};
obj[3] = 'd';
obj.length // 0

典型的“类似数组的对象”是函数的arguments对象，以及大多数 DOM 元素集，还有字符串
// arguments对象
function args() { return arguments }
var arrayLike = args('a', 'b');

arrayLike[0] // 'a'
arrayLike.length // 2
arrayLike instanceof Array // false

// DOM元素集
var elts = document.getElementsByTagName('h3');
elts.length // 3
elts instanceof Array // false

// 字符串
'abc'[1] // 'b'
'abc'.length // 3
'abc' instanceof Array // false

数组的slice方法可以将“类似数组的对象”变成真正的数组
var arr = Array.prototype.slice.call(arrayLike);

除了转为真正的数组，“类似数组的对象”还有一个办法可以使用数组的方法，就是通过call()把数组的方法放到对象上面
function print(value, index) {
  console.log(index + ' : ' + value);
}

Array.prototype.forEach.call(arrayLike, print);


// 2
// forEach 方法
function logArgs() {
  Array.prototype.forEach.call(arguments, function (elem, i) {
    console.log(i + '. ' + elem);
  });
}

// 等同于 for 循环
function logArgs() {
  for (var i = 0; i < arguments.length; i++) {
    console.log(i + '. ' + arguments[i]);
  }
}

字符串也是类似数组的对象，所以也可以用Array.prototype.forEach.call遍历
Array.prototype.forEach.call('abc', function (chr) {
  console.log(chr);
});
// a
// b
// c

这种方法比直接使用数组原生的forEach要慢，所以最好还是先将“类似数组的对象”转为真正的数组，然后再直接调用数组的forEach方法
var arr = Array.prototype.slice.call('abc');
arr.forEach(function (chr) {
  console.log(chr);
});
// a
// b
// c

```