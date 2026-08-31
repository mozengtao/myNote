/*
 * 场景：任务调度 —— "只取下一个"而非整体重排的增量选择模式
 * 心智模型：很多调度器根本不需要一个完整有序的序列，只需要每次能
 *          快速回答"接下来该运行谁"。用一次线性扫描找最优任务、
 *          原地标记完成，比每次都对剩余任务重新排序要简单直接得多，
 *          尤其是当任务会不断有新到达、旧完成、优先级会变化时。
 */
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct Task {
    std::string name;
    int priority;  // 数值越小优先级越高
    bool done = false;
};

// 只扫描一次，在"未完成"的任务里找优先级最高的那个，
// 不对任务数组做任何排序或搬运 —— 这是最朴素的"选择模式"。
static Task *pick_next(std::vector<Task> &tasks)
{
    Task *best = nullptr;
    for (auto &t : tasks) {
        if (t.done)
            continue;
        if (best == nullptr || t.priority < best->priority)
            best = &t;
    }
    return best;
}

static void print_pending(const std::string &label, const std::vector<Task> &tasks)
{
    std::cout << label << ": ";
    for (auto &t : tasks) {
        if (!t.done)
            std::cout << t.name << "(pri=" << t.priority << ") ";
    }
    std::cout << "\n";
}

int main()
{
    std::vector<Task> tasks = {
        {"cleanup", 5}, {"alert", 1}, {"report", 3}, {"backup", 4}, {"healthcheck", 2},
    };

    std::cout << "=== 任务调度：增量选择（不整体排序） ===\n";
    print_pending("初始待处理任务", tasks);

    int round = 1;
    while (true) {
        Task *next = pick_next(tasks);
        if (next == nullptr)
            break;
        std::cout << "第 " << round << " 轮：选中 [" << next->name << "]（优先级=" << next->priority
                   << "）\n";
        next->done = true;  // 原地标记完成，不移动任何元素

        // 模拟运行期间有新任务到达：直接追加到末尾，不需要
        // 重新排序已有任务，也不影响后续 pick_next() 的正确性。
        if (round == 2) {
            tasks.push_back({"urgent-fix", 0, false});
            std::cout << "        （运行期间新任务 [urgent-fix] 到达，优先级=0）\n";
        }

        print_pending("        剩余待处理任务", tasks);
        round++;
    }

    std::cout << "\n说明：pick_next() 每次只做一次 O(n) 扫描来回答\"接下来该运行谁\"，\n"
                 "任务数组自始至终没有被排序或搬运过；新任务可以随时追加，\n"
                 "优先级也可以随时变化，都不影响下一次选择的正确性。\n"
                 "这与前两个示例（增量插入、堆）是同一心智模式的另一种朴素实现：\n"
                 "调度器要的从来不是\"整体有序\"，而是\"每次都能选对下一个\"。\n";
    return 0;
}
