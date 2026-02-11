module tb_top;
    reg clock;
    reg [2:0] inputs;  // S382有3个输入
    wire po0, po1, po2, po3, po4, po5;
    
    top uut(
        .clock(clock),
        .input_0(inputs[0]),
        .input_1(inputs[1]),
        .input_2(inputs[2]),
        .po0(po0),
        .po1(po1),
        .po2(po2),
        .po3(po3),
        .po4(po4),
        .po5(po5)
    );
    
    // 生成时钟
    initial begin
        clock = 0;
        forever #5 clock = ~clock;
    end
    
    initial begin
        // 初始化
        inputs = 3'b000;

        // 强制所有寄存器为0（共21个寄存器）
        force uut.rout_0 = 1'b0;
        force uut.rout_1 = 1'b0;
        force uut.rout_2 = 1'b0;
        force uut.rout_3 = 1'b0;
        force uut.rout_4 = 1'b0;
        force uut.rout_5 = 1'b0;
        force uut.rout_6 = 1'b0;
        force uut.rout_7 = 1'b0;
        force uut.rout_8 = 1'b0;
        force uut.rout_9 = 1'b0;
        force uut.rout_10 = 1'b0;
        force uut.rout_11 = 1'b0;
        force uut.rout_12 = 1'b0;
        force uut.rout_13 = 1'b0;
        force uut.rout_14 = 1'b0;
        force uut.rout_15 = 1'b0;
        force uut.rout_16 = 1'b0;
        force uut.rout_17 = 1'b0;
        force uut.rout_18 = 1'b0;
        force uut.rout_19 = 1'b0;
        force uut.rout_20 = 1'b0;
        
        #10;  // 等待一个时钟周期
        
        // 释放强制，让寄存器正常工作
        release uut.rout_0;
        release uut.rout_1;
        release uut.rout_2;
        release uut.rout_3;
        release uut.rout_4;
        release uut.rout_5;
        release uut.rout_6;
        release uut.rout_7;
        release uut.rout_8;
        release uut.rout_9;
        release uut.rout_10;
        release uut.rout_11;
        release uut.rout_12;
        release uut.rout_13;
        release uut.rout_14;
        release uut.rout_15;
        release uut.rout_16;
        release uut.rout_17;
        release uut.rout_18;
        release uut.rout_19;
        release uut.rout_20;

        #20;
        
      
        #100;
        $display("\nSimulation completed.");
        $finish;
    end

    // 添加波形输出
    initial begin
        $dumpfile("./sim_results/s382.vcd");
        $dumpvars(0, tb_top);
        // 如果需要更详细的信号跟踪，可以添加更多变量
        // $dumpvars(1, uut); // 这会包含uut模块中的所有信号
    end

endmodule