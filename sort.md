- 以行为单位对文本文件进行排序
-
- 常用选项
	- `-n`
		- 按照字符串的数字值进行比较
	- `-k`
		- 按照指定的key值(列)进行排序
	- `-r`
		- 按照相反的条件进行比较
	- -t
		- 指定域分隔符
-
- 应用举例
	- 按照第2列的数字大小进行排序
		- `sort -n -k 2 file.txt`
-
- 帮助文档
	- [man sort](https://man7.org/linux/man-pages/man1/sort.1.html)