- How to Grep Multiple Patterns
	- ```bash
	  grep 'pattern1\|pattern2' fileName_or_filePath
	  grep -E 'pattern1|pattern2' fileName_or_filePath
	  grep -e pattern1 -e pattern2 fileName_or_filePath
	  (egrep 'pattern1|pattern2' fileName_or_filePath
	  The egrep command is an outdated version of extended grep. It does the same function 
	  as grep -E.)
	  ```
- 只打印匹配行的匹配字段
	- `grep -o '(.*)'`
	- `grep -o '([^)]*)'`    如果单行有多个括号，用于多个匹配的多个分行输出
- 匹配行上下文打印
	- 打印匹配行后若干行: `grep -A NUM file`
	- 打印匹配行前若干行: `grep -B NUM file`
	- 打印匹配行前后若干行: `grep -C NUM file`
- 常用选项
	- ```bash
	         -E, --extended-regexp
	                Interpret PATTERNS as extended regular expressions (EREs,
	                see below)
	         -q, --quiet, --silent
	                Quiet; do not write anything to standard output.  Exit
	                immediately with zero status if any match is found, even
	                if an error was detected.  Also see the -s or
	                --no-messages option.
	         -o, --only-matching
	                Print only the matched (non-empty) parts of a matching
	                line, with each such part on a separate output line.
	  ```
- 参考文档
	- [man grep](https://man7.org/linux/man-pages/man1/grep.1.html)
	- [[rg]]
	- [Matching Multiple Strings Using Only One grep](https://www.baeldung.com/linux/grep-matching-multiple-strings)
-
-