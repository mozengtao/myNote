# Web 自动化逆向分析（Web Reverse Engineering）完整分析流程及最佳实践

> **核心目标：**
>
> 不关注网页"长什么样"，而关注**浏览器到底向服务器发送了什么请求（HTTP Request）**。
>
> 一旦能够复现 HTTP 请求，就可以完全脱离浏览器，实现 Python/Go/C++ 等自动化控制。

---

# 一、核心心智模式（Mental Model）

很多人认为：

```
点击按钮
    │
    ▼
浏览器执行魔法
```

实际上应该理解为：

```
用户点击按钮
        │
        ▼
JavaScript / HTML Form
        │
        ▼
HTTP Request
        │
        ▼
服务器
        │
        ▼
HTTP Response
        │
        ▼
页面更新
```

所以：

> **真正需要逆向的不是网页，而是 HTTP 协议。**

---

# 二、Web 自动化分析黄金流程

```
浏览器(F12)
       │
       ▼
Network
       │
       ▼
找到目标HTTP请求
       │
       ▼
分析Request
       │
       ▼
分析Response
       │
       ▼
Copy as cURL
       │
       ▼
curl验证
       │
       ▼
Python requests
       │
       ▼
自动化程序
```

---

# 三、第一步：定位目标操作

不要一打开网页就抓包。

而是明确：

```
我到底想自动化什么？
```

例如：

```
登录

↓

修改配置

↓

点击Apply

↓

删除对象

↓

上传文件

↓

下载日志
```

每一种操作最终都会对应一个 HTTP Request。

---

# 四、第二步：打开 Developer Tools

Chrome / Edge

```
F12
```

切换到：

```
Network
```

建议勾选：

```
✓ Preserve log

✓ Disable cache
```

然后：

```
Clear Network
```

保证只有本次操作产生的请求。

---

# 五、第三步：重新执行操作

例如：

```
选择Outlet

↓

Off Immediate

↓

Apply
```

Network 中会新增请求。

注意：

**不要刷新页面。**

很多目标请求只出现一次。

---

# 六、第四步：找到真正的请求

一般先找：

```
POST
```

例如：

```
POST /Forms/rpduconf1
```

如果没有：

再找：

```
GET
```

很多老设备根本没有 Ajax。

而是：

```
HTML Form

↓

GET

↓

302

↓

新页面
```

---

# 七、第五步：理解 Request

重点分析：

```
Request URL

Method

Headers

Cookies

Payload(Form Data)

Query String
```

例如：

```
POST /Forms/rpduconf1

Cookie:
APCSESSION=xxxx

Payload:

Outlet=14

Action=Off

Delay=0
```

真正决定业务行为的是：

```
Payload
```

---

# 八、第六步：理解 Response

观察：

```
200 OK

302 Redirect

303 See Other

401 Unauthorized

403 Forbidden
```

例如：

```
POST

↓

303

↓

GET 新页面
```

说明：

服务器处理成功。

---

# 九、第七步：查看 Cookies

很多自动化失败都是因为：

```
Cookie

Session

CSRF Token
```

例如：

```
Cookie:

SESSION=xxxxx
```

Python 必须携带：

```python
requests.Session()
```

而不是每次 requests.get()

---

# 十、第八步：分析 Payload

典型形式：

## Form Data

```
Outlet=14

Action=Off
```

或者：

```
id=14

state=0
```

---

## JSON

```
{
    "id":14,
    "state":"off"
}
```

---

## Query String

```
GET

/outlet?id=14&state=off
```

---

# 十一、第九步：分析隐藏字段

很多网页都有：

```html
<input type="hidden">
```

例如：

```html
<input
    name="csrf"
    value="abcdef">
```

或者：

```
token

nonce

timestamp
```

这些都必须携带。

---

# 十二、第十步：Copy as cURL

Chrome：

```
右键请求

↓

Copy

↓

Copy as cURL
```

得到：

```bash
curl \
  -H 'Cookie: ...' \
  --data '...'
```

这是浏览器真实发送的数据。

---

