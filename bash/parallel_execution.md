# Shell 下并行执行（Parallel Execution）最佳实践

对于 Shell 脚本而言，并行执行的核心目标是：

1. 提高执行效率
2. 充分利用 CPU / IO 资源
3. 避免脚本阻塞
4. 可控制并发度
5. 能正确收集执行结果
6. 出错时能够快速失败

---

# 一、心智模型

先建立一个统一认知：

```text
Shell 默认执行模型

cmd1
cmd2
cmd3

执行顺序：

cmd1 ---> cmd2 ---> cmd3
```

属于：

```text
串行执行 (Sequential Execution)
```

---

而并行执行：

```text
cmd1 &
cmd2 &
cmd3 &

wait
```

变成：

```text
               ┌── cmd1
Shell Parent ──┼── cmd2
               └── cmd3

wait
```

即：

```text
Parent Shell
      │
      ├── Child #1
      ├── Child #2
      └── Child #3

Parent 等待所有 Child 结束
```

---

# 二、最简单的并行执行

## 方法1：&

```bash
sleep 5 &
sleep 5 &
sleep 5 &

wait
```

执行时间：

```text
串行:
5+5+5=15s

并行:
≈5s
```

---

查看后台任务：

```bash
jobs
```

例如：

```text
[1] Running sleep 5 &
[2] Running sleep 5 &
[3] Running sleep 5 &
```

---

# 三、wait 的作用

很多人误解：

```bash
cmd &
```

启动完就结束了。

实际上：

```bash
Parent Shell
    │
    └── Child
```

Parent 不等待 Child。

---

例如：

```bash
sleep 5 &
echo done
```

输出：

```text
done
```

sleep 仍然在运行。

---

使用：

```bash
wait
```

变成：

```bash
sleep 5 &
wait

echo done
```

输出：

```text
(5秒后)

done
```

---

# 四、获取每个任务状态

## 保存 PID

```bash
sleep 10 &
pid1=$!

sleep 20 &
pid2=$!

wait "$pid1"
echo "task1=$?"

wait "$pid2"
echo "task2=$?"
```

---

其中：

```bash
$!
```

表示：

```text
最近启动后台任务的 PID
```

---

心智模型：

```text
cmd &
  │
  └── PID

保存 PID

pid=$!

wait pid
```

---

# 五、并行执行多个命令

## 典型写法

```bash
for host in host1 host2 host3
do
    ssh "$host" uptime &
done

wait
```

---

执行模型：

```text
host1 ── ssh
host2 ── ssh
host3 ── ssh

同时执行
```

---

# 六、收集结果

错误写法：

```bash
for host in "${hosts[@]}"
do
    ssh "$host" uptime &
done

wait
```

无法知道哪个失败。

---

正确写法：

```bash
declare -A pids

for host in "${hosts[@]}"
do
    ssh "$host" uptime &
    pids[$!]=$host
done

for pid in "${!pids[@]}"
do
    if wait "$pid"
    then
        echo "${pids[$pid]} success"
    else
        echo "${pids[$pid]} failed"
    fi
done
```

---

执行模型：

```text
PID 1234 -> host1
PID 1235 -> host2
PID 1236 -> host3

wait PID

获得返回值
```

---

# 七、限制并发数（非常重要）

实际工程最常见需求：

```text
1000台设备
```

不能：

```bash
for host in hosts
do
    ssh host &
done
```

否则：

```text
1000 个 ssh
1000 个进程
```

系统可能崩溃。

---

## 方法1：wait -n

Bash 5+

```bash
max_jobs=10

for host in "${hosts[@]}"
do
    ssh "$host" uptime &

    while (( $(jobs -rp | wc -l) >= max_jobs ))
    do
        wait -n
    done
done

wait
```

---

心智模型：

```text
启动10个

满了
 ↓

wait -n

有一个结束

再补一个
```

即：

```text
Worker Pool
```

模型。

---

ASCII：

```text
max=10

slot1  ssh
slot2  ssh
slot3  ssh
...
slot10 ssh

有一个结束

↓

补一个新任务
```

---

# 八、推荐方案：xargs -P

很多场景不需要自己管理 PID。

---

