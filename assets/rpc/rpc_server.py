#!/usr/bin/env python3
"""
RPC 服务器 - 提供远程可调用的函数
运行方式：python3 rpc_server.py
"""

from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
import datetime
import time

# 限制可以调用的路径（安全性）
class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/RPC2',)


class Calculator:
    """计算器服务 - 演示面向对象的 RPC"""
    
    def add(self, x, y):
        """加法"""
        print(f"  [服务器] 执行加法: {x} + {y}")
        return x + y
    
    def subtract(self, x, y):
        """减法"""
        print(f"  [服务器] 执行减法: {x} - {y}")
        return x - y
    
    def multiply(self, x, y):
        """乘法"""
        print(f"  [服务器] 执行乘法: {x} × {y}")
        return x * y
    
    def divide(self, x, y):
        """除法（带错误处理）"""
        print(f"  [服务器] 执行除法: {x} ÷ {y}")
        if y == 0:
            raise ValueError("除数不能为零！")
        return x / y


class DataService:
    """数据服务 - 演示复杂数据类型的处理"""
    
    def __init__(self):
        self.users = {
            1: {"id": 1, "name": "张三", "age": 25, "email": "zhangsan@example.com"},
            2: {"id": 2, "name": "李四", "age": 30, "email": "lisi@example.com"},
            3: {"id": 3, "name": "王五", "age": 28, "email": "wangwu@example.com"},
        }
    
    def get_user(self, user_id):
        """获取用户信息"""
        print(f"  [服务器] 查询用户: ID={user_id}")
        return self.users.get(user_id, {"error": "用户不存在"})
    
    def list_users(self):
        """获取所有用户列表"""
        print(f"  [服务器] 获取所有用户列表")
        return list(self.users.values())
    
    def add_user(self, user_id, name, age, email):
        """添加新用户"""
        print(f"  [服务器] 添加新用户: {name}")
        self.users[user_id] = {
            "id": user_id,
            "name": name,
            "age": age,
            "email": email
        }
        return {"success": True, "message": f"用户 {name} 添加成功"}
    
    def search_users(self, keyword):
        """搜索用户（按名字）"""
        print(f"  [服务器] 搜索用户: keyword={keyword}")
        results = [user for user in self.users.values() 
                   if keyword.lower() in user["name"].lower()]
        return results


# 独立的工具函数
def get_server_time():
    """获取服务器时间"""
    now = datetime.datetime.now()
    print(f"  [服务器] 返回当前时间")
    return now.strftime("%Y-%m-%d %H:%M:%S")


def echo(message):
    """回声函数 - 返回接收到的消息"""
    print(f"  [服务器] 收到消息: {message}")
    return f"服务器回声: {message}"


def process_list(numbers):
    """处理列表 - 返回统计信息"""
    print(f"  [服务器] 处理列表: {numbers}")
    if not numbers:
        return {"error": "列表为空"}
    
    return {
        "count": len(numbers),
        "sum": sum(numbers),
        "average": sum(numbers) / len(numbers),
        "min": min(numbers),
        "max": max(numbers)
    }


def slow_operation(seconds):
    """模拟耗时操作"""
    print(f"  [服务器] 执行耗时操作 ({seconds} 秒)...")
    time.sleep(seconds)
    return f"操作完成！耗时 {seconds} 秒"


def main():
    # 创建服务器
    host = "localhost"
    port = 8000
    
    print("=" * 60)
    print("🚀 RPC 服务器启动中...")
    print("=" * 60)
    
    server = SimpleXMLRPCServer(
        (host, port),
        requestHandler=RequestHandler,
        allow_none=True  # 允许 None 值
    )
    server.register_introspection_functions()  # 允许客户端查询可用方法
    
    # 创建服务实例
    calculator = Calculator()
    data_service = DataService()
    
    # 注册计算器方法
    server.register_function(calculator.add, "add")
    server.register_function(calculator.subtract, "subtract")
    server.register_function(calculator.multiply, "multiply")
    server.register_function(calculator.divide, "divide")
    
    # 注册数据服务方法
    server.register_function(data_service.get_user, "get_user")
    server.register_function(data_service.list_users, "list_users")
    server.register_function(data_service.add_user, "add_user")
    server.register_function(data_service.search_users, "search_users")
    
    # 注册独立函数
    server.register_function(get_server_time, "get_server_time")
    server.register_function(echo, "echo")
    server.register_function(process_list, "process_list")
    server.register_function(slow_operation, "slow_operation")
    
    print(f"✅ 服务器运行在: http://{host}:{port}")
    print(f"📡 等待客户端连接...\n")
    print("可用的服务：")
    print("  - Calculator: add, subtract, multiply, divide")
    print("  - DataService: get_user, list_users, add_user, search_users")
    print("  - Utils: get_server_time, echo, process_list, slow_operation")
    print("\n按 Ctrl+C 停止服务器\n")
    print("=" * 60)
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n\n⛔ 服务器已停止")


if __name__ == "__main__":
    main()

