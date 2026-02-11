`timescale 1ns/1ps

module tb_top();
    // 时钟信号
    reg clock;
    
    // 36个输入信号
    reg input_0;
    reg input_1;
    reg input_2;
    reg input_3;
    reg input_4;
    reg input_5;
    reg input_6;
    reg input_7;
    reg input_8;
    reg input_9;
    reg input_10;
    reg input_11;
    reg input_12;
    reg input_13;
    reg input_14;
    reg input_15;
    reg input_16;
    reg input_17;
    reg input_18;
    reg input_19;
    reg input_20;
    reg input_21;
    reg input_22;
    reg input_23;
    reg input_24;
    reg input_25;
    reg input_26;
    reg input_27;
    reg input_28;
    reg input_29;
    reg input_30;
    reg input_31;
    reg input_32;
    reg input_33;
    reg input_34;
    reg input_35;
    
    // 7个输出信号
    wire po0;
    wire po1;
    wire po2;
    wire po3;
    wire po4;
    wire po5;
    wire po6;
    
    // 实例化待测试模块
    top uut (
        .input_0(input_0),
        .input_1(input_1),
        .input_2(input_2),
        .input_3(input_3),
        .input_4(input_4),
        .input_5(input_5),
        .input_6(input_6),
        .input_7(input_7),
        .input_8(input_8),
        .input_9(input_9),
        .input_10(input_10),
        .input_11(input_11),
        .input_12(input_12),
        .input_13(input_13),
        .input_14(input_14),
        .input_15(input_15),
        .input_16(input_16),
        .input_17(input_17),
        .input_18(input_18),
        .input_19(input_19),
        .input_20(input_20),
        .input_21(input_21),
        .input_22(input_22),
        .input_23(input_23),
        .input_24(input_24),
        .input_25(input_25),
        .input_26(input_26),
        .input_27(input_27),
        .input_28(input_28),
        .input_29(input_29),
        .input_30(input_30),
        .input_31(input_31),
        .input_32(input_32),
        .input_33(input_33),
        .input_34(input_34),
        .input_35(input_35),
        .po0(po0),
        .po1(po1),
        .po2(po2),
        .po3(po3),
        .po4(po4),
        .po5(po5),
        .po6(po6),
        .clock(clock)
    );
    
    // 时钟生成
    initial begin
        clock = 0;
        forever #10 clock = ~clock; // 10ns时钟周期
    end
    
    // 初始化所有输入
    initial begin
        input_0 = 0;
        input_1 = 0;
        input_2 = 0;
        input_3 = 0;
        input_4 = 0;
        input_5 = 0;
        input_6 = 0;
        input_7 = 0;
        input_8 = 0;
        input_9 = 0;
        input_10 = 0;
        input_11 = 0;
        input_12 = 0;
        input_13 = 0;
        input_14 = 0;
        input_15 = 0;
        input_16 = 0;
        input_17 = 0;
        input_18 = 0;
        input_19 = 0;
        input_20 = 0;
        input_21 = 0;
        input_22 = 0;
        input_23 = 0;
        input_24 = 0;
        input_25 = 0;
        input_26 = 0;
        input_27 = 0;
        input_28 = 0;
        input_29 = 0;
        input_30 = 0;
        input_31 = 0;
        input_32 = 0;
        input_33 = 0;
        input_34 = 0;
        input_35 = 0;
        
        // 监视输出变化
        $monitor("Time = %0t: po0 = %b, po1 = %b, po2 = %b, po3 = %b, po4 = %b, po5 = %b, po6 = %b", 
                 $time, po0, po1, po2, po3, po4, po5, po6);
        
        // 运行一段时间后停止
        #100;
        $display("Testbench completed");
        $finish;
    end
    
    // 生成波形文件（可选，用于VCS或其他仿真器）
    initial begin
        $dumpfile("./sim_results/c432.vcd");
        $dumpvars(0, tb_top);
    end
    
endmodule