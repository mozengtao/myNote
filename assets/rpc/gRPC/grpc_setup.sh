#!/bin/bash
# gRPC 环境设置和代码生成脚本

echo "======================================================================"
echo "🔧 gRPC Python 环境设置"
echo "======================================================================"

# 检查 Python 版本
echo ""
echo "📍 步骤 1: 检查 Python 版本"
python3 --version

# 安装依赖
echo ""
echo "📍 步骤 2: 安装 gRPC 和 Protocol Buffers"
echo "   正在安装 grpcio 和 grpcio-tools..."
pip3 install grpcio grpcio-tools

# 生成 Python 代码
echo ""
echo "📍 步骤 3: 从 .proto 文件生成 Python 代码"
echo "   正在编译 calculator.proto..."

python3 -m grpc_tools.protoc \
    -I. \
    --python_out=. \
    --grpc_python_out=. \
    calculator.proto

# 检查生成的文件
if [ -f "calculator_pb2.py" ] && [ -f "calculator_pb2_grpc.py" ]; then
    echo ""
    echo "✅ 代码生成成功！"
    echo ""
    echo "生成的文件："
    echo "  ✓ calculator_pb2.py        (Protocol Buffer 消息类)"
    echo "  ✓ calculator_pb2_grpc.py   (gRPC 服务代码)"
else
    echo ""
    echo "❌ 代码生成失败"
    exit 1
fi

echo ""
echo "======================================================================"
echo "✅ 设置完成！"
echo "======================================================================"
echo ""
echo "现在可以运行："
echo ""
echo "  终端 1 (启动服务器):"
echo "    python3 grpc_server.py"
echo ""
echo "  终端 2 (运行客户端):"
echo "    python3 grpc_client.py"
echo ""
echo "======================================================================"

