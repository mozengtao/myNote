- ```bash
  # print single quote in awk
  #1 use octal escape sequences
  awk '{print "\047" $0 "\047"}' input
  
  #2 pass the quote as a variable
  awk -v q="'" '{print q $0 q}' input
  ```
- how awk works
	- ![Awk work example](https://vds-admin.ru/sites/default/files/article_images/sedawk101hacks004.jpg){:height 419, :width 360}
- 使用shell变量
	- -v 选项
		- `awk -v today="$(date)" -v weekday="Friday" 'BEGIN { print today " is " weekday}'`
	- 命令行参数
		- `awk 'BEGIN { print ARGV[1] " " ARGV[2]}' $arg1 $arg2`
		- `seq 3 | awk '{ print var }' var="hello"`
	- shell环境变量
	  collapsed:: true
		- `awk 'BEGIN { print ENVIRON["HOME"] }'`
- awk 注释
  collapsed:: true
	- 单行注释
	  ```awk
	  # 在这里处理单行的数据
	  print("Hello Comments!"); # 这里也可以写
	  ```
	- 多行注释
	  ```awk
	  0 {
	      use me 4 comment in head
	      but only in English 
	      no awk keyword can appear in there
	  }
	   
	  BEGIN{
	  }
	  {
	      if(0) {
	              at code should use me
	              print("我是不会被执行的");
	      }
	      print("我才会被执行");
	  }
	  END{
	  }
	  ```
- 调用系统命令
	- `echo "Test string" | awk '{ cmd=sprintf("echo %s", $0); system(cmd); }'`
- 正则表达式作为域分隔符
	- `echo "xxAA xxBxx C" | awk -F '(^x+)|( x)' '{ for(i = 1; i <= NF; i++) printf "-->%s<--\n", $i }'`
	  ```sh
	  --><--
	  -->AA<--
	  -->xxBxx<--
	  -->C<--
	  ```
- 打印两个pattern之间的行
	- 包含 BEGIN 和 END pattern的边界
	  collapsed:: true
		- `awk '/DATA BEGIN/, /DATA END/' input.txt`
	- 只包含 BEGIN 边界
	  collapsed:: true
		- `awk '/DATA BEGIN/{ flag = 1 } /DATA END/{ flag = 0 } flag' input.txt`
	- 只包含 END 边界
	  collapsed:: true
		- `awk 'flag; /DATA BEGIN/{ flag = 1 } /DATA END/{ flag = 0 }' input.txt`
	- 不包含 BEGIN 和 END pattern 的边界
	  collapsed:: true
		- `awk '/DATA BEGIN/{ flag = 1; next } /DATA END/{ flag = 0 } flag' input.txt`
- getline语句
  collapsed:: true
	- `getline`
		- 读取下一条记录保存到`$0`，同时更新 NF, NR, FNR, RT
	- `getline <file`
		- 从文件读取下一条记录保存到$0，同时更新NF，RT
	- `getline var`
		- 读取下一条记录保存到var，同时更新 NR, FNR, RT
	- `getline var <file`
		- 从文件读取下一条记录保存到var，同时更新  RT
	- `command | getline [var]`
	  background-color:: red
		- 运行命令并将管道的输出保存到 $0 或者 var
		  background-color:: red
			- 举例
			  ```awk
			  BEGIN {
			         command = "date"
			         if ((command | getline date_now) < 0) {
			           print "Can't get system date" > "/dev/stderr"
			           exit 1
			         }
			         close(command)
			         print "current date is", date_now
			  }
			  ```
			- 注意：command 两侧要用双引号
			  background-color:: red
- AWK 脚本示例
	- ```awk
	  #!/usr/bin/awk -f
	  ```
	- 示例1
	  collapsed:: true
		- ```sh
		  BEGIN {
		  	printf("%-8s%-8s%-8s%-8s%-8s\n", "SLOT", "CM", "MTA", "STB", "EROUTER")
		  	print "-------|-------|-------|-------|-------"
		  }
		  
		  /show cable summary/ {
		  	flag=1
		  	next
		  }
		  
		  NF>9 && $1~/^1/ {
		  	slot=$1
		  
		  	CM[slot]+=strtonum($5)
		  	MTA[slot]+=strtonum($6)
		  	STB[slot]+=strtonum($7)
		  	EROUTER[slot]+=strtonum($8)
		  
		  	if(0) {
		  		if(flag=1)
		  			printf("%-8s%-8s%-8s%-8s%-8s%-8s%-8s%-8s\n", $1, $2, $3, $4, $5, $6, $7, $8)
		  	}
		  }
		  
		  /ok/ {
		  	flag=0
		  }
		  
		  END {
		  	for( key in CM ) {
		  		printf("%-8s%-8s%-8s%-8s%-8s\n", elem, CM[key], MTA[key], STB[key], EROUTER[key])
		  	}
		  }
		  ```
	- 示例2
		- ```awk
		  /inventory device-type/, /inventory eSAFE-config/ {
		  	#print $0
		  	switch($2) {
		  		case "model":
		  			modem["model"]=$3
		  			break;
		  		case "software-vers":
		  			modem["software-vers"]=$3
		  			break;
		  		case "boot-rom-vers":
		  			modem["boot-rom-vers"]=$3
		  			break;
		  		case "vendor-name":
		  			modem["vendor-name"]=$3
		  			break;
		  		case "eSAFE-config":
		  			printf "%-20s%-45s%-35s%-20s\n", modem["model"], modem["software-vers"], modem["boot-rom-vers"], modem["vendor-name"]
		  			cmcnt[modem["model"]]++
		  			delete modem
		  			break;
		  		default:
		  			# do nothing
		  			break;
		  	}
		  }
		  
		  END {
		  	print "-------------------------------------"
		  	printf "%-20s%8s\n", "model", "count"
		  	for ( model in cmcnt ) {
		  		printf "%-20s%8s\n", model, cmcnt[model]
		  	}
		  	print "-------------------------------------"
		  }
		  ```
	- 示例3
	  collapsed:: true
		- ```awk
		  BEGIN {
		  	PAT_PREFIX="4491.2.1.28.1.10.1"
		  
		  	listmibs="4,5"
		  	nmib=split(listmibs, mib, ",")
		  
		  	listchans="3,112"
		  	nchan = split(listchans, chan, ",")
		  
		  	listprofiles="0,1,2,3,255"
		  	nprofile = split(listprofiles, profile, ",")
		  
		  	for(m = 1; m <= nmib; m++)
		  		for(c = 1; c <= nchan; c++)
		  			for(p = 1; p <= nprofile; p++)
		  				pat[m, c, p] = PAT_PREFIX "." mib[m] "." chan[c] "." profile[p]
		  }
		  
		  /SNMPv2-SMI::enterprises/{
		  	for(m = 1; m <= nmib; m++)
		  		for(c = 1; c <= nchan; c++)
		  			for(p = 1; p <= nprofile; p++) {
		  				n = split($1, tmparr, ".")
		  				if($1 ~ pat[m, c, p] && tmparr[n] == profile[p]) {
		  
		  					if(m == 1 && c == 1 && p == 1) {
		  						cnt = 1;
		  						outstr = ""
		  					}
		  
		  					row[cnt++] = $NF
		  
		  					if(m == nmib && c == nchan && p == nprofile) {
		  						outstr = row[1]
		  						for(col = 2; col <= length(row); col++)
		  							outstr = outstr "," row[col]
		  						print outstr
		  
		  						delete row
		  					}
		  				}
		  			}
		  }
		  ```
-
- 参考文档
	- [Idiomatic awk](https://backreference.org/2010/02/10/idiomatic-awk/index.html)
	- [#awk](http://awk.freeshell.org/)
	- [Sed and Awk 101 Hacks](https://vds-admin.ru/sed-and-awk-101-hacks)
	- [AWK实战](https://book.saubcy.com/AwkInAction/HOWTO.html)
	- [The GNU Awk User's Guide](https://ftp.gnu.org/old-gnu/Manuals/gawk-3.1.1/html_node/)
	- [sed & awk](https://docstore.mik.ua/orelly/unix/sedawk/index.htm)
	- [awk reference](https://www3.physnet.uni-hamburg.de/physnet/Tru64-Unix/HTML/APS32DTE/WKXXXXXX.HTM)
	- [awk learnbyexample](https://learnbyexample.github.io/learn_gnuawk/awk-introduction.html)
	- [sed & awk](https://doc.lagout.org/operating%20system%20/linux/Sed%20%26%20Awk.pdf)
	- [CLI text processing with GNU awk](https://learnbyexample.github.io/learn_gnuawk/cover.html)
	- [Understanding Regular Expressions in Awk](https://tecadmin.net/awk-regular-expressions/)
		```bash
		Metacharacter	Description
		.				Matches any single character
		[ ]				Matches any character within the brackets
		^				Matches the beginning of a line
		$				Matches the end of a line
		*				Matches zero or more occurrences of the previous character
		+				Matches one or more occurrences of the previous character
		?				Matches zero or one occurrence of the previous character

		# Matching a Regular Expression
			awk '{
			if (match($0, /\.com$/)) {
				print $0
			}
			}' email.txt

		# Replacing a Regular Expression
			awk '{
			sub(/555/, "666", $0)
			print $0
			}' phone.txt

		# Grouping
		we have a file containing a list of employee names and salaries, and we want to extract the names and salaries separately.
			awk '{
			if (match($0, /^(\w+)\s+(\d+)$/)) {
				name = substr($0, RSTART, RLENGTH)
				salary = substr($0, RSTART+length(name)+1, length($0)-RSTART-length(name))
				print name
				print salary
			}
			}' employees.txt

		# Backreferences
		You can use backreferences (i.e., \1, \2, etc.) to refer to parts of the regular expression that were matched by a group. This allows you to reuse matched substrings in the replacement string.
		we have a file containing a list of phone numbers in the format (XXX) XXX-XXXX, and we want to change the format to XXX-XXX-XXXX.
			awk '{
			sub(/\((\d{3})\) (\d{3})-(\d{4})/, "\1-\2-\3", $0)
			print $0
			}' phone.txt

		# Lookahead and Lookbehind
		You can use lookahead (?=) and lookbehind (?<=) to match patterns only if they are followed by or preceded by another pattern, respectively.
		we have a file containing a list of URLs, and we want to extract only the domain names (i.e., the text between “http://” and the next “/” character).
			awk '{
			if (match($0, /(?<=http:\/\/)[^\/]+/)) {
				print substr($0, RSTART, RLENGTH)
			}
			}' urls.txt
		Here, we use lookahead (?<=) to match the regular expression "/(?<=http://)[^/]+/" (which matches any characters that come after "http://" and before the next "/" character) and extract the domain name.

		# Negated character classes
		we have a file containing a list of email addresses, and we want to extract only the addresses that belong to a specific domain (e.g., example.com). 
		awk '{
			if (match($0, /^[^@]+@example\.com$/)) {
				print $0
			}
			}' emails.txt
		we use a negated character class ([^@]+) to match any characters that are not "@" and extract the username, and then match the literal string "@example.com" to ensure that the address belongs to the specified domain.

		# Alternation
		we have a file containing a list of phone numbers, and we want to extract only the numbers that are either in the format "(XXX) XXX-XXXX" or "XXX-XXX-XXXX". 
			awk '{
			if (match($0, /\((\d{3})\) (\d{3})-(\d{4})|(\d{3})-(\d{3})-(\d{4})/)) {
				print substr($0, RSTART, RLENGTH)
			}
			}' phones.txt
		we use alternation (|) to match either the regular expression "/(\d3)(\d3) (\d{3})-(\d{4})/" (which matches a phone number in the format (XXX) XXX-XXXX) or the regular expression "/(\d{3})-(\d{3})-(\d{4})/" (which matches a phone number in the format XXX-XXX-XXXX).
		```
	- [AWK: String functions](https://tecadmin.net/awk-string-functions/)
		```bash
		length(string): 
			Returns the length of the specified string.
		substr(string, start, length): 
			Returns a substring of the specified string, starting at the specified position and with the specified length.
		index(string, substring): 
			Returns the position of the first occurrence of the specified substring in the specified string.
		split(string, array, separator): 
			Splits the specified string into an array of substrings, using the specified separator to determine where to split the string.
		sub(regexp, replacement, string): 
			This replaces the first occurring regular expression match from the string with “replacement”.
		gsub(regexp, replacement, string): 
			Replaces all occurrences of the specified regular expression in the specified string with the specified replacement string.
		match(string, regexp): 
			Searches the specified string for the first occurrence of the specified regular expression, and returns the position of the match and the length of the matched substring in an array.
		tolower(string) and toupper(string): 
			Converts all uppercase or lowercase characters in the specified string to lowercase or uppercase characters, respectively.
	```