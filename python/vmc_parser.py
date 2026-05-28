#!/usr/bin/env python3
"""
VMC JSON 解析器
解析 VMC-brief.json 文件并输出层级结构的JSON

输出格式：
vmc name
  cm mac (maybe multiple)
    ip addrs (maybe multiple)
    mac state
    cpe number
"""

import json
from typing import List, Optional

from pydantic import BaseModel, Field, ValidationError


# =========================================================
# 1️⃣ Schema 定义层（VMC数据结构）
# =========================================================

class BriefInfo(BaseModel):
    """CM基本信息"""
    port: Optional[str] = None
    ip_address: List[str] = Field(default_factory=list, alias="ip-address")
    mac_state: str = Field(alias="mac-state")
    ds: Optional[str] = None
    us: Optional[str] = None
    timing: Optional[int] = None
    cpe: int
    bpi_plus: Optional[str] = Field(None, alias="bpi-plus")
    docsis_version: Optional[str] = Field(None, alias="docsis-version")
    vrf_name: Optional[str] = Field(None, alias="vrf-name")
    vlan_id: Optional[int] = Field(None, alias="vlan-id")
    sid: Optional[int] = None
    rx_pwr: Optional[str] = Field(None, alias="rx-pwr")


class ModemMac(BaseModel):
    """单个CM MAC信息"""
    mac_address: str = Field(alias="mac-address")
    vmc_name: str = Field(alias="vmc-name")
    md_index: int = Field(alias="md-index")
    brief: BriefInfo


class Vmc(BaseModel):
    """VMC信息"""
    vmc_name: str = Field(alias="vmc-name")
    modem_mac: List[ModemMac] = Field(alias="modem-mac")


class Modem(BaseModel):
    """Modem容器"""
    vmc: List[Vmc]


class CmView(BaseModel):
    """CM视图容器"""
    modem: Modem


class Cable(BaseModel):
    """Cable容器"""
    cm_view: CmView = Field(alias="cm-view")


class VcmOperImpl(BaseModel):
    """VCM操作实现容器"""
    cable: Cable = Field(alias="vcm-operimpl:cable")


class VmcInputData(BaseModel):
    """输入数据的根模型"""
    data: VcmOperImpl


# =========================================================
# 2️⃣ 输出数据结构定义
# =========================================================

class CmInfo(BaseModel):
    """CM输出信息"""
    ip_addrs: List[str]
    mac_state: str
    cpe_number: int


class VmcOutput(BaseModel):
    """VMC输出结构"""
    vmc_name: str
    cms: dict[str, CmInfo]  # key是mac地址


# =========================================================
# 3️⃣ JSON读取与校验
# =========================================================

def load_and_validate(path: str) -> VmcInputData:
    """
    读取JSON并进行schema校验
    """
    with open(path, "r", encoding="utf-8") as f:
        raw = json.load(f)

    try:
        data = VmcInputData(**raw)
        return data

    except ValidationError as e:
        print("❌ JSON schema 校验失败:")
        print(e.json(indent=2))
        raise


# =========================================================
# 4️⃣ 业务处理层（数据转换）
# =========================================================

def process_vmc_data(data: VmcInputData) -> dict:
    """
    将VMC数据转换为目标输出格式

    格式：
    {
      "vmc_name": {
        "cm_mac1": {
          "ip_addrs": [...],
          "mac_state": "...",
          "cpe_number": 0
        },
        ...
      }
    }
    """
    result = {}

    # 遍历所有VMC
    for vmc in data.data.cable.cm_view.modem.vmc:
        vmc_name = vmc.vmc_name
        cms = {}

        # 遍历每个VMC下的所有CM MAC
        for modem_mac in vmc.modem_mac:
            mac_addr = modem_mac.mac_address
            brief = modem_mac.brief

            cms[mac_addr] = {
                "ip_addrs": brief.ip_address,
                "mac_state": brief.mac_state,
                "cpe_number": brief.cpe
            }

        result[vmc_name] = cms

    return result


# =========================================================
# 5️⃣ 输出JSON
# =========================================================

def save_output(result: dict, path: str):
    """保存结果到文件"""
    with open(path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)


def print_hierarchical_output(result: dict):
    """
    打印层级结构的输出（便于查看）
    """
    print("\n📊 VMC 层级结构:")
    print("=" * 50)

    for vmc_name, cms in result.items():
        print(f"\n🏠 VMC: {vmc_name}")

        for mac_addr, cm_info in cms.items():
            print(f"  📱 CM MAC: {mac_addr}")
            print(f"    🌐 IP地址: {', '.join(cm_info['ip_addrs'])}")
            print(f"    📶 MAC状态: {cm_info['mac_state']}")
            print(f"    🖥️  CPE数量: {cm_info['cpe_number']}")


# =========================================================
# 6️⃣ 主流程
# =========================================================

def main():
    input_file = "/tmp/vmc-morris-dentist-1-brief.json"
    output_file = "/tmp/vmc_parsed_output.json"

    try:
        print("🔄 开始处理VMC数据...")

        # Step 1: 读取并校验输入文件
        print(f"📖 读取输入文件: {input_file}")
        data = load_and_validate(input_file)

        # Step 2: 数据转换
        print("🔄 转换数据格式...")
        result = process_vmc_data(data)

        # Step 3: 输出结果
        print(f"💾 保存输出文件: {output_file}")
        save_output(result, output_file)

        # Step 4: 打印层级结构（可选）
        print_hierarchical_output(result)

        print(f"\n✅ 处理完成! 输出文件: {output_file}")

    except FileNotFoundError:
        print(f"❌ 输入文件未找到: {input_file}")
    except ValidationError as e:
        print("❌ JSON格式验证失败")
    except Exception as e:
        print(f"❌ 处理过程中出错: {e}")


# =========================================================
# 7️⃣ 命令行使用示例
# =========================================================

def main_with_args(input_path: str, output_path: str = None):
    """
    支持命令行参数的版本
    """
    if output_path is None:
        output_path = input_path.replace('.json', '_parsed.json')

    try:
        print(f"🔄 处理文件: {input_path}")
        data = load_and_validate(input_path)
        result = process_vmc_data(data)
        save_output(result, output_path)
        print_hierarchical_output(result)
        print(f"✅ 输出文件: {output_path}")

    except Exception as e:
        print(f"❌ 错误: {e}")


if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1:
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else None
        main_with_args(input_file, output_file)
    else:
        main()