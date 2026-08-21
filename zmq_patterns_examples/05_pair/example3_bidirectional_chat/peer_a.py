#!/usr/bin/env python3
"""PAIR 示例3：双向聊天 —— Peer A（bind 端）

同时监听 stdin 和 PAIR socket，谁先有数据就处理谁。
"""
import sys
import zmq


def main():
    ctx = zmq.Context()
    pair = ctx.socket(zmq.PAIR)
    pair.bind("tcp://*:5572")

    poller = zmq.Poller()
    poller.register(pair, zmq.POLLIN)
    poller.register(sys.stdin, zmq.POLLIN)

    print("[PeerA] Connected. Type messages, type /quit to exit.")
    print("> ", end="", flush=True)

    while True:
        socks = dict(poller.poll())

        if pair in socks:
            msg = pair.recv_string()
            print(f"\n[B]: {msg}\n> ", end="", flush=True)

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
