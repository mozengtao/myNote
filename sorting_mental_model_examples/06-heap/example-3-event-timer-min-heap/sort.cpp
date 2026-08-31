/*
 * 场景：堆 —— 定时器/事件队列，动态维护"下一个最早触发的事件"
 * 心智模型：堆不是为了得到完整有序序列，而是为了快速获取当前最值，
 *          并且可以在新事件加入后继续保持这个能力，而不需要重新排序全部事件。
 */
#include <iostream>
#include <queue>
#include <string>
#include <vector>

struct Event {
    int fire_time;
    std::string name;
    int period; // 0 表示一次性事件，非 0 表示周期性事件
};

struct CompareByFireTime {
    bool operator()(const Event &a, const Event &b) const
    {
        return a.fire_time > b.fire_time; // 小顶堆：fire_time 越小优先级越高
    }
};

int main()
{
    std::priority_queue<Event, std::vector<Event>, CompareByFireTime> pq;
    pq.push({100, "heartbeat", 50});
    pq.push({80, "log-flush", 40});
    pq.push({200, "report", 0});
    pq.push({30, "retry", 20});

    std::cout << "=== 堆：定时器事件队列，动态维护最早触发的事件 ===\n";
    std::cout << "初始事件数 = " << pq.size() << "\n\n";

    for (int round = 1; round <= 6 && !pq.empty(); round++) {
        Event e = pq.top();
        pq.pop();
        std::cout << "第 " << round << " 轮：触发事件 [" << e.name
                  << "] fire_time=" << e.fire_time << "\n";
        if (e.period > 0) {
            Event next = e;
            next.fire_time += e.period;
            pq.push(next);
            std::cout << "         -> 周期事件重新调度到 fire_time=" << next.fire_time << "\n";
        }
    }

    std::cout << "\n说明：每次只花 O(log n) 就能取出当前最早的事件，新事件（包括\n"
                 "周期性事件的下一次触发）随时可以加入堆中，完全不需要对整个\n"
                 "事件列表重新排序。std::priority_queue 就是工程中最常用的实现方式。\n";
    return 0;
}
