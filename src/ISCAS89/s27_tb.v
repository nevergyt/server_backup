// s27_tb.v
`timescale 1ns/1ps

module s27_tb;
    // 输入信号
    reg CK, G0, G1, G2, G3;
    
    // 输出信号
    wire G17;
    
    // 实例化被测模块
    s27 uut (
        .CK(CK),
        .G0(G0),
        .G1(G1),
        .G2(G2), 
        .G3(G3),
        .G17(G17)
    );
    
    // 生成时钟信号
    initial begin
        CK = 0;
        forever #10 CK = ~CK; // 20ns周期时钟
    end
    
    // 测试序列
    initial begin
        // 初始化输入信号
        G0 = 0;
        G1 = 0;
        G2 = 0;
        G3 = 0;
        
        // 生成VCD文件用于波形查看
        $dumpfile("s27.vcd");
        $dumpvars(0, s27_tb);
        
        $display("Starting S27 simulation...");
        $display("Time\tCK\tG0\tG1\tG2\tG3\tG17");
        $display("------------------------------------------------");
        
        // 测试用例1：初始状态
        #15;
        
        // 测试用例2：改变输入组合
        G0 = 1; G1 = 0; G2 = 0; G3 = 0;
        #40;
        
        // 测试用例3：改变输入
        G0 = 0; G1 = 1; G2 = 0; G3 = 0;
        #40;
        
        // 测试用例4：改变输入
        G0 = 0; G1 = 0; G2 = 1; G3 = 0;
        #40;
        
        // 测试用例5：改变输入
        G0 = 0; G1 = 0; G2 = 0; G3 = 1;
        #40;
        
        // 测试用例6：复杂输入组合
        G0 = 1; G1 = 1; G2 = 0; G3 = 1;
        #40;
        
        G0 = 1; G1 = 0; G2 = 1; G3 = 0;
        #40;
        
        // 结束仿真
        $display("\nSimulation completed successfully!");
        $finish;
    end
    
    // 在每个时钟上升沿显示状态
    always @(posedge CK) begin
        $display("%0t\t%b\t%b\t%b\t%b\t%b\t%b", 
                 $time, CK, G0, G1, G2, G3, G17);
    end
    
    // 监控重要信号变化
    initial begin
        $monitor("Time=%0t: CK=%b, Inputs: G0=%b G1=%b G2=%b G3=%b, Output: G17=%b",
                 $time, CK, G0, G1, G2, G3, G17);
    end
    
endmodule