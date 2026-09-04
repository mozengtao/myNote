[Regular Expressions Cheat Sheet](https://www.dataquest.io/cheat-sheet/regular-expressions-cheat-sheet/)  
[regex](https://www.rexegg.com/)  
[Regular Expression HOWTO](https://docs.python.org/3/howto/regex.html)  
[Regex Cheat Sheet](https://techearl.com/regex-cheat-sheet)  
[Regular-Expressions.info](https://www.regular-expressions.info/)  
[TextFSM](https://github.com/google/textfsm/wiki/TextFSM)  
[RegexOne](https://regexone.com/)  
[Regular Expressions Quick Reference](https://www.regular-expressions.info/refquick.html)  
[regex101](https://regex101.com/)  
[re — Regular expression operations](https://docs.python.org/3/library/re.html)  
[]()  

| Regex      | 名称     | 含义                   | 示例                     | 匹配示例                 |               |
| ---------- | ------ | -------------------- | ---------------------- | -------------------- | ------------- |
| `.`        | 任意字符   | 匹配除 `\n` 外任意一个字符     | `a.c`                  | `abc`、`axc`          |               |
| `*`        | 零次或多次  | 前一个表达式重复 **0~N** 次   | `ab*`                  | `a`、`ab`、`abbb`      |               |
| `+`        | 一次或多次  | 前一个表达式重复 **1~N** 次   | `ab+`                  | `ab`、`abbbb`         |               |
| `?`        | 零次或一次  | 前一个表达式重复 **0 或 1** 次 | `ab?`                  | `a`、`ab`             |               |
| `{n}`      | 固定次数   | 恰好重复 n 次             | `\d{4}`                | `2025`               |               |
| `{m,n}`    | 范围次数   | 重复 m~n 次             | `\d{1,3}`              | `1`、`12`、`123`       |               |
| `[]`       | 字符集合   | 匹配集合中的任意一个字符         | `[A-Z]`                | `A`、`F`              |               |
| `[^...]`   | 否定字符集合 | 匹配**不属于**集合中的任意一个字符  | `[^ ]+`                | `Ethernet0/0`        |               |
| `()`       | 捕获组    | 分组，并保存匹配内容           | `(GigabitEthernet\d+)` | `GigabitEthernet0`   |               |
| `(?:...)`  | 非捕获组   | 分组，但**不保存**匹配内容      | `(?:up\|down)`         | `up`、`down`          |               |
| `\|`（正则为 ` | `）     | 或                    | 左右表达式二选一               | `up\|down`           | `up` 或 `down` |
| `^`        | 行首     | 匹配一行开始               | `^Interface`           | `Interface Gi0/0`    |               |
| `$`        | 行尾     | 匹配一行结束               | `state$`               | `line state`         |               |
| `\d`       | 数字     | 等价 `[0-9]`           | `\d+`                  | `123`                |               |
| `\D`       | 非数字    | 等价 `[^0-9]`          | `\D+`                  | `abc`                |               |
| `\w`       | 单词字符   | 字母、数字、下划线            | `\w+`                  | `eth0`               |               |
| `\W`       | 非单词字符  | 非 `\w`               | `\W+`                  | `:/-`                |               |
| `\s`       | 空白字符   | 空格、Tab、换行等           | `\s+`                  | `"   "`              |               |
| `\S`       | 非空白字符  | 非空白字符                | `\S+`                  | `GigabitEthernet0/1` |               |



## 正则表达式在线编辑器
[regexr](https://regexr.com/)  
[Regulex](https://jex.im/regulex/#!flags=&re=%5E(a%7Cb)*%3F%24)  
[regex101](https://regex101.com/)  

[Regular Expressions](https://www.grymoire.com/Unix/Regular.html#toc_Regular_Expressions)  
[RegEX   cheatsheet](https://quickref.me/regex.html)  
[Regular Expression Reference](https://www.regular-expressions.info/refcapture.html)  

## 匹配规则
```bash
# 基础元字符
.		匹配任意单个字符​（除换行符）			a.c			  "abc", "a3c"
\		转义字符（使特殊字符变为普通字符）	   a\.txt		 "a.txt"
^		匹配字符串开始位置					  ^start   		"start..."
$		匹配字符串结束位置					  end$	   		"...end"
|		或逻辑（匹配左右任意模式）			   cat|dog		"cat","dog"


# 字符类
[abc]	匹配括号内任意一个字符				 [aeiou]			"a", "e"
[^abc]	匹配不在括号内的任意字符			 [^0-9]				"a", "%"
[a-z]	匹配范围内的字符​（连续ASCII）        [A-F0-9]			 "B", "5"
\d		匹配数字​（等同 [0-9]）               \d{3}				"123", "456"
\w		匹配单词字符​（字母、数字、下划线）    \w+				  "user123", "a"
\s		匹配空白符​（空格、制表符等）          \s+				  " ", "\t"

# 量词
*		匹配前一个元素0次或多次				 ab*c				"ac", "abbc"
+		匹配前一个元素1次或多次              a+					"a", "aaa"
?		匹配前一个元素0次或1次               colou?r			"color", "colour"
{n}		匹配前一个元素恰好n次                \d{4}				"2023"
{n,}	匹配前一个元素至少n次                a{2,}				"aa", "aaaa"
{n,m}	匹配前一个元素n到m次                 \w{3,5}			"abc", "12345"

# 分组与捕获
(pattern)	​捕获分组，可后续引用或提取			(ab)+			   "abab"
(?:pattern)	​非捕获分组​（不保存匹配内容）		 (?:http|ftp)://	"http://", "ftp://"
\1, \2	​	 反向引用分组内容					(\d)\1			  "11", "22"

# 边界与断言
\b			匹配单词边界​（字母与非字母间）		\bcat\b				"cat" 不匹配 "category"
\B			匹配非单词边界					  \Bcat\B			  "category" 中的 "cat"
(?=pattern)	​正向先行断言​（右侧需匹配）		  Windows(?=10)		 "Windows" 后跟 "10"
(?!pattern)	​负向先行断言​（右侧不匹配）		  Windows(?!7)		 "Windows" 后不跟 "7"

# 贪婪与懒惰匹配
.*		​贪婪匹配​（尽可能长）	a.*b	   "aXbYb" → "aXbYb"
.*?	​	 懒惰匹配​（尽可能短）	a.*?b	   "aXbYb" → "aXb"

# 不同工具的正则差异
工具/场景		正则类型			特性说明
​sed			基本正则（BRE）		需转义 +, ?, | 等
​grep -E		扩展正则（ERE）		直接支持 +, ?, |
​awk			扩展正则（ERE）		支持分组、量词
​Perl/Python	PCRE			   支持懒惰匹配、断言、Unicode
```