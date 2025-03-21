[man 1 nohup](https://www.mankier.com/1/nohup)

## nohup
> run a command immune to hangups, with output to a non-tty
```bash
## 工作流程
# 内部实现伪代码
function nohup {
    trap '' SIGHUP    # 忽略挂断信号
    redirect_output   # 将 stdout/stderr 重定向到 nohup.out
    exec "$@"         # 执行目标命令
}
## 输出处理：若终端可写，输出到 nohup.out；若不可写（如 stdout 关闭），静默丢弃
## ​信号传播：子进程继承信号处理设置

nohup ./mcp $sfile 2>&1 | logger -i -p local1.info -t mcp &
nohup find / -print >filenames &
nohup -p 161792     # 使运行的进程忽略所有挂起信号

```

## disown
> bash built-in command
```bash
## 工作流程
# 假设已启动进程
$ sleep 3600 &
[1] 12345

# disown 操作伪代码
function disown {
    remove_job_from_table $1  # 从作业表中删除
    if [[ -n "$hoption" ]]; then
        mark_job_no_sighup $1  # 标记不发送 SIGHUP
    fi
}

## 作业表管理：Shell 维护的作业列表 (jobs -l)
## 选项差异
disown -h：仅标记不发送 SIGHUP，作业仍可见
disown -a：移除所有作业
```