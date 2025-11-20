#!/usr/bin/env python3
"""
gRPC 服务器示例
使用 Protocol Buffers 定义的接口提供计算服务
"""

import grpc
from concurrent import futures
import time
import math

# 导入生成的 gRPC 代码
import calculator_pb2
import calculator_pb2_grpc


class CalculatorServicer(calculator_pb2_grpc.CalculatorServicer):
    """实现 Calculator 服务"""
    
    def Add(self, request, context):
        """加法"""
        result = request.a + request.b
        print(f"[服务器] Add({request.a}, {request.b}) = {result}")
        return calculator_pb2.Result(
            value=result,
            message=f"{request.a} + {request.b} = {result}"
        )
    
    def Subtract(self, request, context):
        """减法"""
        result = request.a - request.b
        print(f"[服务器] Subtract({request.a}, {request.b}) = {result}")
        return calculator_pb2.Result(
            value=result,
            message=f"{request.a} - {request.b} = {result}"
        )
    
    def Multiply(self, request, context):
        """乘法"""
        result = request.a * request.b
        print(f"[服务器] Multiply({request.a}, {request.b}) = {result}")
        return calculator_pb2.Result(
            value=result,
            message=f"{request.a} x {request.b} = {result}"
        )
    
    def Divide(self, request, context):
        """除法（带错误处理）"""
        print(f"[服务器] Divide({request.a}, {request.b})")
        
        if request.b == 0:
            context.set_code(grpc.StatusCode.INVALID_ARGUMENT)
            context.set_details("除数不能为零！")
            return calculator_pb2.Result()
        
        result = request.a / request.b
        return calculator_pb2.Result(
            value=result,
            message=f"{request.a} / {request.b} = {result}"
        )
    
    def SquareRoot(self, request, context):
        """平方根"""
        print(f"[服务器] SquareRoot({request.value})")
        
        if request.value < 0:
            context.set_code(grpc.StatusCode.INVALID_ARGUMENT)
            context.set_details("不能计算负数的平方根！")
            return calculator_pb2.Result()
        
        result = math.sqrt(request.value)
        return calculator_pb2.Result(
            value=result,
            message=f"√{request.value} = {result}"
        )
    
    def GetSquares(self, request, context):
        """服务器流式响应：返回从 1 到 n 的平方"""
        n = int(request.value)
        print(f"[服务器] GetSquares(1 到 {n})")
        
        for i in range(1, n + 1):
            result = i * i
            yield calculator_pb2.Result(
                value=result,
                message=f"{i}² = {result}"
            )
            time.sleep(0.2)  # 模拟流式传输


def serve():
    """启动 gRPC 服务器"""
    # 创建服务器
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    
    # 添加服务
    calculator_pb2_grpc.add_CalculatorServicer_to_server(
        CalculatorServicer(), server
    )
    
    # 监听端口
    port = '50051'
    server.add_insecure_port(f'[::]:{port}')
    
    # 启动服务器
    server.start()
    
    print("=" * 70)
    print("🚀 gRPC 服务器启动成功！")
    print(f"📡 监听端口: {port}")
    print("⏳ 等待客户端连接...")
    print("   按 Ctrl+C 停止服务器")
    print("=" * 70)
    print()
    
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("\n⛔ 服务器已停止")
        server.stop(0)


if __name__ == '__main__':
    serve()

