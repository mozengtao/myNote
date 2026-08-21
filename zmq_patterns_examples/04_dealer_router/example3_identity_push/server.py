#!/usr/bin/env python3
"""DEALER/ROUTER 示例3：基于 Identity 的主动推送服务端"""
import time
import zmq

PUSH_INTERVAL = 2.0  # 秒


def main():
    ctx = zmq.Context()
    router = ctx.socket(zmq.ROUTER)
    router.bind("tcp://*:5570")
    print("[Server] ROUTER listening on tcp://*:5570")

    poller = zmq.Poller()
    poller.register(router, zmq.POLLIN)

    clients = {}  # name -> identity(bytes)
    counter = 0
    last_push = time.monotonic()

    try:
        while True:
            socks = dict(poller.poll(200))  # 200ms超时，定期检查推送时机

            if router in socks:
                identity, payload = router.recv_multipart()
                text = payload.decode()

                if text.startswith("REGISTER "):
                    name = text[len("REGISTER "):]
                    clients[name] = identity
                    print(f"[Server] Registered client: {name}")

            if clients and time.monotonic() - last_push >= PUSH_INTERVAL:
                names = list(clients.keys())
                target_name = names[counter % len(names)]
                target_identity = clients[target_name]

                notif = f"System update #{counter}"
                print(f'[Server] Pushing notification to {target_name}: "{notif}"')
                router.send_multipart([target_identity, notif.encode()])

                counter += 1
                last_push = time.monotonic()
    except KeyboardInterrupt:
        pass
    finally:
        router.close()
        ctx.term()


if __name__ == "__main__":
    main()
