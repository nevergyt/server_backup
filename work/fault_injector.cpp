#include "fault_injector.h"
#include <iostream>

FaultInjector::FaultInjector(mockturtle::aig_network& circuit) 
    : circuit_(circuit), rng_(std::random_device{}()), dist_(0.0, 1.0) {
}

void FaultInjector::inject_random_faults(double fault_probability) {
    stuck_at_faults_.clear();
    
    circuit_.foreach_gate([&](auto node) {
        if (dist_(rng_) < fault_probability) {
            // 随机选择 stuck-at-0 或 stuck-at-1
            stuck_at_faults_[node] = (dist_(rng_) < 0.5);
        }
    });
    
    std::cout << "Injected " << stuck_at_faults_.size() << " random faults" << std::endl;
}

void FaultInjector::set_stuck_at_fault(mockturtle::aig_network::node node, bool stuck_value) {
    stuck_at_faults_[node] = stuck_value;
}

void FaultInjector::clear_faults() {
    stuck_at_faults_.clear();
}

void FaultInjector::clear_node_fault(mockturtle::aig_network::node node) {
    stuck_at_faults_.erase(node);
}

std::vector<bool> FaultInjector::simulate_with_faults(
    const std::vector<bool>& inputs,
    const std::unordered_map<mockturtle::aig_network::node, bool>& node_values) {
    
    auto faulty_values = node_values; // 复制无故障值
    
    // 按拓扑顺序计算门输出（考虑故障）
    std::vector<mockturtle::aig_network::node> topo_order;
    circuit_.foreach_node([&](auto node) {
        topo_order.push_back(node);
    });
    
    // 简单拓扑排序（实际应该使用更复杂的算法）
    for (auto node : topo_order) {
        if (circuit_.is_constant(node)) {
            continue; // 常数节点已经设置
        }
        
        if (circuit_.is_pi(node)) {
            continue; // 主输入已经设置
        }
        
        if (stuck_at_faults_.count(node)) {
            // 如果有故障，使用故障值
            faulty_values[node] = stuck_at_faults_.at(node);
        } else {
            // 否则正常计算
            compute_gate_output_with_values(node, faulty_values);
        }
    }
    
    // 收集输出
    std::vector<bool> outputs;
    circuit_.foreach_po([&](auto po) {
        auto po_node = circuit_.get_node(po);
        bool value = faulty_values[po_node];
        if (circuit_.is_complemented(po)) {
            value = !value;
        }
        outputs.push_back(value);
    });
    
    return outputs;
}

void FaultInjector::compute_gate_output_with_values(
    mockturtle::aig_network::node node, 
    std::unordered_map<mockturtle::aig_network::node, bool>& values) {
    
    if (circuit_.is_constant(node)) {
        values[node] = circuit_.constant_value(node);
        return;
    }
    
    // 获取扇入值（考虑故障）
    std::vector<bool> fanin_values = get_fanin_values_with_faults(node, values);
    
    // 根据门类型计算输出
    if (circuit_.is_and(node)) {
        bool result = true;
        for (bool val : fanin_values) {
            result &= val;
        }
        values[node] = result;
    }
    // 可以扩展其他门类型
}

bool FaultInjector::get_fanin_value_with_faults(
    mockturtle::aig_network::signal fanin,
    const std::unordered_map<mockturtle::aig_network::node, bool>& values) const {
    
    auto node = circuit_.get_node(fanin);
    bool value = values.at(node);
    
    // 如果该节点有故障，使用故障值
    if (stuck_at_faults_.count(node)) {
        value = stuck_at_faults_.at(node);
    }
    
    if (circuit_.is_complemented(fanin)) {
        value = !value;
    }
    
    return value;
}

std::vector<bool> FaultInjector::get_fanin_values_with_faults(
    mockturtle::aig_network::node node,
    const std::unordered_map<mockturtle::aig_network::node, bool>& values) const {
    
    std::vector<bool> fanin_values;
    circuit_.foreach_fanin(node, [&](auto fanin) {
        fanin_values.push_back(get_fanin_value_with_faults(fanin, values));
    });
    return fanin_values;
}

bool FaultInjector::has_fault(mockturtle::aig_network::node node) const {
    return stuck_at_faults_.count(node) > 0;
}

bool FaultInjector::get_fault_value(mockturtle::aig_network::node node) const {
    auto it = stuck_at_faults_.find(node);
    return (it != stuck_at_faults_.end()) ? it->second : false;
}

std::vector<mockturtle::aig_network::node> FaultInjector::get_faulty_nodes() const {
    std::vector<mockturtle::aig_network::node> faulty_nodes;
    for (const auto& [node, _] : stuck_at_faults_) {
        faulty_nodes.push_back(node);
    }
    return faulty_nodes;
}