#!/usr/bin/env python3
"""REQ/REP 示例2：结构化计算服务客户端（多部分消息）"""
import zmq


def call(sock, op, a, b):
    sock.send_string(op, zmq.SNDMORE)
    sock.send_string(a, zmq.SNDMORE)
    sock.send_string(b)

    reply = sock.recv_string()
    print(f"[Client] {op} {a} {b} -> {reply}")


def main():
    ctx = zmq.Context()
    req = ctx.socket(zmq.REQ)
    req.connect("tcp://127.0.0.1:5556")

    call(req, "add", "3", "5")
    call(req, "sub", "10", "4")
    call(req, "mul", "6", "7")
    call(req, "div", "10", "0")
    call(req, "mod", "7", "2")

    req.close()
    ctx.term()


if __name__ == "__main__":
    main()
