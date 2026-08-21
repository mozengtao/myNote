#!/usr/bin/env python3
"""PUB/SUB 示例2：多发布者 —— Source A"""
import time
import zmq

TOPICS = ["NEWS", "SPORT", "AI"]


def main():
    ctx = zmq.Context()
    pub = ctx.socket(zmq.PUB)
    pub.bind("tcp://*:5559")
    print("[SourceA] Broadcasting on tcp://*:5559")

    time.sleep(1)

    i = 0
    try:
        while True:
            topic = TOPICS[i % 3]
            headline = f"{topic} headline #{i}"

            pub.send_string(topic, zmq.SNDMORE)
            pub.send_string("SourceA", zmq.SNDMORE)
            pub.send_string(headline)

            print(f"[SourceA] Sent: {topic} | SourceA | {headline}")
            i += 1
            time.sleep(0.4)
    except KeyboardInterrupt:
        pass
    finally:
        pub.close()
        ctx.term()


if __name__ == "__main__":
    main()
