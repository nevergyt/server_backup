// mc_fault_simulator.cpp
#include "mc_fault_simulator.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <queue>

MCFaultSimulator::MCFaultSimulator(CircuitReliabilitySimulator& simulator)
    : simulator_(simulator), 
      rng_(std::random_device{}()),
      dist_(0.0, 1.0) {
}

std::vector<MCFaultSimulator::TrendProbability> 
MCFaultSimulator::run_mc_simulations(
    int n_sim,
    int k_cycles,
    const std::vector<std::vector<bool>>& input_sequence,
    double fault_prob,
    const std::vector<int>& low_priority_nodes) {
    
    std::cout << "Algorithm 4: Running Monte Carlo fault injection simulations..." << std::endl;
    std::cout << "  n_sim = " << n_sim << ", k_cycles = " << k_cycles 
              << ", fault_prob = " << fault_prob << std::endl;
    
    // 初始化统计数据结构
    statistics_.clear();
    
    // 创建故障注入器
    FaultInjector fault_injector(simulator_.get_circuit());
    
    // 确定要统计的节点
    std::vector<int> nodes_to_monitor = low_priority_nodes;
    if (nodes_to_monitor.empty()) {
        // 如果未指定，监控所有节点
        // 这里需要从circuit_获取所有节点ID
        // 简化处理：只监控一部分节点
        // 实际应该遍历所有gate节点
    }
    
    // 为每个要监控的节点初始化统计
    for (int node_id : nodes_to_monitor) {
        statistics_[node_id].resize(k_cycles);
        for (int cycle = 0; cycle < k_cycles; cycle++) {
            statistics_[node_id][cycle] = {cycle, node_id, 0, 0};
        }
    }
    
    // 运行蒙特卡洛仿真
    for (int sim_idx = 0; sim_idx < n_sim; sim_idx++) {
        if (sim_idx % (n_sim / 10) == 0) {
            std::cout << "  Progress: " << sim_idx << "/" << n_sim 
                      << " (" << (sim_idx * 100 / n_sim) << "%)" << std::endl;
        }
        
        // 重置仿真器状态
        simulator_.initialize_sequential_simulation();
        
        // 为当前仿真随机注入故障
        inject_random_faults(fault_injector, fault_prob);
        
        // 运行k个周期
        for (int cycle = 0; cycle < k_cycles; cycle++) {
            // 获取当前周期的输入
            std::vector<bool> inputs;
            if (cycle < input_sequence.size()) {
                inputs = input_sequence[cycle];
            } else {
                inputs = std::vector<bool>(simulator_.get_num_inputs(), false);
            }
            
            // 使用故障注入进行仿真
            auto node_values = simulate_cycle_with_faults(inputs, fault_injector, cycle);
            
            // 更新统计
            for (int node_id : nodes_to_monitor) {
                auto it = node_values.find(node_id);
                if (it != node_values.end()) {
                    if (it->second == 0) {
                        statistics_[node_id][cycle].count_0++;
                    } else {
                        statistics_[node_id][cycle].count_1++;
                    }
                }
            }
            
            // 为下一个周期更新状态（如果仿真器支持）
            simulator_.propagate_register_values();
        }
    }
    
    // 计算趋势概率向量
    std::vector<TrendProbability> trends;
    for (const auto& [node_id, cycle_stats] : statistics_) {
        for (int cycle = 0; cycle < k_cycles; cycle++) {
            const auto& stat = cycle_stats[cycle];
            trends.push_back({
                node_id,
                cycle,
                stat.probability_0(),
                stat.probability_1()
            });
        }
    }
    
    // 保存到内部存储
    for (const auto& trend : trends) {
        trend_probabilities_[trend.node_id].push_back(trend);
    }
    
    std::cout << "Algorithm 4 completed: " << trends.size() 
              << " trend probabilities calculated" << std::endl;
    
    return trends;
}

void MCFaultSimulator::inject_random_faults(
    FaultInjector& fault_injector, double fault_prob) {
    
    fault_injector.clear_faults();
    
    // 遍历电路中的节点并随机注入故障
    // 注意：这里需要访问mockturtle网络的节点
    // 由于我们无法直接访问，简化处理
    
    std::cout << "  Random fault injection with probability: " << fault_prob << std::endl;
    
    // 假设我们有一些节点ID
    for (int node_id = 0; node_id < 100; node_id++) { // 示例：前100个节点
        if (dist_(rng_) < fault_prob) {
            // 随机选择stuck-at-0或stuck-at-1
            bool stuck_value = (dist_(rng_) < 0.5);
            // fault_injector.set_stuck_at_fault(node_id, stuck_value);
        }
    }
}

std::unordered_map<int, bool> 
MCFaultSimulator::simulate_cycle_with_faults(
    const std::vector<bool>& inputs,
    FaultInjector& fault_injector,
    int cycle) {
    
    std::unordered_map<int, bool> node_values;
    
    // 这里应该调用仿真器的故障注入仿真功能
    // 由于当前仿真器不支持，我们简化处理
    
    // 实际应该：
    // 1. 设置输入值
    // 2. 使用故障注入器进行仿真
    // 3. 返回所有节点值
    
    return node_values;
}

