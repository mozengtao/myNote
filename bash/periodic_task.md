# 周期任务（Periodic Task）的心智模式（黄金法则）

> **一句话总结：**
>
> **不要控制 `sleep`，而要控制"下一次应该开始执行的时间（Deadline）"。**
>
> 从调度器（Scheduler）的角度思考，而不是从循环（Loop）的角度思考。

---

# 1. 为什么这是一个经典问题？

很多人在刚开始写 Bash/Python 时都会写出下面这种代码：

```bash
while true
do
    run_cmd
    sleep 100
done
```

乍一看：

> 每100秒执行一次。

实际上不是。

真正执行周期为

```
周期 = run_cmd耗时 + sleep时间
```

例如：

```
run_cmd = 20s

sleep =100s

实际周期 =120s
```

如果：

```
run_cmd =80s
```

那么：

```
实际周期 =180s
```

因此：

**sleep 控制的是"两次任务结束之间的等待时间"，不是"两次任务开始之间的间隔"。**

---

# 2. 周期任务真正需要控制的对象

很多人认为：

```
run
 ↓
sleep
 ↓
run
```

实际上 Scheduler 想控制的是：

```
Start Time
```

而不是

```
Finish Time
```

因此：

```
下一次开始时间
      ↑
      │
run ──┘
```

真正的调度目标应该始终是：

> **任务应该什么时候开始，而不是什么时候结束。**

---

# 3. 周期任务的三种经典模型

```
                Periodic Task

                     │
     ┌───────────────┼───────────────┐
     │               │               │
     ▼               ▼               ▼

 Delay After     Fixed Rate      Fixed Time
   Finish         (Ticker)      (Cron/Systemd)

```

它们适用于不同场景。

---

# 模型一：Delay After Finish（完成后等待）

最简单，也是 Bash 最容易写出的模型。

```
run
 │
 ▼
sleep(interval)
 │
 ▼
run
```

代码：

```bash
while true
do
    run_cmd
    sleep 100
done
```

时间轴：

```
run(20s)
│
├────────20────────┐
                   │
             sleep100
                   │
                   ▼

下一次开始

周期=120s
```

特点：

✅ 实现简单

❌ 周期越来越长

❌ Drift（漂移）不断累积

适用：

- 日志打印
- 临时脚本
- 对时间要求不高

---

# 模型二：Fixed Rate（固定周期）

这是绝大多数工程里的最佳实践。

思想：

不是：

```
sleep interval
```

而是：

```
sleep until next deadline
```

维护：

```
next_run_time
```

例如：

```
next = now

loop

    sleep until next

    run

    next += interval
```

对应 Bash：

```bash
INTERVAL=100
next=$(date +%s)

while true
do
    now=$(date +%s)

    if (( now < next ))
    then
        sleep $((next-now))
    fi

    run_cmd

    next=$((next+INTERVAL))
done
```

时间轴：

```
0------100------200------300------400

      ^
    run1

      [====20====]

sleep80

200开始

```

即使：

```
run=80秒
```

仍然：

```
100
200
300
400
```

不会漂移。


先执行，再等待:

```bash
INTERVAL=100

next=$(date +%s)

while true
do
    next=$((next + INTERVAL))

    run_cmd

    now=$(date +%s)

    if (( now < next )); then
        sleep $((next - now))
    fi
done
```

再进一步：处理"严重落后"（推荐用于长期运行）
如果 run_cmd 连续很多次耗时远超 INTERVAL，next 会一直落后于当前时间

检测到已经错过多个周期时，直接把 next 推进到未来：
```bash
INTERVAL=100
next=$(date +%s)

while true; do
    run_cmd

    next=$((next + INTERVAL))
    now=$(date +%s)

    while (( next <= now )); do
        next=$((next + INTERVAL))
    done

    sleep $((next - now))
done
```
这是很多调度器（例如事件循环、定时器框架）采用的思路：维护一个不断向前推进的 Deadline，而不是试图补跑所有已经错过的周期

