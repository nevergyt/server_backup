#include "iverilog_simulator.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


// 工具函数实现
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// IverilogSimulator 实现

IverilogSimulator::IverilogSimulator() 
    : use_temp_dir_(true),
      iverilog_available_(false) {
    work_dir_ = create_temp_directory();
    iverilog_available_ = check_iverilog_available();
}

IverilogSimulator::IverilogSimulator(const std::string& work_dir)
    : work_dir_(work_dir),
      use_temp_dir_(false),
      iverilog_available_(false) {
    
    // 创建工作目录
    if (!file_exists(work_dir_)) {
        std::filesystem::create_directories(work_dir_);
    }
    
    iverilog_available_ = check_iverilog_available();
}

IverilogSimulator::~IverilogSimulator() {
    if (use_temp_dir_ && !work_dir_.empty()) {
        remove_directory(work_dir_);
    }
}

bool IverilogSimulator::check_iverilog_available() {
    try {
        std::string command = "iverilog -V >/dev/null 2>&1";
        int result = std::system(command.c_str());
        return (result == 0);
    } catch (...) {
        return false;
    }
}

bool IverilogSimulator::file_exists(const std::string& path) const {
    return std::filesystem::exists(path);
}

std::string IverilogSimulator::create_temp_directory() {

    std::string pattern = "/tmp/iverilog_sim_XXXXXX";
    char* result = mkdtemp(const_cast<char*>(pattern.c_str()));
    if (result == nullptr) {
        throw std::runtime_error("Failed to create temporary directory");
    }
    return result;
}

