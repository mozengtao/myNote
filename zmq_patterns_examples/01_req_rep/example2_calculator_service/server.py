#!/usr/bin/env python3
"""REQ/REP 示例2：结构化计算服务端（多部分消息）"""
import zmq


def handle_request(frames):
    if len(frames) != 3:
        return f"ERR|expected 3 frames, got {len(frames)}"

    op, a_s, b_s = frames
    a, b = float(a_s), float(b_s)

    if op == "add":
        return f"OK|{a + b:g}"
    elif op == "sub":
        return f"OK|{a - b:g}"
    elif op == "mul":
        return f"OK|{a * b:g}"
    elif op == "div":
        if b == 0:
            return "ERR|division by zero"
        return f"OK|{a / b:g}"
    else:
        return f"ERR|unknown operator: {op}"


def main():
    ctx = zmq.Context()
    rep = ctx.socket(zmq.REP)
    rep.bind("tcp://*:5556")
    print("[Server] Calculator service listening on tcp://*:5556")

    try:
        while True:
            frames = [f.decode() for f in rep.recv_multipart()]
            print(f"[Server] Request: {' '.join(frames)}")

            reply = handle_request(frames)
            print(f"[Server] Reply: {reply}")
            rep.send_string(reply)
    except KeyboardInterrupt:
        pass
    finally:
        rep.close()
        ctx.term()


if __name__ == "__main__":
    main()
