#!/usr/bin/env python3
# scripts/parse_binary_aig_correct.py

import struct
import sys
import os

def parse_binary_aig_correct(filename):
    """正确解析二进制 AIG 文件格式"""
    print(f"解析二进制 AIG 文件: {filename}")
    
    with open(filename, 'rb') as f:
        # 读取头部（ASCII 文本）
        header_line = b""
        while True:
            byte = f.read(1)
            if byte == b'\n' or not byte:
                break
            header_line += byte
        
        header_str = header_line.decode('ascii').strip()
        print(f"头部: {header_str}")
        
        # 解析头部
        parts = header_str.split()
        if len(parts) < 6 or parts[0] != 'aig':
            raise ValueError(f"无效的 AIG 头部: {header_str}")
        
        M = int(parts[1])  # 最大变量索引
        I = int(parts[2])  # 输入数量
        L = int(parts[3])  # 锁存器数量
        O = int(parts[4])  # 输出数量
        A = int(parts[5])  # AND 门数量
        
        print(f"  M={M}, I={I}, L={L}, O={O}, A={A}")
        
        # 读取输入（二进制数据）
        inputs = []
        for i in range(I):
            # 输入是 4 字节整数（大端序）
            data = f.read(4)
            if len(data) < 4:
                raise ValueError(f"输入数据不完整，期望 4 字节，得到 {len(data)} 字节")
            lit = struct.unpack('>I', data)[0]
            inputs.append(lit)
        
        # 读取锁存器（二进制数据）
        latches = []
        for i in range(L):
            # 每个锁存器是 8 字节：4 字节当前状态 + 4 字节下一状态
            data = f.read(8)
            if len(data) < 8:
                raise ValueError(f"锁存器数据不完整，期望 8 字节，得到 {len(data)} 字节")
            lit = struct.unpack('>I', data[0:4])[0]
            next_lit = struct.unpack('>I', data[4:8])[0]
            latches.append((lit, next_lit))
        
        # 读取输出（二进制数据）
        outputs = []
        for i in range(O):
            data = f.read(4)
            if len(data) < 4:
                raise ValueError(f"输出数据不完整，期望 4 字节，得到 {len(data)} 字节")
            lit = struct.unpack('>I', data)[0]
            outputs.append(lit)
        
        # 读取 AND 门（二进制数据）
        ands = []
        for i in range(A):
            # 每个 AND 门是 12 字节：4 字节 lhs + 4 字节 rhs0 + 4 字节 rhs1
            data = f.read(12)
            if len(data) < 12:
                raise ValueError(f"AND 门数据不完整，期望 12 字节，得到 {len(data)} 字节")
            lhs = struct.unpack('>I', data[0:4])[0]
            rhs0 = struct.unpack('>I', data[4:8])[0]
            rhs1 = struct.unpack('>I', data[8:12])[0]
            ands.append((lhs, rhs0, rhs1))
        
        # 读取注释（如果有）
        comments = []
        remaining = f.read()
        if remaining:
            try:
                # 尝试解码注释
                comment_text = remaining.decode('ascii', errors='ignore')
                if comment_text.strip():
                    comments = [line.strip() for line in comment_text.split('\n') if line.strip()]
            except:
                # 如果解码失败，跳过注释
                pass
        
        return {
            'header': {'M': M, 'I': I, 'L': L, 'O': O, 'A': A},
            'inputs': inputs,
            'latches': latches,
            'outputs': outputs,
            'ands': ands,
            'comments': comments
        }

def convert_to_aag(data, output_filename):
    """转换为 ASCII AAG 格式"""
    print(f"创建 AAG 文件: {output_filename}")
    
    with open(output_filename, 'w') as f:
        # 写入头部
        h = data['header']
        f.write(f"aag {h['M']} {h['I']} {h['L']} {h['O']} {h['A']}\n")
        
        # 写入输入
        for inp in data['inputs']:
            f.write(f"{inp}\n")
        
        # 写入锁存器
        for latch, next_lit in data['latches']:
            f.write(f"{latch} {next_lit}\n")
        
        # 写入输出
        for out in data['outputs']:
            f.write(f"{out}\n")
        
        # 写入 AND 门
        for lhs, rhs0, rhs1 in data['ands']:
            f.write(f"{lhs} {rhs0} {rhs1}\n")
        
        # 写入注释
        if data['comments']:
            f.write("c\n")
            for comment in data['comments']:
                f.write(f"{comment}\n")
        else:
            f.write("c\n")
            f.write("Converted from binary AIG format\n")

def main():
    if len(sys.argv) < 2:
        print("用法: python3 parse_binary_aig_correct.py <二进制aig文件> [输出aag文件]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else f"{os.path.splitext(input_file)[0]}.aag"
    
    if not os.path.exists(input_file):
        print(f"错误: 文件不存在: {input_file}")
        sys.exit(1)
    
    try:
        # 解析二进制 AIG 文件
        data = parse_binary_aig_correct(input_file)
        
        # 转换为 AAG 格式
        convert_to_aag(data, output_file)
        
        print(f"\n✅ 转换成功: {output_file}")
        
        # 显示摘要
        h = data['header']
        print(f"\n=== 转换摘要 ===")
        print(f"最大变量索引: {h['M']}")
        print(f"输入: {len(data['inputs'])}")
        print(f"锁存器: {len(data['latches'])}")
        print(f"输出: {len(data['outputs'])}")
        print(f"AND 门: {len(data['ands'])}")
        
        if data['inputs']:
            print(f"输入示例: {data['inputs'][:3]}")
        if data['outputs']:
            print(f"输出示例: {data['outputs'][:3]}")
        if data['ands']:
            print(f"AND 门示例: {data['ands'][:2]}")
        
        # 显示文件预览
        print(f"\n输出文件预览:")
        with open(output_file, 'r') as f:
            for i, line in enumerate(f):
                if i < 10:
                    print(f"  {line.rstrip()}")
                else:
                    print("  ...")
                    break
                    
    except Exception as e:
        print(f"❌ 转换失败: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()