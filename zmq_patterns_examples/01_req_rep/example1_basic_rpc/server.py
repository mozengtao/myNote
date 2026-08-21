#!/usr/bin/env python3
"""REQ/REP 示例1：基础 Ping-Pong 服务端"""
import zmq


def main():
    ctx = zmq.Context()
    rep = ctx.socket(zmq.REP)
    rep.bind("tcp://*:5555")
    print("[Server] Listening on tcp://*:5555")

    try:
        while True:
            message = rep.recv_string()
            print(f"[Server] Received: {message}")

            n = message.split(" ")[1]
            rep.send_string(f"World {n}")
    except KeyboardInterrupt:
        pass
    finally:
        rep.close()
        ctx.term()


if __name__ == "__main__":
    main()
