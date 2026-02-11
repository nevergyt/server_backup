#!/usr/bin/env python3
# scripts/analyze_aig_fixed.py

import sys
import os

class AIGParserFixed:
    def __init__(self):
        self.header = {}
        self.inputs = []
        self.outputs = []
        self.ands = []
        self.comments = []
    
    def safe_int_convert(self, value):
        """安全地转换为整数"""
        try:
            return int(value)
        except (ValueError, TypeError):
            print(f"警告: 无法转换 '{value}' 为整数，跳过")
            return None
    
    def parse_header(self, line):
        """解析 AIG 文件头"""
        parts = line.strip().split()
        if len(parts) >= 6 and parts[0] in ['aag', 'aig']:
            self.header = {
                'magic': parts[0],
                'max_var_index': self.safe_int_convert(parts[1]),
                'num_inputs': self.safe_int_convert(parts[2]),
                'num_latches': self.safe_int_convert(parts[3]),
                'num_outputs': self.safe_int_convert(parts[4]),
                'num_ands': self.safe_int_convert(parts[5])
            }
        return self.header
    
    def parse_aig_file(self, filename):
        """解析 AIG 文件，处理二进制和 ASCII"""
        # 首先检测文件格式
        with open(filename, 'rb') as f:
            first_byte = f.read(1)
        
        if first_byte == b'\x31':
            print("检测到二进制 AIG 文件，请使用 binary_aig_to_ascii.py 先转换")
            return False
        
        # 处理 ASCII AIG
        with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
        
        if not lines:
            print("错误: 空文件")
            return False
        
        # 解析头部
        header_line = lines[0]
        if not self.parse_header(header_line):
            print("错误: 无效的 AIG 文件头")
            return False
        
        # 解析输入、输出、门
        input_start = 1
        output_start = input_start + self.header['num_inputs']
        and_start = output_start + self.header['num_outputs']
        
        # 解析输入
        for i in range(input_start, output_start):
            if i < len(lines):
                value = self.safe_int_convert(lines[i].strip())
                if value is not None:
                    self.inputs.append(value)
        
        # 解析输出
        for i in range(output_start, and_start):
            if i < len(lines):
                value = self.safe_int_convert(lines[i].strip())
                if value is not None:
                    self.outputs.append(value)
        
        # 解析 AND 门
        for i in range(and_start, and_start + self.header['num_ands']):
            if i < len(lines):
                parts = lines[i].strip().split()
                if len(parts) >= 3:
                    lhs = self.safe_int_convert(parts[0])
                    rhs0 = self.safe_int_convert(parts[1])
                    rhs1 = self.safe_int_convert(parts[2])
                    
                    if lhs is not None and rhs0 is not None and rhs1 is not None:
                        self.ands.append({
                            'lhs': lhs,
                            'rhs0': rhs0,
                            'rhs1': rhs1
                        })
        
        # 解析注释
        comment_start = and_start + self.header['num_ands']
        if comment_start < len(lines):
            self.comments = lines[comment_start:]
        
        return True
    
    def print_summary(self):
        """打印 AIG 文件摘要"""
        print("=== AIG 文件分析 ===")
        for key, value in self.header.items():
            print(f"{key}: {value}")
        
        print(f"\n输入数量: {len(self.inputs)}")
        print(f"输出数量: {len(self.outputs)}")
        print(f"AND 门数量: {len(self.ands)}")
        
        if self.inputs:
            print(f"\n输入变量 (前10个): {self.inputs[:10]}")
        if self.outputs:
            print(f"输出变量 (前10个): {self.outputs[:10]}")
        if self.ands:
            print(f"\n前5个AND门:")
            for i, and_gate in enumerate(self.ands[:5]):
                print(f"  {i+1}: {and_gate['lhs']} = {and_gate['rhs0']} & {and_gate['rhs1']}")

def main():
    if len(sys.argv) < 2:
        print("用法: python3 analyze_aig_fixed.py <aig文件>")
        sys.exit(1)
    
    aig_file = sys.argv[1]
    if not os.path.exists(aig_file):
        print(f"错误: 文件不存在: {aig_file}")
        sys.exit(1)
    
    parser = AIGParserFixed()
    if parser.parse_aig_file(aig_file):
        parser.print_summary()
    else:
        print("解析失败")

if __name__ == '__main__':
    main()