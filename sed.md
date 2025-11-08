[That's What I sed](https://objectcomputing.com/resources/publications/sett/august-2019-thats-what-i-sed)  
[]()  
[]()  
[]()  
[]()  

## sed 是如何工作的
![how sed works]((./assets/Pasted_image_20221029213136_1668150554863_0.png))  

  Read --> Execute --> Print
   |                     |
   <----------------------
          Repeat

1. Read a line into the pattern space (an internal temporary sed buffer)
2. Execute the sed commands one by one in sequence on the line in the sed pattern space
3. Print the line from the pattern space. After printing this line, the sed pattern space will be empty
4. Repeat from step 1 until the end of the input file is reached

## cheat-sheet
Read line → PS (Pattern Space)
Commands applied in order
Default: print PS
-n : suppress default printing
d : delete PS, goto next cycle
n : output PS (unless -n), read next input into PS, skip rest
c\text : replace whole line with text, skip remaining commands
s/old/new/flags : edit PS in-place (can use g, p, i flags)
N / D : multi-line PS operations

```bash
# by default, sed is greedy
echo "a (one) b (two) c" | sed -n 's/.*(\(.*\)).*/\1/p'		# OUTPUT: "one) b (two"

# simulate non-greedy behavior by telling sed explicitly what not to cross
echo "a (one) b (two)" | sed -n 's/.*(\([^)]*\)).*/\1/p'	# OUTPUT: "one"
# explanation:
	| Regex part  | Meaning                                                              |
	| ----------- | -------------------------------------------------------------------- |
	| `.*(`       | Match everything before the first `(`                                |
	| `\([^)]*\)` | Capture all characters that are **not `)`** — so it stops before `)` |
	| `.*`        | Ignore the rest                                                      |
	| `\1`        | Replace with the captured part                                       |

# multiple matches -extract all
echo "a (one) b (two) c (three)" |
  sed 's/([^)]*)/\n&/g' |
  sed -n 's/.*(\([^)]*\)).*/\1/p'
# first sed: inserts newline before each (...) match (& stands for whole match)
# second sed: extracts content inside each (...)

# no \ is needed after | because | tells the shell: "expect another command next." (a new line is treated like a space)

# operators that don't need \
# Bash (and POSIX shells) have a formal grammar that defines certain “compound command separators” and “control operators.”, When the shell parser encounters one of these tokens at the end of a line,it knows the command is not complete — so it automatically continues reading

|						# Pipe stdout → stdin
&&, ||					# logical operators
|&						# pipe stdout+stderr
;						# sequential
&						# Run command in background
do, then, else, elif,	# Shell keywords (blocks, loops, conditionals)
fi, done, esac, }
(, {, [, [[				# Grouping or conditional opening

# Any operator or keyword that opens a new syntactic construct or expects a command after it implies continuation automatically.

echo "hello" |
  tr a-z A-Z

test -f /etc/passwd &&
  echo "exists" ||
  echo "missing"

echo start ;
  echo done

sleep 1 &
  echo "backgrounded"

make |&
  tee build.log

if true; then
  echo "yes"
else
  echo "no"
fi

In shell, any token that clearly tells the parser “more command is coming” (like |, &&, ;, do, then) makes newline = space.
Otherwise, newline = end of command — unless escaped with \


# command line options
sed -e or --expression {script}
sed -f or --file {script-file}
sed -h or --help
sed -i or --in-place
-n or --quiet or --silent
sed -E or -r or --regexp-extended
sed -s or --separate
sed --version

# regex
. matches any character
# repetition
{n}		# n times
{n,}	# at least n times
{m, n}	# at least m times and not more than n times
?		# zero or one (optional) and is the same as {0, 1}
*		# zero or more and is the same as {0,}
+		# one or more and is the same as {1,}

x? matches an empty string or "x"
x* matches an empty string, "x", "xx", "xxx" and so on
x+ matches "x", "x", "xxx", and so on, but not an empty string
x{3} matches "xxx", exactly three consecutive "x" characters
x{3, } matches three or more consecutive "x" characters
x{, 3} matches zero to three consecutive "x" characters
x{3, 5} matches three to five consecutive "x" characters

# character class
[pie]
[a-d]
[0-9][a-f]
[^aeiou]
[^a-f]
\([^)*]\) matches zero or more characters surrounded by parentheses
/_[^_]+_/
\w is a "word character" and has the same meaning as [A-Za-z0-9_]
\W means not a word character

# Posix character classes [[:name:]]
[[:lower:]] which is equivalent to [a-z]
[[:upper:]] which is equivalent to [A-Z]
[[:digit:]] which is equivalent to [0-9]
[[:alpha:]] which is equivalent to [a-zA-Z]
[[:alnum:]] which is equivalent to [a-zA-Z0-9]
[[:blank:]] which is equivalent to [ \t]

# grouping and back references
/([0-9]{3}).+\1/

# anchors
/^two/
/two$/
/^two$/

# alternatives
# The patterns before and after the | can be any kind of regular expression pattern, not just literal text.
# The order of precedence in regular expressions is grouping, concatenation (characters that must follow others), and alternatives.
/red|green|blue/
/(home|work) address/

# regex flags
g for global matching
i for case-insensitive matching

# exmaples
"
/[0-9]{5}/
/\(\d{3}\)\d{3}-\d{4}/
/[A-Z]['a-z]* ([A-Z]['a-z]*)? [A-Z]['a-z]*/\
"

# pattern space
# hold space

# substitute
sed 's/red/blue/; s/1/one/' input.txt
sed -e 's/red/blue/' -e 's/1/one/' input.txt

# word boundary markers
# beginning of a word
\< or \b

# end of a word
\> or \b

\B represents a position that is not a word boundary

sed -s -i '/\bred\b/blue/g; s/\b1\b/one/g' input.txt

# remove all trailing spaces and tabs
sed -E -i 's/[ \t]+$//' input.txt

# substitution metacharacters
&		insert entire match
\l		change next character to lowercase
\u		change next character to uppercase
\L		change remainder to lowercase up to next \U or \E
\U		change remainder to lowercase up to next \L or \E
\E		ends effect of \L or \U
back references(ex. \1)		insert match for a given group

seq 3 | sed -E 's/[0-9]+/score: &/'
echo "mark volkmann" | sed -E 's/(\w+) (\w+)/\u\1 \U\2/'
echo "mARK vOLKMANN" | sed -E 's/(\w)(\w*) (\w)(\w*)/\u\1\L\2\E \u\3\L\4/'

# substitute command flags
a number		changes a specific match (ex. 2 for only the second)
g				substitute globally (all occurrences)
i				ignore case
m				enables multiline mode
e				executes replacement as a shell command if matched
p				prints replacement if matched
w {file-path}	writes replacement to the file if matched

# any number of these flags can be combined. If the w flag is used, it must come last because it is followed by a file path
s/foo/bar/3g	# replaces all occurrences of "foo" with "bar" starting with the third occurrence

# in multiline mode
^ matches the start of each line.
$ matches the end of each line.
` matches the start of the first line.
' matches the end of the last line.										'

