#!/usr/bin/env python3
"""
VMC JSON 解析器（简化版，不依赖外部库）
解析 VMC-brief.json 文件并输出层级结构的JSON

输出格式：
vmc name
  cm mac (maybe multiple)
    ip addrs (maybe multiple)
    mac state
    cpe number
"""

import json
import sys
from typing import Dict, List, Any


def load_json_file(file_path: str) -> Dict[str, Any]:
    """
    读取JSON文件
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"❌ 文件未找到: {file_path}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"❌ JSON格式错误: {e}")
        sys.exit(1)


def extract_vmc_data(data: Dict[str, Any]) -> Dict[str, Dict[str, Dict[str, Any]]]:
    """
    从复杂的JSON结构中提取VMC数据
    
    返回格式：
    {
        "vmc_name": {
            "mac_address": {
                "ip_addrs": [...],
                "mac_state": "...",
                "cpe_number": 0
            }
        }
    }
    """
    result = {}
    
    try:
        # 导航到VMC数据
        vmc_list = data["data"]["vcm-operimpl:cable"]["cm-view"]["modem"]["vmc"]
        
        for vmc_item in vmc_list:
            vmc_name = vmc_item["vmc-name"]
            cms = {}
            
            # 遍历每个modem-mac
            for modem_mac in vmc_item["modem-mac"]:
                mac_address = modem_mac["mac-address"]
                brief_info = modem_mac["brief"]
                
                # 提取需要的信息
                cm_info = {
                    "ip_addrs": brief_info.get("ip-address", []),
                    "mac_state": brief_info.get("mac-state", ""),
                    "cpe_number": brief_info.get("cpe", 0)
                }
                
                cms[mac_address] = cm_info
            
            result[vmc_name] = cms
            
    except KeyError as e:
        print(f"❌ JSON结构错误，缺少字段: {e}")
        sys.exit(1)
    
    return result


def save_json_output(data: Dict[str, Any], output_path: str) -> None:
    """
    保存JSON输出文件
    """
    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        print(f"✅ 输出文件已保存: {output_path}")
    except Exception as e:
        print(f"❌ 保存文件失败: {e}")


def print_hierarchical_structure(data: Dict[str, Dict[str, Dict[str, Any]]]) -> None:
    """
    以层级结构打印VMC数据
    """
    print("\n📊 VMC 层级结构:")
    print("=" * 50)
    
    for vmc_name, cms in data.items():
        print(f"\n🏠 VMC名称: {vmc_name}")
        
        for mac_address, cm_info in cms.items():
            print(f"  📱 CM MAC: {mac_address}")
            
            # IP地址列表
            ip_addrs = cm_info.get("ip_addrs", [])
            if ip_addrs:
                print(f"    🌐 IP地址: {', '.join(ip_addrs)}")
            else:
                print(f"    🌐 IP地址: 无")
            
            # MAC状态
            mac_state = cm_info.get("mac_state", "未知")
            print(f"    📶 MAC状态: {mac_state}")
            
            # CPE数量
            cpe_number = cm_info.get("cpe_number", 0)
            print(f"    🖥️  CPE数量: {cpe_number}")


def print_summary_stats(data: Dict[str, Dict[str, Dict[str, Any]]]) -> None:
    """
    打印统计信息
    """
    total_vmcs = len(data)
    total_cms = sum(len(cms) for cms in data.values())
    total_ips = sum(
        len(cm_info.get("ip_addrs", [])) 
        for cms in data.values() 
        for cm_info in cms.values()
    )
    
    print(f"\n📈 统计信息:")
    print(f"   VMC总数: {total_vmcs}")
    print(f"   CM总数: {total_cms}")
    print(f"   IP地址总数: {total_ips}")


def main():
    """
    主函数
    """
    # 默认输入输出文件
    input_file = "/tmp/vmc-morris-dentist-1-brief.json"
    output_file = "/tmp/vmc_parsed_output.json"
    
    # 支持命令行参数
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    print(f"🔄 开始处理VMC数据...")
    print(f"📖 输入文件: {input_file}")
    print(f"💾 输出文件: {output_file}")
    
    # Step 1: 读取JSON文件
    json_data = load_json_file(input_file)
    
    # Step 2: 提取VMC数据
    vmc_data = extract_vmc_data(json_data)
    
    # Step 3: 保存输出文件
    save_json_output(vmc_data, output_file)
    
    # Step 4: 打印层级结构
    print_hierarchical_structure(vmc_data)
    
    # Step 5: 打印统计信息
    print_summary_stats(vmc_data)
    
    print(f"\n🎉 处理完成!")


if __name__ == "__main__":
    main()