# 十三、第十一步：验证 curl

终端：

```bash
curl ...
```

如果成功：

说明：

```
协议已经逆向完成
```

---

# 十四、第十二步：翻译为代码

Python：

```python
import requests

session = requests.Session()

session.post(
    url,
    headers=headers,
    cookies=cookies,
    data=data
)
```

Go：

```go
http.Client{}
```

C++：

```
libcurl
```

原理完全一致。

---

# 十五、如果没有 POST 怎么办？

很多老设备：

```
<form method="GET">
```

例如：

```
GET

/outlet.htm

?action=off

&id=14
```

不要先入为主认为：

```
一定是 POST
```

---

# 十六、如果没有 Ajax 怎么办？

很多 Schneider APC

Cisco

HP

Dell

老设备都是：

```
HTML Form

↓

POST

↓

303

↓

GET
```

整个过程没有：

```
XHR

Fetch
```

属于正常现象。

---

# 十七、如何分析 HTML

Elements：

找到：

```html
<form>
```

例如：

```html
<form
    method="POST"
    action="/Forms/rpduconf1">
```

然后：

```html
<input
    name="Outlet">

<input
    name="Action">
```

协议已经暴露。

---

# 十八、如何分析 JavaScript

Sources：

搜索：

```
fetch(

XMLHttpRequest(

axios

submit(

onclick
```

找到：

```
POST哪个URL

发送哪些字段

Header有哪些
```

---

# 十九、如何寻找真正控制设备的请求

很多页面：

```
加载CSS

加载JS

加载图片

加载字体

刷新状态

真正控制命令
```

真正需要的是：

```
点击按钮

↓

新增请求

↓

Method

↓

Payload
```

而不是：

```
favicon.ico

style.css

logo.png
```

---

# 二十、常见HTTP状态码

```
200 OK
```

请求成功。

---

```
302 Found
```

跳转。

---

```
303 See Other
```

POST成功后跳转。

很多老设备都这样。

---

```
401 Unauthorized
```

未登录。

---

```
403 Forbidden
```

权限不足。

---

```
500 Internal Server Error
```

服务器异常。

---

# 二十一、如何判断是否已经逆向成功？

满足以下全部条件：

```
✓ 找到目标Request

✓ 理解URL

✓ 理解Method

✓ 理解Payload

✓ 理解Cookie

✓ 理解Token

✓ curl可复现

✓ Python requests可复现
```

即可认为：

```
Web协议已经逆向完成。
```

---

# 二十二、最佳实践（Best Practices）

## ① 永远从 Network 开始

不要先研究 JavaScript。

Network 是最终事实来源。

---

## ② 每次只分析一个动作

例如：

```
登录

↓

抓包

↓

完成
```

不要一次抓：

```
登录

修改

Apply

刷新

注销
```

否则请求太多。

---

## ③ 每次 Clear

保证：

```
一次动作

对应

几条请求
```

容易定位。

---

## ④ 先理解 HTTP，再写代码

不要一开始就 Selenium。

90% 的后台管理页面：

```
requests

+

Session

+

POST
```

即可完成。

---

## ⑤ 先验证 curl，再写程序

流程：

```
Browser

↓

Copy as cURL

↓

curl

↓

Python

↓

Go

↓

C++
```

每一步都可验证。

---

## ⑥ 使用 Session

不要：

```python
requests.post()
```

而是：

```python
session = requests.Session()
```

保持：

```
Cookie

登录状态
```

---

## ⑦ 保留原始请求

保存：

```
Copy as cURL
```

以后：

```
升级固件

调试

回归测试
```

都非常有价值。

---

# 二十三、统一心智模型（黄金法则）

```
不要逆向网页。

要逆向HTTP协议。

────────────────────────────

浏览器只是HTTP客户端。

网页只是HTTP请求的生成器。

真正需要自动化的是：

        URL
        +
      Method
        +
      Headers
        +
      Cookies
        +
      Payload

一旦能够准确复现HTTP请求，

浏览器就可以完全退出整个自动化流程。
```