module tb_top;
    reg clock;
    reg [3:0] inputs;
    wire po0;
    
    top uut(
        .clock(clock),
        .input_0(inputs[0]),
        .input_1(inputs[1]),
        .input_2(inputs[2]),
        .input_3(inputs[3]),
        .po0(po0)
    );
    
    // 生成时钟
    initial begin
        clock = 0;
        forever #5 clock = ~clock;
    end
    
    initial begin
        // 初始化
        inputs = 4'b0000;

        force uut.rout_0 = 1'b0;
        force uut.rout_1 = 1'b0;
        force uut.rout_2 = 1'b0;
        
        #10;  // 等待一个时钟周期
        
        // 释放强制，让寄存器正常工作
        release uut.rout_0;
        release uut.rout_1;
        release uut.rout_2;

        #20;
        
        // 测试所有可能的输入组合
        for (int i = 0; i < 16; i = i + 1) begin
            #20;
            $display("Inputs=%04b, po0=%b, rout_0=%b, rout_1=%b, rout_2=%b",
                    inputs, po0, uut.rout_0, uut.rout_1, uut.rout_2);
        end
        
        #100;
        $finish;
    end

    initial begin
        $dumpfile("./sim_results/s27.vcd");
        $dumpvars(0, tb_top);
    end

endmodule