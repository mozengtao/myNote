import asyncio
import time

# 用于获取当前时间戳的辅助函数，方便观察时间差
def get_time():
    return time.strftime("%H:%M:%S", time.localtime())

# 模拟一个耗时的 I/O 操作（如网络请求、数据库查询）
async def io_task(task_id, duration):
    # 1. 任务开始
    print(f"[{get_time()}] 任务-{task_id} >>> 开始执行 (预计耗时 {duration}s)")
    
    # 2. 模拟 I/O 等待
    # 关键点：await 会让出控制权，任务进入“挂起”状态，事件循环去运行其他任务
    print(f"[{get_time()}] 任务-{task_id} ... 进入 await 等待 (让出 CPU)")
    await asyncio.sleep(duration)
    
    # 3. 任务恢复
    # 关键点：等待结束后，事件循环重新调度该任务，从 await 下一行继续执行
    print(f"[{get_time()}] 任务-{task_id} <<< 等待结束，恢复执行")
    
    # 4. 任务完成
    print(f"[{get_time()}] 任务-{task_id} === 完成")
    return f"Result-{task_id}"

async def main():
    print(f"\n[{get_time()}] === 主程序启动 ===\n")
    start_time = time.time()
    
    # 创建三个任务，耗时分别为 2s, 1s, 3s
    # 注意：此时任务只是被创建（调度），尚未开始运行，直到遇到 await 或 gather
    task1 = asyncio.create_task(io_task("A", 2))
    task2 = asyncio.create_task(io_task("B", 1))
    task3 = asyncio.create_task(io_task("C", 3))
    
    # 等待所有任务完成
    # gather 会并发运行这些任务，而不是顺序运行
    results = await asyncio.gather(task1, task2, task3)
    
    end_time = time.time()
    print(f"\n[{get_time()}] === 主程序结束 ===")
    print(f"所有任务返回值：{results}")
    print(f"总耗时：{end_time - start_time:.2f} 秒")
    print(f"(如果是同步串行执行，总耗时应为 2+1+3=6 秒)")

if __name__ == "__main__":
    # asyncio.run() 创建并运行事件循环，入口点
    asyncio.run(main())