---

# 模型三：Fixed Time（绝对时间）

Cron、systemd timer 都属于这一类。

例如：

```
12:00

12:05

12:10

12:15
```

无论任务什么时候结束，

都按：

```
绝对时间
```

启动。

例如：

```
12:00

run用了2分钟

12:05

继续启动
```

而不是：

```
12:07
```

这也是企业里最常见的任务调度方式。

---

# 4. 三种模型对比

| 模型 | 控制对象 | 是否累计漂移 | 推荐场景 |
|--------|----------|-------------|------------|
| Delay After Finish | Finish Time | 会 | 临时脚本 |
| Fixed Rate | Next Start Time | 不会 | Daemon、采集程序 |
| Fixed Time | Wall Clock | 不会 | Cron、systemd timer |

---

# 5. 示例一：采集 CPU 利用率

错误写法：

```bash
while true
do
    collect_cpu
    sleep 60
done
```

如果：

```
collect_cpu
```

越来越慢：

```
2秒

3秒

5秒

8秒
```

那么：

```
62

63

65

68
```

采样时间越来越偏。

正确写法：

```
每60秒采一次
```

而不是：

```
采完以后等60秒
```

### 完整 Bash 实现（Fixed Rate 模型）

```bash
#!/usr/bin/env bash
#
# collect_cpu.sh —— 按 Fixed Rate 模型采集 CPU 利用率
#
# 心智模型：只维护"下一次应该开始采集的时间 next"，
#           而不是"采集完之后睡多久"。

set -uo pipefail

INTERVAL=60                    # 采样周期（秒）
LOGFILE=/var/log/cpu_usage.log

collect_cpu() {
    local usage
    usage=$(top -bn1 | awk -F'[, ]+' '/Cpu\(s\)/{print 100-$8}')
    echo "$(date '+%F %T')  cpu_usage=${usage}%" >> "$LOGFILE"
}

next=$(date +%s)

while true; do
    now=$(date +%s)

    # 1. 只计算"该等多久"，而不是直接 sleep 固定值
    if (( now < next )); then
        sleep "$(( next - now ))"
    fi

    # 2. 执行业务逻辑；collect_cpu 耗时不影响下一次调度基准
    collect_cpu

    # 3. 调度基准永远基于"上一次的计划时间"，而不是"当前时间"
    next=$(( next + INTERVAL ))

    # 4. 极端保护：如果采集耗时远超一个周期（系统卡顿、被挂起等），
    #    next 可能已经落后当前时间很多个周期。此时不"疯狂补跑"，
    #    而是直接对齐到未来最近的一个 deadline。
    now=$(date +%s)
    if (( next <= now )); then
        next=$(( now + INTERVAL - (now % INTERVAL) ))
    fi
done
```

要点：`next` 只增不减，`sleep` 的时长永远是"计算出来的"，而不是写死的 `60`。

---

# 6. 示例二：网络设备巡检

需求：

```
每30秒检查所有Router
```

错误：

```
run

sleep30
```

如果：

```
run用了15秒
```

真正：

```
45秒一次
```

正确：

```
00

30

60

90
```

Scheduler 始终围绕：

```
下一次检查时间
```

而不是：

```
sleep30
```

### 完整 Bash 实现（Fixed Rate 模型）

多台设备巡检天然存在"单台慢会拖累整体"的问题，因此本轮内部用后台任务并行探测，
但**调度基准仍然只由 `next` 决定**，不受某一台设备探测快慢的影响。

