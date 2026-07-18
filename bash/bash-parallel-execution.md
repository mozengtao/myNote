# Bash 并行执行方法总结

## 0. 概览

本文系统总结 bash 脚本里常见的 6 类并行执行手段，从最基础到相对高级：

1. [`&` + `wait`](#1--wait--最基础的后台任务与批量等待) —— 最基础的后台任务与批量等待
2. [`jobs`](#2-jobs--任务监控与并发数控制) —— 任务监控与并发数控制
3. [`xargs -P`](#3-xargs--p--基于流水线输入的并行执行) —— 基于流水线输入的并行执行
4. [GNU `parallel`](#4-gnu-parallel--专业并行工具) —— 专业并行工具
5. [`mkfifo` 信号量池](#5-mkfifo-信号量池--令牌桶式并发限流) —— 令牌桶式并发限流
6. [`flock`](#6-flock--并发环境下的互斥与协调) —— 并发环境下的互斥与协调（配合前 5 种方法使用）

每类方法给出 3 个完整、可独立运行的典型脚本示例；此外第7节专门讨论并行场景下常见的**输出乱序**问题及 `$(...)` 缓冲方案，文末附[方法对比表](#8-方法对比表)与[最佳实践清单](#9-最佳实践清单)。

除非特别说明，示例默认假定运行在 Linux + bash 4+ 环境。

---

## 1. `&` + `wait` —— 最基础的后台任务与批量等待

`cmd &` 把命令丢到后台执行，`$!` 拿到刚启动的后台进程 PID；`wait` 不带参数会等待**当前 shell 的所有后台任务**结束，`wait "$pid"` 只等待指定 PID，并把该进程的退出码作为 `wait` 自身的返回值。这是并行的基础手段：不控制并发数，直接把所有任务一次性甩到后台。

### 示例1：并行下载一组URL，收集失败项

```bash
#!/usr/bin/env bash
set -uo pipefail

urls=(
	"https://example.com/a"
	"https://example.com/b"
	"https://example.com/c"
	"https://example.com/d"
)

save_dir="./downloads"
mkdir -p "$save_dir"

fetch_one() {
	local url="$1"
	local out="$2"
	curl -fsSL --max-time 10 -o "$out" "$url"
}

pids=()
url_of_pid=()

for url in "${urls[@]}"; do
	fname="$save_dir/$(basename "$url").html"
	fetch_one "$url" "$fname" &
	pids+=("$!")
	url_of_pid+=("$url")
done

failed=()
for i in "${!pids[@]}"; do
	if ! wait "${pids[$i]}"; then
		failed+=("${url_of_pid[$i]}")
	fi
done

if ((${#failed[@]} > 0)); then
	echo "以下URL下载失败:"
	printf ' - %s\n' "${failed[@]}"
	exit 1
fi

echo "全部下载成功"
```

要点：用两个平行数组 `pids` / `url_of_pid` 把 PID 和任务参数对应起来，逐个 `wait "$pid"` 才能拿到**每个任务各自**的退出码；直接裸 `wait`（不带参数）只能知道"有没有任务失败"的整体情况，无法定位是哪一个。

### 示例2：并行压缩日志文件，最后统一汇总

```bash
#!/usr/bin/env bash
set -uo pipefail

log_dir="./logs"
archive_dir="./archive"
mkdir -p "$archive_dir"

mapfile -t log_files < <(find "$log_dir" -maxdepth 1 -type f -name '*.log')

pids=()
file_of_pid=()

for f in "${log_files[@]}"; do
	base="$(basename "$f")"
	gzip -c "$f" > "$archive_dir/${base}.gz" &
	pids+=("$!")
	file_of_pid+=("$f")
done

fail_count=0
for i in "${!pids[@]}"; do
	if ! wait "${pids[$i]}"; then
		echo "压缩失败: ${file_of_pid[$i]}" >&2
		((fail_count++))
	fi
done

echo "共处理 ${#log_files[@]} 个文件，失败 $fail_count 个"
((fail_count == 0))
```

要点：这种"全量后台 + 最后统一 wait"的方式没有并发上限，适合任务数量不多、且每个任务本身较轻的场景；任务数一旦上到成百上千，会瞬间占满 CPU/IO/文件描述符，这时应换成第 2、5 节的限流方式。

### 示例3：并行执行多个独立健康检查函数

```bash
#!/usr/bin/env bash
set -uo pipefail

check_disk() {
	df -h / | awk 'NR==2 {print "disk_usage=" $5}'
}

check_mem() {
	free -m | awk '/Mem:/ {printf "mem_used_mb=%s\n", $3}'
}

check_load() {
	awk '{print "load1=" $1}' /proc/loadavg
}

check_net() {
	if ping -c1 -W2 8.8.8.8 >/dev/null 2>&1; then
		echo "net=ok"
	else
		echo "net=fail"
	fi
}

report_dir="$(mktemp -d)"
trap 'rm -rf "$report_dir"' EXIT

names=(disk mem load net)
funcs=(check_disk check_mem check_load check_net)
pids=()

for i in "${!funcs[@]}"; do
	"${funcs[$i]}" > "$report_dir/${names[$i]}.out" 2>&1 &
	pids+=("$!")
done

exit_codes=()
for i in "${!pids[@]}"; do
	wait "${pids[$i]}"
	exit_codes+=("$?")
done

echo "==== 健康检查报告 ===="
for i in "${!names[@]}"; do
	printf '[%s] exit=%s -> %s\n' "${names[$i]}" "${exit_codes[$i]}" "$(cat "$report_dir/${names[$i]}.out")"
done
```

要点：每个后台任务的输出重定向到**独立文件**（`report_dir/${names[$i]}.out`），而不是共享同一个文件，避免多个进程同时写导致内容交叉错乱；这是并行脚本里最简单也最重要的一条隔离原则。

---

## 2. `jobs` —— 任务监控与并发数控制

`jobs -p` 列出当前 shell 的后台任务 PID，`jobs -r` 只列出仍在运行的（running），`jobs -l` 打印 PID + 状态的详细信息。单独用 `jobs` 很少能直接完成并行，通常是配合 `wait -n`（等待"任意一个"后台任务结束）组成一个**并发数限流循环**：每提交一个新任务前，先检查当前运行中的任务数是否达到上限，达到就 `wait -n` 让出一个名额。

### 示例1：限并发批量探测主机存活

```bash
#!/usr/bin/env bash
set -uo pipefail

hosts=(host1 host2 host3 host4 host5 host6 host7 host8)
max_parallel="${MAX_PARALLEL:-3}"
result_dir="$(mktemp -d)"
trap 'rm -rf "$result_dir"' EXIT

probe_host() {
	local host="$1"
	if ping -c1 -W1 "$host" >/dev/null 2>&1; then
		echo "up" > "$result_dir/$host.status"
	else
		echo "down" > "$result_dir/$host.status"
	fi
}

for host in "${hosts[@]}"; do
	probe_host "$host" &
	while (( $(jobs -rp | wc -l) >= max_parallel )); do
		wait -n
	done
done
wait

echo "==== 主机存活探测结果 ===="
for host in "${hosts[@]}"; do
	printf '%-10s %s\n' "$host" "$(cat "$result_dir/$host.status" 2>/dev/null || echo unknown)"
done
```

要点：`jobs -rp | wc -l` 统计"运行中"的后台任务数，`wait -n` 只等一个任务结束就返回（而不是等全部），配合 `while` 循环就形成了"任务数超过上限就阻塞、有空位就继续提交"的限流池，这是脚本里最常见的并发控制写法。

### 示例2：限并发校验文件checksum，汇总失败清单

```bash
#!/usr/bin/env bash
set -uo pipefail

target_dir="${1:-.}"
max_parallel="${MAX_PARALLEL:-4}"
fail_list="$(mktemp)"
trap 'rm -f "$fail_list"' EXIT

verify_one() {
	local f="$1"
	if ! sha256sum -c "$f.sha256" >/dev/null 2>&1; then
		echo "$f" >> "$fail_list"
	fi
}

mapfile -t files < <(find "$target_dir" -maxdepth 1 -type f -name '*.sha256' -printf '%p\n' | sed 's/\.sha256$//')

for f in "${files[@]}"; do
	verify_one "$f" &
	while (( $(jobs -rp | wc -l) >= max_parallel )); do
		wait -n
	done
done
wait

if [[ -s "$fail_list" ]]; then
	echo "校验失败的文件:"
	cat "$fail_list"
	exit 1
fi
echo "全部文件校验通过"
```

要点：多个后台进程同时用 `>>` 追加写同一个 `fail_list` 文件，在 Linux 上单次 `write()` 只要不超过 `PIPE_BUF`（通常4KB）大小、且每次写入的是一行较短文本，内核会保证不交叉写入；但这只是"够用"，不是严格保证——如果单条记录可能很长或者对顺序有要求，应该换成第 6 节的 `flock` 方案。

### 示例3：长期调度循环中用 `jobs -l` 做进度可视化并清理超时任务

```bash
#!/usr/bin/env bash
set -uo pipefail

max_parallel=3
max_runtime=5   # 秒，超过视为卡住
task_count=10

worker() {
	local id="$1"
	local sleep_time=$(( (RANDOM % 8) + 1 ))
	sleep "$sleep_time"
	echo "task $id done after ${sleep_time}s"
}

declare -A start_time_of_pid

for ((i = 1; i <= task_count; i++)); do
	worker "$i" &
	pid=$!
	start_time_of_pid["$pid"]="$(date +%s)"

	while (( $(jobs -rp | wc -l) >= max_parallel )); do
		echo "---- 当前后台任务状态 ----"
		jobs -l
		now="$(date +%s)"
		for p in "${!start_time_of_pid[@]}"; do
			if kill -0 "$p" 2>/dev/null; then
				elapsed=$(( now - start_time_of_pid["$p"] ))
				if (( elapsed > max_runtime )); then
					echo "任务 pid=$p 运行超过 ${max_runtime}s，强制结束"
					kill "$p" 2>/dev/null
				fi
			fi
		done
		wait -n 2>/dev/null
	done
done

wait
echo "全部任务处理完成"
```

要点：`jobs -l` 除了 PID 还会打印任务编号和状态（Running/Done），适合在长期运行的守护脚本里周期性打印，做人工可读的进度输出；结合 `kill -0 "$pid"`（只探测进程是否存在，不真正发信号）可以配合时间戳字典实现简单的超时熔断。

---

## 3. `xargs -P` —— 基于流水线输入的并行执行

`xargs -P N` 从标准输入读取参数列表，最多同时启动 N 个子进程执行命令。配合 `-I{}`（每个参数替换一次命令模板）或 `-n1`（每次只取一个参数）非常适合"对列表里每一项跑同一个命令"的场景。要在 `xargs` 派生的 `bash -c` 子进程里调用当前 shell 定义的函数，需要先 `export -f 函数名`。

**注意**：`xargs` 本身的退出码只反映"是否有子命令失败"（有失败时通常是 123），并不会告诉你具体哪一项失败，需要在被调用的命令/函数内部自行记录失败项（做法与第1、2节一致）。

### 示例1：`find -print0` 配合 `xargs -0 -P` 并行压缩图片

```bash
#!/usr/bin/env bash
set -uo pipefail

src_dir="${1:-./images}"
max_parallel="${MAX_PARALLEL:-4}"

compress_one() {
	local f="$1"
	gzip -kf -- "$f"
	echo "compressed: $f"
}
export -f compress_one

find "$src_dir" -maxdepth 1 -type f \( -name '*.jpg' -o -name '*.png' \) -print0 |
	xargs -0 -P "$max_parallel" -I{} bash -c 'compress_one "$1"' _ {}
```

要点：`-print0` / `-0` 组合以 `\0` 作为分隔符，能正确处理文件名里含空格、换行等特殊字符的情况，是比直接按行分割更安全的写法。

### 示例2：批量并行请求API端点，各自落地独立文件

```bash
#!/usr/bin/env bash
set -uo pipefail

endpoints_file="${1:-endpoints.txt}"
out_dir="./responses"
max_parallel="${MAX_PARALLEL:-6}"
mkdir -p "$out_dir"

fetch_endpoint() {
	local url="$1"
	local tag
	tag="$(echo "$url" | md5sum | cut -c1-8)"
	if curl -fsSL --max-time 10 -o "$out_dir/$tag.json" "$url"; then
		echo "OK   $url"
	else
		echo "FAIL $url"
	fi
}
export -f fetch_endpoint

xargs -P "$max_parallel" -I{} bash -c 'fetch_endpoint "$1"' _ {} < "$endpoints_file"
```

要点：用 URL 的 md5 摘要作为落地文件名，天然避免了不同 URL 之间的文件名冲突，也省去了额外的序号管理逻辑。

### 示例3：并行调用当前 shell 定义的函数（`export -f`）

```bash
#!/usr/bin/env bash
set -uo pipefail

process_item() {
	local item="$1"
	sleep 1
	echo "processed: $item (pid=$$)"
}
export -f process_item

items=(alpha beta gamma delta epsilon zeta)
max_parallel="${MAX_PARALLEL:-3}"

printf '%s\n' "${items[@]}" |
	xargs -P "$max_parallel" -n1 -I{} bash -c 'process_item "$1"' _ {}
```

要点：`bash -c 'process_item "$1"' _ {}` 里的 `_` 占位符会赋值给 `$0`（脚本名，不使用），紧跟的 `{}` 才是真正的 `$1`；这是用 `xargs` 调用带参数函数的标准写法。

---

## 4. GNU `parallel` —— 专业并行工具

GNU `parallel` 是专门为"并行跑命令"设计的工具，相比 `xargs -P` 提供了更丰富的能力：多参数笛卡尔积（`:::`）、任务日志（`--joblog`，记录每个任务的耗时/退出码，方便失败重试）、失败策略（`--halt`）等。使用前建议先检查是否已安装（`command -v parallel`），因为它不是所有系统的标配。

### 示例1：`-j` 限并发处理文本列表，`--joblog` 记录任务日志

```bash
#!/usr/bin/env bash
set -uo pipefail

if ! command -v parallel >/dev/null 2>&1; then
	echo "未检测到 GNU parallel，请先安装（如 apt install parallel）" >&2
	exit 1
fi

input_file="${1:-records.csv}"
joblog="./parallel_joblog.txt"
max_parallel="${MAX_PARALLEL:-4}"

process_record() {
	local line="$1"
	IFS=',' read -r id name score <<< "$line"
	sleep 0.5
	echo "id=$id name=$name score=$score processed"
}
export -f process_record

parallel -j "$max_parallel" --joblog "$joblog" process_record {} :::: "$input_file"

echo "==== 任务日志 ===="
column -t "$joblog"
```

要点：`--joblog` 生成的表格里每行含 `Seq Host Starttime JobRuntime Send Receive Exitval Signal Command`，`Exitval` 非 0 的行就是失败任务，可以直接筛出来做重试（`parallel --retry-failed --joblog ...`）。

### 示例2：多参数笛卡尔积并行任务

```bash
#!/usr/bin/env bash
set -uo pipefail

if ! command -v parallel >/dev/null 2>&1; then
	echo "未检测到 GNU parallel，请先安装" >&2
	exit 1
fi

regions=(us eu ap)
services=(auth billing search)
max_parallel="${MAX_PARALLEL:-4}"

deploy_one() {
	local region="$1"
	local service="$2"
	echo "deploying $service to $region ..."
	sleep 1
	echo "deployed $service to $region"
}
export -f deploy_one

parallel -j "$max_parallel" deploy_one {1} {2} ::: "${regions[@]}" ::: "${services[@]}"
```

要点：两个 `:::` 输入源会自动做笛卡尔积（3 个region x 3 个service = 9 个任务），`{1}` `{2}` 分别引用第一个、第二个输入源的当前取值，比手写双重循环 + 后台任务更简洁。

### 示例3：`--halt now,fail=1` 一失败就整体中止

```bash
#!/usr/bin/env bash
set -uo pipefail

if ! command -v parallel >/dev/null 2>&1; then
	echo "未检测到 GNU parallel，请先安装" >&2
	exit 1
fi

max_parallel="${MAX_PARALLEL:-4}"

check_db() { echo "db check ok"; return 0; }
check_cache() { echo "cache check ok"; return 0; }
check_queue() { echo "queue check FAILED"; return 1; }
check_api() { echo "api check ok"; return 0; }
export -f check_db check_cache check_queue check_api

checks=(check_db check_cache check_queue check_api)

if ! parallel -j "$max_parallel" --halt now,fail=1 '{}' ::: "${checks[@]}"; then
	echo "存在检查失败，已中止后续任务" >&2
	exit 1
fi
echo "全部检查通过"
```

要点：`--halt now,fail=1` 表示"只要有1个任务失败，立即杀掉其余仍在运行的任务并退出"，适合"必须全部成功，一旦有失败就没必要继续跑剩下的"这类场景（如多环境部署、前置条件检查）；也可以用 `--halt now,success=1` 表示"任意一个成功就立刻停止"（如多个镜像源里选一个能连通的）。

---

## 5. `mkfifo` 信号量池 —— 令牌桶式并发限流

第2节 `jobs -rp | wc -l` 的限流方式本质是**轮询（忙等）**：循环里反复用 `wc -l` 计数，直到降到阈值以下。任务量大、循环间隔短时会有额外的进程调用开销。用命名管道（FIFO）模拟计数信号量是另一种经典手法：预先往管道里塞 N 个"令牌"，每个任务开始前先阻塞式读一个令牌（没有令牌就一直等，不占CPU），结束后再还回一个令牌——本质上是**阻塞等待**而不是轮询。

### 示例1：信号量函数库 + 严格限并发跑任务

```bash
#!/usr/bin/env bash
set -uo pipefail

max_parallel="${MAX_PARALLEL:-3}"
sem_dir="$(mktemp -d)"
sem_fifo="$sem_dir/sem.fifo"
mkfifo "$sem_fifo"

exec 3<>"$sem_fifo"
rm -f "$sem_fifo"

for ((i = 0; i < max_parallel; i++)); do
	printf 'x' >&3
done

cleanup() {
	exec 3>&-
	rm -rf "$sem_dir"
}
trap cleanup EXIT

sem_acquire() { IFS= read -r -n1 -u3 _; }
sem_release() { printf 'x' >&3; }

do_task() {
	local id="$1"
	local sleep_time=$(( (RANDOM % 3) + 1 ))
	echo "task $id 开始 (sleep ${sleep_time}s)"
	sleep "$sleep_time"
	echo "task $id 结束"
	sem_release
}

for ((i = 1; i <= 8; i++)); do
	sem_acquire
	do_task "$i" &
done

wait
echo "全部任务完成，最大并发数=$max_parallel"
```

要点：`mkfifo` 创建管道文件后立刻用 `exec 3<>"$sem_fifo"` 以读写模式打开到文件描述符3，随后 `rm -f` 删除文件名本身也不影响已经打开的fd（Linux下未被引用的inode会在fd关闭后才真正释放）；`sem_acquire` 用 `read -n1 -u3` 从fd3读1个字符（读不到就阻塞），`sem_release` 用 `printf 'x' >&3` 还回一个令牌。

### 示例2：信号量池并行下载，`trap ... RETURN` 保证异常也归还令牌

```bash
#!/usr/bin/env bash
set -uo pipefail

max_parallel="${MAX_PARALLEL:-3}"
sem_dir="$(mktemp -d)"
sem_fifo="$sem_dir/sem.fifo"
mkfifo "$sem_fifo"
exec 4<>"$sem_fifo"
rm -f "$sem_fifo"

for ((i = 0; i < max_parallel; i++)); do
	printf 'x' >&4
done
trap 'exec 4>&-; rm -rf "$sem_dir"' EXIT

sem_acquire() { IFS= read -r -n1 -u4 _; }
sem_release() { printf 'x' >&4; }

urls=(
	"https://example.com/f1"
	"https://example.com/f2"
	"https://example.com/f3"
	"https://example.com/f4"
	"https://example.com/f5"
)
out_dir="./downloads"
mkdir -p "$out_dir"

fetch_with_token() {
	local url="$1"
	local out="$out_dir/$(basename "$url")"
	trap 'sem_release' RETURN
	if curl -fsSL --max-time 10 -o "$out" "$url"; then
		echo "OK $url"
	else
		echo "FAIL $url"
	fi
}

for url in "${urls[@]}"; do
	sem_acquire
	fetch_with_token "$url" &
done

wait
echo "全部下载任务已结束"
```

要点：`trap 'sem_release' RETURN` 注册在函数内部，当该函数（在后台子shell中）执行完 `return`（无论是正常结束还是 `if` 分支提前退出）都会触发归还令牌，避免"任务内部提前 return 忘记释放令牌，导致池子越用越小最后死锁"的常见bug。

### 示例3：生产者-消费者流水线，固定大小worker pool

```bash
#!/usr/bin/env bash
set -uo pipefail

pool_size="${POOL_SIZE:-3}"
sem_dir="$(mktemp -d)"
sem_fifo="$sem_dir/sem.fifo"
mkfifo "$sem_fifo"
exec 5<>"$sem_fifo"
rm -f "$sem_fifo"

for ((i = 0; i < pool_size; i++)); do
	printf 'x' >&5
done
trap 'exec 5>&-; rm -rf "$sem_dir"' EXIT

sem_acquire() { IFS= read -r -n1 -u5 _; }
sem_release() { printf 'x' >&5; }

queue_dir="$(mktemp -d)"
task_queue="$queue_dir/queue.fifo"
mkfifo "$task_queue"
trap 'rm -rf "$queue_dir"' EXIT

producer() {
	for ((i = 1; i <= 12; i++)); do
		echo "job-$i"
	done > "$task_queue"
}

consume_job() {
	local job="$1"
	sleep 1
	echo "worker(pid=$$) 处理 $job 完成"
	sem_release
}

producer &
producer_pid=$!

while IFS= read -r job; do
	sem_acquire
	consume_job "$job" &
done < "$task_queue"

wait "$producer_pid" 2>/dev/null
wait
echo "流水线处理完成"
```

要点：这里用了两个FIFO——一个（fd5）当计数信号量控制并发度，另一个（`task_queue`）纯粹当"任务队列"传递生产者产出的任务项；`while read ... < "$task_queue"` 会一直阻塞读取直到生产者关闭写端（脱离 `for` 循环、重定向结束）为止，天然实现了"生产者按自己的节奏产出任务、消费者池按并发上限处理"的流水线解耦。

---

## 6. `flock` —— 并发环境下的互斥与协调

`flock` 本身不是"并行执行"的手段，而是配合前 5 种方法使用的**互斥锁**：当多个并行任务需要访问同一个共享资源（同一个文件、同一份计数器、同一个脚本实例）时，用 `flock` 保护临界区，避免竞态导致的数据错乱。核心用法是对一个"锁文件"打开一个文件描述符，再对该fd调用 `flock`：`flock -x fd` 阻塞式加排他锁，`flock -n fd` 非阻塞（拿不到锁立刻返回失败）。

### 示例1：多个并行worker写同一份汇总日志

```bash
#!/usr/bin/env bash
set -uo pipefail

log_file="./combined.log"
lock_file="./combined.log.lock"
: > "$log_file"

write_log() {
	local worker_id="$1"
	local msg="$2"
	(
		flock -x 200
		printf '[worker-%s] %s\n' "$worker_id" "$msg" >> "$log_file"
	) 200>"$lock_file"
}

worker() {
	local id="$1"
	for i in 1 2 3; do
		write_log "$id" "step $i"
		sleep 0.2
	done
}

for id in 1 2 3 4 5; do
	worker "$id" &
done
wait

echo "==== 汇总日志 ===="
cat "$log_file"
rm -f "$lock_file"
```

要点：`( flock -x 200; ...; ) 200>"$lock_file"` 这种"子shell + fd重定向"写法把加锁范围严格限定在子shell内，子shell结束时fd自动关闭、锁自动释放，不需要手动 `flock -u`；对比第2节示例2里"多个进程追加写同一文件"的做法，这里是真正互斥、不依赖 `PIPE_BUF` 之类的隐含假设。

### 示例2：防止脚本被并发重复触发（典型cron场景）

```bash
#!/usr/bin/env bash
set -uo pipefail

lock_file="/tmp/my_scheduled_job.lock"

exec 200>"$lock_file"
if ! flock -n 200; then
	echo "已有一个实例在运行，本次退出" >&2
	exit 0
fi

do_real_work() {
	echo "开始执行任务 (pid=$$) ..."
	sleep 5
	echo "任务执行完成"
}

do_real_work

flock -u 200
exec 200>&-
```

要点：`flock -n 200` 拿不到锁直接返回非0而不阻塞，非常适合cron这种"如果上一次还没跑完，这次就干脆跳过"的场景；锁会随进程退出自动释放，但脚本里显式 `flock -u` + `exec 200>&-` 让释放时机更明确、便于阅读。

### 示例3：多个并行任务共享一个计数器文件

```bash
#!/usr/bin/env bash
set -uo pipefail

counter_file="./counters.txt"
lock_file="./counters.txt.lock"
printf 'success=0\nfail=0\n' > "$counter_file"

bump_counter() {
	local key="$1"
	(
		flock -x 201
		local current
		current="$(awk -F= -v k="$key" '$1==k {print $2}' "$counter_file")"
		current=$(( current + 1 ))
		awk -F= -v k="$key" -v v="$current" '
			$1==k { print k"="v; next }
			{ print }
		' "$counter_file" > "$counter_file.tmp"
		mv "$counter_file.tmp" "$counter_file"
	) 201>"$lock_file"
}

simulate_task() {
	local id="$1"
	if (( id % 3 == 0 )); then
		bump_counter fail
	else
		bump_counter success
	fi
}

for id in $(seq 1 15); do
	simulate_task "$id" &
done
wait

echo "==== 最终统计 ===="
cat "$counter_file"
rm -f "$lock_file"
```

要点：这是典型的"读-改-写"临界区（读当前值 -> 加1 -> 写回），如果不加锁，多个并行进程几乎必然会读到同一个旧值再各自加1写回，导致计数丢失；`flock` 把"读-改-写"整体串行化，保证计数准确。

---

## 7. 并行输出乱序问题与 `$(...)` 缓冲方案

前面几节都提到过"多个后台任务同时写同一个文件/终端会导致内容交叉"，第2节示例2解释了单次 `write()` 在 `PIPE_BUF` 大小以内是原子的，不会出现**一行内容被截断**的情况。但这只解决了"单次写"层面的问题，没有解决另一个更常见的坑：**当同一个任务内部连续调用多次 `echo`/`printf` 时，这几次写操作之间的时间窗口里，调度器完全可能先去执行别的后台任务的写操作**，于是本该连续出现的几行日志，被别的任务的输出行插了进来，多个任务的日志整体交织错乱，读起来完全对不上号。

这不是"某一行被截断"，而是"多行之间的相对顺序被打乱"，`PIPE_BUF` 帮不上忙。典型表现类似：

```text
task 1 step1: 初始化完成
task 2 step1: 初始化完成
task 1 step2: 处理完成
task 2 step2: 处理完成
```

期望的是 `task 1` 的两行连续出现、`task 2` 的两行连续出现，但实际两个任务各自的输出被拆开、交错打印。

解决思路很直接：**不要让任务边算边直接打印，而是先用命令替换 `$(...)`（或者用字符串拼接 `out+=...`）把该任务想输出的全部内容缓冲进一个变量，等任务的逻辑彻底跑完之后，只用一次 `echo`/`printf` 把整段内容打印出来**。这样这个任务对外只发生一次（或紧邻的一小串）真正的写操作，被别的任务插队的窗口被压缩到最小；如果连"这一次写"都不能和别的任务交织，就要再叠加第6节的 `flock`，把"缓冲"和"互斥"两种手段一起用。

### 示例1：用 `$(...)` 整体捕获函数的多行输出，原函数无需改造

```bash
#!/usr/bin/env bash
set -uo pipefail

# worker 内部有多次 echo，如果直接 `worker "$id" &` 扔到后台，
# 同一个 worker 的这几行 echo 之间可能被别的 worker 的 echo 插进来。
worker() {
	local id="$1"
	echo "task $id step1: 初始化完成"
	sleep 0.3
	echo "task $id step2: 处理完成"
	sleep 0.2
	echo "task $id step3: 校验通过"
}

run_buffered() {
	local id="$1"
	local output
	output="$(worker "$id")"   # 用 $(...) 把多次echo的输出整体捕获进变量
	printf '%s\n' "$output"    # 只做一次真正的写操作
}

for id in 1 2 3 4; do
	run_buffered "$id" &
done
wait
```

要点：`output="$(worker "$id")"` 会启动一个子shell执行 `worker`，把它**全部**标准输出（3次 `echo` 产生的3行）收集进 `output` 变量后再返回给调用者，`worker` 本身完全不用改造；随后 `printf '%s\n' "$output"` 才是这个后台任务真正对外的（几乎）唯一一次写操作。要注意 `$(...)` 会吃掉命令输出末尾的换行符，用 `printf '%s\n'` 补回即可保证格式和直接 `echo` 时一致。

### 示例2：缓冲 + 延迟到 `wait` 之后统一打印，彻底杜绝交织

```bash
#!/usr/bin/env bash
set -uo pipefail

ids=(1 2 3 4 5 6)
max_parallel="${MAX_PARALLEL:-3}"
result_dir="$(mktemp -d)"
trap 'rm -rf "$result_dir"' EXIT

worker() {
	local id="$1"
	local output
	output="$(
		echo "task $id: 开始检查"
		sleep $(( (RANDOM % 3) + 1 ))
		echo "task $id: 检查完成，结果=ok"
	)"
	printf '%s\n' "$output" > "$result_dir/$id.out"
}

for id in "${ids[@]}"; do
	worker "$id" &
	while (( $(jobs -rp | wc -l) >= max_parallel )); do
		wait -n
	done
done
wait

echo "==== 按任务顺序统一输出（后台阶段完全不打印，避免任何交织） ===="
for id in "${ids[@]}"; do
	cat "$result_dir/$id.out"
done
```

要点：后台阶段（`worker "$id" &` 到 `wait` 之间）**完全不往终端打印任何内容**——`$(...)` 把 worker 内部的多行 `echo` 整体捕获后，只写入各自独立的结果文件，真正的"打印"被推迟到全部任务结束、回到单线程的主shell之后才发生；此时只剩主shell一个执行流在做输出，天然不可能再出现交织，这是比示例1"任务内单次打印"更彻底的解法，代价是必须等全部任务跑完才能看到任何输出，不适合需要实时进度反馈的场景。

### 示例3：缓冲内容 + `flock` 双重保证，用于必须实时写共享日志的场景

```bash
#!/usr/bin/env bash
set -uo pipefail

log_file="./combined.log"
lock_file="./combined.log.lock"
: > "$log_file"

build_report() {
	local id="$1"
	local report=""
	report+="[worker-$id] 开始处理"$'\n'
	sleep 0.2
	report+="[worker-$id] 阶段1完成"$'\n'
	sleep 0.2
	report+="[worker-$id] 阶段2完成"$'\n'
	report+="[worker-$id] 全部完成"
	printf '%s\n' "$report"
}

worker() {
	local id="$1"
	local report
	report="$(build_report "$id")"   # 先在锁外把多行内容整体缓冲好
	(
		flock -x 200
		printf '%s\n' "$report" >> "$log_file"   # 持锁期间只做一次写
	) 200>"$lock_file"
}

for id in 1 2 3 4 5; do
	worker "$id" &
done
wait

echo "==== 汇总日志（各worker的多行报告保持连续、不交织） ===="
cat "$log_file"
rm -f "$lock_file"
```

要点：这里把"缓冲"和"互斥"两阶段分开——耗时的业务逻辑（`build_report` 里的 `sleep`/拼接过程、多次 `echo`）都在**锁外**完成，只有最后这一次 `printf ... >> "$log_file"` 在锁内执行，把持锁时间压到最短；因为是先用 `$(...)` 把多行整体捕获、再一次性写入，同一个 worker 的4行报告在日志文件里永远保持连续，不会被其他 worker 的报告行插进来打断。这是"缓冲保证任务内部连续"与"`flock` 保证任务之间互斥"两种手段叠加后能给出的最强保证，适合需要一边跑一边追加共享日志、又不能等到全部任务结束才输出的场景。

---

## 8. 方法对比表

| 方法 | 典型场景 | 并发粒度控制 | 错误处理便利性 | 额外依赖 | 典型坑点 |
| --- | --- | --- | --- | --- | --- |
| `&` + `wait` | 任务少、无需严格限流 | 无内置限流，需自己数PID | 好，逐PID `wait` 拿退出码 | 无（bash内置） | 任务量大时一次性起太多进程，打满资源 |
| `jobs` + `wait -n` | 中等任务量，需要限并发 | 好，`jobs -rp` 轮询限流 | 中，需自己收集失败列表 | 无（bash内置） | 轮询本质是忙等；`jobs`只统计当前shell的任务，子shell/函数内部调用要注意作用域 |
| `xargs -P` | 对列表批量跑同一命令 | 好，`-P N` 直接指定 | 差，整体退出码不区分具体失败项 | 无（GNU/BSD自带） | `export -f` 才能在`bash -c`里调用当前shell函数；文件名含特殊字符要用`-0`/`print0` |
| GNU `parallel` | 复杂参数组合、需要任务日志/重试 | 好，`-j N` + 丰富调度策略 | 好，`--joblog`记录每个任务退出码，支持`--retry-failed` | 需要额外安装 `parallel` | 不同系统默认不一定预装；语法与`xargs`不完全兼容，需要单独学习 |
| `mkfifo` 信号量池 | 高频/大量任务，追求低开销限流 | 很好，阻塞式获取令牌，无忙等 | 中，需要配合`trap`确保令牌归还 | 无（bash内置） | 实现复杂，忘记`trap`兜底容易"令牌泄漏"导致池子越用越小 |
| `flock` | 保护共享资源（文件/日志/计数器/单实例） | 不涉及并发数控制，只做互斥 | 好，锁范围明确 | 需要 `flock`（util-linux，多数Linux自带） | 只解决互斥，不解决限流；`flock -n`不阻塞需要显式判断返回值 |

---

## 9. 最佳实践清单

- **先选限流方式，再定并发数**：任务量小、结构简单用 `&` + `wait`；需要限并发优先用 `jobs -rp` 轮询（实现简单）或 `mkfifo` 信号量（开销更低，任务量很大时更合适）；已装 GNU `parallel` 时优先用它，省去自己写限流/日志/重试逻辑。
- **并发数不要凭感觉写死**：用环境变量（如 `MAX_PARALLEL="${MAX_PARALLEL:-4}"`）暴露出来，方便在不同机器上按CPU核数、IO能力调整，也便于调试时临时改成1（退化为串行）来复现问题。
- **每个后台任务的退出码都要收集**：不要只依赖最后一次裸 `wait` 的返回值——它只反映"最后一个被wait的任务"或整体情况，定位不到具体是哪个任务失败。用 PID 数组逐个 `wait "$pid"`，或者让任务内部把失败项写入独立文件/自带失败标记。
- **并行任务的输出要隔离**：多个后台进程绝不要直接共享写同一个文件做业务输出，除非确实用 `flock` 加了锁；否则统一"各写各的文件，主流程结束后再汇总"。
- **任务内部多次print要先缓冲再统一输出**：同一个后台任务连续多次 `echo`/`printf`，即使单次写是原子的，多次写之间也可能被别的任务的输出插队导致行序错乱；用 `$(...)` 把该任务的全部输出整体捕获进变量，最后只做一次打印（详见第7节），能大幅降低多任务日志交织的概率。
- **`set -euo pipefail` 在后台任务里要小心**：`set -e` 只对当前shell生效，后台子进程（`cmd &`）里的错误不会让主脚本退出，必须靠 `wait` 检查返回值；管道中的命令建议用 `set -o pipefail` 避免"前面的命令失败但因为是管道最后一段命令成功而被忽略"。
- **用 `trap ... EXIT` 清理临时资源**：临时目录、命名管道、锁文件、打开的文件描述符，都应该在脚本正常/异常退出时统一清理，避免残留文件越积越多或者残留的具名管道占用资源。
- **信号量/锁要考虑异常路径**：`mkfifo` 信号量池必须保证任务失败、提前 `return`、甚至被 `kill` 时也能归还令牌（`trap ... RETURN` 或在同一子shell里`trap`兜底），否则会造成"令牌泄漏"最终死锁；同理 `flock` 建议用子shell+fd重定向的写法让锁跟随子shell自动释放。
- **区分"限流"和"互斥"两类问题**：`jobs`/`xargs -P`/`parallel`/`mkfifo` 信号量解决的是"同时最多跑几个任务"，`flock` 解决的是"多个任务同时碰同一个资源怎么办"；复杂脚本往往两者都需要——先用信号量/`-P`控制并发数，再用 `flock` 保护共享的日志/计数器/状态文件。
- **善用现成日志**：GNU `parallel` 的 `--joblog` 能直接拿到每个任务的耗时和退出码，比自己手写PID+退出码收集逻辑更省事，任务量大、需要失败重试时优先考虑引入它而不是自己造轮子。
