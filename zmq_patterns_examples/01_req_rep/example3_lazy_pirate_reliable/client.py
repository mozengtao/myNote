#!/usr/bin/env python3
"""REQ/REP 示例3：Lazy Pirate 客户端（超时 + 重试）"""
import zmq

SERVER_ENDPOINT = "tcp://127.0.0.1:5557"
REQUEST_TIMEOUT = 2500  # ms，明显小于服务端最坏延迟(4s)
REQUEST_RETRIES = 3
NUM_TASKS = 5


def new_req_socket(ctx):
    s = ctx.socket(zmq.REQ)
    s.connect(SERVER_ENDPOINT)
    return s


def main():
    ctx = zmq.Context()
    client = new_req_socket(ctx)
    poller = zmq.Poller()
    poller.register(client, zmq.POLLIN)

    for task in range(NUM_TASKS):
        request = f"Request {task}"
        print(f"[Client] Sending: {request}")
        client.send_string(request)

        retries_left = REQUEST_RETRIES
        got_reply = False

        while not got_reply:
            socks = dict(poller.poll(REQUEST_TIMEOUT))

            if client in socks and socks[client] == zmq.POLLIN:
                reply = client.recv_string()
                print(f"[Client] Received reply: {reply}")
                got_reply = True
                retries_left = REQUEST_RETRIES
                continue

            retries_left -= 1
            if retries_left == 0:
                print(f"[Client] Server seems offline, abandoning task: {request}")
                break

            print(f"[Client] No response from server, retrying... ({retries_left} retries left)")

            # REQ socket 处于"半个问句"状态，必须整体丢弃重建
            poller.unregister(client)
            client.close()
            client = new_req_socket(ctx)
            poller.register(client, zmq.POLLIN)
            client.send_string(request)

    client.close()
    ctx.term()


if __name__ == "__main__":
    main()
