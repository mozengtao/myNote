/*
 * 场景：任务调度 —— 最早截止时间优先（EDF），用堆维护任务指针
 * 心智模型：调度器只关心"下一个该运行的是谁"，用堆可以在 O(log N)
 *          时间内完成插入新任务、取出截止时间最近的任务，
 *          完全不需要对整个任务集合做完整排序。
 */
#include <iostream>
#include <queue>
#include <string>
#include <vector>

struct Task {
    std::string name;
    int deadline;  // 截止时间：数值越小越紧急
    int duration;  // 预计运行时长
};

// 比较器只比较 deadline，堆中存放的是指针，交换成本仅是一个地址
struct EarlierDeadline {
    bool operator()(const Task *a, const Task *b) const { return a->deadline > b->deadline; }
};

int main()
{
    std::vector<Task> pool = {
        {"render-frame", 8, 2},
        {"send-heartbeat", 3, 1},
        {"flush-log", 15, 4},
        {"process-input", 5, 1},
        {"sync-db", 20, 6},
    };

    std::cout << "=== 任务调度：最早截止时间优先（EDF） ===\n";
    std::cout << "任务到达顺序（未排序）：";
    for (auto &t : pool)
        std::cout << t.name << "(ddl=" << t.deadline << ") ";
    std::cout << "\n\n";

    // 只把指针压入堆，堆内部的比较/交换从不触碰 Task 本体
    std::priority_queue<Task *, std::vector<Task *>, EarlierDeadline> pq;
    for (auto &t : pool)
        pq.push(&t);

    std::cout << "按截止时间依次调度：\n";
    int clock = 0;
    while (!pq.empty()) {
        Task *t = pq.top();
        pq.pop();
        std::cout << "  时刻 " << clock << "：运行 [" << t->name << "]（截止时间=" << t->deadline
                   << "，时长=" << t->duration << "）"
                   << (clock + t->duration > t->deadline ? "<- 已超过截止时间！" : "") << "\n";
        clock += t->duration;
    }

    std::cout << "\n说明：EDF 调度不需要提前把所有任务按截止时间整体排序一次；\n"
                 "新任务随时可以 O(log N) 插入堆中，取出下一个任务也是 O(log N)，\n"
                 "堆里搬运的始终只是任务指针，Task 本体从未被移动过。\n";
    return 0;
}
