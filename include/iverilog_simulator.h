#ifndef IVERILOG_SIMULATOR_H
#define IVERILOG_SIMULATOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>

// 仿真结果结构体
struct SimulationResult {
    int return_code;
    std::string stdout_output;
    std::string stderr_output;
    bool success;
    
    SimulationResult() : return_code(-1), success(false) {}
};

class IverilogSimulator {
public:
    // 构造函数和析构函数
    IverilogSimulator();
    explicit IverilogSimulator(const std::string& work_dir);
    ~IverilogSimulator();

    // 禁止拷贝
    IverilogSimulator(const IverilogSimulator&) = delete;
    IverilogSimulator& operator=(const IverilogSimulator&) = delete;

    // 文件管理
    void add_source_file(const std::string& file_path);
    void add_source_files(const std::vector<std::string>& file_paths);
    void clear_source_files();

    // 预编译宏定义
    void define_macro(const std::string& name, const std::string& value = "");
    void undefine_macro(const std::string& name);
    void clear_macros();

    // 包含目录
    void add_include_dir(const std::string& directory);
    void clear_include_dirs();

    // 编译器选项
    void set_compiler_options(const std::vector<std::string>& options);
    void add_compiler_option(const std::string& option);
    void clear_compiler_options();

    // 仿真器选项
    void set_simulation_options(const std::vector<std::string>& options);
    void add_simulation_option(const std::string& option);
    void clear_simulation_options();

    // 编译和仿真
    std::string compile(const std::string& output_name = "simulation");
    SimulationResult run_simulation(const std::string& executable_path, 
                                   const std::string& vcd_output = "",
                                   int timeout_ms = 0);
    SimulationResult simulate(const std::string& vcd_output = "",
                             const std::string& output_name = "simulation",
                             int timeout_ms = 0);

    // 工具函数
    std::string get_work_dir() const;
    void clean();
    bool is_iverilog_available() const;

    // 状态查询
    size_t get_source_file_count() const { return source_files_.size(); }
    size_t get_macro_count() const { return defines_.size(); }
    size_t get_include_dir_count() const { return include_dirs_.size(); }

private:
    // 内部实现
    bool check_iverilog_available();
    std::string execute_command(const std::vector<std::string>& command, int timeout_ms = 0);
    std::string build_compiler_command(const std::string& output_path) const;
    std::vector<std::string> build_simulation_command(const std::string& executable_path,
                                                     const std::string& vcd_output) const;
    bool file_exists(const std::string& path) const;
    std::string create_temp_directory();
    void remove_directory(const std::string& path);
    std::string join_strings(const std::vector<std::string>& strings, const std::string& delimiter) const;
    
    // 成员变量
    std::string work_dir_;
    std::vector<std::string> source_files_;
    std::map<std::string, std::string> defines_;
    std::vector<std::string> include_dirs_;
    std::vector<std::string> compiler_options_;
    std::vector<std::string> simulation_options_;
    bool use_temp_dir_;
    bool iverilog_available_;
};

// 工具函数声明
std::vector<std::string> split_string(const std::string& str, char delimiter);

#endif // IVERILOG_SIMULATOR_H