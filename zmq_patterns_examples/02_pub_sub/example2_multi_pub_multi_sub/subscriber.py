#!/usr/bin/env python3
"""PUB/SUB 示例2：多发布者 —— 订阅端（同时连接A和B，只订阅 AI）"""
import zmq


def main():
    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.connect("tcp://127.0.0.1:5559")
    sub.connect("tcp://127.0.0.1:5560")
    sub.setsockopt_string(zmq.SUBSCRIBE, "AI")
    print("[Subscriber] Subscribed to topic: AI (from both Source A and Source B)")

    try:
        while True:
            topic, source, headline = [f.decode() for f in sub.recv_multipart()]
            print(f"[Subscriber] [{source}] {headline}")
    except KeyboardInterrupt:
        pass
    finally:
        sub.close()
        ctx.term()


if __name__ == "__main__":
    main()
