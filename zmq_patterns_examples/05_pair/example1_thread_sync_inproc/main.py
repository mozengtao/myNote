#!/usr/bin/env python3
"""PAIR 示例1：inproc 线程间控制信号

主线程 bind "inproc://control"，工作线程 connect 同一地址。
inproc 要求两端共享同一个 Context，且必须先 bind 后 connect。
"""
import threading
import time
import zmq

NUM_STEPS = 10


def worker_thread(ctx):
    pair = ctx.socket(zmq.PAIR)
    pair.connect("inproc://control")

    cmd = pair.recv_string()
    print(f"[Worker] Received {cmd}, beginning work...")

    poller = zmq.Poller()
    poller.register(pair, zmq.POLLIN)

    stopped = False
    progress = 0
    for progress in range(NUM_STEPS):
        time.sleep(0.2)  # 模拟一步工作

        print(f"[Worker] Progress {progress}/{NUM_STEPS}")
        pair.send_string(f"PROGRESS {progress}")

        # 非阻塞检查是否收到 STOP 信号
        socks = dict(poller.poll(0))
        if pair in socks:
            cmd = pair.recv_string()
            if cmd == "STOP":
                print("[Worker] Received STOP signal, exiting early")
                stopped = True
                break

    if stopped:
        done = f"DONE (stopped early at progress {progress})"
    else:
        done = f"DONE (completed all {NUM_STEPS} steps)"
    pair.send_string(done)

    pair.close()


def main():
    ctx = zmq.Context()
    pair = ctx.socket(zmq.PAIR)
    pair.bind("inproc://control")  # inproc 必须先 bind

    t = threading.Thread(target=worker_thread, args=(ctx,))
    t.start()

    pair.send_string("START")
    print("[Main] Sent START")

    stop_sent = False
    while True:
        msg = pair.recv_string()

        if msg.startswith("PROGRESS"):
            p = int(msg.split(" ")[1])
            print(f"[Main] Progress: {p}/{NUM_STEPS}")

            if p == 3 and not stop_sent:
                print("[Main] Got progress 3, sending STOP early")
                pair.send_string("STOP")
                stop_sent = True
        elif msg.startswith("DONE"):
            print(f"[Main] Worker done: {msg}")
            break

    t.join()

    pair.close()
    ctx.term()


if __name__ == "__main__":
    main()
