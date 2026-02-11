#include "parse_verilog.h"
#include <iostream>
#include <cstdlib>
#include <array>
#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


ParseVerilog::ParseVerilog() 
    : use_temp_dir_(true),
      yosys_available_(false) {
    work_dir_ = create_temp_directory();
    yosys_path_ = "yosys";  // 默认路径
    yosys_available_ = check_yosys_available();
}

ParseVerilog::ParseVerilog(const std::string& work_dir)
    : work_dir_(work_dir),
      use_temp_dir_(false),
      yosys_available_(false) {
    
    // 创建工作目录
    if (!file_exists(work_dir_)) {
        create_directory(work_dir_);
    }
    
    yosys_path_ = "yosys";  // 默认路径
    yosys_available_ = check_yosys_available();
}

ParseVerilog::~ParseVerilog() {
    if (use_temp_dir_ && !work_dir_.empty()) {
        remove_directory(work_dir_);
    }
}

bool ParseVerilog::file_exists(const std::string& path) const {
    return std::filesystem::exists(path);
}

bool ParseVerilog::create_directory(const std::string& path) const {
    return std::filesystem::create_directories(path);
}

std::string ParseVerilog::create_temp_directory() {
    std::string pattern = "/tmp/parse_verilog_XXXXXX";
    char* result = mkdtemp(const_cast<char*>(pattern.c_str()));
    if (result == nullptr) {
        throw std::runtime_error("Failed to create temporary directory");
    }
    return result;
}

