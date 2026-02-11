#include "circuit_simulator.h"

CircuitReliabilitySimulator::CircuitReliabilitySimulator() {
    set_fault_probability(0.01);
}

bool CircuitReliabilitySimulator::read_circuit(const std::string& filename) {
    auto const result = lorina::read_aiger( filename, mockturtle::aiger_reader( circuit_ ) );
    if ( result != lorina::return_code::success )
    {
      std::cout << "Read benchmark failed\n";
      return false;
    }
    
    std::cout << "Successfully read Verilog circuit: " << filename << std::endl;
    std::cout << "  Inputs: " << get_num_inputs() << std::endl;
    std::cout << "  Outputs: " << get_num_outputs() << std::endl;
    std::cout << "  Gates: " << get_num_gates() << std::endl;
    
    return true;
}

bool CircuitReliabilitySimulator::write_verilog(const std::string& filename){
    mockturtle::write_verilog(circuit_,filename);
}


// void CircuitReliabilitySimulator::identify_registers() {
//     registers_.clear();
//     node_to_register_map_.clear();

//     //todo:
//     // 使用 Mockturtle 的 foreach_register 来识别寄存器
//     circuit_.foreach_register([&](auto const& reg) {
//         RegisterInfo reg_info(circuit_.ro_at(reg.second), reg.second);
        
//         // 存储寄存器信号
//         reg_info.data_input = reg.first;
//         reg_info.clock_input = reg.clock;
//         reg_info.output = reg.output;
        
//         // 检查是否有复位和预置信号
//         if (reg.reset != mockturtle::aig_network::signal{}) {
//             reg_info.reset = reg.reset;
//         }
//         if (reg.preset != mockturtle::aig_network::signal{}) {
//             reg_info.preset = reg.preset;
//         }
        
//         registers_.push_back(reg_info);
//         node_to_register_map_[circuit_.get_node(reg.output)] = &registers_.back();
        
//         std::cout << "Identified register: " << reg.name 
//                   << " (output node: " << circuit_.get_node(reg.output) << ")" << std::endl;
//     });
    
//     if (registers_.empty()) {
//         std::cout << "No registers found in the circuit (combinational circuit)" << std::endl;
//     } else {
//         std::cout << "Identified " << registers_.size() << " registers using Mockturtle" << std::endl;
//     }
// }

void CircuitReliabilitySimulator::initialize_sequential_simulation() {
    current_cycle_ = 0;
    node_values_.clear();
    
    // 设置初始状态
    for (auto& reg : registers_) {
        auto it = initial_register_state_.find(reg.index);
        if (it != initial_register_state_.end()) {
            reg.current_state = it->second;
        } else {
            reg.current_state = false; // 默认初始状态为0
        }
        reg.next_state = reg.current_state;
        
        // 设置寄存器输出的初始值
        auto output_node = circuit_.get_node(reg.output);
        node_values_[output_node] = reg.current_state;
    }
    
    setup_constant_nodes();
    std::cout << "Sequential simulation initialized with " 
              << registers_.size() << " registers" << std::endl;
}

void CircuitReliabilitySimulator::set_clock_sequence(const std::vector<bool>& clock_sequence) {
    clock_sequence_ = clock_sequence;
}

void CircuitReliabilitySimulator::set_initial_state(const std::unordered_map<int, bool>& initial_state) {
    initial_register_state_ = initial_state;
}

