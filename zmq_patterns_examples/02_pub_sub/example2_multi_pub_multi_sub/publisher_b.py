#!/usr/bin/env python3
"""PUB/SUB 示例2：多发布者 —— Source B"""
import time
import zmq

TOPICS = ["SPORT", "AI", "NEWS"]


def main():
    ctx = zmq.Context()
    pub = ctx.socket(zmq.PUB)
    pub.bind("tcp://*:5560")
    print("[SourceB] Broadcasting on tcp://*:5560")

    time.sleep(1)

    i = 0
    try:
        while True:
            topic = TOPICS[i % 3]
            headline = f"{topic} headline #{i}"

            pub.send_string(topic, zmq.SNDMORE)
            pub.send_string("SourceB", zmq.SNDMORE)
            pub.send_string(headline)

            print(f"[SourceB] Sent: {topic} | SourceB | {headline}")
            i += 1
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        pub.close()
        ctx.term()


if __name__ == "__main__":
    main()
