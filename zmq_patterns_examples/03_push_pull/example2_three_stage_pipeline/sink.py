#!/usr/bin/env python3
"""PUSH/PULL 示例2：三级流水线 —— Sink"""
import time
import zmq


def main():
    ctx = zmq.Context()

    ctrl = ctx.socket(zmq.PULL)
    ctrl.bind("tcp://*:5565")

    results = ctx.socket(zmq.PULL)
    results.bind("tcp://*:5564")

    print("[Sink] Waiting for control message...")
    expected = int(ctrl.recv_string())
    print(f"[Sink] Expecting {expected} results")

    start = time.monotonic()
    step = max(expected // 5, 1)

    received = 0
    for received in range(1, expected + 1):
        results.recv_string()
        if received % step == 0:
            print(f"[Sink] Progress: {received}/{expected}")

    elapsed = time.monotonic() - start
    print(f"[Sink] All {received} results received. Total time: {elapsed:.2f}s")

    ctrl.close()
    results.close()
    ctx.term()


if __name__ == "__main__":
    main()
