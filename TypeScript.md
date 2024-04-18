- [TypeScript 教程](https://wangdoc.com/typescript/)
- [The TypeScript Handbook](https://www.typescriptlang.org/docs/handbook/intro.html)
- [Playground](https://www.typescriptlang.org/)
- [Learn X in Y minutes](https://learnxinyminutes.com/docs/typescript/)
- [Why You Should Choose TypeScript Over JavaScript](https://serokell.io/blog/why-typescript)
- []()
- []()
- []()
- []()


```ts
TypeScript（简称 TS）是微软公司开发的一种基于 JavaScript （简称 JS）语言的编程语言
TypeScript 可以看成是 JavaScript 的超集（superset），即它继承了后者的全部语法，增加了一些自己的语法，TypeScript 的主要功能是为 JavaScript 添加类型系统
在语法上，JavaScript 属于动态类型语言，TypeScript 引入了一个更强大、更严格的类型系统，属于静态类型语言

类型（type）指的是一组具有相同特征的值。如果两个值具有某种共同的特征，就可以说，它们属于同一种类型，类型是人为添加的一种编程约束和用法提示

类型声明
function toString(num:number):string {
  return String(num);
}

TypeScript 规定，变量只有赋值后才能使用，否则就会报错

类型推断
let foo = 123;
foo = 'hello'; // 报错

// 函数的返回值 类型推断
function toString(num:number) {
  return String(num);
}


TypeScript 项目要想运行，必须先转为 JavaScript 代码，这个代码转换的过程就叫做“编译”（compile）
TypeScript 官方没有做运行环境，只提供编译器。编译时，会将类型声明和类型相关的代码全部删除，只留下能运行的 JavaScript 代码，并且不会改变 JavaScript 的运行结果

TypeScript 代码只涉及类型，不涉及值。所有跟“值”相关的处理，都由 JavaScript 完成。TypeScript 的编译过程，实际上就是把“类型代码”全部拿掉，只保留“值代码”

TypeScript 官方提供的编译器叫做 tsc，可以将 TypeScript 脚本编译成 JavaScript 脚本

TypeScript 允许将tsc的编译参数，写在配置文件tsconfig.json。只要当前目录有这个文件，tsc就会自动读取，所以运行时可以不写参数

any 类型表示没有任何限制，该类型的变量可以赋予任意类型的值,TypeScript 将这种类型称为“顶层类型”（top type），意为涵盖了所有下层
变量类型一旦设为any，TypeScript 实际上会关闭这个变量的类型检查。即使有明显的类型错误，只要句法正确，都不会报错

any类型主要适用以下两个场合:
1. 出于特殊原因，需要关闭某些变量的类型检查，就可以把该变量的类型设为any
2. 为了适配以前老的 JavaScript 项目，让代码快速迁移到 TypeScript，可以把变量类型设为any

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







```