例如：

```bash
cat hosts.txt |
xargs -n1 -P10 -I{} \
ssh {} uptime
```

---

含义：

```text
-n1
每次取1个参数

-P10
10个并发

-I{}
参数占位符
```

---

执行模型：

```text
hosts.txt

host1
host2
host3
...
```

变成：

```text
Worker1 -> host1
Worker2 -> host2
...
Worker10 -> host10
```

完成一个：

```text
Worker5 空闲
```

立即：

```text
Worker5 -> host11
```

---

这是 Linux 下非常经典的并发模型。

---

# 九、推荐方案：GNU Parallel

真正的大规模并发首选。

安装：

```bash
dnf install parallel
```

或者

```bash
apt install parallel
```

---

例如：

```bash
parallel -j 20 ssh {} uptime ::: host1 host2 host3
```

---

心智模型：

```text
GNU Parallel

         Job Queue
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼

 Worker1   Worker2   Worker3
```

自动：

- 调度
- 限流
- 重试
- 收集输出

---

例如：

```bash
parallel -j 50 curl {} ::: "${urls[@]}"
```

---

# 十、处理输出混乱问题

并行最大的问题：

```bash
{
    echo start
    sleep 1
    echo end
} &
```

多个任务同时输出：

```text
start
start
end
start
end
end
```

完全乱序。

---

## 方案1：输出到独立文件

```bash
for host in "${hosts[@]}"
do
    {
        ssh "$host" uptime
    } >"logs/$host.log" 2>&1 &
done

wait
```

---

结果：

```text
logs/

host1.log
host2.log
host3.log
```

---

## 方案2：GNU Parallel

```bash
parallel --keep-order
```

---

# 十一、Fail Fast 模式

某任务失败立即停止全部任务。

---

例如：

```bash
pids=()

for task in "${tasks[@]}"
do
    run_task "$task" &
    pids+=($!)
done

for pid in "${pids[@]}"
do
    if ! wait "$pid"
    then
        kill "${pids[@]}" 2>/dev/null
        exit 1
    fi
done
```

---

模型：

```text
Task1
Task2
Task3

Task2 Failed

↓

Kill Task1
Kill Task3

↓

Exit
```

---

# 十二、生产环境最佳实践

## 小规模（<10任务）

直接：

```bash
cmd &
cmd &
wait
```

---

## 中规模（10~100任务）

推荐：

```bash
wait -n
```

实现：

```text
Worker Pool
```

---

## 大规模（100~10000任务）

推荐：

```bash
xargs -P
```

或者：

```bash
GNU Parallel
```

---

## SSH批量运维

推荐：

```bash
parallel
```

或

```bash
pdsh
```

---

## 批量文件处理

推荐：

```bash
find ... -print0 |
xargs -0 -P N
```

例如：

```bash
find logs -name '*.gz' -print0 |
xargs -0 -P 8 gunzip
```

---

# 十三、程序员应该掌握的四种并发模式

```text
1. Fire and Forget

cmd &

用途：
后台执行
```

```text
2. Fork + Join

cmd1 &
cmd2 &
cmd3 &

wait

用途：
等待全部完成
```

```text
3. Worker Pool

最大并发=N

用途：
生产环境最常见
```

```text
4. Job Queue

任务队列
      ↓
多个 Worker
      ↓
结果收集

用途：
大规模批处理
```

---

# 最终推荐

日常 Shell 工程开发优先级：

```text
★★★★★ GNU Parallel
    ↓
★★★★☆ xargs -P
    ↓
★★★★☆ wait -n (Bash5+)
    ↓
★★★☆☆ cmd & + wait
```

可以把它们理解成同一种模型的不同实现：

```text
                    Job Queue
                         │
                         ▼
                 +---------------+
                 |  Worker Pool  |
                 +---------------+
                    │   │   │
                    ▼   ▼   ▼
                  Task Task Task
                    │   │   │
                    └───┴───┘
                        │
                     Result
```

一旦建立了这个心智模型，Shell 并行执行、批量 SSH、批量文件处理、批量 API 调用、批量编译任务，本质上都只是：
"任务队列 + Worker Pool + 结果收集" 的不同表现形式。