void IverilogSimulator::remove_directory(const std::string& path) {
    try {
        std::filesystem::remove_all(path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: Failed to remove directory " << path << ": " << e.what() << std::endl;
    }
}

std::string IverilogSimulator::join_strings(const std::vector<std::string>& strings, const std::string& delimiter) const {
    std::stringstream ss;
    for (size_t i = 0; i < strings.size(); ++i) {
        if (i > 0) ss << delimiter;
        ss << strings[i];
    }
    return ss.str();
}

void IverilogSimulator::add_source_file(const std::string& file_path) {
    if (!file_exists(file_path)) {
        throw std::runtime_error("Source file not found: " + file_path);
    }
    source_files_.push_back(file_path);
}

void IverilogSimulator::add_source_files(const std::vector<std::string>& file_paths) {
    for (const auto& file_path : file_paths) {
        add_source_file(file_path);
    }
}

void IverilogSimulator::clear_source_files() {
    source_files_.clear();
}

void IverilogSimulator::define_macro(const std::string& name, const std::string& value) {
    defines_[name] = value;
}

void IverilogSimulator::undefine_macro(const std::string& name) {
    defines_.erase(name);
}

void IverilogSimulator::clear_macros() {
    defines_.clear();
}

void IverilogSimulator::add_include_dir(const std::string& directory) {
    if (!file_exists(directory)) {
        throw std::runtime_error("Include directory not found: " + directory);
    }
    include_dirs_.push_back(directory);
}

void IverilogSimulator::clear_include_dirs() {
    include_dirs_.clear();
}

void IverilogSimulator::set_compiler_options(const std::vector<std::string>& options) {
    compiler_options_ = options;
}

void IverilogSimulator::add_compiler_option(const std::string& option) {
    compiler_options_.push_back(option);
}

void IverilogSimulator::clear_compiler_options() {
    compiler_options_.clear();
}

void IverilogSimulator::set_simulation_options(const std::vector<std::string>& options) {
    simulation_options_ = options;
}

void IverilogSimulator::add_simulation_option(const std::string& option) {
    simulation_options_.push_back(option);
}

void IverilogSimulator::clear_simulation_options() {
    simulation_options_.clear();
}

std::string IverilogSimulator::build_compiler_command(const std::string& output_path) const {
    std::vector<std::string> command = {"iverilog"};
    
    // 添加编译器选项
    command.insert(command.end(), compiler_options_.begin(), compiler_options_.end());
    
    // 添加预编译宏定义
    for (const auto& [name, value] : defines_) {
        if (!value.empty()) {
            command.push_back("-D" + name + "=" + value);
        } else {
            command.push_back("-D" + name);
        }
    }
    
    // 添加包含目录
    for (const auto& include_dir : include_dirs_) {
        command.push_back("-I");
        command.push_back(include_dir);
    }
    
    // 添加源文件
    command.insert(command.end(), source_files_.begin(), source_files_.end());
    
    // 设置输出文件
    command.push_back("-o");
    command.push_back(output_path);
    
    return join_strings(command, " ");
}

std::vector<std::string> IverilogSimulator::build_simulation_command(
    const std::string& executable_path,
    const std::string& vcd_output) const {
    
    std::vector<std::string> command = {"vvp"};
    
    // 添加仿真器选项
    command.insert(command.end(), simulation_options_.begin(), simulation_options_.end());
    
    // 添加VCD输出选项（正确的语法）
    if (!vcd_output.empty()) {
        std::string vcd_path = std::filesystem::path(work_dir_) / vcd_output;
        // command.push_back("-vcd");
        // command.push_back(vcd_path);
    }
    
    // 添加可执行文件（必须在最后）
    command.push_back(executable_path);
    
    return command;
}

std::string IverilogSimulator::execute_command(const std::vector<std::string>& command, int timeout_ms) {
    std::string full_command = join_strings(command, " ");
    
    std::cout << "Executing: " << full_command << std::endl;
    
    std::array<char, 128> buffer;
    std::string result;
    
    // 使用popen执行命令
    FILE* pipe = popen(full_command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    int return_code = pclose(pipe);
    
    if (return_code != 0) {
        throw std::runtime_error("Command failed with return code: " + std::to_string(return_code));
    }
    
    return result;
}

std::string IverilogSimulator::compile(const std::string& output_name) {
    if (!iverilog_available_) {
        throw std::runtime_error("Icarus Verilog is not available");
    }
    
    if (source_files_.empty()) {
        throw std::runtime_error("No source files added for compilation");
    }
    
    std::string output_path = std::filesystem::path(work_dir_) / (output_name + ".out");
    std::string command_str = build_compiler_command(output_path);
    
    try {
        auto result = execute_command(split_string(command_str, ' '));
        std::cout << "Compilation successful!" << std::endl;
        return output_path;
    } catch (const std::exception& e) {
        std::string error_msg = "Compilation failed: " + std::string(e.what());
        std::cerr << error_msg << std::endl;
        throw std::runtime_error(error_msg);
    }
}

SimulationResult IverilogSimulator::run_simulation(
    const std::string& executable_path,
    const std::string& vcd_output,
    int timeout_ms) {
    
    SimulationResult result;
    
    if (!file_exists(executable_path)) {
        result.stderr_output = "Executable not found: " + executable_path;
        return result;
    }
    
    auto command = build_simulation_command(executable_path, vcd_output);
    
    try {
        std::string output = execute_command(command, timeout_ms);
        result.stdout_output = output;
        result.return_code = 0;
        result.success = true;
        std::cout << "Simulation completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        result.stderr_output = "Simulation failed: " + std::string(e.what());
        result.return_code = -1;
        result.success = false;
        std::cerr << result.stderr_output << std::endl;
    }
    
    return result;
}

SimulationResult IverilogSimulator::simulate(
    const std::string& vcd_output,
    const std::string& output_name,
    int timeout_ms) {
    
    SimulationResult result;
    
    try {
        std::string executable_path = compile(output_name);
        result = run_simulation(executable_path, vcd_output, timeout_ms);
    } catch (const std::exception& e) {
        result.stderr_output = "Simulation error: " + std::string(e.what());
        result.return_code = -1;
        result.success = false;
    }
    
    return result;
}

std::string IverilogSimulator::get_work_dir() const {
    return work_dir_;
}

void IverilogSimulator::clean() {
    if (!work_dir_.empty() && std::filesystem::exists(work_dir_)) {
        for (const auto& entry : std::filesystem::directory_iterator(work_dir_)) {
            std::filesystem::remove_all(entry.path());
        }
        std::cout << "Cleaned work directory: " << work_dir_ << std::endl;
    }
}

bool IverilogSimulator::is_iverilog_available() const {
    return iverilog_available_;
}