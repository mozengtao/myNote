#!/usr/bin/env python3
"""
gRPC 层次模型可视化演示
展示从应用层到网络层的数据流动过程
"""

import time

def print_header(text):
    print("\n" + "=" * 70)
    print(f"  {text}")
    print("=" * 70)


def print_layer(layer_name, action, details=""):
    print(f"\n[{layer_name}]")
    print(f"  ↓ {action}")
    if details:
        print(f"     {details}")
    time.sleep(0.3)


def visualize_client_request():
    """演示客户端请求的层次流动"""
    
    print_header("客户端请求流程：从应用层到网络层")
    
    print("\n🔵 客户端发起 RPC 调用：Add(5, 10)")
    time.sleep(0.5)
    
    # 第1层：应用层
    print_layer(
        "第1层：应用层",
        "创建请求对象",
        "request = BinaryOperation(a=5, b=10)"
    )
    
    # 第2层：gRPC 核心层
    print_layer(
        "第2层：gRPC 核心层",
        "序列化为 Protocol Buffer",
        "bytes = request.SerializeToString() → [0x08, 0x05, 0x10, 0x0a]"
    )
    
    print_layer(
        "第2层：gRPC 核心层",
        "添加 gRPC 元数据",
        "metadata = {':path': '/Calculator/Add', ':method': 'POST', ...}"
    )
    
    # 第3层：HTTP/2 层
    print_layer(
        "第3层：HTTP/2 层",
        "创建 HEADERS 帧",
        "HEADERS frame with method path, content-type"
    )
    
    print_layer(
        "第3层：HTTP/2 层",
        "创建 DATA 帧",
        "DATA frame with serialized message (4 bytes)"
    )
    
    print_layer(
        "第3层：HTTP/2 层",
        "压缩头部（HPACK）",
        "压缩后大小减少约 70%"
    )
    
    print_layer(
        "第3层：HTTP/2 层",
        "分配 Stream ID",
        "Stream ID: 1 (多路复用)"
    )
    
    # 第4层：TCP/IP 层
    print_layer(
        "第4层：TCP/IP 层",
        "封装为 TCP 数据包",
        "TCP segment with seq=1000, ack=2000"
    )
    
    print_layer(
        "第4层：TCP/IP 层",
        "发送到网络",
        "通过 TCP 连接发送到 localhost:50051"
    )
    
    print("\n" + "─" * 70)
    print("  📡 数据通过网络传输...")
    print("─" * 70)


def visualize_server_response():
    """演示服务器响应的层次流动"""
    
    print_header("服务器响应流程：从网络层到应用层")
    
    print("\n🔴 服务器处理请求")
    time.sleep(0.5)
    
    # 第4层：TCP/IP 层
    print_layer(
        "第4层：TCP/IP 层",
        "接收 TCP 数据包",
        "TCP segment received from client"
    )
    
    # 第3层：HTTP/2 层
    print_layer(
        "第3层：HTTP/2 层",
        "解析 HTTP/2 帧",
        "HEADERS frame + DATA frame"
    )
    
    print_layer(
        "第3层：HTTP/2 层",
        "解压头部（HPACK）",
        "还原原始头部信息"
    )
    
    print_layer(
        "第3层：HTTP/2 层",
        "从 Stream ID 1 提取数据",
        "读取完整的请求消息"
    )
    
    # 第2层：gRPC 核心层
    print_layer(
        "第2层：gRPC 核心层",
        "提取方法名和元数据",
        "method = '/Calculator/Add'"
    )
    
    print_layer(
        "第2层：gRPC 核心层",
        "反序列化 Protocol Buffer",
        "request = BinaryOperation.ParseFromString(bytes) → a=5, b=10"
    )
    
    print_layer(
        "第2层：gRPC 核心层",
        "路由到对应的 Servicer",
        "调用 CalculatorServicer.Add(request, context)"
    )
    
    # 第1层：应用层
    print_layer(
        "第1层：应用层",
        "执行业务逻辑",
        "result = 5 + 10 = 15"
    )
    
    print_layer(
        "第1层：应用层",
        "创建响应对象",
        "return Result(value=15)"
    )
    
    print("\n" + "─" * 70)
    print("  📤 准备发送响应...")
    print("─" * 70)
    
    # 响应返回（反向流程）
    print("\n[响应返回流程]")
    print("  [应用层] → [gRPC核心] → [HTTP/2] → [TCP/IP] → 网络 → 客户端")


