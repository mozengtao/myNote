# Grep 命令详细使用指南

## 基本语法
```bash
grep [选项] 模式 文件名
```

## 常用选项详解

### 1. -E (扩展正则表达式)
启用扩展正则表达式，等同于 egrep
```bash
grep -E "(cat|dog)" file.txt
```

### 2. -F (固定字符串)
将模式视为固定字符串，而非正则表达式，等同于 fgrep
```bash
grep -F "hello world" file.txt
```

### 3. -i (忽略大小写)
匹配时忽略大小写
```bash
grep -i "ERROR" logfile.txt
```

### 4. -v (反向匹配)
显示不匹配的行
```bash
grep -v "^#" config.txt
```

### 5. -o (仅输出匹配部分)
只显示匹配的部分，而不是整行
```bash
grep -o "https://[^[:space:]]*" webpage.html
```

### 6. -n (显示行号)
在输出中显示匹配行的行号
```bash
grep -n "function" script.js
```

### 7. -r (递归搜索)
递归搜索目录中的所有文件
```bash
grep -r "TODO" /path/to/project/
```

### 8. --include (包含文件模式)
只在匹配指定模式的文件中搜索
```bash
grep --include="*.py" -r "import" /path/to/project/
```

### 9. --exclude (排除文件模式)
排除匹配指定模式的文件
```bash
grep --exclude="*.log" -r "error" /path/to/project/
```

### 10. -A (显示匹配行之后的行)
显示匹配行及其后面的N行
```bash
grep -A 3 "Exception" error.log
```

### 11. -B (显示匹配行之前的行)
显示匹配行及其前面的N行
```bash
grep -B 2 "FATAL" error.log
```

### 12. -C (显示匹配行前后的行)
显示匹配行及其前后各N行
```bash
grep -C 2 "WARNING" error.log
```

### 13. -w (整词匹配)
只匹配完整的单词
```bash
grep -w "test" file.txt
```

### 14. -q (静默模式)
不输出匹配内容，只返回退出状态
```bash
grep -q "success" result.txt && echo "Found success"
```

### 15. -c (计数)
只显示匹配行的数量
```bash
grep -c "error" logfile.txt
```

### 16. -l (显示文件名)
只显示包含匹配内容的文件名
```bash
grep -l "main" *.c
```

### 17. --color (彩色输出)
高亮显示匹配的内容
```bash
grep --color=always "pattern" file.txt
```

## 高级用法

### 正则表达式分组
使用括号创建分组，便于后续引用
```bash
grep -E "([0-9]{1,3}\.){3}[0-9]{1,3}" access.log
```

### 反向引用
在正则表达式中引用前面的分组
```bash
grep -E "([a-zA-Z]+)\1" file.txt  # 匹配重复的单词
```

### 提取分组内容
结合 -o 选项提取特定分组的内容
```bash
echo "Email: user@example.com" | grep -oE "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}"
```

### 命名分组 (使用 perl 兼容正则)
```bash
grep -P "(?P<year>\d{4})-(?P<month>\d{2})-(?P<day>\d{2})" dates.txt
```

### 复杂模式组合示例

#### 1. 匹配IP地址
```bash
grep -E "^([0-9]{1,3}\.){3}[0-9]{1,3}$" ip_list.txt
```

#### 2. 匹配邮箱地址
```bash
grep -E "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$" emails.txt
```

#### 3. 匹配电话号码
```bash
grep -E "^1[3-9][0-9]{9}$" phones.txt
```

#### 4. 在日志中查找特定时间范围的错误
```bash
grep -E "2024-03-(2[0-9]|3[01]).*ERROR" application.log
```

#### 5. 提取URL中的域名
```bash
grep -oE "https?://([^/]+)" urls.txt | grep -oE "([^/]+)$"
```

## 实用组合示例

### 1. 查找包含特定函数的Python文件
```bash
grep --include="*.py" -rn "def.*function_name" /project/
```

### 2. 排除注释行和空行
```bash
grep -v "^#\|^$" config.txt
```

### 3. 统计代码行数（排除注释和空行）
```bash
grep -v "^#\|^$\|^[[:space:]]*#" *.py | wc -l
```

### 4. 查找多个模式
```bash
grep -E "(error|warning|fatal)" -i logfile.txt
```

### 5. 在压缩文件中搜索
```bash
zgrep "pattern" compressed_file.gz
```

### 6. 使用管道组合多个grep
```bash
ps aux | grep python | grep -v grep
```

## 性能优化技巧

### 1. 使用 -F 进行固定字符串搜索（更快）
```bash
grep -F "exact_string" large_file.txt
```

### 2. 限制搜索范围
```bash
grep --include="*.log" --exclude-dir=".git" -r "pattern" .
```

### 3. 使用 -m 限制匹配数量
```bash
grep -m 10 "pattern" huge_file.txt  # 找到10个匹配后停止
```

## 常见错误和解决方案

### 1. 特殊字符转义
```bash
grep "\." file.txt  # 匹配实际的点号
grep "\$" file.txt  # 匹配美元符号
```

### 2. 空格处理
```bash
grep "hello world" file.txt      # 包含空格的搜索
grep -E "hello[[:space:]]+world" # 匹配多个空格
```

### 3. 多行匹配
```bash
grep -Pzo "pattern1.*\n.*pattern2" file.txt  # 跨行匹配
```

## 总结

grep是Linux/Unix系统中最强大的文本搜索工具之一。掌握这些选项和技巧可以大大提高文本处理和日志分析的效率。建议根据具体需求组合使用不同选项，以达到最佳效果。

对于复杂的正则表达式，建议先在小范围内测试，确保模式正确后再应用到大文件或生产环境中。