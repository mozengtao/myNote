
[Bash Function & How to Use It](https://phoenixnap.com/kb/bash-function)  
[Shell Parameter Expansion](https://www.gnu.org/software/bash/manual/html_node/Shell-Parameter-Expansion.html)  
[Notes](https://johannst.github.io/notes/intro.html#notes)  
[GNU bash shell tips](http://molk.ch/tips/gnu/bash/index.html)  
[]()  
[How to Use Regex in Bash Scripting](https://labex.io/tutorials/shell-how-to-use-regex-in-bash-scripting-392579)  
[Bash Features](https://www.gnu.org/software/bash/manual/html_node/)  
[]()  
[]()  
[IPC Performance Comparison: Anonymous Pipes, Named Pipes, Unix Sockets, and TCP Sockets](https://www.baeldung.com/linux/ipc-performance-comparison)  

## use env var for consistent command usage
```bash
export SNMPNAME="snmp-evc-morris-dentist-1"
nomad alloc status $(nomad job allocs $SNMPNAME | awk '/snmp/{print $1}') 2>/dev/null  | awk '/snmp-nsi-port/{print $3}'
```

## command
```bash
command cmd args...     # run cmd, skipping aliases/functions
command -v cmd          # print path or name if found; exit 0/1
command -V cmd          # verbose “type” info
command -p cmd args...  # search using a standard PATH

# Bypass an alias or function wrapper
# alias grep='grep --color=auto' or a grep() function exists
command grep -n pattern file        # runs real grep, no alias/function

# Check if a command exists
if ! command -v jq >/dev/null 2>&1; then
  echo "Please install jq"
fi

# Get what will actually run
command -V ls
# command -V ls       -> ls is aliased to `ls --color=auto'

# Use a predictable PATH (scripts, restricted envs)
command -p mkdir -p -- "$dest"
command -p rm -rf -- "$dir"

# Avoid recursive calls inside wrappers
mv() { echo "logging..."; command mv "$@"; }
```

## default paramers
```bash
#/usr/bin/env bash

# Default if unset or empty
greet() {
  local name=${1:-world}
  echo "Hello, $name"
}
# greet         -> Hello, world
# greet Alice   -> Hello, Alice

# Default only if unset (empty string allowed)
demo() {
  local val=${1-DEFAULT}
  printf '[%s]\n' "$val"
}
# demo          -> [DEFAULT]
# demo ""       -> []

# Multiple defaults
build() {
  local target=${1:-all}
  local jobs=${2:-4}
  echo "target=$target jobs=$jobs"
}
# build          -> target=all jobs=4
# build clean    -> target=clean jobs=4
# build new 2    -> target=new jobs=2

# Default from environment, overridable by arg
run() {
  local threads=${1:-${THREADS:-4}}
  echo "threads=$threads"
}
# THREADS=8 run       -> threads=8
# run 16              -> threads=16

# With getopts (flags + defaults)
deploy() {
  local env=staging timeout=30
  while getopts "e:t:" opt; do
    case $opt in
      e) env=$OPTARG ;;
      t) timeout=$OPTARG ;;
    esac
  done
  echo "env=$env timeout=$timeout"
}
# deploy                   -> env=staging timeout=30
# deploy -e prod -t 60     -> env=prod timeout=60

# Default list/array when no args
process() {
  local args=("$@")
  if [ ${#args[@]} -eq 0 ]; then
    args=(*.txt)
  fi
  printf '%s ' "${args[@]}"
}
# process                         -> 
# process file1.txt file2.txt     -> file1.txt file2.txt
```

## alias workaround for string which has more than 1 word
```bash
git() {
    if [[ "$1" == "review" ]]; then
        shift
        # run git review -t <current branch>
        command git review -t "$(git branch --show-current | xargs basename)" "$@"
    else
        command git "$@"
    fi
}

# 2
git config --global alias.review '!f() { git review -t "$(git branch --show-current | xargs basename)"; }; f'


```

## /usr/bin/env
```bash
# Advantage: it doesn't hardcode the interpreter path

#!/usr/bin/env python3

1. env search the PATH for python3
2. execute it with the script as an argument


```


## struct of fields
```bash
# 1
#/usr/bin/env bash

declare -A entry1=(
  [cmd]="echo"
  [file]="/tmp/file1"
)

declare -A entry2=(
  [cmd]="ls"
  [file]="/tmp/file2"
)

records=(entry1 entry2)

for rec in "${records[@]}"; do
  declare -n curr_rec="$rec"
  cmd=${curr_rec[cmd]}
  file=${curr_rec[file]}
  if ! command -v $cmd > /dev/null 2>&1; then
    echo "$cmd does not exist"
    exit 1
  fi
  $cmd $file
done

# 2
#/usr/bin/env bash

records=(
  "cmd=echo file=/tmp/file1"
  "cmd=ls file=/tmp/file2"
)

for rec in "${records[@]}"; do
  eval "$rec"
  if ! command -v $cmd > /dev/null 2>&1; then
    echo "$cmd does not exist"
    exit 1
  fi
  $cmd $file
done

# 3
# cmds.json
[
  { "cmd": "echo", "file": "/tmp/file1" },
  { "cmd": "ls",   "file": "/tmp/file2" }
]

#/usr/bin/env bash
set -euo pipefail

json_file="cmds.json"

if [[ ! -f "$json_file" ]]; then
  echo "JSON file '$json_file' not found!"
  exit 1
fi

jq -c '.[]' "$json_file" | while read -r entry; do
  cmd=$(echo "$entry" | jq -r '.cmd')
  file=$(echo "$entry" | jq -r '.file')
  if ! command -v $cmd > /dev/null 2>&1; then
    echo "$cmd does not exist"
    exit 1
  fi
  $cmd $file
done
```

## identify filesystem and read-only status
```bash
$ df -h /etc/ssh/ssh_config
Filesystem      Size  Used Avail Use% Mounted on
/dev/sda4       974M  740K  906M   1% /etc/ssh

$ mount | grep -e /etc/ssh
/dev/sda4 on /etc/ssh type ext4 (rw,relatime,stripe=128)

$ findmnt /etc/ssh
TARGET   SOURCE          FSTYPE OPTIONS
/etc/ssh /dev/sda4[/ssh] ext4   rw,relatime,stripe=128

$ cat /proc/mounts  | grep /etc/ssh
/dev/sda4 /etc/ssh ext4 rw,relatime,stripe=128 0 0

```


## remove header lines
```bash
ps aux | tail -n +2
ps aux | awk 'NR > 1'
ps aux | sed 1d
```

## trim N characters
```bash
# trim first N characters
cut -c6- file.txt
awk '{ print substr($0, 6) }' file.txt
sed 's/^......//' file.txt

# trim last N characters
awk '{ print substr($0, 1, length($)-5) }' file.txt
sed 's/......$//' file.txt
```

## ethtool
> query or control network driver and hardware settings
[ethtool](https://www.mankier.com/8/ethtool)  


## jq
[jq](https://www.mankier.com/1/jq)  
> Command-line JSON processor
[Convert TSV to JSON](https://onlinetools.com/json/convert-tsv-to-json)  
[jqlang](https://github.com/jqlang)  
[playground](https://play.jqlang.org/)  
[manual](https://jqlang.org/manual/)  
[How To Transform JSON Data with jq](https://www.digitalocean.com/community/tutorials/how-to-transform-json-data-with-jq)  
[]()  
[]()  
```bash
# seaCreatures.json
[
    { "name": "Sammy", "type": "shark", "clams": 5 },
    { "name": "Bubbles", "type": "orca", "clams": 3 },
    { "name": "Splish", "type": "dolphin", "clams": 2 },
    { "name": "Splash", "type": "dolphin", "clams": 2 }
]

jq '.' seaCreatures.json
.   filter, also known as identity operator

jq '.[]' seaCreatures.json  # operate on the values of that array instead of the array itself
.[] array value iterator

jq '.[] | .name' seaCreatures.json
|   pipe operator
# Output
"Sammy"
"Bubbles"
"Splish"
"Splash"

jq -r '.[] | .name' seaCreatures.json
-r  raw output
# Output
Sammy
Bubbles
Splish
Splash

jq '.[] | .clams' seaCreatures.json
# Output
5
3
2
2

jq '[.[] | .clams]' seaCreatures.json   # wrap values in an array
# Output
[
  5,
  3,
  2,
  2
]

jq 'map(.clams)' seaCreatures.json
# Output
[
  5,
  3,
  2,
  2
]

jq 'map(.clams) | add' seaCreatures.json
add filter

jq 'map( (select(.type == "dolphin")) )' seaCreatures.json  #  pair select with map to apply select to every value in an array
# Output
[
  {
    "name": "Splish",
    "type": "dolphin",
    "clams": 2
  },
  {
    "name": "Splash",
    "type": "dolphin",
    "clams": 2
  }
]

jq 'map(select(.type == "dolphin") .clams)' seaCreatures.json
# Output
[
  2,
  2
]

jq 'map(select(.type == "dolphin").clams) | add' seaCreatures.json
# Output
4

# Transforming Data to a New Data Structure
jq '{ creatures: [], totalClams: 0, totalDolphinClams: 0 }' seaCreatures.json
# Output
{
  "creatures": [],
  "totalClams": 0,
  "totalDolphinClams": 0
}

jq '{ creatures: map(.name), totalClams: map(.clams) | add, totalDolphinClams: map(select(.type == "dolphin").clams) | add }' seaCreatures.json
# Output
{
  "creatures": [
    "Sammy",
    "Bubbles",
    "Splish",
    "Splash"
  ],
  "totalClams": 12,
  "totalDolphinClams": 4
}
```

## ip
[ip(8)](https://man7.org/linux/man-pages/man8/ip.8.html)  
[ip-link](https://www.mankier.com/8/ip-link)  
[ip-address](https://www.mankier.com/8/ip-address)  
[ip-addrlabel](https://www.mankier.com/8/ip-addrlabel)  
[ip-rule](https://www.mankier.com/8/ip-rule)  
[**Guide to IP Layer Network Administration with Linux**](http://linux-ip.net/html/index.html)  
[Virtual Routing and Forwarding (VRF)](https://www.kernel.org/doc/Documentation/networking/vrf.txt)  
[Working with Linux VRFs](https://www.dasblinkenlichten.com/working-with-linux-vrfs/)  
[Linux ip Command Examples](https://www.cyberciti.biz/faq/linux-ip-command-examples-usage-syntax/)  
[]()  
[]()  
[]()  
[]()  
[]()  

```bash
ip
	link(l)					Network device
	address(a, addr)		Protocol (IP or IPv6) address on a device
	addrlabel(addrl)		Label configuration for protocol address selection
	neighbour(n, neigh)		ARP or NDISC cache entry
	route(r)				Routing table entry
	rule(ru)				Rule in routing policy database
	maddress(m, maddr)		Multicast address
	mroute(mr)				Multicast routing cache entry
	tunnel(t)				Tunnel over IP
	xfrm(x)					Framework for IPsec protocol
    ...

ip link help
```


## network cmds
[nslookup](https://www.mankier.com/1/nslookup)  
> query Internet name servers interactively
[ss](https://www.mankier.com/8/ss)  
> another utility to investigate sockets
[netcat](https://www.mankier.com/1/netcat)  
> arbitrary TCP and UDP connections and listens
[nc Linux Command | The Complete Netcat Usage Guide](https://ioflood.com/blog/nc-linux-command/)  
[ncat](https://www.mankier.com/1/ncat)  
> Concatenate and redirect sockets
[socat command in Linux](https://allcommands.top/os/linux/socat/)  
[Interprocess Communication With Unix Sockets](https://www.baeldung.com/linux/communicate-with-unix-sockets)  
[]()  
[]()  
[]()  
[]()  
```bash
#Internet domain sockets
#Unix domain sockets
# nc
# stream socket
# server side
nc -lU /tmp/my.sock # -U option of nc uses stream sockets by default
# client side
nc -U /tmp/my.sock

ss -xa | grep my.sock   # check the socket type

# datagram socket
# server side
nc -lUu /tmp/my.sock
# client side
nc -Uu /tmp/my.sock

ss -xa | grep my.sock   # check the socket type

# socat
# server side
socat - UNIX-LISTEN:/tmp/my.sock
# client side
socat - UNIX-CONNECT:/tmp/my.sock

# socket
# server side
socket -s /tmp/my.sock
# client side
socket /tmp/my.sock
```

## BASH_REMATCH
```bash
if [[ "hello123" =~ ^([a-z]+)([0-9]+)$ ]]; then
    echo "完整匹配: ${BASH_REMATCH[0]}"   # hello123
    echo "字母部分: ${BASH_REMATCH[1]}"   # hello
    echo "数字部分: ${BASH_REMATCH[2]}"   # 123
fi

# ${BASH_REMATCH[0]}: The entire string that was matched.
# ${BASH_REMATCH[1]}: The first parenthesized subexpression match.
# ${BASH_REMATCH[2]}: The second parenthesized subexpression match.

#
string="Hello World"
pattern='(H[a-z]+)\s*(W[a-z]+)'
if [[ $string =~ $pattern ]]; then
    echo "Match found!"
    for i in "${!BASH_REMATCH[@]}"; do
        echo "$i: ${BASH_REMATCH[$i]}"
    done
fi
```

## while read
```bash
# 基础语法
while IFS= read -r line
do
    # process
done < intput_file

# 文件逐行处理
count=0
while IFS= read -r line; do
    ((count++))
done < access.log
echo "Total lines: $count"

# 
while read -r file; do
    ((total++))
done < <(find . -type f)
echo "Total files: $total"

# 解析结构化数据
while IFS=, read -r name age city; do
    echo "$name is $age years old from $city"
done < users.csv

# 命令输出处理
df -h | tail -n +2 | while read -r filesystem size used avail use_percent mount; do
    if [[ $use_percent > 80% ]]; then
        echo "WARNING: $filesystem at $use_percent"
    fi
done

# 过滤与数据转换
grep "Failed password" auth.log | while read -r line; do
    [[ $line =~ [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+ ]] && echo "${BASH_REMATCH[0]}"
done

# 批量文件操作
find . -name "*.jpg" | while read -r file; do
    mv "$file" "${file%.jpg}_back.jpg"
done

# 网络数据处理
netstat -tun  | while read -r proto recvq sendq local foreign state; do
    [[ $state == "ESTABLISHED" ]] && echo "Active: $local -> $foreign"
done

# 进程管理
ps aux | grep "python"  | while read -r user pid cpu mem vsz rss tty stat start time command; do
    kill -15 $pid
    echo "Sent SIGTERM to $pid"
done

# 配置文件解析
while read -r line; do
    [[ $line =~ ^\[(.*)\]$ ]] && section=${BASH_REMATCH[1]}
    [[ $line =~ ^(.*)=(.*)$ ]] && {
        key=${BASH_REMATCH[1]}
        value=${BASH_REMATCH[2]}
        declare "CONFIG_${section}_${key}=$value"
    }
done < config.ini

# 实时日志监控
tail -f app.log | while read -r line; do
    case $line in
        *ERROR*) echo "ALERT: $line" >&2 ;;
        *WARN*) echo "Warning: $line" ;;
    esac
done

# 跨行数据处理
# 处理多行记录 (使用空行分隔)
while IFS= read -r line || [[ -n $line ]]; do
    [[ -z $line ]] && {
        process_record "$record"
        record=""
        continue
    }
    record+="$line"$'\n'
done < multi_line.txt

# 超时控制
while read -t 5 -r input; do
    echo "You entered: $input"
done
```

## curl
[curl tutorial](https://curl.se/docs/tutorial.html)  
[curl man page](https://curl.se/docs/manpage.html)  
[Everything curl](https://ec.haxx.se/index.html)  
```bash
if curl -I --connect-timeout 3 --proxy 135.242.60.169:58080 https://www.google.com > /dev/null 2>&1; then
    echo "ping google successfully"
fi
```

## xargs
[xargs原理剖析及用法详解](https://www.cnblogs.com/f-ck-need-u/p/5925923.html)  
[man xargs](https://www.man7.org/linux/man-pages/man1/xargs.1.html)  
[How to Use the xargs Command on Linux](https://www.howtogeek.com/435164/how-to-use-the-xargs-command-on-linux/)  
[Using xargs in Combination With bash -c to Create Complex Commands](https://www.howtogeek.com/devops/using-xargs-in-combination-with-bash-c-to-create-complex-commands/)  
```bash
# Linux 下有些命令(echo, ...)不接受 标准输入 作为参数, xargs 将标准输入转换为命令行参数
$ ls
files  txt1  txt2

files:
txt1
txt2

txt1:
1

txt2:
2

$ cat files | xargs -t cat
cat txt1 txt2
1
2

$ cat files | xargs -t -n 1 cat
cat txt1
1
cat txt2
2

#
find /etc -maxdepth 1 -name "*.conf" -print0 | xargs -0 -i grep "hostname" -l {} 

#
cat files.txt | xargs -I file sh -c 'echo file; touch file'

#
find /tmp -name "*.tmp" -type f -print0 | xargs -0 /bin/rm -f

#
pidof sleep | xargs kill -9

#
sleep 300 &
pidof sleep |
xargs -I{} echo "echo 'The PID of your sleep process was: {}'; kill -9 {}; echo 'PID {} has now been terminated'" |
xargs -I{} bash -c "{}"

The PID of your sleep process was: 42513
PID 42513 has now been terminated
[1]+  Killed                  sleep 300

#
sleep 300 &
pgrep -x sleep | while read pid; do
    echo "The PID of the sleep process was: $pid"
    kill -9 "$pid" && echo "PID $pid has been terminated" || echo "Failed to terminate PID $pid"
done
The PID of the sleep process was: 62020
```

## wget
> The non-interactive network downloader.
[wget(1)](https://www.mankier.com/1/wget1)  

## pgrep
> look up, signal, or wait for processes based on name and other attributes
[pgrep(1)](https://www.mankier.com/1/pgrep)  

## lrzsz
>  free x/y/zmodem implementation
[lrzsz: free x/y/zmodem implementation](https://www.ohse.de/uwe/software/lrzsz.html)  

## locale
> get locale-specific information
[locale(1)](https://www.mankier.com/1/locale)  

## jq
> Command-line JSON processor
[jq(1)](https://www.mankier.com/1/jq)  

## lsof
> list open files
[lsof Command in Linux with Examples](https://phoenixnap.com/kb/lsof-command)  
[man lsof](https://linux.die.net/man/8/lsof)  
[Linux lsof Command Examples](https://www.thegeekstuff.com/2012/08/lsof-command-examples/)  
```bash
    lsof  -i
    lsof  -i 6
    lsof  -iTCP
    lsof  -i :22
    lsof  -i@172.16.12.5
    lsof  -i@172.16.12.5:22
    lsof  -i -sTCP:LISTEN
    lsof  -i -sTCP:ESTABLISHED
    lsof  -u daniel
    lsof  -u ^daniel
    kill  -9  `lsof -t -u daniel`
    lsof  -c syslog-ng
    lsof  -c ssh -c init
    lsof  -p 10075
    lsof  /var/log/messages/
    lsof  /home/daniel/firewall_whitelist.txt
    lsof  -u daniel -i @1.1.1.1
    kill  -HUP `lsof -t -c sshd`
    lsof  +L1
    lsof  +d /usr/lib
    lsof  +D /var/log/
    lsof  +D /home -u ^mary
    lsof  -t /usr/share/mime/mime.cache
    lsof  -u mary -c ssh -a
    lsof  -u mary -c ssh -a -r5
    lsof  -i -a -p 932650
    lsof  -i -a -c ssh
    lsof  -i tcp:25
    lsof  -i udp:53
    lsof  -i :1-1024
```


## ln
> make links between files
[ln(1)](https://www.mankier.com/1/ln)  


## fuser
> identify processes using files or sockets
[fuser(1)](https://www.mankier.com/1/fuser)  


## grep
> print lines that match patterns
[grep(1)](https://www.mankier.com/1/grep)  
```bash
## Grep Multiple Patterns
grep 'pattern1\|pattern2' fileName_or_filePath
grep -E 'pattern1|pattern2' fileName_or_filePath
grep -e pattern1 -e pattern2 fileName_or_filePath

## 只打印匹配行的匹配字段
grep -o '(.*)'
grep -o '([^)]*)'  # 如果单行有多个括号，用于多个匹配的多个分行输出

## 匹配行上下文打印
grep -A NUM file	# After
grep -B NUM file	# Before
grep -C NUM file	# Center

## 常用选项
-E, --extended-regexp
-q, --quiet, --silent
-o, --only-matching
-e PATTERN, --regexp=PATTERN
-w, --word-regexp
--color
```

## strip
> discard symbols and other data from object files
[strip(1)](https://www.mankier.com/1/strip)  

## strings
> print the sequences of printable characters in files
[strings(1)](https://www.mankier.com/1/strings)  

## killall
> kill processes by name
[killall(1)](https://www.mankier.com/1/killall)  

## kill
> terminate a process
[kill(1)](https://www.mankier.com/1/kill)  
[Termination Signals](https://www.gnu.org/software/libc/manual/html_node/Termination-Signals.html)  
[SIGKILL signal handling](https://stackoverflow.com/questions/15766036/sigkill-signal-handling)  
[What does a program do when it's sent SIGKILL signal?](https://unix.stackexchange.com/questions/485644/what-does-a-program-do-when-its-sent-sigkill-signal)
[killall5(8)](https://linux.die.net/man/8/killall5)  
```bash
# spin
SPIN='-\|/'
function spin {
        i=0
        while kill -0 $1 2> /dev/null
        do
                i=$(( (i+1)%4 ))
                printf "\b${SPIN:$i:1}"
                sleep .1
        done
        printf "\bDONE\n"
}

echo 1
sleep 20 &
spin $!
```

## install
> copy files and set attributes
[install(1)](https://www.mankier.com/1/install)  

## flock
> manage locks from shell scripts
[flock(1)](https://www.mankier.com/1/flock)  

## gcore
> Generate a core file of a running program
[gcore(1)](https://www.mankier.com/1/gcore)  

## gstack
> Print a stack trace of a running program
[gstack(1)](https://www.mankier.com/1/gstack)  

## xxd
> make a hex dump or do the reverse.
[xxd(1)](https://www.mankier.com/1/xxd)  
```bash
# Options
       -p | -ps | -postscript | -plain
              Output in postscript continuous hexdump style. Also known as plain hexdump style.

       -r | -revert
              Reverse operation: convert (or patch) hexdump into binary.  If not writing to stdout, xxd writes into its output file without truncating it. Use the combination -r -p to read  plain, hexadecimal dumps without line number information and without a particular column layout. Additional Whitespace and line-breaks are allowed anywhere.

       -l len | -len len
              Stop after writing <len> octets.			  

       -s [+][-]seek
              Start at <seek> bytes abs. (or rel.) infile offset.  + indicates that the seek is relative to the current stdin file position (meaningless when not reading from stdin).  - indicates that the seek should be that many characters from the end of the input (or if combined with +: before the current stdin file position).  Without -s option, xxd starts at the current file position.		

       -g bytes | -groupsize bytes
              Separate  the output of every <bytes> bytes (two hex characters or eight bit-digits each) by a whitespace.  Specify -g 0 to suppress grouping.  <Bytes> defaults to 2 in normal mode, 4 in little-endian mode and 1 in bits mode.  Grouping does not apply to postscript or include style.			  	  

# "xxd -r -p" is used to convert plain hex dump (space-separated hex values) into binary data, 
echo "eb fe 11 22 33 44" | xxd -revert -plain > test.bin		# Writes the bytes 0xEB 0xFE 0x11 0x22 0x33 0x44 to the output

# Common Use Cases

#1 Generate a Hexdump
xxd file.bin > file.txt
xxd -revert file.txt > file.bin

#2 Create Binary from Hex (Reverse Mode)
echo "55aa" | xxd -revert -plain > sector.sig  # Write 0x55 0xAA to a file

#3 Inspect Specific Bytes
xxd -seek 0x1FE -len 2 -plain /dev/sda  # Read 2 bytes at offset 510 (0x1FE)

#4 Edit Binary Files
# Patch byte at offset 0x05 to 0xFF
echo "000005: FF" | xxd -revert - file.bin
```

## hexdump
> display file contents in hexadecimal, decimal, octal, or ascii
[hexdump(1)](https://www.mankier.com/1/hexdump)  
[]()

## od
> dump files in octal and other formats
[od(1)](https://www.mankier.com/1/od)  
```bash
# od -Ax -t x1 -j 166 -N 4 main
    -Ax
        按照16进制格式输出
    -t x1
        输出格式为十六进制单字节显示
    -j 166
        从文件开始跳过166(0xa6)个字节
    -N 4
        只dump 4字节数据
```

## id
> print real and effective user and group IDs
[id(1)](https://www.mankier.com/1/id)  
[ruid, euid, suid usage in Linux](https://mudongliang.github.io/2020/09/17/ruid-euid-suid-usage-in-linux.html)  
[Linux — Process Permissions](https://tonylixu.medium.com/linux-process-permissions-cf7698d5b08f)  
[]()  
[]()  

## useradd
> create a new user or update default new user information
[useradd](https://www.mankier.com/8/useradd)  
[The Complete Guide to “useradd” Command in Linux – 15 Practical Examples](https://www.tecmint.com/add-users-in-linux/)  

## usermod
> modify a user account
[usermod](https://www.mankier.com/8/usermod)  
[A Complete Guide to Usage of ‘usermod’ command – 15 Practical Examples with Screenshots](https://www.tecmint.com/usermod-command-examples/)  
[15 usermod command examples in Linux](https://www.golinuxcloud.com/usermod-command-in-linux/)  
[usermod Command in Linux | Explained](https://itslinuxfoss.com/usermod-command-linux-explained/)  

## userdel
> delete a user account and related files
[userdel](https://www.mankier.com/8/userdel)  

## groupadd
> create a new group
[groupadd](https://www.mankier.com/8/groupadd)  

## groupmod
> modify a group definition on the system
[groupmod](https://www.mankier.com/8/groupmod)  

## groupdel
> delete a group
[groupdel](https://www.mankier.com/8/groupdel)  

## chsh
> change your login shell
[chsh](https://www.mankier.com/1/chsh)  

## passwd
> change user password
[passwd](https://www.mankier.com/1/passwd)  

## stat
> display file or file system status
[stat](https://www.mankier.com/1/stat)  

## watch
> execute a program periodically, showing output fullscreen
[watch(1)](https://www.mankier.com/1/watch)  

## tail
> output the last part of files
[tail(1)](https://www.mankier.com/1/tail)  

## head
> output the first part of files
[head(1)](https://www.mankier.com/1/head)  

## du
> estimate file space usage
[du(1)](https://www.mankier.com/1/du)  

## df
> report file system space usage
[df(1)](https://www.mankier.com/1/df)  

## cut
> remove sections from each line of files
[cut(1)](https://www.mankier.com/1/cut)  

## sort
> sort lines of text files
[sort(1)](https://www.mankier.com/1/sort)  

## tr
> translate or delete characters
[tr(1)](https://www.mankier.com/1/tr)  
```bash
tr '\0' '\n' < /proc/40013/environ

```

## date
> print or set the system date and time
[date(1)](https://www.mankier.com/1/date)  

## column
> columnate lists
[column(1)](https://www.mankier.com/1/column)  

## chrt
> manipulate the real-time attributes of a process
[chrt(1)](https://www.mankier.com/1/chrt)  

## cmp
> compare two files byte by byte
[cmp(1)](https://www.mankier.com/1/cmp)

## file
> determine file type
[file(1)](https://www.mankier.com/1/file)  

## find
[find(1)](https://www.mankier.com/1/find)  
```bash
find . -name vcmts.cfg -print0 | xargs -0 vim
find . -name vcmts.cfg -exec vim {} +

# 使用 \; (低效)
find . -name *.txt -exec echo {} \;
# 执行过程：
#   echo file1
#   echo file2
#   ... 
#   echo fileN

# 使用 + (高效)
find . -name *.txt -exec echo {} +
# 执行过程：
#   echo file1 file2 ... fileN

# 正确：{} 必须在命令末尾
find . -exec cmd {} +

# 错误：{} 不在末尾
find . -exec {} cmd +  # 无效

# 先备份再编辑
find . -name *.cfg -exec cp {} {}.bak \; -exec vim {} +

# 仅对修改时间>30天的文件操作
find /var/log -name *.log -mtime +30 -exec gzip {} +

# 编辑所有找到的配置文件
find /etc -name "*.conf" -exec vim {} +

# 修复所有脚本权限
find ~/scripts -name "*.sh" -exec chmod +x {} +

# 在所有 PHP 文件中替换字符串
find . -name "*.php" -exec sed -i 's/old/new/g' {} +

# 压缩所有旧日志
find /var/log -name "*.log" -mtime +7 -exec gzip {} +
```

## exec
- > The Linux `exec` command executes a Shell command without creating a new process. Instead, it replaces the currently open Shell operation. Depending on the command usage, `exec` has different behaviors and use cases.
```
使用指定命令替换当前shell执行环境，而不创建subshell，底层系统调用 [[execve]]

当使用exec执行command时，如果想在command中返回shell环境，可以在command的最后执行`sh -l`命令，达到返回shell环境的目的

sh -l
	Make bash act as if it had been invoked as a login shell (see INVOCATION below)

```
[The Uses of the Exec Command](https://www.baeldung.com/linux/exec-command-in-shell-script)  
[Linux exec Command With Examples](https://phoenixnap.com/kb/linux-exec)  
[man sh](https://linux.die.net/man/1/sh)  

## eval
- > On Unix-like operating systems, eval is a builtin command of the Bash shell. It concatenates its arguments into a single string, joining the arguments with spaces, then executes that string as a bash command. It's similar to running bash -c "string", but eval executes the command in the current shell environment rather than creating a child shell process.
```bash
# 提供给eval命令的字符串可以包含预留关键字，这些关键字会在第一轮被解释，之后别的字符串在第二轮被解释
	cmd="if true; then echo 1; else echo 0; fi"
	eval "$cmd"
# eval 命令可以用来提供额外的间接层引用
	  cmd1="cmd2"
	  cmd2="echo Hi!"
	  eval "\${$cmd1}"
# eval 在当前 shell 环境下执行，而不是子 shell
	  cat variables.txt
	  first=How-to
	  second=Geek
	  
	  eval "$(cat variables.txt)"
	  echo $first $second			# How-to Geek
```

## 在 Linux 后台 Shell 脚本中实现动态行为控制
```bash
# 1 文件检测控制
debug_echo() {
    if [ -f "/tmp/debug_flag" ]; then
        echo "[DEBUG] $*"
    fi
}

main() {
    debug_echo "$0 starts to execute ..."
    # ...
    debug_echo "$0 is finishing ..."
}
# touch /tmp/debug_flag
# rm /tmp/debug_flag

# 2 信号动态控制
DEBUG=false

trap 'enable_debug" USR1
trap 'disable_debug" USR2

enable_debug() {
    DEBUG=true
    echo "DEBUG enabled at $(date)" >> /var/log/script.log
}

disable_debug() {
    DEBUG=false
    echo "DEBUG disabled at $(date)" >> /var/log/script.log
}

debug_echo() {
    if $DEBUG; then
        echo "[DEBUG] $*" >> /var/log/script.log
    fi
}

while true; do
    debug_echo "$0 starts to execute at $(date)"
    # ...
    debug_echo "$0 finished at $(date)"
done

# kill -USR1 PID
# kill -USR2 PID
```

## 在终端固定位置每秒刷新显示文件内容
```bash
# 1
while true; do
  # move cursor to top-left corner(0,0) + clear the terminal screen
  # \033[0;0H moves the cursor to the top-left corner
  # \033[2J clears the screen
  printf "\033[0;0H\033[2J"		# ​清除当前屏幕内容并重置光标位置​​
  
  # show timestamp
  echo "[Last updated: $(date +%T)]"
  cat /path/to/file | grep --color -E 'WARNING|ERROR'
  
  sleep 1
done

# 
while sleep 1; do
  # 清屏并重置光标到(0,0)
  printf "\033[0;0H\033[2J"
  
  # 显示文件内容（支持带行号）
  echo "==== FILE CONTENTS ($(date)) ===="
  display_lines=$(( $(tput lines) - 3 ))	# tput lines 用来​​获取当前终端的行数（高度）​​，以字符行数为单位
  cat -n /path/to/file | tail -n ${display_lines}
done

# 2
watch -n1 -ct "date +'%H:%M:%S'; cat /path/to/file"
watch -n1 "grep --color -E 'CRITICAL|Failed' /var/log/syslog"

# 3
while true; do
  clear
  date +'%H:%M:%S';
  echo "CPU: $(top -bn1 | awk '/Cpu/{print $2}')% | Mem: $(free -m | awk '/Mem/{print $3}')MB"
  tail -n 5 /var/log/syslog
  sleep 1
done
```

## bash interactive mode
```bash
# 1 使用子 shell
dirs=(
#    "/path/to/dir1"
#    "/path/to/dir2"
)

for dir in "${dirs[@]}"; do
    (
        echo "entering $dir ..."
        cd -- "$dir" || exit 1			# 使用--避免目录名以-开头的问题
        
        # 启动bash的交互式shell, 操作完成后​​，输入exit或按Ctrl+D返回
        bash -i
        
        echo "leaving $dir ..."
    )
done

echo "all done!"

# 2 使用pushd/popd
org_dir=$(pwd)

dirs=(
    "/home/morrism/x1/dir1"
    "/home/morrism/x1/dir2"
)

for dir in "${dirs[@]}"; do
    pushd "$dir" >/dev/null || exit 1
    echo "current dir: $(pwd)"
    
    # 启动当前用户的默认shell​​(bash, zsh, ...)​，输入exit或按Ctrl+D返回
    $SHELL
    
    popd >/dev/null
done

# return to org directory
cd "$org_dir" || exit
```

## Utilities
### xclip
[xclip](https://www.mankier.com/1/xclip)  
[Copy and paste at the Linux command line with xclip](https://opensource.com/article/19/7/xclip)  
[]()  
[]()  
```bash
# 复制文本到剪贴板
xclip -sel clip file_name
tail -n 30 logfile.log | xclip -sel clip

echo "hello world" | xclip -selection clipboard
echo "hello world" | xclip -sel c

#
xclip -selection clipboard -o	# Paste from clipboard to terminal
xclip -s c -o

# aliases
alias cbcopy="xclip -selection clipboard"
alias cbpaste="xclip -selection clipboard -o"
```

## Commands
[How to Get Bash Script’s Own Path](https://www.systutorials.com/how-to-get-bash-scripts-own-path/)  
[Bash Variables](https://www.gnu.org/software/bash/manual/html_node/Bash-Variables.html)  
[]()  
[]()  
```bash
# 1
if [ -n "${BASH_SOURCE}" ]; then
    MYROOT="`dirname ${BASH_SOURCE}`"
elif [ -n "${ZSH_NAME}" ]; then
    MYROOT="`dirname $0`"
else
    MYROOT="`pwd`"
fi
MYROOT="`readlink -f ${MYROOT}`"

for key in ${MYROOT}/gpg/*.key; do
    cat $key | gpg --quiet --import
done

# Check for uninitialized submodules
git -C ${MYROOT} submodule status | while read -r hash submodule extra; do
    if [ "${hash#-}" != "${hash}" ]; then
        echo "Submodule ${submodule} not initialized, initializing now"
        git -C ${MYROOT} submodule init ${submodule}
        git -C ${MYROOT} submodule update ${submodule}
    fi
done

. ${MYROOT}/meta-vcommon/scripts/vcm-yocto-init

# We're done with MYROOT now
unset MYROOT

# 2
```
## Shell Expansions
[Shell Expansions](https://www.gnu.org/software/bash/manual/html_node/Shell-Expansions.html)  
[Shell Variables](https://www.gnu.org/software/bash/manual/html_node/Shell-Variables.html)  
[]()  
[]()  
[]()  
[]()  
### Bash brace expansion
```bash
{start..end}
{start..end..increment}
{pattern1,pattern2,pattern3}

echo {1..10}
echo {5..1}
echo {1..10..2}

echo {a..z}
echo {A..Z}

touch file{a,b,c}.txt
touch file{1..5}.txt

echo {a,b}{1,2,3}
echo {a..c}{1..3}

echo {2020..2023}-{01..12}
```

## Bash 命令分隔符
```bash
1. 分号 ;
command1; command2	# 顺序执行命令，无论前一个命令是否成功

2. 逻辑运算符 && 和 ||
command1 && command2				# command2 仅在 command1 成功后执行
command1 || command2  				# command2 仅在 command1 失败后执行
command1 && command2 || command3  	# command1 成功执行 command2，否则执行 command3

3. 管道 | 和 |&
command1 | command2        # command1 的 stdout 传递给 command2
command1 |& command2       # command1 的 stdout 和 stderr 均传递给 command2

4. 后台符号 & (隐含命令终止的作用)
command1 & command2  	   			# command1 后台运行，command2 立即执行
for i in {1..3}; do sleep $i & done	# 后台符号 & 隐含命令终止的作用

5. 换行符
command1
command2  # 等同于 command1; command2

6. 命令分组 {} 和 ()
{ command1; command2; }    # 注意末尾分号和空格
(command1; command2)       # 在子 Shell 中执行，变量修改不影响父 Shell
```

## 在条件判断中进行正则表达式匹配
```bash
[[ 字符串 =~ 正则表达式 ]]	# 1. ​​必须使用双方括号  2. 正则表达式无需引号​​

# 1
if [[ "abc123" =~ ^[a-z]+[0-9]+$ ]]; then
    echo "字符串以字母开头，数字结尾"
fi

# 2
[[ "hello world" =~ ^hello\ world$ ]]      # 正确（转义空格）
[[ "example.com" =~ ^example\.com$ ]]      # 正确（转义 .）

# 3
pattern="^[a-z]+[0-9]+$"
if [[ "abc123" =~ $pattern ]]; then
    echo "匹配成功"
fi

# 捕获分组
BASH_REMATCH 数组保存匹配结果
	BASH_REMATCH[0]：整个匹配内容
	BASH_REMATCH[1]：第一个捕获分组，依此类推

if [[ "Date: 2023-10-30" =~ ([0-9]{4}-[0-9]{2}-[0-9]{2}) ]]; then
    echo "日期为：${BASH_REMATCH[1]}"
fi
```

## source
```bash
​​特性​​				source script.sh (或 . script.sh)	  c
​​执行环境​​			当前 Shell 进程						   新建的子 Shell 进程
​​进程 ID (PID)​​		与当前终端相同						   新建独立 PID
​​资源消耗​​			较低（无新进程开销）					较高（需要创建新进程）
​​脚本权限要求​​		无需可执行权限							必须具有可执行权限


# source script.sh
bash内置的 source 命令用来读取并执行脚本文件的内容
当使用 source 命令执行脚本时，它是在当前 source 它的 shell 环境下执行的，因此，脚本可以访问当前 source 它的 shell 下的所有变量，另一方面，source 命令执行完成后，脚本文件中的所有定义(包括变量和函数)在脚本的 parent shell 变得可用，因此通过 source 命令可以用来在不同脚本之间共享内容

strace ./script.sh

# source script.sh
而当通过脚本名称或者bash命令执行脚本时，它是在一个新的 shell 下运行的，因此，脚本只能访问 parent shell 中的通过 export 导出的变量或者函数，并且，该脚本下所有的子 shell 中的定义在该脚本退出时都不复存在
# 直接追踪运行在bash中的进程
echo $$		# 获取 terminal 1 的 PID
sudo strace -p xxx	# 在 terminal 2 中 trace terminal 1 对应的 bash 进程，xxx 为 terminal 1 的 PID
source script.sh	# 在 terminal 1 中 source script.sh，观察 terminal 2 的输出


# ​​BASH_SOURCE
	​​BASH_SOURCE 是一个特殊的数组变量​​，用于追踪脚本的执行来源和调用栈关系

#1 获取当前脚本路径
if [ -n "${BASH_SOURCE}" ]; then
    MYROOT="`dirname ${BASH_SOURCE}`"
elif [ -n "${ZSH_NAME}" ]; then
    MYROOT="`dirname $0`"
else
    MYROOT="`pwd`"
fi
MYROOT="`readlink -f ${MYROOT}`"

# echo "Current script: ${BASH_SOURCE[0]}"

#2 判断脚本加载方式
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "运行方式: 直接执行"
    # 主程序逻辑
else
    echo "运行方式: source 加载"
    # 初始化或函数库逻辑
fi
```
## eval
```bash
# 利用 eval 命令结合其他linux命令(例如 awk)获取所需要的信息的变量定义
: <<'COMMENT'
基本思想：
	1. 利用 awk 中的 BEGIN block进行必要的初始化等 setup 准备工作
	2. 根据 awk 的输入，结合{}中的代码逻辑进行针对逐行的相关计算
	3. 在 awk 的 END block中利用 printf等输出语句生成bash中的变量初始化操作的语句
	4. 利用 bash 下的 eval 命令完成变量的真正初始化操作
COMMENT

eval $(
    awk 'BEGIN {
            printf("VAR=\"");
            printf("1");				# awk 中的 ; 用于 分隔语句 或 结束表达式
            printf("\";");				# bash中 ; 用于分隔同一行的多个命令
        }'
)

echo $VAR
```

## colorize terminal output
[ANSI Escape Sequences  颜色输出](https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797)  
```bash

# colorize output in the terminal
ANSI codes are standardized sequences to control text formatting (color, style, etc.).
Bash 中输出带颜色的字符串到终端，通过 ​​ANSI 转义码​​（ANSI Escape Codes）实现

# 1
echo -e "\e[STYLE;FG;BGmTEXT\e[0m"
	\e[ starts the escape sequence.
	STYLE, FG (foreground), and BG (background) are numeric codes.
	\e[0m resets formatting

echo -e "\e[1;31;47mHello World\e[0m"

# 2
The tput command interacts with your terminal’s capabilities via the terminfo database. It’s more readable and avoids hardcoding ANSI values.
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
BOLD=$(tput bold)
RESET=$(tput sgr0)

echo "${BOLD}${RED}Error: Something went wrong.${RESET}"

# 3
RED="\e[31m"
GREEN="\e[32m"
BOLD="\e[1m"
RESET="\e[0m"

printf "%bhello world%b\n" ${RED} ${RESET}

# 4
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
YELLOW=$(tput setaf 3)
BLUE=$(tput setaf 4)
BOLD=$(tput bold)
RESET=$(tput sgr0)

log_error() { echo "${RED}${BOLD}[ERROR]${RESET} $*"; }
log_warn()  { echo "${YELLOW}[WARN]${RESET} $*"; }
log_info()  { echo "${BLUE}[INFO]${RESET} $*"; }
log_ok()    { echo "${GREEN}[OK]${RESET} $*"; }

log_error "服务启动失败" Error code: 123
log_ok "备份完成"

# 
```

## builtins
[Bash Builtin Commands](https://www.gnu.org/software/bash/manual/bash.html#Bash-Builtins)  
[bash-builtins](https://manpages.ubuntu.com/manpages/jammy/man7/bash-builtins.7.html)  
[complete](https://www.gnu.org/software/bash/manual/bash.html#index-complete)  
[]()  
```bash
man builtins

## :
: [arguments]
		No effect; the command does nothing beyond expanding arguments and performing any specified redirections.  The return status is zero.

## complete

# Skeleton to copy/paste for writing simple completions.

# a program foo with the following interface
foo -c green|red|blue -s low|high -f <file> -h

# 为名为 foo 的​​自定义命令​​实现了​​智能选项补全​​功能
function _foo() {
    # 当前正在输入的词（用户键入但未完成的词）
    local curr=$2
    # 上一个完成的词（通常是已经输入的选项）
    local prev=$3

    # 命令支持的所有选项列表
    local opts="-c -s -f -h"
    # COMPREPLY 数组 是 Bash 自动补全机制的核心变量，存储自动补全的建议列表
    # -W "green red blue"：指定备选词列表
    # -f：进行文件名补全
    # -- $curr：只显示与当前输入匹配的候选项
    case $prev in
        -c) COMPREPLY=( $(compgen -W "green red blue" -- $curr) );;
        -s) COMPREPLY=( $(compgen -W "low high" -- $curr) );;
        -f) COMPREPLY=( $(compgen -f -- $curr) );;
        *)  COMPREPLY=( $(compgen -W "$opts" -- $curr) );;
    esac
}

# 指定使用函数 _foo 进行补全目标命令 foo
complete -F _foo foo

```

## 重定向
```bash
command > file							# 标准输出重定向
> file									# 等价于 ": > file"，即执行空操作，效果为 file 被打开并截断，无数据写入

command >> file							# 追加输出重定向

command 2> file							# 标准错误重定向

command > file 2>&1 (command &> file)	# 标准输出和标准错误同时重定向

command < file							# 标准输入重定向 (从文件读取输入，而非终端)

command <<delimiterXXX
delimiterXXX							# Here 文档 (将输入数据嵌入到命令中，直到遇到指定分隔符)

cat <<EOF > myfile.conf
some
multi-line
content
EOF

command <<< string						# Here 字符串 (将字符串作为命令的输入)

# 1
mail -s "Report" user@example.com <<END
This is the body of the email.
Line 2.
END

# 2
wc -w <<< "Hello World"  # 统计字符串中的单词数（输出 2）

# 3
exec 3> data.txt  # 创建文件描述符 3 并写入 data.txt
echo "Custom FD" >&3  # 通过 FD 3 写入文件
exec 3>&-  # 关闭文件描述符 3

# 4
script.sh > output.log 2> errors.log  # 输出和错误分开记录

# 5 生成配置文件
cat > config.conf <<EOF
[Settings]
debug = false
EOF
```

## Bash Commands Similar to Linux Syscalls
```bash
## ​**1. Process Management**
| Linux Syscall | Bash Equivalent       | Purpose & Example                                        |
|---------------|-----------------------|----------------------------------------------------------|
| `fork()`      | `&` operator          | Create subshell/background process:<br>`sleep 10 &`      |
| `execve()`    | `exec`                | Replace shell process:<br>`exec /bin/zsh`                |
| `waitpid()`   | `wait`                | Wait for child processes:<br>`wait $pid`                 |
| `clone()`     | `( )` subshell        | Isolated execution context:<br>`(cd /tmp && ls)`         |
| `exit()`      | `exit`                | Terminate script:<br>`exit 1`                            |


## ​**2. File Operations**
| Linux Syscall | Bash Equivalent          | Example                                               |
|---------------|--------------------------|-------------------------------------------------------|
| `open()`      | `exec` + file descriptors| Persistent FD handling:<br>`exec 3> file.txt`         |
| `read()`      | `read`                   | Read from FD:<br>`read -u 3 data`                     |
| `write()`     | `echo` + redirection     | Write to FD:<br>`echo "text" >&3`                     |
| `close()`     | `exec` + `&-`            | Close FD:<br>`exec 3>&-`                              |
| `stat()`      | `test`                   | File checks:<br>`if [ -f file.txt ]; then ...`        |

## ​**3. Signal Handling**
| Linux Syscall  | Bash Equivalent | Example                                                       |
|----------------|-----------------|---------------------------------------------------------------|
| `kill()`       | `kill`          | Send signals:<br>`kill -HUP $pid`                             |
| `sigaction()`  | `trap`          | Signal handlers:<br>`trap 'cleanup' SIGINT`                   |
## ​**4. Memory Management**
| Linux Syscall | Bash Feature    | Example                                                                |
|---------------|-----------------|------------------------------------------------------------------------|
| `mmap()`      | `/dev/shm`      | Shared memory:<br>`dd if=/dev/zero of=/dev/shm/mem bs=1M count=100`    |
| `brk()`       | `ulimit`        | Memory limits:<br>`ulimit -v 500000` (500MB virtual memory limit)      |

## ​**5. Network Operations**
| Linux Syscall | Bash Tool       | Example                                                                |
|---------------|-----------------|------------------------------------------------------------------------|
| `socket()`    | `nc` (netcat)   | TCP communication:<br>`nc -l 8080`                                     |
| `connect()`   | `curl`/`wget`   | HTTP requests:<br>`curl -s http://api.example.com/data.json`           |
| `bind()`      | `ss`/`netstat`  | Socket monitoring:<br>`ss -tulpn \| grep :443`                         |

## ​**6. Advanced System Interaction**
| Linux Syscall       | Bash Feature    | Example                                                          |
|---------------------|-----------------|------------------------------------------------------------------|
| `ioctl()`           | `stty`          | Terminal control:<br>`stty -echo` (disable input echoing)        |
| `getpid()`          | `$$` variable   | Process ID:<br>`echo "Script PID: $$"`                           |
| `gettimeofday()`    | `date`          | Timestamps:<br>`date +%s.%N` (nanosecond precision)              |


## Common Patterns
# File Locking
	exec 200>/tmp/lockfile
	flock -n 200 || exit 1
	# Critical section here
	flock -u 200

# Signal-Driven Scripts
	trap 'echo "Exiting cleanly"; rm tempfile' EXIT SIGINT

# Debugging Tools
	strace -e trace=open,read bash -c 'cat file.txt'

## When to Avoid Bash
For syscall-intensive tasks, prefer:
	​C (direct syscall access via <unistd.h>)
	​Python (os, fcntl, mmap modules)
	​Rust (memory-safe systems programming)
```

## exec
```bash
##exec 命令主要用途
	1. replacing the current shell process

	exec command [arguments]

	Behavior:
		1. Replaces the ​current shell process with the specified command.
		2. The original shell process is terminated, and the new command takes over its PID.
		3. After the command exits, the shell session ends (if used interactively) or the script terminates.

		# Script exits after `nginx` finishes (no new process created)
		#!/bin/bash
		setup_environment
		exec nginx -g "daemon off;"

	Use Cases:
		1. Minimize Resource Usage: Avoid creating a subshell when running a final command in a script.
		2. Switch Shells: Replace the current shell with another (e.g., exec zsh).
		3. Run Commands with Environment Changes: Apply new environment variables permanently.

	2. manipulating file descriptors

	exec [n]<file   # Open file for reading on descriptor `n`
	exec [n]>file   # Open file for writing on descriptor `n`
	exec [n]>&-     # Close descriptor `n`

	# Redirect All Output
		exec >script.log 2>&1  # All subsequent output goes to script.log

	# Persistent File Handles
	exec 3>output.txt
	echo "Hello" >&3
	exec 3>&-  # Close descriptor 3	

	# Replace `;` with `+` to pass multiple files to a single process
	find . -name "*.log" -exec rm {} +

	# Overlay Environment Variables
	exec env VAR=value /path/to/program

	# Log Entire Script Execution
	#!/bin/bash
	exec > >(tee script.log) 2>&1  # Tee output to file and terminal
	echo "Debug info..."

	# Read Multiple Files Simultaneously
	exec 3<file1 4<file2
	read -u 3 line1
	read -u 4 line2
	exec 3<&- 4<&-

	# Replace shell process
	exec zsh

	# Replace `;` with `+` to pass multiple files to a single process
	find . -name "*.log" -exec rm {} +
```

## howto generate 512-byte disk image file
```bash
( echo "eb fe 11 22 33 44" | xxd -r -p; \
  cat /dev/zero | head -c 504; \
  echo "55 aa" | xxd -r -p \
) > minimal.img

# Structure of the Code
(
  [Part 1: Hex bytes]  					# Initial Hex Bytes
  [Part 2: 504 null bytes]  			# Null Padding
  [Part 3: Final 2-byte signature]		# Boot Signature
) > minimal.img
```

## subshell
```bash
# 在 Bash 中 subshell 是一个由 parent shell 创建的独立的 child process，用于在隔离的环境下执行 shell 命令

## What a Subshell Does
1. 创建隔离的运行环境 (Creates an isolated execution environment)
	Commands inside (...) run in a ​child process, separate from the parent shell
	Variables, aliases, and shell options modified inside the subshell do not affect the parent shell.
2. 合并输出 (​Combines output)
	All output (stdout) from commands inside the subshell is ​merged into a single stream

## subshell 的主要特征
#1. Sequential Execution: subshell 中的命令按顺序执行
#2. Single Redirection: subshell 的所有输出统一重定向到特定的文件
	( echo "eb fe 11 22 33 44" | xxd -r -p; \
	cat /dev/zero | head -c 504; \
	echo "55 aa" | xxd -r -p \
	) > minimal.img

	等价于

	# Equivalent without a subshell (less efficient):
	echo "eb fe 11 22 33 44" | xxd -r -p > minimal.img
	cat /dev/zero | head -c 504 >> minimal.img
	echo "55 aa" | xxd -r -p >> minimal.img

#3. Process Isolation: subshell 中的变量或者shell state改变不会影响到 parent shell
	(
	MY_VAR="changed"
	echo "Subshell: $MY_VAR"  # Output: Subshell: changed
	)
	echo "Parent: $MY_VAR"      # Output: Parent: [empty/unmodified]
#4. Parallel Processing: 并行处理
	(sleep 1 & sleep 2 & wait)  # Runs sleeps in parallel
	(long_command1) & (long_command2) & wait

## subshell 的主要应用场景
#1. Grouping Commands for Redirection
# Send all output to a log file
	(
	echo "Starting task..."
	complex_command
	echo "Finished."
	) > log.txt
#2. Temporary Environment Changes
	(cd /tmp && ls)  # Changing directory only affects the subshell
#3. Background Execution
	(
	command1
	command2
	) &
#4. Capturing Output
	contents=$(ls /tmp | grep 'log')

	result=$(
	echo "Hello"
	echo "World"
	)
	echo "$result"  # Output: Hello\nWorld
#5. Pipeline stages:
	generate_data | (process_data; cleanup)
	cat file.txt | ( while read line; do ...; done )
#6. Avoid Traps/Signals
	(trap 'echo Ignored' INT; sleep 10)  # Parent shell's INT trap is unaffected.

#7. Process substitution (<(cmd)) implicitly uses subshells
	diff <(echo "Hello") <(echo "World")

## Subshell ( ... ) vs. Code Block { ... }
# subshell
	#1. Runs in a separate process.
	#2. Inherits variables and state from the parent but cannot modify them.
	#3. Suitable for isolated tasks or combining output.

# code block
	#1. Runs in the ​current shell process (Shared environment).
	#2. Modifications to variables or state affect the parent shell (No isolation).
	#3. No process overhead (faster).
	#4. Cannot be backgrounded directly: The entire block runs in sequence
	#5. Syntax requirements: Commands must end with semicolons or newlines, and there must be spaces around the braces

	{
	echo "Hello"
	echo "World"
	} > output.txt

## Advanced Scenarios
#1 Chaining Multiple Subshells
(ls /tmp; (cd /var && ls)) > combined_output.txt

#2 Combining with Functions
run_in_subshell() {
    (echo "In subshell: $1"; process_data "$1")
}

process_data() {
    echo "Processing $1"
}

run_in_subshell "example"

#3 Complex Redirection with Code Blocks
{ 
    echo "Starting log"; 
    command1 2>&1; 
    command2 2>&1; 
    echo "Ending log"; 
} | tee logfile.txt

#4 Conditional Execution
	{ command1 && command2; } || echo "Failed"
```

## 判断命令是否安装
```bash
if ! command -v COMMAND &> /dev/null; then
    echo "Error: COMMAND is not installed. Please install it first."
    exit 1
fi
```

## 等待所有后台作业完成
```bash
# 启动两个后台作业
echo "Starting job 1..."
sleep 3 &  # 后台作业1
pid1=$!

echo "Starting job 2..."
sleep 6 &   # 后台作业2
pid2=$!

# 等待所有后台作业完成
wait < <(jobs -p)	# 命令 jobs -p 用于返回所有后台进程的 PID，wait 命令用于等待当前shell下所指定的后台进程结束（如果不指定参数，则代表所有的后台进程）
# wait $pid1 $pid2
# wait %1 %2
```

## 解析配置文件
```bash
## 使用 source 直接加载 符合 Bash 语法的配置文件
config.sh:
DB_HOST="localhost"
DB_PORT=3306
DEBUG=true

main.sh:
load_config() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "Error: config file '$file' does not exist" >&2
        return 1
    fi
    source "$file"
}

load_config "config.sh"
echo "Host: $DB_HOST, Port: $DB_PORT, Debug: $DEBUG"

## 解析 JSON 文件
config.json:
{
    "database": {
        "host": "localhost",
        "port": 3306
    },
    "server": {
        "debug": true
    }
}

main.sh:
DB_HOST=""
DB_PORT=""
DEBUG=""

parse_json() {
    local file="$1"
    if ! command -v jq >/dev/null 2>&1; then
        echo "Error: command jq not found" >&2
        return 1
    fi
    if [ ! -f "$file" ]; then
        echo "Error: config file '$file' does not exist" >&2
        return 1
    fi

    DB_HOST=$(jq -r '.database.host' "$file")
    DB_PORT=$(jq -r '.database.port' "$file")
    DEBUG=$(jq -r '.server.debug' "$file")
    # export DB_HOST DB_PORT DEBUG
}

parse_json "config.json"
echo "Host: $DB_HOST, Port: $DB_PORT, Debug: $DEBUG"

## 直接解析键值对文件
config.env:
DB_HOST=localhost
DB_PORT=3306
DEBUG=true

main.sh:
parse_config() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "Error: config file '$file' does not exist" >&2
        return 1
    fi

    while IFS='=' read -r key value; do
        # 跳过空行和注释
        [[ -z "$key" || "$key" =~ ^# ]] && continue
        # 去除首尾空白
        key=$(echo "$key" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        value=$(echo "$value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
		# value=$(echo "$value" | sed 's/^"\(.*\)"$/\1/;s/^'\''\(.*\)'\''$/\1/')	# 对于带双引号或者单引号的值，去掉引号
        export "$key=$value"	# 导出变量
		# eval "$key=$value"
    done < "$file"
}

parse_config "config.env"
echo "Host: $DB_HOST, Port: $DB_PORT, Debug: $DEBUG"
```

## 位置参数
```bash
$*：将所有位置参数合并为一个字符串，默认以 IFS（内部字段分隔符，通常是空格）连接
$@：表示所有位置参数的列表，保持每个参数的独立性

使用 $* 的场景：需要将所有参数作为一个整体字符串传递时
log() {
    echo "日志: $*" >> log.txt
}
log "操作成功" "用户: alice"	# log.txt 内容：日志: 操作成功 用户: alice

使用 $@ 的场景：需要逐个处理参数，或将参数传递给其他命令时
# 1
wrapper() {
    ls "$@"
}
wrapper -l dir1 "dir 2"		   # 等价于 ls -l dir1 "dir 2"，保持参数独立
# 2
call_cmd() {
    some_cmd "$@"
}
```

## 返回值 和 退出状态
```bash
add_nums() {
    if [ "$#" -lt 2 ]; then
        echo "Need at least 2 args"
        return 1
    fi
    
    local sum=0
    for num in "$@"; do
        sum=$(( $sum + $num ))
    done
    
    echo $sum		# 使用 echo 捕获返回值
    return 0		# 使用 return 返回状态码
}

# 2
add_nums() {
    if [ "$#" -lt 2 ]; then
        echo "Need at least 2 args"
        return 1
    fi
    
    local sum=0
    while [ "$#" -gt 0 ]; do
        sum=$(( $sum + $1 ))
        shift
    done
    
    echo $sum
    return 0
}

result=$(add_nums 2 3)
echo "Result: $result"

# 3 (return multiple values via Array)
my_function() {
    echo "John Doe 30"
}

read name surname age < <(my_function)
echo "Name: $name, Surname: $surname, Age: $age"

# return both value and exit code
divide_numbers() {
    local num1=$1
    local num2=$2

    if [[ $num2 -eq 0 ]]; then
        echo "Error: Division by zero"
        return 1  # Exit code 1 for error
    fi

    echo "$((num1 / num2))"
    return 0  # Exit code 0 for success
}

result=$(divide_numbers 10 2)
exit_code=$?

if [[ $exit_code -eq 0 ]]; then
    echo "Result: $result"
else
    echo "Failed: $result"
fi

# use associative arrays for key-value pairs
declare -A user_info

get_user_info() {
    user_info[name]="Alice"
    user_info[age]=28
    user_info[city]="New York"
}

get_user_info

echo "Name: ${user_info[name]}, Age: ${user_info[age]}, City: ${user_info[city]}"

# return JSON data
get_user_json() {
    echo '{"name": "Alice", "age": 28, "city": "New York"}'
}

json_result=$(get_user_json)
echo "User JSON: $json_result"

# Extract values using jq (requires jq package)
name=$(echo "$json_result" | jq -r '.name')
age=$(echo "$json_result" | jq -r '.age')

echo "Extracted Name: $name, Age: $age"

```

## 最佳实践
```bash
## 计算文件行数
# 参数: $1 - 文件路径
# 返回: 行数（通过 echo 输出），状态码 0 表示成功
count_lines() {
    local file="$1"
    local count

    if [ ! -f "$file" ]; then
        echo "错误: 文件 '$file' 不存在" >&2
        return 1
    fi

    count=$(wc -l < "$file")
    echo "$count"
    return 0
}

# 主逻辑
main() {
    local result
    result=$(count_lines "test.txt") || {
        echo "处理失败，退出"
        exit 1
    }
    echo "文件行数: $result"
}

main "$@"
```

## BATS(Bash Automated Testing System) 开元测试框架
```bash
# sudo apt-get install bats

# deploy.sh
deploy() {
        echo "deploy $1"
}

# test_deploy.bats
#!/usr/bin/env bats

load 'deploy.sh'

@test "test deploy" {
        run deploy "server1"
        [ "$status" -eq 0 ]
        [ "$output" = "deploy server1" ]
}
```

## 检查进程是否存在
```bash
# kill -0 pid 会向进程发送一个信号 0。这个信号不会被进程识别或处理，它只是用来测试进程是否存在
# 如果进程存在，kill -0 pid 的退出状态（exit status）为 0；如果进程不存在或当前用户没有权限发送信号给该进程，退出状态为非零值
if kill -0 $PID 2>/dev/null; then
    echo "进程 $PID 正在运行"
else
    echo "进程 $PID 已停止或不存在"
fi
```

## 进程替换
```bash
# <(...) 是 进程替换（process substitution） 的语法, 命令 ... 的输出被视为一个临时文件，<(...) 生成一个文件名（通常是 /dev/fd/<fd> 的形式）
# CMD < <(...) 该命令的作用是将 <(...) 生成的文件作为输入传递给前一个 <, 即前一个 < 是输入重定向的符号

##  应用：等待所有后台作业完成

# 启动两个后台作业
echo "Starting job 1..."
sleep 3 &  # 后台作业1
pid1=$!

echo "Starting job 2..."
sleep 6 &   # 后台作业2
pid2=$!

# 等待所有后台作业完成
wait < <(jobs -p)	# 命令 jobs -p 用于返回所有后台进程的 PID，wait 命令用于等待当前shell下所指定的后台进程结束（如果不指定参数，则代表所有的后台进程）
# wait $pid1 $pid2
# wait %1 %2

echo "All background jobs are done!"
```

## debug tips
```bash
#1 Use set -e for Error Handling
set -e  # Stop script on error
set -u  # Treat unset variables as errors

#2 Use "$@" for Handling Arguments, Use "$@" to reference all script arguments correctly, preserving spaces and special characters
for arg in "$@"; do
    echo "$arg"
done

#3 Check Command Success with $?
cp file1.txt file2.txt
if [ $? -eq 0 ]; then
    echo "Copy successful"
else
    echo "Copy failed"
fi

#4 Use Functions to Reuse Code
greet() {
    echo "Hello, $1!"
}
greet "Alice"  # Outputs: Hello, Alice!

#5 Use [[ for Safer Conditionals: Prefer [[ over [ for conditionals. It handles complex conditions better, especially with strings and regex
if [[ "$name" == "Alice" ]]; then
    echo "Hello, Alice!"
fi

#6 Use trap to Handle Signals: Use trap to clean up resources when the script is interrupted (e.g., on Ctrl + C).
trap 'echo "Script interrupted"; exit' INT

#7 Use Loops Efficiently: Use for, while, and until loops to process data efficiently
for file in *.txt; do
    echo "Processing $file"
done

#8 Use Parameter Expansion for Defaults: Provide default values for variables using ${VAR:-default}
name=${1:-"World"}  # If no argument is provided, default to "World"
echo "Hello, $name"

#9 Debug with set -x or bash -x: Use set -x within a script or run it with bash -x to print each command as it’s executed (great for debugging)
set -x  # Enable debugging
# Your code
set +x  # Disable debugging

#10 Use declare for Typed Variables: Use declare to enforce types on variables (like integers or read-only variables)
declare -i counter=10  # Integer variable
declare -r readonly_var="Cannot change me"  # Read-only variable

#11 use local keyword inside of function
my_function() {
  local count=5
  for ((i=1; i<=count; i++)); do
    echo "Iteration $i"
  done
}
```

## 解析命令行参数
```bash
#1 直接解析命令行参数
echo "script name: $0"
echo "1st param: $1"
echo "2st param: $2"
echo "all params: $@"
echo "param num: $#"

#2 使用 getopts 解析短选项选项和参数
# a:表示 -a 选项必须带有一个参数，c 表示 -c 选项不需要参数
# : 在开头表示静默错误处理模式，可以在 \? 分支中自定义错误处理，否则如果遇到无效选项，getopts 会自动输出错误信息到标准错误流
while getopts ":a:b:c" opt; do
  case $opt in
    a)
      echo "option -a value: $OPTARG"
      ;;
    b)
      echo "option -b value: $OPTARG"
      ;;
    c)
      echo "option -c enabled"
      ;;
    \?)
      echo "invalid option: -$OPTARG"
      ;;
    :)
      echo "option -$OPTARG needs a param"
      ;;
  esac
done

#3 自定义方式处理长选项
usage() {
	echo "usage: $0 [-a value1] [-b value2] [-h]"
}

while [[ "$1" != "" ]]; do
    case $1 in
        -a | --arg1 )
            shift
            ARG1=$1
            ;;
        -b | --arg2 )
            shift
            ARG2=$1
            ;;
        -h | --help )
            usage
            exit 0
            ;;
        * )
            echo "invalid option: $1"
            exit 1
    esac
    shift
done

echo "ARG1: $ARG1"
echo "ARG2: $ARG2"
```

```bash
# locale - get locale-specific information
morrism@PC24036:~/testdir$ locale -a
C
C.utf8
en_US.utf8
POSIX
zh_CN.utf8

morrism@PC24036:~/testdir$ echo $LANG
en_US.utf8

# 1 (preferred)
function_name() {
# Function code here
}

# 2 (not recommended)
function function_name() {
# Function code here
}

# 函数参数
Argument						Role
$0								Reserves the function's name when defined in the terminal. When defined in a bash script, $0 returns the script's name and location.
$1, $2, etc.					Corresponds to the argument's position after the function name.
$#								Holds the count of positional arguments passed to the function.
$@ and $*						Hold the positional arguments list and function the same when used this way.
"$@"							Expands the list to separate strings. For example "$1", "$2", etc.
"$*"							Expands the list into a single string, separating parameters with a space. For example "$1 $2" etc.
```


[GNU Coreutils](https://www.gnu.org/software/coreutils/manual/html_node/index.html)

[Bash Shebang](https://linuxize.com/post/bash-shebang/)
[What is Shebang in Bash? | Your Script Interpreter Guide](https://ioflood.com/blog/shebang-bash/)
[How does the #! shebang work?](https://stackoverflow.com/questions/3009192/how-does-the-shebang-work)
[The #! magic, details about the shebang/hash-bang mechanism on various Unix flavours](https://www.in-ulm.de/~mascheck/various/shebang/)
```bash
This sequence of characters (#!) is called shebang and is used to tell the operating system which interpreter to use to parse the rest of the file.

Shebang Interpreter Directive :
	#!interpreter [arguments]

	1.The directive must be the first line in the script.
	2.The directive must start with shebang #!
	3.White space after the shebang characters is optional.
	4.Interpreter is the full path to a binary file (ex: /bin/sh, /bin/bash).
	5.Interpreter arguments are optional.

Two ways to use the Shebang directive and set the interpreter:
1.Using the absolute path to the bash binary:
	#!/bin/bash
2.Using the env utility:
	#!/usr/bin/env bash
	it will search for the bash executable in the user’s $PATH environmental variable. If there are more than one paths to bash, the first one will be used by the script
```


bash下的特殊变量
```bash
morrism@localhost ~ $ echo $BASHPID
589311	# 当前bash的进程ID，在某些场景下可能与 $$ 的结果不同
morrism@localhost ~ $ echo $$
589311	# 当前bash的进程ID
morrism@localhost ~ $ echo $PPID
112411	# 当前bash的进程的父进程ID
```

options
```bash
-c string If  the -c option is present, then commands are read from string.  If there are arguments after
          the string, they are assigned to the positional parameters, starting with $0.

#1 bash xxx
读取 xxx 文件的内容，并将 xxx 的内容作为 bash 脚本命令执行

# bash -c xxx
将 xxx 字符串作为 bash 命令进行执行，等价于 start a bash session and execute xxx command，如果后面跟位置参数则位置参数作为 xxx 的参数
bash -c 'echo "$0" "$1"' foo bar	# foo bar
bash -c 'echo "$@"' bash foo bar	# foo bar


-l        Make bash act as if it had been invoked as a login shell (see INVOCATION below).
```

```bash
# 常用快捷键
CTRL+A              # 移动到行首，同 <Home>
CTRL+B              # 向后移动，同 <Left>
CTRL+C              # 结束当前命令
CTRL+D              # 删除光标前的字符，同 <Delete> ，或者没有内容时，退出会话
CTRL+E              # 移动到行末，同 <End>
CTRL+F              # 向前移动，同 <Right>
CTRL+K              # 删除光标位置到行末的内容
CTRL+R              # 历史命令反向搜索，使用 CTRL+G 退出搜索
CTRL+T              # 交换前后两个字符
CTRL+U              # 删除字符到行首
CTRL+V              # 输入字符字面量，先按 CTRL+V 再按任意键
CTRL+W              # 删除光标左边的一个单词

ALT+b               # 向后（左边）移动一个单词
ALT+d               # 删除光标后（右边）一个单词
ALT+f               # 向前（右边）移动一个单词
ALT+t               # 交换单词
```

## bash脚本中调用python返回的结果
```bash
#!/usr/bin/bash
pc1()
{
	ip_expr="$1"
	if [[ $# -eq 0 || $1 = '-' ]]; then
		read -r ip_expr
	fi

	python -c 'print('"$ip_expr"')'
}

pc2()
{
	python -c 'import sys; print(eval(sys.argv[1]))' "$1" ;
}

pc2()
{
	python -c '\
		import sys;\
		print(eval(sys.argv[1]))\
		' "$1" ;
}

echo '1+2*3' | pc1
pc2 '1+2*3'
```

```bash
# getopts
usage() { 
	echo "Usage: $0 [-p <80|443>] [-h <string>] [-f]" 1>&2
	exit 1
}

while getopts ":p:h:f" o; do
	case "${o}" in
		p)
			PORT=${OPTARG}
			[[ $PORT != "80" && $PORT != "443" ]] && usage
			;;
		h)
			HOST=${OPTARG}
			;;
		f)  
			FORCE=1
			;;
		:)  
			echo "ERROR: Option -$OPTARG requires an argument"
			usage
			;;
		\?)
			echo "ERROR: Invalid option -$OPTARG"
			usage
			;;
	esac
done
shift $((OPTIND-1))

# Check required switches exist
if [ -z "${PORT}" ] || [ -z "${HOST}" ]; then
	usage
fi

echo "p = ${PORT}"
echo "h = ${HOST}"

# check user is root or not
check_root () {
if [[ $EUID -ne 0 ]]; then
	echo "This script must be run as root" 
	exit 1
fi
}

# 使用统一的方式执行命令
startup() {
		case "$1" in
				*)
					"$@"
					;;
		esac
}

startup ls -l /tmp
  
# git prompt (可以放在~/.bashrc文件里)
color_my_prompt {
	local __user_and_host="\[\033[01;32m\]\u@\h"
	local __cur_location="\[\033[01;34m\]\w"
	local __git_branch_color="\[\033[31m\]"
	#local __git_branch="\`ruby -e \"print (%x{git branch 2> /dev/null}.grep(/^\*/).first || '').gsub(/^\* (.+)$/, '(\1) ')\"\`"
	local __git_branch='`git branch 2> /dev/null | grep -e ^* | sed -E  s/^\\\\\*\ \(.+\)$/\(\\\\\1\)\ /`'
	local __prompt_tail="\[\033[35m\]$"
	local __last_color="\[\033[00m\]"
	export PS1="$__user_and_host $__cur_location $__git_branch_color$__git_branch$__prompt_tail$__last_color "
}
color_my_prompt

# bash实现spin功能
SPIN='-\|/'
spin() {
		i=0
		while kill -0 $1 2> /dev/null
		do
				i=$(( (i+1)%4 ))
				printf "\b${SPIN:$i:1}"
				sleep .1
		done
		printf "\bDONE\n"
}

echo 1
sleep 20 &
spin $!

echo 2
sleep 20 &
spin $!

echo 3
sleep 20 &
spin $!

使用场景：
Untaring build/macfie-powerpc.tgz ---> DONE
Untaring build/nms-ccap-en.tgz ---> DONE
.......

# ($!) Expands to the process ID of the job most recently placed into the background,  whether executed as an asynchronous command or using the bg builtin


# while true
SPIN='-\|/'

i=0
# while true
while :
do
		i=$(( (i+1)%4 ))
		printf "\b${SPIN:$i:1}"
		sleep .1
done

  
# 分别根据路径获取目录和文件名
dirname /path/to/file.txt
basename /path/to/file.txt

#Bash自动产生的变量 Bash automatically assigns variables that provide information about the current user 
# about the current user
UID, EUID, GROUPS
# aout the current host
HOSTTYPE, OSTYPE, MACHTYPE, and HOSTNAME
# about the instance of Bash that is running
BASH, BASH_VERSION, and BASH_VERSINFO

# 检查当前用户是否为root
check_root () {
if [[ $EUID -ne 0 ]]; then
	echo "This script must be run as root" 
	exit 1
fi
}

# 重复执行命令多次
repeat() {
	for ((i=0;i<$1;i++)); do
		eval ${*:2}
	done
}
# usage: repeat 5 echo "1 2 3"

# 获取当前执行脚本的路径
MY_PATH=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
PYTHON_PATH=${MY_PATH}/../cathcart-utils/python
export PYTHONPATH=${PYTHON_PATH}:${PYTHONPATH}

```
``` bash
	  BASH_SOURCE
	   An  array  variable  whose  members  are  the source filenames where the corresponding shell 
	   function names in the FUNCNAME array variable are defined.  The shell function ${FUNCNAME[$i]}
	   is defined in the file ${BASH_SOURCE[$i]} and called from ${BASH_SOURCE[$i+1]}.
	  
	  MY_PATH=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
	  PYTHON_PATH=${MY_PATH}/../cathcart-utils/python
	  export PYTHONPATH=${PYTHON_PATH}:${PYTHONPATH}

		arr=()	Create an empty array
		arr=(1 2 3)	Initialize array
		${arr[2]}	Retrieve third element
		${arr[@]}	Retrieve all elements
		${!arr[@]}	Retrieve array indices
		${#arr[@]}	Calculate array size
		arr[0]=3	Overwrite 1st element
		arr+=(4)	Append value(s)
		str=$(ls)	Save ls output as a string
		arr=( $(ls) )	Save ls output as an array of files
		${arr[@]:s:n}	Retrieve n elements starting at index s
```
[An introduction to Bash arrays](https://opensource.com/article/18/5/you-dont-know-bash-intro-bash-arrays)

## bash算术运算
```bash
$((expression))	# (())用来求值, $用来保存结果
awk 'BEGIN { x = 2; y = 3; print "x + y = "(x+y) }'
echo "2+3" | bc
expr 2 + 3
let x=2+3 | echo $x
echo "2.3 * 3.2" | bc -l
perl -e 'print 2.3*3.2'

number=1
echo $((++number))
echo $((number++))

calculate() { printf "%s\n" "$@" | bc -l; }
calculate 1 + 2

base#number
echo $((2#1010+2#1010))	# 二进制
echo $((010+010))		# 八进制
echo $((0xA+0xA))		# 十六进制
```

## 数组
```bash
# 1
files=(*.txt)
for file in ${files[@]}
do
	echo $file
done
# 2
files=()
while read -r -d ''
do
	files+=("$REPLY")
done < <(find . -name '*.txt' -print0)

echo ${files[@]}

# 3
dec2bin=({0..1}{0..1}{0..1}{0..1}{0..1}{0..1}{0..1}{0..1})
echo ${dec2bin[25]}

# 4
month=("Jan" "Feb" "Mar" "Apr" "May" "Jun" "Jul" "Aug" "Sep" "Oct" "Nov" "Dec")
echo ${month[3]}
```
## string operations
[Bash String Manipulation](https://earthly.dev/blog/bash-string/)  
[Advanced String Operations in Bash: Building Custom Functions](https://karandeepsingh.ca/posts/advanced-string-operations-in-bash-building-custom-functions/)  
[String Manipulation in Bash Script: A Linux Shell Guide](https://ioflood.com/blog/bash-string-manipulation/)  
[Bash string manipulation cheatsheet](https://gist.github.com/magnetikonline/90d6fe30fc247ef110a1)  
[Manipulating Strings](https://tldp.org/LDP/abs/html/string-manipulation.html)  
[]()  
[]()  
[]()  
[]()  
[Regular expressions](https://computing.stat.berkeley.edu/tutorial-using-bash/regex.html)  
[How to Master String Manipulation in Bash Scripting](https://labex.io/tutorials/shell-how-to-master-string-manipulation-in-bash-scripting-392539)  
[**Manipulating Strings**](https://tldp.org/LDP/abs/html/string-manipulation.html)  
[How to Master Bash Regex Patterns](https://labex.io/tutorials/shell-how-to-master-bash-regex-patterns-390416)  
[How to Use Regex in Bash Scripting](https://labex.io/tutorials/shell-how-to-use-regex-in-bash-scripting-392579)  
```bash
# ​​语法​​	​                 ​方向​​	​​          示例输入​​	        ​输出​​	​​  说明​
# ${var%pattern}	    结尾最短匹配	file.txt.bak	file.txt	删除最短匹配后缀
# ${var%%pattern}	    结尾最长匹配	file.txt.bak	file	    删除最长匹配后缀
# ${var#pattern}	    开头最短匹配	/usr/bin/ls	    usr/bin/ls	删除最短匹配前缀
# ${var##pattern}	    开头最长匹配	/usr/bin/ls	    ls	        删除最长匹配前缀
# ​​${var/%pattern/str}​​	​​结尾精确替换​​	eth1np0	        eth1	    替换结尾匹配部分

### 1. 获取字符串长度
string="hello"
echo ${#string} # 5

### 2. 提取子字符串 ${string:position}, ${string:position:length}
string="hello world"
echo ${string:6:5}  # world

### 3. 查找字符串中的字符是否在字符集中出现，返回第一个出现在字符集中的字符的索引
string="hello world"
echo $(expr index "$string" "world")  # 3

### 4. 替换子字符串
#### 替换第一个匹配的子字符串 ${string/pattern/replacement}
string="hello world"
echo ${string/world/universe} # hello universe

#### 替换所有匹配的子字符串 ${string//pattern/replacement}
string="hello world world"
echo ${string//world/universe}  # hello universe universe

### 5. 删除子字符串
#### 从开头删除最短匹配
string="path/to/file.txt"
echo ${string#*/}   # to/file.txt

#### 从开头删除最长匹配
string="path/to/file.txt"
echo ${string##*/}  # file.txt

#### 从结尾删除最短匹配
string="path/to/file.txt"
echo ${string%.txt} # path/to/file

#### 从结尾删除最长匹配
string="path/to/file.txt"
echo ${string%%/*}  # path

interface="eth0np0"
base_if=${interface/%np[0-9]/}  # 得到 "eth0"

### 6. 转换大小写
#### 转换为小写
string="HELLO WORLD"
echo ${string,,}    # hello world

#### 转换为大写
string="hello world"
echo ${string^^}    # HELLO WORLD

### 7. 拼接字符串
string1="hello"
string2="world"
echo "$string1 $string2"  # hello world

string1+=" world"
echo $string1 # hello world

### 8. 检查字符串是否为空
string=""
if [ -z "$string" ]; then
  echo "String is empty"  # 
else
  echo "String is not empty"
fi

### 9. 检查字符串是否非空
string="hello"
if [ -n "$string" ]; then
  echo "String is not empty"  # 
else
  echo "String is empty"
fi

### 替换从左开始的匹配 ${string/#pattern/replacement}
str="aa bb bb aa"
echo ${str/#aa/cc}	# cc bb bb aa

### 替换从右开始的匹配 ${string/%pattern/replacement}
str="aa bb bb aa"
echo ${str/%aa/cc}	# aa bb bb cc

### 字符串删除
str="aa bb bb aa"
# 删除首次匹配
echo ${str/aa}	# bb bb aa
# 删除所有匹配
echo ${str//aa}	# bb bb

### 字符串匹配删除
filename="bash.string.txt"
## 最短匹配
# 从前开始匹配
echo ${filename#*.}	# string.txt
# 从后开始匹配
echo ${filename%.*}	# bash.string

## 最长匹配
# 从前开始匹配
echo ${filename##*.}	# txt
# 从后开始匹配
echo ${filename%%.*}	# bash

### 字符串大小写转换
legend="john nash"
actor="JULIA ROBERTS"

echo ${legend^^}		# JOHN NASH
echo ${actor,,}			# julia roberts
echo ${legend^}			# John nash
echo ${actor,}			# jULIA ROBERTS
echo ${legend^^[jn]}	# JohN Nash

### 正则匹配
file="repair-report-12.5.pdf"
pattern='([0-9]*\.[0-9]*\.pdf)'

if  [[ $file =~ $pattern ]]; then
	echo ${BASH_REMATCH[1]}		# 12.5.pdf
else
	echo "No version found"
fi

```

## expr operations
```bash
### 1. 数值运算
echo $(expr 3 + 2)  # 5
echo $(expr 5 - 2)  # 3
echo $(expr 3 \* 2) # 6
echo $(expr 6 / 2)  # 3
echo $(expr 5 % 2)  # 1

### 2. 字符串操作
string="hello"
echo $(expr length "$string")   # 5
string="hello world"
echo $(expr substr "$string" 7 5)   # world
string="hello world"
echo $(expr index "$string" "world")    # 3

### 3. 比较运算
echo $(expr 5 = 5)  # 1
echo $(expr 5 != 4) # 1
echo $(expr 5 \> 4) # 1
echo $(expr 4 \< 5) # 1
echo $(expr 5 \>= 5)    # 1
echo $(expr 4 \<= 5)    # 1

### 4. 逻辑运算
echo $(expr 1 \& 1)   # 1
echo $(expr 0 \| 1)  # 1
```

## arithmetic operations
```bash
### 1. 使用 `expr`
echo $(expr 3 + 2)  # 5
echo $(expr 5 - 2)  # 3
echo $(expr 3 \* 2) # 6
echo $(expr 6 / 2)  # 3
echo $(expr 5 % 2)  # 1

### 2. 使用双括号 `(( ))`
echo $((3 + 2)) # 5

myNum=100
((myNum+=200))
echo $myNum # 300
echo $((5 - 2)) # 3
echo $((3 * 2)) # 6
echo $((6 / 2)) # 3
echo $((5 % 2)) # 1

### 3. 使用 `let`
let result=3+2
echo $result    # 5
let result=5-2
echo $result    # 3
let result=3*2
echo $result    # 6
let result=6/2
echo $result    # 3
let result=5%2
echo $result    # 1

### 4. 使用 `bc` 进行浮点运算
echo $(echo "3.5 + 2.1" | bc)   # 5.6
echo $(echo "5.5 - 2.1" | bc)   # 3.4  
echo $(echo "3.5 * 2.1" | bc)   # 7.35
echo $(echo "scale=2; 6.5 / 2.1" | bc)   # 3.09
```

[Matching regex in bash](https://thedukh.com/2022/10/matching-regex-in-bash/)  
[Bash Reference Manual](https://www.gnu.org/software/bash/manual/html_node/index.html#SEC_Contents)  
[How to Declare and Access Associative Array in Bash](https://phoenixnap.com/kb/bash-associative-array)  
[Bash Scripting – Associative Array Explained With Examples](https://ostechnix.com/bash-associative-array/)  
[Take control of your data with associative arrays in Bash](https://opensource.com/article/20/6/associative-arrays-bash)  
[Bash shift builtin command](https://www.computerhope.com/unix/bash/shift.htm)  
[A Complete Guide On How To Use Bash Arrays](https://www.shell-tips.com/bash/arrays/#gsc.tab=0)  
[**How to Use Command Line Arguments in a Bash Script**](https://www.baeldung.com/linux/use-command-line-arguments-in-bash-script)  
[pure bash bible](https://github.com/dylanaraps/pure-bash-bible?tab=readme-ov-file)  
[Bash getopts builtin command](https://www.computerhope.com/unix/bash/getopts.htm)  
[Parsing bash script options with getopts](https://sookocheff.com/post/bash/parsing-bash-script-arguments-with-shopts/)  
[Parse Command Line Arguments in Bash](https://www.baeldung.com/linux/bash-parse-command-line-arguments)  
[bash(1)](https://manpages.org/bash)  
[GNU Bash Reference Manual](https://www.linuxtopia.org/online_books/bash_reference_guide/index.html)  
[Advanced Bash-Scripting Guide](https://www.linuxtopia.org/online_books/advanced_bash_scripting_guide/index.html) #online  
[Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html#)  
[The set Command in Linux](https://www.baeldung.com/linux/set-command)  
[Pattern Matching](https://www.gnu.org/software/bash/manual/bash.html#Pattern-Matching)  
[Pattern Matching In Bash](https://www.linuxjournal.com/content/pattern-matching-bash)  
[Special Parameters](https://www.gnu.org/software/bash/manual/bash.html#Shell-Parameters)  
[阮一峰 Bash 脚本教程](https://www.bookstack.cn/books/bash-tutorial)  
[Bash scripting cheatsheet](https://devhints.io/bash)  
[Advanced Bash-Scripting Guide](https://tldp.org/LDP/abs/html/)  
[Shell Style Guide](https://google.github.io/styleguide/shellguide.html)  
[syntax brackets](https://ss64.com/bash/syntax-brackets.html)  
[Shell Scripting Primer](https://developer.apple.com/library/archive/documentation/OpenSource/Conceptual/ShellScripting/Introduction/Introduction.html#//apple_ref/doc/uid/TP40004268)  
[bash style guide](https://github.com/bahamas10/bash-style-guide) #github  
[coding standards](https://linuxcommand.org/lc3_adv_standards.php)  
[man sh](https://linux.die.net/man/1/sh)  
[编写健壮的 Shell 脚本](https://morven.life/posts/how-to-write-robust-shell-script/)  
[Everything you never wanted to know about ANSI escape codes](https://notes.burke.libbey.me/ansi-escape-codes/)  
[ANSI escape code generator](https://ansi.gabebanks.net/)  
[explain shell](https://explainshell.com/) #online  
[Find the Script’s Filename Within the Same Script in Bash](https://www.baeldung.com/linux/find-bash-script-filename)  
[Changing the Default Shell in Linux](https://www.baeldung.com/linux/change-default-shell)  
[Include Files in a Bash Shell Script With source Command](https://www.baeldung.com/linux/source-include-files)  
[Bash Source Command](https://linuxize.com/post/bash-source-command/)  
[BASH TIPS & TRICKS](https://tecadmin.net/category/bash-tips-tricks/)  
[bash-utility](https://github.com/labbots/bash-utility) #online #github  
[Google Style Guides](https://google.github.io/styleguide/) #online  
[pure-sh-bible](https://github.com/dylanaraps/pure-sh-bible)  