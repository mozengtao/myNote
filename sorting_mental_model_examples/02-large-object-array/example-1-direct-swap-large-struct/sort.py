"""
场景：大对象数组直接交换（反面示例）
心智模型：Record 很大时，直接交换整个对象搬移开销巨大。
         本示例统计交换次数 x sizeof(Record)，量化这个代价。

注意：Python 里的普通对象（class 实例）本身就是通过引用存储在 list 中的，
list.sort() 天然只搬移引用，不会有这里说的问题。为了真实复现 C/C++ 中
"值类型大结构体"被直接拷贝的场景，这里改用 ctypes.Structure 构造一段
连续内存里的定长大记录，模拟 C 语言的内存布局。
"""

import ctypes
import random

PAYLOAD_SIZE = 500


class Record(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int),
        ("score", ctypes.c_double),
        ("payload", ctypes.c_char * PAYLOAD_SIZE),  # 模拟大对象负载
    ]


bytes_moved = 0


def swap_record(arr, i, j):
    global bytes_moved
    tmp = Record()
    ctypes.memmove(ctypes.byref(tmp), ctypes.byref(arr[i]), ctypes.sizeof(Record))
    ctypes.memmove(ctypes.byref(arr[i]), ctypes.byref(arr[j]), ctypes.sizeof(Record))
    ctypes.memmove(ctypes.byref(arr[j]), ctypes.byref(tmp), ctypes.sizeof(Record))
    bytes_moved += 3 * ctypes.sizeof(Record)  # 一次交换相当于 3 次整体拷贝


def partition(arr, low, high):
    pivot_score = arr[high].score
    i = low - 1
    for j in range(low, high):
        if arr[j].score < pivot_score:
            i += 1
            swap_record(arr, i, j)
    swap_record(arr, i + 1, high)
    return i + 1


def quicksort(arr, low, high):
    if low < high:
        p = partition(arr, low, high)
        quicksort(arr, low, p - 1)
        quicksort(arr, p + 1, high)


def main():
    random.seed(42)
    n = 2000
    array_type = Record * n
    records = array_type()
    for i in range(n):
        records[i].id = i
        records[i].score = random.uniform(0, 1000)
        records[i].payload = bytes([65 + i % 26]) * PAYLOAD_SIZE

    print("=== 大对象数组：直接交换整个 Record（反面示例） ===")
    print(f"sizeof(Record) = {ctypes.sizeof(Record)} 字节, 记录数 = {n}")

    quicksort(records, 0, n - 1)

    ok = all(records[i - 1].score <= records[i].score for i in range(1, n))
    print(f"排序结果: {'有序（正确）' if ok else '无序（错误）'}")
    print(f"累计搬移数据量 约 {bytes_moved / (1024 * 1024):.2f} MB")
    print(
        f"\n说明：每次交换都要整体拷贝 sizeof(Record)={ctypes.sizeof(Record)} 字节，"
        "这里用 ctypes.Structure 模拟 C 语言的值类型大结构体；"
        "普通 Python 对象存的是引用，天然不会有这个问题（对照 example-2）。"
    )


if __name__ == "__main__":
    main()