sed -e 's/red/blue/' -f script1.sed -e 's/one/two/g' -f script2.sed my-input.txt

# sed command
# address command options

# address
a line number
a range of line numbers
a regular expression to match
a pair of regular expressions that identify the first and last lines of ranges
a line number and a regular expression
a regular expression and a number preceded by "+"
a line number and a step count separated by "~"
$

7
8,12
/ice cream/
/April/, /July/
7, /April/
/April/, +3
5~3				on line 5 and every line that is a multiple of three lines after that (5, 8, 11, ...)
$				only on the last line

# Adding an exclamation mark to the end of any address negates it.
7!			every line except line 7
/April/!	every line that does not contain "April."
$!			every line except the last

# When a regular expression is used as an address, subsequent commands that use a regular expression can omit it to default to the same regular expression.
`/^[A-Z][0-9]{3}/ s//hide/`

# common commands
a text		append text after normal output
c text		change normal output to text
d			delete the pattern space (clears it)
D			delete only the first line in the pattern space
i text		insert text before normal output
s/regex/subex/flags	substitutes text with different text

=			prints current line number followed by a newline
e			executes the pattern space as a shell command
p			prints the pattern space (useful when AutoPrint is off)
P			prints only the first line (up to newline) in the pattern space
r {file}	reads content of a file and prints it
R {file}	reads and prints the next line of a file
w {file}	appends the pattern space to given file
W {file}	appends first line (up to newline) of the pattern space to given file
y			replaces given characters with others (transliterates)

# control flow commands
:label		defines label that can be targeted by b and t commands
b			branches to end of sed script
b label		branches to a label
l			prints the pattern space in a special format for debugging
n			reads next line into the pattern space
N			appends next line into the pattern space preceded by a newline
q			prints the pattern space and quits without processing remaining lines
Q			quits without printing the pattern space or processing remaining lines
t			branches to end of sed script if substitution was performed
t label		branches to a label if substitution was performed
T			branches to end of sed script if substitution was NOT performed
T label		branches to a label if substitution was NOT performed
z			clears the pattern space

# hold space commands
h	copies the pattern space to the hold space
H	appends the pattern space to the hold space
g	copies the hold space to the pattern space
G	appends the hold space to the pattern space
x	exchanges the pattern space and the hold space

# command syntax
[address]command[arguments]
or
[address1],[address2]command[arguments]

