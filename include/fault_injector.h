#pragma once

#include "circuit_simulator.h"
#include <random>
#include <unordered_map>

class FaultInjector {
public:
    explicit FaultInjector(mockturtle::aig_network& circuit);
    ~FaultInjector() = default;
    
    // 故障注入接口
    void inject_random_faults(double fault_probability);
    void set_stuck_at_fault(mockturtle::aig_network::node node, bool stuck_value);
    void clear_faults();
    void clear_node_fault(mockturtle::aig_network::node node);
    
    // 仿真接口
    std::vector<bool> simulate_with_faults(
        const std::vector<bool>& inputs,
        const std::unordered_map<mockturtle::aig_network::node, bool>& node_values);
    
    // 故障信息获取
    const std::unordered_map<mockturtle::aig_network::node, bool>& 
        get_injected_faults() const { return stuck_at_faults_; }
    
    bool has_fault(mockturtle::aig_network::node node) const;
    bool get_fault_value(mockturtle::aig_network::node node) const;
    
    // 统计信息
    size_t get_num_injected_faults() const { return stuck_at_faults_.size(); }
    std::vector<mockturtle::aig_network::node> get_faulty_nodes() const;
    
private:
    mockturtle::aig_network& circuit_;
    std::unordered_map<mockturtle::aig_network::node, bool> stuck_at_faults_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;
    
    // 门计算函数（考虑故障）
    void compute_gate_output_with_values(
        mockturtle::aig_network::node node, 
        std::unordered_map<mockturtle::aig_network::node, bool>& values);
    
    // 辅助函数
    bool get_fanin_value_with_faults(
        mockturtle::aig_network::signal fanin,
        const std::unordered_map<mockturtle::aig_network::node, bool>& values) const;
        
    std::vector<bool> get_fanin_values_with_faults(
        mockturtle::aig_network::node node,
        const std::unordered_map<mockturtle::aig_network::node, bool>& values) const;
};