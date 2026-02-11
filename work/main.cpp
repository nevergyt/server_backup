#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include "circuit_simulator.h"
#include "fault_injector.h"
#include "fstra.h"
#include "iverilog_simulator.h"
#include "parse_verilog.h"
#include "vcd_parser.h"
#include <omp.h>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <circuit_file> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -fp <value>    Fault probability (default: 0.01)" << std::endl;
    std::cout << "  -mc <count>    Monte Carlo iterations (default: 1000)" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
}



int main(int argc, char* argv[]) {
    // 默认参数
    std::string circuit_file;
    double fault_probability = 0.01;
    int monte_carlo_iterations = 1000;
    int runCycles;
    std::vector<int> vec_int={0,0,0,0};
    bool simOpen=true;
    bool computeOpen=true;

    omp_set_nested(1);  // 启用嵌套并行
    omp_set_max_active_levels(2);  // 允许2层嵌套


    bool is_comb=false;
    std::string ISCAS89Name="s382";
    std::string ISCAS85Name="c432";


    std::string nameblif;
    std::string nameVerilog;
    std::string nameTb;
    std::string path;

    ParseVerilog parser("./parse");


    if(is_comb){
        path="../src/benchmarks/"+ISCAS85Name+".aig";
        nameTb=ISCAS85Name+"_tb.v";
        nameblif=ISCAS85Name+".blif";
        nameVerilog=ISCAS85Name+"_aig.v";
        runCycles=1;
        if(!parser.read_circuit(path))return -1;
    }
    else{
        path="../src/benchmarks89/AIG/"+ISCAS89Name+".blif";
        nameTb=ISCAS89Name+"_tb.v";
        nameblif=ISCAS89Name+".blif";
        nameVerilog=ISCAS89Name+"_aig.v";
        runCycles=5;
        if(!parser.read_blifCircuit(path))return -1;
    }

    parser.parse_verilog(nameblif,nameVerilog,!is_comb);



     // 创建仿真器实例
    IverilogSimulator sim("./sim_results");
    VCDParser vcd_parser;
    
    

    if(simOpen){

        std::cout << "Icarus Verilog available: " << (sim.is_iverilog_available() ? "Yes" : "No") << std::endl;
        std::cout << "Work directory: " << sim.get_work_dir() << std::endl;


        // 添加源文件
        sim.add_source_file("s382_aig.v");
        sim.add_source_file(nameTb);

        // sim.add_source_file("c432_aig.v");
        // sim.add_source_file("c432_tb.v");

        // sim.add_source_file("s27_aig.v");
        // sim.add_source_file("s27_tb.v");
        
        // 设置预编译宏
        sim.define_macro("DEBUG");
        sim.define_macro("CLOCK_FREQ", "1000000");
        
        
        // // 设置编译器选项
        sim.set_compiler_options({"-g2012", "-Wall"});
        
        // // // 运行仿真
        SimulationResult result = sim.simulate("waveform.vcd", "my_simulation");
        
        // 输出结果
        std::cout << "Simulation " << (result.success ? "succeeded" : "failed") << std::endl;
        std::cout << "Return code: " << result.return_code << std::endl;

        if (!vcd_parser.parseFile("./sim_results/s382.vcd")) {
                std::cerr << "Failed to parse VCD file." << std::endl;
                return -1;
        }
        
        vcd_parser.setClockSignal("clock"); // 上升沿触发
    }
    

    // 解析 VCD 文件
    

    if(computeOpen){
        FSTRAAnalyzer fs_tra_analyzer(parser.get_circuit(),sim,vcd_parser);

        // 初始化 FS 节点
        fs_tra_analyzer.initializeFSNodes(runCycles);
        
        // fs_tra_analyzer.runParallelReliabilityCalculation(vec_int,runCycles);

        // fs_tra_analyzer.FS_TRAMethod(runCycles,5);
        fs_tra_analyzer.FS_TRAMethodByCycle(runCycles,5);
    }

    
    
    return 0;
    
}