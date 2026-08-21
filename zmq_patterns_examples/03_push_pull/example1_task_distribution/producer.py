#!/usr/bin/env python3
"""PUSH/PULL 示例1：任务分发端"""
import random
import time
import zmq

NUM_TASKS = 20


def main():
    ctx = zmq.Context()
    push = ctx.socket(zmq.PUSH)
    push.bind("tcp://*:5562")
    print("[Producer] Listening on tcp://*:5562")

    time.sleep(1)  # 给 Worker 一点时间连接

    print(f"[Producer] Dispatching {NUM_TASKS} tasks...")
    for i in range(NUM_TASKS):
        cost = random.randint(1, 5)
        push.send_string(f"{i} {cost}")
        time.sleep(0.05)
    print("[Producer] Done dispatching.")

    time.sleep(1)
    push.close()
    ctx.term()


if __name__ == "__main__":
    main()
