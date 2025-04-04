- [Web APIs](https://developer.mozilla.org/en-US/docs/Web/API)
- [The JavaScript language](https://javascript.info/js)
- [JavaScript](https://developer.mozilla.org/en-US/docs/Learn/JavaScript)
- [JavaScript](https://developer.mozilla.org/en-US/docs/Web/JavaScript)
- [The Modern JavaScript Tutorial](https://javascript.info/)
- [Airbnb JavaScript Style Guide](https://github.com/airbnb/javascript)
- [JavaScript Functions Tutorial](https://www.freecodecamp.org/news/javascript-function-iife-parameters-code-blocks-explained/)
- [JavaScript教程](https://www.liaoxuefeng.com/wiki/1022910821149312)
- [JavaScript 教程](https://wangdoc.com/javascript/)

```js
A JavaScript function is an executable piece of code developers use to bundle a block of zero or more statements.
The instructions in a function's body can be a variable declaration, return call, console.log() invocation, function definition, or any other JavaScript statements.

function nameOfFunction(parameter1, parameter2, ..., parameterX) {
  // function's body
}

// Define a function with two parameters:
function myName(firstName, lastName) {
  console.log(`My full name is ${firstName} ${lastName}.`);
}

// Define a function with a default parameter
function myName(firstName, lastName = "Sofela") {
  console.log(`My full name is ${firstName} ${lastName}.`);
}

// A block is a pair of braces {...} used to group multiple statements together
function getTime() {
  const hourNow = new Date().getHours();
  const minutesNow = new Date().getMinutes();
  console.log(`The time is ${hourNow}:${minutesNow}.`);
}

// A function body is where you place a sequence of statements you want to execute
const bestColors = ["Coral", "Blue", "DeepPink"];

function updateMyBestColors(previousColors, newColor) {
   const mybestColors = [...previousColors, newColor];
   return mybestColors;
}

updateMyBestColors(bestColors, "GreenYellow");


Four types of JavaScript functions:
1.Function declaration
2.Function expression
3.Arrow function expression
4.Immediately invoking function expression

//1 A function declaration is a function created without assigning it to a variable
function addNumbers() {
  return 100 + 20;
}

//2 A function expression is a function you create and assign to a variable.
const myFuncExpr = function addNumbers() {
  return 100 + 20;
};

// anonymous function expression can also be used
const myFuncExpr = function() {
  return 100 + 20;
};

// 3 An arrow function expression is a shorthand way to write a function expression
// You have to omit the function keyword and function name while writing an arrow function expression. Otherwise, JavaScript will throw a SyntaxError.
// 3.1
const myFuncExpr = () => {
  return 100 + 20;
};

Important stuff to know about the JavaScript arrow function expression:
1.The parameters' parentheses are optional
2.The curly brackets and return keyword are optional
3.Use parentheses to wrap any implicit object return

// 1 arrow function contains only a single parameter. In such a case, its parentheses can be omitted
const myFuncExpr = a => {
  return a + 20;
};

// 2 arrow function contains only a single statement. In that case, you can omit its curly brackets and the return keyword
const myFuncExpr = (x, y) => x + y;

// 3 Suppose you wish to return an object implicitly. In such a case, wrap the object in a grouping operator (...)
const myFuncExpr = () => ({
  carColor: "White",
  shoeColor: "Yellow",
});

// you can use the grouping operator to return any single value
const myFuncExpr = x => (x + 56);

// 4 An immediately invoked function expression (IIFE) is a function expression that invokes itself automatically.
// We sometimes call an IIFE a "Self-Invoking Function Expression" or "Self-Executing Anonymous Function Expression."
(function() {
  /* ... */
})();

An IIFE is composed of three main components:
1.A grouping operator: The first pair of parentheses ()
2.A function: Enclosed within the grouping operator
3.An invocator: The last pair of parentheses ()

// define a named IIFE
// The snippet enclosed the addNumbers function in parentheses to make the computer evaluate its data type before invoking it immediately after the evaluation
(function addNumbers() {
  console.log(100 + 20);
})();

// define an anonymous IIFE
(function() {
  console.log(100 + 20);
})();

// define an arrow function IIFE
(() => console.log(100 + 20))();

// define an async IIFE
(async () => console.log(await 100 + 20))();

```


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

- 运算符
```js
算术运算符
x + y
x - y
x * y
x / y
x ** y
x % y
++x 或者 x++
--x 或者 x--
 +x
-x

// 1
true + true // 2
1 + true // 2

'a' + 'bc' // "abc" (字符串的连接运算符)

1 + 'a' // "1a"
false + 'a' // "falsea"

'3' + 4 + 5 // "345"    （如果一个运算子是字符串，非字符串会转成字符串）
3 + 4 + '5' // "75"


除了加法运算符，其他算术运算符（比如减法、除法和乘法）都不会发生重载。
它们的规则是：所有运算子一律转为数值，再进行相应的数学运算

1 - '2' // -1
1 * '2' // 2
1 / '2' // 0.5


如果运算子是对象，必须先转成原始类型的值，然后再相加
var obj = { p: 1 };
obj + 2 // "[object Object]2"   // (对象obj转成原始类型的值是[object Object])

对象转成原始类型的值的规则
1.自动调用对象的valueOf方法（对象的valueOf方法总是返回对象自身）
2.自动调用对象的toString方法，将其转为字符串

var obj = { p: 1 };
obj.valueOf().toString() // "[object Object]"

自定义toString方法
var obj = {
  toString: function () {
    return 'hello';
  }
};

obj + 2 // "hello2"


特例，如果运算子是一个Date对象的实例，那么会优先执行toString方法
var obj = new Date();
obj.valueOf = function () { return 1 };
obj.toString = function () { return 'hello' };

obj + 2 // "hello2"
对象obj是一个Date对象的实例，并且自定义了valueOf方法和toString方法，结果toString方法优先执行

数值运算符（+）它是一元运算符
数值运算符的作用在于可以将任何值转为数值

+true // 1
+[] // 0
+{} // NaN

负数值运算符（-），也同样具有将一个值转为数值的功能，只不过得到的值正负相反
var x = 1;
-x // -1
-(-x) // 

赋值运算符
var x = 1;
x += y
x -= y
x *= y
x /= y
x %= y
x **= y
x >>= y
x <<= y
x >>>= y
x &= y
x |= y
x ^= y

比较运算符
JavaScript 一共提供了8个比较运算符
> 大于运算符
< 小于运算符
<= 小于或等于运算符
>= 大于或等于运算符
== 相等运算符
=== 严格相等运算符
!= 不相等运算符
!== 严格不相等运算符

八个比较运算符分成两类：相等比较和非相等比较
非相等比较：
先看两个运算子是否都是字符串，如果是的，就按照字典顺序比较（实际上是比较 Unicode 码点），否则，将两个运算子都转成数值，再比较数值的大小

字符串的比较：
字符串按照字典顺序进行比较
'cat' > 'dog' // false
'cat' > 'catalog' // false

非字符串的比较：
1.原始类型值
如果两个运算子都是原始类型的值，则是先转成数值再比较
5 > '4' // true
// 等同于 5 > Number('4')
// 即 5 > 4

true > false // true
// 等同于 Number(true) > Number(false)
// 即 1 > 0

2 > true // true
// 等同于 2 > Number(true)
// 即 2 > 1

任何值（包括NaN本身）与NaN使用非相等运算符进行比较，返回的都是false
1 > NaN // false
1 <= NaN // false
'1' > NaN // false
'1' <= NaN // false
NaN > NaN // false
NaN <= NaN // false

2.对象
如果运算子是对象，会转为原始类型的值，再进行比较
对象转换成原始类型的值，算法是先调用valueOf方法；如果返回的还是对象，再接着调用toString方法
var x = [2];
x > '11' // true
// 等同于 [2].valueOf().toString() > '11'
// 即 '2' > '11'

x.valueOf = function () { return '1' };
x > '11' // false
// 等同于 (function () { return '1' })() > '11'
// 即 '1' > '11'

[2] > [1] // true
// 等同于 [2].valueOf().toString() > [1].valueOf().toString()
// 即 '2' > '1'

[2] > [11] // true
// 等同于 [2].valueOf().toString() > [11].valueOf().toString()
// 即 '2' > '11'

({ x: 2 }) >= ({ x: 1 }) // true
// 等同于 ({ x: 2 }).valueOf().toString() >= ({ x: 1 }).valueOf().toString()
// 即 '[object Object]' >= '[object Object]

严格相等运算符
相等运算符（==）比较两个值是否相等
严格相等运算符（===）比较它们是否为“同一个值”。如果两个值不是同一类型，严格相等运算符（===）直接返回false，而相等运算符（==）会将它们转换成同一个类型，再用严格相等运算符进行比较
1.不同类型的值
如果两个值的类型不同，直接返回false
1 === "1" // false
true === "true" // false

2.同一类的原始类型值
同一类型的原始类型的值（数值、字符串、布尔值）比较时，值相同就返回true，值不同就返回false
1 === 0x1 // true

NaN与任何值都不相等（包括自身）。另外，正0等于负0
NaN === NaN  // false
+0 === -0 // true

复合类型值
两个复合类型（对象、数组、函数）的数据比较时，比较的是它们是否指向同一个地址
{} === {} // false
[] === [] // false
(function () {} === function () {}) // false
// 运算符两边的空对象、空数组、空函数的值，都存放在不同的内存地址，结果是false

var v1 = {};
var v2 = v1;
v1 === v2 // true

对于两个对象的比较，严格相等运算符比较的是地址，而大于或小于运算符比较的是值
var obj1 = {};
var obj2 = {};

obj1 > obj2 // false
obj1 < obj2 // false
obj1 === obj2 // false

undefined和null与自身严格相等
undefined === undefined // true
null === null // true

var v1;
var v2;
v1 === v2 // true

严格不相等运算符
先求严格相等运算符的结果，然后返回相反值



相等运算符 ==
相等运算符用来比较相同类型的数据时，与严格相等运算符完全一样
1 == 1.0
// 等同于
1 === 1.0

比较不同类型的数据时，相等运算符会先将数据进行类型转换，然后再用严格相等运算符比较
1.原始类型值
原始类型的值会转换成数值再进行比较
1 == true // true
// 等同于 1 === Number(true)

0 == false // true
// 等同于 0 === Number(false)

2 == true // false
// 等同于 2 === Number(true)

2 == false // false
// 等同于 2 === Number(false)

'true' == true // false
// 等同于 Number('true') === Number(true)
// 等同于 NaN === 1

'' == 0 // true
// 等同于 Number('') === 0
// 等同于 0 === 0

'' == false  // true
// 等同于 Number('') === Number(false)
// 等同于 0 === 0

'1' == true  // true
// 等同于 Number('1') === Number(true)
// 等同于 1 === 1

'\n  123  \t' == 123 // true
// 因为字符串转为数字时，省略前置和后置的空格

2.对象与原始类型值比较
对象（这里指广义的对象，包括数组和函数）与原始类型的值比较时，先调用对象的valueOf()方法，如果得到原始类型的值，就按照上一小节的规则，互相比较；如果得到的还是对象，则再调用toString()方法，得到字符串形式，再进行比较
// 1
// 数组与数值的比较
[1] == 1 // true

// 数组与字符串的比较
[1] == '1' // true
[1, 2] == '1,2' // true

// 对象与布尔值的比较
[1] == true // true
[2] == true // false

// 2
const obj = {
  valueOf: function () {
    console.log('执行 valueOf()');
    return obj;
  },
  toString: function () {
    console.log('执行 toString()');
    return 'foo';
  }
};

obj == 'foo'
// 执行 valueOf()
// 执行 toString()
// true

3. undefined 和 null
undefined和null只有与自身比较，或者互相比较时，才会返回true；与其他类型的值比较时，结果都为false

4.相等运算符的缺点
相等运算符隐藏的类型转换，会带来一些违反直觉的结果

不相等运算符 !=
先求相等运算符的结果，然后返回相反值


布尔运算符
取反运算符：!
且运算符：&&
或运算符：||
三元运算符：?:


二进制位运算符
|
&
~
^
<<
>>
>>> 头部补零的右移运算符

头部补零的右移运算符（>>>）与右移运算符（>>）只有一个差别，就是一个数的二进制形式向右移动时，头部一律补零，而不考虑符号位。所以，该运算总是得到正值

位运算符可以用作设置对象属性的开关
var FLAG_A = 1; // 0001
var FLAG_B = 2; // 0010
var FLAG_C = 4; // 0100
var FLAG_D = 8; // 1000

var flags = 5; // 二进制的0101

if (flags & FLAG_C) {
  // ...
}
// 0101 & 0100 => 0100 => true


其他运算符

void运算符的作用是执行一个表达式，然后不返回任何值，或者说返回undefined
void(0) // undefined
void (x = 5) //undefined
这个运算符的主要用途是浏览器的书签工具（Bookmarklet），以及在超级链接中插入代码防止网页跳转
<script>
function f() {
  console.log('Hello World');
}
</script>
<a href="http://example.com" onclick="f(); return false;">点击</a>


逗号运算符用于对两个表达式求值，并返回后一个表达式的值
var x = 0;
var y = (x++, 10);

数据类型的转换
JavaScript 是一种动态类型语言，变量没有类型限制，可以随时赋予任意值

强制转换
强制转换主要指使用Number()、String()和Boolean()三个函数

Number函数，可以将任意类型的值转化成数值
1.原始类型值
// 数值：转换后还是原来的值
Number(324) // 324

// 字符串：如果可以被解析为数值，则转换为相应的数值
Number('324') // 324

// 字符串：如果不可以被解析为数值，返回 NaN
Number('324abc') // NaN

// 空字符串转为0
Number('') // 0

// 布尔值：true 转成 1，false 转成 0
Number(true) // 1
Number(false) // 0

// undefined：转成 NaN
Number(undefined) // NaN

// null：转成0
Number(null) // 0

Number函数将字符串转为数值，要比parseInt函数严格很多。基本上，只要有一个字符无法转成数值，整个字符串就会被转为NaN
parseInt('42 cats') // 42
Number('42 cats') // NaN

parseInt和Number函数都会自动过滤一个字符串前导和后缀的空格
parseInt('\t\v\r12.34\n') // 12
Number('\t\v\r12.34\n') // 12.34


2.对象
Number方法的参数是对象时，将返回NaN，除非是包含单个数值的数组
Number({a: 1}) // NaN
Number([1, 2, 3]) // NaN
Number([5]) // 5


String函数可以将任意类型的值转化成字符串
1.原始类型值
String(123) // "123"
String('abc') // "abc"
String(true) // "true"
String(undefined) // "undefined"
String(null) // "null"

2.对象
String方法的参数如果是对象，返回一个类型字符串；如果是数组，返回该数组的字符串形式

String({a: 1}) // "[object Object]"
String([1, 2, 3]) // "1,2,3"

String方法背后的转换规则：
1.先调用对象自身的toString方法。如果返回原始类型的值，则对该值使用String函数，不再进行以下步骤。

2.如果toString方法返回的是对象，再调用原对象的valueOf方法。如果valueOf方法返回原始类型的值，则对该值使用String函数，不再进行以下步骤。

3.如果valueOf方法返回的是对象，就报错


Boolean()函数可以将任意类型的值转为布尔值
除了以下五个值的转换结果为false，其他的值全部为true
Boolean(undefined) // false
Boolean(null) // false
Boolean(0) // false
Boolean(NaN) // false
Boolean('') // false

自动转换
以下三种情况时，JavaScript 会自动转换数据类型
1.不同类型的数据互相运算
123 + 'abc' // "123abc"
2.对非布尔值类型的数据求布尔值
if ('abc') {
  console.log('hello')
}  // "hello"
3.对非数值类型的值使用一元运算符（即+和-）
+ {foo: 'bar'} // NaN
- [1, 2, 3] // NaN
```

- 错误处理
```js
JavaScript 解析或运行时，一旦发生错误，引擎就会抛出一个错误对象。JavaScript 原生提供Error构造函数，所有抛出的错误都是这个构造函数的实例
var err = new Error('出错了');
err.message // "出错了"

Error实例对象的属性
message：错误提示信息
name：错误名称（非标准属性）
stack：错误的堆栈（非标准属性）


Error实例对象是最一般的错误类型，在它的基础上，存在Error的6个派生对象
1.SyntaxError 对象
2.ReferenceError 对象
3.RangeError 对象
4.TypeError 对象
5.URIError 对象
6.EvalError 对象

自定义错误
function UserError(message) {
  this.message = message || '默认信息';
  this.name = 'UserError';
}

UserError.prototype = new Error();
UserError.prototype.constructor = UserError;

new UserError('这是自定义的错误！');

throw 语句
throw语句的作用是手动中断程序执行，抛出一个错误
var x = -1;

if (x <= 0) {
  throw new Error('x 必须为正数');
}
// Uncaught Error: x 必须为正数

// 抛出自定义错误
function UserError(message) {
  this.message = message || '默认信息';
  this.name = 'UserError';
}

throw new UserError('出错了！');
// Uncaught UserError {message: "出错了！", name: "UserError"}

throw可以抛出任何类型的值。也就是说，它的参数可以是任何值
// 抛出一个字符串
throw 'Error！';
// Uncaught Error！

// 抛出一个数值
throw 42;
// Uncaught 42

// 抛出一个布尔值
throw true;
// Uncaught true

// 抛出一个对象
throw {
  toString: function () {
    return 'Error!';
  }
};
// Uncaught {toString: ƒ}


try...catch 结构
try {
  foo.bar();
} catch (e) {
  if (e instanceof EvalError) {
    console.log(e.name + ": " + e.message);
  } else if (e instanceof RangeError) {
    console.log(e.name + ": " + e.message);
  }
  // ...
}

try...catch结构允许在最后添加一个finally代码块，表示不管是否出现错误，都必需在最后运行的语句

finally代码块用法的典型场景
openFile();

try {
  writeFile(Data);
} catch(e) {
  handleError(e);
} finally {
  closeFile();
}

try...catch...finally三者之间的执行顺序
function f() {
  try {
    console.log(0);
    throw 'bug';
  } catch(e) {
    console.log(1);
    return true; // 这句原本会延迟到 finally 代码块结束再执行
    console.log(2); // 不会运行
  } finally {
    console.log(3);
    return false; // 这句会覆盖掉前面那句 return
    console.log(4); // 不会运行
  }

  console.log(5); // 不会运行
}

var result = f();
// 0
// 1
// 3

result
// false

```

- 编程风格
```js
区块
block {
  // ...
}

圆括号
表示函数调用时，函数名与左括号之间没有空格。
表示函数定义时，函数名与左括号之间没有空格。
其他情况时，前面位置的语法元素与左括号之间，都有一个空格。
// 圆括号表示函数的调用
console.log('abc');

// 圆括号表示表达式的组合
(1 + 2) * 3

全局变量
用大写字母表示变量名
UPPER_CASE

变量声明
把变量声明都放在代码块的头部，避免自动将变量声明“提升”（hoist）到代码块（block）的头部

不要使用with语句

建议不要使用相等运算符（==），只使用严格相等运算符（===）

建议不要将不同目的的语句，合并成一行

建议自增（++）和自减（--）运算符尽量使用+=和-=代替

switch...case不使用大括号，不利于代码形式的统一，建议改写成对象结构
function doAction(action) {
  switch (action) {
    case 'hack':
      return 'hack';
    case 'slash':
      return 'slash';
    case 'run':
      return 'run';
    default:
      throw new Error('Invalid action.');
  }
}

改写为

function doAction(action) {
  var actions = {
    'hack': function () {
      return 'hack';
    },
    'slash': function () {
      return 'slash';
    },
    'run': function () {
      return 'run';
    }
  };

  if (typeof actions[action] !== 'function') {
    throw new Error('Invalid action.');
  }

  return actions[action]();
}


```

- console 对象与控制台
```js
console对象是 JavaScript 的原生对象，它有点像 Unix 系统的标准输出stdout和标准错误stderr，可以输出各种信息到控制台，并且还提供了很多有用的辅助方法

console.log()
console.info()
console.debug()

console.warn()
console.error()

console.table()
对于某些复合类型的数据，console.table方法可以将其转为表格显示
  // 1
  var languages = [
    { name: "JavaScript", fileExtension: ".js" },
    { name: "TypeScript", fileExtension: ".ts" },
    { name: "CoffeeScript", fileExtension: ".coffee" }
  ];

  console.table(languages);

  // 2
  var languages = {
    csharp: { name: "C#", paradigm: "object-oriented" },
    fsharp: { name: "F#", paradigm: "functional" }
  };

  console.table(languages);

console.count()
count方法用于计数，输出它被调用了多少次
该方法可以接受一个字符串作为参数，作为标签，对执行次数进行分类
function greet(user) {
  console.count(user);
  return "hi " + user;
}

greet('bob')
// bob: 1
// "hi bob"

greet('alice')
// alice: 1
// "hi alice"

greet('bob')
// bob: 2
// "hi bob"


console.dir()
dir方法用来对一个对象进行检查（inspect），并以易于阅读和打印的格式显示
console.log({f1: 'foo', f2: 'bar'})
// Object {f1: "foo", f2: "bar"}

console.dir({f1: 'foo', f2: 'bar'})
// Object
//   f1: "foo"
//   f2: "bar"
//   __proto__: Object


console.dirxml()
dirxml方法主要用于以目录树的形式，显示 DOM 节点


console.assert()
console.assert方法主要用于程序运行过程中，进行条件判断，如果不满足条件，就显示一个错误，但不会中断程序执行。这样就相当于提示用户，内部状态不正确

它接受两个参数，第一个参数是表达式，第二个参数是字符串。只有当第一个参数为false，才会提示有错误，在控制台输出第二个参数，否则不会有任何结果

console.assert(false, '判断条件不成立')
// Assertion failed: 判断条件不成立

// 相当于
try {
  if (!false) {
    throw new Error('判断条件不成立');
  }
} catch(e) {
  console.error(e);
}


console.time()
console.timeEnd() 
用于计时，可以算出一个操作所花费的准确时间
console.time('Array initialize');

var array= new Array(1000000);
for (var i = array.length - 1; i >= 0; i--) {
  array[i] = new Object();
};

console.timeEnd('Array initialize');
// Array initialize: 1914.481ms

console.group()
console.groupEnd()
console.groupCollapsed()
console.group和console.groupEnd这两个方法用于将显示的信息分组。它只在输出大量信息时有用，分在一组的信息，可以用鼠标折叠/展开
console.group('一级分组');
console.log('一级分组的内容');

console.group('二级分组');
console.log('二级分组的内容');

console.groupEnd(); // 二级分组结束
console.groupEnd(); // 一级分组结束
console.groupCollapsed方法与console.group方法很类似，唯一的区别是该组的内容，在第一次显示时是收起的（collapsed），而不是展开的
console.groupCollapsed('Fetching Data');

console.log('Request Sent');
console.error('Error: Server not responding (500)');

console.groupEnd();



console.trace()
console.clear()
onsole.trace方法显示当前执行的代码在堆栈中的调用路径
console.clear方法用于清除当前控制台的所有输出，将光标回置到第一行。如果用户选中了控制台的“Preserve log”选项，console.clear方法将不起作用
console.trace()
// console.trace()
//   (anonymous function)
//   InjectedScript._evaluateOn
//   InjectedScript._evaluateAndWrap
//   InjectedScript.evaluate



console对象的所有方法，都可以被覆盖。因此，可以按照自己的需要，定义console.log方法
['log', 'info', 'warn', 'error'].forEach(function(method) {
  console[method] = console[method].bind(
    console,
    new Date().toISOString()
  );
});

console.log("出错了！");
// 2014-05-18T09:00.000Z 出错了！



控制台命令行 API
$_属性返回上一个表达式的值

$0 - $4
控制台保存了最近5个在 Elements 面板选中的 DOM 元素，$0代表倒数第一个（最近一个），$1代表倒数第二个，以此类推直到$4

$(selector)
$$(selector)
$x(path)
inspect(object)
getEventListeners(object)
keys(object)，values(object)
monitorEvents(object[, events]) ，unmonitorEvents(object[, events])



debugger 语句
debugger语句主要用于除错，作用是设置断点。如果有正在运行的除错工具，程序运行到debugger语句时会自动停下。如果没有除错工具，debugger语句不会产生任何结果，JavaScript 引擎自动跳过这一句
```

- 标准库
- Object 对象
```js
JavaScript 的所有其他对象都继承自Object对象，即那些对象都是Object的实例
Object对象的原生方法:
1.Object本身的方法(直接定义在Object对象的方法)
Object.print = function (o) { console.log(o) };
2.Object的实例方法(定义在Object原型对象Object.prototype上的方法)
Object.prototype.print = function () {
  console.log(this);
};

var obj = new Object();
obj.print() // Object


Object()
Object本身是一个函数，可以当作工具方法使用，将任意值转为对象。这个方法常用于保证某个值一定是对象
var obj = Object();
// 等同于
var obj = Object(undefined);
var obj = Object(null);

obj instanceof Object // true

如果参数是原始类型的值，Object方法将其转为对应的包装对象的实例
var obj = Object(1);
obj instanceof Object // true
obj instanceof Number // true

var obj = Object('foo');
obj instanceof Object // true
obj instanceof String // true

var obj = Object(true);
obj instanceof Object // true
obj instanceof Boolean // true

如果Object方法的参数是一个对象，它总是返回该对象
var arr = [];
var obj = Object(arr); // 返回原数组
obj === arr // true

var value = {};
var obj = Object(value) // 返回原对象
obj === value // true

var fn = function () {};
var obj = Object(fn); // 返回原函数
obj === fn // true

判断变量是否为对象的函数
function isObject(value) {
  return value === Object(value);
}

isObject([]) // true
isObject(true) // false


Object 构造函数
var obj = new Object();
该写法生成新对象，与字面量的写法var obj = {}是等价的。或者说，后者只是前者的一种简便写法

Object(value)与new Object(value)两者虽然用法相似，但语义是不同的
var o1 = {a: 1};
var o2 = new Object(o1);
o1 === o2 // true

var obj = new Object(123);
obj instanceof Number // true
Object(value)表示将value转成一个对象，new Object(value)则表示新生成一个对象，它的值是value


Object 的静态方法

Object.keys()
Object.getOwnPropertyNames()
Object.keys方法和Object.getOwnPropertyNames方法都用来遍历对象的属性
Object.keys方法的参数是一个对象，返回一个数组。该数组的成员都是该对象自身的（而不是继承的）所有属性名
var obj = {
  p1: 123,
  p2: 456
};

Object.keys(obj) // ["p1", "p2"]

Object.getOwnPropertyNames方法与Object.keys类似，也是接受一个对象作为参数，返回一个数组，包含了该对象自身的所有属性名
var obj = {
  p1: 123,
  p2: 456
};

Object.getOwnPropertyNames(obj) // ["p1", "p2"]

Object.keys方法只返回可枚举的属性，Object.getOwnPropertyNames方法还返回不可枚举的属性名
var a = ['Hello', 'World'];

Object.keys(a) // ["0", "1"]
Object.getOwnPropertyNames(a) // ["0", "1", "length"]

由于 JavaScript 没有提供计算对象属性个数的方法，所以可以用这两个方法代替
var obj = {
  p1: 123,
  p2: 456
};

Object.keys(obj).length // 2
Object.getOwnPropertyNames(obj).length // 2

其他方法

对象属性模型的相关方法
Object.getOwnPropertyDescriptor()：获取某个属性的描述对象。
Object.defineProperty()：通过描述对象，定义某个属性。
Object.defineProperties()：通过描述对象，定义多个属性

控制对象状态的方法
Object.preventExtensions()：防止对象扩展。
Object.isExtensible()：判断对象是否可扩展。
Object.seal()：禁止对象配置。
Object.isSealed()：判断一个对象是否可配置。
Object.freeze()：冻结一个对象。
Object.isFrozen()：判断一个对象是否被冻结

原型链相关方法
Object.create()：该方法可以指定原型对象和属性，返回一个新的对象。
Object.getPrototypeOf()：获取对象的Prototype对象


Object 的实例方法
Object.prototype.valueOf()：返回当前对象对应的值。
Object.prototype.toString()：返回当前对象对应的字符串形式。
Object.prototype.toLocaleString()：返回当前对象对应的本地字符串形式。
Object.prototype.hasOwnProperty()：判断某个属性是否为当前对象自身的属性，还是继承自原型对象的属性。
Object.prototype.isPrototypeOf()：判断当前对象是否为另一个对象的原型。
Object.prototype.propertyIsEnumerable()：判断某个属性是否可枚举

// 1
var obj = new Object();
obj.valueOf = function () {
  return 2;
};

1 + obj // 3

// 2
数组、字符串、函数、Date 对象都分别部署了自定义的toString方法，覆盖了Object.prototype.toString方法
[1, 2, 3].toString() // "1,2,3"

'123'.toString() // "123"

(function () {
  return 123;
}).toString()
// "function () {
//   return 123;
// }"

(new Date()).toString()
// "Tue May 10 2016 09:11:31 GMT+0800 (CST)"

// 3
Object.prototype.toString方法返回对象的类型字符串，因此可以用来判断一个值的类型
// 1
var obj = {};
obj.toString() // "[object Object]"

由于实例对象可能会自定义toString方法，覆盖掉Object.prototype.toString方法，所以为了得到类型字符串，最好直接使用Object.prototype.toString方法
Object.prototype.toString.call(value)
上面代码表示对value这个值调用Object.prototype.toString方法


不同数据类型的Object.prototype.toString方法返回值"
数值：返回[object Number]
字符串：返回[object String]
布尔值：返回[object Boolean]
undefined：返回[object Undefined]
null: 返回[object Null]
数组：返回[object Array]
arguments 对象：返回[object Arguments]
函数：返回[object Function]
Error 对象：返回[object Error]
Date 对象：返回[object Date]
RegExp 对象：返回[object RegExp]
其他对象：返回[object Object]

Object.prototype.toString.call(2) // "[object Number]"
Object.prototype.toString.call('') // "[object String]"
Object.prototype.toString.call(true) // "[object Boolean]"
Object.prototype.toString.call(undefined) // "[object Undefined]"
Object.prototype.toString.call(null) // "[object Null]"
Object.prototype.toString.call(Math) // "[object Math]"
Object.prototype.toString.call({}) // "[object Object]"
Object.prototype.toString.call([]) // "[object Array]"

一个比typeof运算符更准确的类型判断函数
var type = function (o){
  var s = Object.prototype.toString.call(o);
  return s.match(/\[object (.*?)\]/)[1].toLowerCase();
};

type({}); // "object"
type([]); // "array"
type(5); // "number"
type(null); // "null"
type(); // "undefined"
type(/abcd/); // "regex"
type(new Date()); // "date"


在上面这个type函数的基础上，还可以加上专门判断某种类型数据的方法
var type = function (o){
  var s = Object.prototype.toString.call(o);
  return s.match(/\[object (.*?)\]/)[1].toLowerCase();
};

['Null',
 'Undefined',
 'Object',
 'Array',
 'String',
 'Number',
 'Boolean',
 'Function',
 'RegExp'
].forEach(function (t) {
  type['is' + t] = function (o) {
    return type(o) === t.toLowerCase();
  };
});

type.isObject({}) // true
type.isNumber(NaN) // true
type.isRegExp(/abc/) // true


Object.prototype.toLocaleString()
这个方法的主要作用是留出一个接口，让各种不同的对象实现自己版本的toLocaleString，用来返回针对某些地域的特定的值
var person = {
  toString: function () {
    return 'Henry Norman Bethune';
  },
  toLocaleString: function () {
    return '白求恩';
  }
};

person.toString() // Henry Norman Bethune
person.toLocaleString() // 白求恩

目前，主要有三个对象自定义了toLocaleString方法
Array.prototype.toLocaleString()
Number.prototype.toLocaleString()
Date.prototype.toLocaleString()

// 1
var date = new Date();
date.toString() // "Tue Jan 01 2018 12:01:33 GMT+0800 (CST)"
date.toLocaleString() // "1/01/2018, 12:01:33 PM"


Object.prototype.hasOwnProperty()
Object.prototype.hasOwnProperty方法接受一个字符串作为参数，返回一个布尔值，表示该实例对象自身是否具有该属性
// 1
var obj = {
  p: 123
};

obj.hasOwnProperty('p') // true
obj.hasOwnProperty('toString') // false

// toString属性是继承的，所以返回false
```

- 属性描述对象
```js
JavaScript 提供了一个内部数据结构，用来描述对象的属性，控制它的行为，比如该属性是否可写、可遍历等等。这个内部数据结构称为“属性描述对象”（attributes object）
每个属性都有自己对应的属性描述对象，保存该属性的一些元信息

属性描述对象提供6个元属性:
1 value
value是该属性的属性值，默认为undefined。
2 writable
属性值 value 是否可改变（即是否可写），默认为true
3 enumerable
属性是否可遍历，默认为true
4 configurable
属性的可配置性，默认为true。如果设为false，将阻止某些操作改写属性描述对象，比如无法删除该属性，也不得改变各种元属性（value属性除外），即configurable属性控制了属性描述对象的可写性。
5 get
属性的取值函数 getter，默认为undefined。
6 set
属性的存值函数 setter ，默认为undefined


Object.getOwnPropertyDescriptor()
获取属性描述对象，参数为 目标对象 和 对应目标对象的某个属性名
只能用于对象自身的属性，不能用于继承的属性

Object.getOwnPropertyNames()
返回一个数组，成员是参数对象自身的全部属性的属性名，不管该属性是否可遍历
（Object.keys只返回对象自身的可遍历属性的全部属性名）

Object.defineProperty()
通过属性描述对象，定义或修改一个属性，然后返回修改后的对象
Object.defineProperty(object, propertyName, attributesObject)
// 1
var obj = Object.defineProperty({}, 'p', {
  value: 123,
  writable: false,
  enumerable: true,
  configurable: false
});

obj.p // 123

obj.p = 246;
obj.p // 123



Object.defineProperties()
如果一次性定义或修改多个属性，可以使用Object.defineProperties()方法
// 
var obj = Object.defineProperties({}, {
  p1: { value: 123, enumerable: true },
  p2: { value: 'abc', enumerable: true },
  p3: { get: function () { return this.p1 + this.p2 },
    enumerable:true,
    configurable:true
  }
});

obj.p1 // 123
obj.p2 // "abc"
obj.p3 // "123abc"

注意，一旦定义了取值函数get（或存值函数set），就不能将writable属性设为true，或者同时定义value属性，否则会报错


Object.prototype.propertyIsEnumerable()
返回一个布尔值，用来判断某个属性是否可遍历
这个方法只能用于判断对象自身的属性，对于继承的属性一律返回false


属性描述对象的各个属性称为“元属性”，因为它们可以看作是控制属性的属性
value属性是目标属性的值
writable属性是一个布尔值，决定了目标属性的值（value）是否可以被改变
enumerable（可遍历性）返回一个布尔值，表示目标属性是否可遍历
configurable(可配置性）返回一个布尔值，决定了是否可以修改属性描述对象


除了直接定义以外，属性还可以用存取器（accessor）定义
其中，存值函数称为setter，使用属性描述对象的set属性
取值函数称为getter，使用属性描述对象的get属性

// 1
var obj = Object.defineProperty({}, 'p', {
  get: function () {
    return 'getter';
  },
  set: function (value) {
    console.log('setter: ' + value);
  }
});

obj.p // "getter"
obj.p = 123 // "setter: 123"
// obj.p定义了get和set属性。obj.p取值时，就会调用get；赋值时，就会调用set

// 写法二 (更常用)
var obj = {
  get p() {
    return 'getter';
  },
  set p(value) {
    console.log('setter: ' + value);
  }
};

注意，取值函数get不能接受参数，存值函数set只能接受一个参数（即属性的值）

存取器往往用于，属性的值依赖对象内部数据的场合
var obj ={
  $n : 5,
  get next() { return this.$n++ },
  set next(n) {
    if (n >= this.$n) this.$n = n;
    else throw new Error('新的值必须大于当前值');
  }
};

obj.next // 5

obj.next = 10;
obj.next // 10

obj.next = 5;
// Uncaught Error: 新的值必须大于当前值


对象的拷贝
将一个对象的所有属性，拷贝到另一个对象
var extend = function (to, from) {
  for (var property in from) {
    if (!from.hasOwnProperty(property)) continue;
    Object.defineProperty(
      to,
      property,
      Object.getOwnPropertyDescriptor(from, property)
    );
  }

  return to;
}

extend({}, { get a(){ return 1 } })
// { get a(){ return 1 } })
// hasOwnProperty那一行用来过滤掉继承的属性


控制对象状态
有时需要冻结对象的读写状态，防止对象被改变
JavaScript 提供了三种冻结方法:
Object.preventExtensions
Object.seal
Object.freeze

Object.preventExtensions方法可以使得一个对象无法再添加新的属性
var obj = new Object();
Object.preventExtensions(obj);

Object.defineProperty(obj, 'p', {
  value: 'hello'
});
// TypeError: Cannot define property:p, object is not extensible.

obj.p = 1;
obj.p // undefined

Object.isExtensible方法用于检查一个对象是否使用了Object.preventExtensions方法。也就是说，检查是否可以为一个对象添加属性
var obj = new Object();

Object.isExtensible(obj) // true
Object.preventExtensions(obj);
Object.isExtensible(obj) // false


Object.seal方法使得一个对象既无法添加新属性，也无法删除旧属性
var obj = { p: 'hello' };
Object.seal(obj);

delete obj.p;
obj.p // "hello"

obj.x = 'world';
obj.x // undefined


Object.isSealed方法用于检查一个对象是否使用了Object.seal方法
var obj = { p: 'a' };

Object.seal(obj);
Object.isSealed(obj) // true


Object.freeze方法可以使得一个对象无法添加新属性、无法删除旧属性、也无法改变属性的值，使得这个对象实际上变成了常量
var obj = {
  p: 'hello'
};

Object.freeze(obj);

obj.p = 'world';
obj.p // "hello"

obj.t = 'hello';
obj.t // undefined

delete obj.p // false
obj.p // "hello"


Object.isFrozen方法用于检查一个对象是否使用了Object.freeze方法
var obj = {
  p: 'hello'
};

Object.freeze(obj);
Object.isFrozen(obj) // true

Object.isFrozen的一个用途是，确认某个对象没有被冻结后，再对它的属性赋值
var obj = {
  p: 'hello'
};

Object.freeze(obj);

if (!Object.isFrozen(obj)) {
  obj.p = 'world';
}

```

- Array 对象
```js
Array是 JavaScript 的原生对象
var arr = Array(2);
// 等同于
var arr = new Array(2);

// good
var arr = [1, 2];


静态方法:
Array.isArray()
//
var arr = [1, 2, 3];

typeof arr // "object"
Array.isArray(arr) // true

实例方法:
数组的valueOf方法返回数组本身
var arr = [1, 2, 3];
arr.valueOf() // [1, 2, 3]

数组的toString方法返回数组的字符串形式
var arr = [1, 2, 3];
arr.toString() // "1,2,3"

var arr = [1, 2, 3, [4, 5, 6]];
arr.toString() // "1,2,3,4,5,6"

push方法用于在数组的末端添加一个或多个元素，并返回添加新元素后的数组长度
var arr = [];

arr.push(1) // 1
arr.push('a') // 2
arr.push(true, {}) // 4
arr // [1, 'a', true, {}]


pop方法用于删除数组的最后一个元素，并返回该元素
var arr = ['a', 'b', 'c'];

arr.pop() // 'c'
arr // ['a', 'b']

[].pop() // undefined


shift()方法用于删除数组的第一个元素，并返回该元素
var a = ['a', 'b', 'c'];

a.shift() // 'a'
a // ['b', 'c']

unshift()方法用于在数组的第一个位置添加元素，并返回添加新元素后的数组长度
// 1
var a = ['a', 'b', 'c'];

a.unshift('x'); // 4
a // ['x', 'a', 'b', 'c']

// 2
var arr = [ 'c', 'd' ];
arr.unshift('a', 'b') // 4
arr // [ 'a', 'b', 'c', 'd' ]


join()方法以指定参数作为分隔符，将所有数组成员连接为一个字符串返回，默认用逗号分隔
// 1
var a = [1, 2, 3, 4];

a.join(' ') // '1 2 3 4'
a.join(' | ') // "1 | 2 | 3 | 4"
a.join() // "1,2,3,4"

// 2
[undefined, null].join('#')
// '#'

['a',, 'b'].join('-')
// 'a--b'

// 3
Array.prototype.join.call('hello', '-')
// "h-e-l-l-o"

var obj = { 0: 'a', 1: 'b', length: 2 };
Array.prototype.join.call(obj, '-')
// 'a-b'

concat方法用于多个数组的合并，将新数组的成员，添加到原数组成员的后部，然后返回一个新数组，原数组不变
// 1
['hello'].concat(['world'])
// ["hello", "world"]

['hello'].concat(['world'], ['!'])
// ["hello", "world", "!"]

[].concat({a: 1}, {b: 2})
// [{ a: 1 }, { b: 2 }]

[2].concat({a: 1})
// [2, {a: 1}]

// 2
[1, 2, 3].concat(4, 5, 6)
// [1, 2, 3, 4, 5, 6]

// 3 如果数组成员包括对象，concat方法返回当前数组的一个浅拷贝
var obj = { a: 1 };
var oldArray = [obj];

var newArray = oldArray.concat();

obj.a = 2;
newArray[0].a // 2


reverse方法用于颠倒排列数组元素，返回改变后的数组
var a = ['a', 'b', 'c'];

a.reverse() // ["c", "b", "a"]
a // ["c", "b", "a"]


slice()方法用于提取目标数组的一部分，返回一个新数组，原数组不变
arr.slice(start, end);
// 1
var a = ['a', 'b', 'c'];

a.slice(0) // ["a", "b", "c"]
a.slice(1) // ["b", "c"]
a.slice(1, 2) // ["b"]
a.slice(2, 6) // ["c"]
a.slice() // ["a", "b", "c"]


// 2
var a = ['a', 'b', 'c'];
a.slice(-2) // ["b", "c"]
a.slice(-2, -1) // ["b"]

// 3
var a = ['a', 'b', 'c'];
a.slice(4) // []
a.slice(2, 1) // []

slice()方法的一个重要应用，是将类似数组的对象转为真正的数组
Array.prototype.slice.call({ 0: 'a', 1: 'b', length: 2 })
// ['a', 'b']

Array.prototype.slice.call(document.querySelectorAll("div"));
Array.prototype.slice.call(arguments);


splice()方法用于删除原数组的一部分成员，并可以在删除的位置添加新的数组成员，返回值是被删除的元素
arr.splice(start, count, addElement1, addElement2, ...);
// 1
var a = ['a', 'b', 'c', 'd', 'e', 'f'];
a.splice(4, 2) // ["e", "f"]
a // ["a", "b", "c", "d"]

// 2
var a = ['a', 'b', 'c', 'd', 'e', 'f'];
a.splice(4, 2, 1, 2) // ["e", "f"]
a // ["a", "b", "c", "d", 1, 2]

// 3
var a = ['a', 'b', 'c', 'd', 'e', 'f'];
a.splice(-4, 2) // ["c", "d"]

// 4
var a = [1, 1, 1];

a.splice(1, 0, 2) // []
a // [1, 2, 1, 1]

// 5
var a = [1, 2, 3, 4];
a.splice(2) // [3, 4]
a // [1, 2]


sort方法对数组成员进行排序，默认是按照字典顺序排序。排序后，原数组将被改变
// 
['d', 'c', 'b', 'a'].sort()
// ['a', 'b', 'c', 'd']

[4, 3, 2, 1].sort()
// [1, 2, 3, 4]

[11, 101].sort()
// [101, 11]

[10111, 1101, 111].sort()
// [10111, 1101, 111]

// 2 按照自定义方式排序
[10111, 1101, 111].sort(function (a, b) {
  return a - b;
})
// [111, 1101, 10111]


// 3
[
  { name: "张三", age: 30 },
  { name: "李四", age: 24 },
  { name: "王五", age: 28  }
].sort(function (o1, o2) {
  return o1.age - o2.age;
})
// [
//   { name: "李四", age: 24 },
//   { name: "王五", age: 28  },
//   { name: "张三", age: 30 }
// ]


map()方法将数组的所有成员依次传入参数函数，然后把每一次的执行结果组成一个新数组返回

// 1
[1, 2, 3].map(function(elem, index, arr) {
  return elem * index;
});
// [0, 2, 6]

// 2
var arr = ['a', 'b', 'c'];

[1, 2].map(function (e) {
  return this[e];
}, arr)
// ['b', 'c']


// 3
如果数组有空位，map()方法的回调函数在这个位置不会执行，会跳过数组的空位
var f = function (n) { return 'a' };

[1, undefined, 2].map(f) // ["a", "a", "a"]
[1, null, 2].map(f) // ["a", "a", "a"]
[1, , 2].map(f) // ["a", , "a"]


forEach()方法不返回值，只用来操作数据
// 1
function log(element, index, array) {
  console.log('[' + index + '] = ' + element);
}

[2, 5, 9].forEach(log);
// [0] = 2
// [1] = 5
// [2] = 9

// 2
var out = [];

[1, 2, 3].forEach(function(elem) {
  this.push(elem * elem);
}, out);

out // [1, 4, 9]


forEach()方法无法中断执行，总是会将所有成员遍历完
如果希望符合某种条件时，就中断遍历，要使用for循环
// 1
var arr = [1, 2, 3];

for (var i = 0; i < arr.length; i++) {
  if (arr[i] === 2) break;
  console.log(arr[i]);
}
// 1

forEach()方法也会跳过数组的空位
var log = function (n) {
  console.log(n + 1);
};

[1, undefined, 2].forEach(log)
// 2
// NaN
// 3

[1, null, 2].forEach(log)
// 2
// 1
// 3

[1, , 2].forEach(log)
// 2
// 3


filter()方法用于过滤数组成员，满足条件的成员组成一个新数组返回
// 1
[1, 2, 3, 4, 5].filter(function (elem) {
  return (elem > 3);
})
// [4, 5]

// 2
[1, 2, 3, 4, 5].filter(function (elem, index, arr) {
  return index % 2 === 0;
});
// [1, 3, 5]

// 3
var obj = { MAX: 3 };
var myFilter = function (item) {
  if (item > this.MAX) return true;
};

var arr = [2, 8, 3, 4, 1, 3, 2, 9];
arr.filter(myFilter, obj) // [8, 4, 9]


some()
every()
这两个方法类似“断言”（assert），返回一个布尔值，表示判断数组成员是否符合某种条件

some方法是只要一个成员的返回值是true，则整个some方法的返回值就是true，否则返回false
// 1
var arr = [1, 2, 3, 4, 5];
arr.some(function (elem, index, arr) {
  return elem >= 3;
});
// true

every方法是所有成员的返回值都是true，整个every方法才返回true，否则返回false
// 1
var arr = [1, 2, 3, 4, 5];
arr.every(function (elem, index, arr) {
  return elem >= 3;
});
// false

// 2
function isEven(x) { return x % 2 === 0 }

[].some(isEven) // false
[].every(isEven) // true

some和every方法还可以接受第二个参数，用来绑定参数函数内部的this变量

reduce()方法和reduceRight()方法依次处理数组的每个成员，最终累计为一个值
reduce()是从左到右处理
reduceRight()则是从右到左

// 1
[1, 2, 3, 4, 5].reduce(function (a, b) {
  console.log(a, b);
  return a + b;
})
// 1 2
// 3 3
// 6 4
// 10 5
//最后结果：15

用法
[1, 2, 3, 4, 5].reduce(function (
  a,   // 累积变量，必须
  b,   // 当前变量，必须
  i,   // 当前位置，可选
  arr  // 原数组，可选
) {
  // ... ...


// 1
[1, 2, 3, 4, 5].reduce(function (a, b) {
  return a + b;
}, 10);
// 25

// 2
function add(prev, cur) {
  return prev + cur;
}

[].reduce(add)
// TypeError: Reduce of empty array with no initial value
[].reduce(add, 1)
// 1


// 3
function subtract(prev, cur) {
  return prev - cur;
}

[3, 2, 1].reduce(subtract) // 0
[3, 2, 1].reduceRight(subtract) // -4


// 4 找出字符长度最长的数组成员
function findLongest(entries) {
  return entries.reduce(function (longest, entry) {
    return entry.length > longest.length ? entry : longest;
  }, '');
}

findLongest(['aaa', 'bb', 'c']) // "aaa"

indexOf方法返回给定元素在数组中第一次出现的位置，如果没有出现则返回-1
// 1
var a = ['a', 'b', 'c'];

a.indexOf('b') // 1
a.indexOf('y') // -1

// 2
['a', 'b', 'c'].indexOf('a', 1) // -1

lastIndexOf方法返回给定元素在数组中最后一次出现的位置，如果没有出现则返回-1
// 1
var a = [2, 5, 9, 2];
a.lastIndexOf(2) // 3
a.lastIndexOf(7) // -1

// 2
[NaN].indexOf(NaN) // -1
[NaN].lastIndexOf(NaN) // -1
// 方法内部，使用严格相等运算符（===）进行比较，而NaN是唯一一个不等于自身的值


链式使用
var users = [
  {name: 'tom', email: 'tom@example.com'},
  {name: 'peter', email: 'peter@example.com'}
];

users
.map(function (user) {
  return user.email;
})
.filter(function (email) {
  return /^t/.test(email);
})
.forEach(function (email) {
  console.log(email);
});
// "tom@example.com"

```

- 包装对象
```js
“包装对象”，指的是与数值、字符串、布尔值分别相对应的Number、String、Boolean三个原生对象。这三个原生对象可以把原始类型的值变成（包装成）对象
var v1 = new Number(123);
var v2 = new String('abc');
var v3 = new Boolean(true);

typeof v1 // "object"
typeof v2 // "object"
typeof v3 // "object"

v1 === 123 // false
v2 === 'abc' // false
v3 === true // false

包装对象的设计目的
首先是使得“对象”这种类型可以覆盖 JavaScript 所有的值，整门语言有一个通用的数据模型
其次是使得原始类型的值也有办法调用自己的方法


Number、String和Boolean 作为普通函数调用，常常用于将任意类型的值转为原始类型的 数值、字符串和布尔值
// 字符串转为数值
Number('123') // 123

// 数值转为字符串
String(123) // "123"

// 数值转为布尔值
Boolean(123) // true


实例方法:
valueOf()方法返回包装对象实例对应的原始类型的值
new Number(123).valueOf()  // 123
new String('abc').valueOf() // "abc"
new Boolean(true).valueOf() // true

toString()方法返回对应的字符串形式
new Number(123).toString() // "123"
new String('abc').toString() // "abc"
new Boolean(true).toString() // "true"

原始类型与实例对象的自动转换 (调用结束后，包装对象实例会自动销毁)
var str = 'abc';
str.length // 3

// 等同于
var strObj = new String(str)
// String {
//   0: "a", 1: "b", 2: "c", length: 3, [[PrimitiveValue]]: "abc"
// }
strObj.length // 3

自动转换生成的包装对象是只读的，无法修改
var s = 'Hello World';
s.x = 123;
s.x // undefined

自定义方法
包装对象还可以自定义方法和属性，供原始类型的值直接调用
String.prototype.double = function () {
  return this.valueOf() + this.valueOf();
};

'abc'.double()
// abcabc

Number.prototype.double = function () {
  return this.valueOf() + this.valueOf();
};

(123).double() // 246
// 123外面必须要加上圆括号，否则后面的点运算符（.）会被解释成小数点

```

- Boolean 对象
- Number 对象
```js
// 1
var n = new Number(1);
typeof n // "object"

// 2
Number(true) // 1

静态属性:
Number.POSITIVE_INFINITY // Infinity
Number.NEGATIVE_INFINITY // -Infinity
Number.NaN // NaN

Number.MAX_VALUE
// 1.7976931348623157e+308
Number.MAX_VALUE < Infinity
// true

Number.MIN_VALUE
// 5e-324
Number.MIN_VALUE > 0
// true

Number.MAX_SAFE_INTEGER // 9007199254740991
Number.MIN_SAFE_INTEGER // -9007199254740991

实例方法:
toString方法，用来将一个数值转为字符串形式
(10).toString() // "10"

(10).toString(2) // "1010"
(10).toString(8) // "12"
(10).toString(16) // "a"

10.toString(2)
// SyntaxError: Unexpected token ILLEGAL

10..toString(2)
// "1010"

// 其他方法还包括
10 .toString(2) // "1010"
10.0.toString(2) // "1010"

10.5.toString() // "10.5"
10.5.toString(2) // "1010.1"
10.5.toString(8) // "12.4"
10.5.toString(16) // "a.8"

10['toString'](2) // "1010"
toString方法只能将十进制的数，转为其他进制的字符串。如果要将其他进制的数，转回十进制，需要使用parseInt方法


toFixed()方法先将一个数转为指定位数的小数，然后返回这个小数对应的字符串
(10).toFixed(2) // "10.00"
10.005.toFixed(2) // "10.01"

(10.055).toFixed(2) // 10.05
(10.005).toFixed(2) // 10.01

toExponential方法用于将一个数转为科学计数法形式
(10).toExponential()  // "1e+1"
(10).toExponential(1) // "1.0e+1"
(10).toExponential(2) // "1.00e+1"

(1234).toExponential()  // "1.234e+3"
(1234).toExponential(1) // "1.2e+3"
(1234).toExponential(2) // "1.23e+3"

Number.prototype.toPrecision()方法用于将一个数转为指定位数的有效数字
(12.34).toPrecision(1) // "1e+1"
(12.34).toPrecision(2) // "12"
(12.34).toPrecision(3) // "12.3"
(12.34).toPrecision(4) // "12.34"
(12.34).toPrecision(5) // "12.340"

(12.35).toPrecision(3) // "12.3"
(12.25).toPrecision(3) // "12.3"
(12.15).toPrecision(3) // "12.2"
(12.45).toPrecision(3) // "12.4"

Number.prototype.toLocaleString()方法接受一个地区码作为参数，返回一个字符串，表示当前数字在该地区的当地书写形式
(123).toLocaleString('zh-Hans-CN-u-nu-hanidec')
// "一二三"

(123).toLocaleString('en-US', { style: 'currency', currency: 'USD' })
// "$123.00"

自定义方法:
// 1
Number.prototype.add = function (x) {
  return this + x;
};

8['add'](2) // 10

// 2
Number.prototype.subtract = function (x) {
  return this - x;
};

(8).add(2).subtract(4)
// 6

// 3
Number.prototype.iterate = function () {
  var result = [];
  for (var i = 0; i <= this; i++) {
    result.push(i);
  }
  return result;
};

(8).iterate()
// [0, 1, 2, 3, 4, 5, 6, 7, 8]
```

- String 对象
```js
// 1
var s1 = 'abc';
var s2 = new String('abc');

typeof s1 // "string"
typeof s2 // "object"

s2.valueOf() // "abc"

// 2
new String('abc')
// String {0: "a", 1: "b", 2: "c", length: 3}

(new String('abc'))[1] // "b"

// 3
String(true) // "true"
String(5) // "5"

静态方法:
String.fromCharCode()。该方法的参数是一个或多个数值，代表 Unicode 码点，返回值是这些码点组成的字符串
// 1
String.fromCharCode() // ""
String.fromCharCode(97) // "a"
String.fromCharCode(104, 101, 108, 108, 111)
// "hello"

实例属性:
// 1
'abc'.length // 3

// 2
charAt方法返回指定位置的字符，参数是从0开始编号的位置
// 1
var s = new String('abc');

s.charAt(1) // "b"
s.charAt(s.length - 1) // "c

// 2
'abc'.charAt(1) // "b"
'abc'[1] // "b"

// 3
'abc'.charAt(-1) // ""
'abc'.charAt(3) // ""

charCodeAt()方法返回字符串指定位置的 Unicode 码点（十进制表示），相当于String.fromCharCode()的逆操作
'abc'.charCodeAt(1) // 98
'abc'.charCodeAt() // 97

'abc'.charCodeAt(-1) // NaN
'abc'.charCodeAt(4) // NaN

concat方法用于连接两个字符串，返回一个新字符串，不改变原字符串
// 1
var s1 = 'abc';
var s2 = 'def';

s1.concat(s2) // "abcdef"
s1 // "abc"

// 2
'a'.concat('b', 'c') // "abc"

// 3
var one = 1;
var two = 2;
var three = '3';

''.concat(one, two, three) // "123"
one + two + three // "33"

slice()方法用于从原字符串取出子字符串并返回，不改变原字符串
// 1
'JavaScript'.slice(0, 4) // "Java"

// 2
'JavaScript'.slice(4) // "Script"

// 3
'JavaScript'.slice(-6) // "Script"
'JavaScript'.slice(0, -6) // "Java"
'JavaScript'.slice(-2, -1) // "p"

// 4
'JavaScript'.slice(2, 1) // ""

substring方法用于从原字符串取出子字符串并返回，不改变原字符串，跟slice方法很相像
'JavaScript'.substring(0, 4) // "Java"

'JavaScript'.substring(4) // "Script"

'JavaScript'.substring(10, 4) // "Script"
// 等同于
'JavaScript'.substring(4, 10) // "Script"

//如果参数是负数，substring方法会自动将负数转为0
'JavaScript'.substring(-3) // "JavaScript"
'JavaScript'.substring(4, -3) // "Java"

substr方法用于从原字符串取出子字符串并返回，不改变原字符串，跟slice和substring方法的作用相同
'JavaScript'.substr(4, 6) // "Script"
'JavaScript'.substr(4) // "Script"

如果第一个参数是负数，表示倒数计算的字符位置。如果第二个参数是负数，将被自动转为0，因此会返回空字符串
'JavaScript'.substr(-6) // "Script"
'JavaScript'.substr(4, -1) // ""


indexOf方法用于确定一个字符串在另一个字符串中第一次出现的位置，返回结果是匹配开始的位置。如果返回-1，就表示不匹配
'hello world'.indexOf('o') // 4
'JavaScript'.indexOf('script') // -1

'hello world'.indexOf('o', 6) // 7

'hello world'.lastIndexOf('o') // 7

'hello world'.lastIndexOf('o', 6) // 4


trim方法用于去除字符串两端的空格，返回一个新字符串，不改变原字符串
'  hello world  '.trim()
// "hello world"

'\r\nabc \t'.trim() // 'abc'

toLowerCase方法用于将一个字符串全部转为小写，toUpperCase则是全部转为大写。它们都返回一个新字符串，不改变原字符串
'Hello World'.toLowerCase()
// "hello world"

'Hello World'.toUpperCase()
// "HELLO WORLD"


match方法用于确定原字符串是否匹配某个子字符串，返回一个数组，成员为匹配的第一个字符串。如果没有找到匹配，则返回null
'cat, bat, sat, fat'.match('at') // ["at"]
'cat, bat, sat, fat'.match('xt') // null

var matches = 'cat, bat, sat, fat'.match('at');
matches.index // 1
matches.input // "cat, bat, sat, fat"

match方法还可以使用正则表达式作为参数


search方法的用法基本等同于match，但是返回值为匹配的第一个位置。如果没有找到匹配，则返回-1
'cat, bat, sat, fat'.search('at') // 1
search方法还可以使用正则表达式作为参数

replace方法用于替换匹配的子字符串，一般情况下只替换第一个匹配（除非使用带有g修饰符的正则表达式）
'aaa'.replace('a', 'b') // "baa"
replace方法还可以使用正则表达式作为参数


split方法按照给定规则分割字符串，返回一个由分割出来的子字符串组成的数组
'a|b|c'.split('|') // ["a", "b", "c"]
'a|b|c'.split('') // ["a", "|", "b", "|", "c"]
'a|b|c'.split() // ["a|b|c"]
'a||c'.split('|') // ['a', '', 'c']

'|b|c'.split('|') // ["", "b", "c"]
'a|b|'.split('|') // ["a", "b", ""]

split方法还可以接受第二个参数，限定返回数组的最大成员数
'a|b|c'.split('|', 0) // []
'a|b|c'.split('|', 1) // ["a"]
'a|b|c'.split('|', 2) // ["a", "b"]
'a|b|c'.split('|', 3) // ["a", "b", "c"]
'a|b|c'.split('|', 4) // ["a", "b", "c"]


localeCompare方法用于比较两个字符串。它返回一个整数，如果小于0，表示第一个字符串小于第二个字符串；如果等于0，表示两者相等；如果大于0，表示第一个字符串大于第二个字符串
'apple'.localeCompare('banana') // -1
'apple'.localeCompare('apple') // 0

'B' > 'a' // false
'B'.localeCompare('a') // 1
localeCompare方法会考虑自然语言的排序情况，将B排在a的前面

localeCompare还可以有第二个参数，指定所使用的语言（默认是英语），然后根据该语言的规则进行比较
'ä'.localeCompare('z', 'de') // -1
'ä'.localeCompare('z', 'sv') // 1

```

- Math 对象
```js
静态属性 
Math.E // 2.718281828459045
Math.LN2 // 0.6931471805599453
Math.LN10 // 2.302585092994046
Math.LOG2E // 1.4426950408889634
Math.LOG10E // 0.4342944819032518
Math.PI // 3.141592653589793
Math.SQRT1_2 // 0.7071067811865476
Math.SQRT2 // 1.4142135623730951

静态方法
Math.abs()：绝对值
Math.ceil()：向上取整
Math.floor()：向下取整
Math.max()：最大值
Math.min()：最小值
Math.pow()：幂运算
Math.sqrt()：平方根
Math.log()：自然对数
Math.exp()：e的指数
Math.round()：四舍五入
Math.random()：随机数

Math.random()返回0到1之间的一个伪随机数，可能等于0，但是一定小于1
// 1
function getRandomArbitrary(min, max) {
  return Math.random() * (max - min) + min;
}

getRandomArbitrary(1.5, 6.5)
// 2.4942810038223864

// 2
function getRandomInt(min, max) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

getRandomInt(1, 6) // 5

// 3
function random_str(length) {
  var ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
  ALPHABET += 'abcdefghijklmnopqrstuvwxyz';
  ALPHABET += '0123456789-_';
  var str = '';
  for (var i = 0; i < length; ++i) {
    var rand = Math.floor(Math.random() * ALPHABET.length);
    str += ALPHABET.substring(rand, rand + 1);
  }
  return str;
}

random_str(6) // "NdQKOr"

三角函数方法
Math.sin(0) // 0
Math.cos(0) // 1
Math.tan(0) // 0

Math.sin(Math.PI / 2) // 1

Math.asin(1) // 1.5707963267948966
Math.acos(1) // 0
Math.atan(1) // 0.7853981633974483

```

- Date 对象
```js
无论有没有参数，直接调用Date总是返回当前时间
Date()
// "Tue Dec 01 2015 09:34:43 GMT+0800 (CST)"

Date(2000, 1, 1)
// "Tue Dec 01 2015 09:34:43 GMT+0800 (CST)"

// 1
var today = new Date();

today
// "Tue Dec 01 2015 09:34:43 GMT+0800 (CST)"

// 等同于
today.toString()
// "Tue Dec 01 2015 09:34:43 GMT+0800 (CST)"


// 2
// 参数为时间零点开始计算的毫秒数
new Date(1378218728000)
// Tue Sep 03 2013 22:32:08 GMT+0800 (CST)

// 参数为日期字符串
new Date('January 6, 2013');
// Sun Jan 06 2013 00:00:00 GMT+0800 (CST)

// 参数为多个整数，
// 代表年、月、日、小时、分钟、秒、毫秒
new Date(2013, 0, 1, 0, 0, 0, 0)
// Tue Jan 01 2013 00:00:00 GMT+0800 (CST)


关于Date构造函数的参数
1.参数可以是负整数，代表1970年元旦之前的时间
new Date(-1378218728000)
// Fri Apr 30 1926 17:27:52 GMT+0800 (CST)

2.只要是能被Date.parse()方法解析的字符串，都可以当作参数
new Date('2013-2-15')
new Date('2013/2/15')
new Date('02/15/2013')
new Date('2013-FEB-15')
new Date('FEB, 15, 2013')
new Date('FEB 15, 2013')
new Date('February, 15, 2013')
new Date('February 15, 2013')
new Date('15 Feb 2013')
new Date('15, February, 2013')
// Fri Feb 15 2013 00:00:00 GMT+0800 (CST)

3.参数为年、月、日等多个整数时，年和月是不能省略的，其他参数都可以省略的
new Date(2013)
// Thu Jan 01 1970 08:00:02 GMT+0800 (CST)

new Date(2013, 0)
// Tue Jan 01 2013 00:00:00 GMT+0800 (CST)
new Date(2013, 0, 1)
// Tue Jan 01 2013 00:00:00 GMT+0800 (CST)
new Date(2013, 0, 1, 0)
// Tue Jan 01 2013 00:00:00 GMT+0800 (CST)
new Date(2013, 0, 1, 0, 0, 0, 0)
// Tue Jan 01 2013 00:00:00 GMT+0800 (CST)

参数如果超出了正常范围，会被自动折算
new Date(2013, 15)
// Tue Apr 01 2014 00:00:00 GMT+0800 (CST)
new Date(2013, 0, 0)
// Mon Dec 31 2012 00:00:00 GMT+0800 (CST)

参数还可以使用负数，表示扣去的时间
new Date(2013, -1)
// Sat Dec 01 2012 00:00:00 GMT+0800 (CST)
new Date(2013, 0, -1)
// Sun Dec 30 2012 00:00:00 GMT+0800 (CST)

日期的运算
类型自动转换时，Date实例如果转为数值，则等于对应的毫秒数；如果转为字符串，则等于对应的日期字符串
var d1 = new Date(2000, 2, 1);
var d2 = new Date(2000, 3, 1);

d2 - d1
// 2678400000
d2 + d1
// "Sat Apr 01 2000 00:00:00 GMT+0800 (CST)Wed Mar 01 2000 00:00:00 GMT+0800 (CST)"

静态方法
Date.now() // 1364026285194

Date.parse('Aug 9, 1995')
Date.parse('January 26, 2011 13:51:50')
Date.parse('Mon, 25 Dec 1995 13:30:00 GMT')
Date.parse('Mon, 25 Dec 1995 13:30:00 +0430')
Date.parse('2011-10-10')
Date.parse('2011-10-10T14:48:00')

Date.parse('xxx') // NaN

// 格式
Date.UTC(year, month[, date[, hrs[, min[, sec[, ms]]]]])

// 用法
Date.UTC(2011, 0, 1, 2, 3, 4, 567)
// 1293847384567

实例方法
to类：从Date对象返回一个字符串，表示指定的时间。
get类：获取Date对象的日期和时间。
set类：设置Date对象的日期和时间

```

- RegExp 对象
```js
新建正则表达式
// 1
var regex = /xyz/;

// 2
var regex = new RegExp('xyz');

RegExp构造函数还可以接受第二个参数，表示修饰符
var regex = new RegExp('xyz', 'i');
// 等价于
var regex = /xyz/i;


实例属性
1. 修饰符相关，用于了解设置了什么修饰符
RegExp.prototype.ignoreCase：返回一个布尔值，表示是否设置了i修饰符。
RegExp.prototype.global：返回一个布尔值，表示是否设置了g修饰符。
RegExp.prototype.multiline：返回一个布尔值，表示是否设置了m修饰符。
RegExp.prototype.flags：返回一个字符串，包含了已经设置的所有修饰符，按字母排序

2. 与修饰符无关的属性
RegExp.prototype.lastIndex：返回一个整数，表示下一次开始搜索的位置。该属性可读写，但是只在进行连续搜索时有意义，详细介绍请看后文。
RegExp.prototype.source：返回正则表达式的字符串形式（不包括反斜杠），该属性只读

// 1
var r = /abc/igm;

r.ignoreCase // true
r.global // true
r.multiline // true
r.flags // 'gim'

// 2
var r = /abc/igm;

r.lastIndex // 0
r.source // "abc"

实例方法
正则实例对象的test方法返回一个布尔值，表示当前模式是否能匹配参数字符串
/cat/.test('cats and dogs') // true

如果正则表达式带有g修饰符，则每一次test方法都从上一次结束的位置开始向后匹配
var r = /x/g;
var s = '_x_x';

r.lastIndex // 0
r.test(s) // true

r.lastIndex // 2
r.test(s) // true

r.lastIndex // 4
r.test(s) // false

带有g修饰符时，可以通过正则对象的lastIndex属性指定开始搜索的位置
var r = /x/g;
var s = '_x_x';

r.lastIndex = 4;
r.test(s) // false

r.lastIndex // 0
r.test(s) // true

带有g修饰符时，正则表达式内部会记住上一次的lastIndex属性，这时不应该更换所要匹配的字符串，否则会有一些难以察觉的错误
var r = /bb/g;
r.test('bb') // true
r.test('-bb-') // false


lastIndex属性只对同一个正则表达式有效
var count = 0;
while (/a/g.test('babaa')) count++;
// 上面代码会导致无限循环，因为while循环的每次匹配条件都是一个新的正则表达式，导致lastIndex属性总是等于0

如果正则模式是一个空字符串，则匹配所有字符串
new RegExp('').test('abc')
// true

exec()方法，用来返回匹配结果。如果发现匹配，就返回一个数组，成员是匹配成功的子字符串，否则返回null
// 1
var s = '_x_x';
var r1 = /x/;
var r2 = /y/;

r1.exec(s) // ["x"]
r2.exec(s) // null

// 2
var s = '_x_x';
var r = /_(x)/;

r.exec(s) // ["_x", "x"]


// 3
var r = /a(b+)a/;
var arr = r.exec('_abbba_aba_');

arr // ["abbba", "bbb"]

arr.index // 1
arr.input // "_abbba_aba_"

// 正则表达式加上g修饰符
var reg = /a/g;
var str = 'abc_abc_abc'

var r1 = reg.exec(str);
r1 // ["a"]
r1.index // 0
reg.lastIndex // 1

var r2 = reg.exec(str);
r2 // ["a"]
r2.index // 4
reg.lastIndex // 5

var r3 = reg.exec(str);
r3 // ["a"]
r3.index // 8
reg.lastIndex // 9

var r4 = reg.exec(str);
r4 // null
reg.lastIndex // 0


// 4
var reg = /a/g;
var str = 'abc_abc_abc'

while(true) {
  var match = reg.exec(str);
  if (!match) break;
  console.log('#' + match.index + ':' + match[0]);
}
// #0:a
// #4:a
// #8:a

正则实例对象的lastIndex属性不仅可读，还可写

字符串的实例方法
String.prototype.match()：返回一个数组，成员是所有匹配的子字符串。
String.prototype.search()：按照给定的正则表达式进行搜索，返回一个整数，表示匹配开始的位置。
String.prototype.replace()：按照给定的正则表达式进行替换，返回替换后的字符串。
String.prototype.split()：按照给定规则进行字符串分割，返回一个数组，包含分割后的各个成员

// 1
var s = '_x_x';
var r1 = /x/;
var r2 = /y/;

s.match(r1) // ["x"]
s.match(r2) // null

// 2
var s = 'abba';
var r = /a/g;

s.match(r) // ["a", "a"]
r.exec(s) // ["a"]

// 3
var r = /a|b/g;
r.lastIndex = 7;
'xaxb'.match(r) // ['a', 'b']
r.lastIndex // 0


// 1
'_x_x'.search(/x/)
// 1

str.replace(search, replacement)

// 1
'aaa'.replace('a', 'b') // "baa"
'aaa'.replace(/a/, 'b') // "baa"
'aaa'.replace(/a/g, 'b') // "bbb"

// 2
var str = '  #id div.class  ';

str.replace(/^\s+|\s+$/g, '')
// "#id div.class"

// 3
'hello world'.replace(/(\w+)\s(\w+)/, '$2 $1')
// "world hello"

'abc'.replace('b', '[$`-$&-$\']')
// "a[a-b-c]c"

// 4
'3 and 5'.replace(/[0-9]+/g, function (match) {
  return 2 * match;
})
// "6 and 10"

var a = 'The quick brown fox jumped over the lazy dog.';
var pattern = /quick|brown|lazy/ig;

a.replace(pattern, function replacer(match) {
  return match.toUpperCase();
});
// The QUICK BROWN fox jumped over the LAZY do


// 5
var prices = {
  'p1': '$1.99',
  'p2': '$9.99',
  'p3': '$5.00'
};

var template = '<span id="p1"></span>'
  + '<span id="p2"></span>'
  + '<span id="p3"></span>';

template.replace(
  /(<span id=")(.*?)(">)(<\/span>)/g,
  function(match, $1, $2, $3, $4){
    return $1 + $2 + $3 + prices[$2] + $4;
  }
);
// "<span id="p1">$1.99</span><span id="p2">$9.99</span><span id="p3">$5.00</span>"

str.split(separator, [limit])

// 1
// 非正则分隔
'a,  b,c, d'.split(',')
// [ 'a', '  b', 'c', ' d' ]

// 正则分隔，去除多余的空格
'a,  b,c, d'.split(/, */)
// [ 'a', 'b', 'c', 'd' ]

// 指定返回数组的最大成员
'a,  b,c, d'.split(/, */, 2)
[ 'a', 'b' ]

// 2
// 例一
'aaa*a*'.split(/a*/)
// [ '', '*', '*' ]

// 例二
'aaa**a*'.split(/a*/)
// ["", "*", "*", "*"]


// 3 如果正则表达式带有括号，则括号匹配的部分也会作为数组成员返回
'aaa*a*'.split(/(a*)/)
// [ '', 'aaa', '*', 'a', '*' ]


字面量字符和元字符
/dog/.test('old dog') // true

点字符（.）匹配除回车（\r）、换行(\n) 、行分隔符（\u2028）和段分隔符（\u2029）以外的所有字符
/c.t/

位置字符
^ 表示字符串的开始位置
$ 表示字符串的结束位

// test必须出现在开始位置
/^test/.test('test123') // true

// test必须出现在结束位置
/test$/.test('new test') // true

// 从开始位置到结束位置只有test
/^test$/.test('test') // true
/^test$/.test('test test') // false


选择符（|）
/11|22/.test('911') // true

// 匹配fred、barney、betty之中的一个
/fred|barney|betty/

/a( |\t)b/.test('a\tb') // true

转义符
正则表达式中，需要反斜杠转义的，一共有12个字符：^ . [ $ ( ) | * + ? { 和 \
/1+1/.test('1+1')
// false

/1\+1/.test('1+1')
// true

(new RegExp('1\+1')).test('1+1')
// false

(new RegExp('1\\+1')).test('1+1')
// true


特殊字符
\n 匹配换行键
\r 匹配回车键
......


字符类
/[abc]/.test('hello world') // false
/[abc]/.test('apple') // true

/[^abc]/.test('bbc news') // true
/[^abc]/.test('bbc') // false

// 如果方括号内没有其他字符，即只有[^]，就表示匹配一切字符，其中包括换行符。相比之下，点号作为元字符（.）是不包括换行符的
var s = 'Please yes\nmake my day!';

s.match(/yes.*day/) // null
s.match(/yes[^]*day/) // [ 'yes\nmake my day
// 注意，脱字符只有在字符类的第一个位置才有特殊含义，否则就是字面含义


/a-z/.test('b') // false
/[a-z]/.test('b') // true

[0-9.,]
[0-9a-fA-F]
[a-zA-Z0-9-]
[1-31]

连字符还可以用来指定 Unicode 字符的范围
var str = "\u0130\u0131\u0132";
/[\u0128-\uFFFF]/.test(str)
// true

预定义模式
d 匹配0-9之间的任一数字，相当于[0-9]。
\D 匹配所有0-9以外的字符，相当于[^0-9]。
\w 匹配任意的字母、数字和下划线，相当于[A-Za-z0-9_]。
\W 除所有字母、数字和下划线以外的字符，相当于[^A-Za-z0-9_]。
\s 匹配空格（包括换行符、制表符、空格符等），相等于[ \t\r\n\v\f]。
\S 匹配非空格的字符，相当于[^ \t\r\n\v\f]。
\b 匹配词的边界。
\B 匹配非词边界，即在词的内部

// \s 的例子
/\s\w*/.exec('hello world') // [" world"]

// \b 的例子
/\bworld/.test('hello world') // true
/\bworld/.test('hello-world') // true
/\bworld/.test('helloworld') // false

// \B 的例子
/\Bworld/.test('hello-world') // false
/\Bworld/.test('helloworld') // true

通常，正则表达式遇到换行符（\n）就会停止匹配
var html = "<b>Hello</b>\n<i>world!</i>";

/.*/.exec(html)[0]
// "<b>Hello</b>"


使用\s字符类，就能包括换行符
var html = "<b>Hello</b>\n<i>world!</i>";

/[\S\s]*/.exec(html)[0]
// "<b>Hello</b>\n<i>world!</i>"
上面代码中，[\S\s]指代一切字符



重复类
模式的精确匹配次数，使用大括号（{}）表示。{n}表示恰好重复n次，{n,}表示至少重复n次，{n,m}表示重复不少于n次，不多于m次
/lo{2}k/.test('look') // true
/lo{2,5}k/.test('looook') // true


量词符
量词符用来设定某个模式出现的次数
? 问号表示某个模式出现0次或1次，等同于{0, 1}
* 星号表示某个模式出现0次或多次，等同于{0,}
+ 加号表示某个模式出现1次或多次，等同于{1,}

// t 出现0次或1次
/t?est/.test('test') // true
/t?est/.test('est') // true

// t 出现1次或多次
/t+est/.test('test') // true
/t+est/.test('ttest') // true
/t+est/.test('est') // false

// t 出现0次或多次
/t*est/.test('test') // true
/t*est/.test('ttest') // true
/t*est/.test('tttest') // true
/t*est/.test('est') // true

贪婪模式
匹配到下一个字符不满足匹配规则为止
var s = 'aaa';
s.match(/a+/) // ["aaa"]

非贪婪模式，即最小可能匹配
+?：表示某个模式出现1次或多次，匹配时采用非贪婪模式。
*?：表示某个模式出现0次或多次，匹配时采用非贪婪模式。
??：表格某个模式出现0次或1次，匹配时采用非贪婪模式
var s = 'aaa';
s.match(/a+?/) // ["a"]

'abb'.match(/ab*/) // ["abb"]
'abb'.match(/ab*?/) // ["a"]

'abb'.match(/ab?/) // ["ab"]
'abb'.match(/ab??/) // ["a"]


修饰符
修饰符（modifier）表示模式的附加规则，放在正则模式的最尾部
// 单个修饰符
var regex = /test/i;

// 多个修饰符
var regex = /test/ig;

g修饰符表示全局匹配（global），加上它以后，正则对象将匹配全部符合条件的结果，主要用于搜索和替换
// 1
var regex = /b/;
var str = 'abba';

regex.test(str); // true
regex.test(str); // true
regex.test(str); // true

// 2
var regex = /b/g;
var str = 'abba';

regex.test(str); // true
regex.test(str); // true
regex.test(str); // false

i修饰符以后表示忽略大小写（ignoreCase）
/abc/.test('ABC') // false
/abc/i.test('ABC') // true

/world$/.test('hello world\n') // false
/world$/m.test('hello world\n') // true


m修饰符表示多行模式（multiline），会修改^和$的行为。默认情况下（即不加m修饰符时），^和$匹配字符串的开始处和结尾处，加上m修饰符以后，^和$还会匹配行首和行尾，即^和$会识别换行符（\n）
/world$/.test('hello world\n') // false
/world$/m.test('hello world\n') // true

/^b/m.test('a\nb') // true


组匹配
// 1
/fred+/.test('fredd') // true
/(fred)+/.test('fredfred') // true

// 2
var m = 'abcabc'.match(/(.)b(.)/);
m
// ['abc', 'a', 'c']


使用组匹配时，不宜同时使用g修饰符，否则match方法不会捕获分组的内容
var m = 'abcabc'.match(/(.)b(.)/g);
m // ['abc', 'abc']


/(.)b(.)\1b\2/.test("abcabc")
// true
\1表示第一个括号匹配的内容（即a），\2表示第二个括号匹配的内容（即c）


/y(..)(.)\2\1/.test('yabccab') // true

/y((..)\2)\1/.test('yabababab') // true

// 1
var tagName = /<([^>]+)>[^<]*<\/\1>/;

tagName.exec("<b>bold</b>")[1]
// 'b'


// 2
var html = '<b class="hello">Hello</b><i>world</i>';
var tag = /<(\w+)([^>]*)>(.*?)<\/\1>/g;

var match = tag.exec(html);

match[1] // "b"
match[2] // " class="hello""
match[3] // "Hello"

match = tag.exec(html);

match[1] // "i"
match[2] // ""
match[3] // "world"


非捕获组
(?:x)称为非捕获组（Non-capturing group），表示不返回该组匹配的内容，即匹配的结果中不计入这个括号
// 1
var m = 'abc'.match(/(?:.)b(.)/);
m // ["abc", "c"]

// 2
// 正常匹配
var url = /(http|ftp):\/\/([^/\r\n]+)(\/[^\r\n]*)?/;

url.exec('http://google.com/');
// ["http://google.com/", "http", "google.com", "/"]

// 非捕获组匹配
var url = /(?:http|ftp):\/\/([^/\r\n]+)(\/[^\r\n]*)?/;

url.exec('http://google.com/');
// ["http://google.com/", "google.com", "/"]


先行断言
x(?=y)称为先行断言（Positive look-ahead），x只有在y前面才匹配，y不会被计入返回结果

“先行断言”中，括号里的部分是不会返回的
var m = 'abc'.match(/b(?=c)/);
m // ["b"]


先行否定断言
x(?!y)称为先行否定断言（Negative look-ahead），x只有不在y前面才匹配，y不会被计入返回结果

/\d+(?!\.)/.exec('3.14')
// ["14"]

“先行否定断言”中，括号里的部分是不会返回的
```

- JSON 对象
```js
JSON 对值的类型和格式有严格的规定:
1.复合类型的值只能是数组或对象，不能是函数、正则表达式对象、日期对象。
2.原始类型的值只有四种：字符串、数值（必须以十进制表示）、布尔值和null（不能使1.用NaN, Infinity, -Infinity和undefined）。
3.字符串必须使用双引号表示，不能使用单引号。
4.对象的键名必须放在双引号里面。
5.数组或对象最后一个成员的后面，不能加逗号。


JSON对象是 JavaScript 的原生对象
静态方法：
JSON.stringify()
JSON.parse()

JSON.stringify()方法用于将一个值转为 JSON 字符串
// 1
JSON.stringify('abc') // ""abc""
JSON.stringify(1) // "1"
JSON.stringify(false) // "false"
JSON.stringify([]) // "[]"
JSON.stringify({}) // "{}"

JSON.stringify([1, "false", false])
// '[1,"false",false]'

JSON.stringify({ name: "张三" })
// '{"name":"张三"}'

// 2
JSON.stringify('foo') === "foo" // false
JSON.stringify('foo') === "\"foo\"" // true

JSON.stringify(false) // "false"
JSON.stringify('false') // "\"false\""


如果对象的属性是undefined、函数或 XML 对象，该属性会被JSON.stringify()过滤
var obj = {
  a: undefined,
  b: function () {}
};

JSON.stringify(obj) // "{}"

如果数组的成员是undefined、函数或 XML 对象，则这些值被转成null
var arr = [undefined, function () {}];
JSON.stringify(arr) // "[null,null]"


正则对象会被转成空对象
JSON.stringify(/foo/) // "{}"

JSON.stringify()方法会忽略对象的不可遍历的属性
var obj = {};
Object.defineProperties(obj, {
  'foo': {
    value: 1,
    enumerable: true
  },
  'bar': {
    value: 2,
    enumerable: false
  }
});

JSON.stringify(obj); // "{"foo":1}"


JSON.stringify()方法还可以接受一个数组，作为第二个参数，指定参数对象的哪些属性需要转成字符串
// 1
var obj = {
  'prop1': 'value1',
  'prop2': 'value2',
  'prop3': 'value3'
};

var selectedProperties = ['prop1', 'prop2'];

JSON.stringify(obj, selectedProperties)
// "{"prop1":"value1","prop2":"value2"}"


// 2
function f(key, value) {
  if (typeof value === "number") {
    value = 2 * value;
  }
  return value;
}

JSON.stringify({ a: 1, b: 2 }, f)
// '{"a": 2,"b": 4}'


处理函数是递归处理所有的键
var obj = {a: {b: 1}};

function f(key, value) {
  console.log("["+ key +"]:" + value);
  return value;
}

JSON.stringify(obj, f)
// []:[object Object]
// [a]:[object Object]
// [b]:1
// '{"a":{"b":1}}


递归处理中，每一次处理的对象，都是前一次返回的值
var obj = {a: 1};

function f(key, value) {
  if (typeof value === 'object') {
    return {b: 2};
  }
  return value * 2;
}

JSON.stringify(obj, f)
// "{"b": 4}"


如果处理函数返回undefined或没有返回值，则该属性会被忽略
function f(key, value) {
  if (typeof(value) === "string") {
    return undefined;
  }
  return value;
}

JSON.stringify({ a: "abc", b: 123 }, f)
// '{"b": 123}'


JSON.stringify()还可以接受第三个参数，用于增加返回的 JSON 字符串的可读性
// 1
// 默认输出
JSON.stringify({ p1: 1, p2: 2 })
// JSON.stringify({ p1: 1, p2: 2 })

// 分行输出
JSON.stringify({ p1: 1, p2: 2 }, null, '\t')
// {
// 	"p1": 1,
// 	"p2": 2
// }

第三个属性如果是一个数字，则表示每个属性前面添加的空格(最多不超过10个)
JSON.stringify({ p1: 1, p2: 2 }, null, 2);
/*
"{
  "p1": 1,
  "p2": 2
}"
*/


参数对象的 toJSON() 方法
如果参数对象有自定义的toJSON()方法，那么JSON.stringify()会使用这个方法的返回值作为参数，而忽略原对象的其他属性
// 1
var user = {
  firstName: '三',
  lastName: '张',

  get fullName(){
    return this.lastName + this.firstName;
  }
};

JSON.stringify(user)
// "{"firstName":"三","lastName":"张","fullName":"张三"}"

// 2
var user = {
  firstName: '三',
  lastName: '张',

  get fullName(){
    return this.lastName + this.firstName;
  },

  toJSON: function () {
    return {
      name: this.lastName + this.firstName
    };
  }
};

JSON.stringify(user)
// "{"name":"张三"}"


toJSON()方法的一个应用是，将正则对象自动转为字符串
var obj = {
  reg: /foo/
};

// 不设置 toJSON 方法时
JSON.stringify(obj) // "{"reg":{}}"

// 设置 toJSON 方法时
RegExp.prototype.toJSON = RegExp.prototype.toString;
JSON.stringify(/foo/) // ""/foo/""



JSON.parse()方法用于将 JSON 字符串转换成对应的值
// 1
JSON.parse('{}') // {}
JSON.parse('true') // true
JSON.parse('"foo"') // "foo"
JSON.parse('[1, 5, "false"]') // [1, 5, "false"]
JSON.parse('null') // null

var o = JSON.parse('{"name": "张三"}');
o.name // 张三

// 2
try {
  JSON.parse("'String'");
} catch(e) {
  console.log('parsing error');
}

// 3
function f(key, value) {
  if (key === 'a') {
    return value + 10;
  }
  return value;
}

JSON.parse('{"a": 1, "b": 2}', f)
// {a: 11, b: 2}


实现对象的深拷贝
JSON.parse(JSON.stringify(obj))
这种写法，可以深度克隆一个对象，但是对象内部不能有 JSON不允许的数据类型，比如函数、正则对象、日期对象等
```

- 面向对象
```js
1.对象是单个实物的抽象
2.对象是一个容器，封装了属性（property）和方法（method）

构造函数
专门用来生成实例对象的函数

var Vehicle = function () {
  this.price = 1000;
};
构造函数名字的第一个字母通常大写

构造函数的特点：
1.函数体内部使用了this关键字，代表了所要生成的对象实例
2.生成对象的时候，必须使用new命令

// 1
var Vehicle = function (p) {
  'use strict';   // 为了保证构造函数必须与new命令一起使用，一个解决办法是，构造函数内部使用严格模式
  this.price = p;
};

var v = new Vehicle(500);


new 命令的原理:
1.创建一个空对象，作为将要返回的对象实例。
2.将这个空对象的原型，指向构造函数的prototype属性。
3.将这个空对象赋值给函数内部的this关键字。
4.开始执行构造函数内部的代码。


new命令简化的内部流程
function _new(/* 构造函数 */ constructor, /* 构造函数参数 */ params) {
  // 将 arguments 对象转为数组
  var args = [].slice.call(arguments);
  // 取出构造函数
  var constructor = args.shift();
  // 创建一个空对象，继承构造函数的 prototype 属性
  var context = Object.create(constructor.prototype);
  // 执行构造函数
  var result = constructor.apply(context, args);
  // 如果返回结果是对象，就直接返回，否则返回 context 对象
  return (typeof result === 'object' && result != null) ? result : context;
}

// 实例
var actor = _new(Person, '张三', 28);


new.target:
函数内部可以使用new.target属性。如果当前函数是new命令调用，new.target指向当前函数，否则为undefined
// 1
function f() {
  console.log(new.target === f);
}

f() // false
new f() // true

// 2 判断函数调用的时候，是否使用new命令
function f() {
  if (!new.target) {
    throw new Error('请使用 new 命令调用！');
  }
  // ...
}

f() // Uncaught Error: 请使用 new 命令调用！


Object.create() 创建实例对象
以现有的对象作为模板，生成新的实例对象

var person1 = {
  name: '张三',
  age: 38,
  greeting: function() {
    console.log('Hi! I\'m ' + this.name + '.');
  }
};

var person2 = Object.create(person1);

person2.name // 张三
person2.greeting() // Hi! I'm 张三.
// 对象person1是person2的模板，后者继承了前者的属性和方法

this 关键字
this就是属性或方法“当前”所在的对象

// 1
var person = {
  name: '张三',
  describe: function () {
    return '姓名：'+ this.name;
  }
};

person.describe()
// "姓名：张三"

// 2 属性所在的当前对象是可变的，即this的指向是可变的
var A = {
  name: '张三',
  describe: function () {
    return '姓名：'+ this.name;
  }
};

var B = {
  name: '李四'
};

B.describe = A.describe;
B.describe()
// "姓名：李四"

// 3
function f() {
  return '姓名：'+ this.name;
}

var A = {
  name: '张三',
  describe: f
};

var B = {
  name: '李四',
  describe: f
};

A.describe() // "姓名：张三"
B.describe() // "姓名：李四"

// 4
var A = {
  name: '张三',
  describe: function () {
    return '姓名：'+ this.name;
  }
};

var name = '李四';
var f = A.describe;
f() // "姓名：李四"


JavaScript 语言之所以有 this 的设计，跟内存里面的数据结构有关系

var obj = { foo:  5 };
foo属性，实际上是以下面的形式保存的
{
  foo: {
    [[value]]: 5
    [[writable]]: true
    [[enumerable]]: true
    [[configurable]]: true
  }
}

由于 属性的值可能是一个函数，因此引擎会将函数单独保存在内存中，然后再将函数的地址赋值给foo属性的value属性
{
  foo: {
    [[value]]: 函数的地址
    ...
  }
}

由于函数是一个单独的值，所以它可以在不同的环境（上下文）执行

var f = function () {};
var obj = { f: f };

// 单独执行
f()

// obj 环境执行
obj.f()


JavaScript 允许在函数体内部，引用当前环境的其他变量
var f = function () {
  console.log(x);
};

JavaScript 允许在函数体内部，引用当前环境的其他变量
由于函数可以在不同的运行环境执行，所以需要有一种机制，能够在函数体内部获得当前的运行环境（context）。所以，this就出现了，它的设计目的就是在函数体内部，指代函数当前的运行环境

this 使用场合
1.全局环境使用this，它指的就是顶层对象window
this === window // true

function f() {
  console.log(this === window);
}
f() // true

2.构造函数中的this，指的是实例对象
var Obj = function (p) {
  this.p = p;
};

3.如果对象的方法里面包含this，this的指向就是方法运行时所在的对象。该方法赋值给另一个对象，就会改变this的指向

下面这几种用法，都会改变this的指向
// 情况一
(obj.foo = obj.foo)() // window
// 情况二
(false || obj.foo)() // window
// 情况三
(1, obj.foo)() // window

// 情况一
(obj.foo = function () {
  console.log(this);
})()
// 等同于
(function () {
  console.log(this);
})()

// 情况二
(false || function () {
  console.log(this);
})()

// 情况三
(1, function () {
  console.log(this);
})()


绑定 this 的方法
JavaScript 提供了call、apply、bind这三个方法，来切换/固定this的指向
函数实例的call方法，可以指定函数内部this的指向（即函数执行时所在的作用域），然后在所指定的作用域中，调用该函数
// 1
var obj = {};

var f = function () {
  return this;
};

f() === window // true
f.call(obj) === obj // true

call方法的参数，应该是一个对象。如果参数为空、null和undefined，则默认传入全局对象
var n = 123;
var obj = { n: 456 };

function a() {
  console.log(this.n);
}

a.call() // 123
a.call(null) // 123
a.call(undefined) // 123
a.call(window) // 123
a.call(obj) // 456


如果call方法的参数是一个原始值，那么这个原始值会自动转成对应的包装对象，然后传入call方法
var f = function () {
  return this;
};

f.call(5)
// Number {[[PrimitiveValue]]: 5}


call方法接受多个参数，call的第一个参数就是this所要指向的那个对象，后面的参数则是函数调用时所需的参数
function add(a, b) {
  return a + b;
}

add.call(this, 1, 2) // 3


call方法的一个应用是调用对象的原生方法
var obj = {};
obj.hasOwnProperty('toString') // false

// 覆盖掉继承的 hasOwnProperty 方法
obj.hasOwnProperty = function () {
  return true;
};
obj.hasOwnProperty('toString') // true

Object.prototype.hasOwnProperty.call(obj, 'toString') // false
hasOwnProperty是obj对象继承的方法，如果这个方法一旦被覆盖，就不会得到正确结果
call方法可以解决这个问题，它将hasOwnProperty方法的原始定义放到obj对象上执行，这样无论obj上有没有同名方法，都不会影响结果


apply方法的作用与call方法类似，也是改变this指向，然后再调用该函数，唯一的区别就是，它接收一个数组作为函数执行时的参数
func.apply(thisValue, [arg1, arg2, ...])

// 1
function f(x, y){
  console.log(x + y);
}

f.call(null, 1, 1) // 2
f.apply(null, [1, 1]) // 2

// 1 找出数组最大元素
var a = [10, 2, 4, 15, 9];
Math.max.apply(null, a) // 15

// 2 将数组的空元素变为undefined
Array.apply(null, ['a', ,'b'])
// [ 'a', undefined, 'b' ]

//
var a = ['a', , 'b'];

function print(i) {
  console.log(i);
}

a.forEach(print)
// a
// b

Array.apply(null, a).forEach(print)
// a
// undefined
// b


// 3 转换类似数组的对象
Array.prototype.slice.apply({0: 1, length: 1}) // [1]
Array.prototype.slice.apply({0: 1}) // []
Array.prototype.slice.apply({0: 1, length: 2}) // [1, undefined]
Array.prototype.slice.apply({length: 1}) // [undefined]

// 4 绑定回调函数的对象
var o = new Object();

o.f = function () {
  console.log(this === o);
}

var f = function (){
  o.f.apply(o);
  // 或者 o.f.call(o);
};

// jQuery 的写法
$('#button').on('click', f)


bind()方法用于将函数体内的this绑定到某个对象，然后返回一个新函数
// 1
var d = new Date();
d.getTime() // 1481869925657

var print = d.getTime;
print() // Uncaught TypeError: this is not a Date object.
// getTime()方法内部的this，绑定Date对象的实例，赋给变量print以后，内部的this已经不指向Date对象的实例了

// 2
var print = d.getTime.bind(d);
print() // 1481869925657
// bind()方法将getTime()方法内部的this绑定到d对象，这时就可以安全地将这个方法赋值给其他变量了


// bind方法的参数就是所要绑定this的对象
var counter = {
  count: 0,
  inc: function () {
    this.count++;
  }
};

var func = counter.inc.bind(counter);
func();
counter.count // 1
// counter.inc()方法被赋值给变量func。这时必须用bind()方法将inc()内部的this，绑定到counter，否则就会出错

// this绑定到其他对象也是可以的
var counter = {
  count: 0,
  inc: function () {
    this.count++;
  }
};

var obj = {
  count: 100
};
var func = counter.inc.bind(obj);
func();
obj.count // 101
// bind()方法将inc()方法内部的this，绑定到obj对象。结果调用func函数以后，递增的就是obj内部的count属性

// bind()还可以接受更多的参数，将这些参数绑定原函数的参数
var add = function (x, y) {
  return x * this.m + y * this.n;
}

var obj = {
  m: 2,
  n: 2
};

var newAdd = add.bind(obj, 5);
newAdd(5) // 20

如果bind()方法的第一个参数是null或undefined，等于将this绑定到全局对象，函数运行时this指向顶层对象（浏览器为window）

bind()方法使用注意点
1. bind()方法每运行一次，就返回一个新函数
2. 结合回调函数使用
3. 结合call()方法使用


对象的继承
原型对象的作用，就是定义所有实例对象共享的属性和方法

构造函数的缺点:
// 
function Cat(name, color) {
  this.name = name;
  this.color = color;
  this.meow = function () {
    console.log('喵喵');
  };
}

var cat1 = new Cat('大毛', '白色');
var cat2 = new Cat('二毛', '黑色');

cat1.meow === cat2.meow
// false
cat1和cat2是同一个构造函数的两个实例，它们都具有meow方法。由于meow方法是生成在每个实例对象上面，所以两个实例就生成了两次，也就是说，每新建一个实例，就会新建一个meow方法


prototype 属性
JavaScript 继承机制的设计思想就是，原型对象的所有属性和方法，都能被实例对象共享

JavaScript 规定，每个函数都有一个prototype属性，指向一个对象

function f() {}
typeof f.prototype // "object"

prototype属性对于普通函数来说，该属性基本无用。但是，对于构造函数来说，生成实例的时候，该属性会自动成为实例对象的原型

// prototype
function Animal(name) {
  this.name = name;
}
Animal.prototype.color = 'white';
Animal.prototype.walk = function () {
  console.log(this.name + ' is walking');
};

var cat1 = new Animal('大毛');
var cat2 = new Animal('二毛');

cat1.color // 'white'
cat2.color // 'white'


原型链
JavaScript 规定，所有对象都有自己的原型对象（prototype）
一方面，任何一个对象，都可以充当其他对象的原型；另一方面，由于原型对象也是对象，所以它也有自己的原型。因此，就会形成一个“原型链”（prototype chain）

所有对象的原型最终都可以上溯到Object.prototype，即Object构造函数的prototype属性

Object.prototype的原型是null。null没有任何属性和方法，也没有自己的原型。因此，原型链的尽头就是null
Object.getPrototypeOf(Object.prototype)
// null


// 举例来说，如果让构造函数的prototype属性指向一个数组，就意味着实例对象可以调用数组方法
var MyArray = function () {};

MyArray.prototype = new Array();
MyArray.prototype.constructor = MyArray;

var mine = new MyArray();
mine.push(1, 2, 3);
mine.length // 3
mine instanceof Array // true

constructor 属性
prototype对象有一个constructor属性，默认指向prototype对象所在的构造函数
function P() {}
P.prototype.constructor === P // true

由于constructor属性定义在prototype对象上面，意味着可以被所有实例对象继承
function P() {}
var p = new P();

p.constructor === P // true
p.constructor === P.prototype.constructor // true
p.hasOwnProperty('constructor') // false


constructor属性的作用是，可以得知某个实例对象，到底是哪一个构造函数产生的
function F() {};
var f = new F();

f.constructor === F // true
f.constructor === RegExp // false

有了constructor属性，就可以从一个实例对象新建另一个实例
function Constr() {}
var x = new Constr();

var y = new x.constructor();
y instanceof Constr // true
// x是构造函数Constr的实例，可以从x.constructor间接调用构造函数。这使得在实例方法中，调用自身的构造函数成为可能

Constr.prototype.createCopy = function () {
  return new this.constructor();
};
// 上面代码中，createCopy方法调用构造函数，新建另一个实例


constructor属性表示原型对象与构造函数之间的关联关系，如果修改了原型对象，一般会同时修改constructor属性，防止引用的时候出错
function Person(name) {
  this.name = name;
}

Person.prototype.constructor === Person // true

Person.prototype = {
  method: function () {}
};

Person.prototype.constructor === Person // false
Person.prototype.constructor === Object // true
// 上面代码中，构造函数Person的原型对象改掉了，但是没有修改constructor属性，导致这个属性不再指向Person


所以，修改原型对象时，一般要同时修改constructor属性的指向
// 坏的写法
C.prototype = {
  method1: function (...) { ... },
  // ...
};

// 好的写法
C.prototype = {
  constructor: C,
  method1: function (...) { ... },
  // ...
};

// 更好的写法
C.prototype.method1 = function (...) { ... };
// 上面代码中，要么将constructor属性重新指向原来的构造函数，要么只在原型对象上添加方法，这样可以保证instanceof运算符不会失真


如果不能确定constructor属性是什么函数，还有一个办法：通过name属性，从实例得到构造函数的名称
function Foo() {}
var f = new Foo();
f.constructor.name // "Foo"


instanceof 运算符
instanceof运算符返回一个布尔值，表示对象是否为某个构造函数的实例
instanceof运算符的左边是实例对象，右边是构造函数

// 1
var v = new Vehicle();
v instanceof Vehicle // true


v instanceof Vehicle
// 等同于
Vehicle.prototype.isPrototypeOf(v)


由于instanceof检查整个原型链，因此同一个实例对象，可能会对多个构造函数都返回true
var d = new Date();
d instanceof Date // true
d instanceof Object // true


特殊情况：左边对象的原型链上，只有null对象
var obj = Object.create(null);
typeof obj // "object"
obj instanceof Object // false
// 唯一的instanceof运算符判断会失真的情况（一个对象的原型是null）

instanceof运算符的一个用处，是判断值的类型
var x = [1, 2, 3];
var y = {};
x instanceof Array // true
y instanceof Object // true

注意，instanceof运算符只能用于对象，不适用原始类型的值


对于undefined和null，instanceof运算符总是返回false
undefined instanceof Object // false
null instanceof Object // false


构造函数的继承
1. 在子类的构造函数中，调用父类的构造函数
2. 让子类的原型指向父类的原型，这样子类就可以继承父类原型

//
function Shape() {
  this.x = 0;
  this.y = 0;
}

Shape.prototype.move = function (x, y) {
  this.x += x;
  this.y += y;
  console.info('Shape moved.');
};

// 让Rectangle构造函数继承Shape
// 第一步，子类继承父类的实例
function Rectangle() {
  Shape.call(this); // 调用父类构造函数
}
// 另一种写法
function Rectangle() {
  this.base = Shape;
  this.base();
}

// 第二步，子类继承父类的原型
Rectangle.prototype = Object.create(Shape.prototype);
Rectangle.prototype.constructor = Rectangle;

// 测试
var rect = new Rectangle();

rect instanceof Rectangle  // true
rect instanceof Shape  // true


// 有时只需要单个方法的继承
ClassB.prototype.print = function() {
  ClassA.prototype.print.call(this);
  // some code
}
// 子类B的print方法先调用父类A的print方法，再部署自己的代码


多重继承
JavaScript 不提供多重继承功能，即不允许一个对象同时继承多个对象。但是，可以通过变通方法，实现这个功能

function M1() {
  this.hello = 'hello';
}

function M2() {
  this.world = 'world';
}

function S() {
  M1.call(this);
  M2.call(this);
}

// 继承 M1
S.prototype = Object.create(M1.prototype);
// 继承链上加入 M2
Object.assign(S.prototype, M2.prototype);

// 指定构造函数
S.prototype.constructor = S;

var s = new S();
s.hello // 'hello'
s.world // 'world


模块
模块是实现特定功能的一组属性和方法的封装

// 1 简单的做法是把模块写成一个对象，所有的模块成员都放到这个对象里面
var module1 = new Object({
　_count : 0,
　m1 : function (){
　　//...
　},
　m2 : function (){
  　//...
　}
});

缺点：这样的写法会暴露所有模块成员，内部状态可以被外部改写。比如，外部代码可以直接改变内部计数器的值


// 2 封装私有变量：构造函数的写法
function StringBuilder() {
  var buffer = [];

  this.add = function (str) {
     buffer.push(str);
  };

  this.toString = function () {
    return buffer.join('');
  };

}
缺点：将私有变量封装在构造函数中，导致构造函数与实例对象是一体的，总是存在于内存之中，无法在使用完成后清除。这意味着，构造函数有双重作用，既用来塑造实例对象，又用来保存实例对象的数据，违背了构造函数与实例对象在数据上相分离的原则（即实例对象的数据，不应该保存在实例对象以外）。同时，非常耗费内存

// 改进
function StringBuilder() {
  this._buffer = [];
}

StringBuilder.prototype = {
  constructor: StringBuilder,
  add: function (str) {
    this._buffer.push(str);
  },
  toString: function () {
    return this._buffer.join('');
  }
};
缺点：将私有变量放入实例对象中，好处是看上去更自然，但是它的私有变量可以从外部读写，不是很安全


// 3 封装私有变量：立即执行函数的写法
var module1 = (function () {
　var _count = 0;
　var m1 = function () {
　  //...
　};
　var m2 = function () {
　　//...
　};
　return {
　　m1 : m1,
　　m2 : m2
　};
})();
// JavaScript 模块的基本写法


模块的放大模式 augmentation
// 1
var module1 = (function (mod){
　mod.m3 = function () {
　　//...
　};
　return mod;
})(module1);
// 为module1模块添加了一个新方法m3()，然后返回新的module1模块

// 2 "宽放大模式"（Loose augmentation）
var module1 = (function (mod) {
　//...
　return mod;
})(window.module1 || {});
// 与"放大模式"相比，“宽放大模式”就是“立即执行函数”的参数可以是空对象


输入全局变量
独立性是模块的重要特点，模块内部最好不与程序的其他部分直接交互，为了在模块内部调用全局变量，必须显式地将其他变量输入模块
var module1 = (function ($, YAHOO) {
　//...
})(jQuery, YAHOO);
// module1模块需要使用 jQuery 库和 YUI 库，就把这两个库（其实是两个模块）当作参数输入module1。这样做除了保证模块的独立性，还使得模块之间的依赖关系变得明显


Object 对象的相关方法
Object.getPrototypeOf方法返回参数对象的原型。这是获取原型对象的标准方法
// 1
var F = function () {};
var f = new F();
Object.getPrototypeOf(f) === F.prototype // true

// 2 几种特殊对象的原型
// 空对象的原型是 Object.prototype
Object.getPrototypeOf({}) === Object.prototype // true

// Object.prototype 的原型是 null
Object.getPrototypeOf(Object.prototype) === null // true

// 函数的原型是 Function.prototype
function f() {}
Object.getPrototypeOf(f) === Function.prototype // true


Object.setPrototypeOf方法为参数对象设置原型，返回该参数对象。它接受两个参数，第一个是现有对象，第二个是原型对象。
var a = {};
var b = {x: 1};
Object.setPrototypeOf(a, b);

Object.getPrototypeOf(a) === b // true
a.x // 1


Object.create()方法
从一个实例对象，生成另一个实例对象

// 原型对象
var A = {
  print: function () {
    console.log('hello');
  }
};

// 实例对象
var B = Object.create(A);

Object.getPrototypeOf(B) === A // true
B.print() // hello
B.print === A.print // true

下面三种方式生成的新对象是等价的
var obj1 = Object.create({});
var obj2 = Object.create(Object.prototype);
var obj3 = new Object();

Object.create()方法生成的新对象，动态继承了原型。在原型上添加或修改任何方法，会立刻反映在新对象之上
var obj1 = { p: 1 };
var obj2 = Object.create(obj1);

obj1.p = 2;
obj2.p // 2


Object.create()方法还可以接受第二个参数。该参数是一个属性描述对象，它所描述的对象属性，会添加到实例对象，作为该对象自身的属性
var obj = Object.create({}, {
  p1: {
    value: 123,
    enumerable: true,
    configurable: true,
    writable: true,
  },
  p2: {
    value: 'abc',
    enumerable: true,
    configurable: true,
    writable: true,
  }
});

// 等同于
var obj = Object.create({});
obj.p1 = 123;
obj.p2 = 'abc';


Object.create()方法生成的对象，继承了它的原型对象的构造函数
function A() {}
var a = new A();
var b = Object.create(a);

b.constructor === A // true
b instanceof A // true

isPrototypeOf方法，用来判断该对象是否为参数对象的原型
var o1 = {};
var o2 = Object.create(o1);
var o3 = Object.create(o2);

o2.isPrototypeOf(o3) // true
o1.isPrototypeOf(o3) // true


Object.prototype.isPrototypeOf({}) // true
Object.prototype.isPrototypeOf([]) // true
Object.prototype.isPrototypeOf(/xyz/) // true
Object.prototype.isPrototypeOf(Object.create(null)) // false


实例对象的__proto__属性（前后各两个下划线），返回该对象的原型。该属性可读写
var obj = {};
var p = {};

obj.__proto__ = p;
Object.getPrototypeOf(obj) === p // true

Object.getOwnPropertyNames方法返回一个数组，成员是参数对象本身的所有属性的键名，不包含继承的属性键名
Object.getOwnPropertyNames(Date)
// ["parse", "arguments", "UTC", "caller", "name", "prototype", "now", "length"]


对象实例的hasOwnProperty方法返回一个布尔值，用于判断某个属性定义在对象自身，还是定义在原型链上
Date.hasOwnProperty('length') // true
Date.hasOwnProperty('toString') // false

in运算符常用于检查一个属性是否存在
in运算符返回一个布尔值，表示一个对象是否具有某个属性。它不区分该属性是对象自身的属性，还是继承的属性
'length' in Date // true
'toString' in Date // true

获得对象的所有可遍历属性
var o1 = { p1: 123 };

var o2 = Object.create(o1, {
  p2: { value: "abc", enumerable: true }
});

for (p in o2) {
  console.info(p);
}
// p2
// p1

在for...in循环中获得对象自身的属性
for ( var name in object ) {
  if ( object.hasOwnProperty(name) ) {
    /* loop code */
  }
}

获得对象的所有属性的函数
function inheritedPropertyNames(obj) {
  var props = {};
  while(obj) {
    Object.getOwnPropertyNames(obj).forEach(function(p) {
      props[p] = true;
    });
    obj = Object.getPrototypeOf(obj);
  }
  return Object.getOwnPropertyNames(props);
}


对象的拷贝
1.确保拷贝后的对象，与原对象具有同样的原型
2.确保拷贝后的对象，与原对象具有同样的实例属性

// 1
function copyObject(orig) {
  var copy = Object.create(Object.getPrototypeOf(orig));
  copyOwnPropertiesFrom(copy, orig);
  return copy;
}

function copyOwnPropertiesFrom(target, source) {
  Object
    .getOwnPropertyNames(source)
    .forEach(function (propKey) {
      var desc = Object.getOwnPropertyDescriptor(source, propKey);
      Object.defineProperty(target, propKey, desc);
    });
  return target;
}

// 2
function copyObject(orig) {
  return Object.create(
    Object.getPrototypeOf(orig),
    Object.getOwnPropertyDescriptors(orig)
  );
}


严格模式
为了兼容以前的代码，又不能改变老的语法，只能不断添加新的语法，引导程序员使用新语法

严格模式可以用于整个脚本，也可以只用于单个函数
'use strict';

1.use strict放在脚本文件的第一行，整个脚本都将以严格模式运行
<script>
  'use strict';
  console.log('这是严格模式');
</script>

2.use strict放在函数体的第一行，则整个函数以严格模式运行
function strict() {
  'use strict';
  return '这是严格模式';
}

```

- 异步操作
```js
单线程模型
JavaScript 只在一个线程上运行

注意，JavaScript 只在一个线程上运行，不代表 JavaScript 引擎只有一个线程。事实上，JavaScript 引擎有多个线程，单个脚本只能在一个线程上运行（称为主线程），其他线程都是在后台配合

异步操作的模式
1.回调函数
function f1(callback) {
  // ...
  callback();
}

function f2() {
  // ...
}

f1(f2);

缺点：不利于代码的阅读和维护，各个部分之间高度耦合（coupling），使得程序结构混乱、流程难以追踪，而且每个任务只能指定一个回调函数

2.事件监听

为f1绑定一个事件
f1.on('done', f2);

对f1进行改写：
function f1() {
  setTimeout(function () {
    // ...
    f1.trigger('done');
  }, 1000);
}
// f1.trigger('done')表示，执行完成后，立即触发done事件，从而开始执行f2

缺点：整个程序都要变成事件驱动型，运行流程会变得很不清晰。阅读代码的时候，很难看出主流程


3.发布/订阅

f2向信号中心jQuery订阅done信号
jQuery.subscribe('done', f2);

f1进行如下改写
function f1() {
  setTimeout(function () {
    // ...
    jQuery.publish('done');
  }, 1000);
}

jQuery.publish('done')的意思是，f1执行完成后，向信号中心jQuery发布done信号，从而引发f2的执行

f2完成执行后，可以取消订阅（unsubscribe）
jQuery.unsubscribe('done', f2);


异步操作的流程控制
1.串行执行
编写一个流程控制函数，让它来控制异步任务，一个任务完成以后，再执行另一个
var items = [ 1, 2, 3, 4, 5, 6 ];
var results = [];

function async(arg, callback) {
  console.log('参数为 ' + arg +' , 1秒后返回结果');
  setTimeout(function () { callback(arg * 2); }, 1000);
}

function final(value) {
  console.log('完成: ', value);
}

function series(item) {
  if(item) {
    async( item, function(result) {
      results.push(result);
      return series(items.shift());
    });
  } else {
    return final(results[results.length - 1]);
  }
}

series(items.shift());

2.并行执行
所有异步任务同时执行，等到全部完成以后，才执行final函数
var items = [ 1, 2, 3, 4, 5, 6 ];
var results = [];

function async(arg, callback) {
  console.log('参数为 ' + arg +' , 1秒后返回结果');
  setTimeout(function () { callback(arg * 2); }, 1000);
}

function final(value) {
  console.log('完成: ', value);
}

items.forEach(function(item) {
  async(item, function(result){
    results.push(result);
    if(results.length === items.length) {
      final(results[results.length - 1]);
    }
  })
});
缺点：如果并行的任务较多，很容易耗尽系统资源，拖慢运行速度

3.并行与串行的结合
设置一个门槛，每次最多只能并行执行n个异步任务
var items = [ 1, 2, 3, 4, 5, 6 ];
var results = [];
var running = 0;
var limit = 2;

function async(arg, callback) {
  console.log('参数为 ' + arg +' , 1秒后返回结果');
  setTimeout(function () { callback(arg * 2); }, 1000);
}

function final(value) {
  console.log('完成: ', value);
}

function launcher() {
  while(running < limit && items.length > 0) {
    var item = items.shift();
    async(item, function(result) {
      results.push(result);
      running--;
      if(items.length > 0) {
        launcher();
      } else if(running === 0) {
        final(results);
      }
    });
    running++;
  }
}

launcher();


定时器
JavaScript 提供定时执行代码的功能，叫做定时器（timer），主要由setTimeout()和setInterval()这两个函数来完成。它们向任务队列添加定时任务
setTimeout和setInterval函数，都返回一个整数值，表示计数器编号

setTimeout(function (a,b) {
  console.log(a + b);
}, 1000, 1, 1)

setInterval指定某个任务每隔一段时间就执行一次，也就是无限次的定时执行

// setInterval的一个常见用途是实现轮询
var hash = window.location.hash;
var hashWatcher = setInterval(function() {
  if (window.location.hash != hash) {
    updatePage();
  }
}, 1000);

// 为了确保两次执行之间有固定的间隔，可以不用setInterval，而是每次执行结束后，使用setTimeout指定下一次执行的具体时间
var i = 1;
var timer = setTimeout(function f() {
  // ...
  timer = setTimeout(f, 2000);
}, 2000);


clearTimeout和clearInterval函数用来取消对应的定时器
// 1
var id1 = setTimeout(f, 1000);
var id2 = setInterval(f, 1000);

clearTimeout(id1);
clearInterval(id2);

// 取消当前所有的setTimeout定时器
(function() {
  // 每轮事件循环检查一次
  var gid = setInterval(clearAllTimeouts, 0);

  function clearAllTimeouts() {
    var id = setTimeout(function() {}, 0);
    while (id > 0) {
      if (id !== gid) {
        clearTimeout(id);
      }
      id--;
    }
  }
})();


运行机制
setTimeout和setInterval的运行机制，是将指定的代码移出本轮事件循环，等到下一轮事件循环，再检查是否到了指定时间。如果到了，就执行对应的代码；如果不到，就继续等待
setTimeout和setInterval指定的回调函数，必须等到本轮事件循环的所有同步任务都执行完，才会开始执行，但是的任务到底需要多少时间执行完，是不确定的


Promise 对象
Promise 对象是 JavaScript 的异步操作解决方案，为异步操作提供统一接口
Promise 可以让异步操作写起来，就像在写同步操作的流程，而不必一层层地嵌套回调函数

Promise 的设计思想是，所有异步任务都返回一个 Promise 实例。Promise 实例有一个then方法，用来指定下一步的回调函数

// 1
function f1(resolve, reject) {
  // 异步代码...
}

var p1 = new Promise(f1);

p1.then(f2);


Promise 对象的状态

Promise 实例具有三种状态:
异步操作未完成（pending）
异步操作成功（fulfilled）
异步操作失败（rejected

三种的状态的变化途径只有两种
从“未完成”到“成功”
从“未完成”到“失败”

Promise 的最终结果只有两种
异步操作成功，Promise 实例传回一个值（value），状态变为fulfilled
异步操作失败，Promise 实例抛出一个错误（error），状态变为rejected


Promise 构造函数
var promise = new Promise(function (resolve, reject) {
  // ...

  if (/* 异步操作成功 */){
    resolve(value);
  } else { /* 异步操作失败 */
    reject(new Error());
  }
});

resolve和reject两个函数 由 JavaScript 引擎提供，不用自己实现
resolve函数的作用是，将Promise实例的状态从“未完成”变为“成功”（即从pending变为fulfilled），在异步操作成功时调用，并将异步操作的结果，作为参数传递出去
reject函数的作用是，将Promise实例的状态从“未完成”变为“失败”（即从pending变为rejected），在异步操作失败时调用，并将异步操作报出的错误，作为参数传递出去

// 1
function timeout(ms) {
  return new Promise((resolve, reject) => {
    setTimeout(resolve, ms, 'done');
  });
}

timeout(100)
// timeout(100)返回一个 Promise 实例。100毫秒以后，该实例的状态会变为fulfilled

Promise 实例的then方法，用来添加回调函数
then方法可以接受两个回调函数:
第一个是异步操作成功时（变为fulfilled状态）的回调函数
第二个是异步操作失败（变为rejected）时的回调函数（该参数可以省略）
一旦状态改变，就调用相应的回调函数

// 1
var p1 = new Promise(function (resolve, reject) {
  resolve('成功');
});
p1.then(console.log, console.error);
// "成功"

var p2 = new Promise(function (resolve, reject) {
  reject(new Error('失败'));
});
p2.then(console.log, console.error);
// Error: 失败

// then方法的链式使用
p1
  .then(step1)
  .then(step2)
  .then(step3)
  .then(
    console.log,
    console.error
  );
// p1后面有四个then，意味依次有四个回调函数。只要前一步的状态变为fulfilled，就会依次执行紧跟在后面的回调函数


then方法添加回调函数的不同的写法
// 写法一
f1().then(function () {
  return f2();
});

// 写法二
f1().then(function () {
  f2();
});

// 写法三
f1().then(f2());

// 写法四
f1().then(f2);


```

- DOM
```js
DOM 是 JavaScript 操作网页的接口，全称为“文档对象模型”（Document Object Model）
DOM 的作用是将网页转为一个 JavaScript 对象，从而可以用脚本进行各种操作（比如增删内容）
DOM 只是一个接口规范，可以用各种语言实现

DOM 的最小组成单位叫做节点（node）。文档的树形结构（DOM 树），就是由各种不同类型的节点组成

节点的类型:
Document：整个文档树的顶层节点
DocumentType：doctype标签（比如<!DOCTYPE html>）
Element：网页的各种HTML标签（比如<body>、<a>等）
Attr：网页元素的属性（比如class="right"）
Text：标签之间或标签包含的文本
Comment：注释
DocumentFragment：文档的片段

浏览器提供一个原生的节点对象Node，上面这七种节点都继承了Node，因此具有一些共同的属性和方法

一个文档的所有节点，按照所在的层级，可以抽象成一种树状结构。这种树状结构就是 DOM 树

浏览器原生提供document节点，代表整个文档
document
// 整个文档树

除了根节点，其他节点都有三种层级关系
父节点关系（parentNode）：直接的那个上级节点
子节点关系（childNodes）：直接的下级节点
同级节点关系（sibling）：拥有同一个父节点的节点

DOM 提供操作接口，用来获取这三种关系的节点
子节点接口包括firstChild（第一个子节点）和lastChild（最后一个子节点）等属
同级节点接口包括nextSibling（紧邻在后的那个同级节点）和previousSibling（紧邻在前的那个同级节点）属性


Node 接口
所有 DOM 节点对象都继承了 Node 接口，拥有一些共同的属性和方法。这是 DOM 操作的基础

nodeType属性返回一个整数值，表示节点的类型
不同节点的nodeType属性值和对应的常量如下
文档节点（document）：9，对应常量Node.DOCUMENT_NODE
元素节点（element）：1，对应常量Node.ELEMENT_NODE
属性节点（attr）：2，对应常量Node.ATTRIBUTE_NODE
文本节点（text）：3，对应常量Node.TEXT_NODE
文档片断节点（DocumentFragment）：11，对应常量Node.DOCUMENT_FRAGMENT_NODE
文档类型节点（DocumentType）：10，对应常量Node.DOCUMENT_TYPE_NODE
注释节点（Comment）：8，对应常量Node.COMMENT_NODE

// 确定节点类型
var node = document.documentElement.firstChild;
if (node.nodeType === Node.ELEMENT_NODE) {
  console.log('该节点是元素节点');
}

nodeName属性返回节点的名称
// HTML 代码如下
// <div id="d1">hello world</div>
var div = document.getElementById('d1');
div.nodeName // "DIV"

不同节点的nodeName属性值如下
文档节点（document）：#document
元素节点（element）：大写的标签名
属性节点（attr）：属性的名称
文本节点（text）：#text
文档片断节点（DocumentFragment）：#document-fragment
文档类型节点（DocumentType）：文档的类型
注释节点（Comment）：#comment


nodeValue属性返回一个字符串，表示当前节点本身的文本值，该属性可读写
// HTML 代码如下
// <div id="d1">hello world</div>
var div = document.getElementById('d1');
div.nodeValue // null
div.firstChild.nodeValue // "hello world"

textContent属性返回当前节点和它的所有后代节点的文本内容
// HTML 代码为
// <div id="divA">This is <span>some</span> text</div>

document.getElementById('divA').textContent
// This is some text


baseURI属性返回一个字符串，表示当前网页的绝对路径// 当前网页的网址为
// http://www.example.com/index.html
document.baseURI
// "http://www.example.com/index.html"


Node.ownerDocument属性返回当前节点所在的顶层文档对象，即document对象
var d = p.ownerDocument;
d === document // true


Node.nextSibling属性返回紧跟在当前节点后面的第一个同级节点。如果当前节点后面没有同级节点，则返回null
// HTML 代码如下
// <div id="d1">hello</div><div id="d2">world</div>
var d1 = document.getElementById('d1');
var d2 = document.getElementById('d2');

d1.nextSibling === d2 // true


previousSibling属性返回当前节点前面的、距离最近的一个同级节点。如果当前节点前面没有同级节点，则返回null
// HTML 代码如下
// <div id="d1">hello</div><div id="d2">world</div>
var d1 = document.getElementById('d1');
var d2 = document.getElementById('d2');

d2.previousSibling === d1 // true


parentNode属性返回当前节点的父节点。对于一个节点来说，它的父节点只可能是三种类型：元素节点（element）、文档节点（document）和文档片段节点（documentfragment
if (node.parentNode) {
  node.parentNode.removeChild(node);
}

parentElement属性返回当前节点的父元素节点。如果当前节点没有父节点，或者父节点类型不是元素节点，则返回null
if (node.parentElement) {
  node.parentElement.style.color = 'red';
}

firstChild属性返回当前节点的第一个子节点，如果当前节点没有子节点，则返回null
// HTML 代码如下
// <p id="p1"><span>First span</span></p>
var p1 = document.getElementById('p1');
p1.firstChild.nodeName // "SPAN"


childNodes属性返回一个类似数组的对象（NodeList集合），成员包括当前节点的所有子节点
var children = document.querySelector('ul').childNodes;

// 遍历某个节点的所有子节点
var div = document.getElementById('div1');
var children = div.childNodes;

for (var i = 0; i < children.length; i++) {
  // ...
}


isConnected属性返回一个布尔值，表示当前节点是否在文档之中
var test = document.createElement('p');
test.isConnected // false

document.body.appendChild(test);
test.isConnected // true


方法
appendChild()方法接受一个节点对象作为参数，将其作为最后一个子节点，插入当前节点。该方法的返回值就是插入文档的子节点
var p = document.createElement('p');
document.body.appendChild(p);

hasChildNodes方法返回一个布尔值，表示当前节点是否有子节点
var foo = document.getElementById('foo');

if (foo.hasChildNodes()) {
  foo.removeChild(foo.childNodes[0]);
}

// hasChildNodes方法结合firstChild属性和nextSibling属性，可以遍历当前节点的所有后代节点
function DOMComb(parent, callback) {
  if (parent.hasChildNodes()) {
    for (var node = parent.firstChild; node; node = node.nextSibling) {
      DOMComb(node, callback);
    }
  }
  callback(parent);
}

// 用法
DOMComb(document.body, console.log)


cloneNode方法用于克隆一个节点。它接受一个布尔值作为参数，表示是否同时克隆子节点。它的返回值是一个克隆出来的新节点
var cloneUL = document.querySelector('ul').cloneNode(true);


insertBefore方法用于将某个节点插入父节点内部的指定位置
var insertedNode = parentNode.insertBefore(newNode, referenceNode);


removeChild方法接受一个子节点作为参数，用于从当前节点移除该子节点。返回值是移除的子节点
var divA = document.getElementById('A');
divA.parentNode.removeChild(divA);

// 移除当前节点的所有子节点
var element = document.getElementById('top');
while (element.firstChild) {
  element.removeChild(element.firstChild);
}


replaceChild方法用于将一个新的节点，替换当前节点的某一个子节点
var replacedNode = parentNode.replaceChild(newChild, oldChild);

contains方法返回一个布尔值，表示参数节点是否满足以下三个条件之一:
参数节点为当前节点。
参数节点为当前节点的子节点
参数节点为当前节点的后代节点

document.body.contains(node)


compareDocumentPosition方法的用法，与contains方法完全一致，返回一个六个比特位的二进制值，表示参数节点与当前节点的关系
// HTML 代码如下
// <div id="mydiv">
//   <form><input id="test" /></form>
// </div>

var div = document.getElementById('mydiv');
var input = document.getElementById('test');

div.compareDocumentPosition(input) // 20
input.compareDocumentPosition(div) // 10


isEqualNode方法返回一个布尔值，用于检查两个节点是否相等。所谓相等的节点，指的是两个节点的类型相同、属性相同、子节点相同
// 1
var p1 = document.createElement('p');
var p2 = document.createElement('p');

p1.isEqualNode(p2) // true

// 2
var p1 = document.createElement('p');
var p2 = document.createElement('p');

p1.isSameNode(p2) // false
p1.isSameNode(p1) // true

normalize方法用于清理当前节点内部的所有文本节点（text）。它会去除空的文本节点，并且将毗邻的文本节点合并成一个，也就是说不存在空的文本节点，以及毗邻的文本节点
var wrapper = document.createElement('div');

wrapper.appendChild(document.createTextNode('Part 1 '));
wrapper.appendChild(document.createTextNode('Part 2 '));

wrapper.childNodes.length // 2
wrapper.normalize();
wrapper.childNodes.length // 1


getRootNode()方法返回当前节点所在文档的根节点document，与ownerDocument属性的作用相同
document.body.firstChild.getRootNode() === document
// true
document.body.firstChild.getRootNode() === document.body.firstChild.ownerDocument
// true



NodeList 接口，HTMLCollection 接口
节点都是单个对象，有时需要一种数据结构，能够容纳多个节点。DOM 提供两种节点集合，用于容纳多个节点：NodeList和HTMLCollection

NodeList可以包含各种类型的节点，HTMLCollection只能包含 HTML 元素节点


NodeList 接口
NodeList实例是一个类似数组的对象，它的成员是节点对象
// 1
document.body.childNodes instanceof NodeList // true

// 2
var children = document.body.childNodes;

Array.isArray(children) // false

children.length // 34
children.forEach(console.log)

// 如果NodeList实例要使用数组方法，可以将其转为真正的数组
var children = document.body.childNodes;
var nodeArr = Array.prototype.slice.call(children);

// for循环 遍历 NodeList 实例
var children = document.body.childNodes;

for (var i = 0; i < children.length; i++) {
  var item = children[i];
}


length属性返回 NodeList 实例包含的节点数量
document.querySelectorAll('xxx').length
// 0

forEach方法用于遍历 NodeList 的所有成员。它接受一个回调函数作为参数，每一轮遍历就执行一次这个回调函数，用法与数组实例的forEach方法完全一致
var children = document.body.childNodes;
children.forEach(function f(item, i, list) {
  // ...
}, this);


item方法接受一个整数值作为参数，表示成员的位置，返回该位置上的成员
document.body.childNodes.item(0)

所有类似数组的对象，都可以使用方括号运算符取出成员
document.body.childNodes[0]


keys()返回键名的遍历器，values()返回键值的遍历器，entries()返回的遍历器同时包含键名和键值的信息
var children = document.body.childNodes;

for (var key of children.keys()) {
  console.log(key);
}
// 0
// 1
// 2
// ...

for (var value of children.values()) {
  console.log(value);
}
// #text
// <script>
// ...

for (var entry of children.entries()) {
  console.log(entry);
}
// Array [ 0, #text ]
// Array [ 1, <script> ]
// ...


HTMLCollection 接口
HTMLCollection是一个节点对象的集合，只能包含元素节点（element），不能包含其他类型的节点

HTMLCollection的返回值是一个类似数组的对象，但是与NodeList接口不同，HTMLCollection没有forEach方法，只能使用for循环遍历

length属性返回HTMLCollection实例包含的成员数量
document.links.length // 18

item方法接受一个整数值作为参数，表示成员的位置，返回该位置上的成员
var c = document.images;
var img0 = c.item(0);

namedItem方法的参数是一个字符串，表示id属性或name属性的值，返回当前集合中对应的元素节点。如果没有对应的节点，则返回null
// HTML 代码如下
// <img id="pic" src="http://example.com/foo.jpg">

var pic = document.getElementById('pic');
document.images.namedItem('pic') === pic // true



ParentNode 接口，ChildNode 接口
ParentNode接口表示当前节点是一个父节点，提供一些处理子节点的方法。ChildNode接口表示当前节点是一个子节点，提供一些相关方法

children属性返回一个HTMLCollection实例，成员是当前节点的所有元素子节点。该属性只读

firstElementChild属性返回当前节点的第一个元素子节点。如果没有任何元素子节点，则返回null

lastElementChild属性返回当前节点的最后一个元素子节点，如果不存在任何元素子节点，则返回null

childElementCount属性返回一个整数，表示当前节点的所有元素子节点的数目。如果不包含任何元素子节点，则返回0

append()方法为当前节点追加一个或多个子节点，位置是最后一个元素子节点的后面

prepend()方法为当前节点追加一个或多个子节点，位置是第一个元素子节点的前面


如果一个节点有父节点，那么该节点就拥有了ChildNode接口
remove()方法用于从父节点移除当前节点

before()方法用于在当前节点的前面，插入一个或多个同级节点。两者拥有相同的父节点

after()方法用于在当前节点的后面，插入一个或多个同级节点，两者拥有相同的父节点

replaceWith()方法使用参数节点，替换当前节点。参数可以是元素节点，也可以是文本节点


Document 节点

document节点对象代表整个文档，每张网页都有自己的document对象

window.document属性就指向这个对象

document对象的不同获取办法:
正常的网页，直接使用document或window.document。
iframe框架里面的网页，使用iframe节点的contentDocument属性。
Ajax 操作返回的文档，使用XMLHttpRequest对象的responseXML属性。
内部节点的ownerDocument属性


属性 
document.defaultView属性返回document对象所属的window对象。如果当前文档不属于window对象，该属性返回null

对于 HTML 文档来说，document对象一般有两个子节点，第一个子节点是document.doctype，指向<DOCTYPE>节点，即文档类型（Document Type Declaration，简写DTD）节点

document.documentElement属性返回当前文档的根元素节点（root）

document.body属性指向<body>节点，document.head属性指向<head>节点

document.scrollingElement属性返回文档的滚动元素。也就是说，当文档整体滚动时，到底是哪个元素在滚动

document.activeElement属性返回获得当前焦点（focus）的 DOM 元素

document.fullscreenElement属性返回当前以全屏状态展示的 DOM 元素


节点集合属性

document.links属性返回当前文档所有设定了href属性的<a>及<area>节点

document.forms属性返回所有<form>表单节点

document.images属性返回页面所有<img>图片节点

document.embeds属性和document.plugins属性，都返回所有<embed>节点

document.scripts属性返回所有<script>节点

document.styleSheets属性返回网页内嵌或引入的 CSS 样式表集合

文档静态信息属性
document.documentURI属性和document.URL属性都返回一个字符串，表示当前文档的网址

document.domain属性返回当前文档的域名，不包含协议和端口

Location对象是浏览器提供的原生对象，提供 URL 相关的信息和操作方法

document.lastModified属性返回一个字符串，表示当前文档最后修改的时间

document.title属性返回当前文档的标题

document.characterSet属性返回当前文档的编码

document.referrer属性返回一个字符串，表示当前文档的访问者来自哪里

document.dir返回一个字符串，表示文字方向。它只有两个可能的值：rtl表示文字从右到左，阿拉伯文是这种方式；ltr表示文字从左到右，包括英语和汉语在内的大多数文字采用这种方式

compatMode属性返回浏览器处理文档的模式，可能的值为BackCompat（向后兼容模式）和CSS1Compat（严格模式）


文档状态属性
document.hidden属性返回一个布尔值，表示当前页面是否可见

document.visibilityState返回文档的可见状态

document.readyState属性返回当前文档的状态，共有三种可能的值
loading：加载 HTML 代码阶段（尚未完成解析）
interactive：加载外部资源阶段
complete：加载完成


document.cookie属性用来操作浏览器 Cookie

document.designMode属性控制当前文档是否可编辑

document.currentScript属性只用在<script>元素的内嵌脚本或加载的外部脚本之中，返回当前脚本所在的那个 DOM 节点，即<script>元素的 DOM 节点

document.implementation属性返回一个DOMImplementation对象。该对象有三个方法，主要用于创建独立于当前文档的新的 Document 对象
DOMImplementation.createDocument()：创建一个 XML 文档
DOMImplementation.createHTMLDocument()：创建一个 HTML 文档
DOMImplementation.createDocumentType()：创建一个 DocumentType 对象


方法
document.open方法清除当前文档所有内容，使得文档处于可写状态，供document.write方法写入内容
document.close方法用来关闭document.open()打开的文档

document.write方法用于向当前文档写入内容

document.querySelector方法接受一个 CSS 选择器作为参数，返回匹配该选择器的元素节点。如果有多个节点满足匹配条件，则返回第一个匹配的节点。如果没有发现匹配的节点，则返回null

document.getElementsByTagName()方法搜索 HTML 标签名，返回符合条件的元素

document.getElementsByClassName()方法返回一个类似数组的对象（HTMLCollection实例），包括了所有class名字符合指定条件的元素，元素的变化实时反映在返回结果中

document.getElementsByName()方法用于选择拥有name属性的 HTML 元素（比如<form>、<radio>、<img>、<frame>、<embed>和<object>等），返回一个类似数组的的对象（NodeList实例），因为name属性相同的元素可能不止一个

document.getElementById()方法返回匹配指定id属性的元素节点。如果没有发现匹配的节点，则返回null

document.elementFromPoint()方法返回位于页面指定位置最上层的元素节点

document.createElement方法用来生成元素节点，并返回该节点

document.createTextNode方法用来生成文本节点（Text实例），并返回该节点。它的参数是文本节点的内容

document.createAttribute方法生成一个新的属性节点（Attr实例），并返回它

document.createComment方法生成一个新的注释节点，并返回该节点

document.createDocumentFragment方法生成一个空的文档片段对象（DocumentFragment实例）

document.createEvent方法生成一个事件对象（Event实例），该对象可以被element.dispatchEvent方法使用，触发指定事件

addEventListener,removeEventListener,dispatchEvent 用于处理document节点的事件，都继承自EventTarget接口，用于处理document节点的事件

document.hasFocus方法返回一个布尔值，表示当前文档之中是否有元素被激活或获得焦点

document.adoptNode方法将某个节点及其子节点，从原来所在的文档或DocumentFragment里面移除，归属当前document对象，返回插入后的新节点
document.importNode方法则是从原来所在的文档或DocumentFragment里面，拷贝某个节点及其子节点，让它们归属当前document对象

document.createNodeIterator方法返回一个子节点遍历器

document.createTreeWalker方法返回一个 DOM 的子树遍历器

execCommand方法可以用来改变内容的样式

queryCommandSupported()方法返回一个布尔值，表示浏览器是否支持document.execCommand()的某个命令

document.queryCommandEnabled()方法返回一个布尔值，表示当前是否可用document.execCommand()的某个命令


Element 节点
Element节点对象对应网页的 HTML 元素。每一个 HTML 元素，在 DOM 树上都会转化成一个Element节点对象（以下简称元素节点），元素节点的nodeType属性都是1



实例属性

Element.id属性返回指定元素的id属性，该属性可读写

Element.tagName属性返回指定元素的大写标签名，与nodeName属性的值相等

Element.dir属性用于读写当前元素的文字方向，可能是从左到右（"ltr"），也可能是从右到左（"rtl"）

Element.accessKey属性用于读写分配给当前元素的快捷键

Element.draggable属性返回一个布尔值，表示当前元素是否可拖动。该属性可读写

Element.lang属性返回当前元素的语言设置。该属性可读写

Element.tabIndex属性返回一个整数，表示当前元素在 Tab 键遍历时的顺序。该属性可读写

Element.title属性用来读写当前元素的 HTML 属性title。该属性通常用来指定，鼠标悬浮时弹出的文字提示框


元素状态的相关属性

Element.hidden属性返回一个布尔值，表示当前 HTML 元素的hidden属性的值

HTML 元素可以设置contentEditable属性，使得元素的内容可以编辑
Element.isContentEditable属性返回一个布尔值，同样表示是否设置了contenteditable属性。该属性只读

Element.attributes属性返回一个类似数组的对象，成员是当前元素节点的所有属性节点

className属性用来读写当前元素节点的class属性
classList属性返回一个类似数组的对象，当前元素节点的每个class就是这个对象的一个成员

Element.dataset属性返回一个对象，可以从这个对象读写data-属性

Element.innerHTML属性返回一个字符串，等同于该元素包含的所有 HTML 代码。该属性可读写，常用来设置某个节点的内容

Element.outerHTML属性返回一个字符串，表示当前元素节点的所有 HTML 代码，包括该元素本身和所有子元素

Element.clientHeight属性返回一个整数值，表示元素节点的 CSS 高度（单位像素），只对块级元素生效，对于行内元素返回0。如果块级元素没有设置 CSS 高度，则返回实际高度

Element.clientLeft属性等于元素节点左边框（left border）的宽度（单位像素），不包括左侧的padding和margin

Element.scrollHeight属性返回一个整数值（小数会四舍五入），表示当前元素的总高度（单位像素）

Element.scrollLeft属性表示当前元素的水平滚动条向右侧滚动的像素数量，Element.scrollTop属性表示当前元素的垂直滚动条向下滚动的像素数量

Element.offsetParent属性返回最靠近当前元素的、并且 CSS 的position属性不等于static的上层元素

Element.offsetHeight属性返回一个整数，表示元素的 CSS 垂直高度（单位像素）

Element.offsetLeft返回当前元素左上角相对于Element.offsetParent节点的水平位移，Element.offsetTop返回垂直位移，单位为像素


每个元素节点都有style用来读写该元素的行内样式信息
Element.children属性返回一个类似数组的对象（HTMLCollection实例），包括当前元素节点的所有子元素

Element.firstElementChild属性返回当前元素的第一个元素子节点，Element.lastElementChild返回最后一个元素子节点

Element.nextElementSibling属性返回当前元素节点的后一个同级元素节点

实例方法

属性相关方法
getAttribute()：读取某个属性的值
getAttributeNames()：返回当前元素的所有属性名
setAttribute()：写入属性值
hasAttribute()：某个属性是否存在
hasAttributes()：当前元素是否有属性
removeAttribute()：删除属性

Element.querySelector方法接受 CSS 选择器作为参数，返回父元素的第一个匹配的子元素

Element.querySelectorAll方法接受 CSS 选择器作为参数，返回一个NodeList实例，包含所有匹配的子元素

Element.getElementsByClassName方法返回一个HTMLCollection实例，成员是当前元素节点的所有具有指定 class 的子元素节点

Element.getElementsByTagName()方法返回一个HTMLCollection实例，成员是当前节点的所有匹配指定标签名的子元素节点

Element.closest方法接受一个 CSS 选择器作为参数，返回匹配该选择器的、最接近当前节点的一个祖先节点（包括当前节点本身）

Element.matches方法返回一个布尔值，表示当前元素是否匹配给定的 CSS 选择器


事件相关方法

以下三个方法与Element节点的事件相关。这些方法都继承自EventTarget接口
Element.addEventListener()：添加事件的回调函数
Element.removeEventListener()：移除事件监听函数
Element.dispatchEvent()：触发事件

Element.scrollIntoView方法滚动当前元素，进入浏览器的可见区域

Element.getBoundingClientRect方法返回一个对象，提供当前元素节点的大小、位置等信息，基本上就是 CSS 盒状模型的所有信息

Element.getClientRects方法返回一个类似数组的对象，里面是当前元素在页面上形成的所有矩形（所以方法名中的Rect用的是复数）

Element.insertAdjacentElement方法在相对于当前元素的指定位置，插入一个新的节点

Element.insertAdjacentHTML方法用于将一个 HTML 字符串，解析生成 DOM 结构，插入相对于当前节点的指定位置

Element.remove方法继承自 ChildNode 接口，用于将当前元素节点从它的父节点移除

Element.focus方法用于将当前页面的焦点，转移到指定元素上
Element.blur方法用于将焦点从当前元素移除

Element.click方法用于在当前元素上模拟一次鼠标点击，相当于触发了click事件


属性的操作
HTML 元素包括标签名和若干个键值对，这个键值对就称为“属性”（attribute）

属性本身是一个对象（Attr对象），一般都是通过元素节点对象（HTMlElement对象）来操作属性

元素对象有一个attributes属性，返回一个类似数组的动态对象，成员是该元素标签的所有属性节点对象，属性的实时变化都会反映在这个节点对象上

HTML 元素的标准属性（即在标准中定义的属性），会自动成为元素节点对象的属性

属性操作的标准方法
getAttribute()
getAttributeNames()
setAttribute()
hasAttribute()
hasAttributes()
removeAttribute()


dataset 属性
在HTML元素上附加数据，供 JavaScript 脚本使用

使用元素节点对象的dataset属性，它指向一个对象，可以用来操作 HTML 元素标签的data-*属性
// 
<div id="mydiv" data-foo="bar"

var n = document.getElementById('mydiv');
n.dataset.foo // bar
n.dataset.foo = 'baz'

删除一个data-*属性，可以直接使用delete命令

delete document.getElementById('myDiv').dataset.foo;



Text 节点和 DocumentFragment 节点
文本节点（Text）代表元素节点（Element）和属性节点（Attribute）的文本内容。如果一个节点只包含一段文本，那么它就有一个文本子节点，代表该节点的文本内容

Text 节点的属性

data属性等同于nodeValue属性，用来设置或读取文本节点的内容

wholeText属性将当前文本节点与毗邻的文本节点，作为一个整体返回

length属性返回当前文本节点的文本长度

nextElementSibling属性返回紧跟在当前文本节点后面的那个同级元素节点。如果取不到元素节点，则返回null


Text 节点的方法
appendData()：在Text节点尾部追加字符串。
deleteData()：删除Text节点内部的子字符串，第一个参数为子字符串开始位置，第二个参数为子字符串长度。
insertData()：在Text节点插入字符串，第一个参数为插入位置，第二个参数为插入的子字符串。
replaceData()：用于替换文本，第一个参数为替换开始位置，第二个参数为需要被替换掉的长度，第三个参数为新加入的字符串。
subStringData()：用于获取子字符串，第一个参数为子字符串在Text节点中的开始位置，第二个参数为子字符串长度


remove方法用于移除当前Text节点

splitText方法将Text节点一分为二，变成两个毗邻的Text节点


DocumentFragment 节点 
DocumentFragment节点代表一个文档的片段，本身就是一个完整的 DOM 树形结构

DocumentFragment节点对象没有自己的属性和方法，全部继承自Node节点和ParentNode接口。也就是说，DocumentFragment节点比Node节点多出以下四个属性
children：返回一个动态的HTMLCollection集合对象，包括当前DocumentFragment对象的所有子元素节点。
firstElementChild：返回当前DocumentFragment对象的第一个子元素节点，如果没有则返回null。
lastElementChild：返回当前DocumentFragment对象的最后一个子元素节点，如果没有则返回null。
childElementCount：返回当前DocumentFragment对象的所有子元素数量


CSS 操作

CSS 与 JavaScript 是两个有着明确分工的领域
CSS 负责页面的视觉效果
JavaScript 负责与用户的行为互动

1. 使用网页元素节点的getAttribute()方法、setAttribute()方法和removeAttribute()方法，直接读写或删除网页元素的style属性

2. CSSStyleDeclaration 接口用来操作元素的样式。三个地方部署了这个接口
元素节点的style属性（Element.style）
CSSStyle实例的style属性
window.getComputedStyle()的返回值


CSSStyleDeclaration.cssText属性用来读写当前规则的所有样式声明文本

CSSStyleDeclaration.length属性返回一个整数值，表示当前规则包含多少条样式声明

CSSStyleDeclaration.parentRule属性返回当前规则所属的那个样式块（CSSRule 实例）

CSSStyleDeclaration.getPropertyPriority方法接受 CSS 样式的属性名作为参数，返回一个字符串，表示有没有设置important优先级

CSSStyleDeclaration.getPropertyValue方法接受 CSS 样式属性名作为参数，返回一个字符串，表示该属性的属性值

CSSStyleDeclaration.item方法接受一个整数值作为参数，返回该位置的 CSS 属性名

CSSStyleDeclaration.removeProperty方法接受一个属性名作为参数，在 CSS 规则里面移除这个属性，返回这个属性原来的值

CSSStyleDeclaration.setProperty方法用来设置新的 CSS 属性


CSS 模块的侦测
...


CSS 对象
浏览器原生提供 CSS 对象，为 JavaScript 操作 CSS 提供一些工具方法

CSS.escape方法用于转义 CSS 选择器里面的特殊字符
CSS.supports方法返回一个布尔值，表示当前环境是否支持某一句 CSS 规则

CSS 伪元素
CSS 伪元素是通过 CSS 向 DOM 添加的元素，主要是通过:before和:after选择器生成，然后用content属性指定伪元素的内容


StyleSheet接口代表网页的一张样式表，包括<link>元素加载的样式表和<style>元素内嵌的样式表


实例属性

StyleSheet.disabled返回一个布尔值，表示该样式表是否处于禁用状态

Stylesheet.href返回样式表的网址

StyleSheet.media属性返回一个类似数组的对象（MediaList实例），成员是表示适用媒介的字符串

StyleSheet.title属性返回样式表的title属性

StyleSheet.type属性返回样式表的type属性，通常是text/css

CSS 的@import命令允许在样式表中加载其他样式表

StyleSheet.ownerNode属性返回StyleSheet对象所在的 DOM 节点，通常是<link>或<style>

CSSStyleSheet.cssRules属性指向一个类似数组的对象（CSSRuleList实例），里面每一个成员就是当前样式表的一条 CSS 规则


实例方法
CSSStyleSheet.insertRule方法用于在当前样式表的插入一个新的 CSS 规则

CSSStyleSheet.deleteRule方法用来在样式表里面移除一条规则，它的参数是该条规则在cssRules对象中的位置


CSSRuleList 接口是一个类似数组的对象，表示一组 CSS 规则，成员都是 CSSRule 实例


一条 CSS 规则包括两个部分：CSS 选择器和样式声明
JavaScript 通过 CSSRule 接口操作 CSS 规则。一般通过 CSSRuleList 接口（StyleSheet.cssRules）获取 CSSRule 实例


CSSRule 实例的属性 

CSSRule.cssText属性返回当前规则的文本

CSSRule.parentStyleSheet属性返回当前规则所在的样式表对象（StyleSheet 实例）

CSSRule.parentRule属性返回包含当前规则的父规则，如果不存在父规则（即当前规则是顶层规则），则返回null

CSSRule.type属性返回一个整数值，表示当前规则的类型


CSSStyleRule 接口
CSSStyleRule.selectorText属性返回当前规则的选择器

CSSStyleRule.style属性返回一个对象（CSSStyleDeclaration 实例），代表当前规则的样式声明，也就是选择器后面的大括号里面的部分


window.matchMedia()方法用来将 CSS 的Media Query条件语句，转换成一个 MediaQueryList 实例

MediaQueryList.media属性返回一个字符串，表示对应的 MediaQuery 条件语句

MediaQueryList.matches属性返回一个布尔值，表示当前页面是否符合指定的 MediaQuery 条件语句

MediaQueryList.onchange属性用来指定change事件的监听函数


MediaQueryList 实例有两个方法MediaQueryList.addListener()和MediaQueryList.removeListener()，用来为change事件添加或撤销监听函数



Mutation Observer API 用来监视 DOM 变动。DOM 的任何变动，比如节点的增减、属性的变动、文本内容的变动，这个 API 都可以得到通知

MutationObserver构造函数 用来 新建一个观察器实例，同时指定这个实例的回调函数
var observer = new MutationObserver(function (mutations, observer) {
  mutations.forEach(function(mutation) {
    console.log(mutation);
  });
});


MutationObserver 的实例方法

observe()方法用来启动监听
var article = document.querySelector('article');

var options = {
  'childList': true,
  'attributes':true
} ;

observer.observe(article, options);


disconnect()方法用来停止观察。调用该方法后，DOM 再发生变动，也不会触发观察器
takeRecords()方法用来清除变动记录，即不再处理未处理的变动。该方法返回变动记录的数组


DOM 每次发生变化，就会生成一条变动记录（MutationRecord 实例）。该实例包含了与变动相关的所有信息。Mutation Observer 处理的就是一个个MutationRecord实例所组成的数组

```

- 事件
```js
事件的本质是程序各个组成部分之间的一种通信方式，也是异步编程的一种实现。DOM 支持大量的事件

DOM 节点的事件操作（监听和触发），都定义在EventTarget接口，该接口主要提供三个实例方法
addEventListener()：绑定事件的监听函数
removeEventListener()：移除事件的监听函数
dispatchEvent()：触发事件

EventTarget.addEventListener()用于在当前节点或对象上（即部署了 EventTarget 接口的对象），定义一个特定事件的监听函数
target.addEventListener(type, listener[, useCapture]);

EventTarget.removeEventListener()方法用来移除addEventListener()方法添加的事件监听函数
div.addEventListener('click', listener, false);
div.removeEventListener('click', listener, false);

EventTarget.dispatchEvent()方法在当前节点上触发指定事件，从而触发监听函数的执行
para.addEventListener('click', hello, false);
var event = new Event('click');
para.dispatchEvent(event);


事件模型
浏览器的事件模型，就是通过监听函数（listener）对事件做出反应。事件发生后，浏览器监听到了这个事件，就会执行对应的监听函数

HTML 语言允许在元素的属性中，直接定义某些事件的监听代码
<body onload="doSomething()">
<div onclick="console.log('触发事件')">

元素节点对象的事件属性，同样可以指定监听函数
window.onload = doSomething;

div.onclick = function (event) {
  console.log('触发事件');
};

所有 DOM 节点实例都有addEventListener方法，用来为该节点定义事件的监听函数
window.addEventListener('load', doSomething, false);


EventTarget.addEventListener是推荐的指定监听函数的方法。它有如下优点：
同一个事件可以添加多个监听函数。
能够指定在哪个阶段（捕获阶段还是冒泡阶段）触发监听函数。
除了 DOM 节点，其他对象（比如window、XMLHttpRequest等）也有这个接口，它等于是整个 JavaScript 统一的监听函数接口。

this 的指向
监听函数内部的this指向触发事件的那个元素节点


事件的传播
一个事件发生后，会在子元素和父元素之间传播（propagation）。这种传播分成三个阶段
第一阶段：从window对象传导到目标节点（上层传到底层），称为“捕获阶段”（capture phase）。
第二阶段：在目标节点上触发，称为“目标阶段”（target phase）。
第三阶段：从目标节点传导回window对象（从底层传回上层），称为“冒泡阶段”（bubbling phase）


事件的代理
事件会在冒泡阶段向上传播到父节点，因此可以把子节点的监听函数定义在父节点上，由父节点的监听函数统一处理多个子元素的事件。这种方法叫做事件的代理（delegation）


Event 对象
事件发生以后，会产生一个事件对象，作为参数传给监听函数。浏览器原生提供一个Event对象，所有的事件都是这个对象的实例

实例属性

Event.bubbles属性返回一个布尔值，表示当前事件是否会冒泡

Event.eventPhase属性返回一个整数常量，表示事件目前所处的阶段。该属性只读

Event.cancelable属性返回一个布尔值，表示事件是否可以取消

Event.cancelBubble属性是一个布尔值，如果设为true，相当于执行Event.stopPropagation()，可以阻止事件的传播

Event.defaultPrevented属性返回一个布尔值，表示该事件是否调用过Event.preventDefault方法。该属性只读


任意事件都有两个与事件相关的节点，一个是事件的原始触发节点（Event.target），另一个是事件当前正在通过的节点（Event.currentTarget）
Event.currentTarget属性返回事件当前所在的节点，即事件当前正在通过的节点，也就是当前正在执行的监听函数所在的那个节点。随着事件的传播，这个属性的值会变
Event.target属性返回原始触发事件的那个节点，即事件最初发生的节点。这个属性不会随着事件的传播而改变


Event.type属性返回一个字符串，表示事件类型。事件的类型是在生成事件的时候指定的。该属性只读

Event.timeStamp属性返回一个毫秒时间戳，表示事件发生的时间。它是相对于网页加载成功开始计算的

Event.isTrusted属性返回一个布尔值，表示该事件是否由真实的用户行为产生。比如，用户点击链接会产生一个click事件，该事件是用户产生的；Event构造函数生成的事件，则是脚本产生的

Event.detail属性只有浏览器的 UI （用户界面）事件才具有。该属性返回一个数值，表示事件的某种信息


实例方法

Event.preventDefault方法取消浏览器对当前事件的默认行为

stopPropagation方法阻止事件在 DOM 中继续传播，防止再触发定义在别的节点上的监听函数，但是不包括在当前节点上其他的事件监听函数

Event.stopImmediatePropagation方法阻止同一个事件的其他监听函数被调用，不管监听函数定义在当前节点还是其他节点

Event.composedPath()返回一个数组，成员是事件的最底层节点和依次冒泡经过的所有上层节点



鼠标事件
鼠标事件都继承了MouseEvent接口

点击事件
click：按下鼠标（通常是按下主按钮）时触发。
dblclick：在同一个元素上双击鼠标时触发。
mousedown：按下鼠标键时触发。
mouseup：释放按下的鼠标键时触发

移动事件
mousemove：当鼠标在一个节点内部移动时触发。当鼠标持续移动时，该事件会连续触发。为了避免性能问题，建议对该事件的监听函数做一些限定，比如限定一段时间内只能运行一次。
mouseenter：鼠标进入一个节点时触发，进入子节点不会触发这个事件（详见后文）。
mouseover：鼠标进入一个节点时触发，进入子节点会再一次触发这个事件（详见后文）。
mouseout：鼠标离开一个节点时触发，离开父节点也会触发这个事件（详见后文）。
mouseleave：鼠标离开一个节点时触发，离开父节点不会触发这个事件（详见后文）


MouseEvent 接口
MouseEvent接口代表了鼠标相关的事件，单击（click）、双击（dblclick）、松开鼠标键（mouseup）、按下鼠标键（mousedown）等动作，所产生的事件对象都是MouseEvent实例。此外，滚轮事件和拖拉事件也是MouseEvent实例


MouseEvent 接口的实例属性
MouseEvent.altKey、MouseEvent.ctrlKey、MouseEvent.metaKey、MouseEvent.shiftKey这四个属性都返回一个布尔值，表示事件发生时，是否按下对应的键。它们都是只读属性

MouseEvent.button属性返回一个数值，表示事件发生时按下了鼠标的哪个键。该属性只读
MouseEvent.buttons属性返回一个三个比特位的值，表示同时按下了哪些键。它用来处理同时按下多个鼠标键的情况。该属性只读

MouseEvent.clientX属性返回鼠标位置相对于浏览器窗口左上角的水平坐标（单位像素）
MouseEvent.clientY属性返回垂直坐标。这两个属性都是只读属性


MouseEvent.movementX属性返回当前位置与上一个mousemove事件之间的水平距离（单位像素）
MouseEvent.movementY属性返回当前位置与上一个mousemove事件之间的垂直距离（单位像素）

MouseEvent.screenX属性返回鼠标位置相对于屏幕左上角的水平坐标（单位像素），MouseEvent.screenY属性返回垂直坐标。这两个属性都是只读属性

MouseEvent.offsetX属性返回鼠标位置与目标节点左侧的padding边缘的水平距离（单位像素），MouseEvent.offsetY属性返回与目标节点上方的padding边缘的垂直距离

MouseEvent.pageX属性返回鼠标位置与文档左侧边缘的距离（单位像素），MouseEvent.pageY属性返回与文档上侧边缘的距离（单位像素）

MouseEvent.relatedTarget属性返回事件的相关节点


MouseEvent.getModifierState方法返回一个布尔值，表示有没有按下特定的功能键


WheelEvent 接口继承了 MouseEvent 实例，代表鼠标滚轮事件的实例对象。目前，鼠标滚轮相关的事件只有一个wheel事件，用户滚动鼠标的滚轮，就生成这个事件的实例

WheelEvent事件实例除了具有Event和MouseEvent的实例属性和实例方法，还有一些自己的实例属性，但是没有自己的实例方法
WheelEvent.deltaX：数值，表示滚轮的水平滚动量。
WheelEvent.deltaY：数值，表示滚轮的垂直滚动量。
WheelEvent.deltaZ：数值，表示滚轮的 Z 轴滚动量。
WheelEvent.deltaMode：数值，表示上面三个属性的单位，0是像素，1是行，2是页


键盘事件

键盘事件由用户击打键盘触发，它们都继承了KeyboardEvent接口
keydown：按下键盘时触发。
keypress：按下有值的键时触发，即按下 Ctrl、Alt、Shift、Meta 这样无值的键，这个事件不会触发。对于有值的键，按下时先触发keydown事件，再触发这个事件。
keyup：松开键盘时触发该事件


KeyboardEvent接口用来描述用户与键盘的互动。这个接口继承了Event接口，并且定义了自己的实例属性和实例方法

KeyboardEvent 的实例属性
KeyboardEvent.altKey：是否按下 Alt 键
KeyboardEvent.ctrlKey：是否按下 Ctrl 键
KeyboardEvent.metaKey：是否按下 meta 键（Mac 系统是一个四瓣的小花，Windows 系统是 windows 键）
KeyboardEvent.shiftKey：是否按下 Shift 键


KeyboardEvent.code属性返回一个字符串，表示当前按下的键的字符串形式。该属性只读

KeyboardEvent.key属性返回一个字符串，表示按下的键名。该属性只读

KeyboardEvent.location属性返回一个整数，表示按下的键处在键盘的哪一个区域。它可能取以下值

KeyboardEvent.repeat返回一个布尔值，代表该键是否被按着不放，以便判断是否重复这个键，即浏览器会持续触发keydown和keypress事件，直到用户松开手为止

eyboardEvent 的实例方法
KeyboardEvent.getModifierState()方法返回一个布尔值，表示是否按下或激活指定的功能键


进度事件
进度事件用来描述资源加载的进度，主要由 AJAX 请求、<img>、<audio>、<video>、<style>、<link>等外部资源的加载触发，继承了ProgressEvent接口

主要事件：
abort：外部资源中止加载时（比如用户取消）触发。如果发生错误导致中止，不会触发该事件。
error：由于错误导致外部资源无法加载时触发。
load：外部资源加载成功时触发。
loadstart：外部资源开始加载时触发。
loadend：外部资源停止加载时触发，发生顺序排在error、abort、load等事件的后面。
progress：外部资源加载过程中不断触发。
timeout：加载超时时触发。

ProgressEvent接口主要用来描述外部资源加载的进度，比如 AJAX 加载、<img>、<video>、<style>、<link>等外部资源加载。进度相关的事件都继承了这个接口



表单事件
input事件当<input>、<select>、<textarea>的值发生变化时触发，input事件对象继承了InputEvent接口

select事件当在<input>、<textarea>里面选中文本时触发

change事件当<input>、<select>、<textarea>的值发生变化时触发。它与input事件的最大不同，就是不会连续触发，只有当全部修改完成时才会触发

用户提交表单时，如果表单元素的值不满足校验条件，就会触发invalid事件


reset事件当表单重置（所有表单成员变回默认值）时触发。

submit事件当表单数据向服务器提交时触发。注意，submit事件的发生对象是<form>元素，而不是<button>元素，因为提交的是表单，而不是按钮，这两个事件发生在表单对象<form>上，而不是发生在表单的成员上


InputEvent接口主要用来描述input事件的实例。该接口继承了Event接口，还定义了一些自己的实例属性和实例方法
InputEvent.data属性返回一个字符串，表示变动的内容
InputEvent.inputType属性返回一个字符串，表示字符串发生变更的类型
InputEvent.dataTransfer属性返回一个 DataTransfer 实例


触摸事件

浏览器的触摸 API 由三个部分组成:
Touch：一个触摸点
TouchList：多个触摸点的集合
TouchEvent：触摸引发的事件实例


Touch 接口代表单个触摸点。触摸点可能是一根手指，也可能是一根触摸笔

Touch 接口的实例属性
Touch.identifier属性返回一个整数，表示触摸点的唯一 ID

Touch.screenX属性和Touch.screenY属性，分别表示触摸点相对于屏幕左上角的横坐标和纵坐标，与页面是否滚动无关。
Touch.clientX属性和Touch.clientY属性，分别表示触摸点相对于浏览器视口左上角的横坐标和纵坐标，与页面是否滚动无关。
Touch.pageX属性和Touch.pageY属性，分别表示触摸点相对于当前页面左上角的横坐标和纵坐标，包含了页面滚动带来的位移

Touch.radiusX属性和Touch.radiusY属性，分别返回触摸点周围受到影响的椭圆范围的 X 轴半径和 Y 轴半径，单位为像素。乘以 2 就可以得到触摸范围的宽度和高度。
Touch.rotationAngle属性表示触摸区域的椭圆的旋转角度，单位为度数，在0到90度之间

Touch.force属性返回一个0到1之间的数值，表示触摸压力。0代表没有压力，1代表硬件所能识别的最大压力

Touch.target属性返回一个元素节点，代表触摸发生时所在的那个元素节点。即使触摸点已经离开了这个节点，该属性依然不变


TouchList接口表示一组触摸点的集合。它的实例是一个类似数组的对象，成员是Touch的实例对象，表示所有触摸点

TouchEvent 接口继承了 Event 接口，表示由触摸引发的事件实例，通常来自触摸屏或轨迹板

TouchEvent 的实例属性
TouchEvent.altKey：布尔值，表示触摸时是否按下了 Alt 键。
TouchEvent.ctrlKey：布尔值，表示触摸时是否按下了 Ctrl 键。
TouchEvent.shiftKey：布尔值：表示触摸时是否按下了 Shift 键。
TouchEvent.metaKey：布尔值，表示触摸时是否按下了 Meta 键 或 Windows 键


TouchEvent.changedTouches属性返回一个TouchList实例，成员是一组Touch实例对象，表示本次触摸事件的相关触摸点

TouchEvent.touches属性返回一个TouchList实例，成员是所有仍然处于活动状态（即触摸中）的触摸点。一般来说，一个手指就是一个触摸点

TouchEvent.targetTouches属性返回一个TouchList实例，成员是触摸事件的目标元素节点内部、所有仍然处于活动状态（即触摸中）的触摸点


触摸事件的种类 TouchEvent.type
touchstart：用户开始触摸时触发，它的target属性返回发生触摸的元素节点。
touchend：用户不再接触触摸屏时（或者移出屏幕边缘时）触发，它的target属性与touchstart事件一致的，就是开始触摸时所在的元素节点。它的changedTouches属性返回一个TouchList实例，包含所有不再触摸的触摸点（即Touch实例对象）。
touchmove：用户移动触摸点时触发，它的target属性与touchstart事件一致。如果触摸的半径、角度、力度发生变化，也会触发该事件。
touchcancel：触摸点取消时触发，比如在触摸区域跳出一个模态窗口（modal window）、触摸点离开了文档区域（进入浏览器菜单栏）、用户的触摸点太多，超过了支持的上限（自动取消早先的触摸点）


拖拉事件
拖拉（drag）指的是，用户在某个对象上按下鼠标键不放，拖动它到另一个位置，然后释放鼠标键，将该对象放在那里

当元素节点或选中的文本被拖拉时，就会持续触发拖拉事件
drag：拖拉过程中，在被拖拉的节点上持续触发（相隔几百毫秒）。
dragstart：用户开始拖拉时，在被拖拉的节点上触发，该事件的target属性是被拖拉的节点。通常应该在这个事件的监听函数中，指定拖拉的数据。
dragend：拖拉结束时（释放鼠标键或按下 ESC 键）在被拖拉的节点上触发，该事件的target属性是被拖拉的节点。它与dragstart事件，在同一个节点上触发。不管拖拉是否跨窗口，或者中途被取消，dragend事件总是会触发的。
dragenter：拖拉进入当前节点时，在当前节点上触发一次，该事件的target属性是当前节点。通常应该在这个事件的监听函数中，指定是否允许在当前节点放下（drop）拖拉的数据。如果当前节点没有该事件的监听函数，或者监听函数不执行任何操作，就意味着不允许在当前节点放下数据。在视觉上显示拖拉进入当前节点，也是在这个事件的监听函数中设置。
dragover：拖拉到当前节点上方时，在当前节点上持续触发（相隔几百毫秒），该事件的target属性是当前节点。该事件与dragenter事件的区别是，dragenter事件在进入该节点时触发，然后只要没有离开这个节点，dragover事件会持续触发。
dragleave：拖拉操作离开当前节点范围时，在当前节点上触发，该事件的target属性是当前节点。如果要在视觉上显示拖拉离开操作当前节点，就在这个事件的监听函数中设置。
drop：被拖拉的节点或选中的文本，释放到目标节点时，在目标节点上触发。注意，如果当前节点不允许drop，即使在该节点上方松开鼠标键，也不会触发该事件。如果用户按下 ESC 键，取消这个操作，也不会触发该事件。该事件的监听函数负责取出拖拉数据，并进行相关处理


拖拉事件都继承了DragEvent接口，这个接口又继承了MouseEvent接口和Event接口

所有拖拉事件的实例都有一个DragEvent.dataTransfer属性，用来读写需要传递的数据。这个属性的值是一个DataTransfer接口的实例


DataTransfer 的实例属性
DataTransfer.dropEffect属性用来设置放下（drop）被拖拉节点时的效果，会影响到拖拉经过相关区域时鼠标的形状

DataTransfer.effectAllowed属性设置本次拖拉中允许的效果

DataTransfer.files属性是一个 FileList 对象，包含一组本地文件，可以用来在拖拉操作中传送

DataTransfer.types属性是一个只读的数组，每个成员是一个字符串，里面是拖拉的数据格式（通常是 MIME 值）

DataTransfer.items属性返回一个类似数组的只读对象（DataTransferItemList 实例），每个成员就是本次拖拉的一个对象（DataTransferItem 实例）


DataTransfer 的实例方法

DataTransfer.setData()方法用来设置拖拉事件所带有的数据

DataTransfer.getData()方法接受一个字符串（表示数据类型）作为参数，返回事件所带的指定类型的数据（通常是用setData方法添加的数据）

DataTransfer.clearData()方法接受一个字符串（表示数据类型）作为参数，删除事件所带的指定类型的数据

拖动过程中（dragstart事件触发后），浏览器会显示一张图片跟随鼠标一起移动，表示被拖动的节点


其他常见事件
beforeunload事件在窗口、文档、各种资源将要卸载前触发。它可以用来防止用户不小心卸载资源
unload事件在窗口关闭或者document对象将要卸载时触发。它的触发顺序排在beforeunload、pagehide事件后面
load事件在页面或某个资源加载成功时触发。注意，页面或资源从浏览器缓存加载，并不会触发load事件
error事件是在页面或资源加载失败时触发。abort事件在用户取消加载时触发

session 历史事件
pageshow事件在页面加载时触发，包括第一次加载和从缓存加载两种情况
pagehide事件与pageshow事件类似，当用户通过“前进/后退”按钮，离开当前页面时触发
popstate事件在浏览器的history对象的当前记录发生显式切换时触发
hashchange事件在 URL 的 hash 部分（即#号后面的部分，包括#号）发生变化时触发。该事件一般在window对象上监听

网页状态事件
网页下载并解析完成以后，浏览器就会在document对象上触发 DOMContentLoaded 事件
readystatechange事件当 Document 对象和 XMLHttpRequest 对象的readyState属性发生变化时触发

窗口事件
scroll事件在文档或文档元素滚动时触发，主要出现在用户拖动滚动条
resize事件在改变浏览器窗口大小时触发，主要发生在window对象上面
fullscreenchange事件在进入或退出全屏状态时触发，该事件发生在document对象上面

剪贴板事件
cut：将选中的内容从文档中移除，加入剪贴板时触发。
copy：进行复制动作时触发。
paste：剪贴板内容粘贴到文档后触发

焦点事件
焦点事件发生在元素节点和document对象上面，与获得或失去焦点相关


CustomEvent 接口用于生成自定义的事件实例


GlobalEventHandlers 接口
指定事件的回调函数，推荐使用的方法是元素的addEventListener方法
除了之外，还有一种方法可以直接指定事件的回调函数， div.onclick = clickHandler;，这个接口是由GlobalEventHandlers接口提供的。它的优点是使用比较方便，缺点是只能为每个事件指定一个回调函数，并且无法指定事件触发的阶段（捕获阶段还是冒泡阶段）
...
```
- 浏览器环境
```js
JavaScript 是浏览器的内置脚本语言。
浏览器内置了 JavaScript 引擎，并且提供各种接口，让 JavaScript 脚本可以控制浏览器的各种功能。一旦网页内嵌了 JavaScript 脚本，浏览器加载网页，就会去执行脚本，从而达到操作浏览器的目的，实现网页的各种动态效果

1. 代码嵌入网页的方法
网页中嵌入 JavaScript 代码，主要有四种方法
<script>元素直接嵌入代码。
<script>标签加载外部脚本
事件属性
URL 协议

// 1 script 元素嵌入代码
// demo.html:
<script>
  var x = 1 + 5;
  console.log(x);
</script>

// 2
<script type="application/javascript">
  console.log('Hello World');
</script>

// 如果type属性的值，浏览器不认识，那么它不会执行其中的代码，但是，这个<script>节点依然存在于 DOM 之中，可以使用<script>节点的text属性读出它的内容
<script id="mydata" type="x-custom-data">
  console.log('Hello World');
</script>

document.getElementById('mydata').text
//   console.log('Hello World');

script 元素加载外部脚本
// 1
<script src="https://www.example.com/script.js"></script>

// 2
<script charset="utf-8" src="https://www.example.com/script.js"></script>

// 3 为了防止攻击者篡改外部脚本，script标签允许设置一个integrity属性，写入该外部脚本的 Hash 签名，用来验证脚本的一致性
<script src="/assets/application.js"
  integrity="sha256-TvVUHzSfftWg1rcfL6TIJ0XKEGrgLyEq6lEpcmrG9qs=">
</script>


事件属性 
网页元素的事件属性（比如onclick和onmouseover），可以写入 JavaScript 代码。当指定事件发生时，就会调用这些代码
<button id="myBtn" onclick="console.log(this.id)">点击</button>


URL 协议
URL 支持javascript:协议，即在 URL 的位置写入代码，使用这个 URL 的时候就会执行 JavaScript 代码
<a href="javascript:console.log('Hello')">点击</a>

浏览器的地址栏也可以执行javascript:协议
javascript:console.log('Hello')

// 如果 JavaScript 代码返回一个字符串，浏览器就会新建一个文档，展示这个字符串的内容，原有文档的内容都会消失
<a href="javascript: new Date().toLocaleTimeString();">点击</a>


浏览器加载 JavaScript 脚本，主要通过<script>元素完成。
正常的网页加载流程:
1.浏览器一边下载 HTML 网页，一边开始解析。也就是说，不等到下载完，就开始解析。
2.解析过程中，浏览器发现<script>元素，就暂停解析，把网页渲染的控制权转交给 JavaScript 引擎。
3.如果<script>元素引用了外部脚本，就下载该脚本再执行，否则就直接执行代码。
4.JavaScript 引擎执行完毕，控制权交还渲染引擎，恢复往下解析 HTML 网页

加载外部脚本时，浏览器会暂停页面渲染，等待脚本下载并执行完成后，再继续渲染。原因是 JavaScript 代码可以修改 DOM，所以必须把控制权让给它，否则会导致复杂的线程竞赛的问题

为了避免这种情况，较好的做法是将<script>标签都放在页面底部


defer 属性
为了解决脚本文件下载阻塞网页渲染的问题，一个方法是对<script>元素加入defer属性
<script src="a.js" defer></script>
<script src="b.js" defer></script>
// 只有等到 DOM 加载完成后，才会执行a.js和b.js

defer属性的运行流程
1.浏览器开始解析 HTML 网页。
2.解析过程中，发现带有defer属性的<script>元素。
3.浏览器继续往下解析 HTML 网页，同时并行下载<script>元素加载的外部脚本。
4.浏览器完成解析 HTML 网页，此时再回过头执行已经下载完成的脚本

对于内置而不是加载外部脚本的script标签，以及动态生成的script标签，defer属性不起作用。另外，使用defer加载的外部脚本不应该使用document.write方法


async 属性
解决“阻塞效应”的另一个方法是对<script>元素加入async属性

<script src="a.js" async></script>
<script src="b.js" async></script>
// async属性的作用是，使用另一个进程下载脚本，下载时不会阻塞渲染

async 属性的运行流程
1.浏览器开始解析 HTML 网页。
2.解析过程中，发现带有async属性的script标签。
3.浏览器继续往下解析 HTML 网页，同时并行下载<script>标签中的外部脚本。
4.脚本下载完成，浏览器暂停解析 HTML 网页，开始执行下载的脚本。
5.脚本执行完毕，浏览器恢复解析 HTML 网页

async属性可以保证脚本下载的同时，浏览器继续渲染，一旦采用这个属性，就无法保证脚本的执行顺序

一般来说，如果脚本之间没有依赖关系，就使用async属性，如果脚本之间有依赖关系，就使用defer属性。如果同时使用async和defer属性，后者不起作用，浏览器行为由async属性决定


脚本的动态加载
script>元素还可以动态生成，生成后再插入页面，从而实现脚本的动态加载

// 设置async属性为false 保证脚本的执行顺序
['a.js', 'b.js'].forEach(function(src) {
  var script = document.createElement('script');
  script.src = src;
  script.async = false;
  document.head.appendChild(script);
});
// 代码不会阻塞页面渲染，而且可以保证b.js在a.js后面执行, 需要注意的是，在这段代码后面加载的脚本文件，会因此都等待b.js执行完成后再执行。

// 为动态加载的脚本指定回调函数
function loadScript(src, done) {
  var js = document.createElement('script');
  js.src = src;
  js.onload = function() {
    done();
  };
  js.onerror = function() {
    done(new Error('Failed to load script ' + src));
  };
  document.head.appendChild(js);
}


加载使用的协议
如果不指定协议，浏览器默认采用 HTTP 协议下载

// 默认采用 HTTP 协议下载
<script src="example.js"></script>

// 采用 HTTPS 协议下载
<script src="https://example.js"></script>

// 根据页面本身的协议来决定加载协议
<script src="//example.js"></script>


浏览器的组成
浏览器的核心是两部分：渲染引擎和 JavaScript 解释器（又称 JavaScript 引擎）

渲染引擎的主要作用是，将网页代码渲染为用户视觉可以感知的平面文档

不同的浏览器有不同的渲染引擎
Firefox：Gecko 引擎
Safari：WebKit 引擎
Chrome：Blink 引擎
IE: Trident 引擎
Edge: EdgeHTML 引擎


渲染引擎处理网页的四个阶段
1.解析代码：HTML 代码解析为 DOM，CSS 代码解析为 CSSOM（CSS Object Model）。
2.对象合成：将 DOM 和 CSSOM 合成一棵渲染树（render tree）。
3.布局：计算出渲染树的布局（layout）。
4.绘制：将渲染树绘制到屏幕

以上四步并非严格按顺序执行



JavaScript 引擎的主要作用是，读取网页中的 JavaScript 代码，对其处理后运行
为了提高运行速度，目前的浏览器都将 JavaScript 进行一定程度的编译，生成类似字节码（bytecode）的中间代码，以提高运行速度，字节码不能直接运行，而是运行在一个虚拟机（Virtual Machine）之上，一般也把虚拟机称为 JavaScript 引擎

常见的一些 JavaScript 虚拟机：
Chakra (Microsoft Internet Explorer)
Nitro/JavaScript Core (Safari)
Carakan (Opera)
SpiderMonkey (Firefox)
V8 (Chrome, Chromium)


浏览器里面，window对象（注意，w为小写）指当前的浏览器窗口。它也是当前页面的顶层对象，即最高一层的对象，所有其他对象都是它的下属。一个变量如果未声明，那么默认就是顶层对象的属性
a = 1;
window.a // 1

window 对象的属性
window.name属性是一个字符串，表示当前浏览器窗口的名字
window.closed属性返回一个布尔值，表示窗口是否关闭
window.opener属性表示打开当前窗口的父窗口
window.self和window.window属性都指向窗口本身。这两个属性只读
window.frames属性返回一个类似数组的对象，成员为页面内所有框架窗口，包括frame元素和iframe元素。window.frames[0]表示页面中第一个框架窗口
window.length属性返回当前网页包含的框架总数

window.frameElement属性主要用于当前窗口嵌在另一个网页的情况（嵌入<object>、<iframe>或<embed>元素），返回当前窗口所在的那个元素节点

window.top属性指向最顶层窗口，主要用于在框架窗口（frame）里面获取顶层窗口
window.parent属性指向父窗口。如果当前窗口没有父窗口，window.parent指向自身

window.status属性用于读写浏览器状态栏的文本

window.devicePixelRatio属性返回一个数值，表示一个 CSS 像素的大小与一个物理像素的大小之间的比率。也就是说，它表示一个 CSS 像素由多少个物理像素组成

位置大小属性
window.screenX和window.screenY属性，返回浏览器窗口左上角相对于当前屏幕左上角的水平距离和垂直距离（单位像素）
window.innerHeight和window.innerWidth属性，返回网页在当前窗口中可见部分的高度和宽度，即“视口”（viewport）的大小（单位像素）
window.outerHeight和window.outerWidth属性返回浏览器窗口的高度和宽度，包括浏览器菜单和边框（单位像素）
window.scrollX属性返回页面的水平滚动距离，window.scrollY属性返回页面的垂直滚动距离，单位都为像素
window.pageXOffset属性和window.pageYOffset属性，是window.scrollX和window.scrollY别名

组件属性
组件属性返回浏览器的组件对象
window.locationbar：地址栏对象
window.menubar：菜单栏对象
window.scrollbars：窗口的滚动条对象
window.toolbar：工具栏对象
window.statusbar：状态栏对象
window.personalbar：用户安装的个人工具栏对象

这些对象的visible属性是一个布尔值，表示这些组件是否可见


全局对象属性
window.document：指向document对象
window.location：指向Location对象，用于获取当前窗口的 URL 信息
window.navigator：指向Navigator对象，用于获取环境信息
window.history：指向History对象，表示浏览器的浏览历史
window.localStorage：指向本地储存的 localStorage 数据
window.sessionStorage：指向本地储存的 sessionStorage 数据
window.console：指向console对象，用于操作控制台
window.screen：指向Screen对象，表示屏幕信息
window.isSecureContext属性返回一个布尔值，表示当前窗口是否处在加密环境

window 对象的方法
window.alert()、window.prompt()、window.confirm()都是浏览器与用户互动的全局方法

window.alert()方法弹出的对话框，只有一个“确定”按钮，往往用来通知用户某些信息
window.prompt()方法弹出的对话框，提示文字的下方，还有一个输入框，要求用户输入信息，并有“确定”和“取消”两个按钮。它往往用来获取用户输入的数据
window.confirm()方法弹出的对话框，除了提示信息之外，只有“确定”和“取消”两个按钮，往往用来征询用户是否同意

window.open方法用于新建另一个浏览器窗口，类似于浏览器菜单的新建窗口选项
window.close方法用于关闭当前窗口
window.stop()方法完全等同于单击浏览器的停止按钮，会停止加载图像、视频等正在或等待加载的对象

window.moveTo()方法用于移动浏览器窗口到指定位置
window.moveBy()方法将窗口移动到一个相对位置

window.resizeTo()方法用于缩放窗口到指定大小
window.resizeBy()方法用于缩放窗口

window.scrollTo方法用于将文档滚动到指定位置
window.scroll()方法是window.scrollTo()方法的别名
window.scrollBy()方法用于将网页滚动指定距离（单位像素）


window.print方法会跳出打印对话框，与用户点击菜单里面的“打印”命令效果相同
window.focus()方法会激活窗口，使其获得焦点，出现在其他窗口的前面
window.getSelection方法返回一个Selection对象，表示用户现在选中的文本

window.getComputedStyle()方法接受一个元素节点作为参数，返回一个包含该元素的最终样式信息的对象
window.matchMedia()方法用来检查 CSS 的mediaQuery语句

window.requestAnimationFrame()推迟到浏览器下一次重流时执行，执行完才会进行下一次重绘
window.requestIdleCallback()跟setTimeout类似，也是将某个函数推迟执行，但是它保证将回调函数推迟到系统资源空闲时执行

事件
window对象可以接收以下事件
load事件发生在文档在浏览器窗口加载完毕时。window.onload属性可以指定这个事件的回调函数
浏览器脚本发生错误时，会触发window对象的error事件

window 对象的事件监听属性
window.onafterprint：afterprint事件的监听函数。
window.onbeforeprint：beforeprint事件的监听函数。
window.onbeforeunload：beforeunload事件的监听函数。
window.onhashchange：hashchange事件的监听函数。
window.onlanguagechange: languagechange的监听函数。
window.onmessage：message事件的监听函数。
window.onmessageerror：MessageError事件的监听函数。
window.onoffline：offline事件的监听函数。
window.ononline：online事件的监听函数。
window.onpagehide：pagehide事件的监听函数。
window.onpageshow：pageshow事件的监听函数。
window.onpopstate：popstate事件的监听函数。
window.onstorage：storage事件的监听函数。
window.onunhandledrejection：未处理的 Promise 对象的reject事件的监听函数。
window.onunload：unload事件的监听函数


多窗口操作
网页可以使用iframe元素，嵌入其他网页，因此一个网页之中会形成多个窗口

各个窗口之中的脚本，可以引用其他窗口
top：顶层窗口，即最上层的那个窗口
parent：父窗口
self：当前窗口，即自身


对于iframe嵌入的窗口，document.getElementById方法可以拿到该窗口的 DOM 节点，然后使用contentWindow属性获得iframe节点包含的window对象

window.frames属性返回一个类似数组的对象，成员是所有子窗口的window对象。可以使用这个属性，实现窗口之间的互相引用


window.navigator属性指向一个包含浏览器和系统信息的 Navigator 对象。脚本通过这个属性了解用户的环境信息
navigator.userAgent属性返回浏览器的 User Agent 字符串，表示用户设备信息，包含了浏览器的厂商、版本、操作系统等信息

Navigator.plugins属性返回一个类似数组的对象，成员是 Plugin 实例对象，表示浏览器安装的插件

Navigator.platform属性返回用户的操作系统信息，比如MacIntel、Win32、Linux x86_64等

navigator.onLine属性返回一个布尔值，表示用户当前在线还是离线（浏览器断线）

Navigator.language属性返回一个字符串，表示浏览器的首选语言。该属性只读
Navigator.languages属性返回一个数组，表示用户可以接受的语言

Navigator.geolocation属性返回一个 Geolocation 对象，包含用户地理位置的信息。注意，该 API 只有在 HTTPS 协议下可用，否则调用下面方法时会报错

navigator.cookieEnabled属性返回一个布尔值，表示浏览器的 Cookie 功能是否打开


Navigator 对象的方法
navigator.javaEnabled()方法返回一个布尔值，表示浏览器是否能运行 Java Applet 小程序
Navigator.sendBeacon()方法用于向服务器异步发送数据，

Navigator 的实验性属性
......


Screen 对象
Screen 对象表示当前窗口所在的屏幕，提供显示设备的信息。window.screen属性指向这个对象
Screen.height：浏览器窗口所在的屏幕的高度（单位像素）。除非调整显示器的分辨率，否则这个值可以看作常量，不会发生变化。显示器的分辨率与浏览器设置无关，缩放网页并不会改变分辨率。
Screen.width：浏览器窗口所在的屏幕的宽度（单位像素）。
Screen.availHeight：浏览器窗口可用的屏幕高度（单位像素）。因为部分空间可能不可用，比如系统的任务栏或者 Mac 系统屏幕底部的 Dock 区，这个属性等于height减去那些被系统组件的高度。
Screen.availWidth：浏览器窗口可用的屏幕宽度（单位像素）。
Screen.pixelDepth：整数，表示屏幕的色彩位数，比如24表示屏幕提供24位色彩。
Screen.colorDepth：Screen.pixelDepth的别名。严格地说，colorDepth 表示应用程序的颜色深度，pixelDepth 表示屏幕的颜色深度，绝大多数情况下，它们都是同一件事。
Screen.orientation：返回一个对象，表示屏幕的方向。该对象的type属性是一个字符串，表示屏幕的具体方向，landscape-primary表示横放，landscape-secondary表示颠倒的横放，portrait-primary表示竖放，portrait-secondary表示颠倒的竖放



Cookie 是服务器保存在浏览器的一小段文本信息，一般大小不能超过4KB。浏览器每次向服务器发出请求，就会自动附上这段信息

Cookie 的目的就是区分用户，以及放置状态信息，它的使用场景主要如下
对话（session）管理：保存登录状态、购物车等需要记录的信息。
个性化信息：保存用户的偏好，比如网页的字体大小、背景色等等。
追踪用户：记录和分析用户行为


每个 Cookie 都有以下几方面的元数据
Cookie 的名字
Cookie 的值（真正的数据写在这里面）
到期时间（超过这个时间会失效）
所属域名（默认为当前域名）
生效的路径（默认为当前网址）


Cookie 由 HTTP 协议生成，也主要是供 HTTP 协议使用
......


AJAX 是 Asynchronous JavaScript and XML 的缩写，指的是通过 JavaScript 的异步通信，从服务器获取 XML 文档从中提取数据，再更新当前网页的对应部分，而不用刷新整个网页

后来，AJAX 这个词就成为 JavaScript 脚本发起 HTTP 通信的代名词，也就是说，只要用脚本发起通信，就可以叫做 AJAX 通信

AJAX 包括以下几个步骤
1.创建 XMLHttpRequest 实例
2.发出 HTTP 请求
3.接收服务器传回的数据
4.更新网页数据

简单来说，AJAX 通过原生的XMLHttpRequest对象发出 HTTP 请求，得到服务器返回的数据后，再进行处理
现在，服务器返回的都是 JSON 格式的数据，XML 格式已经过时了，但是 AJAX 这个名字已经成了一个通用名词，字面含义已经消失了

XMLHttpRequest对象是 AJAX 的主要接口，用于浏览器与服务器之间的通信，可以支持多种协议（比如file或ftp），发送任何格式的数据（包括字符串和二进制）
//
var xhr = new XMLHttpRequest();   // 生成实例
xhr.open('GET', 'http://www.example.com/page.php', true);   // 使用open()方法指定建立 HTTP 连接

xhr.onreadystatechange = handleStateChange;   // 指定回调函数，监听通信状态（readyState属性）的变化

function handleStateChange() {
  // ...
}

xhr.send(null);   使用send()方法，实际发出请求

一旦拿到服务器返回的数据，AJAX 不会刷新整个网页，而是只更新网页里面的相关部分
AJAX 只能向同源网址（协议、域名、端口都相同）发出 HTTP 请求，如果发出跨域请求，就会报错

// 完整例子
var xhr = new XMLHttpRequest();

xhr.onreadystatechange = function(){
  // 通信成功时，状态值为4
  if (xhr.readyState === 4){
    if (xhr.status === 200){
      console.log(xhr.responseText);
    } else {
      console.error(xhr.statusText);
    }
  }
};

xhr.onerror = function (e) {
  console.error(xhr.statusText);
};

xhr.open('GET', '/endpoint', true);
xhr.send(null)


XMLHttpRequest 的实例属性
XMLHttpRequest.readyState返回一个整数，表示实例对象的当前状态
0，表示 XMLHttpRequest 实例已经生成，但是实例的open()方法还没有被调用。
1，表示open()方法已经调用，但是实例的send()方法还没有调用，仍然可以使用实例的setRequestHeader()方法，设定 HTTP 请求的头信息。
2，表示实例的send()方法已经调用，并且服务器返回的头信息和状态码已经收到。
3，表示正在接收服务器传来的数据体（body 部分）。这时，如果实例的responseType属性等于text或者空字符串，responseText属性就会包含已经收到的部分信息。
4，表示服务器返回的数据已经完全接收，或者本次接收已经失败


XMLHttpRequest.onreadystatechange属性指向一个监听函数。readystatechange事件发生时（实例的readyState属性变化），就会执行这个属性

XMLHttpRequest.response属性表示服务器返回的数据体（即 HTTP 回应的 body 部分）。它可能是任何数据类型，比如字符串、对象、二进制对象等等

XMLHttpRequest.responseType属性是一个字符串，表示服务器返回数据的类型
XMLHttpRequest.responseType属性可以等于以下值
""（空字符串）：等同于text，表示服务器返回文本数据。
"arraybuffer"：ArrayBuffer 对象，表示服务器返回二进制数组。
"blob"：Blob 对象，表示服务器返回二进制对象。
"document"：Document 对象，表示服务器返回一个文档对象。
"json"：JSON 对象。
"text"：字符串

XMLHttpRequest.responseText属性返回从服务器接收到的字符串，该属性为只读。只有 HTTP 请求完成接收以后，该属性才会包含完整的数据

XMLHttpRequest.responseXML属性返回从服务器接收到的 HTML 或 XML 文档对象，该属性为只读

XMLHttpRequest.responseURL属性是字符串，表示发送数据的服务器的网址

XMLHttpRequest.status属性返回一个整数，表示服务器回应的 HTTP 状态码

XMLHttpRequest.timeout属性返回一个整数，表示多少毫秒后，如果请求仍然没有得到结果，就会自动终止
XMLHttpRequestEventTarget.ontimeout属性用于设置一个监听函数，如果发生 timeout 事件，就会执行这个监听函数

事件监听属性
XMLHttpRequest 对象可以对以下事件指定监听函数
XMLHttpRequest.onloadstart：loadstart 事件（HTTP 请求发出）的监听函数
XMLHttpRequest.onprogress：progress事件（正在发送和加载数据）的监听函数
XMLHttpRequest.onabort：abort 事件（请求中止，比如用户调用了abort()方法）的监听函数
XMLHttpRequest.onerror：error 事件（请求失败）的监听函数
XMLHttpRequest.onload：load 事件（请求成功完成）的监听函数
XMLHttpRequest.ontimeout：timeout 事件（用户指定的时限超过了，请求还未完成）的监听函数
XMLHttpRequest.onloadend：loadend 事件（请求完成，不管成功或失败）的监听函数


XMLHttpRequest.withCredentials属性是一个布尔值，表示跨域请求时，用户信息（比如 Cookie 和认证的 HTTP 头信息）是否会包含在请求之中，默认为false

XMLHttpRequest 不仅可以发送请求，还可以发送文件，这就是 AJAX 文件上传。发送文件以后，通过XMLHttpRequest.upload属性可以得到一个对象，通过观察这个对象，可以得知上传的进展


XMLHttpRequest 的实例方法
XMLHttpRequest.open()方法用于指定 HTTP 请求的参数，或者说初始化 XMLHttpRequest 实例对象
XMLHttpRequest.send()方法用于实际发出 HTTP 请求。它的参数是可选的，如果不带参数，就表示 HTTP 请求只有一个 URL，没有数据体，典型例子就是 GET 请求；如果带有参数，就表示除了头信息，还带有包含具体数据的信息体，典型例子就是 POST 请求
XMLHttpRequest.setRequestHeader()方法用于设置浏览器发送的 HTTP 请求的头信息。该方法必须在open()之后、send()之前调用
XMLHttpRequest.overrideMimeType()方法用来指定 MIME 类型，覆盖服务器返回的真正的 MIME 类型，从而让浏览器进行不一样的处理
XMLHttpRequest.getResponseHeader()方法返回 HTTP 头信息指定字段的值，如果还没有收到服务器回应或者指定字段不存在，返回null
XMLHttpRequest.getAllResponseHeaders()方法返回一个字符串，表示服务器发来的所有 HTTP 头信息
XMLHttpRequest.abort()方法用来终止已经发出的 HTTP 请求。调用这个方法以后，readyState属性变为4，status属性变为0。


XMLHttpRequest 实例的事件

readyState属性的值发生改变，就会触发 readyStateChange 事件
上传文件时，XMLHttpRequest 实例对象本身和实例的upload属性，都有一个progress事件，会不断返回上传的进度
load 事件表示服务器传来的数据接收完毕，error 事件表示请求出错，abort 事件表示请求被中断（比如用户取消请求）
abort、load和error这三个事件，会伴随一个loadend事件，表示请求结束，但不知道其是否成功
服务器超过指定时间还没有返回结果，就会触发 timeout 事件


同源限制
同源政策的目的，是为了保证用户信息的安全，防止恶意的网站窃取数据
同源政策是必需的，否则 Cookie 可以共享，互联网就毫无安全可言了

如果非同源，共有三种行为受到限制
（1） 无法读取非同源网页的 Cookie、LocalStorage 和 IndexedDB。
（2） 无法接触非同源网页的 DOM。
（3） 无法向非同源地址发送 AJAX 请求（可以发送，但浏览器会拒绝接受响应）

Cookie 是服务器写入浏览器的一小段信息，只有同源的网页才能共享

iframe元素可以在当前网页之中，嵌入其他网页。每个iframe元素形成自己的窗口，即有自己的window对象。iframe窗口之中的脚本，可以获得父窗口和子窗口。但是，只有在同源的情况下，父窗口和子窗口才能通信；如果跨域，就无法拿到对方的 DOM

片段标识符（fragment identifier）指的是，URL 的#号后面的部分，比如http://example.com/x.html#fragment的#fragment。如果只是改变片段标识符，页面不会重新刷新


同源政策规定，AJAX 请求只能发给同源的网址，否则就报错

WebSocket 是一种通信协议，使用ws://（非加密）和wss://（加密）作为协议前缀。该协议不实行同源政策，只要服务器支持，就可以通过它进行跨源通信

CORS 是跨源资源分享（Cross-Origin Resource Sharing）的缩写。它是 W3C 标准，属于跨源 AJAX 请求的根本解决方法。相比 JSONP 只能发GET请求，CORS 允许任何类型的请求


CORS 通信
CORS 是一个 W3C 标准，全称是“跨源资源共享”（Cross-origin resource sharing），它允许浏览器向跨源的服务器，发出XMLHttpRequest请求，从而克服了 AJAX 只能同源使用的限制

CORS 需要浏览器和服务器同时支持。目前，所有浏览器都支持该功能
整个 CORS 通信过程，都是浏览器自动完成，不需要用户参与
实现 CORS 通信的关键是服务器。只要服务器实现了 CORS 接口，就可以跨源通信

CORS 请求分成两类：简单请求（simple request）和非简单请求（not-so-simple request）

只要同时满足以下两大条件，就属于简单请求
1. 请求方法是以下三种方法之一
HEAD
GET
POST

2.HTTP 的头信息不超出以下几种字段
Accept
Accept-Language
Content-Language
Last-Event-ID
Content-Type：只限于三个值application/x-www-form-urlencoded、multipart/form-data、text/plain

凡是不同时满足上面两个条件，就属于非简单请求。一句话，简单请求就是简单的 HTTP 方法与简单的 HTTP 头信息的结合

简单请求
对于简单请求，浏览器直接发出 CORS 请求。具体来说，就是在头信息之中，增加一个Origin字段


CORS 请求默认不包含 Cookie 信息（以及 HTTP 认证信息等），这是为了降低 CSRF 攻击的风险。但是某些场合，服务器可能需要拿到 Cookie，这时需要服务器显式指定Access-Control-Allow-Credentials字段，告诉浏览器可以发送 Cookie
Access-Control-Allow-Credentials: true
同时，开发者必须在 AJAX 请求中打开withCredentials属性
var xhr = new XMLHttpRequest();
xhr.withCredentials = true;


非简单请求
非简单请求是那种对服务器提出特殊要求的请求，比如请求方法是PUT或DELETE，或者Content-Type字段的类型是application/json
非简单请求的 CORS 请求，会在正式通信之前，增加一次 HTTP 查询请求，称为“预检”请求（preflight）。浏览器先询问服务器，当前网页所在的域名是否在服务器的许可名单之中，以及可以使用哪些 HTTP 方法和头信息字段。只有得到肯定答复，浏览器才会发出正式的XMLHttpRequest请求，否则就报错


Storage 接口用于脚本在浏览器保存数据
两个对象部署了这个接口： window.sessionStorage和window.localStorage
sessionStorage保存的数据用于浏览器的一次会话（session），当会话结束（通常是窗口关闭），数据被清空
localStorage保存的数据长期存在，下一次访问该网站的时候，网页可以直接读取以前保存的数据

Storage 接口只有一个属性
Storage.length：返回保存的数据项个数

5个方法
Storage.setItem()方法用于存入数据
Storage.getItem()方法用于读取数据
Storage.removeItem()方法用于清除某个键名对应的键值
Storage.clear()方法用于清除所有保存的数据
Storage.key()方法接受一个整数作为参数（从零开始），返回该位置对应的键名


Storage 接口储存的数据发生变化时，会触发 storage 事件，可以指定这个事件的监听函数
window.addEventListener('storage', onStorageChange);


window.history属性指向 History 对象，它表示当前窗口的浏览历史

History 对象主要有两个属性
History.length：当前窗口访问过的网址数量（包括当前网页）
History.state：History 堆栈最上层的状态值

方法
History.back()：移动到上一个网址，等同于点击浏览器的后退键。对于第一个访问的网址，该方法无效果。
History.forward()：移动到下一个网址，等同于点击浏览器的前进键。对于最后一个访问的网址，该方法无效果。
History.go()：接受一个整数作为参数，以当前网址为基准，移动到参数指定的网址，比如go(1)相当于forward()，go(-1)相当于back()。如果参数超过实际存在的网址范围，该方法无效果；如果不指定参数，默认参数为0，相当于刷新当前页面

History.pushState()方法用于在历史中添加一条记录
History.replaceState()方法用来修改 History 对象的当前记录，其他都与pushState()方法一模一样
每当同一个文档的浏览历史（即history对象）出现变化时，就会触发popstate事件



Location对象是浏览器提供的原生对象，提供 URL 相关的信息和操作方法

Location对象提供以下属性
Location.href：整个 URL。
Location.protocol：当前 URL 的协议，包括冒号（:）。
Location.host：主机。如果端口不是协议默认的80和433，则还会包括冒号（:）和端口。
Location.hostname：主机名，不包括端口。
Location.port：端口号。
Location.pathname：URL 的路径部分，从根路径/开始。
Location.search：查询字符串部分，从问号?开始。
Location.hash：片段字符串部分，从#开始。
Location.username：域名前面的用户名。
Location.password：域名前面的密码。
Location.origin：URL 的协议、主机名和端口


方法
assign方法接受一个 URL 字符串作为参数，使得浏览器立刻跳转到新的 URL
replace方法接受一个 URL 字符串作为参数，使得浏览器立刻跳转到新的 URL
reload方法使得浏览器重新加载当前网址，相当于按下浏览器的刷新按钮
toString方法返回整个 URL 字符串，相当于读取Location.href属性


URL 的编码和解码

网页的 URL 只能包含合法的字符。合法字符分成两类
1.URL 元字符：分号（;），逗号（,），斜杠（/），问号（?），冒号（:），at（@），&，等号（=），加号（+），美元符号（$），井号（#）
2.语义字符：a-z，A-Z，0-9，连词号（-），下划线（_），点（.），感叹号（!），波浪线（~），星号（*），单引号（'），圆括号（()）

除了以上字符，其他字符出现在 URL 之中都必须转义，规则是根据操作系统的默认编码，将每个字节转为百分号（%）加上两个大写的十六进制字母

JavaScript 提供四个 URL 的编码/解码方法
encodeURI()
encodeURIComponent()
decodeURI()
decodeURIComponent()

encodeURI()方法用于转码整个 URL。它的参数是一个字符串，代表整个 URL。它会将元字符和语义字符之外的字符，都进行转义
encodeURI('http://www.example.com/q=春节')
// "http://www.example.com/q=%E6%98%A5%E8%8A%82"

encodeURIComponent()方法用于转码 URL 的组成部分，会转码除了语义字符之外的所有字符，即元字符也会被转码
encodeURIComponent('春节')
// "%E6%98%A5%E8%8A%82"
encodeURIComponent('http://www.example.com/q=春节')
// "http%3A%2F%2Fwww.example.com%2Fq%3D%E6%98%A5%E8%8A%82"

decodeURI()方法用于整个 URL 的解码。它是encodeURI()方法的逆运算
decodeURI('http://www.example.com/q=%E6%98%A5%E8%8A%82')
// "http://www.example.com/q=春节"

decodeURIComponent()用于URL 片段的解码。它是encodeURIComponent()方法的逆运算
decodeURIComponent('%E6%98%A5%E8%8A%82')
// "春节"


URL 接口
浏览器原生提供URL()接口，它是一个构造函数，用来构造、解析和编码 URL
// 1
var url = new URL('http://www.example.com/index.html');
url.href
// "http://www.example.com/index.html"

// 2
var url1 = new URL('index.html', 'http://example.com');
url1.href
// "http://example.com/index.html"

var url2 = new URL('page2.html', 'http://example.com/page1.html');
url2.href
// "http://example.com/page2.html"

var url3 = new URL('..', 'http://example.com/a/b.html')
url3.href
// "http://example.com/"


URL 实例的属性与Location对象的属性基本一致，返回当前 URL 的信息


实例属性
URL.href：返回整个 URL
URL.protocol：返回协议，以冒号:结尾
URL.hostname：返回域名
URL.host：返回域名与端口，包含:号，默认的80和443端口会省略
URL.port：返回端口
URL.origin：返回协议、域名和端口
URL.pathname：返回路径，以斜杠/开头
URL.search：返回查询字符串，以问号?开头
URL.searchParams：返回一个URLSearchParams实例，该属性是Location对象没有的
URL.hash：返回片段识别符，以井号#开头
URL.password：返回域名前面的密码
URL.username：返回域名前面的用户名

// var url = new URL('http://user:passwd@www.example.com:4097/path/a.html?x=111#part1');

url.href
// "http://user:passwd@www.example.com:4097/path/a.html?x=111#part1"
url.protocol
// "http:"
url.hostname
// "www.example.com"
url.host
// "www.example.com:4097"
url.port
// "4097"
url.origin
// "http://www.example.com:4097"
url.pathname
// "/path/a.html"
url.search
// "?x=111"
url.searchParams
// URLSearchParams {}
url.hash
// "#part1"
url.password
// "passwd"
url.username
// "user"


静态方法
URL.createObjectURL()方法用来为上传/下载的文件、流媒体文件生成一个 URL 字符串
URL.revokeObjectURL()方法用来释放URL.createObjectURL()方法生成的 URL 实例


URLSearchParams对象是浏览器的原生对象，用来构造、解析和处理 URL 的查询字符串（即 URL 问号后面的部分
// 方法一：传入字符串
var params = new URLSearchParams('?foo=1&bar=2');
// 等同于
var params = new URLSearchParams(document.location.search);

// 方法二：传入数组
var params = new URLSearchParams([['foo', 1], ['bar', 2]]);

// 方法三：传入对象
var params = new URLSearchParams({'foo' : 1 , 'bar' : 2});

// URLSearchParams会对查询字符串自动编码
var params = new URLSearchParams({'foo': '你好'});
params.toString() // "foo=%E4%BD%A0%E5%A5%BD"


toString方法返回实例的字符串形式
// 1
var url = new URL('https://example.com?foo=1&bar=2');
var params = new URLSearchParams(url.search);

params.toString() // "foo=1&bar=2'

// 2
var params = new URLSearchParams({version: 2.0});
window.location.href = location.pathname + '?' + params;


append()方法用来追加一个查询参数。它接受两个参数，第一个为键名，第二个为键值
delete()方法用来删除指定的查询参数。它接受键名作为参数
has()方法返回一个布尔值，表示查询字符串是否包含指定的键名
set()方法用来设置查询字符串的键值
get()方法用来读取查询字符串里面的指定键。它接受键名作为参数
getAll()方法返回一个数组，成员是指定键的所有键值

sort()方法对查询字符串里面的键进行排序，规则是按照 Unicode 码点从小到大排列

keys方法返回的是键名的遍历器，values方法返回的是键值的遍历器，entries返回的是键值对的遍历器


ArrayBuffer 对象表示一段二进制数据，用来模拟内存里面的数据。通过这个对象，JavaScript 可以读写二进制数据。这个对象可以看作内存数据的表达

Blob 对象表示一个二进制文件的数据内容，比如一个图片文件的内容就可以通过 Blob 对象读写。它通常用来读写文件，它的名字是 Binary Large Object （二进制大型对象）的缩写。它与 ArrayBuffer 的区别在于，它用于操作二进制文件，而 ArrayBuffer 用于操作内存


实例属性和实例方法
Blob具有两个实例属性size和type，分别返回数据的大小和类型
Blob具有一个实例方法slice，用来拷贝原来的数据，返回的也是一个Blob实例


获取文件信息
文件选择器<input type="file">用来让用户选取文件

AJAX 请求时，如果指定responseType属性为blob，下载下来的就是一个 Blob 对象

浏览器允许使用URL.createObjectURL()方法，针对 Blob 对象生成一个临时 URL，以便于某些 API 使用

取得 Blob 对象以后，可以通过FileReader对象，读取 Blob 对象的内容，即文件内容


File 对象代表一个文件，用来读写文件信息。它继承了 Blob 对象，或者说是一种特殊的 Blob 对象，所有可以使用 Blob 对象的场合都可以使用它


实例属性和实例方法

File 对象的实例属性
File.lastModified：最后修改时间
File.name：文件名或文件路径
File.size：文件大小（单位字节）
File.type：文件的 MIME 类型

// 1
var myFile = new File([], 'file.bin', {
  lastModified: new Date(2018, 1, 1),
});
myFile.lastModified // 1517414400000
myFile.name // "file.bin"
myFile.size // 0
myFile.type // ""


FileList对象是一个类似数组的对象，代表一组选中的文件，每个成员都是一个 File 实例

FileReader 对象用于读取 File 对象或 Blob 对象所包含的文件内容


表单（<form>）用来收集用户提交的数据，发送到服务器


FormData 对象

表单数据以键值对的形式向服务器发送，这个过程是浏览器自动完成的，有时候，我们希望通过脚本完成这个过程，构造或编辑表单的键值对，然后通过脚本发送给服务器。浏览器原生提供了 FormData 对象来完成这项工作

FormData 的实例方法
FormData.get(key)：获取指定键名对应的键值，参数为键名。如果有多个同名的键值对，则返回第一个键值对的键值。
FormData.getAll(key)：返回一个数组，表示指定键名对应的所有键值。如果有多个同名的键值对，数组会包含所有的键值。
FormData.set(key, value)：设置指定键名的键值，参数为键名。如果键名不存在，会添加这个键值对，否则会更新指定键名的键值。如果第二个参数是文件，还可以使用第三个参数，表示文件名。
FormData.delete(key)：删除一个键值对，参数为键名。
FormData.append(key, value)：添加一个键值对。如果键名重复，则会生成两个相同键名的键值对。如果第二个参数是文件，还可以使用第三个参数，表示文件名。
FormData.has(key)：返回一个布尔值，表示是否具有该键名的键值对。
FormData.keys()：返回一个遍历器对象，用于for...of循环遍历所有的键名。
FormData.values()：返回一个遍历器对象，用于for...of循环遍历所有的键值。
FormData.entries()：返回一个遍历器对象，用于for...of循环遍历所有的键值对。如果直接用for...of循环遍历 FormData 实例，默认就会调用这个方法


表单的内置验证
表单提交的时候，浏览器允许开发者指定一些条件，它会自动验证各个表单控件的值是否符合条件

// 1
<!-- 必填 -->
<input required>

<!-- 必须符合正则表达式 -->
<input pattern="banana|cherry">

<!-- 字符串长度必须为6个字符 -->
<input minlength="6" maxlength="6">

<!-- 数值必须在1到10之间 -->
<input type="number" min="1" max="10">

<!-- 必须填入 Email 地址 -->
<input type="email">

<!-- 必须填入 URL -->
<input type="URL">


表单元素和表单控件都有checkValidity()方法，用于手动触发校验
// 触发整个表单的校验
form.checkValidity()

// 触发单个表单控件的校验
formControl.checkValidity()


控件元素的willValidate属性是一个布尔值，表示该控件是否会在提交时进行校验

控件元素的validationMessage属性返回一个字符串，表示控件不满足校验条件时，浏览器显示的提示文本

控件元素的setCustomValidity()方法用来定制校验失败时的报错信息

控件元素的属性validity属性返回一个ValidityState对象，包含当前校验状态的信息

表单元素的 HTML 属性novalidate，可以关闭浏览器的自动校验

表单能够用四种编码，向服务器发送数据。编码格式由表单的enctype属性决定

用户上传文件，也是通过表单。具体来说，就是通过文件输入框选择本地文件，提交表单的时候，浏览器就会把这个文件发送到服务器




IndexedDB 就是浏览器提供的本地数据库，它可以被网页脚本创建和操作。IndexedDB 允许储存大量数据，提供查找接口，还能建立索引

IndexedDB 的特点
1. 键值对储存。 IndexedDB 内部采用对象仓库（object store）存放数据。所有类型的数据都可以直接存入，包括 JavaScript 对象。对象仓库中，数据以“键值对”的形式保存，每一个数据记录都有对应的主键，主键是独一无二的，不能有重复，否则会抛出一个错误。
2. 异步。 IndexedDB 操作时不会锁死浏览器，用户依然可以进行其他操作，这与 LocalStorage 形成对比，后者的操作是同步的。异步设计是为了防止大量数据的读写，拖慢网页的表现。
3. 支持事务。 IndexedDB 支持事务（transaction），这意味着一系列操作步骤之中，只要有一步失败，整个事务就都取消，数据库回滚到事务发生之前的状态，不存在只改写一部分数据的情况。
4. 同源限制。 IndexedDB 受到同源限制，每一个数据库对应创建它的域名。网页只能访问自身域名下的数据库，而不能访问跨域的数据库。
5. 储存空间大。 IndexedDB 的储存空间比 LocalStorage 大得多，一般来说不少于 250MB，甚至没有上限。
6. 支持二进制储存。 IndexedDB 不仅可以储存字符串，还可以储存二进制数据（ArrayBuffer 对象和 Blob 对象）


操作流程
// 打开数据库
var request = window.indexedDB.open(databaseName, version);

// 新建数据库
request.onupgradeneeded = function (event) {
  db = event.target.result;
  var objectStore;
  if (!db.objectStoreNames.contains('person')) {
    objectStore = db.createObjectStore('person', { keyPath: 'id' });
  }
}

// 新建索引
request.onupgradeneeded = function(event) {
  db = event.target.result;
  var objectStore = db.createObjectStore('person', { keyPath: 'id' });
  objectStore.createIndex('name', 'name', { unique: false });
  objectStore.createIndex('email', 'email', { unique: true });
}

// 新增数据
function add() {
  var request = db.transaction(['person'], 'readwrite')
    .objectStore('person')
    .add({ id: 1, name: '张三', age: 24, email: 'zhangsan@example.com' });

  request.onsuccess = function (event) {
    console.log('数据写入成功');
  };

  request.onerror = function (event) {
    console.log('数据写入失败');
  }
}

add();

// 读取数据
function read() {
   var transaction = db.transaction(['person']);
   var objectStore = transaction.objectStore('person');
   var request = objectStore.get(1);

   request.onerror = function(event) {
     console.log('事务失败');
   };

   request.onsuccess = function( event) {
      if (request.result) {
        console.log('Name: ' + request.result.name);
        console.log('Age: ' + request.result.age);
        console.log('Email: ' + request.result.email);
      } else {
        console.log('未获得数据记录');
      }
   };
}

read();

// 遍历数据
function readAll() {
  var objectStore = db.transaction('person').objectStore('person');

   objectStore.openCursor().onsuccess = function (event) {
     var cursor = event.target.result;

     if (cursor) {
       console.log('Id: ' + cursor.key);
       console.log('Name: ' + cursor.value.name);
       console.log('Age: ' + cursor.value.age);
       console.log('Email: ' + cursor.value.email);
       cursor.continue();
    } else {
      console.log('没有更多数据了！');
    }
  };
}

readAll();


// 更新数据
function update() {
  var request = db.transaction(['person'], 'readwrite')
    .objectStore('person')
    .put({ id: 1, name: '李四', age: 35, email: 'lisi@example.com' });

  request.onsuccess = function (event) {
    console.log('数据更新成功');
  };

  request.onerror = function (event) {
    console.log('数据更新失败');
  }
}

update();

// 删除数据
function remove() {
  var request = db.transaction(['person'], 'readwrite')
    .objectStore('person')
    .delete(1);

  request.onsuccess = function (event) {
    console.log('数据删除成功');
  };
}

remove();

// 使用索引

objectStore.createIndex('name', 'name', { unique: false });   // 假定新建表格的时候，对name字段建立了索引

var transaction = db.transaction(['person'], 'readonly');
var store = transaction.objectStore('person');
var index = store.index('name');
var request = index.get('李四');

request.onsuccess = function (e) {
  var result = e.target.result;
  if (result) {
    // ...
  } else {
    // ...
  }
}



浏览器原生提供indexedDB对象，作为开发者的操作接口

indexedDB.open()方法用于打开数据库。这是一个异步操作，但是会立刻返回一个 IDBOpenDBRequest 对象

indexedDB.deleteDatabase()方法用于删除一个数据库，参数为数据库的名字

indexedDB.cmp()方法比较两个值是否为 indexedDB 的相同的主键

IDBRequest 对象表示打开的数据库连接，indexedDB.open()方法和indexedDB.deleteDatabase()方法会返回这个对象。数据库的操作都是通过这个对象完成的

打开数据成功以后，可以从IDBOpenDBRequest对象的result属性上面，拿到一个IDBDatabase对象，它表示连接的数据库。后面对数据库的操作，都通过这个对象完成

IDBObjectStore 对象对应一个对象仓库（object store）。IDBDatabase.createObjectStore()方法返回的就是一个 IDBObjectStore 对象

IDBTransaction 对象用来异步操作数据库事务，所有的读写操作都要通过这个对象进行

IDBIndex 对象代表数据库的索引，通过这个对象可以获取数据库里面的记录

DBCursor 对象代表指针对象，用来遍历数据仓库（IDBObjectStore）或索引（IDBIndex）的记录

IDBKeyRange 对象代表数据仓库（object store）里面的一组主键。根据这组主键，可以获取数据仓库或索引里面的一组记录



JavaScript 语言采用的是单线程模型
Web Worker 的作用，就是为 JavaScript 创造多线程环境，允许主线程创建 Worker 线程，将一些任务分配给后者运行


Web Worker 的使用限制：
1. 同源限制
2. DOM 限制
3. 全局对象限制
4. 通信联系
5. 脚本限制
6. 文件限制

主线程采用new命令，调用Worker()构造函数，新建一个 Worker 线程

Worker 线程内部需要有一个监听函数，监听message事件

Worker 内部如果要加载其他脚本，有一个专门的方法importScripts()

主线程可以监听 Worker 是否发生错误。如果发生错误，Worker 会触发主线程的error事件

使用完毕，为了节省系统资源，必须关闭 Worker


主线程与 Worker 之间的通信 是传值而不是传址，Worker 对通信内容的修改，不会影响到主线程


```

- 网页元素接口
```js
<a>元素用来设置链接
<a>元素有一系列 URL 相关属性，可以用来操作链接地址
hash：片段识别符（以#开头）
host：主机和端口（默认端口80和443会省略）
hostname：主机名
href：完整的 URL
origin：协议、域名和端口
password：主机名前的密码
pathname：路径（以/开头）
port：端口
protocol：协议（包含尾部的冒号:）
search：查询字符串（以?开头）
username：主机名前的用户名

// HTML 代码如下
// <a id="test" href="http://user:passwd@example.com:8081/index.html?bar=1#foo">test</a>
var a = document.getElementById('test');
a.hash // "#foo"
a.host // "example.com:8081"
a.hostname // "example.com"
a.href // "http://user:passed@example.com:8081/index.html?bar=1#foo"
a.origin // "http://example.com:8081"
a.password // "passwd"
a.pathname // "/index.html"
a.port // "8081"
a.protocol // "http:"
a.search // "?bar=1"
a.username // "user"


accessKey属性用来读写<a>元素的快捷键
// HTML 代码如下
// <a id="test" href="http://example.com">test</a>
var a = document.getElementById('test');
a.accessKey = 'k';

download属性表示当前链接不是用来浏览，而是用来下载的。它的值是一个字符串，表示用户下载得到的文件名
// HTML 代码如下
// <a id="test" href="foo.jpg">下载</a>
var a = document.getElementById('test');
a.download = 'bar.jpg';

hreflang属性用来读写<a>元素的 HTML 属性hreflang，表示链接指向的资源的语言
// HTML 代码如下
// <a id="test" href="https://example.com" hreflang="en">test</a>
var a = document.getElementById('test');
a.hreflang // "en"

referrerPolicy属性用来读写<a>元素的 HTML 属性referrerPolicy，指定当用户点击链接时，如何发送 HTTP 头信息的referer字段
// HTML 代码如下
// <a id="test" href="https://example.com" referrerpolicy="no-referrer">test</a>
var a = document.getElementById('test');
a.referrerPolicy // "no-referrer"

rel属性用来读写<a>元素的 HTML 属性rel，表示链接与当前文档的关系
// HTML 代码如下
// <a id="test" href="https://example.com" rel="license">license.html</a>
var a = document.getElementById('test');
a.rel // "license"

tabIndex属性的值是一个整数，用来读写当前<a>元素在文档里面的 Tab 键遍历顺序
// HTML 代码如下
// <a id="test" href="https://example.com">test</a>
var a = document.getElementById('test');
a.tabIndex // 0

target属性用来读写<a>元素的 HTML 属性target
// HTML 代码如下
// <a id="test" href="https://example.com" target="_blank">test</a>
var a = document.getElementById('test');
a.target // "_blank"

text属性用来读写<a>元素的链接文本，等同于当前节点的textContent属性
// HTML 代码如下
// <a id="test" href="https://example.com">test</a>
var a = document.getElementById('test');
a.text // "test"

type属性用来读写<a>元素的 HTML 属性type，表示链接目标的 MIME 类型
// HTML 代码如下
// <a id="test" type="video/mp4" href="example.mp4">video</a>
var a = document.getElementById('test');
a.type // "video/mp4"


<a>元素的方法都是继承的，主要有以下三个
blur()：从当前元素移除键盘焦点，详见HTMLElement接口的介绍。
focus()：当前元素得到键盘焦点，详见HTMLElement接口的介绍。
toString()：返回当前<a>元素的 HTML 属性href


<img>元素用于插入图片，主要继承了 HTMLImageElement 接口

<form>元素代表了表单，继承了 HTMLFormElement 接口

<input>元素主要用于表单组件，它继承了 HTMLInputElement 接口

<button>元素继承了HTMLButtonElement接口

<option>元素表示下拉框（<select>，<optgroup>或<datalist>）里面的一个选项。它是 HTMLOptionElement 接口的实例

<video>元素用来加载视频，是HTMLVideoElement对象的实例
<audio>元素用来加载音频，是HTMLAudioElement对象的实例
HTMLVideoElement和HTMLAudioElement都继承了HTMLMediaElement，所以这两个 HTML 元素有许多共同的属性和方法


HTMLMediaElement并没有对应的 HTML 元素，而是作为<video>和<audio>的基类，定义一些它们共同的属性和方法

HTMLVideoElement接口代表了<video>元素。这个接口继承了HTMLMediaElement接口，并且有一些自己的属性和方法

HTMLAudioElement接口代表了<audio>元素，该接口继承了HTMLMediaElement，但是没有定义自己的属性和方法。浏览器原生提供一个Audio()构造函数，返回的就是HTMLAudioElement实例

事件
<video>和<audio>元素有以下事件
loadstart：开始加载媒体文件时触发。
progress：媒体文件加载过程中触发，大概是每秒触发2到8次。
loadedmetadata：媒体文件元数据加载成功时触发。
loadeddata：当前播放位置加载成功后触发
......

```