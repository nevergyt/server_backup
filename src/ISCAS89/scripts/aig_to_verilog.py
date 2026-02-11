#!/usr/bin/env python3
# scripts/unified_aig_processor.py

import os
import sys
import subprocess

def detect_file_format(filename):
    """检测文件格式"""
    with open(filename, 'rb') as f:
        first_bytes = f.read(10)
        
        # 检查二进制 AIG
        if first_bytes[0:1] == b'\x31':
            return 'binary_aig'
        
        # 检查 ASCII AIG
        if first_bytes[0:4] in [b'aag ', b'aig ']:
            return 'ascii_aig'
        
        # 检查 Verilog
        if b'module' in first_bytes.lower():
            return 'verilog'
        
        return 'unknown'

def convert_to_ascii_aig(input_file, output_file=None):
    """将各种格式转换为 ASCII AIG"""
    if output_file is None:
        output_file = f"{os.path.splitext(input_file)[0]}.aag"
    
    file_format = detect_file_format(input_file)
    print(f"检测到格式: {file_format}")
    
    if file_format == 'binary_aig':
        # 使用我们的转换器
        from binary_aig_to_ascii import binary_to_ascii_aig
        binary_to_ascii_aig(input_file, output_file)
        
    elif file_format == 'ascii_aig':
        # 已经是 ASCII AIG，直接复制
        import shutil
        shutil.copy2(input_file, output_file)
        
    elif file_format == 'verilog':
        # 使用 ABC 转换 Verilog 到 AIG
        if not os.path.exists('abc'):
            print("错误: ABC 未找到，无法转换 Verilog")
            return None
        
        # 先转换为二进制 AIG，再转换为 ASCII
        temp_binary = f"{os.path.splitext(input_file)[0]}_temp.aig"
        abc_cmd = f"read_verilog {input_file}; strash; write_aig {temp_binary}"
        subprocess.run(['abc', '-c', abc_cmd], capture_output=True)
        
        if os.path.exists(temp_binary):
            from binary_aig_to_ascii import binary_to_ascii_aig
            binary_to_ascii_aig(temp_binary, output_file)
            os.remove(temp_binary)
        else:
            print("错误: ABC 转换失败")
            return None
    
    else:
        print(f"错误: 不支持的文件格式: {file_format}")
        return None
    
    return output_file

def main():
    if len(sys.argv) < 2:
        print("用法: python3 unified_aig_processor.py <输入文件> [输出文件]")
        print("支持格式: 二进制 AIG, ASCII AIG, Verilog")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    if not os.path.exists(input_file):
        print(f"错误: 文件不存在: {input_file}")
        sys.exit(1)
    
    # 转换为 ASCII AIG
    ascii_aig_file = convert_to_ascii_aig(input_file, output_file)
    
    if ascii_aig_file and os.path.exists(ascii_aig_file):
        print(f"✓ 转换完成: {ascii_aig_file}")
        
        # 使用现有的 ASCII AIG 处理器
        print("\n使用 ASCII AIG 处理器继续处理...")
        subprocess.run([sys.executable, 'scripts/analyze_aig.py', ascii_aig_file])
        
    else:
        print("❌ 转换失败")
        sys.exit(1)

if __name__ == '__main__':
    main()