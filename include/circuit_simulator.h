#pragma once

#include <mockturtle/mockturtle.hpp>
#include <lorina/aiger.hpp>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

class CircuitReliabilitySimulator {
public:
    struct RegisterInfo {
        mockturtle::aig_network::node node;
        int index;
        mockturtle::aig_network::signal data_input;
        mockturtle::aig_network::signal clock_input;
        mockturtle::aig_network::signal output;
        mockturtle::aig_network::signal reset;
        mockturtle::aig_network::signal preset;
        bool current_state;
        bool next_state;
        
        RegisterInfo(mockturtle::aig_network::node n, const int& idx) 
            : node(n), index(idx), current_state(false), next_state(false) {}
    };

    struct SimulationCycle {
        int cycle_number;
        std::unordered_map<mockturtle::aig_network::node, bool> node_values;
        std::unordered_map<int, bool> register_states;  // 使用寄存器名作为键
        std::vector<bool> primary_inputs;
        std::vector<bool> primary_outputs;
        std::vector<bool> register_outputs;
        bool clock_value;
        
        SimulationCycle(int cycle) : cycle_number(cycle), clock_value(false) {}
    };



    CircuitReliabilitySimulator();
    ~CircuitReliabilitySimulator() = default;
    
    // 电路读取接口
    bool read_circuit(const std::string& filename);
    bool write_verilog(const std::string& filename);

    
    // 故障概率设置
    void set_fault_probability(double fp);
    void set_node_fault_probability(mockturtle::aig_network::node node, double fp);
    double get_node_fault_probability(mockturtle::aig_network::node node) const;
    
    // 仿真接口
    std::vector<bool> fault_free_simulation(const std::vector<bool>& inputs);
    std::unordered_map<mockturtle::aig_network::node, bool> get_node_values() const;
    std::vector<bool> fault_free_simulation_iverilog(const std::vector<bool>& inputs);

    //时序仿真
    void initialize_sequential_simulation();
    void set_clock_sequence(const std::vector<bool>& clock_sequence);
    void set_initial_state(const std::unordered_map<int, bool>& initial_state);  
    SimulationCycle simulate_sequential_cycle(const std::vector<bool>& inputs, int cycle);
    std::vector<SimulationCycle> simulate_sequential_circuit(
        const std::vector<std::vector<bool>>& input_sequence, 
        int num_cycles,
        bool reset_between_cycles = false);


    void identify_registers();
    const std::vector<RegisterInfo>& get_registers() const { return registers_; }
    std::unordered_map<int, bool> get_current_register_state() const;  
    void set_register_state(int register_index, bool state);  
    RegisterInfo* find_register_by_output(mockturtle::aig_network::node output_node);
    RegisterInfo* get_register_by_index(int index);  



    void set_clock_period(int period) { clock_period_ = period; }
    void set_clock_duty_cycle(double duty_cycle) { clock_duty_cycle_ = duty_cycle; }
    bool generate_clock_signal(int cycle, int phase = 0) const;
    
    // 电路信息获取
    mockturtle::aig_network& get_circuit() { return circuit_; }
    const mockturtle::aig_network& get_circuit() const { return circuit_; }
    size_t get_num_inputs() const;
    size_t get_num_outputs() const;
    size_t get_num_gates() const;
    size_t get_num_registers() const { return registers_.size(); }
    
    // 节点信息
    std::vector<mockturtle::aig_network::node> get_primary_inputs() const;
    std::vector<mockturtle::aig_network::node> get_primary_outputs() const;
    std::vector<mockturtle::aig_network::node> get_gates() const;
    std::vector<mockturtle::aig_network::node> get_register_outputs() const;

    void print_circuit_info() const;
    void print_registers() const;
    void print_simulation_state(int cycle) const;
    
    
private:
    mockturtle::aig_network circuit_;
    std::unordered_map<mockturtle::aig_network::node, bool> node_values_;
    std::unordered_map<mockturtle::aig_network::node, double> fault_probabilities_;

    std::vector<RegisterInfo> registers_;
    std::unordered_map<mockturtle::aig_network::node, RegisterInfo*> node_to_register_map_;
    std::unordered_map<int, RegisterInfo*> index_to_register_map_; 
    std::vector<bool> clock_sequence_;
    int current_cycle_;
    int clock_period_;
    double clock_duty_cycle_;
    std::unordered_map<int, bool> initial_register_state_;  
    
    // 门计算函数
    void compute_gate_output(mockturtle::aig_network::node node);
    bool compute_and_gate(const std::vector<bool>& inputs);
    bool compute_or_gate(const std::vector<bool>& inputs);
    bool compute_not_gate(bool input);
    bool compute_nand_gate(const std::vector<bool>& inputs);
    bool compute_nor_gate(const std::vector<bool>& inputs);
    bool compute_xor_gate(const std::vector<bool>& inputs);

    //时序辅助函数
    void setup_constant_nodes();
    void update_register_next_states();
    void propagate_register_values();
    bool is_clock_edge(int cycle) const;
    bool is_register_output(mockturtle::aig_network::node node) const;
    

    
    // 辅助函数
    bool get_fanin_value(mockturtle::aig_network::signal fanin) const;
    std::vector<bool> get_fanin_values(mockturtle::aig_network::node node) const;
};