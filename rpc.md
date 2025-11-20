> 远程过程调用（RPC - Remote Procedure Call）是一种分布式计算的通信协议，它允许程序调用另一个地址空间（通常在另一台机器上）的函数或方法，就像调用本地函数一样简单

- RPC 让你可以像调用本地函数一样调用远程服务器上的函数
```
# 本地调用
result = calculate(5, 10)

# RPC 调用（看起来一样，但实际在远程服务器执行）
result = remote_service.calculate(5, 10)  # 实际在另一台机器上运行
```

- 工作原理
```
客户端                      网络                        服务器
   |                         |                            |
   | 1. 调用远程函数          |                            |
   |------------------------>|                            |
   |                         | 2. 发送请求                 |
   |                         |--------------------------->|
   |                         |                            | 3. 执行函数
   |                         |                            |
   |                         | 4. 返回结果                 |
   |                         |<---------------------------|
   | 5. 接收结果              |                            |
   |<------------------------|                            |
```

- example code
  assets/rpc/README_RPC.md
  assets/rpc/rpc_client.py
  assets/rpc/rpc_demo.py
  assets/rpc/rpc_server.py
  assets/rpc/RPC使用指南.md

  - gRPC example
      README.md
      RPC技术对比.md
      calculator.proto
      gRPC使用指南.md
      grpc_client.py
      grpc_server.py
      grpc_setup.sh