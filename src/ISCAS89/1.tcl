# 读取 AIG 文件
read_aiger s27.aig

# 处理和展平
proc
flatten

# 映射到基本门
techmap
abc -g AND,NOT

# 清理
opt_clean

# 写出 Verilog
write_verilog -noattr output.v