"""
场景：批量缓冲区 —— 直接搬移大缓冲区 vs 只排指针的成本对比
心智模型：批量数据重排时，直接交换整个大缓冲区代价 = 交换次数 x sizeof(Buffer)；
         只交换指针（引用）代价 = 交换次数 x 一个引用的大小，两者可能相差几个数量级。

方案一用 ctypes.Structure 模拟 C 语言里"值类型"的大缓冲区，直接交换会产生
真实的内存拷贝；方案二用普通 Python 对象（列表天然存的就是引用）做对比。
"""

import ctypes
import random

BUF_SIZE = 2048
N = 200
REF_SIZE = 8  # 64 位系统上一个引用/指针的大致大小


class Buffer(ctypes.Structure):
    _fields_ = [
        ("seq_no", ctypes.c_int),
        ("data", ctypes.c_char * BUF_SIZE),
    ]


class BufferObj:
    __slots__ = ("seq_no", "data")

    def __init__(self, seq_no, data):
        self.seq_no = seq_no
        self.data = data


bytes_direct = 0
bytes_pointer = 0


def swap_buffer_direct(arr, i, j):
    global bytes_direct
    tmp = Buffer()
    ctypes.memmove(ctypes.byref(tmp), ctypes.byref(arr[i]), ctypes.sizeof(Buffer))
    ctypes.memmove(ctypes.byref(arr[i]), ctypes.byref(arr[j]), ctypes.sizeof(Buffer))
    ctypes.memmove(ctypes.byref(arr[j]), ctypes.byref(tmp), ctypes.sizeof(Buffer))
    bytes_direct += 3 * ctypes.sizeof(Buffer)


def swap_pointer(arr, i, j):
    global bytes_pointer
    arr[i], arr[j] = arr[j], arr[i]
    bytes_pointer += 3 * REF_SIZE


def bubble_sort_direct(arr, n):
    for i in range(n - 1):
        for j in range(n - 1 - i):
            if arr[j].seq_no > arr[j + 1].seq_no:
                swap_buffer_direct(arr, j, j + 1)


def bubble_sort_pointer(arr, n):
    for i in range(n - 1):
        for j in range(n - 1 - i):
            if arr[j].seq_no > arr[j + 1].seq_no:
                swap_pointer(arr, j, j + 1)


def main():
    random.seed(999)
    seq_numbers = [random.randint(0, 9999) for _ in range(N)]

    array_type = Buffer * N
    buffers_a = array_type()
    for i, seq in enumerate(seq_numbers):
        buffers_a[i].seq_no = seq
        buffers_a[i].data = b"X" * BUF_SIZE

    buffers_b = [BufferObj(seq, b"X" * BUF_SIZE) for seq in seq_numbers]

    print("=== 批量缓冲区：直接搬移 vs 只排指针的成本对比 ===")
    print(f"缓冲区数量 = {N}，sizeof(Buffer) = {ctypes.sizeof(Buffer)} 字节，引用大小 约 {REF_SIZE} 字节")

    bubble_sort_direct(buffers_a, N)
    bubble_sort_pointer(buffers_b, N)

    ok = all(buffers_a[i].seq_no == buffers_b[i].seq_no for i in range(N))
    print(f"两种方案排序结果是否一致: {'是' if ok else '否'}")
    print(f"方案一 (直接交换 Buffer)   累计搬移数据量 约 {bytes_direct / (1024 * 1024):.2f} MB")
    print(f"方案二 (只交换引用)        累计搬移数据量 约 {bytes_pointer / 1024:.2f} KB")
    print(
        "\n说明：两种方案交换次数完全相同，唯一的区别是每次交换搬移的"
        "数据量——直接交换整块缓冲区 vs 只交换一个引用，实际搬移量"
        "相差了几个数量级，这就是批量缓冲区场景下优先选择指针排序的原因。"
    )


if __name__ == "__main__":
    main()
