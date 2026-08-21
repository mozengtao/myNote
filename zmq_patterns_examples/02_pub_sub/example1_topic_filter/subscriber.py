#!/usr/bin/env python3
"""PUB/SUB 示例1：主题过滤广播 —— 订阅端（只订阅 AI）"""
import zmq


def main():
    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.connect("tcp://127.0.0.1:5558")
    sub.setsockopt_string(zmq.SUBSCRIBE, "AI")
    print("[Subscriber] Subscribed to topic: AI")

    try:
        while True:
            msg = sub.recv_string()
            print(f"[Subscriber] {msg}")
    except KeyboardInterrupt:
        pass
    finally:
        sub.close()
        ctx.term()


if __name__ == "__main__":
    main()
