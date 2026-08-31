/*
 * 场景：批量缓冲区 —— 音视频帧缓冲区按时间戳重排，不拷贝大帧数据
 * 心智模型：网络抖动会导致帧乱序到达，重建播放顺序时只需要排序
 *          "帧引用"（指针/索引），庞大的帧数据本体不需要移动。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

constexpr int kFrameSize = 4096; // 模拟一帧压缩后的数据大小

struct Frame {
    int seq_no;
    long timestamp_ms;
    std::string data;
};

int main()
{
    std::vector<long> arrival_timestamps = {320, 80, 240, 0, 160, 400}; // 网络抖动导致的乱序到达

    std::vector<Frame> buffer;
    for (size_t i = 0; i < arrival_timestamps.size(); i++)
        buffer.push_back({static_cast<int>(i), arrival_timestamps[i],
                           std::string(kFrameSize, 'A' + static_cast<char>(i))});

    std::vector<Frame *> ptrs;
    for (auto &f : buffer)
        ptrs.push_back(&f);

    std::cout << "=== 批量缓冲区：音视频帧按时间戳重排（不拷贝帧数据） ===\n";
    std::cout << "到达顺序 (timestamp_ms): ";
    for (auto &f : buffer)
        std::cout << f.timestamp_ms << " ";
    std::cout << "\n每帧大小 = " << kFrameSize << " 字节\n";

    std::sort(ptrs.begin(), ptrs.end(),
              [](const Frame *a, const Frame *b) { return a->timestamp_ms < b->timestamp_ms; });

    std::cout << "重建播放顺序 (timestamp_ms): ";
    for (auto *f : ptrs)
        std::cout << f->timestamp_ms << " ";
    std::cout << "\n\n说明：整个重排过程只交换了指针（8 字节），\n"
                 "每帧 "
              << kFrameSize << " 字节的实际数据始终留在 buffer 原来的位置上。\n";
    return 0;
}
