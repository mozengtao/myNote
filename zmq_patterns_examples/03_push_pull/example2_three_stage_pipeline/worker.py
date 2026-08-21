#!/usr/bin/env python3
"""PUSH/PULL 示例2：三级流水线 —— Worker
用法：python3 worker.py <id>
"""
import sys
import time
import zmq


def main():
    worker_id = sys.argv[1] if len(sys.argv) > 1 else "?"

    ctx = zmq.Context()
    receiver = ctx.socket(zmq.PULL)
    receiver.connect("tcp://127.0.0.1:5563")

    sender = ctx.socket(zmq.PUSH)
    sender.connect("tcp://127.0.0.1:5564")

    processed = 0
    try:
        while True:
            msg = receiver.recv_string()
            cost = int(msg)
            time.sleep(cost / 1000.0)
            processed += 1
            sender.send_string("done")
    except KeyboardInterrupt:
        pass
    finally:
        print(f"[Worker {worker_id}] processed {processed} tasks")
        receiver.close()
        sender.close()
        ctx.term()


if __name__ == "__main__":
    main()
