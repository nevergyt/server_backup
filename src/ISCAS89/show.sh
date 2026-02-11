#!/bin/bash
# scripts/abc_visualize_safe.sh

set -e

INPUT_FILE="$1"
OUTPUT_NAME="${2:-${INPUT_FILE%.*}}"

if [ -z "$INPUT_FILE" ] || [ ! -f "$INPUT_FILE" ]; then
    echo "错误: 文件不存在: $INPUT_FILE"
    exit 1
fi

echo "安全模式可视化: $INPUT_FILE"

TEMP_DIR=$(mktemp -d)

# 方法1: 尝试直接读取并生成 DOT
echo "方法1: 直接读取 Verilog..."
abc -c "read_verilog $INPUT_FILE; write_dot $TEMP_DIR/method1.dot" 2>/dev/null

if [ -f "$TEMP_DIR/method1.dot" ] && [ -s "$TEMP_DIR/method1.dot" ]; then
    echo "✓ 方法1 成功"
    cp "$TEMP_DIR/method1.dot" "${OUTPUT_NAME}.dot"
else
    echo "方法1 失败，尝试方法2..."
    
    # 方法2: 使用 strash 前先进行简单处理
    abc -c "read_verilog $INPUT_FILE; sweep; write_dot $TEMP_DIR/method2.dot" 2>/dev/null
    
    if [ -f "$TEMP_DIR/method2.dot" ] && [ -s "$TEMP_DIR/method2.dot" ]; then
        echo "✓ 方法2 成功"
        cp "$TEMP_DIR/method2.dot" "${OUTPUT_NAME}.dot"
    else
        echo "方法2 失败，尝试方法3..."
        
        # 方法3: 跳过所有优化
        abc -c "read_verilog $INPUT_FILE; write_dot $TEMP_DIR/method3.dot" 2>/dev/null
        
        if [ -f "$TEMP_DIR/method3.dot" ] && [ -s "$TEMP_DIR/method3.dot" ]; then
            echo "✓ 方法3 成功"
            cp "$TEMP_DIR/method3.dot" "${OUTPUT_NAME}.dot