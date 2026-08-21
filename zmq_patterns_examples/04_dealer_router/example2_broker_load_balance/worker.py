#!/usr/bin/env python3
"""DEALER/ROUTER 示例2：REP Worker（连接到 Broker 后端）
用法：python3 worker.py <id>
"""
import sys
import zmq


def main():
    worker_id = sys.argv[1] if len(sys.argv) > 1 else "?"

    ctx = zmq.Context()
    rep = ctx.socket(zmq.REP)
    rep.connect("tcp://127.0.0.1:5569")
    print(f"[Worker {worker_id}] Connected to broker backend")

    try:
        while True:
            message = rep.recv_string()
            print(f"[Worker {worker_id}] Handling: {message}")
            rep.send_string(f"Handled by worker {worker_id}: {message}")
    except KeyboardInterrupt:
        pass
    finally:
        rep.close()
        ctx.term()


if __name__ == "__main__":
    main()
