## Grep Multiple Patterns
```bash
grep 'pattern1\|pattern2' fileName_or_filePath
grep -E 'pattern1|pattern2' fileName_or_filePath
grep -e pattern1 -e pattern2 fileName_or_filePath
```
## 只打印匹配行的匹配字段
```bash
grep -o '(.*)'
grep -o '([^)]*)'  # 如果单行有多个括号，用于多个匹配的多个分行输出
```
## 匹配行上下文打印
```bash
grep -A NUM file	# After
grep -B NUM file	# Before
grep -C NUM file	# Center
```

## 常用选项
```bash
-E, --extended-regexp
	Interpret PATTERNS as extended regular expressions (EREs, see below)
-q, --quiet, --silent
	Quiet; do not write anything to standard output.  Exit immediately with zero status if any match is found, even if an error was detected.  Also see the -s or --no-messages option.
-o, --only-matching
	Print only the matched (non-empty) parts of a matching line, with each such part on a separate output line.
-e PATTERN, --regexp=PATTERN
	Use  PATTERN  as  the  pattern.  If this option is used multiple times or is combined with the -f (--file) option, search for all patterns given. This option can be used to protect a pattern beginning with “-”.
-w, --word-regexp
	Select  only  those  lines  containing matches that form whole words. 
```

[man grep](https://man7.org/linux/man-pages/man1/grep.1.html)  
[[rg]]  
[Matching Multiple Strings Using Only One grep](https://www.baeldung.com/linux/grep-matching-multiple-strings)  