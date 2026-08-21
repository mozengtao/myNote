#!/usr/bin/env python3
"""DEALER/ROUTER 示例2：扩展请求-响应 Broker

前端 ROUTER 面向 REQ 客户端，后端 DEALER 面向 REP Worker，
zmq.proxy 自动转发消息，DEALER 天然做 Round-Robin 负载均衡。
"""
import zmq


def main():
    ctx = zmq.Context()

    frontend = ctx.socket(zmq.ROUTER)
    frontend.bind("tcp://*:5568")

    backend = ctx.socket(zmq.DEALER)
    backend.bind("tcp://*:5569")

    print("[Broker] Frontend (clients) on tcp://*:5568")
    print("[Broker] Backend  (workers) on tcp://*:5569")
    print("[Broker] Proxying...")

    try:
        zmq.proxy(frontend, backend)
    except KeyboardInterrupt:
        pass
    finally:
        frontend.close()
        backend.close()
        ctx.term()


if __name__ == "__main__":
    main()
