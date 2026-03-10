## 1. 最简无依赖: & + wait
```bash
# 主函数：& + wait 实现并行
run_parallel_functions() {
    # 两个函数后加&，放入后台同时运行
    start_serviceA &
    start_serviceB &

    # 等待所有后台子进程执行完成
    wait

    # 所有并行任务完成后，输出最终提示
    echo -e "\n[$(date +%H:%M:%S)] All services started successfully!"
}

# 调用测试
run_parallel_functions
```
## 2. 进程替换 <()
> bash 原生扩展特性<()，将函数启动为独立子进程并行执行，核心优势是可对每个函数的输出做独立重定向
```bash
# 主函数：进程替换作为cat命令参数（规范用法，无Permission denied）
run_parallel_functions() {
    # 遵循进程替换「必须作为命令参数」的原生规则，<()作为cat命令的参数，启动独立子进程并行执行
    # cat <(命令) 是进程替换的经典规范用法：cat 命令读取进程替换的临时文件描述符输出，间接让<()后的命令启动为独立子进程
    # 独立输出重定向，日志文件单独生成
    cat <(start_serviceA > serviceA_start.log 2>&1) &
    cat <(start_serviceB > serviceB_start.log 2>&1) &

    # 等待所有后台子进程完成
    wait

    echo -e "\n[$(date +%H:%M:%S)] All services started successfully! (logs in *.log)"
}

# 调用测试
run_parallel_functions
```

## 3. 系统自带工具: xargs + -P（精准限制并行数）
```bash
# 关键步骤：导出函数为环境变量（让xargs子shell可识别，必须放在函数定义后、主函数前）
export -f start_serviceA start_serviceB

# 主函数：xargs + -P 实现并行（限制并行数为2）
run_parallel_functions() {
    # echo传递待并行的函数名 → xargs处理
    # -I {}：将{}作为函数名占位符；-P 2：指定同时运行2个进程（与函数数一致）
    echo -e "start_serviceA\nstart_serviceB" | xargs -I {} -P 2 bash -c "{}"

    # 无需额外wait：xargs -P 会自动阻塞，直到所有并行进程完成后再退出
    echo -e "\n[$(date +%H:%M:%S)] All services started successfully!"
}

# 调用测试
run_parallel_functions
```
## 4. 专业并行工具: GNU Parallel（功能最强，灵活扩展）
```bash
# 关键步骤：导出函数为环境变量（让GNU Parallel可识别）
export -f start_serviceA start_serviceB

# 主函数：GNU Parallel 实现并行（基础版）
run_parallel_functions() {
    # 核心语法：parallel ::: 待并行的函数/命令列表
    parallel ::: start_serviceA start_serviceB

    # 无需额外wait：parallel自动等待所有并行进程完成
    echo -e "\n[$(date +%H:%M:%S)] All services started successfully!"
}

# 调用测试
run_parallel_functions
```