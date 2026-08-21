#!/usr/bin/env python3
"""DEALER/ROUTER 示例2：REQ 客户端（连接到 Broker 前端）
用法：python3 client.py <name>
"""
import sys
import zmq

NUM_REQUESTS = 3


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "?"

    ctx = zmq.Context()
    req = ctx.socket(zmq.REQ)
    req.connect("tcp://127.0.0.1:5568")

    for i in range(NUM_REQUESTS):
        msg = f"req {i}"
        print(f"[Client {name}] Sending: {msg}")
        req.send_string(msg)

        reply = req.recv_string()
        print(f"[Client {name}] Received: {reply}")

    req.close()
    ctx.term()


if __name__ == "__main__":
    main()
