#!/usr/bin/env python3
"""PUSH/PULL 示例1：Worker（可启动多个实例做负载均衡）
用法：python3 worker.py <id>
"""
import sys
import time
import zmq


def main():
    worker_id = sys.argv[1] if len(sys.argv) > 1 else "?"

    ctx = zmq.Context()
    pull = ctx.socket(zmq.PULL)
    pull.connect("tcp://127.0.0.1:5562")

    try:
        while True:
            msg = pull.recv_string()
            task_id, cost = msg.split(" ")
            cost = int(cost)
            print(f"[Worker {worker_id}] processing task {task_id} (cost={cost} units)")
            time.sleep(cost * 0.1)
    except KeyboardInterrupt:
        pass
    finally:
        pull.close()
        ctx.term()


if __name__ == "__main__":
    main()
