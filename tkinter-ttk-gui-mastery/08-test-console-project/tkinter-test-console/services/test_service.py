"""
services/test_service.py

Layer 8 综合项目：业务服务层（Test Service）。

这个模块实现"模拟设备测试"的全部业务逻辑，并负责把它放进一个后台线程里
运行。严格遵守 Layer 7 建立的规则：

    - 本模块不 import tkinter，不引用任何 Widget/Variable。
    - 唯一的输出通道是 queue.Queue（ServiceMessage）。
    - 取消是协作式的（threading.Event），不是强制终止。
    - 所有异常在后台线程内部被捕获，转换成 ServiceMessage 传回调用方。

对应架构图（见根 README）：

    Tkinter UI -> Application Controller -> Test Service -> Worker Thread
        -> Queue -> Tkinter UI

本模块就是其中的 "Test Service" + "Worker Thread" 两层。
"""

from __future__ import annotations

import queue
import random
import threading
import time
from dataclasses import dataclass
from typing import Literal

from models import TestConfig, TestRunResult

# 每个模拟步骤的耗时（秒）。用较短的时间方便教学时快速看到效果；
# 真实工程中这里会是一次 SSH 命令、一次 HTTP 请求等真实耗时操作。
STEP_DURATION_SECONDS = 0.15

# 单次测试运行失败的模拟概率（仅用于演示异常/失败路径，不代表真实设备行为）。
SIMULATED_FAILURE_PROBABILITY = 0.15

ServiceMessageKind = Literal[
    "log",
    "run_progress",  # 当前批次内，第几步 / 共多少步
    "run_result",  # 单次运行完成，payload 是 TestRunResult
    "batch_done",  # 整个批次（repeat_count 次）全部正常完成
    "batch_cancelled",  # 批次被用户取消
    "batch_error",  # 批次因未预期异常终止
]


@dataclass(frozen=True)
class ServiceMessage:
    """Test Service -> UI 的统一消息信封（与 Layer 7 的 WorkerMessage 同一模式）。"""

    kind: ServiceMessageKind
    payload: object = None


def _run_single_test(
    run_index: int, config: TestConfig, cancel_event: threading.Event, out_queue: "queue.Queue[ServiceMessage]"
) -> TestRunResult | None:
    """执行一次具体的测试运行（批次中的第 run_index 次）。

    返回 None 表示在执行过程中检测到取消信号，调用方需要据此提前结束
    整个批次；否则返回这一次的 TestRunResult。
    """
    started_at = time.monotonic()
    total_steps = config.test_type.step_count

    for step in range(1, total_steps + 1):
        if cancel_event.is_set():
            return None

        time.sleep(STEP_DURATION_SECONDS)

        out_queue.put(
            ServiceMessage(
                "run_progress",
                payload={"run_index": run_index, "step": step, "total_steps": total_steps},
            )
        )

    duration_ms = int((time.monotonic() - started_at) * 1000)
    passed = random.random() >= SIMULATED_FAILURE_PROBABILITY
    message = "设备响应正常" if passed else "设备响应超时或返回异常状态"

    return TestRunResult(
        run_index=run_index, passed=passed, duration_ms=duration_ms, message=message
    )


def run_test_batch(
    config: TestConfig,
    cancel_event: threading.Event,
    out_queue: "queue.Queue[ServiceMessage]",
) -> None:
    """在后台线程中执行的入口函数：连续执行 config.repeat_count 次测试。

    这是 Layer 7 "run_simulated_test" 的工程化版本：
        - 支持"连续执行多个测试"（repeat_count）。
        - 每次运行的结果单独上报（run_result），不等全部跑完才汇总。
        - 任意时刻可以被取消，取消后已经完成的运行结果仍然有效保留。
        - 任何未预期异常都会被捕获并转换成 batch_error 消息。
    """
    try:
        for run_index in range(1, config.repeat_count + 1):
            if cancel_event.is_set():
                out_queue.put(ServiceMessage("batch_cancelled", payload=run_index - 1))
                return

            out_queue.put(
                ServiceMessage(
                    "log",
                    payload=f"开始第 {run_index}/{config.repeat_count} 次测试"
                    f"（{config.test_type.display_name}，设备 {config.device_address}）",
                )
            )

            result = _run_single_test(run_index, config, cancel_event, out_queue)

            if result is None:
                # _run_single_test 内部检测到了取消信号。
                out_queue.put(ServiceMessage("batch_cancelled", payload=run_index - 1))
                return

            out_queue.put(ServiceMessage("run_result", payload=result))
            status_text = "PASS" if result.passed else "FAIL"
            out_queue.put(
                ServiceMessage(
                    "log",
                    payload=f"第 {run_index} 次测试结束：{status_text}"
                    f"（耗时 {result.duration_ms} ms，{result.message}）",
                )
            )

        out_queue.put(ServiceMessage("batch_done", payload=config.repeat_count))
    except Exception as exc:  # noqa: BLE001 - 故意捕获所有异常传回 UI
        out_queue.put(ServiceMessage("batch_error", payload=str(exc)))


class TestService:
    """Application Controller 用来控制后台任务的句柄。

    这个类本身仍然不 import tkinter；它只负责"管理线程的生命周期"
    （创建、启动、取消、等待结束），不管理界面展示（展示逻辑在
    ui/main_window.py 里，通过轮询这个类暴露的队列来完成）。
    """

    def __init__(self) -> None:
        self._thread: threading.Thread | None = None
        self._cancel_event: threading.Event | None = None
        self.messages: "queue.Queue[ServiceMessage]" = queue.Queue()

    def is_running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def start(self, config: TestConfig) -> None:
        if self.is_running():
            raise RuntimeError("已有测试批次在运行，不能重复启动。")

        config.validate()

        self._cancel_event = threading.Event()
        self._thread = threading.Thread(
            target=run_test_batch,
            args=(config, self._cancel_event, self.messages),
            daemon=True,
        )
        self._thread.start()

    def cancel(self) -> None:
        if self._cancel_event is not None:
            self._cancel_event.set()

    def shutdown(self, timeout: float = 2.0) -> None:
        """供应用退出时调用：请求取消并等待线程结束。"""
        self.cancel()
        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout=timeout)
