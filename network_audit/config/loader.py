"""配置加载器：把 config.yaml 解析为强类型的 Config 对象。

设计要点：
- 对外只暴露 ``load_config(path) -> Config``。
- 程序其它地方使用 ``config.hosts`` / ``config.timeout``，而不是裸 dict 的 ``data["hosts"]``。
- 优先使用 PyYAML；若环境未安装，则用一个仅覆盖本项目这份简单 YAML 的降级解析器，
  保证项目"开箱即跑"，不强依赖第三方库。
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Config:
    """运行配置。是一个对象，而不是到处传递的 dict。"""

    hosts: list[str] = field(default_factory=list)
    username: str = "admin"
    timeout: int = 10
    report_path: str = "report.md"


def load_config(path: str | Path) -> Config:
    """读取 YAML 文件并返回 Config 对象。"""
    path = Path(path)
    text = path.read_text(encoding="utf-8")
    data = _parse_yaml(text)
    return Config(
        hosts=list(data.get("hosts", [])),
        username=str(data.get("username", "admin")),
        timeout=int(data.get("timeout", 10)),
        report_path=str(data.get("report_path", "report.md")),
    )


def _parse_yaml(text: str) -> dict:
    """优先 PyYAML，缺失时降级到内置极简解析器。"""
    try:
        import yaml  # type: ignore

        return yaml.safe_load(text) or {}
    except ImportError:
        return _parse_simple_yaml(text)


def _parse_simple_yaml(text: str) -> dict:
    """仅支持本项目这份 YAML 的子集：标量键值对 + 简单列表。

    这是基础设施层的妥协实现：让 demo 在没有 PyYAML 的环境里也能运行。
    它不是一个通用 YAML 解析器，真实项目请直接依赖 PyYAML。
    """
    result: dict = {}
    current_list_key: str | None = None

    for raw_line in text.splitlines():
        line = raw_line.split("#", 1)[0].rstrip()
        if not line.strip():
            continue

        if line.lstrip().startswith("- "):
            item = line.lstrip()[2:].strip()
            if current_list_key is not None:
                result[current_list_key].append(_coerce(item))
            continue

        if ":" in line:
            key, _, value = line.partition(":")
            key = key.strip()
            value = value.strip()
            if value == "":
                result[key] = []
                current_list_key = key
            else:
                result[key] = _coerce(value)
                current_list_key = None

    return result


def _coerce(value: str):
    """把 YAML 标量字符串转成 int / 原字符串。"""
    if value.isdigit():
        return int(value)
    return value
