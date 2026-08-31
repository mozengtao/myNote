/*
 * 场景：批量缓冲区 —— 直接搬移大缓冲区 vs 只排指针的成本对比
 * 心智模型：批量数据重排时，直接交换整个大缓冲区代价 = 交换次数 x sizeof(Buffer)；
 *          只交换指针代价 = 交换次数 x sizeof(指针)，两者可能相差几个数量级。
 */
#include <cstring>
#include <iostream>
#include <vector>

constexpr int kBufSize = 2048;
constexpr int kN = 200;

struct Buffer {
    int seq_no;
    char data[kBufSize];
};

static long g_bytes_direct = 0;
static long g_bytes_pointer = 0;

static void swap_buffer_direct(Buffer &a, Buffer &b)
{
    Buffer tmp = a;
    a = b;
    b = tmp;
    g_bytes_direct += 3L * static_cast<long>(sizeof(Buffer));
}

static void swap_ptr(Buffer *&a, Buffer *&b)
{
    Buffer *tmp = a;
    a = b;
    b = tmp;
    g_bytes_pointer += 3L * static_cast<long>(sizeof(Buffer *));
}

static void bubble_sort_direct(std::vector<Buffer> &arr)
{
    int n = static_cast<int>(arr.size());
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (arr[j].seq_no > arr[j + 1].seq_no)
                swap_buffer_direct(arr[j], arr[j + 1]);
}

static void bubble_sort_pointer(std::vector<Buffer *> &ptrs)
{
    int n = static_cast<int>(ptrs.size());
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (ptrs[j]->seq_no > ptrs[j + 1]->seq_no)
                swap_ptr(ptrs[j], ptrs[j + 1]);
}

int main()
{
    unsigned int seed = 999;
    std::vector<int> seq_numbers(kN);
    for (int i = 0; i < kN; i++) {
        seed = seed * 1103515245u + 12345u;
        seq_numbers[i] = static_cast<int>((seed >> 16) % 10000);
    }

    std::vector<Buffer> buffers_a(kN);
    std::vector<Buffer> buffers_b(kN);
    for (int i = 0; i < kN; i++) {
        buffers_a[i].seq_no = seq_numbers[i];
        buffers_b[i].seq_no = seq_numbers[i];
        std::memset(buffers_a[i].data, 'X', kBufSize);
        std::memset(buffers_b[i].data, 'X', kBufSize);
    }

    std::vector<Buffer *> ptrs(kN);
    for (int i = 0; i < kN; i++)
        ptrs[i] = &buffers_b[i];

    std::cout << "=== 批量缓冲区：直接搬移 vs 只排指针的成本对比 ===\n";
    std::cout << "缓冲区数量 = " << kN << "，sizeof(Buffer) = " << sizeof(Buffer)
              << " 字节，sizeof(Buffer*) = " << sizeof(Buffer *) << " 字节\n";

    bubble_sort_direct(buffers_a);
    bubble_sort_pointer(ptrs);

    bool ok = true;
    for (int i = 0; i < kN; i++) {
        if (buffers_a[i].seq_no != ptrs[i]->seq_no) {
            ok = false;
            break;
        }
    }

    std::cout << "两种方案排序结果是否一致: " << (ok ? "是" : "否") << "\n";
    std::cout << "方案一 (直接交换 Buffer)   累计搬移数据量 约 "
              << (g_bytes_direct / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "方案二 (只交换 Buffer*)    累计搬移数据量 约 " << (g_bytes_pointer / 1024.0)
              << " KB\n";
    std::cout << "\n说明：两种方案交换次数完全相同，唯一的区别是每次交换搬移的\n"
                 "数据量——直接交换整块缓冲区 vs 只交换一个指针，实际搬移量\n"
                 "相差了几个数量级，这就是批量缓冲区场景下优先选择指针排序的原因。\n";
    return 0;
}