CircuitReliabilitySimulator::SimulationCycle CircuitReliabilitySimulator::simulate_sequential_cycle(
    const std::vector<bool>& inputs, int cycle) {
    
    SimulationCycle sim_cycle(cycle);
    current_cycle_ = cycle;
    
    // 生成时钟信号
    bool clock_value = generate_clock_signal(cycle);
    sim_cycle.clock_value = clock_value;
    
    // 设置主输入值
    int input_idx = 0;
    circuit_.foreach_pi([&](auto node) {
        bool value = false;
        if (input_idx < inputs.size()) {
            value = inputs[input_idx];
        }
        
        node_values_[node] = value;
        sim_cycle.primary_inputs.push_back(value);
        input_idx++;
    });
    
    // 设置寄存器的当前状态值
    for (const auto& reg : registers_) {
        auto output_node = circuit_.get_node(reg.output);
        node_values_[output_node] = reg.current_state;
        sim_cycle.register_states[reg.index] = reg.current_state;
    }
    
    // 按拓扑顺序计算组合逻辑
    // auto topo_order = sequential_topological_sort();
    // for (auto node : topo_order) {
    //     if (!circuit_.is_pi(node) && !circuit_.is_constant(node) && 
    //         !is_register_output(node)) {
    //         compute_gate_output(node);
    //     }
    // }
    
    // 更新寄存器的下一个状态
    update_register_next_states();
    
    // 收集主输出值
    circuit_.foreach_po([&](auto po) {
        auto po_node = circuit_.get_node(po);
        bool value = node_values_[po_node];
        if (circuit_.is_complemented(po)) {
            value = !value;
        }
        sim_cycle.primary_outputs.push_back(value);
    });
    
    // 收集寄存器输出值
    for (const auto& reg : registers_) {
        auto output_node = circuit_.get_node(reg.output);
        sim_cycle.register_outputs.push_back(node_values_[output_node]);
    }
    
    // 记录所有节点值
    for (const auto& [node, value] : node_values_) {
        sim_cycle.node_values[node] = value;
    }
    
    // 在时钟上升沿更新寄存器状态
    bool previous_clock = generate_clock_signal(cycle - 1);
    if (is_clock_edge(cycle)) {
        propagate_register_values();
    }
    
    return sim_cycle;
}

std::vector<CircuitReliabilitySimulator::SimulationCycle> 
CircuitReliabilitySimulator::simulate_sequential_circuit(
    const std::vector<std::vector<bool>>& input_sequence, 
    int num_cycles,
    bool reset_between_cycles) {
    
    std::vector<SimulationCycle> simulation_result;
    initialize_sequential_simulation();
    
    for (int cycle = 0; cycle < num_cycles; cycle++) {
        std::vector<bool> inputs;
        if (cycle < input_sequence.size()) {
            inputs = input_sequence[cycle];
        } else {
            // 如果没有提供输入，使用默认值
            inputs = std::vector<bool>(get_num_inputs(), false);
        }
        
        auto sim_cycle = simulate_sequential_cycle(inputs, cycle);
        simulation_result.push_back(sim_cycle);
        
        if (reset_between_cycles) {
            // 重置寄存器状态
            for (auto& reg : registers_) {
                reg.current_state = false;
            }
        }
    }
    
    std::cout << "Sequential simulation completed for " << num_cycles << " cycles" << std::endl;
    return simulation_result;
}

void CircuitReliabilitySimulator::update_register_next_states() {
    for (auto& reg : registers_) {
        if (reg.data_input != mockturtle::aig_network::signal{}) {
            auto data_node = circuit_.get_node(reg.data_input);
            auto it = node_values_.find(data_node);
            if (it != node_values_.end()) {
                reg.next_state = it->second;
            }
        }
        
        // 检查复位和预置信号
        if (reg.reset != mockturtle::aig_network::signal{}) {
            auto reset_node = circuit_.get_node(reg.reset);
            auto reset_it = node_values_.find(reset_node);
            if (reset_it != node_values_.end() && reset_it->second) {
                reg.next_state = false; // 复位有效，下一个状态为0
            }
        }
        
        if (reg.preset != mockturtle::aig_network::signal{}) {
            auto preset_node = circuit_.get_node(reg.preset);
            auto preset_it = node_values_.find(preset_node);
            if (preset_it != node_values_.end() && preset_it->second) {
                reg.next_state = true; // 预置有效，下一个状态为1
            }
        }
    }
}

CircuitReliabilitySimulator::RegisterInfo* CircuitReliabilitySimulator::get_register_by_index(int index) {
    auto it = index_to_register_map_.find(index);
    return (it != index_to_register_map_.end()) ? it->second : nullptr;
}


void CircuitReliabilitySimulator::propagate_register_values() {
    for (auto& reg : registers_) {
        reg.current_state = reg.next_state;
        auto output_node = circuit_.get_node(reg.output);
        node_values_[output_node] = reg.current_state;
    }
}

