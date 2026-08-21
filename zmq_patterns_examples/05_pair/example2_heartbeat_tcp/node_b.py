#!/usr/bin/env python3
"""PAIR 示例2：心跳保活 —— Node B（connect 端）

逻辑与 node_a.py 完全相同，只有 bind/connect 和名字不同，
这正体现了 PAIR 两端完全对等、没有角色区分的特性。
"""
import time
import zmq

HEARTBEAT_INTERVAL = 1.0
TIMEOUT = 3.0


def main():
    ctx = zmq.Context()
    pair = ctx.socket(zmq.PAIR)
    pair.connect("tcp://127.0.0.1:5571")
    print("[NodeB] PAIR connected to tcp://127.0.0.1:5571")

    poller = zmq.Poller()
    poller.register(pair, zmq.POLLIN)

    last_sent = 0.0
    last_seen = time.monotonic()
    seq = 0
    warned = False

    try:
        while True:
            socks = dict(poller.poll(200))

            if pair in socks:
                msg = pair.recv_string()
                print(f"[NodeB] Received: {msg}")
                last_seen = time.monotonic()
                warned = False

            now = time.monotonic()
            if now - last_sent >= HEARTBEAT_INTERVAL:
                msg = f"PING B {seq}"
                seq += 1
                pair.send_string(msg)
                print(f"[NodeB] Sent: {msg}")
                last_sent = now

            if not warned and now - last_seen > TIMEOUT:
                gap = now - last_seen
                print(f"[NodeB] *** WARNING: no message from peer for {gap:.1f}s, peer might be down ***")
                warned = True
    except KeyboardInterrupt:
        pass
    finally:
        pair.close()
        ctx.term()


if __name__ == "__main__":
    main()
