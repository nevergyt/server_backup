#!/usr/bin/env python3
# scripts/analyze_binary_aig_structure.py

import struct
import sys
import os

def analyze_binary_aig_structure(filename):
    """分析二进制 AIG 文件的结构"""
    print(f"分析二进制 AIG 文件结构: {filename}")
    
    file_size = os.path.getsize(filename)
    print(f"文件大小: {file_size} 字节")
    
    with open(filename, 'rb') as f:
        # 读取并显示头部
        header_line = b""
        while True:
            byte = f.read(1)
            if byte == b'\n' or not byte:
                break
            header_line += byte
        
        header_str = header_line.decode('ascii', errors='replace').strip()
        print(f"头部: {header_str}")
        
        # 解析头部
        parts = header_str.split()
        if len(parts) >= 6 and parts[0] == 'aig':
            M, I, L, O, A = map(int, parts[1:6])
            
            print(f"\n=== 文件结构分析 ===")
            print(f"最大变量索引 (M): {M}")
            print(f"输入数量 (I): {I}")
            print(f"锁存器数量 (L): {L}")
            print(f"输出数量 (O): {O}")
            print(f"AND 门数量 (A): {A}")
            
            # 计算预期的数据大小
            header_size = len(header_line) + 1  # +1 用于换行符
            input_size = I * 4
            latch_size = L * 8
            output_size = O * 4
            and_size = A * 12
            total_data_size = input_size + latch_size + output_size + and_size
            total_expected_size = header_size + total_data_size
            
            print(f"\n=== 大小计算 ===")
            print(f"头部大小: {header_size} 字节")
            print(f"输入数据: {I} × 4 = {input_size} 字节")
            print(f"锁存器数据: {L} × 8 = {latch_size} 字节")
            print(f"输出数据: {O} × 4 = {output_size} 字节")
            print(f"AND 门数据: {A} × 12 = {and_size} 字节")
            print(f"总数据大小: {total_data_size} 字节")
            print(f"预期文件大小: {total_expected_size} 字节")
            print(f"实际文件大小: {file_size} 字节")
            
            if file_size > total_expected_size:
                comment_size = file_size - total_expected_size
                print(f"注释大小: {comment_size} 字节")
            elif file_size < total_expected_size:
                missing = total_expected_size - file_size
                print(f"⚠️  文件不完整，缺少 {missing} 字节")
            
            # 显示当前位置
            current_pos = f.tell()
            print(f"\n当前文件位置: {current_pos} 字节")
            
            # 读取并显示一些二进制数据
            if I > 0:
                print(f"\n=== 输入数据示例 ===")
                f.seek(header_size)  # 回到数据开始位置
                for i in range(min(3, I)):
                    data = f.read(4)
                    if len(data) == 4:
                        value = struct.unpack('>I', data)[0]
                        print(f"  输入 {i+1}: {data.hex()} = {value}")
            
            # 如果有锁存器，显示示例
            if L > 0 and f.tell() + 8 <= file_size:
                print(f"\n=== 锁存器数据示例 ===")
                for i in range(min(2, L)):
                    data = f.read(8)
                    if len(data) == 8:
                        lit = struct.unpack('>I', data[0:4])[0]
                        next_lit = struct.unpack('>I', data[4:8])[0]
                        print(f"  锁存器 {i+1}: {data.hex()} = {lit} -> {next_lit}")
            
            # 如果有 AND 门，显示示例
            if A > 0 and f.tell() + 12 <= file_size:
                print(f"\n=== AND 门数据示例 ===")
                for i in range(min(2, A)):
                    data = f.read(12)
                    if len(data) == 12:
                        lhs = struct.unpack('>I', data[0:4])[0]
                        rhs0 = struct.unpack('>I', data[4:8])[0]
                        rhs1 = struct.unpack('>I', data[8:12])[0]
                        print(f"  AND {i+1}: {data.hex()} = {lhs} = {rhs0} & {rhs1}")
        
        else:
            print("错误: 无效的 AIG 文件头部")

def main():
    if len(sys.argv) < 2:
        print("用法: python3 analyze_binary_aig_structure.py <二进制aig文件>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    if not os.path.exists(input_file):
        print(f"错误: 文件不存在: {input_file}")
        sys.exit(1)
    
    analyze_binary_aig_structure(input_file)

if __name__ == '__main__':
    main()