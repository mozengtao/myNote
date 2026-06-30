"""统一的日志配置。横切关注点，与具体业务无关。"""

from __future__ import annotations

import logging

_CONFIGURED = False


def setup_logging(level: int = logging.INFO) -> None:
    """配置一次根 logger 的输出格式。重复调用是安全的。"""
    global _CONFIGURED
    if _CONFIGURED:
        return
    logging.basicConfig(
        level=level,
        format="%(asctime)s | %(levelname)-5s | %(name)-18s | %(message)s",
        datefmt="%H:%M:%S",
    )
    _CONFIGURED = True


def get_logger(name: str) -> logging.Logger:
    """获取一个带名字的 logger。"""
    return logging.getLogger(name)