```bash
#!/usr/bin/env bash
#
# router_patrol.sh —— 网络设备巡检（Fixed Rate 模型）
#
# 场景：每30秒巡检一次所有路由器，单台设备探测耗时不应影响整体节拍。

set -uo pipefail

INTERVAL=30
LOGFILE=/var/log/router_patrol.log
ROUTERS=(
10.0.0.1
10.0.0.2
10.0.0.3
)

check_router() {
    local ip=$1
    if ping -c1 -W2 "$ip" &>/dev/null; then
        echo "$(date '+%F %T')  ${ip}  UP" >> "$LOGFILE"
    else
        echo "$(date '+%F %T')  ${ip}  DOWN" >> "$LOGFILE"
    fi
}

patrol_all() {
    local ip
    for ip in "${ROUTERS[@]}"; do
        check_router "$ip" &      # 并行探测，缩短单轮耗时
    done
    wait                          # 等本轮全部探测完成，再进入下一轮判断
}

next=$(date +%s)

while true; do
    now=$(date +%s)

    if (( now < next )); then
        sleep "$(( next - now ))"
    fi

    patrol_all

    # 无论本轮巡检用了15秒还是45秒，下一次开始时间永远是
    # 00 / 30 / 60 / 90 ... 这条固定节拍线，不会被拖慢。
    next=$(( next + INTERVAL ))
done
```

---

# 7. 示例三：日志轮询（Log Polling）

例如：

```
每5秒读取一次日志
```

错误：

```bash
while true
do
    read_log
    sleep 5
done
```

如果：

```
read_log

用了8秒
```

真正：

```
13秒
```

一次。

正确：

```
next +=5
```

即使：

```
某一次慢了
```

下一轮仍然尽量追赶计划时间，而不会无限漂移。

### 完整 Bash 实现（Fixed Rate 模型）

```bash
#!/usr/bin/env bash
#
# tail_log.sh —— 日志轮询（Fixed Rate 模型）
#
# 场景：每5秒检查日志文件是否有新增内容，用文件大小/行号记录读取位置，
#      避免重复读取，也避免因为 read_log 偶尔变慢而导致节拍漂移。

set -uo pipefail

INTERVAL=5
LOGFILE=/var/log/app.log
POSFILE=/tmp/app.log.pos

read_log() {
    local last_pos cur_size
    last_pos=$(cat "$POSFILE" 2>/dev/null || echo 0)
    cur_size=$(stat -c%s "$LOGFILE" 2>/dev/null || echo 0)

    if (( cur_size > last_pos )); then
        tail -c +"$((last_pos + 1))" "$LOGFILE"
    fi

    echo "$cur_size" > "$POSFILE"
}

next=$(date +%s)

while true; do
    now=$(date +%s)

    if (( now < next )); then
        sleep "$(( next - now ))"
    fi

    read_log

    # 关键：next 基于"计划时间"累加，即使某一次 read_log 用了8秒（超过了
    # 5秒的 INTERVAL），下一轮也只是立刻执行（因为 next 已经 <= now），
    # 随后继续按 5、10、15... 的节拍对齐，而不会永远比原计划晚 3 秒。
    next=$(( next + INTERVAL ))
done
```

---

# 8. 示例四：磁盘空间监控（显式演示"追赶"处理）

需求：

```
每5分钟检查一次磁盘使用率，超过阈值告警
```

这个例子比前三个例子更进一步：**显式演示当某一轮任务异常耗时（例如磁盘卡住、NFS 挂起）、
`next` 严重落后于当前时间时应该怎么处理**——既不是无脑 `sleep 300`，也不是"疯狂补跑"错过的每一轮。

