#!/usr/bin/env python3
"""PAIR 示例2：心跳保活 —— Node A（bind 端）"""
import time
import zmq

HEARTBEAT_INTERVAL = 1.0
TIMEOUT = 3.0


def main():
    ctx = zmq.Context()
    pair = ctx.socket(zmq.PAIR)
    pair.bind("tcp://*:5571")
    print("[NodeA] PAIR bound on tcp://*:5571")

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
                print(f"[NodeA] Received: {msg}")
                last_seen = time.monotonic()
                warned = False

            now = time.monotonic()
            if now - last_sent >= HEARTBEAT_INTERVAL:
                msg = f"PING A {seq}"
                seq += 1
                pair.send_string(msg)
                print(f"[NodeA] Sent: {msg}")
                last_sent = now

            if not warned and now - last_seen > TIMEOUT:
                gap = now - last_seen
                print(f"[NodeA] *** WARNING: no message from peer for {gap:.1f}s, peer might be down ***")
                warned = True
    except KeyboardInterrupt:
        pass
    finally:
        pair.close()
        ctx.term()


if __name__ == "__main__":
    main()
