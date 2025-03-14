## sed 是如何工作的
![how sed works]((./assets/Pasted_image_20221029213136_1668150554863_0.png))  
```bash
# Read --> Execute --> Print
   |                     |
   <----------------------
          Repeat

1. Read a line into the pattern space (an internal temporary sed buffer)
2. Execute the sed commands one by one in sequence on the line in the sed pattern space
3. Print the line from the pattern space. After printing this line, the sed pattern space will be empty
4. Repeat from step 1 until the end of the input file is reached

```bash
# 替换
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
sed -n '/DATA BEGIN/, /DATA END/{ /DATA END/!p}' input.txt
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
