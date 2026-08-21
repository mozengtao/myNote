#!/usr/bin/env python3
"""PUSH/PULL 示例3：日志收集端（多对一）"""
from collections import OrderedDict
import zmq

NUM_PRODUCERS = 3
LOGS_PER_PRODUCER = 10
TOTAL_LOGS = NUM_PRODUCERS * LOGS_PER_PRODUCER


def main():
    ctx = zmq.Context()
    pull = ctx.socket(zmq.PULL)
    pull.bind("tcp://*:5566")
    print(f"[Collector] Listening on tcp://*:5566, waiting for {TOTAL_LOGS} logs total")

    counts = OrderedDict()

    received = 0
    for received in range(1, TOTAL_LOGS + 1):
        msg = pull.recv_string()
        print(f"[Collector] {msg}")

        source = msg.split(":")[0]
        counts[source] = counts.get(source, 0) + 1

    print(f"[Collector] All {received} logs received.")
    summary = ", ".join(f"{k}={v}" for k, v in counts.items())
    print(f"[Collector] Summary: {summary}")

    pull.close()
    ctx.term()


if __name__ == "__main__":
    main()
