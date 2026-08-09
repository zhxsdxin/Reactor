#!/bin/bash
# Reactor Echo 服务 10 客户端并发演示
# 用法: bash demo.sh [port]

PORT=${1:-8080}
SERVER="./server"

echo "=========================================="
echo " Reactor Echo Server - 10 客户端并发演示"
echo "=========================================="

# -------------------- ① 编译 --------------------
echo ""
echo "[1/4] 编译..."
g++ -std=c++17 -o server src/*.cpp -Iinclude -lpthread -pthread
if [ $? -ne 0 ]; then
    echo "编译失败!"
    exit 1
fi
echo "编译成功"

# -------------------- ② 启动服务器 --------------------
echo ""
echo "[2/4] 启动服务器 (端口 $PORT)..."
$SERVER $PORT &
SERVER_PID=$!
sleep 1                      # 等服务器就绪

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "服务器启动失败!"
    exit 1
fi
echo "服务器 PID=$SERVER_PID, 就绪"

# -------------------- ③ 10 个客户端并发 --------------------
echo ""
echo "[3/4] 启动 10 个客户端并发测试..."
echo ""

PASS=0
FAIL=0

for i in $(seq 1 10); do
    (
        # 每个客户端发一条消息, 期望收到一样的回显
        SEND="client-$i: hello reactor!"
        RECV=$(echo "$SEND" | nc -w2 localhost $PORT)
        if [ "$RECV" = "$SEND" ]; then
            echo "  [✓] 客户端 #$i: 发送=接收 → 正确"
        else
            echo "  [✗] 客户端 #$i: 发送='$SEND' 接收='$RECV'"
        fi
    ) &
done

# 等待所有客户端完成
wait

# 统计结果
PASS=$(jobs -l | wc -l)  # 粗略统计
echo ""
echo "10 个客户端全部执行完毕"

# -------------------- ④ 停止服务器 --------------------
echo ""
echo "[4/4] 停止服务器..."
sleep 1
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
echo "服务器已停止"
echo ""
echo "=========================================="
echo " 演示结束"
echo "=========================================="
