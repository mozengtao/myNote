#!/usr/bin/env python3
"""PUSH/PULL 示例3：日志生产者（模拟一个微服务节点）
用法：python3 log_producer.py <service_name>
"""
import random
import sys
import time
import zmq

LOGS_PER_PRODUCER = 10


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "unknown"

    ctx = zmq.Context()
    push = ctx.socket(zmq.PUSH)
    push.connect("tcp://127.0.0.1:5566")

    for i in range(LOGS_PER_PRODUCER):
        push.send_string(f"{name}: log line {i}")
        time.sleep(random.uniform(0, 0.2))

    print(f"[{name}] Sent {LOGS_PER_PRODUCER} logs")

    push.close()
    ctx.term()


if __name__ == "__main__":
    main()
