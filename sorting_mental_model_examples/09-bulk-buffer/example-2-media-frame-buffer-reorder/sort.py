"""
场景：批量缓冲区 —— 音视频帧缓冲区按时间戳重排，不拷贝大帧数据
心智模型：网络抖动会导致帧乱序到达，重建播放顺序时只需要排序
         "帧引用"，庞大的帧数据本体不需要移动。
"""

from dataclasses import dataclass

FRAME_SIZE = 4096  # 模拟一帧压缩后的数据大小


@dataclass
class Frame:
    seq_no: int
    timestamp_ms: int
    data: bytes


def main():
    arrival_timestamps = [320, 80, 240, 0, 160, 400]  # 网络抖动导致的乱序到达

    buffer = [
        Frame(i, ts, bytes([65 + i]) * FRAME_SIZE)
        for i, ts in enumerate(arrival_timestamps)
    ]

    print("=== 批量缓冲区：音视频帧按时间戳重排（不拷贝帧数据） ===")
    print("到达顺序 (timestamp_ms): " + " ".join(str(f.timestamp_ms) for f in buffer))
    print(f"每帧大小 = {FRAME_SIZE} 字节")

    # sorted() 只重排引用，不会拷贝 Frame 里几 KB 的 data
    playback_order = sorted(buffer, key=lambda f: f.timestamp_ms)

    print(
        "重建播放顺序 (timestamp_ms): "
        + " ".join(str(f.timestamp_ms) for f in playback_order)
    )
    print(
        f"\n说明：整个重排过程只重新排列了引用，每帧 {FRAME_SIZE} 字节的实际数据"
        "始终留在原来创建时的位置上，从未被拷贝或移动。"
    )


if __name__ == "__main__":
    main()