bool CircuitReliabilitySimulator::generate_clock_signal(int cycle, int phase) const {
    if (!clock_sequence_.empty()) {
        int index = cycle % clock_sequence_.size();
        return clock_sequence_[index];
    } else {
        // 生成默认时钟信号
        int clock_phase = (cycle * 2 + phase) % (clock_period_ * 2);
        return clock_phase < (clock_period_ * 2 * clock_duty_cycle_);
    }
}

bool CircuitReliabilitySimulator::is_clock_edge(int cycle) const {
    bool current_clock = generate_clock_signal(cycle);
    bool previous_clock = generate_clock_signal(cycle - 1);
    return current_clock && !previous_clock; // 检测上升沿
}

bool CircuitReliabilitySimulator::is_register_output(mockturtle::aig_network::node node) const {
    return node_to_register_map_.find(node) != node_to_register_map_.end();
}

CircuitReliabilitySimulator::RegisterInfo* CircuitReliabilitySimulator::find_register_by_output(
    mockturtle::aig_network::node output_node) {
    auto it = node_to_register_map_.find(output_node);
    return (it != node_to_register_map_.end()) ? it->second : nullptr;
}

std::unordered_map<int, bool> CircuitReliabilitySimulator::get_current_register_state() const {
    std::unordered_map<int, bool> state;
    for (const auto& reg : registers_) {
        state[reg.index] = reg.current_state;
    }
    return state;
}

void CircuitReliabilitySimulator::set_register_state(int register_index, bool state) {
    auto reg_ptr = get_register_by_index(register_index);
    if (reg_ptr) {
        reg_ptr->current_state = state;
        auto output_node = circuit_.get_node(reg_ptr->output);
        node_values_[output_node] = state;
    } else {
        std::cerr << "Warning: Register index " << register_index << " not found" << std::endl;
    }
}

std::vector<mockturtle::aig_network::node> CircuitReliabilitySimulator::get_register_outputs() const {
    std::vector<mockturtle::aig_network::node> outputs;
    for (const auto& reg : registers_) {
        outputs.push_back(circuit_.get_node(reg.output));
    }
    return outputs;
}

void CircuitReliabilitySimulator::print_registers() const {
    std::cout << "=== Registers (using Mockturtle) ===" << std::endl;
    for (size_t i = 0; i < registers_.size(); i++) {
        const auto& reg = registers_[i];
        std::cout << "Register [" << reg.index << "]" << std::endl;
        std::cout << "  Output node: " << circuit_.get_node(reg.output) << std::endl;
        std::cout << "  Data input node: " << circuit_.get_node(reg.data_input) << std::endl;
        std::cout << "  Clock input node: " << circuit_.get_node(reg.clock_input) << std::endl;
        
        if (reg.reset != mockturtle::aig_network::signal{}) {
            std::cout << "  Reset node: " << circuit_.get_node(reg.reset) << std::endl;
        }
        if (reg.preset != mockturtle::aig_network::signal{}) {
            std::cout << "  Preset node: " << circuit_.get_node(reg.preset) << std::endl;
        }
        
        std::cout << "  Current state: " << reg.current_state << std::endl;
    }
}

void CircuitReliabilitySimulator::print_simulation_state(int cycle) const {
    std::cout << "=== Simulation State (Cycle " << cycle << ") ===" << std::endl;
    
    // 打印寄存器状态
    std::cout << "Register States: ";
    for (const auto& reg : registers_) {
        std::cout << "[" << reg.index << "]=" << reg.current_state << " ";
    }
    std::cout << std::endl;
    
    // 打印主输出
    std::cout << "Primary Outputs: ";
    circuit_.foreach_po([&](auto po) {
        auto po_node = circuit_.get_node(po);
        auto it = node_values_.find(po_node);
        if (it != node_values_.end()) {
            bool value = it->second;
            if (circuit_.is_complemented(po)) {
                value = !value;
            }
            std::cout << value << " ";
        }
    });
    std::cout << std::endl;
    
    // 打印时钟状态
    std::cout << "Clock: " << generate_clock_signal(cycle) << std::endl;
}


void CircuitReliabilitySimulator::set_fault_probability(double fp) {
    fault_probabilities_.clear();
    circuit_.foreach_gate([&](auto node) {
        fault_probabilities_[node] = fp;
    });
    
}

void CircuitReliabilitySimulator::set_node_fault_probability(
    mockturtle::aig_network::node node, double fp) {
    fault_probabilities_[node] = fp;
}

