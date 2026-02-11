// mc_fault_simulator.h
#ifndef MC_FAULT_SIMULATOR_H
#define MC_FAULT_SIMULATOR_H

#include "circuit_simulator.h"
#include "fault_injector.h"
#include <random>
#include <unordered_map>
#include <vector>
#include <functional>

class MCFaultSimulator {
public:
    // 故障统计数据结构
    struct FaultStatistics {
        int cycle;
        int node_id;
        int count_0;
        int count_1;
        
        double probability_0() const { 
            return (count_0 + count_1) > 0 ? 
                static_cast<double>(count_0) / (count_0 + count_1) : 0.0; 
        }
        
        double probability_1() const { 
            return (count_0 + count_1) > 0 ? 
                static_cast<double>(count_1) / (count_0 + count_1) : 0.0; 
        }
    };
    
    // 趋势概率向量
    struct TrendProbability {
        int node_id;
        int cycle;
        double prob_0;
        double prob_1;
    };
    
    MCFaultSimulator(CircuitReliabilitySimulator& simulator);
    
    // 算法4：蒙特卡洛故障注入仿真
    std::vector<TrendProbability> run_mc_simulations(
        int n_sim,                    // 仿真次数
        int k_cycles,                 // 周期数
        const std::vector<std::vector<bool>>& input_sequence, // 输入序列
        double fault_prob = 0.01,     // 故障概率
        const std::vector<int>& low_priority_nodes = {}); // 低优先级节点
    
    // 获取趋势概率向量
    std::unordered_map<int, std::pair<double, double>> 
    get_low_priority_trend_vectors(int k_cycles, double priority_threshold = 0.1);
    
    // 计算节点优先级（基于论文公式3-7）
    std::unordered_map<int, double> calculate_gate_priorities(int k_cycles = 1);
    
    // 保存结果
    void save_results(const std::string& filename);
    
    // 加载结果
    void load_results(const std::string& filename);
    
private:
    CircuitReliabilitySimulator& simulator_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;
    
    // 仿真结果存储
    std::unordered_map<int, std::vector<FaultStatistics>> statistics_;
    
    // 趋势概率向量
    std::unordered_map<int, std::vector<TrendProbability>> trend_probabilities_;
    
    // 辅助函数
    void inject_random_faults(FaultInjector& fault_injector, double fault_prob);
    std::vector<bool> simulate_cycle_with_faults(
        const std::vector<bool>& inputs,
        FaultInjector& fault_injector,
        int cycle);
    
    // 优先级计算辅助函数
    double calculate_py_pre(int node_id, int cycle);
    double calculate_py_suc(int node_id, int cycle);
    std::unordered_map<int, double> get_fanout_source_lengths();
    std::unordered_map<int, int> get_topological_distances();
};

#endif // MC_FAULT_SIMULATOR_H