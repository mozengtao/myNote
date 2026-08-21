#!/usr/bin/env python3
"""PUB/SUB 示例3：处理很慢的订阅端（小 RCVHWM）"""
import time
import zmq

RCVHWM = 20
TOTAL_EXPECTED = 1000


def main():
    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.setsockopt(zmq.RCVHWM, RCVHWM)
    sub.connect("tcp://127.0.0.1:5561")
    sub.setsockopt_string(zmq.SUBSCRIBE, "")

    print(f"[Subscriber] Subscribed, RCVHWM={RCVHWM}, simulating slow processing (5ms/msg)")

    expected = 0
    received = 0

    # 用最后一个序号(TOTAL_EXPECTED-1)作为"流结束"信号，而不是数received数量，
    # 因为如果消息被丢弃，received永远达不到TOTAL_EXPECTED。
    while True:
        msg = sub.recv_string()
        received += 1

        seq = int(msg.split(" ")[1])
        print(f"[Subscriber] recv {msg}")

        if seq != expected:
            print(f"[Subscriber] *** Gap detected: expected {expected}, got {seq} "
                  f"(lost {seq - expected} messages) ***")
        expected = seq + 1

        time.sleep(0.005)  # 模拟慢处理

        if seq == TOTAL_EXPECTED - 1:
            break

    print(f"[Subscriber] Done. Sent={TOTAL_EXPECTED} (by publisher), "
          f"Received={received}, Lost={TOTAL_EXPECTED - received}")

    sub.close()
    ctx.term()


if __name__ == "__main__":
    main()
