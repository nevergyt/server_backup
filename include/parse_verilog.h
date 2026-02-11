#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mockturtle/mockturtle.hpp>
#include <mockturtle/algorithms/klut_to_graph.hpp>
#include <lorina/aiger.hpp>


class ParseVerilog {
public:

    ParseVerilog();
    explicit ParseVerilog(const std::string& work_dir);
    ~ParseVerilog();

    ParseVerilog(const ParseVerilog&) = delete;
    ParseVerilog& operator=(const ParseVerilog&) = delete;


    // 主要功能函数
    bool read_circuit(const std::string& filename);
    bool read_blifCircuit(const std::string& filename);
    bool write_blif(const std::string& filename,bool isSeq);
    bool convert_blif_to_verilog(const std::string& blif_file, 
                                const std::string& verilog_output);
    bool parse_verilog(const std::string& filename,
                                const std::string& verilog_output,bool isSeq);
    
    // 网络信息
    mockturtle::aig_network& get_circuit() { return circuit_; }
    size_t get_gate_count() const;
    size_t get_pi_count() const;
    size_t get_po_count() const;
    std::vector<std::string> get_input_names() const;
    std::vector<std::string> get_output_names() const;
    
    // Yosys 控制
    void set_yosys_path(const std::string& path);
    bool check_yosys_available() const;
    
    // 工具函数
    std::string get_work_dir() const;
    void clean();

private:
    // 内部实现
    bool execute_yosys_command(const std::string& script);
    std::string generate_yosys_script(const std::string& blif_file, 
                                     const std::string& verilog_output);
    bool create_directory(const std::string& path) const;
    bool file_exists(const std::string& path) const;
    std::string create_temp_directory();
    void remove_directory(const std::string& path);
    
    // 成员变量
    std::string work_dir_;
    std::string yosys_path_;
    mockturtle::aig_network circuit_;
    mockturtle::klut_network klu_circuit;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    bool use_temp_dir_;
    bool yosys_available_;
};
