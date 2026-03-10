> read 从标准输入（键盘）或文件描述符中读取一行文本，并将其赋值给一个或多个变量

## IFS
> IFS（Internal Field Separator，内部字段分隔符）决定了 read 如何将输入的一整行字符串“切开”并分配给不同的变量
> IFS 的默认值是 空格（Space）、制表符（Tab） 和 换行符（Newline）
```bash
# 指定自定义分隔符
IFS=":" read -r user:home
echo "用户 $user 的主目录是 $home"

# IFS=（设为空值）的作用: 完整保留行首和行尾的空格
普通 read line：会把行首和行尾的空格删掉（Trim）
IFS= read -r line：原封不动地读取整行内容

# 局部作用域（临时修改）: 将 IFS 写在 read 命令之前，它只对当前的这一行命令生效
# 这种写法很安全，不会改变全局的 IFS 变量
IFS="," read -r col1 col2 <<< "data1,data2"
```

## Usage
```bash
# -r: 不解释反斜杠 \，将其视为普通字符（推荐用法）
read -r line

# -p: 显示提示信息
read -p "请输入您的姓名: " name

# -s: 输入时不显示字符（常用于输入密码）
read -s -p "请输入密码: " password

# -n: 限制读取字符数。读取满 N 个字符后自动结束，无需按回车
read -n 1 -p "确认吗(y/n)?" ans

# -t: 设置超时时间。超过指定秒数未输入则自动停止
read -t 5 -p "请在5秒内输入: " input

# -a: 读入数组。将输入的内容按分隔符拆分到数组中
read -a words

# 读取多个变量
# 输入：Alice 25 Beijing
read name age city
echo "$name is $age years old and lives in $city."

# 配合 while 循环读取文件
# 推荐使用 -r 选项防止路径中的反斜杠被转义
while read -r line; do
    echo "处理行内容: $line"
done < test.txt

# 自定义分隔符 (IFS)
# 读取以冒号分隔的系统用户信息
IFS=":" read -r username password uid gid info home shell <<< "root:x:0:0:root:/root:/bin/bash"
echo "用户 $username 的主目录是 $home"

# -d: read with a delimiter
read -d ';' -p 'Enter a string terminated by a semicolon: ' string
echo "You entered: $string"

# read multiple variables
read -r -p 'Enter three variables: ' var1 var2 var3
echo "You entered: $var1, $var2, $var3"

# read a line
read -r -p 'Enter a line: ' line
echo "You entered: $line"

# restore IFS
OLDIFS=$IFS
IFS=','
read -r -p 'Enter 3 comma-separated values: ' first second third
echo "You entered: $first, $second, $third"
IFS=$OLDIFS

# read from a file
while IFS= read -r line; do
  echo "$line"
done < input.txt

# read from a pipe
seq 3 | while IFS= read -r line; do
  echo "$line"
done

# 从文件描述符读取
while IFS= read -r line; do
  echo "$line"
done < <(seq 3)

# 从管道读取
find . -type f -name "*.c" -print0 | while IFS= read -r -d '' file; do
  echo "$file"
done

# read from a pipe
ls -1 | while IFS= read -r file; do
  echo "$file"
done

# 菜单示例
echo "1) Option 1"
echo "2) Option 2"
echo "3) Option 3"
echo "q) Quit"

while true; do
  read -n 1 -p "Enter your choice(1-3 or q): " choice
  echo
  case $choice in
    1) echo "You chose Option 1";;
    2) echo "You chose Option 2";;
    3) echo "You chose Option 3";;
    q) echo "Quitting"; exit 0;;
    *) echo "Invalid choice";;
  esac
done

# 确认
read -p 'Continue? (Yy/Nn): ' continue
if [[ $continue =~ [Yy] ]]; then
  echo "Continuing"
else
  echo "Exiting"
  exit 0
fi

# 读取 CSV 文件
while IFS=',' read -r col1 col2 col3; do
  echo "Column 1: $col1, Column 2: $col2, Column 3: $col3"
done < input.csv

# special variable
read -p 'input:'
echo "You entered: $REPLY"

# read into variables
read -r first rest <<< "This is a test"
echo "First: $first, Rest: $rest"

# here-string
read -r var1 var2 <<< "hello world"
echo "var1: $var1, var2: $var2"

# here-document
read -r first_line <<'EOF'
This is the first line
This is the second line
This is the third line
EOF

echo "First line: $first_line"
```

```bash
for file in $(find . -name '* *.png'); do
  echo $file;
done

问题: $(command)的结果会被 shell 进行分词（根据 IFS变量，默认是空格、制表符、换行符分割）,所以 "file name.png"被分割成了2个单词:"file", "name"

# 解决方案
find . -name '* *.png' | while IFS= read -r file; do
    echo "$file"
done

# or

while IFS= read -r file; do
    echo "$file"
done < <(find . -name '* *.png')

# or

# 将结果存入数组
mapfile -t files < <(find . -name '* *.png')

# 遍历数组
for file in "${files[@]}"; do
    echo "$file"
done

# or

# 临时修改 IFS
OLDIFS=$IFS
IFS=$'\n'

for file in $(find . -name '* *.png' | sed 's/.\/assets\///'); do
    echo "$file"
done

IFS=$OLDIFS
```
