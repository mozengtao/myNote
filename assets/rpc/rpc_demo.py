#!/usr/bin/env python3
"""
RPC 完整演示 - 在一个脚本中同时运行服务器和客户端
这个脚本使用线程来演示 RPC 的工作原理
"""

from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
import xmlrpc.client
import threading
import time
import sys


# ==================== 服务器端代码 ====================

class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/RPC2',)


class Calculator:
    """计算器服务"""
    
    def add(self, x, y):
        print(f"  [服务器] 执行加法: {x} + {y}")
        return x + y
    
    def subtract(self, x, y):
        print(f"  [服务器] 执行减法: {x} - {y}")
        return x - y
    
    def multiply(self, x, y):
        print(f"  [服务器] 执行乘法: {x} x {y}")
        return x * y
    
    def divide(self, x, y):
        print(f"  [服务器] 执行除法: {x} ÷ {y}")
        if y == 0:
            raise ValueError("除数不能为零！")
        return x / y


class UserService:
    """用户服务"""
    
    def __init__(self):
        self.users = {
            1: {"id": 1, "name": "张三", "age": 25},
            2: {"id": 2, "name": "李四", "age": 30},
            3: {"id": 3, "name": "王五", "age": 28},
        }
    
    def get_user(self, user_id):
        print(f"  [服务器] 查询用户: ID={user_id}")
        return self.users.get(user_id, {"error": "用户不存在"})
    
    def list_users(self):
        print(f"  [服务器] 获取所有用户列表")
        return list(self.users.values())


def process_data(numbers):
    """数据处理函数"""
    print(f"  [服务器] 处理数据: {numbers}")
    return {
        "count": len(numbers),
        "sum": sum(numbers),
        "average": sum(numbers) / len(numbers) if numbers else 0,
        "min": min(numbers) if numbers else None,
        "max": max(numbers) if numbers else None
    }


def run_server():
    """运行 RPC 服务器"""
    host = "localhost"
    port = 9000
    
    print(f"\n🚀 [服务器] 启动在 {host}:{port}")
    
    server = SimpleXMLRPCServer(
        (host, port),
        requestHandler=RequestHandler,
        allow_none=True,
        logRequests=False  # 不显示每个请求的日志
    )
    
    # 创建服务实例
    calculator = Calculator()
    user_service = UserService()
    
    # 注册所有方法
    server.register_function(calculator.add, "add")
    server.register_function(calculator.subtract, "subtract")
    server.register_function(calculator.multiply, "multiply")
    server.register_function(calculator.divide, "divide")
    server.register_function(user_service.get_user, "get_user")
    server.register_function(user_service.list_users, "list_users")
    server.register_function(process_data, "process_data")
    
    print("✅ [服务器] 准备就绪，等待请求...\n")
    
    # 运行服务器
    server.serve_forever()


# ==================== 客户端代码 ====================

def run_client():
    """运行 RPC 客户端"""
    # 等待服务器启动
    time.sleep(1)
    
    server_url = "http://localhost:9000"
    
    try:
        print("\n" + "=" * 70)
        print("🔌 [客户端] 连接到 RPC 服务器")
        print("=" * 70)
        
        # 创建服务器代理
        proxy = xmlrpc.client.ServerProxy(server_url, allow_none=True)
        
        # ==================== 测试 1: 计算器服务 ====================
        print("\n📌 测试 1: 计算器服务")
        print("-" * 70)
        
        result = proxy.add(15, 27)
        print(f"[客户端] add(15, 27) = {result}")
        
        result = proxy.subtract(100, 35)
        print(f"[客户端] subtract(100, 35) = {result}")
        
        result = proxy.multiply(8, 9)
        print(f"[客户端] multiply(8, 9) = {result}")
        
        result = proxy.divide(144, 12)
        print(f"[客户端] divide(144, 12) = {result}")
        
        # 测试错误处理
        print("\n💥 测试错误处理:")
        try:
            proxy.divide(10, 0)
        except xmlrpc.client.Fault as fault:
            print(f"[客户端] ❌ 捕获到远程异常: {fault.faultString}")
        
        # ==================== 测试 2: 用户服务 ====================
        print("\n📌 测试 2: 用户服务")
        print("-" * 70)
        
        user = proxy.get_user(1)
        print(f"[客户端] 获取用户(ID=1): {user}")
        
        all_users = proxy.list_users()
        print(f"[客户端] 所有用户:")
        for user in all_users:
            print(f"         - {user['name']} (年龄: {user['age']})")
        
        # ==================== 测试 3: 数据处理 ====================
        print("\n📌 测试 3: 数据处理")
        print("-" * 70)
        
        numbers = [10, 20, 30, 40, 50]
        stats = proxy.process_data(numbers)
        print(f"[客户端] 处理列表 {numbers}:")
        print(f"         数量: {stats['count']}, 总和: {stats['sum']}, 平均: {stats['average']:.2f}")
        print(f"         最小: {stats['min']}, 最大: {stats['max']}")
        
        # ==================== 测试完成 ====================
        print("\n" + "=" * 70)
        print("✅ [客户端] 所有测试完成！")
        print("=" * 70)
        
        print("\n💡 重要概念：")
        print("   - 所有函数都在服务器端执行")
        print("   - 客户端只是发送请求并接收结果")
        print("   - 就像调用本地函数一样简单！")
        print("\n" + "=" * 70)
        
    except Exception as e:
        print(f"\n❌ [客户端] 错误: {e}")
    
    finally:
        # 测试完成后退出程序
        print("\n按 Ctrl+C 退出...")
        time.sleep(2)
        import os
        os._exit(0)


# ==================== 主程序 ====================

def main():
    print("=" * 70)
    print("🎯 RPC (远程过程调用) 完整演示")
    print("=" * 70)
    print("\n本演示将展示：")
    print("  ✓ 如何创建 RPC 服务器")
    print("  ✓ 如何调用远程函数")
    print("  ✓ 如何处理复杂数据类型")
    print("  ✓ 如何处理远程异常")
    
    # 在后台线程运行服务器
    server_thread = threading.Thread(target=run_server, daemon=True)
    server_thread.start()
    
    # 在主线程运行客户端
    try:
        run_client()
    except KeyboardInterrupt:
        print("\n\n⛔ 程序已停止")
        sys.exit(0)


if __name__ == "__main__":
    main()

