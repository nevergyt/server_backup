module top_tb;

    // 输入信号
    reg input_0;
    reg input_1;
    reg input_2;
    reg input_3;
    reg input_4;
    reg clock;
    
    // 输出信号
    wire po0;
    wire po1;
    
    // 测试向量计数器
    integer test_num;
    
    // 实例化待测模块
    top uut (
        .input_0(input_0),
        .input_1(input_1),
        .input_2(input_2),
        .input_3(input_3),
        .input_4(input_4),
        .po0(po0),
        .po1(po1),
        .clock(clock)
    );

    initial begin
        clock = 0;
        forever #10 clock = ~clock; // 周期20ns（50MHz）
    end
    
    // 测试序列
    initial begin
        // 初始化信号
        input_0 = 0;
        input_1 = 0;
        input_2 = 0;
        input_3 = 0;
        input_4 = 0;
        test_num = 0;
        
        // 打印表头
        $display("========== Testbench for top module ==========");
        $display("Time\tTest\tin0\tin1\tin2\tin3\tin4\tpo0\tpo1");
        $display("-------------------------------------------------------");
        
        // 等待全局复位
        #10;
        
        // 测试1: 所有输入为0
        test_num = 1;
        {input_0, input_1, input_2, input_3, input_4} = 5'b00000;
        #10;
        $display("%0t\t%0d\t%b\t%b\t%b\t%b\t%b\t%b\t%b", 
                 $time, test_num, input_0, input_1, input_2, input_3, input_4, po0, po1);
        
        
        #200;

        // 测试完成
        $display("\n========== Test Complete ==========");
        $finish;
    end
    
    // 波形记录（可选，用于VCS/Verdi等仿真器）
    initial begin
        $dumpfile("./sim_results/c17.vcd");
        $dumpvars(0, top_tb);
    end
    
endmodule