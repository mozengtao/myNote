[HTTP权威指南](https://github.com/oxidation99/MyBooks-1/tree/master)

[What happens when you type a URL into your browser?](https://aws.amazon.com/blogs/mobile/what-happens-when-you-type-a-url-into-your-browser/)  
[What happens when you type a URL into your browser?](https://blog.bytebytego.com/p/what-happens-when-you-type-a-url)  

[System Design Interview An Insider’s Guide.pdf](https://github.com/G33kzD3n/Catalogue/blob/master/System%20Design%20Interview%20An%20Insider%E2%80%99s%20Guide%20by%20Alex%20Xu%20(z-lib.org).pdf)   
[System Design Interview An Insider’s Guide.pdf](https://github.com/mukul96/System-Design-AlexXu/blob/master/System%20Design%20Interview%20An%20Insider%E2%80%99s%20Guide%20by%20Alex%20Xu%20(z-lib.org).pdf)  

HTML 是网页使用的语言，定义了网页的结构和内容。浏览器访问网站，其实就是从服务器下载 HTML 代码，然后渲染出网页，它的最大特点就是支持超链接，点击链接就可以跳转到其他网页，从而构成了整个互联网

浏览器的网页开发，涉及三种技术：HTML、CSS 和 JavaScript
HTML 语言定义网页的结构和内容
CSS 样式表定义网页的样式
JavaScript 语言定义网页与用户的互动行为

```html
// 简单网页的 HTML 源码
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <title>网页标题</title>
</head>
<body>
  <p>Hello World</p>
</body>
</html>

网页的 HTML 代码由许许多多不同的标签（tag）构成。学习 HTML 语言，就是学习各种标签的用法


标签

标签用来告诉浏览器，如何处理这段代码。标签的内容就是浏览器所要渲染的、展示在网页上的内容
<title>网页标题</title>     // <title>和</title>就是一对标签

标签放在一对尖括号里面（比如<title>），大多数标签都是成对出现的，分成开始标签和结束标签，结束标签在标签名之前加斜杠，也有一些标签不是成对使用，而是只有开始标签，没有结束标签
<meta charset="utf-8">

HTML 标签名是大小写不敏感，习惯都是使用小写

HTML 语言忽略缩进和换行

// 1
<title>网页标题</title>

// 2
<title>
  网页标题
</title>

// 3
<title>网页
标题</title>

整个网页的 HTML 代码完全可以写成一行，浏览器照样解析，结果完全一样，正式发布网页之前，开发者有时会把源码压缩成一行，以减少传输的字节数


元素
“标签”和“元素”基本上是同义词，只是使用的场合不一样：标签是从源码角度来看，元素是从编程角度来看，比如<p>标签对应网页的p元素

嵌套的标签就构成了网页元素的层级关系
<div><p>hello world</p></div>
div元素内部包含了一个p元素，div是p的父元素，p是div的子元素

元素可以分成两大类：块级元素（block）和行内元素（inline）

块级元素默认占据一个独立的区域，在网页上会自动另起一行，占据 100% 的宽度
<p>hello</p>
<p>world</p>
p元素是块级元素，因此浏览器会将内容分成两行显示

行内元素默认与其他元素在同一行，不产生换行
<span>hello</span>
<span>world</span>
span元素是行内元素，因此浏览器会将两行内容放在一行显示


属性
属性（attribute）是标签的额外信息，使用空格与标签名和其他属性分隔
<img src="demo.jpg" width="500">        // <img>标签有两个属性：src和width

属性可以用等号指定属性值，属性值一般放在双引号里面，这不是必需的，但推荐总是使用双引号，属性名是大小写不敏感的


符合 HTML 语法标准的网页的基本结构
<!DOCTYPE html>                 // 文档类型，告诉浏览器如何解析网页
<html lang="zh-CN">             // <html>标签是网页的顶层容器(root element)，一个网页只能有一个<html>标签
<head>                          // <head>标签是一个容器标签，用于放置网页的元信息，为网页渲染提供额外信息
  <meta charset="utf-8">        // <meta>标签用于设置或说明网页的元数据，必须放在<head>里面
  <title></title>               // <title>标签用于指定网页的标题
</head>
<body>                          // <body>标签是一个容器标签，用于放置网页的主体内容
</body>
</html>


<head>是<html>的第一个子元素。如果网页不包含<head>，浏览器会自动创建一个
<head>的子元素一般有下面七个:
<meta>：设置网页的元数据。
<link>：连接外部样式表。
<title>：设置网页标题。
<style>：放置内嵌的样式表。
<script>：引入脚本。
<noscript>：浏览器不支持脚本时，所要显示的内容。
<base>：设置网页内部相对 URL 的计算基准


HTML 语言有自己的空格处理规则。标签内容的头部和尾部的空格，一律忽略不计
标签内容里面的多个连续空格（包含制表符\t），会被浏览器合并成一个，浏览器还会将文本里面的换行符（\n）和回车符（\r），替换成空格(即：HTML 源码里面的换行，不会产生换行效果)

注释以<!--开头，以-->结尾
<!-- 这是一个注释 -->

注释可以是多行的，并且内部的 HTML 都不再生效了
<!--
  <p>hello world</p>
-->


URL 是“统一资源定位符”（Uniform Resource Locator），表示各种资源的互联网地址

URL 由多个部分组成
https://www.example.com:80/path/to/myfile.html?key1=value1&key2=value2#anchor

协议（scheme）是浏览器请求服务器资源的方法                        //  https://
主机（host）是资源所在的网站名或服务器的名字，又称为域名           // www.example.com
端口（port）区分同一个域名下面同时包含多个网站                    // 80
路径（path）是资源在网站的位置                                   // /path/index.html
查询参数（parameter）是提供给服务器的额外信息                     // ?key1=value1&key2=value2 (多组参数之间使用&连接)
锚点（anchor）是网页内部的定位点                                 // #ancho


URL 字符
26个英语字母（包括大写和小写）
10个阿拉伯数字
连词号（-）
句点（.）
下划线（_）

此外，还有18个字符属于 URL 的保留字符，只能在给定的位置出现，网址的其他部分如果要使用这些保留字符，必须使用它们的转义形式
!：%21
#：%23
$：%24
&：%26
'：%27
(：%28
)：%29
*：%2A
+：%2B
,：%2C
/：%2F
:：%3A
;：%3B
=：%3D
?：%3F
@：%40
[：%5B
]：%5D

既不属于合法字符、也不属于保留字符的其他字符（比如汉字），浏览器会自动将它们转义，发给服务器，转义方法是使用这些字符的十六进制 UTF-8 编码，每两位算作一组，然后每组头部添加百分号（%）

www.example.com/中国.html
浏览器会自动将它转义为
www.example.com/%e4%b8%ad%e5%9b%bd.html     // 中的转义形式是%e4%b8%ad，国是%e5%9b%bd


绝对 URL 指的是，只靠 URL 本身就能确定资源的位置
相对 URL 指的是，URL 不包含资源位置的全部信息，必须结合当前网页的位置，才能定位资源


<base>标签指定网页内部的所有相对 URL 的计算基准。整张网页只能有一个<base>标签，而且只能放在<head>里面

<head>
<base href="https://www.example.com/files/" target="_blank">
</head>

<base>标签必须至少具有href属性或target属性之一

一旦设置了<base>，就对整个网页都有效。如果要改变某个链接的行为，只能用绝对链接替代相对链接。尤其需要注意锚点，这时锚点也是针对<base>计算的，而不是针对当前网页的 URL


网页元素的属性（attribute）可以定制元素的行为，不同的属性会导致元素有不同的行为

元素属性的写法是 HTML 标签内部的“键值对”
<html lang="en">
// <html>标签内部的键值对lang="en"，就称为html元素的属性。属性名为lang，属性值为en

<input type="text" required
// 布尔属性的属性值可以省略，只要添加了属性名，就表示打开该属性


全局属性（global attributes）是所有元素都可以使用的属性

id属性是元素在网页内的唯一标识符
id属性的值必须是全局唯一的，同一个页面不能有两个相同的id属性。另外，id属性的值不得包含空格
id属性的值还可以在最前面加上#，放到 URL 中作为锚点，定位到该元素在网页内部的位置 (用户访问网址https://foo.com/index.html#bar的时候，浏览器会自动将页面滚动到bar的位置，让用户第一眼就看到这部分内容)

<p id="p1"></p>
<p id="p2"></p>
<p id="p3"></p>
// 三个<p>标签具有不同的id属性，因此可以区分


class属性用来对网页元素进行分类。如果不同元素的class属性值相同，就表示它们是一类的
<p class="para"></p>
<p></p>
<p class="para"></p>
// 第一个<p>和第三个<p>是一类，因为它们的class属性相同

元素可以同时具有多个 class，它们之间使用空格分隔
<p class="p1 p2 p3"></p>
// p元素同时具有p1、p2、p3三个 class

title属性用来为元素添加附加说明 (鼠标悬浮在元素上面时，会将title属性值作为浮动提示，显示出来)
<div title="版权说明">
  <p>本站内容使用创意共享许可证，可以自由使用。</p>
</div>
// title属性解释了这一块内容的目的


tabindex属性的值是一个整数，表示用户按下 Tab 键的时候，网页焦点转移的顺序
不同的属性值的含义
负整数：该元素可以获得焦点（比如使用 JavaScript 的focus()方法），但不参与 Tab 键对网页元素的遍历。这个值通常是-1。
0：该元素参与 Tab 键的遍历，顺序由浏览器指定，通常是按照其在网页里面出现的位置。
正整数：网页元素按照从小到大的顺序（1、2、3、……），参与 Tab 键的遍历。如果多个元素的tabindex属性相同，则按照在网页源码里面出现的顺序遍历

<p tabindex="0">这段文字可以获得焦点。</p>
// 一般来说，tabindex属性最好都设成0，按照自然顺序进行遍历

accesskey属性指定网页元素获得焦点的快捷键，该属性的值必须是单个的可打印字符，只要按下快捷键，该元素就会得到焦点

<button accesskey="s">提交</button>
// <button>的快捷键是s，按下快捷键，该元素就得到了焦点

accesskey属性的字符键，必须配合功能键，一起按下才会生效，比如，Chrome 浏览器在 Windows 系统和 Linux 系统的快捷键是Alt + 字符键


style属性用来指定当前元素的 CSS 样式
<p style="color: red;">hello</p>

hidden是一个布尔属性，表示当前的网页元素不再跟页面相关
<p hidden>本句不会显示在页面上。</p>
注意，CSS 的可见性设置，高于hidden属性。如果 CSS 设为该元素可见，hidden属性将无效

lang属性指定网页元素使用的语言
<p lang="en">hello</p>
<p lang="zh">你好</p>

dir属性表示文字的阅读方向，有三个可能的值
ltr：从左到右阅读，比如英语。
rtl：从右到左阅读，阿拉伯语、波斯语、希伯来语都属于这一类。
auto：浏览器根据内容的解析结果，自行决定

translate属性只用于文本元素，用来指示翻译软件
<p>
  <span translate="no">Wien<span>
  named world's most liveable city (again)!
</p>
// translate="no"用来告诉翻译软件，不要翻译<span>元素内的文本

HTML 网页的内容默认是用户不能编辑，contenteditable属性允许用户修改内容
<p contenteditable="true">
鼠标点击，本句内容可修改。
</p>


spellcheck属性表示是否打开拼写检查
<p contenteditable="true" spellcheck="true">
英语单词 separate 容易写错成 seperate。
</p>


data-属性用于放置自定义数据。如果没有其他属性或元素合适放置数据，就可以放在data-属性
<a href="#" class="tooltip" data-tip="this is the tip!">链接</a>
// data-tip用于放置链接的提示文字
data-属性只能通过 CSS 或 JavaScript 利用

事件处理属性
onabort, onautocomplete, onautocompleteerror, onblur, oncancel ......


HTML 字符编码
服务器向浏览器发送 HTML 网页文件时，会通过 HTTP 头信息，声明网页的编码方式
Content-Type: text/html; charset=UTF-

网页内部也会再用<meta>标签，再次声明网页的编码
<meta charset="UTF-8">

每个字符有一个 Unicode 号码，称为码点（code point），不是每一个 Unicode 字符都能直接在 HTML 语言里面显示
HTML 允许使用 Unicode 码点表示字符，浏览器会自动将码点转成对应的字符

字符的码点表示法是：
十进制：&#N;
十六进制：&#xN;

<p>hello</p>
<!-- 等同于 -->
<p>&#104;&#101;&#108;&#108;&#111;</p>
<!-- 等同于 -->
<p>&#x68;&#x65;&#x6c;&#x6c;&#x6f;</p>

注意，HTML 标签本身不能使用码点表示

字符的实体表示法
HTML 为一些特殊字符，规定了容易记忆的名字，允许通过名字来表示它们，这称为实体表示法（entity）
<：&lt;
>：&gt;
......


HTML 标签的名称都带有语义（semantic），使用时应该尽量符合标签的语义

// 典型的语义结构的网页
<body>
  <header>页眉</header>
  <main>
    <article>
      <h1>文章标题</h1>
      <p>文章内容</p>
    </article>
  </main>
  <footer>页尾</footer>
</body>

常用标签
<header>标签可以用在多个场景，既可以表示整个网页的头部，也可以表示一篇文章或者一个区块的头部
<footer>标签表示网页、文章或章节的尾部。如果用于整张网页的尾部，就称为“页尾”，通常包含版权信息或者其他相关信息
<main>标签表示页面的主体内容，一个页面只能有一个<main>
<article>标签表示页面里面一段完整的内容，即使页面的其他部分不存在，也具有独立使用的意义，通常用来表示一篇文章或者一个论坛帖子。它可以有自己的标题（<h1>到<h6>）
<aside>标签用来放置与网页或文章主要内容间接相关的部分
<section>标签表示一个含有主题的独立部分，通常用在文档里面表示一个章节，比如<article>可以包含多个<section>
<nav>标签用于放置页面或文档的导航信息
文章的标题:
<h1>：一级标题
<h2>：二级标题
<h3>：三级标题
<h4>：四级标题
<h5>：五级标题
<h6>：六级标题

如果主标题包含多级标题（比如带有副标题），那么可以使用<hgroup>标签，将多级标题放在其中
<hgroup>
  <h1>Heading 1</h1>
  <h2>Subheading 1</h2>
  <h2>Subheading 2</h2>
</hgroup>
注意，<hgroup>只能包含<h1>~<h6>，不能包含其他标签


文本标签
<div>是一个通用标签，表示一个区块（division）
<p>标签是一个块级元素，代表文章的一个段落（paragraph）
<span>是一个通用目的的行内标签（即不会产生换行），不带有任何语义
<br>让网页产生一个换行效果。该标签是单独使用的，没有闭合标签
<wbr>标签跟<br>很相似，表示一个可选的断行。如果一行的宽度足够，则不断行；如果宽度不够，需要断行，就在<wbr>的位置的断行
<hr>用来在一篇文章中分隔两个不同的主题，浏览器会将其渲染为一根水平线
<code>标签是一个行内元素，表示标签内容是计算机代码，浏览器默认会以等宽字体显示，如果要表示多行代码，<code>标签必须放在<pre>内部。<code>本身仅表示一行代码
<kbd>标签是一个行内元素，原意是用户从键盘输入的内容，现在扩展到各种输入，包括语音输入
<samp>标签是一个行内元素，表示计算机程序输出内容的一个例子
<mark>是一个行内标签，表示突出显示的内容
<small>是一个行内标签，浏览器会将它包含的内容，以小一号的字号显示，不需要使用 CSS 样式
<time>是一个行内标签，为跟时间相关的内容提供机器可读的格式
<address>标签是一个块级元素，表示某人或某个组织的联系方式
<abbr>标签是一个行内元素，表示标签内容是一个缩写
<ins>标签是一个行内元素，表示原始文档添加（insert）的内容
<del>表示删除（delete）的内容。它们通常用于展示文档的删改
<dfn>是一个行内元素，表示标签内容是一个术语（definition），本段或本句包含它的定义
<ruby>标签表示文字的语音注释，主要用于东亚文字，比如汉语拼音和日语的片假名
<bdo>标签是一个行内元素，表示文字方向与网页主体内容的方向不一致


列表是一系列排列好的项目，主要分成两类：有序列表和无序列表
// 有序列表
1. 列表项 A
2. 列表项 B
3. 列表项 C

// 无序列表
· 列表项 A
· 列表项 B
· 列表项 C

<ol>标签是一个有序列表容器（ordered list），会在内部的列表项前面产生数字编号
<ol>标签的属性：
reversed属性产生倒序的数字列表
start属性的值是一个整数，表示数字列表的起始编号
type属性指定数字编号的样式：
a：小写字母
A：大写字母
i：小写罗马数字
I：大写罗马数字
1：整数（默认值

<ul>标签是一个无序列表容器（unordered list），会在内部的列表项前面产生实心小圆点，作为列表符号
<li>表示列表项，用在<ol>或<ul>容器之中

<dl>标签是一个块级元素，表示一组术语的列表（description list），<dl>常用来定义词汇表
术语名（description term）由<dt>标签定义
术语解释（description detail）由<dd>标签定义


<img>标签用于插入图片
<img>标签的属性
alt属性用来设定图片的文字说明。图片不显示时，图片的位置上会显示该文本
width属性和height属性可以指定图片显示时的宽度和高度，单位是像素或百分比
......

<figure>标签可以理解为一个图像区块，将图像和相关信息封装在一起，<figcaption>是它的可选子元素，表示图像的文本描述，通常用于放置标题，可以出现多个

网页在不同尺寸的设备上，都能产生良好的显示效果，叫做“响应式设计”（responsive web design）。响应式设计的网页图像，就是“响应式图像”(responsive image)


<img>标签的属性
srcset属性用来指定多张图像，适应不同像素密度的屏幕
像素密度的适配，只适合显示区域一样大小的图像。如果希望不同尺寸的屏幕，显示不同大小的图像，srcset属性就不够用了，必须搭配sizes属性
<img>标签的srcset属性和sizes属性分别解决了像素密度和屏幕大小的适配，但如果要同时适配不同像素密度、不同大小的屏幕，就要用到<picture>标签

图像格式的选择
除了响应式图像，<picture>标签还可以用来选择不同格式的图像。比如，如果当前浏览器支持 Webp 格式，就加载这种格式的图像，否则加载 PNG 图像


<a>标签就代表一个可以跳转的链接。它不仅可以跳转到其他页面，也可以跳转到文本、图像、文件等资源，甚至当前页面的某个位置
<a>标签的属性：
href属性给出链接指向的网址。它的值应该是一个 URL 或者锚点
hreflang属性给出链接指向的网址所使用的语言，纯粹是提示性的，没有实际功能，主要供搜索引擎使用
title属性给出链接的说明信息。鼠标悬停在链接上方时，浏览器会将这个属性的值，以提示块的形式显示出来
target属性指定如何展示打开的链接。它可以是在指定的窗口打开，也可以在<iframe>里面打开
rel属性说明链接与当前页面的关系，常见的rel属性的值：
alternate：当前文档的另一种形式，比如翻译。
author：作者链接。
bookmark：用作书签的永久地址。
external：当前文档的外部参考文档。
help：帮助链接。
license：许可证链接。
next：系列文档的下一篇。
nofollow：告诉搜索引擎忽略该链接，主要用于用户提交的内容，防止有人企图通过添加链接，提高该链接的搜索排名。
noreferrer：告诉浏览器打开链接时，不要将当前网址作为 HTTP 头信息的Referer字段发送出去，这样可以隐藏点击的来源。
noopener：告诉浏览器打开链接时，不让链接窗口通过 JavaScript 的window.opener属性引用原始窗口，这样就提高了安全性。
prev：系列文档的上一篇。
search：文档的搜索链接。
tag：文档的标签链接。

referrerpolicy属性用于精确设定点击链接时，浏览器发送 HTTP 头信息的Referer字段的行为
ping属性指定一个网址，用户点击的时候，会向该网址发出一个 POST 请求，通常用于跟踪用户的行为
type属性给出链接 URL 的 MIME 类型，比如到底是网页，还是图像或文件。它也是纯粹提示性的属性，没有实际功能
download属性表明当前链接用于下载，而不是跳转到另一个 URL

链接也可以指向一个邮件地址，使用mailto协议。用户点击后，浏览器会打开本机默认的邮件程序，让用户向指定的地址发送邮件
如果是手机浏览的页面，还可以使用tel协议，创建电话链接。用户点击该链接，会唤起电话，可以进行拨号


<link>

<link>标签主要用于将当前网页与相关的外部资源联系起来，通常放在<head>元素里面
href属性表示<link>标签所链接的资源
rel属性表示外部资源与当前文档之间的关系，是<link>标签的必需属性，可以视为对href属性所链接资源的说明
hreflang属性用来表示href属性链接资源的所用语言，通常指当前页面的其他语言版本

资源的预加载
浏览器预加载某些资源，等到使用的时候，就不用再从网上下载了，立即就能使用，预处理指令可以做到这一点

预加载主要的五种类型：
<link rel="preload">告诉浏览器尽快下载并缓存资源（如脚本或样式表），该指令优先级较高，浏览器肯定会执行
<link rel="prefetch">的使用场合是，如果后续的页面需要某个资源，并且希望预加载该资源，以便加速页面渲染。该指令不是强制性的，优先级较低，浏览器不一定会执行
<link rel="preconnect">要求浏览器提前与某个域名建立 TCP 连接。当你知道，很快就会请求该域名时，这会很有帮助
<link rel="dns-prefetch">要求浏览器提前执行某个域名的 DNS 解析
<link rel="prerender">要求浏览器加载某个网页，并且提前渲染它。用户点击指向该网页的链接时，就会立即呈现该页面，如果确定用户下一步会访问该页面，这会很有帮助
media属性给出外部资源生效的媒介条件

<script>标签用于在网页插入脚本，<noscript>标签用于指定浏览器不支持脚本时的显示内容
<script>用于加载脚本代码，目前主要是加载 JavaScript 代码
<noscript>标签用于浏览器不支持或关闭 JavaScript 时，所要显示的内容

<video>标签是一个块级元素，用于放置视频。如果浏览器支持加载的视频格式，就会显示一个播放器，否则显示<video>内部的子元素
<audio>标签是一个块级元素，用于放置音频，用法与<video>标签基本一致

<track>标签用于指定视频的字幕，格式是 WebVTT （.vtt文件），放置在<video>标签内部。它是一个单独使用的标签，没有结束标签
<source>标签用于<picture>、<video>、<audio>的内部，用于指定一项外部资源。单标签是单独使用的，没有结束标签

<embed>标签用于嵌入外部内容，这个外部内容通常由浏览器插件负责控制。
<object>标签作用跟<embed>相似，也是插入外部资源，由浏览器插件处理。它可以视为<embed>的替代品，有标准化行为，只限于插入少数几种通用资源，没有历史遗留问题，因此更推荐使用。
<object>标签是一个容器元素，内部可以使用<param>标签，给出插件所需要的运行参数

<iframe>标签生成一个指定区域，在该区域中嵌入其他网页。它是一个容器元素，如果浏览器不支持<iframe>，就会显示内部的子元素
为了限制<iframe>的风险，HTML 提供了sandbox属性，允许设置嵌入的网页的权限，等同于提供了一个隔离层，即“沙箱”
<iframe>指定的网页会立即加载，有时这不是希望的行为。<iframe>滚动进入视口以后再加载，这样会比较节省带宽，loading属性可以触发<iframe>网页的懒加载


表格（table）以行（row）和列（column）的形式展示数据

<table>是一个块级容器标签，所有表格内容都要放在这个标签里面
<caption>总是<table>里面的第一个子元素，表示表格的标题。该元素是可选的

<thead>、<tbody>、<tfoot>都是块级容器元素，且都是<table>的一级子元素，分别表示表头、表体和表尾

<colgroup>是<table>的一级子元素，用来包含一组列的定义。<col>是<colgroup>的子元素，用来定义表格的一列

<tr>标签表示表格的一行（table row）。如果表格有<thead>、<tbody>、<tfoot>，那么<tr>就放在这些容器元素之中，否则直接放在<table>的下一级

<th>和<td>都用来定义表格的单元格。其中，<th>是标题单元格，<td>是数据单元格


表单（form）是用户输入信息与网页互动的一种形式。大多数情况下，用户提交的信息会发给服务器，比如网站的搜索栏就是表单
表单由一种或多种的小部件组成，比如输入框、按钮、单选框或复选框。这些小部件称为控件（controls）

<form>标签用来定义一个表单，所有表单内容放到这个容器元素之中
<form>表单的enctype属性，指定了采用 POST 方法提交数据时，浏览器给出的数据的 MIME 类型

<fieldset>标签是一个块级容器标签，表示控件的集合，用于将一组相关控件组合成一组

<legend>标签用来设置<fieldset>控件组的标题，通常是<fieldset>内部的第一个元素，会嵌入显示在控件组的上边框里面

<label>标签是一个行内元素，提供控件的文字说明，帮助用户理解控件的目的

<input>标签是一个行内元素，用来接收用户的输入
type属性决定了<input>的形式。该属性可以取以下值：
text 是普通的文本输入框，用来输入单行文本
search 是一个用于搜索的文本输入框，基本等同于type="text"
button 是没有默认行为的按钮，通常脚本指定click事件的监听函数来使用
submit 是表单的提交按钮。用户点击这个按钮，就会把表单提交给服务器
image 表示将一个图像文件作为提交按钮，行为和用法与type="submit"完全一致
reset 是一个重置按钮，用户点击以后，所有表格控件重置为初始值
checkbox 是复选框，允许选择或取消选择该选项
radio 是单选框，表示一组选择之中，只能选中一项
email 是一个只能输入电子邮箱的文本输入框。表单提交之前，浏览器会自动验证是否符合电子邮箱的格式，如果不符合就会显示提示，无法提交到服务器
password 是一个密码输入框。用户的输入会被遮挡，字符通常显示星号（*）或点（·）
file 是一个文件选择框，允许用户选择一个或多个文件，常用于文件上传功能
hidden 是一个不显示在页面的控件，用户无法输入它的值，主要用来向服务器传递一些隐藏信息
number 是一个数字输入框，只能输入数字
range 是一个滑块，用户拖动滑块，选择给定范围之中的一个数值
url 是一个只能输入网址的文本框
tel 是一个只能输入电话号码的输入框
color 是一个选择颜色的控件，它的值一律都是#rrggbb格式
date 是一个只能输入日期的输入框，用户可以输入年月日，但是不能输入时分秒
time 是一个只能输入时间的输入框，可以输入时分秒，不能输入年月日
month 是一个只能输入年份和月份的输入框，格式为YYYY-MM
week 是一个输入一年中第几周的输入框。格式为yyyy-Www
datetime-local 是一个时间输入框，让用户输入年月日和时分，格式为yyyy-MM-ddThh:mm


<button>标签会生成一个可以点击的按钮，没有默认行为，通常需要用type属性或脚本指定按钮的功能

<select>标签用于生成一个下拉菜单

<option>标签用在<select>、<optgroup>、<datalist>里面，表示一个菜单项

<datalist>标签是一个容器标签，用于为指定控件提供一组相关数据

<textarea>是一个块级元素，用来生成多行的文本框

<output>标签是一个行内元素，用于显示用户操作的结果

<progress>标签是一个行内元素，表示任务的完成进度。浏览器通常会将显示为进度条

<meter>标签是一个行内元素，表示指示器，用来显示已知范围内的一个值，很适合用于任务的当前进度、磁盘已用空间、充电量等带有比例性质的场


其他标签
<dialog>标签表示一个可以关闭的对话框

<dialog>元素的 JavaScript API 提供Dialog.showModal()和Dialog.close()两个方法，用于打开/关闭对话框

<dialog>元素有两个事件，可以监听
1. close：对话框关闭时触发
2. cancel：用户按下esc键关闭对话框时触发

<details>标签用来折叠内容，浏览器会折叠显示该标签的内容
Details元素的open属性返回<details>当前是打开还是关闭
```