```bash
#!/usr/bin/env bash
#
# check_disk.sh —— 磁盘空间监控（Fixed Rate + 显式"追赶"处理）

set -uo pipefail

INTERVAL=300                   # 5 分钟
THRESHOLD=85                   # 磁盘使用率告警阈值(%)
LOGFILE=/var/log/disk_check.log

check_disk() {
    while read -r pct mount; do
        if (( pct >= THRESHOLD )); then
            echo "$(date '+%F %T')  WARN  ${mount} used=${pct}%" >> "$LOGFILE"
        fi
    done < <(df -P | awk 'NR>1 {gsub("%","",$5); print $5, $6}')
}

next=$(date +%s)

while true; do
    now=$(date +%s)

    if (( now < next )); then
        sleep "$(( next - now ))"
    fi

    check_disk

    next=$(( next + INTERVAL ))
    now=$(date +%s)

    # 黄金法则五的具体应用：我要控制的是"开始间隔"，不是"完成间隔"。
    # 如果 check_disk 卡了很久，next 可能已经落后当前时间超过一个周期。
    # 这时不应该为了"补齐历史"把 check_disk 连续跑 N 次，
    # 而是直接对齐到未来最近的一个节拍点，丢弃已经错过的中间轮次。
    if (( next <= now )); then
        missed=$(( (now - next) / INTERVAL + 1 ))
        echo "$(date '+%F %T')  INFO  skipped ${missed} cycle(s), realign schedule" >> "$LOGFILE"
        next=$(( next + missed * INTERVAL ))
    fi
done
```

---

# 9. 示例五：服务健康检查守护进程（调度与业务逻辑解耦 + 优雅退出）

需求：

```
每10秒检查一次服务健康状态，可被 systemd 正常 stop/restart
```

这个例子重点演示**黄金法则四**：调度周期属于 Scheduler，任务耗时/成败属于业务逻辑，
`health_check` 内部逻辑无论快慢、成功失败，都不应该影响 `next` 的计算方式；
同时演示如何让"等待 deadline"这件事**可被信号中断**，而不是死等一个长 `sleep`。

```bash
#!/usr/bin/env bash
#
# healthcheck_daemon.sh —— 服务健康检查（Fixed Rate + 优雅退出）

set -uo pipefail

INTERVAL=10
PIDFILE=/var/run/healthcheck.pid
SERVICE_URL="http://127.0.0.1:8080/health"

running=1
trap 'running=0' SIGTERM SIGINT

echo $$ > "$PIDFILE"

health_check() {
    if ! curl -fsS -o /dev/null --max-time 3 "$SERVICE_URL"; then
        logger -t healthcheck "service unhealthy: ${SERVICE_URL}"
    fi
}

next=$(date +%s)

while (( running )); do
    now=$(date +%s)

    if (( now < next )); then
        # 把长 sleep 拆成 1 秒一片，保证 SIGTERM/SIGINT 能在 1 秒内被响应，
        # 这与"控制 deadline"并不冲突，只是让等待过程本身可中断。
        remaining=$(( next - now ))
        while (( running && remaining > 0 )); do
            slice=$(( remaining < 1 ? remaining : 1 ))
            sleep "$slice"
            remaining=$(( remaining - slice ))
        done
    fi

    (( running )) || break

    health_check

    next=$(( next + INTERVAL ))
done

rm -f "$PIDFILE"
```

---

# 10. 示例六：整点对齐任务（Fixed Time 模型，不依赖 crontab）

需求：

```
每小时整点（00分00秒）生成一次报表
```

前面五个例子都是 **Fixed Rate**（相对节拍，如"每30秒"）。这个例子演示第三种模型
**Fixed Time**（绝对墙钟时间，如 cron 的 `0 * * * *`）——即使报表生成跨越了一个整点，
下一次触发时间依然锚定在真实的墙上时钟刻度上，而不是"结束时间 + 间隔"。

