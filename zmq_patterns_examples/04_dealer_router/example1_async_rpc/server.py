#!/usr/bin/env python3
"""DEALER/ROUTER 示例1：ROUTER 服务端

ROUTER 收消息时，第一帧总是"这条消息来自谁"的 identity；
回复时必须把同样的 identity 作为第一帧发回去。
"""
import time
import zmq


def main():
    ctx = zmq.Context()
    router = ctx.socket(zmq.ROUTER)
    router.bind("tcp://*:5567")
    print("[Server] ROUTER listening on tcp://*:5567")

    try:
        while True:
            identity, payload = router.recv_multipart()
            payload_str = payload.decode()

            print(f"[Server] Received from client: {payload_str}")
            time.sleep((10 + (len(payload_str) * 7) % 50) / 1000.0)  # 模拟处理耗时

            reply = f"Reply to {payload_str}"
            router.send_multipart([identity, reply.encode()])
    except KeyboardInterrupt:
        pass
    finally:
        router.close()
        ctx.term()


if __name__ == "__main__":
    main()
