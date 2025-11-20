#!/usr/bin/env python3
"""
gRPC 客户端示例
调用 gRPC 服务器提供的计算服务
"""

import grpc

# 导入生成的 gRPC 代码
import calculator_pb2
import calculator_pb2_grpc


def print_section(title):
    """打印分隔标题"""
    print(f"\n{'=' * 70}")
    print(f"📌 {title}")
    print('=' * 70)


def run_client():
    """运行 gRPC 客户端"""
    
    # 创建连接通道
    with grpc.insecure_channel('localhost:50051') as channel:
        # 创建存根（stub）
        stub = calculator_pb2_grpc.CalculatorStub(channel)
        
        print("=" * 70)
        print("🔌 gRPC 客户端")
        print("   连接到: localhost:50051")
        print("=" * 70)
        
        # ==================== 测试 1: 基本算术运算 ====================
        print_section("1. 基本算术运算")
        
        # 加法
        response = stub.Add(calculator_pb2.BinaryOperation(a=15, b=27))
        print(f"[客户端] Add(15, 27)")
        print(f"         结果: {response.value}")
        print(f"         消息: {response.message}")
        
        # 减法
        response = stub.Subtract(calculator_pb2.BinaryOperation(a=100, b=35))
        print(f"\n[客户端] Subtract(100, 35)")
        print(f"         结果: {response.value}")
        print(f"         消息: {response.message}")
        
        # 乘法
        response = stub.Multiply(calculator_pb2.BinaryOperation(a=8, b=9))
        print(f"\n[客户端] Multiply(8, 9)")
        print(f"         结果: {response.value}")
        print(f"         消息: {response.message}")
        
        # 除法
        response = stub.Divide(calculator_pb2.BinaryOperation(a=144, b=12))
        print(f"\n[客户端] Divide(144, 12)")
        print(f"         结果: {response.value}")
        print(f"         消息: {response.message}")
        
        # ==================== 测试 2: 平方根 ====================
        print_section("2. 平方根运算")
        
        response = stub.SquareRoot(calculator_pb2.Number(value=16))
        print(f"[客户端] SquareRoot(16)")
        print(f"         结果: {response.value}")
        print(f"         消息: {response.message}")
        
        response = stub.SquareRoot(calculator_pb2.Number(value=2))
        print(f"\n[客户端] SquareRoot(2)")
        print(f"         结果: {response.value}")
        print(f"         消息: {response.message}")
        
        # ==================== 测试 3: 错误处理 ====================
        print_section("3. 错误处理")
        
        # 除以零
        print("[客户端] 测试除以零:")
        try:
            response = stub.Divide(calculator_pb2.BinaryOperation(a=10, b=0))
        except grpc.RpcError as e:
            print(f"         ❌ 捕获错误: {e.details()}")
            print(f"         状态码: {e.code()}")
        
        # 负数平方根
        print("\n[客户端] 测试负数平方根:")
        try:
            response = stub.SquareRoot(calculator_pb2.Number(value=-4))
        except grpc.RpcError as e:
            print(f"         ❌ 捕获错误: {e.details()}")
            print(f"         状态码: {e.code()}")
        
        # ==================== 测试 4: 流式响应 ====================
        print_section("4. 流式响应 (服务器流)")
        
        print("[客户端] 请求: GetSquares(1 到 5)")
        print("         接收流式数据:")
        
        # 调用流式 RPC
        responses = stub.GetSquares(calculator_pb2.Number(value=5))
        
        for response in responses:
            print(f"         ← {response.message}")
        
        # ==================== 完成 ====================
        print_section("测试完成")
        print("✅ 所有 gRPC 调用成功完成！")
        print()
        print("💡 gRPC 特点：")
        print("   - 使用 Protocol Buffers 定义接口")
        print("   - 支持多种调用模式（一元、流式）")
        print("   - 高性能、强类型")
        print("   - 跨语言支持")
        print("=" * 70)


if __name__ == '__main__':
    try:
        run_client()
    except grpc.RpcError as e:
        print(f"\n❌ gRPC 错误: {e.details()}")
        print(f"   状态码: {e.code()}")
        print("\n💡 提示: 请确保 gRPC 服务器正在运行")
        print("   启动命令: python3 grpc_server.py")
    except Exception as e:
        print(f"\n❌ 错误: {e}")