def visualize_layer_architecture():
    """可视化层次架构"""
    
    print_header("gRPC 四层架构模型")
    
    layers = [
        {
            "name": "第1层：应用层 (Application Layer)",
            "components": [
                "• 用户代码（业务逻辑）",
                "• Client Stub（客户端存根）",
                "• Server Servicer（服务器实现）",
                "• Request/Response Messages"
            ],
            "responsibility": "业务逻辑实现"
        },
        {
            "name": "第2层：gRPC 核心层 (gRPC Core Layer)",
            "components": [
                "• Channel（通道管理）",
                "• Call（调用管理）",
                "• Server（服务器）",
                "• Stub（存根）",
                "• Interceptor（拦截器）",
                "• Context（上下文）",
                "• Protocol Buffer 序列化/反序列化"
            ],
            "responsibility": "RPC 调用管理、序列化、流控制"
        },
        {
            "name": "第3层：HTTP/2 层 (HTTP/2 Transport Layer)",
            "components": [
                "• 多路复用（Multiplexing）",
                "• 流控制（Flow Control）",
                "• 头部压缩（HPACK）",
                "• 二进制帧（Binary Framing）",
                "• 服务器推送（Server Push）",
                "• 优先级控制（Priority）"
            ],
            "responsibility": "网络传输、流控制、多路复用"
        },
        {
            "name": "第4层：TCP/IP 层 (TCP/IP Layer)",
            "components": [
                "• TCP 连接管理",
                "• 可靠传输（重传、顺序）",
                "• IP 路由",
                "• 端到端通信"
            ],
            "responsibility": "底层网络传输、可靠性保证"
        }
    ]
    
    for i, layer in enumerate(layers, 1):
        print(f"\n{'┌' if i == 1 else '├'}{'─' * 68}┐")
        print(f"│ {layer['name']:<66} │")
        print(f"│ 职责：{layer['responsibility']:<59} │")
        print(f"├{'─' * 68}┤")
        for component in layer['components']:
            print(f"│   {component:<64} │")
        print(f"└{'─' * 68}┘" if i == len(layers) else "")
        if i < len(layers):
            print("                              ↕")


def show_data_flow_example():
    """展示实际数据流动示例"""
    
    print_header("实际数据示例：Add(5, 10)")
    
    print("\n[应用层] 创建请求")
    print("  Python 对象: BinaryOperation(a=5, b=10)")
    
    print("\n[gRPC 核心层] 序列化")
    print("  Protocol Buffer 二进制: 0x08 0x05 0x10 0x0a (4 字节)")
    print("  解释: field 1 = 5, field 2 = 10")
    
    print("\n[HTTP/2 层] 封装")
    print("  HEADERS 帧:")
    print("    :method = POST")
    print("    :path = /Calculator/Add")
    print("    :authority = localhost:50051")
    print("    content-type = application/grpc+proto")
    print("    grpc-encoding = identity")
    print("  DATA 帧:")
    print("    Compressed flag = 0")
    print("    Message length = 4")
    print("    Message = 0x08 0x05 0x10 0x0a")
    
    print("\n[TCP/IP 层] 传输")
    print("  源地址: 127.0.0.1:xxxxx")
    print("  目标地址: 127.0.0.1:50051")
    print("  TCP 序列号: 1000")
    print("  TCP 确认号: 2000")


def compare_with_osi():
    """与 OSI 模型对比"""
    
    print_header("gRPC 模型 vs OSI 七层模型")
    
    print("\n OSI 模型              gRPC 模型                   功能")
    print("─" * 70)
    print(" 7. 应用层        →    应用层                     业务逻辑")
    print(" 6. 表示层        →    gRPC 核心层（序列化）      数据序列化")
    print(" 5. 会话层        →    gRPC 核心层（调用管理）    会话管理")
    print(" 4. 传输层        →    HTTP/2 层                  传输控制")
    print(" 3. 网络层        →    TCP/IP                     路由")
    print(" 2. 数据链路层    →    TCP/IP                     MAC 地址")
    print(" 1. 物理层        →    TCP/IP                     物理传输")


def main():
    """主函数"""
    
    print("\n" + "★" * 70)
    print("            gRPC 层次模型可视化演示")
    print("★" * 70)
    
    # 1. 显示架构
    visualize_layer_architecture()
    
    input("\n按 Enter 继续查看客户端请求流程...")
    
    # 2. 客户端请求流程
    visualize_client_request()
    
    input("\n按 Enter 继续查看服务器响应流程...")
    
    # 3. 服务器响应流程
    visualize_server_response()
    
    input("\n按 Enter 继续查看实际数据示例...")
    
    # 4. 实际数据流动
    show_data_flow_example()
    
    input("\n按 Enter 继续查看 OSI 模型对比...")
    
    # 5. OSI 对比
    compare_with_osi()
    
    print("\n" + "=" * 70)
    print("  ✅ 演示完成！")
    print("  📖 详细文档请查看：gRPC层次模型.md")
    print("=" * 70 + "\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⛔ 演示中断")

