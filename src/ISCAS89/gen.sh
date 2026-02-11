#!/bin/bash

# 为 AIG 格式代码生成电路图
yosys -p "
read_aiger s27.aig;
proc -norom;        
memory_collect;
flatten;
techmap;
simplemap;
opt_clean;
write_verilog -noattr output.v;"