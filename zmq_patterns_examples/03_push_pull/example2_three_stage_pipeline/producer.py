#!/usr/bin/env python3
"""PUSH/PULL 示例2：三级流水线 —— Producer (ventilator)"""
import random
import time
import zmq

NUM_TASKS = 100


def main():
    ctx = zmq.Context()

    sender = ctx.socket(zmq.PUSH)
    sender.bind("tcp://*:5563")

    sink_ctrl = ctx.socket(zmq.PUSH)
    sink_ctrl.connect("tcp://127.0.0.1:5565")

    print("[Producer] Listening on tcp://*:5563 (tasks)")
    time.sleep(1)  # 给 Worker 和 Sink 连接的时间

    sink_ctrl.send_string(str(NUM_TASKS))
    print(f"[Producer] Told Sink to expect {NUM_TASKS} results")

    total_cost = 0
    for _ in range(NUM_TASKS):
        cost = random.randint(1, 20)  # ms
        total_cost += cost
        sender.send_string(str(cost))

    print(f"[Producer] Dispatched {NUM_TASKS} tasks, total workload={total_cost}ms")

    time.sleep(1)
    sender.close()
    sink_ctrl.close()
    ctx.term()


if __name__ == "__main__":
    main()
