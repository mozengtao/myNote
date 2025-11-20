#!/usr/bin/env python3
"""
RPC 客户端 - 调用远程服务器上的函数
运行方式：python3 rpc_client.py

注意：需要先启动 rpc_server.py
"""

import xmlrpc.client
import sys


def print_section(title):
    """打印分隔标题"""
    print(f"\n{'=' * 60}")
    print(f"📌 {title}")
    print('=' * 60)


def main():
    # 连接到 RPC 服务器
    server_url = "http://localhost:8000"
    
    print("=" * 60)
    print("🔌 正在连接到 RPC 服务器...")
    print(f"   地址: {server_url}")
    print("=" * 60)
    
    try:
        # 创建服务器代理对象
        proxy = xmlrpc.client.ServerProxy(server_url, allow_none=True)
        
        # 测试连接
        print("✅ 连接成功！\n")
        
        # ==================== 1. 计算器服务 ====================
        print_section("1. 计算器服务测试")
        
        result1 = proxy.add(15, 27)
        print(f"远程调用: add(15, 27) = {result1}")
        
        result2 = proxy.subtract(100, 35)
        print(f"远程调用: subtract(100, 35) = {result2}")
        
        result3 = proxy.multiply(8, 9)
        print(f"远程调用: multiply(8, 9) = {result3}")
        
        result4 = proxy.divide(144, 12)
        print(f"远程调用: divide(144, 12) = {result4}")
        
        # 测试错误处理
        print("\n测试错误处理：")
        try:
            proxy.divide(10, 0)
        except xmlrpc.client.Fault as fault:
            print(f"❌ 捕获到远程异常: {fault.faultString}")
        
        # ==================== 2. 数据服务 ====================
        print_section("2. 数据服务测试")
        
        # 获取单个用户
        user = proxy.get_user(1)
        print(f"获取用户 (ID=1): {user}")
        
        # 获取所有用户
        all_users = proxy.list_users()
        print(f"\n所有用户列表:")
        for user in all_users:
            print(f"  - {user['name']} (年龄: {user['age']}, 邮箱: {user['email']})")
        
        # 添加新用户
        print("\n添加新用户:")
        result = proxy.add_user(4, "赵六", 32, "zhaoliu@example.com")
        print(f"  {result}")
        
        # 搜索用户
        print("\n搜索用户 (关键字: '张'):")
        search_results = proxy.search_users("张")
        for user in search_results:
            print(f"  - 找到: {user['name']}")
        
        # ==================== 3. 工具函数 ====================
        print_section("3. 工具函数测试")
        
        # 获取服务器时间
        server_time = proxy.get_server_time()
        print(f"服务器时间: {server_time}")
        
        # 回声测试
        echo_result = proxy.echo("你好，RPC！")
        print(f"回声测试: {echo_result}")
        
        # 处理列表
        numbers = [10, 20, 30, 40, 50]
        stats = proxy.process_list(numbers)
        print(f"\n列表处理 {numbers}:")
        print(f"  数量: {stats['count']}")
        print(f"  总和: {stats['sum']}")
        print(f"  平均: {stats['average']:.2f}")
        print(f"  最小: {stats['min']}")
        print(f"  最大: {stats['max']}")
        
        # ==================== 4. 耗时操作 ====================
        print_section("4. 耗时操作测试")
        
        print("调用远程耗时操作 (2秒)...")
        print("⏳ 等待中...")
        result = proxy.slow_operation(2)
        print(f"✅ {result}")
        
        # ==================== 5. 查看可用方法 ====================
        print_section("5. 服务器信息")
        
        # 列出所有可用方法
        methods = proxy.system.listMethods()
        print(f"服务器提供的所有方法 (共 {len(methods)} 个):")
        for i, method in enumerate(methods, 1):
            if not method.startswith('system.'):
                print(f"  {i}. {method}")
        
        # 获取方法帮助信息
        print(f"\n查看 'add' 方法的帮助:")
        try:
            help_text = proxy.system.methodHelp('add')
            print(f"  {help_text if help_text else '加法'}")
        except:
            print("  (无帮助信息)")
        
        # ==================== 完成 ====================
        print_section("测试完成")
        print("✅ 所有 RPC 调用成功完成！")
        print("💡 提示：这些函数实际上都在远程服务器上执行")
        print("=" * 60)
        
    except ConnectionRefusedError:
        print("\n❌ 错误：无法连接到服务器")
        print("请确保 rpc_server.py 正在运行")
        print("启动命令：python3 rpc_server.py")
        sys.exit(1)
    
    except Exception as e:
        print(f"\n❌ 发生错误: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()

