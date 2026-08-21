#!/usr/bin/env python3
"""DEALER/ROUTER 示例1：DEALER 客户端
用法：python3 client.py <name>
"""
import sys
import zmq

NUM_REQUESTS = 5


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "?"

    ctx = zmq.Context()
    dealer = ctx.socket(zmq.DEALER)
    dealer.connect("tcp://127.0.0.1:5567")

    print(f"[Client {name}] Sending {NUM_REQUESTS} requests without waiting for replies...")
    for i in range(NUM_REQUESTS):
        req = f"req {i}"
        dealer.send_string(req)
        print(f"[Client {name}] Sent: {req}")

    for _ in range(NUM_REQUESTS):
        reply = dealer.recv_string()
        print(f"[Client {name}] Received reply: {reply}")

    dealer.close()
    ctx.term()


if __name__ == "__main__":
    main()