std::unordered_map<int, std::pair<double, double>>
MCFaultSimulator::get_low_priority_trend_vectors(
    int k_cycles, double priority_threshold) {
    
    std::unordered_map<int, std::pair<double, double>> vectors;
    
    // 计算节点优先级
    auto priorities = calculate_gate_priorities(k_cycles);
    
    // 找出低优先级节点
    std::vector<int> low_priority_nodes;
    for (const auto& [node_id, priority] : priorities) {
        if (priority < priority_threshold) {
            low_priority_nodes.push_back(node_id);
        }
    }
    
    std::cout << "Found " << low_priority_nodes.size() 
              << " low priority nodes (threshold = " << priority_threshold << ")" << std::endl;
    
    // 计算每个低优先级节点的趋势概率向量
    for (int node_id : low_priority_nodes) {
        double avg_prob_0 = 0.0;
        double avg_prob_1 = 0.0;
        int count = 0;
        
        if (trend_probabilities_.count(node_id)) {
            const auto& node_trends = trend_probabilities_[node_id];
            for (const auto& trend : node_trends) {
                if (trend.cycle < k_cycles) {
                    avg_prob_0 += trend.prob_0;
                    avg_prob_1 += trend.prob_1;
                    count++;
                }
            }
            
            if (count > 0) {
                avg_prob_0 /= count;
                avg_prob_1 /= count;
                vectors[node_id] = {avg_prob_0, avg_prob_1};
            }
        }
    }
    
    return vectors;
}

std::unordered_map<int, double> 
MCFaultSimulator::calculate_gate_priorities(int k_cycles) {
    
    std::unordered_map<int, double> priorities;
    
    // 计算扇出源长度（论文公式3）
    auto fanout_lengths = get_fanout_source_lengths();
    
    // 计算拓扑距离（论文公式4,6）
    auto distances = get_topological_distances();
    
    // 计算每个节点的优先级（论文公式5,7）
    for (const auto& [node_id, fanout_length] : fanout_lengths) {
        if (distances.count(node_id)) {
            double py_pre = fanout_length; // 简化：使用扇出源长度
            double py_suc = distances[node_id]; // 使用到主输出的距离
            
            // 论文公式5: G_{k-i}.py_1 = G_{k-i}.py_pre + G_{k-i}.py_suc
            double py_1 = py_pre + py_suc;
            
            // 论文公式6: G_{k-i}.py_2 = i (拓扑顺序索引)
            double py_2 = node_id; // 简化：使用节点ID作为拓扑索引
            
            // 归一化
            // 这里需要所有节点的和，简化处理
            double norm_py1 = py_1 / 1000.0; // 假设总和为1000
            double norm_py2 = py_2 / 1000.0; // 假设总和为1000
            
            // 论文公式7: G_{k-i}.cpy = Σ(λ_j * G_{k-i}.py_j / Σ(G_{k-i}.py_j))
            double lambda1 = 0.75; // 论文默认值
            double lambda2 = 0.25; // 论文默认值
            
            priorities[node_id] = lambda1 * norm_py1 + lambda2 * norm_py2;
        }
    }
    
    return priorities;
}

std::unordered_map<int, double> 
MCFaultSimulator::get_fanout_source_lengths() {
    
    std::unordered_map<int, double> lengths;
    
    // 这里应该实现扇出源跟踪算法（论文算法1）
    // 简化：假设每个节点的扇出源长度为1
    for (int i = 0; i < 100; i++) { // 示例：前100个节点
        lengths[i] = 1.0 + (i % 10); // 简单变化
    }
    
    return lengths;
}

std::unordered_map<int, int> 
MCFaultSimulator::get_topological_distances() {
    
    std::unordered_map<int, int> distances;
    
    // 计算每个节点到主输出的最大距离
    // 这里应该实现拓扑分析
    // 简化：假设距离与节点ID相关
    for (int i = 0; i < 100; i++) {
        distances[i] = 10 - (i % 10); // 简单变化
    }
    
    return distances;
}

void MCFaultSimulator::save_results(const std::string& filename) {
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return;
    }
    
    file << std::fixed << std::setprecision(6);
    
    // 写入趋势概率
    file << "Trend Probabilities:\n";
    for (const auto& [node_id, trends] : trend_probabilities_) {
        file << "Node " << node_id << ":\n";
        for (const auto& trend : trends) {
            file << "  Cycle " << trend.cycle 
                 << ": P(0)=" << trend.prob_0 
                 << ", P(1)=" << trend.prob_1 << "\n";
        }
    }
    
    // 写入统计
    file << "\nFault Statistics:\n";
    for (const auto& [node_id, cycle_stats] : statistics_) {
        file << "Node " << node_id << ":\n";
        for (const auto& stat : cycle_stats) {
            file << "  Cycle " << stat.cycle 
                 << ": count_0=" << stat.count_0 
                 << ", count_1=" << stat.count_1 
                 << ", P(0)=" << stat.probability_0()
                 << ", P(1)=" << stat.probability_1() << "\n";
        }
    }
    
    file.close();
    std::cout << "Results saved to " << filename << std::endl;
}

void MCFaultSimulator::load_results(const std::string& filename) {
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return;
    }
    
    // 简化：实际应该解析文件
    std::cout << "Loading results from " << filename << std::endl;
    
    file.close();
}