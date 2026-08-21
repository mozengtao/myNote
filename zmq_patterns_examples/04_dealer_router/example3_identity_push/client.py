#!/usr/bin/env python3
"""DEALER/ROUTER 示例3：注册并接收主动推送的客户端
用法：python3 client.py <name>
"""
import sys
import zmq


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "?"

    ctx = zmq.Context()
    dealer = ctx.socket(zmq.DEALER)
    dealer.connect("tcp://127.0.0.1:5570")

    dealer.send_string(f"REGISTER {name}")
    print(f"[Client {name}] Registered with server")

    try:
        while True:
            notif = dealer.recv_string()
            print(f"[Client {name}] Notification: {notif}")
    except KeyboardInterrupt:
        pass
    finally:
        dealer.close()
        ctx.term()


if __name__ == "__main__":
    main()