sed 's/foo/bar/' file.txt
sed '/pattern/s/foo/bar/' file.txt
sed '1,5s/foo/bar/' file.txt
sed '/begin/,/end/d' file.txt
sed '/pat/{cmd1; cmd2}' file.txt
sed '$p' file.txt
```

```bash
# 替换
echo "name=alice" | sed 's/^name=\(.*\)$/The name is \1/'
sed -i 's/\r$//' INPUTFILE	# remove ^M (CR)
sed -i 's/SEARCH_REGEX/REPLACEMENT/g' INPUTFILE
sed -i 's/\bfoo\b/linux/g' file.txt		# use the word-boundary expression (\b) at both ends of the search string. This ensures 
the partial words are not matched.
sed -i 's/foo/linux/gI' file.txt		# make the pattern match case insensitive
sed -i 's/\/bin\/bash/\/usr\/bin\/zsh/g' file.txt
sed -i 's|/bin/bash|/usr/bin/zsh|g' file.txt	# use any other character as delimiter character
sed -i 's/\b[0-9]\{3\}\b/number/g' file.txt
sed -i 's/\b[0-9]\{3\}\b/{&}/g' file.txt

sed -n '1p; 3p; 5p' file	# 打印不连续的若干行
sed -n '/GAINSPEED-\(ENVMON\|MEMORY\|PROCESS\)/Ip' XXX		#忽略大小写
echo "We felt happy because we saw the others were happy." | sed 's/\bwe\b/you/g'	# Replace Using Word Boundary

# 使用shell环境变量
var=abc; echo abc | sed "s/$var/& &/g"		# 直接使用双引号
var=abc; echo abc | sed 's/'"$var"'/& &/g'	# 单独使用双引号

echo "pattern 123 456" | sed '/pattern/ s/.*/xxxx/'		# 匹配后替换整行

## 匹配两个pattern之间的行
# 包含 BEGIN 和 END pattern的边界
sed -n '/DATA BEGIN/, /DATA END/p' input.txt
# 只包含 BEGIN 边界
sed -n '/DATA BEGIN/, /DATA END/{ /DATA END/!p}' input.**txt**
# 只包含 END 边界
sed -n '/DATA BEGIN/, /DATA END/{ /DATA BEGIN/!p}' input.txt
# 不包含 BEGIN 和 END pattern 的边界
sed -n '/DATA BEGIN/, /DATA END/{ /DATA BEGIN/! {/DATA END/!p}}' input.txt
```

## pattern space 和 hold space
```bash
# pattern space
	sed逐行处理文本，每行内容临时存放到pattern space，模式空间的内容会自动打印，之后被自动清空，用来接收下一行输入
# hold space
	该缓冲区的内容不会自动清空，也不会自动打印，需要sed的命令来进行处理

# pattern space 和 hold space 相关的操作命令
d
	删除pattern space的内容，开始下一行处理
h H
	复制/追加pattern space的内容到hold space
g G
	复制/追加hold space的内容到pattern space
x
	交换hold space和pattern space的内容
```
[sed](https://www.gnu.org/software/sed/manual/sed.html)  

[sedsed](https://github.com/aureliojargas/sedsed) Debugger and code formatter for sed scripts

[How to Use sed to Find and Replace String in Files](https://linuxize.com/post/how-to-use-sed-to-find-and-replace-string-in-files/)  
[Regular Expression Syntax](https://www.gnu.org/software/sed/manual/html_node/Regular-Expressions.html#Regular-Expressions)  
[Sed and Awk 101 Hacks](https://vds-admin.ru/sed-and-awk-101-hacks)  
[sed, a stream editor](https://www.gnu.org/software/sed/manual/sed.html)  
[Introduction to Sed](https://www.grymoire.com/Unix/Sed.html#toc_Table_of_Contents)  
[Handy one-liners for SED](https://edoras.sdsu.edu/doc/sed-oneliners.html)  
[Awk and Sed One-Liners Explained](https://www.osnews.com/story/21004/awk-and-sed-one-liners-explained/)  
[sed & awk](https://doc.lagout.org/operating%20system%20/linux/Sed%20%26%20Awk.pdf)  
[sed & awk](https://docstore.mik.ua/orelly/unix/sedawk/) #online  
[sed and awk](https://cs.nyu.edu/~mohri/unix08/lect5.pdf)  
[Sed - An Introduction and Tutorial by Bruce Barnett](https://www.grymoire.com/Unix/Sed.html#toc_Sed_-_An_Introduction_and_Tutorial_by_Bruce_Barnett)  
[How to Match a Pattern “N” Times Using sed](https://www.baeldung.com/linux/sed-match-pattern-n-times)  
[How to Replace Text After a Specific Word Using  sed](https://www.baeldung.com/linux/sed-replace-text-after-word)  
