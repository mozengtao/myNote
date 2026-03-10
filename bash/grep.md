> grep is used to print lines that match patterns
[grep(1)](https://www.mankier.com/1/grep)  
[Regular Expressions in Grep (Regex)](https://linuxize.com/post/regular-expressions-in-grep/)..
[Regular expressions in grep ( regex ) with examples](https://www.cyberciti.biz/faq/grep-regular-expressions/)..
[]()..
[]()..
[]()..
```bash
# (A|B)：扩展正则的分组或逻辑，匹配 A 或 B
# \.：正则中.是通配符（匹配任意单个字符），需转义为字面量的点
# 55[45]：中括号[]匹配其中任意一个字符
snmpwalk -v 2c -c public $SNMPNSI IF-MIB:ifTable -M /home/tcao/mibs | grep -E '(ifOperStatus|ifAdminStatus)\.55[45]'
IF-MIB::ifAdminStatus.554 = INTEGER: down(2)
IF-MIB::ifAdminStatus.555 = INTEGER: down(2)
IF-MIB::ifOperStatus.554 = INTEGER: down(2)
IF-MIB::ifOperStatus.555 = INTEGER: down(2)

grep 'pattern1\|pattern2' file # Basic OR
grep -E 'pattern1|pattern2' file # Extended regex (recommended)

grep -e pattern1 -e pattern2 file # Multiple -e options
grep -iE 'pattern1|pattern2' file # Case-insensitive OR
grep -wE 'pattern1|pattern2' file # OR with whole word matching
grep -E '(fatal|critical) error' file.txt # Match "fatal" or "critical" followed by "error"

## 常用选项
-E, --extended-regexp
-q, --quiet, --silent
-o, --only-matching
-e PATTERN, --regexp=PATTERN
-w, --word-regexp
--color
i, --ignore-case
-v, --invert-match

# regex in grep
3 regular expression syntaxes are supported:
	1. Basic Regular Expressions (BRE)
	2. Extended Regular Expressions (ERE)
	3. Perl Compatible Regular Expressions (PCRE)

# 在文件中指定pattern
grep -f include.txt input.txt
grep -v -f exclude.txt input.txt


# awk version
awk '
    /abc/ { next }
    /hij/ { next }
    { print }
' input.txt

# grep -Ev
grep -Ev 'abc|hij' input.txt
grep -Ev '(abc|hij)' input.txt      # Parentheses only matter for grouping when mixed with surrounding patterns


# grep -E
echo "foo abc bar" | grep -E 'foo (abc|hij) bar'
echo "foo hij bar" | grep -E 'foo (abc|hij) bar'


echo "foo abc hij bar" | grep -E 'foo abc|hij bar'
# Output:
foo abc hij bar

# literal match
grep bash /etc/passwd

# anchoring
grep '^linux' file.txt
grep 'linux$' file.txt
grep '^linux$' file.txt

# match any single character
grep 'l.nux' file.txt

# match a group of characters
grep 'l[ai]nux' file.txt
grep 'co[^l]a' file.txt
grep '^[A-Z]' file.txt

# predefined classes of characters
[:alnum:]	Alphanumeric characters.
[:alpha:]	Alphabetic characters.
[:blank:]	Space and tab.
[:digit:]	Digits.
[:lower:]	Lowercase letters.
[:upper:]	Uppercase letters.
[:punct:]
[:space:]

# quantifiers
*		Match the preceding item zero or more times.
?		Match the preceding item zero or one time.
+		Match the preceding item one or more times.
{n}		Match the preceding item exactly n times.
{n,}	Match the preceding item at least n times.
{,m}	Match the preceding item at most m times.
{n,m}	Match the preceding item from n to m times.

#  match lines that starts with capital letter and ends with either period or comma
grep -E '^[A-Z].*[.,]$' file.txt

# basci regular expression
grep 'b\?right' file.txt
grep 'fatal\|error\|critical' /var/log/nginx/error.log

# extended regular expression
grep -E 'b?right' file.txt
grep -E '[[:digit:]]{3,9}' file.txt
grep -E 'fatal|error|critical' /var/log/nginx/error.log

# group patterns together and reference them as one item
# match both “fearless” and "less"
grep -E '(fear)?less' file.txt

# special backslash expressions
\w : Word character [a-zA-Z0-9] OR a _ (underscore)
\W : [^\w] Inverse of \w, any non-word character
\s : Spaces, tabs, in some contexts new-lines
\S : [^\s] Inverse of \s, any non-space character
\b : Boundary between adjacent word and space, 0-length anchor (Perl regex)
\B : [^\b] In the middle of a word or multiple spaces, 0-length anchor (Perl regex)
\< : Boundary at start of word, 0-length anchor
\> : Boundary at end of word, 0-length anchor
refer back to an exact copy of a matched (group) using \1, \2, etc..

echo "hello world!" | grep -Po '\<world\>'	# world
echo "hello world!" | grep -Eo '\<world\>'	# world

echo 'word1 word_2 thirdWord' | grep -E -o '^\w+\s+\w+'	# word1 word_2
echo 'word1 word_2 thirdWord!?' | grep -E -o '\s\w+\s'	# (space) word_2 (space)
echo 'word1 word_2 thirdWord!?' | grep -E -o '\S+'		# word1
														# word_2
														# thirdWord!?
echo 'word1 word_2 thirdWord!?' | grep -E -o '.+\<'		# word1 word_2

echo 'word1 word_2 thirdWord!?' | grep -P -o '\b\w+\b'	# word1
														# word_2
														# thirdWord

echo 'word1 word_2 thirdWord!?' | grep -E -o '\<\w+\>'	# word1
														# word_2
														# thirdWord

echo 'word1 word_2 thirdWord!?' | grep -P -o '\B\w+\B'	# ord
														# ord_
														# hirdWor

echo 'blah1 blah2 blah2 blah4?' | grep -E -o '(\w+)\s+\1'				# blah2 blah2
echo 'blah1 blah2 blah2 blah4?' | grep -E -o '([a-z]+)([0-9])\s+\1\2'	# blah2 blah2
echo 'ABCDEFGGFEDCBA' | grep -E -o '(\w)(\w)(\w)\3\2\1'					# EFGGFE

# match separate words abject and object
grep '\b[ao]bject\b' file.txt


## Grep Multiple Patterns
grep 'pattern1\|pattern2' fileName_or_filePath
grep -E 'pattern1|pattern2' fileName_or_filePath
grep -e pattern1 -e pattern2 fileName_or_filePath

## 只打印匹配行的匹配字段
# 注意：'grep -o' 打印 entire match, 而不是 capture groups
echo "123 (abc) 456" | grep -o -E '\(.*\)'          # output: (abc) 而不是 abc

echo "123 (abc) 456" | sed -n 's/.*(\(.*\)).*/\1/p' # output: abc
    # \( ... \) is used to define a capture group (in basic sed regex, parentheses must be escaped)
    123 (abc) 456
    ^^^ ^^^^ ^^^^
    |   |    |
    |   |    └── .* (after closing parenthesis)
    |   └────── \(.*\)  → captured text: abc
    └────────── .* (before opening parenthesis)

    Entire match (.*(\(.*\)).*) → "123 (abc) 456"
    Captured group (\(.*\)) → "abc"
    Replacement \1 → "abc"


grep -o '(.*)'
grep -o '([^)]*)'  # 如果单行有多个括号，用于多个匹配的多个分行输出

#
email="john.doe@example.com"
domain=$(echo $email | grep -Eo '[^@[:space:]]+\.[^@[:space:]]+$')
echo "The domain is $domain"	# The domain is example.com

## 匹配行上下文打印
grep -A NUM file	# After
grep -B NUM file	# Before
grep -C NUM file	# Center

grep 'vivek' /etc/passwd
grep -w '^vivek' /etc/passwd
grep -i -w 'vivek' /etc/passwd
grep -E -i -w 'vivek|raj' /etc/passwd
grep -E -i '^(linux|unix)' filename
grep '[vV]ivek' filename
grep '[vV][iI][Vv][Ee][kK]' filename
grep -w '[vV]ivek[0-9]' filename
grep 'foo[0-9][0-9]' filename
grep '[vV]ivek[^0-9]' test
grep -E '[[:digit:]]{1,3}\.[[:digit:]]{1,3}\.[[:digit:]]{1,3}\.[[:digit:]]{1,3}' file
grep --color regex filename

grep '^foo' filename
grep 'foo$' filename
grep '^foo$' filename
grep '^$' filename
```