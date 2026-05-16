#!/usr/bin/env python3
"""
JSON + Schema 校验（Pydantic）通用模板

核心思想：
    JSON → Schema(Model) → Validation → Typed Object → Business Logic → Output

适用场景：
    - API 返回 JSON 解析
    - 配置文件校验
    - 数据处理 pipeline
"""

import json
from typing import List, Optional

from pydantic import BaseModel, Field, ValidationError, field_validator


# =========================================================
# 1️⃣ Schema 定义层（数据结构 + 约束 = 核心）
# =========================================================

class Item(BaseModel):
    # 基本字段定义（类型 + 约束）
    id: int
    name: str
    price: float = Field(..., ge=0)  # ge=0 表示 >= 0

    # 自定义校验（用于复杂规则）
    @field_validator("price")
    def check_price(cls, v):
        if v > 10000:
            raise ValueError("price too large")
        return v


class User(BaseModel):
    name: str
    age: int = Field(..., ge=0)


class InputData(BaseModel):
    user: User
    items: List[Item]
    metadata: Optional[dict] = None  # 可选字段


# =========================================================
# 2️⃣ JSON 读取 + Schema 校验（入口防火墙）
# =========================================================

def load_and_validate(path: str) -> InputData:
    """
    读取 JSON 并进行 schema 校验
    - 成功：返回强类型对象
    - 失败：直接抛异常（不要吞）
    """
    with open(path, "r", encoding="utf-8") as f:
        raw = json.load(f)

    try:
        # 核心：Pydantic 自动完成
        # 1. 类型转换（str → float 等）
        # 2. 字段校验
        # 3. 嵌套结构解析
        data = InputData(**raw)
        return data

    except ValidationError as e:
        print("❌ JSON schema 校验失败:")
        print(e.json(indent=2))
        raise


# =========================================================
# 3️⃣ 业务处理层（强类型，逻辑更清晰）
# =========================================================

def process(data: InputData) -> dict:
    """
    这里写你的核心业务逻辑
    此时 data 已经是“强类型对象”，不是 dict
    """

    # 示例：统计总价
    total_price = sum(item.price for item in data.items)

    # 示例：结构重组（projection）
    result = {
        "username": data.user.name,
        "total_price": total_price,
        "item_count": len(data.items),
    }

    return result


# =========================================================
# 4️⃣ 输出 JSON
# =========================================================

def save_output(result: dict, path: str):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)


# =========================================================
# 5️⃣ 示例数据（方便你直接运行验证）
# =========================================================

def generate_sample_input(path: str):
    """
    生成一份合法的示例 JSON
    """
    sample = {
        "user": {
            "name": "Morris",
            "age": 30
        },
        "items": [
            {"id": 1, "name": "apple", "price": 3.5},
            {"id": 2, "name": "banana", "price": 2.0}
        ],
        "metadata": {
            "source": "demo"
        }
    }

    with open(path, "w", encoding="utf-8") as f:
        json.dump(sample, f, indent=2, ensure_ascii=False)


# =========================================================
# 6️⃣ 主流程（标准 pipeline）
# =========================================================

def main():
    input_file = "input.json"
    output_file = "output.json"

    # Step 1: 生成示例输入（实际项目中可以删掉）
    generate_sample_input(input_file)

    # Step 2: 读取 + 校验
    data = load_and_validate(input_file)

    # Step 3: 业务处理
    result = process(data)

    # Step 4: 输出结果
    save_output(result, output_file)

    print("✅ 处理完成:", output_file)


# =========================================================
# 7️⃣ 入口
# =========================================================

if __name__ == "__main__":
    main()