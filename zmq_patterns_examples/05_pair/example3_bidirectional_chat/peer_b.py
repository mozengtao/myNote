#!/usr/bin/env python3
"""PAIR 示例3：双向聊天 —— Peer B（connect 端）

逻辑与 peer_a.py 完全相同，只有 bind/connect 和标签不同。
"""
import sys
import zmq


def main():
    ctx = zmq.Context()
    pair = ctx.socket(zmq.PAIR)
    pair.connect("tcp://127.0.0.1:5572")

    poller = zmq.Poller()
    poller.register(pair, zmq.POLLIN)
    poller.register(sys.stdin, zmq.POLLIN)

    print("[PeerB] Connected. Type messages, type /quit to exit.")
    print("> ", end="", flush=True)

    while True:
        socks = dict(poller.poll())

        if pair in socks:
            msg = pair.recv_string()
            print(f"\n[A]: {msg}\n> ", end="", flush=True)

        if sys.stdin.fileno() in socks:
            line = sys.stdin.readline().rstrip("\n")
            pair.send_string(line)

            if line == "/quit":
                break

            print("> ", end="", flush=True)

    pair.close()
    ctx.term()


if __name__ == "__main__":
    main()
