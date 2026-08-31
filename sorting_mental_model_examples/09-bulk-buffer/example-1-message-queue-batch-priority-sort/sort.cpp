/*
 * 场景：批量缓冲区 —— 消息队列批量出队后按优先级排序指针
 * 心智模型：消息体可能较大（几百字节到几 KB），批处理时只排序消息指针，
 *          消息本身留在原来的缓冲区里不动。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Message {
    int id;
    int priority; // 数值越大优先级越高
    std::string payload;
};

int main()
{
    std::vector<int> priorities = {2, 5, 2, 8, 5, 1, 8, 3};
    std::vector<Message> inbox;
    for (size_t i = 0; i < priorities.size(); i++)
        inbox.push_back({static_cast<int>(i) + 1, priorities[i], std::string(256, 'a' + static_cast<char>(i))});

    std::vector<Message *> ptrs;
    for (auto &m : inbox)
        ptrs.push_back(&m);

    std::cout << "=== 批量缓冲区：消息队列批量出队后按优先级排序指针 ===\n";
    std::cout << "到达顺序 (id:priority): ";
    for (auto &m : inbox)
        std::cout << m.id << ":" << m.priority << " ";
    std::cout << "\n";

    // std::stable_sort 保证优先级相同时保留原始到达顺序（FIFO）
    std::stable_sort(ptrs.begin(), ptrs.end(),
                      [](const Message *a, const Message *b) { return a->priority > b->priority; });

    std::cout << "按优先级处理顺序 (id:priority): ";
    for (auto *m : ptrs)
        std::cout << m->id << ":" << m->priority << " ";
    std::cout << "\n\n说明：id=2 和 id=5 优先级都是 5，排序后 id=2 仍然排在 id=5 前面，\n"
                 "这是 stable_sort 保留的到达顺序（FIFO）。整个过程只移动指针，\n"
                 "inbox 里每条消息的 payload 都没有被拷贝或搬移过。\n";
    return 0;
}