```bash
#!/usr/bin/env bash
#
# hourly_report.sh —— 整点对齐任务（Fixed Time 模型）

set -uo pipefail

generate_report() {
    echo "$(date '+%F %T')  generating hourly report..."
    # ... 实际报表生成逻辑，耗时可能是几秒，也可能是几分钟 ...
}

# 计算"下一个整点"的绝对时间戳（基于本地墙钟，不受时区偏移影响）
#
# 注意：不要写成 date -d "... +1 hour"，GNU date 在字符串结尾会把
# " +1" 误判成时区偏移（例如 UTC+1），导致结果整体错位。
# 正确做法：先算出本小时整点的 epoch，再用整数加法 +3600。
next_top_of_hour() {
    local this_hour_epoch
    this_hour_epoch=$(date -d "$(date '+%Y-%m-%d %H:00:00')" +%s)
    echo $(( this_hour_epoch + 3600 ))
}

next=$(next_top_of_hour)

while true; do
    now=$(date +%s)

    if (( now < next )); then
        sleep "$(( next - now ))"
    fi

    generate_report

    # Fixed Time 的关键：next 直接重新计算"下一个整点"，
    # 而不是 next + INTERVAL。即使某一轮跑到了 12:03 才结束，
    # 下一次触发依然是 13:00，而不是 "结束时间 + 1小时"。
    next=$(next_top_of_hour)
done
```

与 Cron/systemd timer 的关系：这个脚本在纯 Bash 里手工实现了 Cron 的核心思想——
调度目标永远是**绝对时间点**，而不是相对上一次结束时间的偏移。生产环境中通常直接
用 `systemd timer` 或 `crontab` 替代这类脚本，但理解其原理有助于排查"任务为什么没有
按整点触发"之类的问题。

---

# 11. 更高层次的 Scheduler 心智模型

真正成熟的软件（Go、Java、Linux Timer、Kubernetes Controller）几乎都遵循同一个思想：

```
             Scheduler

                  │
                  ▼

         Calculate Deadline

                  │

                  ▼

        Sleep Until Deadline

                  │

                  ▼

            Execute Task

                  │

                  ▼

      Calculate Next Deadline

                  │

                  └──────────────┐
                                 │
                                 ▼
                              Next Loop
```

注意：

整个 Scheduler 从来没有关心：

```
sleep多久
```

它真正关心的是：

```
下一次什么时候应该开始。
```

---

# 12. 黄金法则（Golden Rules）

## 黄金法则一

> **不要控制 Sleep，控制 Deadline。**

不是：

```
sleep(interval)
```

而是：

```
sleep_until(next_deadline)
```

---

## 黄金法则二

> **调度的是"开始时间（Start Time）"，不是"结束时间（Finish Time）"。**

结束时间不可预测。

开始时间才是 Scheduler 能控制的。

---

## 黄金法则三

> **始终维护一个"下一次执行时间（next_run_time）"，不要维护"睡眠多久（sleep interval）"。**

这是：

- Linux Timer
- Go Ticker
- Java ScheduledExecutor
- Python sched
- systemd timer
- Cron

共同遵循的核心思想。

---

## 黄金法则四

> **任务耗时属于业务逻辑；调度周期属于 Scheduler。两者应解耦。**

不要让：

```
run_cmd()
```

决定：

```
下一次什么时候开始
```

Scheduler 才应该负责时间控制。

---

## 黄金法则五

> **任何周期任务，都先问自己："我要控制的是完成间隔，还是开始间隔？"**

如果答案是：

> **"每隔 N 秒开始一次任务"**

那么几乎总应该选择 **Fixed Rate（Deadline 驱动）**，而不是简单的：

```bash
sleep N
```

---

# 13. 最终心智模型（建议牢记）

```
                 Scheduler

                     │
                     ▼

          "下一次什么时候开始？"

                     │
                     ▼

          计算 Next Deadline

                     │
                     ▼

        等待直到 Deadline

                     │
                     ▼

             执行业务逻辑

                     │
                     ▼

      Next Deadline += Interval

                     │
                     └──────────────► 下一轮
```

> **从工程实践的角度看，优秀的周期任务设计不是围绕 `sleep` 展开，而是围绕 `Deadline（目标开始时间）` 展开。**
>
> 一旦建立这一心智模型，你会发现 Bash、Python、Go、Java、Linux Timer、Cron、systemd timer 等各种定时机制，本质上都遵循同一种调度思想，只是在时间精度、并发能力和异常处理方面有所不同。