> TypeScript is JavaScript with syntax for types.

# playground
[Javascript Playground](https://playcode.io/)  
[**CodePen**](https://codepen.io/)  
> CodePen is a social development environment for front-end designers and developers.
[jsfiddle](https://jsfiddle.net/)  
[HTML-CSS-JS](https://html-css-js.com/)  
[onecompiler](https://onecompiler.com/html)  
[Online HTML Editor](https://www.programiz.com/html/online-compiler/)  
[]()  
[]()  

# tutorials
[A Beginner's Guide to HTML, CSS, and JavaScript](https://hackernoon.com/a-beginners-guide-to-html-css-and-javascript)  
[**HTML DOM for Beginners: What is the Document Object Model?**](https://www.fullstackfoundations.com/blog/html-dom)  
[**Full Stack Foundations**](https://www.fullstackfoundations.com)  
[Web APIs](https://developer.mozilla.org/en-US/docs/Web/API)  
[JavaScript and HTML DOM](https://www.w3schools.com/jsref/)  
[]()  
[Learn HTML](https://www.programiz.com/html)  
[Learn CSS](https://www.programiz.com/css)  
[Learn JavaScript](https://www.programiz.com/javascript)  
[Learn TypeScript](https://www.programiz.com/typescript)  
[]()  
[]()  
[TypeScript Types Explained – A Mental Model to Help You Think in Types](https://www.freecodecamp.org/news/a-mental-model-to-think-in-typescript-2/)  
[]()  


[学习 Web 开发](https://developer.mozilla.org/zh-CN/docs/Learn)  
[Web 开发技术](https://developer.mozilla.org/zh-CN/docs/Web)  
[**Web technology for developers**](https://developer.mozilla.org/en-US/docs/Web)  

[Practice Your Coding Skills by Building a Program in Different Ways](https://www.freecodecamp.org/news/practice-coding-skills-by-building-a-program-different-ways/)  

[Typescript Practices](https://omakoleg.github.io/typescript-practices/)  
[typescript-eslint Rules](https://typescript-eslint.io/rules/)  

[TypeScript 入门教程](https://ts.xcatliu.com/)  
[TypeScript 使用指南手册](http://www.patrickzhong.com/TypeScript/PREFACE.html)  
[TypeScript 教程](https://wangdoc.com/typescript/)  
[TypeScript Deep Dive](https://basarat.gitbook.io/typescript)  
[**The TypeScript Handbook**](https://www.typescriptlang.org/docs/handbook/intro.html)  
[Playground](https://www.typescriptlang.org/)  
[Learn X in Y minutes](https://learnxinyminutes.com/docs/typescript/)  
[How Promises Work in JavaScript](https://www.freecodecamp.org/news/guide-to-javascript-promises/)  
[A guide to async/await in TypeScript](https://blog.logrocket.com/async-await-typescript/)  
[Introduction to async/await in TypeScript](https://www.atatus.com/blog/introduction-to-async-await-in-typescript/)  
[async/await support in ES6 targets](https://www.typescriptlang.org/docs/handbook/release-notes/typescript-1-7.html)  
[Mastering Async Programming in TypeScript: Promises, Async/Await, and Callbacks](https://dev.to/cliff123tech/mastering-async-programming-in-typescript-promises-asyncawait-and-callbacks-148b)

## Books
JavaScript高级程序设计 # pdf


```TS
// TypeScript(TS) is JavaScript with syntax for types.
// JavaScript 属于动态类型语言，TypeScript 引入了一个更强大、更严格的类型系统，属于静态类型语言

function toString(num:number):string {
  return String(num);
}


let foo = 123;  // type inference
foo = 'hello';  // error

function toString(num:number) { // function return type inference
  return String(num);
}

// compile: TypeScript code -> JavaScript code
// TypeScript 代码只涉及类型，不涉及值。所有跟“值”相关的处理，都由 JavaScript 完成。TypeScript 的编译过程，实际上就是把“类型代码”全部拿掉，只保留“值代码”

// tsc: TypeScript -> JavaScript
// tsc config file: tsconfig.json

// any type
// unknown type

unknown 类型 是 严格版的any，unknown可以看作是更安全的any
unknown类型相较于any类型的几个限制：
1. unknown类型的变量，不能直接赋值给其他类型的变量（除了any类型和unknown类型）
2. 不能直接调用unknown类型变量的方法和属性
3. unknown类型变量能够进行的运算是有限的，只能进行比较运算（运算符==、===、!=、!==、||、&&、?）、取反运算（运算符!）、typeof运算符和instanceof运算符这几种

只有经过“类型缩小”，unknown类型变量才可以使用
// 1
let a:unknown = 1;

if (typeof a === 'number') {
  let r = a + 10; // 正确
}
// unknown类型的变量a经过typeof运算以后，能够确定实际类型是number，就能用于加法运算了

// 2
let s:unknown = 'hello';

if (typeof s === 'string') {
  s.length; // 正确
}
// 确定变量s的类型为字符串以后，才能调用它的length属性

never 类型
TypeScript 还引入了“空类型”的概念，即该类型为空，不包含任何值
never类型的使用场景，主要是在一些类型运算之中，保证类型运算的完整性
// 1
function fn(x:string|number) {
  if (typeof x === 'string') {
    // ...
  } else if (typeof x === 'number') {
    // ...
  } else {
    x; // never 类型
  }
}

never类型的一个重要特点是，可以赋值给任意其他类型
never类型是任何其他类型所共有的，TypeScript 把这种情况称为“底层类型”（bottom type）

TypeScript 有两个“顶层类型”（any和unknown），但是“底层类型”只有never唯一一个


TypeScript 的类型系统
TypeScript 继承了 JavaScript 的类型:
boolean
string
number
bigint
symbol
object
undefined
null

以上8种类型可以看作 TypeScript 的基本类型，复杂类型由它们组合而成


包装对象类型
原始类型的值，都有对应的包装对象（wrapper object）。所谓“包装对象”，指的是这些值在需要时，会自动产生的对象

大写的Object类型代表 JavaScript 语言里面的广义对象。所有可以转成对象的值，都是Object类型，这囊括了几乎所有的值
小写的object类型代表 JavaScript 里面的狭义对象，即可以用字面量表示的对象，只包含对象、数组和函数，不包括原始类型的值
// 1
let obj:Object;
 
obj = true;
obj = 'hi';
obj = 1;
obj = { foo: 123 };
obj = [1, 2];
obj = (a:number) => a + 1;

// 2
let obj:object;
 
obj = { foo: 123 };
obj = [1, 2];
obj = (a:number) => a + 1;
obj = true; // 报错
obj = 'hi'; // 报错
obj = 1; // 报错


undefined和null既是值，又是类型
作为值，它们有一个特殊的地方：任何其他类型的变量都可以赋值为undefined或null


TypeScript 规定，单个值也是一种类型，称为“值类型”
// 1
let x:'hello';

x = 'hello'; // 正确
x = 'world'; // 报错
// 变量x的类型是字符串hello，导致它只能赋值为这个字符串，赋值为其他字符串就会报错

TypeScript 推断类型时，遇到const命令声明的变量，如果代码里面没有注明类型，就会推断该变量是值类型
// x 的类型是 "https"
const x = 'https';

// y 的类型是 string
const y:string = 'https';
// 变量x是const命令声明的，TypeScript 就会推断它的类型是值https，而不是string类型


注意，const命令声明的变量，如果赋值为对象，并不会推断为值类型，这是因为 JavaScript 里面，const变量赋值为对象时，属性值是可以改变的
// x 的类型是 { foo: number }
const x = { foo: 1 };


联合类型（union types）指的是多个类型组成的一个新类型，使用符号|表示，联合类型可以与值类型相结合，表示一个变量的值有若干种可能
联合类型本身可以看成是一种“类型放大”（type widening），处理时就需要“类型缩小”（type narrowing）
// 1
let setting:true|false;

let gender:'male'|'female';

let rainbowColor:'赤'|'橙'|'黄'|'绿'|'青'|'蓝'|'紫';

联合类型的第一个成员前面，也可以加上竖杠|，这样便于多行书写
//
let x:
  | 'one'
  | 'two'
  | 'three'
  | 'four';

“类型缩小”是 TypeScript 处理联合类型的标准方法，凡是遇到可能为多种类型的场合，都需要先缩小类型，再进行处理
// 1
function printId(
  id:number|string
) {
  if (typeof id === 'string') {
    console.log(id.toUpperCase());
  } else {
    console.log(id);
  }
}

// 2
function getPort(
  scheme: 'http'|'https'
) {
  switch (scheme) {
    case 'http':
      return 80;
    case 'https':
      return 443;
  }


交叉类型（intersection types）指的多个类型组成的一个新类型，使用符号&表示

交叉类型的主要用途是表示对象的合成
// 1
let obj:
  { foo: string } &
  { bar: string };

obj = {
  foo: 'hello',
  bar: 'world'
};
// 变量obj同时具有属性foo和属性bar

交叉类型常常用来为对象类型添加新属性
// 2
type A = { foo: number };

type B = A & { bar: number };
// 类型B是一个交叉类型，用来在A的基础上增加了属性bar


type命令用来定义一个类型的别名，type命令属于类型相关的代码，编译成 JavaScript 的时候，会被全部删除
别名可以让类型的名字变得更有意义，也能增加代码的可读性，还可以使复杂类型用起来更方便，便于以后修改变量的类型
别名不允许重名，同一个别名声明两次，会报错
别名的作用域是块级作用域。这意味着，代码块内部定义的别名，影响不到外部

// 1
type Color = 'red';

if (Math.random() < 0.5) {
  type Color = 'blue';
}


别名支持使用表达式，也可以在定义一个别名时，使用另一个别名，即别名允许嵌套
// 2
type World = "world";
type Greeting = `hello ${World}`;


JavaScript 语言中，typeof 运算符是一个一元运算符，返回一个字符串，代表操作数的类型， typeof 的操作数是一个值
JavaScript 里面，typeof运算符只可能返回八种结果，而且都是字符串
typeof undefined; // "undefined"
typeof true; // "boolean"
typeof 1337; // "number"
typeof "foo"; // "string"
typeof {}; // "object"
typeof parseInt; // "function"
typeof Symbol(); // "symbol"
typeof 127n // "bigint"

TypeScript 将typeof运算符移植到了类型运算，它的操作数依然是一个值，但是返回的不是字符串，而是该值的 TypeScript 类型
// 1
const a = { x: 0 };

type T0 = typeof a;   // { x: number }
type T1 = typeof a.x; // number
// 这种用法的typeof返回的是 TypeScript 类型，所以只能用在类型运算之中（即跟类型相关的代码之中），不能用在值运算

同一段代码可能存在两种typeof运算符，一种用在值相关的 JavaScript 代码部分，另一种用在类型相关的 TypeScript 代码部分
// 1
let a = 1;
let b:typeof a;                     // 类型运算

if (typeof a === 'number') {        // 值运算
  b = a;
}

由于编译时不会进行 JavaScript 的值运算，所以TypeScript 规定，typeof 的参数只能是标识符，不能是需要运算的表达式
type T = typeof Date(); // 报错

typeof命令的参数不能是类型
type Age = number;
type MyAge = typeof Age; // 报错

TypeScript 支持块级类型声明，即类型可以声明在代码块（用大括号表示）里面，并且只在当前代码块有效
// 1
if (true) {
  type T = number;
  let v:T = 5;
} else {
  type T = string;
  let v:T = 'hello';
}

TypeScript 为这种情况定义了一个专门术语。如果类型A的值可以赋值给类型B，那么类型A就称为类型B的子类型（subtype）

// 1
type T = number|string;

let a:number = 1;
let b:T = a;        // 类型number就是类型number|string的子类型

TypeScript 的一个规则是，凡是可以使用父类型的地方，都可以使用子类型，但是反过来不行



TypeScript 数组有一个根本特征：所有成员的类型必须相同，但是成员数量是不确定的

// 1
let arr:number[] = [1, 2, 3];

let arr:(number|string)[];

let arr:Array<number> = [1, 2, 3];

let arr:Array<number|string>;

数组的成员是可以动态变化的


数组的类型推断

const arr = [];
arr // 推断为 any[]

arr.push(123);
arr // 推断类型为 number[]

arr.push('abc');
arr // 推断类型为 (string|number)[]

类型推断的自动更新只发生初始值为空数组的情况。如果初始值不是空数组，类型推断就不会更新

JavaScript 规定，const命令声明的数组变量是可以改变成员的
TypeScript 允许声明只读数组，方法是在数组类型前面加上readonly关键字，TypeScript 将readonly number[]与number[]视为两种不一样的类型，后者是前者的子类型
// 1
const arr:readonly number[] = [0, 1];

arr[1] = 2; // 报错
arr.push(3); // 报错
delete arr[0]; // 报错


TypeScript 使用T[][]的形式，表示二维数组，T是最底层数组成员的类型
// 1
var multi:number[][] =
  [[1,2,3], [23,24,25]];


元组（tuple）是 TypeScript 特有的数据类型，它表示成员类型可以自由设置的数组

// 1
const s:[string, string, boolean]
  = ['a', 'b', true];

数组的成员类型写在方括号外面（number[]），元组的成员类型是写在方括号里面（[number]）
TypeScript 的区分方法就是，成员类型写在方括号里面的就是元组，写在外面的就是数组
// 数组
let a:number[] = [1];

// 元组
let t:[number] = [1];

使用元组时，必须明确给出类型声明，不能省略，否则 TypeScript 会把一个值自动推断为数组
// a 的类型被推断为 (number | boolean)[]
let a = [1, true];

元组成员的类型可以添加问号后缀（?），表示该成员是可选的
let a:[number, number?] = [1];

问号只能用于元组的尾部成员，也就是说，所有可选成员必须在必选成员之后
type myTuple = [
  number,
  number,
  number?,
  string?
];


使用扩展运算符（...），可以表示不限成员数量的元组
// 1
type NamedNums = [
  string,
  ...number[]
];

const a:NamedNums = ['A', 1, 2];
const b:NamedNums = ['B', 1, 2, 3];

扩展运算符（...）用在元组的任意位置都可以，它的后面只能是一个数组或元组
// 1
type t1 = [string, number, ...boolean[]];
type t2 = [string, ...boolean[], number];
type t3 = [...boolean[], string, number];

元组的成员可以添加成员名，这个成员名是说明性的，可以任意取名，没有实际作用
type Color = [
  red: number,
  green: number,
  blue: number
];

const c:Color = [255, 255, 255];

元组可以通过方括号，读取成员类型
// 1
type Tuple = [string, number];
type Age = Tuple[1]; // number

// 2
type Tuple = [string, number, Date];
type TupleEl = Tuple[number];  // string|number|Date
// Tuple[number]表示元组Tuple的所有数值索引的成员类型，所以返回string|number|Date，即这个类型是三种值的联合类型


只读元组
// 写法一
type t = readonly [number, string]

// 写法二
type t = Readonly<[number, string]>

成员数量的推断
如果没有可选成员和扩展运算符，TypeScript 会推断出元组的成员数量（即元组长度）
如果包含了可选成员，TypeScript 会推断出可能的成员数量
如果使用了扩展运算符，TypeScript 就无法推断出成员数量

一旦扩展运算符使得元组的成员数量无法推断，TypeScript 内部就会把该元组当成数组处理


扩展运算符（...）将数组（注意，不是元组）转换成一个逗号分隔的序列，这时 TypeScript 会认为这个序列的成员数量是不确定的，因为数组的成员数量是不确定的
这导致如果函数调用时，使用扩展运算符传入函数参数，可能发生参数数量与数组长度不匹配的报错，解决方法：把成员数量不确定的数组，写成成员数量确定的元组，再使用扩展运算符
// 1
const arr:[number, number] = [1, 2];

function add(x:number, y:number){
  // ...
}

add(...arr) // 正确


另一种写法是使用as const断言
const arr = [1, 2] as const;
// 因为 TypeScript 会认为arr的类型是readonly [1, 2]，这是一个只读的值类型，可以当作数组，也可以当作元组


Symbol 是 ES2015 新引入的一种原始类型的值。它类似于字符串，但是每一个 Symbol 值都是独一无二的，与其他任何值都不相等
Symbol 值通过Symbol()函数生成。在 TypeScript 里面，Symbol 的类型使用symbol表示
// 1
let x:symbol = Symbol();
let y:symbol = Symbol();

x === y // false
// 变量x和y的类型都是symbol，且都用Symbol()生成，但是它们是不相等的

symbol类型包含所有的 Symbol 值，但是无法表示某一个具体的 Symbol 值，TypeScript 设计了symbol的一个子类型unique symbol，它表示单个的、某个具体的 Symbol 值

unique symbol表示单个值，所以这个类型的变量是不能修改值的，只能用const命令声明，不能用let声明

// 正确
const x:unique symbol = Symbol();

// 报错
let y:unique symbol = Symbol();

const命令为变量赋值 Symbol 值时，变量类型默认就是unique symbol
const x:unique symbol = Symbol();
// 等同于
const x = Symbol();


每个声明为unique symbol类型的变量，它们的值都是不一样的，其实属于两个值类型
const a:unique symbol = Symbol();
const b:unique symbol = Symbol();

a === b // 报错
// 变量a和变量b的类型虽然都是unique symbol，但其实是两个值类型

// 1
const a:unique symbol = Symbol();
const b:typeof a = a; // 正确

unique symbol 类型的一个作用，就是用作属性名，这可以保证不会跟其他属性名冲突。如果要把某一个特定的 Symbol 值当作属性名，那么它的类型只能是 unique symbol，不能是 symbol
// 1
const x:unique symbol = Symbol();
const y:symbol = Symbol();

interface Foo {
  [x]: string; // 正确
  [y]: string; // 报错
}
// 变量y当作属性名，但是y的类型是 symbol，不是固定不变的值，导致报错

unique symbol类型也可以用作类（class）的属性值，但只能赋值给类的readonly static属性
class C {
  static readonly foo:unique symbol = Symbol();
}
// static和readonly两个限定符缺一不可，这是为了保证这个属性是固定不变的


函数
// 1
function hello(
  txt:string
):void {
  console.log('hello ' + txt);
}

// 变量被赋值为一个函数
// 写法一
const hello = function (txt:string) {
  console.log('hello ' + txt);
}

// 写法二
const hello:
  (txt:string) => void
= function (txt) {
  console.log('hello ' + txt);
};

// 用type命令为函数类型定义一个别名

type MyFunc = (txt:string) => void;

const hello:MyFunc = function (txt) {
  console.log('hello ' + txt);
};


函数的实际参数个数，可以少于类型指定的参数个数，但是不能多于，即 TypeScript 允许省略参数

// 1
let x = (a:number) => 0;
let y = (b:number, s:string) => 0;

y = x; // 正确
x = y; // 报错

// 变量要套用另一个函数类型，使用typeof运算符
function add(
  x:number,
  y:number
) {
  return x + y;
}

const myAdd:typeof add = function (x, y) {
  return x + y;
}

这是一个很有用的技巧，任何需要类型的地方，都可以使用typeof运算符从一个值获取类型

// 函数类型的对象的写法
let add:{
  (x:number, y:number):number
};
 
add = function (x, y) {
  return x + y;
};

函数类型的对象写法如下
{
  (参数列表): 返回值
}
这种写法的函数参数与返回值之间，间隔符是冒号:，而不是正常写法的箭头=>，因为这里采用的是对象类型的写法，对象的属性名与属性值之间使用的是冒号

这种写法平时很少用，但是非常合适用在一个场合：函数本身存在属性
function f(x:number) {
  console.log(x);
}

f.version = '1.0';
// 函数f()本身还有一个属性version。这时，f完全就是一个对象，类型就要使用对象的写法


TypeScript 提供 Function 类型表示函数，任何函数都属于这个类型
Function 类型的值都可以直接执行，Function 类型的函数可以接受任意数量的参数，每个参数的类型都是any，返回值的类型也是any，代表没有任何约束


箭头函数是普通函数的一种简化写法

// 1
const repeat = (
  str:string,
  times:number
):string => str.repeat(times);
// 变量repeat被赋值为一个箭头函数，类型声明写在箭头函数的定义里面。其中，参数的类型写在参数名后面，返回值类型写在参数列表的圆括号后面

类型写在箭头函数的定义里面，与使用箭头函数表示函数类型，写法有所不同
// 1
function greet(
  fn:(a:string) => void
):void {
  fn('world');
}
// 函数greet()的参数fn是一个函数，类型就用箭头函数表示。这时，fn的返回值类型要写在箭头右侧，而不是写在参数列表的圆括号后面

// 2
type Person = { name: string };

const people = ['alice', 'bob', 'jan'].map(
  (name):Person => ({name})
);
// Person是一个类型别名，代表一个对象，该对象有属性name。变量people是数组的map()方法的返回值

如果函数的某个参数可以省略，则在参数名后面加问号表示
// 1
function f(x?:number) {
  // ...
}

f(); // OK
f(10); // OK

参数名带有问号，表示该参数的类型实际上是原始类型|undefined
// 1
function f(x?:number) {
  return x;
}

f(undefined) // 正确

类型显式设为undefined的参数，就不能省略
// 1
function f(x:number|undefined) {
  return x;
}

f() // 报错

函数的可选参数只能在参数列表的尾部，跟在必选参数的后面
//
let myFunc:
  (a?:number, b:number) => number; // 报错

如果前部参数有可能为空，这时只能显式注明该参数类型可能为undefined
//
let myFunc:
  (
    a:number|undefined,
    b:number
  ) => number;


函数体内部用到可选参数时，需要判断该参数是否为undefined
//
let myFunc:
  (a:number, b?:number) => number; 

myFunc = function (x, y) {
  if (y === undefined) {
    return x;
  }
  return x + y;
}

参数默认值
设置了默认值的参数，就是可选的。如果不传入该参数，它就会等于默认值
// 1
function createPoint(
  x:number = 0,
  y:number = 0
):[number, number] {
  return [x, y];
}

createPoint() // [0, 0]

设有默认值的参数，如果传入undefined，也会触发默认值
//
function f(x = 456) {
  return x;
}

f(undefined) // 456


具有默认值的参数如果不位于参数列表的末尾，调用时不能省略，如果要触发默认值，必须显式传入undefined

function add(
  x:number = 0,
  y:number
) {
  return x + y;
}

add(1) // 报错
add(undefined, 1) // 正确


参数解构

// 1
function f(
  [x, y]: [number, number]
) {
  // ...
}

function sum(
  { a, b, c }: {
     a: number;
     b: number;
     c: number
  }
) {
  console.log(a + b + c);
}

// 2 参数解构可以结合类型别名（type 命令）一起使用，代码会看起来简洁一些
type ABC = { a:number; b:number; c:number };

function sum({ a, b, c }:ABC) {
  console.log(a + b + c);
}


rest 参数表示函数剩余的所有参数，它可以是数组（剩余参数类型相同），也可能是元组（剩余参数类型不同）

// rest 参数为数组
function joinNumbers(...nums:number[]) {
  // ...
}

// rest 参数为元组
function f(...args:[boolean, number]) {
  // ...
}

如果元组里面的参数是可选的，则要使用可选参数
function f(
  ...args: [boolean, string?]
) {}

// 1
function multiply(n:number, ...m:number[]) {
  return m.map((x) => n * x);
}

rest 参数甚至可以嵌套
//
function f(...args:[boolean, ...string[]]) {
  // ...
}

rest 参数可以与变量解构结合使用
//
function repeat(
  ...[str, times]: [string, number]
):string {
  return str.repeat(times);
}

// 等同于
function repeat(
  str: string,
  times: number
):string {
  return str.repeat(times);
}

readonly 只读参数
//
function arraySum(
  arr:readonly number[]
) {
  // ...
  arr[0] = 0; // 报错
}

readonly关键字目前只允许用在数组和元组类型的参数前面，如果用在其他类型的参数前面，就会报错


void 类型表示函数没有返回值

// 1
function f():void {
  console.log('hello');
}

// 2
function f():void {
  return undefined; // 正确
}

function f():void {
  return null; // 正确
}

never类型表示肯定不会出现的值。它用在函数的返回值，就表示某个函数肯定不会返回值，即函数不会正常执行结束
主要有以下两种情况:
1.抛出错误的函数
// 1
function fail(msg:string):never {
  throw new Error(msg);
}

只有抛出错误，才是 never 类型。如果显式用return语句返回一个 Error 对象，返回值就不是 never 类型
//
function fail():Error {
  return new Error("Something failed");
}

2.无限执行的函数
// 1
const sing = function():never {
  while (true) {
    console.log('sing');
  }
};


never类型不同于void类型。前者表示函数没有执行结束，不可能有返回值；后者表示函数正常执行结束，但是不返回值，或者说返回undefined
// 正确
function sing():void {
  console.log('sing');
}

// 报错
function sing():never {
  console.log('sing');
}

函数内部允许声明其他类型，该类型只在函数内部有效

一个函数的返回值还是一个函数，那么前一个函数就称为高阶函数（higher-order function）
//
(someValue: number) => (multiplier: number) => someValue * multiplier;


函数重载

TypeScript 对于“函数重载”的类型声明方法是，逐一定义每一种情况的类型
JavaScript 函数只能有一个实现，必须在这个实现当中，处理不同的参数

// 1
function reverse(str:string):string;
function reverse(arr:any[]):any[];
function reverse(
  stringOrArray:string|any[]
):string|any[] {
  if (typeof stringOrArray === 'string')
    return stringOrArray.split('').reverse().join('');
  else
    return stringOrArray.slice().reverse();
}

重载的各个类型描述与函数的具体实现之间，不能有其他代码，否则报错


对象的方法也可以使用重载
// 1
class StringBuilder {
  #data = '';

  add(num:number): this;
  add(bool:boolean): this;
  add(str:string): this;
  add(value:any): this {
    this.#data += String(value);
    return this;
  }

  toString() {
    return this.#data;
  }
}

函数重载也可以用来精确描述函数参数与返回值之间的对应关系

// 1
function createElement(
  tag:'a'
):HTMLAnchorElement;
function createElement(
  tag:'canvas'
):HTMLCanvasElement;
function createElement(
  tag:'table'
):HTMLTableElement;
function createElement(
  tag:string
):HTMLElement {
  // ...
}

// 1 函数重载，也可以用对象表示
type CreateElement = {
  (tag:'a'): HTMLAnchorElement;
  (tag:'canvas'): HTMLCanvasElement;
  (tag:'table'): HTMLTableElement;
  (tag:string): HTMLElement;
}

优先使用联合类型替代函数重载，除非多个参数之间、或者某个参数与返回值之间，存在对应关系
// 写法一
function len(s:string):number;
function len(arr:any[]):number;
function len(x:any):number {
  return x.length;
}

// 写法二
function len(x:any[]|string):number {
  return x.length;
}


构造函数
JavaScript 语言使用构造函数，生成对象的实例，构造函数的最大特点，就是必须使用new命令调用

构造函数的类型写法，就是在参数列表前面加上new命令
// 1
class Animal {
  numLegs:number = 4;
}

type AnimalConstructor = new () => Animal;

function create(c:AnimalConstructor):Animal {
  return new c();
}

const a = create(Animal);


// 2 构造函数还有另一种类型写法，就是采用对象形式
type F = {
  new (s:string): object;
};

某些函数既是构造函数，又可以当作普通函数使用，比如Date()


除了原始类型，对象是 JavaScript 最基本的数据结构

对象类型的最简单声明方法，就是使用大括号表示对象，在大括号内部声明每个属性和方法的类型
// 1
const obj:{
  x:number;
  y:number;
} = { x: 1, y: 1 };

// 2
// 属性类型以分号结尾
type MyObj = {
  x:number;
  y:number;
};

// 属性类型以逗号结尾
type MyObj = {
  x:number,
  y:number,
};

对象的方法使用函数类型描述
// 1
const obj:{
  x: number;
  y: number;
  add(x:number, y:number): number;
  // 或者写成
  // add: (x:number, y:number) => number;
} = {
  x: 1,
  y: 1,
  add(x, y) {
    return x + y;
  }
};

对象类型可以使用方括号读取属性的类型
//
type User = {
  name: string,
  age: number
};
type Name = User['name']; // string


除了type命令可以为对象类型声明一个别名，TypeScript 还提供了interface命令，可以把对象类型提炼为一个接口

// 写法一
type MyObj = {
  x:number;
  y:number;
};

const obj:MyObj = { x: 1, y: 1 };

// 写法二
interface MyObj {
  x: number;
  y: number;
}

const obj:MyObj = { x: 1, y: 1 };


TypeScript 不区分对象自身的属性和继承的属性，一律视为对象的属性
//
interface MyInterface {
  toString(): string; // 继承的属性
  prop: number; // 自身的属性
}

const obj:MyInterface = { // 正确
  prop: 123,
};


如果某个属性是可选的（即可以忽略），需要在属性名后面加一个问号
type User = {
  firstName: string;
  lastName?: string;
};

// 等同于
type User = {
  firstName: string;
  lastName?: string|undefined;
};


读取可选属性之前，必须检查一下是否为undefined
const user:{
  firstName: string;
  lastName?: string;
} = { firstName: 'Foo'};

if (user.lastName !== undefined) {
  console.log(`hello ${user.firstName} ${user.lastName}`)
}

// 建议的写法
// 写法一
let firstName = (user.firstName === undefined)
  ? 'Foo' : user.firstName;
let lastName = (user.lastName === undefined)
  ? 'Bar' : user.lastName;

// 写法二
let firstName = user.firstName ?? 'Foo';
let lastName = user.lastName ?? 'Bar';


属性名前面加上readonly关键字，表示这个属性是只读属性，不能修改

// 1
type Point = {
  readonly x: number;
  readonly y: number;
};

const p:Point = { x: 0, y: 0 };

p.x = 100; // 报错

// 2
interface Home {
  readonly resident: {
    name: string;
    age: number
  };
}

const h:Home = {
  resident: {
    name: 'Vicky',
    age: 42
  }
};

h.resident.age = 32; // 正确
h.resident = {
  name: 'Kate',
  age: 23 
} // 报错

如果一个对象有两个引用，即两个变量对应同一个对象，其中一个变量是可写的，另一个变量是只读的，那么从可写变量修改属性，会影响到只读变量
//
interface Person {
  name: string;
  age: number;
}

interface ReadonlyPerson {
  readonly name: string;
  readonly age: number;
}

let w:Person = {
  name: 'Vicky',
  age: 42,
};

let r:ReadonlyPerson = w;

w.age += 1;
r.age // 43


有些时候，无法事前知道对象会有多少属性，比如外部 API 返回的对象。这时 TypeScript 允许采用属性名表达式的写法来描述类型，称为“属性名的索引类型”

索引类型里面，最常见的就是属性名的字符串索引
//
type MyObj = {
  [property: string]: string
};

const obj:MyObj = {
  foo: 'a',
  bar: 'b',
  baz: 'c',
};
// [property: string]的property表示属性名，它的类型是string，即属性名类型为string


解构赋值用于直接从对象中提取属性

const {id, name, price} = product;

目前没法为解构变量指定类型，因为对象解构里面的冒号，JavaScript 指定了其他用途

let { x: foo, y: bar } = obj;

// 等同于
let foo = obj.x;
let bar = obj.y;
// 冒号不是表示属性x和y的类型，而是为这两个属性指定新的变量名

如果要为x和y指定类型，不得不写成下面这样
let { x: foo, y: bar }
  : { x: string; y: number } = obj;


“结构类型”原则（structural typing）
只要对象 B 满足 对象 A 的结构特征，TypeScript 就认为对象 B 兼容对象 A 的类型

// 1
type A = {
  x: number;
};

type B = {
  x: number;
  y: number;
};

对象A只有一个属性x，类型为number。对象B满足这个特征，因此兼容对象A，只要可以使用A的地方，就可以使用B

//
const B = {
  x: 1,
  y: 1
};

const A:{ x: number } = B; // 正确


如果对象使用字面量表示，会触发 TypeScript 的严格字面量检查（strict object literal checking）。如果字面量的结构跟类型定义的不一样（比如多出了未定义的属性），就会报错

// 1
等号右边是一个对象的字面量，这时会触发严格字面量检查
const point:{
  x:number;
  y:number;
} = {
  x: 1,
  y: 1,
  z: 1 // 报错
};

// 2
如果等号右边不是字面量，而是一个变量，根据结构类型原则，是不会报错的
const myPoint = {
  x: 1,
  y: 1,
  z: 1
};

const point:{
  x:number;
  y:number;
} = myPoint; // 


根据“结构类型”原则，如果一个对象的所有属性都是可选的，那么其他对象跟它都是结构类似的


空对象是 TypeScript 的一种特殊值，也是一种特殊类型
空对象没有自定义属性，所以对自定义属性赋值就会报错。空对象只能使用继承的属性，即继承自原型对象Object.prototype的属性

// 1
// 错误
const pt = {};
pt.x = 3;
pt.y = 4;

// 正确
const pt = {
  x: 3,
  y: 4
};


// 分步声明，一个比较好的方法是，使用扩展运算符（...）合成一个新对象
const pt0 = {};
const pt1 = { x: 3 };
const pt2 = { y: 4 };

const pt = {
  ...pt0, ...pt1, ...pt2
};


空对象作为类型，其实是Object类型的简写形式
let d:{};
// 等同于
// let d:Object;

d = {};
d = { x: 1 };
d = 'hello';
d = 2;


interface 是对象的模板，可以看作是一种类型约定，使用了某个模板的对象，就拥有了指定的类型结构

// 1
interface Person {
  firstName: string;
  lastName: string;
  age: number;
}
// 定义了一个接口Person，它指定一个对象模板，拥有三个属性firstName、lastName和age，任何实现这个接口的对象，都必须部署这三个属性，并且必须符合规定的类型

实现该接口很简单，只要指定它作为对象的类型即可

// 1
const p:Person = {
  firstName: 'John',
  lastName: 'Smith',
  age: 25
};

// 变量p的类型就是接口Person

方括号运算符可以取出 interface 某个属性的类型
//
interface Foo {
  a: string;
}

type A = Foo['a']; // string


interface 可以表示对象的各种语法，它的成员有5种形式
对象属性
对象的属性索引
对象方法
函数
构造函数

对象属性

interface Point {
  x: number;
  y: number;
}
// x和y都是对象的属性，分别使用冒号指定每个属性的类型

对象的属性索引

interface A {
  [prop: string]: number;
}
// [prop: string]就是属性的字符串索引，表示属性名只要是字符串，都符合类型要求

属性索引共有string、number和symbol三种类型


对象的方法共有三种写法
// 写法一
interface A {
  f(x: boolean): string;
}

// 写法二
interface B {
  f: (x: boolean) => string;
}

// 写法三
interface C {
  f: { (x: boolean): string };
}

属性名可以采用表达式
const f = 'f';

interface A {
  [f](x: boolean): string;
}

类型方法可以重载
interface A {
  f(): number;
  f(x: boolean): boolean;
  f(x: string, y: string): string;
}

interface 里面的函数重载，不需要给出实现，但是，由于对象内部定义方法时，无法使用函数重载的语法，所以需要额外在对象外部给出函数方法的实现

// 
interface A {
  f(): number;
  f(x: boolean): boolean;
  f(x: string, y: string): string;
}

function MyFunc(): number;
function MyFunc(x: boolean): boolean;
function MyFunc(x: string, y: string): string;
function MyFunc(
  x?:boolean|string, y?:string
):number|boolean|string {
  if (x === undefined && y === undefined) return 1;
  if (typeof x === 'boolean' && y === undefined) return true;
  if (typeof x === 'string' && typeof y === 'string') return 'hello';
  throw new Error('wrong parameters');  
}

const a:A = {
  f: MyFunc
}
// 接口A的方法f()有函数重载，需要额外定义一个函数MyFunc()实现这个重载，然后部署接口A的对象a的属性f等于函数MyFunc()

interface 也可以用来声明独立的函数
//
interface Add {
  (x:number, y:number): number;
}

const myAdd:Add = (x,y) => x + y;
// 接口Add声明了一个函数类型


interface 内部可以使用new关键字，表示构造函数
interface ErrorConstructor {
  new (message?: string): Error;
}
// 接口ErrorConstructor内部有new命令，表示它是一个构造函数


interface 可以使用extends关键字，继承其他 interface
// 1
interface Shape {
  name: string;
}

interface Circle extends Shape {
  radius: number;
}
//Circle继承了Shape，所以Circle其实有两个属性name和radius


interface 允许多重继承
// 1
interface Style {
  color: string;
}

interface Shape {
  name: string;
}

interface Circle extends Style, Shape {
  radius: number;
}
// Circle同时继承了Style和Shape，所以拥有三个属性color、name和radius

多重接口继承，实际上相当于多个父接口的合并
如果子接口与父接口存在同名属性，那么子接口的属性会覆盖父接口的属性


interface 可以继承type命令定义的对象类型
// 1
type Country = {
  name: string;
  capital: string;
}

interface CountryWithPop extends Country {
  population: number;
}

// CountryWithPop继承了type命令定义的Country对象，并且新增了一个population属性

注意，如果type命令定义的类型不是对象，interface 就无法继承


interface 还可以继承 class，即继承该类的所有成员
// 1
class A {
  x:string = '';

  y():boolean {
    return true;
  }
}

interface B extends A {
  z: number
}
// B继承了A，因此B就具有属性x、y()和z

实现B接口的对象就需要实现这些属性
const b:B = {
  x: '',
  y: function(){ return true },
  z: 123
}


多个同名接口会合并成一个接口
// 1

interface Box {
  height: number;
  width: number;
}

interface Box {
  length: number;
}
// 两个Box接口会合并成一个接口，同时有height、width和length三个属性


同名接口合并时，如果同名方法有不同的类型声明，那么会发生函数重载

// 1

interface Cloner {
  clone(animal: Animal): Animal;
}

interface Cloner {
  clone(animal: Sheep): Sheep;
}

interface Cloner {
  clone(animal: Dog): Dog;
  clone(animal: Cat): Cat;
}

// 等同于
interface Cloner {
  clone(animal: Dog): Dog;
  clone(animal: Cat): Cat;
  clone(animal: Sheep): Sheep;
  clone(animal: Animal): Animal;
}


interface 与 type 的异同

都能为对象类型起名
// 1
type Country = {
  name: string;
  capital: string;
}

interface Country {
  name: string;
  capital: string;
}

class命令也有类似作用，通过定义一个类，同时定义一个对象类型，但是，它会创造一个值，编译后依然存在
如果只是单纯想要一个类型，应该使用type或interface

interface 与 type 的区别：
1.type能够表示非对象类型，而interface只能表示对象类型（包括数组、函数等）
2.interface可以继承其他类型，type不支持继承
  继承的主要作用是添加属性，type定义的对象类型如果想要添加属性，只能使用&运算符，重新定义一个类型
3.同名interface会自动合并，同名type则会报错
4.interface不能包含属性映射（mapping），type可以
5.this关键字只能用于interface
6.type 可以扩展原始数据类型，interface 不行
7.interface无法表达某些复杂类型（比如交叉类型和联合类型），但是type可以

// 1
type Animal = {
  name: string
}

type Bear = Animal & {
  honey: boolean
}
// 类型Bear在Animal的基础上添加了一个属性honey


interface添加属性，采用的是继承的写法
// 1
interface Animal {
  name: string
}

interface Bear extends Animal {
  honey: boolean
}


继承时，type 和 interface 是可以换用的。interface 可以继承 type
// 1
type Foo = { x: number; };

interface Bar extends Foo {
  y: number;
}

// 2
interface Foo {
  x: number;
}

type Bar = Foo & { y: number; };


// 4
interface Point {
  x: number;
  y: number;
}

// 正确
type PointCopy1 = {
  [Key in keyof Point]: Point[Key];
};

// 报错
interface PointCopy2 {
  [Key in keyof Point]: Point[Key];
};

// 5
// 正确
interface Foo {
  add(num:number): this;
};

// 报错
type Foo = {
  add(num:number): this;
};

// 6
// 正确
type MyStr = string & {
  type: 'new'
};

// 报错
interface MyStr extends string {
  type: 'new'
}

// 7
type A = { /* ... */ };
type B = { /* ... */ };

type AorB = A | B;
type AorBwithName = AorB & {
  name: string
};
// 类型AorB是一个联合类型，AorBwithName则是为AorB添加一个属性

一般情况下，interface灵活性比较高，便于扩充类型或自动合并，建议优先使用
如果有复杂的类型运算，那么没有其他选择只能使用type


类（class）是面向对象编程的基本构件，封装了属性和方法

类的属性可以在顶层声明，也可以在构造方法内部声明，对于顶层声明的属性，可以在声明时同时给出类型

// 1
class Point {
  x:number;
  y:number;
}

属性名前面加上 readonly 修饰符，就表示该属性是只读的。实例对象不能修改这个属性

// 1
class A {
  readonly id = 'foo';
}


类的方法就是普通函数，类型声明方式与函数一致

// 1
class Point {
  x:number;
  y:number;

  constructor(x:number, y:number) {
    this.x = x;
    this.y = y;
  }

  add(point:Point) {
    return new Point(
      this.x + point.x,
      this.y + point.y
    );
  }
}
// 构造方法constructor()和普通方法add()都注明了参数类型，但是省略了返回值类型，因为 TypeScript 可以自己推断出来

类的方法跟普通函数一样，可以使用参数默认值，以及函数重载

// 1
class Point {
  x: number;
  y: number;

  constructor(x = 0, y = 0) {
    this.x = x;
    this.y = y;
  }
}

// 2
class Point {
  constructor(x:number, y:string);
  constructor(s:string);
  constructor(xs:number|string, y?:string) {
    // ...
  }
}

构造方法不能声明返回值类型，否则报错，因为它总是返回实例对象


存取器（accessor）是特殊的类方法，包括取值器（getter）和存值器（setter）两种方法，取值器用来读取属性，存值器用来写入属性

// 1
class C {
  _name = '';
  get name() {
    return this._name;
  }
  set name(value) {
    this._name = value;
  }
}
// get name()是取值器，其中get是关键词，name是属性名，外部读取name属性时，实例对象会自动调用这个方法
// set name()是存值器，其中set是关键词，name是属性名。外部写入name属性时，实例对象会自动调用这个方法

TypeScript 对存取器有以下规则
1.如果某个属性只有get方法，没有set方法，那么该属性自动成为只读属性
2.set方法的参数类型，必须兼容get方法的返回值类(TypeScript 5.1 版做出了改变，现在两者可以不兼容)
3.get方法与set方法的可访问性必须一致，要么都为公开方法，要么都为私有方法

类允许定义属性索引
// 1
class MyClass {
  [s:string]: boolean |
    ((s:string) => boolean);

  get(s:string) {
    return this[s] as boolean;
  }
}
// [s:string]表示所有属性名类型为字符串的属性，它们的属性值要么是布尔值，要么是返回布尔值的函数


类的 interface 接口

interface 接口或 type 别名，可以用对象的形式，为 class 指定一组检查条件，类使用 implements 关键字，表示当前类满足这些外部类型条件的限制

// 1
interface Country {
  name:string;
  capital:string;
}
// 或者
type Country = {
  name:string;
  capital:string;
}

class MyCountry implements Country {
  name = '';
  capital = '';
}

// interface或type都可以定义一个对象类型。类MyCountry使用implements关键字，表示该类的实例对象满足这个外部类型


implements关键字后面，不仅可以是接口，也可以是另一个类。这时，后面的类将被当作接口
// 1
class Car {
  id:number = 1;
  move():void {};
}

class MyCar implements Car {
  id = 2; // 不可省略
  move():void {};   // 不可省略
}

// implements后面是类Car，这时 TypeScript 就把Car视为一个接口，要求MyCar实现Car里面的每一个属性和方法，否则就会报错

interface 描述的是类的对外接口，也就是实例的公开属性和公开方法，不能定义私有的属性和方法


类可以实现多个接口（其实是接受多重限制），每个接口之间使用逗号分隔
// 1
class Car implements MotorVehicle, Flyable, Swimmable {
  // ...
}

同时实现多个接口并不是一个好的写法，容易使得代码难以管理，可以使用两种方法替代
1.类的继承
2.接口的继承

// 1
class Car implements MotorVehicle {
}

class SecretCar extends Car implements Flyable, Swimmable {
}
// Car类实现了MotorVehicle，而SecretCar类继承了Car类，然后再实现Flyable和Swimmable两个接口，相当于SecretCar类同时实现了三个接口

// 2
interface MotorVehicle {
  // ...
}
interface Flyable {
  // ...
}
interface Swimmable {
  // ...
}

interface SuperCar extends MotoVehicle,Flyable, Swimmable {
  // ...
}

class SecretCar implements SuperCar {
  // ...
}
// 类SecretCar通过SuperCar接口，就间接实现了多个接口


TypeScript 不允许两个同名的类，但是如果一个类和一个接口同名，那么接口会被合并进类
// 1
class A {
  x:number = 1;
}

interface A {
  y:number;
}

let a = new A();
a.y = 10;

a.x // 1
a.y // 10


TypeScript 的类本身就是一种类型，但是它代表该类的实例类型，而不是 class 的自身类型

对于引用实例对象的变量来说，既可以声明类型为 Class，也可以声明类型为 Interface，因为两者都代表实例对象的类型
// 1
interface MotorVehicle {
}

class Car implements MotorVehicle {
}

// 写法一
const c1:Car = new Car();
// 写法二
const c2:MotorVehicle = new Car();
// 变量的类型可以写成类Car，也可以写成接口MotorVehicle。它们的区别是，如果类Car有接口MotoVehicle没有的属性和方法，那么只有变量c1可以调用这些属性和方法


类名作为类型使用，实际上代表一个对象，因此可以把类看作为对象类型起名，TypeScript 有三种方法可以为对象类型起名：type、interface 和 class


要获得一个类的自身类型，一个简便的方法就是使用 typeof 运算符

// 1
function createPoint(
  PointClass:typeof Point,
  x:number,
  y:number
):Point {
  return new PointClass(x, y);
}
// createPoint()的第一个参数PointClass是Point类自身，要声明这个参数的类型，简便的方法就是使用typeof Point

JavaScript 语言中，类只是构造函数的一种语法糖，本质上是构造函数的另一种写法。所以，类的自身类型可以写成构造函数的形式
// 1
function createPoint(
  PointClass: new (x:number, y:number) => Point,
  x: number,
  y: number
):Point {
  return new PointClass(x, y);
}

构造函数也可以写成对象形式
// 1
function createPoint(
  PointClass: {
    new (x:number, y:number): Point
  },
  x: number,
  y: number
):Point {
  return new PointClass(x, y);
}

可以把构造函数提取出来，单独定义一个接口（interface），这样可以大大提高代码的通用性
// 1
interface PointConstructor {
  new(x:number, y:number):Point;
}

function createPoint(
  PointClass: PointConstructor,
  x: number,
  y: number
):Point {
  return new PointClass(x, y);
}

类的自身类型就是一个构造函数，可以单独定义一个接口来表示


Class 也遵循“结构类型原则”。一个对象只要满足 Class 的实例结构，就跟该 Class 属于同一个类型

只要 A 类具有 B 类的结构，哪怕还有额外的属性和方法，TypeScript 也认为 A 兼容 B 的类型
不仅是类，如果某个对象跟某个 class 的实例结构相同，TypeScript 也认为两者的类型相同

// 1
class Person {
  name: string;
}

const obj = { name: 'John' };
const p:Person = obj; // 正确

由于这种情况，运算符instanceof不适用于判断某个对象是否跟某个 class 属于同一类型
obj instanceof Person // false


确定两个类的兼容关系时，只检查实例成员，不考虑静态成员和构造方法
// 1
class Point {
  x: number;
  y: number;
  static t: number;
  constructor(x:number) {}
}

class Position {
  x: number;
  y: number;
  z: number;
  constructor(x:string) {}
}

const point:Point = new Position('');


如果类中存在私有成员（private）或保护成员（protected），那么确定兼容关系时，TypeScript 要求私有成员和保护成员来自同一个类，这意味着两个类需要存在继承关系
// 1
// 情况一
class A {
  private name = 'a';
}

class B extends A {
}

const a:A = new B();

// 情况二
class A {
  protected name = 'a';
}

class B extends A {
  protected name = 'b';
}

const a:A = new B();


类的继承
类（这里又称“子类”）可以使用 extends 关键字继承另一个类（这里又称“基类”）的所有属性和方法

子类可以覆盖基类的同名方法，但是，子类的同名方法不能与基类的类型定义相冲突

// 1
class A {
  greet() {
    console.log('Hello, world!');
  }
}

class B extends A {
  greet(name?: string) {
    if (name === undefined) {
      super.greet();                // 使用super关键字指代基类是常见做法
    } else {
      console.log(`Hello, ${name}`);
    }
  }
}

如果基类包括保护成员（protected修饰符），子类可以将该成员的可访问性设置为公开（public修饰符），也可以保持保护成员不变，但是不能改用私有成员（private修饰符）
// 1
class A {
  protected x: string = '';
  protected y: string = '';
  protected z: string = '';
}

class B extends A {
  // 正确
  public x:string = '';

  // 正确
  protected y:string = '';

  // 报错
  private z: string = '';
}

extends关键字后面不一定是类名，可以是一个表达式，只要它的类型是构造函数
// 1
// 例一
class MyArray extends Array<number> {}

// 例二
class MyError extends Error {}

// 例三
class A {
  greeting() {
    return 'Hello from A';
  }
}
class B {
  greeting() {
    return 'Hello from B';
  }
}

interface Greeter {
  greeting(): string;
}

interface GreeterConstructor {
  new (): Greeter;
}

function getGreeterBase():GreeterConstructor {
  return Math.random() >= 0.5 ? A : B;
}

class Test extends getGreeterBase() {
  sayHello() {
    console.log(this.greeting());
  }
}


类的内部成员的外部可访问性，由三个可访问性修饰符（access modifiers）控制：public、private和protected
public修饰符表示这是公开成员，外部可以自由访问
private修饰符表示私有成员，只能用在当前类的内部，类的实例和子类都不能使用该成员
protected修饰符表示该成员是保护成员，只能在类的内部使用该成员，实例无法使用该成员，但是子类内部可以使用

// 1
class Greeter {
  public greet() {
    console.log("hi!");
  }
}

const g = new Greeter();
g.greet();

// 2
class A {
  private x:number = 0;
}

const a = new A();
a.x // 报错

class B extends A {
  showX() {
    console.log(this.x); // 报错
  }
}

// 3
class A {
  protected x = 1;
}

class B extends A {
  getX() {
    return this.x;
  }
}

const a = new A();
const b = new B();

a.x // 报错
b.getX() // 1


实例属性的简写形式

class Point {
  x:number;
  y:number;

  constructor(x:number, y:number) {
    this.x = x;
    this.y = y;
  }
}

简写为

class Point {
  constructor(
    public x:number,
    public y:number
  ) {}
}

const p = new Point(10, 10);
p.x // 10
p.y // 10

// 构造方法的参数x前面有public修饰符， TypeScript 就会自动声明一个公开属性x，同时还会设置x的值为构造方法的参数值


// 2
class A {
  constructor(
    public a: number,
    protected b: number,
    private c: number,
    readonly d: number
  ) {}
}

// 编译结果
class A {
    a;
    b;
    c;
    d;
    constructor(a, b, c, d) {
      this.a = a;
      this.b = b;
      this.c = c;
      this.d = d;
    }
}

// 3
class A {
  constructor(
    public readonly x:number,
    protected readonly y:number,
    private readonly z:number
  ) {}
}


类的内部可以使用static关键字，定义静态成员，静态成员是只能通过类本身使用的成员，不能通过实例对象使用

static关键字前面可以使用 public、private、protected 修饰符

静态私有属性也可以用 ES6 语法的#前缀表示

class MyClass {
  private static x = 0;
}
等价于
class MyClass {
  static #x = 0;            //  ES6 语法的#前缀
}

public和protected的静态成员可以被继承
// 1
class A {
  public static x = 1;
  protected static y = 1;
}

class B extends A {
  static getY() {
    return B.y;
  }
}

B.x // 1
B.getY() // 1


类也可以写成泛型，使用类型参数

// 1
class Box<Type> {
  contents: Type;

  constructor(value:Type) {
    this.contents = value;
  }
}

const b:Box<string> = new Box('hello!');

静态成员不能使用泛型的类型参数


TypeScript 允许在类的定义前面，加上关键字abstract，表示该类不能被实例化，只能当作其他类的模板。这种类就叫做“抽象类”（abstract class）

抽象类的子类也可以是抽象类，也就是说，抽象类可以继承其他抽象类

抽象类的内部可以有已经实现好的属性和方法，也可以有还未实现的属性和方法

抽象类的作用是，确保各种相关的子类都拥有跟基类相同的接口，可以看作是模板。其中的抽象成员都是必须由子类实现的成员，非抽象成员则表示基类已经实现的、由所有子类共享的成员

// 1
abstract class A {
  id = 1;
}

class B extends A {
  amount = 100;
}

const b = new B();

b.id // 1
b.amount // 100



类的方法经常用到this关键字，它表示该方法当前所在的对象

// 1
class A {
  name = 'A';

  getName() {
    return this.name;
  }
}

const a = new A();
a.getName() // 'A'

const b = {
  name: 'b',
  getName: a.getName
};
b.getName() // 'b'

// 如果getName()在变量a上运行，this指向a；如果在b上运行，this指向b


有些场合需要给出this类型，但是 JavaScript 函数通常不带有this参数，这时 TypeScript 允许函数增加一个名为this的参数，放在参数列表的第一位，用来描述函数内部的this关键字的类型

this参数的类型可以声明为各种对象

// 1
function foo(
  this: { name: string }
) {
  this.name = 'Jack';
  this.name = 0; // 报错
}

foo.call({ name: 123 }); // 报错


在类的内部，this本身也可以当作类型使用，表示当前类的实例对象
// 1
class Box {
  contents:string = '';

  set(value:string):this {
    this.contents = value;
    return this;
  }
}

this类型不允许应用于静态成员

有些方法返回一个布尔值，表示当前的this是否属于某种类型。这时，这些方法的返回值类型可以写成this is Type的形式，其中用到了is运算符
// 1
class FileSystemObject {
  isFile(): this is FileRep {
    return this instanceof FileRep;
  }

  isDirectory(): this is Directory {
    return this instanceof Directory;
  }

  // ...
}
// 两个方法的返回值类型都是布尔值，写成this is Type的形式，可以精确表示返回值
























































```

- Example 1
```js
// 模拟Makefile

type MakeRule = {
  target: string,
  prerequisites: Array<string>,
  commands: Array<string>,
};

type Makefile = {
  rules: Array<MakeRule>,
};

let makefile_run = (self: Makefile) => {
  if(self.rules.length === 0) {
    throw new Error(`no rule was found in the makefile`);
  }
  let first_rule = self.rules[0];
  eval_rule(first_rule, self.rules);
};

let eval_rule = (current_rule: MakeRule, rules: Array<MakeRule>) => {
  for (let p of current_rule.prerequisites) {
    let next_rule = rules.find(({target}) => target === p);
    if(next_rule === undefined)
      continue;
    eval_rule(next_rule, rules);
  }
  console.log(`running commnand "${current_rule.commands.join(" ")}"`);
};

let sample_makefile: Makefile = {
  rules: [
    {
      target: "target1",
      prerequisites: ["target2"],
      commands: ["command 1"]
    },
    {
      target: "target2",
      prerequisites: ["target3"],
      commands: ["command 2"]
    },
    {
      target: "target3",
      prerequisites: ["prerequisite 4"],
      commands: ["command 3"]
    },
  ]
};

makefile_run(sample_makefile);
```