void CircuitReliabilitySimulator::setup_constant_nodes() {
    try {
        // 设置常数0节点
        auto constant0_signal = circuit_.get_constant(false);
        auto constant0_node = circuit_.get_node(constant0_signal);
        if (circuit_.is_constant(constant0_node)) {
            node_values_[constant0_node] = false;
        }
        
        // 设置常数1节点（如果存在独立的节点）
        auto constant1_signal = circuit_.get_constant(true);
        auto constant1_node = circuit_.get_node(constant1_signal);
        if (circuit_.is_constant(constant1_node) && constant1_node != constant0_node) {
            node_values_[constant1_node] = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error setting up constant nodes: " << e.what() << std::endl;
    }
}

double CircuitReliabilitySimulator::get_node_fault_probability(
    mockturtle::aig_network::node node) const {
    auto it = fault_probabilities_.find(node);
    return (it != fault_probabilities_.end()) ? it->second : 0.01;
}

std::vector<bool> CircuitReliabilitySimulator::fault_free_simulation_iverilog(const std::vector<bool>& inputs){
    
}


std::vector<bool> CircuitReliabilitySimulator::fault_free_simulation(const std::vector<bool>& inputs) {
    // 对于时序电路，单次仿真使用当前状态
    node_values_.clear();
    setup_constant_nodes();
    
    // 设置输入值
    int input_idx = 0;
    circuit_.foreach_pi([&](auto node) {
        if (input_idx < inputs.size()) {
            node_values_[node] = inputs[input_idx++];
        } else {
            node_values_[node] = false;
        }
    });
    
    // 设置寄存器当前状态
    for (const auto& reg : registers_) {
        auto output_node = circuit_.get_node(reg.output);
        node_values_[output_node] = reg.current_state;
    }
    
    // 计算组合逻辑
    // auto topo_order = sequential_topological_sort();
    // for (auto node : topo_order) {
    //     if (!circuit_.is_pi(node) && !circuit_.is_constant(node) && 
    //         !is_register_output(node)) {
    //         compute_gate_output(node);
    //     }
    // }
    
    // 收集输出
    std::vector<bool> outputs;
    circuit_.foreach_po([&](auto po) {
        auto po_node = circuit_.get_node(po);
        bool value = node_values_[po_node];
        if (circuit_.is_complemented(po)) {
            value = !value;
        }
        outputs.push_back(value);
    });
    
    return outputs;
}


void CircuitReliabilitySimulator::compute_gate_output(mockturtle::aig_network::node node) {
    if (circuit_.is_constant(node)) {
        node_values_[node] = circuit_.constant_value(node);
        return;
    }
    
    if (circuit_.is_pi(node)) {
        return; 
    }
    
    // 获取所有扇入的值
    std::vector<bool> fanin_values = get_fanin_values(node);
    
    // 根据门类型计算输出
    if (circuit_.is_and(node)) {
        node_values_[node] = compute_and_gate(fanin_values);
    }
    // 可以扩展其他门类型
    else {
        // 默认为 AND 门
        node_values_[node] = compute_and_gate(fanin_values);
    }
}

bool CircuitReliabilitySimulator::compute_and_gate(const std::vector<bool>& inputs) {
    bool result = true;
    for (bool val : inputs) {
        result &= val;
    }
    return result;
}

bool CircuitReliabilitySimulator::get_fanin_value(mockturtle::aig_network::signal fanin) const {
    auto node = circuit_.get_node(fanin);
    bool value = node_values_.at(node);
    if (circuit_.is_complemented(fanin)) {
        value = !value;
    }
    return value;
}

std::vector<bool> CircuitReliabilitySimulator::get_fanin_values(
    mockturtle::aig_network::node node) const {
    
    std::vector<bool> values;
    circuit_.foreach_fanin(node, [&](auto fanin) {
        values.push_back(get_fanin_value(fanin));
    });
    return values;
}


size_t CircuitReliabilitySimulator::get_num_inputs() const {
    return circuit_.num_pis();
}

size_t CircuitReliabilitySimulator::get_num_outputs() const {
    return circuit_.num_pos();
}

size_t CircuitReliabilitySimulator::get_num_gates() const {
    return circuit_.num_gates();
}