#!/usr/bin/env python3
"""REQ/REP 示例1：基础 Ping-Pong 客户端"""
import zmq


def main():
    ctx = zmq.Context()
    req = ctx.socket(zmq.REQ)
    req.connect("tcp://127.0.0.1:5555")

    for i in range(5):
        msg = f"Hello {i}"
        print(f"[Client] Sending: {msg}")
        req.send_string(msg)

        reply = req.recv_string()
        print(f"[Client] Received reply: {reply}")

    req.close()
    ctx.term()


if __name__ == "__main__":
    main()
