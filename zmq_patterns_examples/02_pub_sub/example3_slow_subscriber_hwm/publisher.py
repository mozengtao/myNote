#!/usr/bin/env python3
"""PUB/SUB 示例3：高速发布端（小 SNDHWM）"""
import time
import zmq

TOTAL_MESSAGES = 1000
SNDHWM = 20


def main():
    ctx = zmq.Context()
    pub = ctx.socket(zmq.PUB)
    pub.setsockopt(zmq.SNDHWM, SNDHWM)
    pub.bind("tcp://*:5561")

    print(f"[Publisher] Listening on tcp://*:5561, SNDHWM={SNDHWM}")
    time.sleep(1)  # 给 Subscriber 建立连接的时间

    for i in range(TOTAL_MESSAGES):
        pub.send_string(f"SEQ {i}")

    print(f"[Publisher] Done sending {TOTAL_MESSAGES} messages")

    time.sleep(1)  # 留点时间让最后一批消息真正发出
    pub.close()
    ctx.term()


if __name__ == "__main__":
    main()