void ParseVerilog::remove_directory(const std::string& path) {
    try {
        std::filesystem::remove_all(path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: Failed to remove directory " << path << ": " << e.what() << std::endl;
    }
}

bool ParseVerilog::check_yosys_available() const {
    try {
        std::string command = yosys_path_ + " -h > /dev/null 2>&1";
        int result = std::system(command.c_str());
        return (result == 0);
    } catch (...) {
        return false;
    }
}


bool ParseVerilog::read_circuit(const std::string& filename) {
    auto const result = lorina::read_aiger( filename, mockturtle::aiger_reader( circuit_ ) );
    if ( result != lorina::return_code::success )
    {
      std::cout << "Read benchmark failed\n";
      return false;
    }
    
    std::cout << "Successfully read Aig circuit: " << filename << std::endl; 
    return true;
}


bool ParseVerilog::read_blifCircuit(const std::string& filename) {
    auto const result = lorina::read_blif(filename, mockturtle::blif_reader(klu_circuit));
    if ( result != lorina::return_code::success )
    {
      std::cout << "Read benchmark failed\n";
      return false;
    }
    
    circuit_=mockturtle::convert_klut_to_graph<mockturtle::aig_network>( klu_circuit );
    std::cout << "Successfully read Blif circuit: " << filename << std::endl; 
    return true;
}

bool ParseVerilog::write_blif(const std::string& filename,bool isSeq) {


    try {
        mockturtle::names_view name_aig{circuit_};

        int wire=0;
        int input=0;
        int latch=0;

        name_aig.foreach_node([&] (auto node,auto index){
            auto signal = name_aig.make_signal(node);
            if(name_aig.is_pi(node)){
                // name_aig.set_name(signal, "input_" + std::to_string(input++));
            }
            else if(name_aig.is_ro(node)){
                name_aig.set_name(signal, "rout_" + std::to_string(latch++));
            }
            else{
                name_aig.set_name(signal, "signal_" + std::to_string(name_aig.node_to_index(node))); 
            }
        });

        name_aig.foreach_pi([&] (auto node,auto index){
            auto signal = name_aig.make_signal(node);
            if(index==0&&isSeq){
                name_aig.set_name(signal, "clock");
            }
            else{
                name_aig.set_name(signal, "input_" + std::to_string(input++));
            }
        });

        // name_aig.foreach_ro([&](auto node){
        //     auto signal = name_aig.make_signal(node);
        //     std::cout<< "ro register :  " <<name_aig.get_name(signal)<<std::endl;
        // });
        
        // name_aig.foreach_register([&](auto pair){
        //     std::cout<< "name signal register input:  " <<name_aig.get_name(pair.first)<<std::endl;
        //     auto signal = name_aig.make_signal(pair.second);
        //     std::cout<< "name signal register output:  " <<name_aig.get_name(signal)<<std::endl;
        // });

        mockturtle::write_blif(name_aig,filename);

        mockturtle::write_dot( name_aig, "aig_before.blif.dot" );
        
        std::cout << "Successfully wrote BLIF file: " << filename << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error writing BLIF file: " << e.what() << std::endl;
        return false;
    }
}

std::string ParseVerilog::generate_yosys_script(const std::string& blif_file, 
                                               const std::string& verilog_output) {
    std::stringstream script;
    


    script << "# Yosys script for BLIF to Verilog conversion\n";
    script << "read_blif " << blif_file << "\n";
    // script << "synth\n";
    // script << "proc\n";
    // script << "flatten\n";
    script << "write_verilog -noattr " << verilog_output << "\n";
    
    return script.str();
}

bool ParseVerilog::execute_yosys_command(const std::string& script) {
    if (!yosys_available_) {
        std::cerr << "Yosys is not available. Please install Yosys or set correct path." << std::endl;
        return false;
    }
    
    // 写入 Yosys 脚本文件
    std::string script_file = std::filesystem::path(work_dir_) / "yosys_script.ys";
    std::ofstream script_stream(script_file);
    if (!script_stream.is_open()) {
        std::cerr << "Failed to create Yosys script file: " << script_file << std::endl;
        return false;
    }
    
    script_stream << script;
    script_stream.close();
    
    // 执行 Yosys
    std::string command = yosys_path_ + " -s " + script_file;
    std::cout << "Executing Yosys: " << command << std::endl;
    
    int result = std::system(command.c_str());
    
    if (result != 0) {
        std::cerr << "Yosys execution failed with return code: " << result << std::endl;
        return false;
    }
    
    std::cout << "Yosys execution completed successfully" << std::endl;
    return true;
}

bool ParseVerilog::convert_blif_to_verilog(const std::string& blif_file, 
                                          const std::string& verilog_output) {
    if (!file_exists(blif_file)) {
        std::cerr << "BLIF file not found: " << blif_file << std::endl;
        return false;
    }
    
    try {
        std::string script = generate_yosys_script(blif_file, verilog_output);
        bool success = execute_yosys_command(script);
        
        if (success && file_exists(verilog_output)) {
            std::cout << "Successfully converted BLIF to Verilog: " << verilog_output << std::endl;
            return true;
        } else {
            std::cerr << "Failed to generate Verilog output" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error converting BLIF to Verilog: " << e.what() << std::endl;
        return false;
    }
}

bool ParseVerilog::parse_verilog(const std::string& filename,const std::string& verilog_output,bool isSeq){
    if(!write_blif(filename,isSeq)){
        std::cout<<"Error writing BLIF file: "<<std::endl;
        return false;
    }

    if(!convert_blif_to_verilog(filename,verilog_output)){
        std::cout<<"Error convert verilog file: "<<std::endl;
        return false;
    }

    return true;
}



std::vector<std::string> ParseVerilog::get_input_names() const {
    return input_names_;
}

std::vector<std::string> ParseVerilog::get_output_names() const {
    return output_names_;
}

std::string ParseVerilog::get_work_dir() const {
    return work_dir_;
}

void ParseVerilog::clean() {
    if (!work_dir_.empty() && std::filesystem::exists(work_dir_)) {
        for (const auto& entry : std::filesystem::directory_iterator(work_dir_)) {
            std::filesystem::remove_all(entry.path());
        }
        std::cout << "Cleaned work directory: " << work_dir_ << std::endl;
    }
}