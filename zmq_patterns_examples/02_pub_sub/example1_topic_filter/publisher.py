#!/usr/bin/env python3
"""PUB/SUB 示例1：主题过滤广播 —— 发布端"""
import time
import zmq

TOPICS = ["NEWS", "SPORT", "AI"]


def main():
    ctx = zmq.Context()
    pub = ctx.socket(zmq.PUB)
    pub.bind("tcp://*:5558")
    print("[Publisher] Broadcasting on tcp://*:5558")

    time.sleep(1)  # 给 Subscriber 一点时间建立连接

    i = 0
    try:
        while True:
            topic = TOPICS[i % 3]
            msg = f"{topic}: message {i}"
            print(f"[Publisher] Sending: {msg}")
            pub.send_string(msg)

            i += 1
            time.sleep(0.3)
    except KeyboardInterrupt:
        pass
    finally:
        pub.close()
        ctx.term()


if __name__ == "__main__":
    main()
