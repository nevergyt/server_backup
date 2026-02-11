import os
import subprocess
import argparse

parser = argparse.ArgumentParser(description='Convert Verilog files to AIG or BLIF')
parser.add_argument('--input', type=str, default='./ISCAS89') 
parser.add_argument('--output', type=str, default='./OAIG2')
parser.add_argument('--backend', type=str, choices=['abc', 'yosys'], default='yosys',
                    help='backend to use for conversion (abc or yosys). yosys will write BLIF by default')
parser.add_argument('--post-abc', action='store_true',
                    help='when using yosys backend, call ABC on the generated BLIF to produce AIG (.aig)')

def detect_top_module(verilog_path):
    # 收集文件中所有 module 名称，优先返回与文件名相同的模块；否则返回最后声明的模块
    modules = []
    try:
        with open(verilog_path, 'r') as f:
            for line in f:
                stripped = line.strip()
                if stripped.startswith('//') or stripped == '':
                    continue
                if stripped.startswith('module'):
                    parts = stripped.split()
                    if len(parts) >= 2:
                        name = parts[1].split('(')[0].strip()
                        modules.append(name)
    except Exception:
        pass

    base = os.path.splitext(os.path.basename(verilog_path))[0]
    # prefer module matching filename
    for m in modules:
        if m == base:
            return m
    # otherwise return last module if any
    if modules:
        return modules[-1]
    return base


def verilog_to_aig_or_blif(verilog_dir, output_dir, backend='yosys', post_abc=False):
    
    os.makedirs(output_dir, exist_ok=True)

    # 遍历文件夹下的所有 Verilog 文件
    for filename in os.listdir(verilog_dir):
        if not filename.endswith('.v'):
            continue

        verilog_path = os.path.join(verilog_dir, filename)
        base = os.path.splitext(filename)[0]

        if backend == 'abc':
            out_name = base + '.aig'
            out_path = os.path.join(output_dir, out_name)
            abc_cmd = f'read_verilog "{verilog_path}"; strash; aigmap; write_aiger "{out_path}"'
            print(f"[INFO] (abc) Processing {filename} -> {out_name}")
            try:
                subprocess.run(["abc", "-c", abc_cmd], check=True)
            except subprocess.CalledProcessError as e:
                print(f"[ERROR] (abc) Failed to process {filename}: {e}")

        else:  # yosys -> write BLIF
            out_name = base + '.blif'
            out_path = os.path.join(output_dir, out_name)
            top = detect_top_module(verilog_path)
            # 构造 yosys -p 命令序列
            yosys_cmd = (
                f'read_verilog {verilog_path}; '
                f'hierarchy -top {top}; '
                'proc; opt; techmap; opt; '
                'flatten; '
                'aigmap; opt; '
                f'write_blif {out_path}'
            )

            print(f"[INFO] (yosys) Processing {filename} -> {out_name} (top={top})")
            try:
                subprocess.run(["yosys", "-p", yosys_cmd], check=True)
            except subprocess.CalledProcessError as e:
                print(f"[ERROR] (yosys) Failed to process {filename}: {e}")

            # 可选：使用 ABC 读取 BLIF 并生成 AIG
            if post_abc:
                # 先修复 BLIF 中的 .subckt $dff（将其转换成标准 .latch），因为 ABC 读取时可能找不到 $dff 模型
                # def fix_blif_dff_subckt(blif_path):
                #     try:
                #         with open(blif_path, 'r') as f:
                #             lines = f.readlines()
                #     except Exception:
                #         return False

                #     out_lines = []
                #     for line in lines:
                #         stripped = line.strip()
                #         if stripped.startswith('.subckt') and '$dff' in stripped:
                #             # 解析 key=val 对，寻找 D=... Q=...
                #             # 例如: .subckt $dff CLK=CK D=DFF_9.D Q=DFF_9.Q
                #             parts = stripped.split()
                #             kv = {}
                #             for p in parts[2:]:
                #                 if '=' in p:
                #                     k, v = p.split('=', 1)
                #                     kv[k] = v
                #             dnet = kv.get('D') or kv.get('d')
                #             qnet = kv.get('Q') or kv.get('q')
                #             # 如果找到了 D 和 Q，则替换为 .latch D Q
                #             if dnet and qnet:
                #                 out_lines.append(f'.latch {dnet} {qnet} re CLK 0\n')
                #             else:
                #                 # 无法解析则保留原行
                #                 out_lines.append(line)
                #         else:
                #             out_lines.append(line)

                #     try:
                #         with open(blif_path, 'w') as f:
                #             f.writelines(out_lines)
                #         return True
                #     except Exception:
                #         return False

                # fixed = fix_blif_dff_subckt(out_path)
                # if not fixed:
                #     print(f"[WARN] Could not fix BLIF {out_name} before calling ABC; proceeding anyway")

                aig_name = base + '.aig'
                aig_path = os.path.join(output_dir, aig_name)
                # Some abc installations may not expose 'aigmap' as a direct command or may rely on aliases in abc.rc.
                # Use a conservative sequence that works in basic abc: read_blif; strash; write_aiger
                abc_cmd = f'read_blif "{out_path}"; strash; write_aiger -u "{aig_path}"'
                print(f"[INFO] (post-abc) Running ABC on {out_name} -> {aig_name}")
                try:
                    subprocess.run(["abc", "-c", abc_cmd], check=True)
                except subprocess.CalledProcessError as e:
                    print(f"[ERROR] (post-abc) ABC failed on {out_name}: {e}")

    if backend == 'abc':
        print("[DONE] All Verilog files converted to AIG (abc).")
    else:
        print("[DONE] All Verilog files converted to BLIF (yosys).")

if __name__ == "__main__":
    # 示例：指定 Verilog 文件夹和输出文件夹
    args = parser.parse_args()
    verilog_folder = args.input
    output_folder = args.output
    backend = args.backend

    args = args
    verilog_to_aig_or_blif(verilog_folder, output_folder, backend=backend, post_abc=args.post_abc)
