#!/usr/bin/env python3
"""REQ/REP 示例3：模拟"偶尔很慢"的服务端"""
import random
import time
import zmq


def main():
    ctx = zmq.Context()
    rep = ctx.socket(zmq.REP)
    rep.bind("tcp://*:5557")
    print("[Server] Listening on tcp://*:5557 (30% chance of 4s slow response)")

    try:
        while True:
            message = rep.recv_string()
            print(f"[Server] Received: {message}")

            if random.random() < 0.3:
                print("[Server] Simulating slow processing (4s)...")
                time.sleep(4)
            else:
                time.sleep(random.uniform(0, 0.3))

            reply = f"Reply to {message}"
            rep.send_string(reply)
            print(f"[Server] Sent: {reply}")
    except KeyboardInterrupt:
        pass
    finally:
        rep.close()
        ctx.term()


if __name__ == "__main__":
    main()
