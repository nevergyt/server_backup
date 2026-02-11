
#ifndef VCD_PARSER_HPP
#define VCD_PARSER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <algorithm>

// ==================== 基础数据结构 ====================

enum class VCDValue { VCD_0, VCD_1, VCD_X, VCD_Z, VCD_ERROR };

inline VCDValue char_to_vcd_value(char c) {
    switch (c) {
        case '0': return VCDValue::VCD_0;
        case '1': return VCDValue::VCD_1;
        case 'x': case 'X': return VCDValue::VCD_X;
        case 'z': case 'Z': return VCDValue::VCD_Z;
        default: return VCDValue::VCD_ERROR;
    }
}

inline std::string vcd_value_to_string(VCDValue val) {
    switch (val) {
        case VCDValue::VCD_0: return "0";
        case VCDValue::VCD_1: return "1";
        case VCDValue::VCD_X: return "X";
        case VCDValue::VCD_Z: return "Z";
        default: return "E";
    }
}

struct VCDSignal {
    std::string identifier;      // VCD标识符，如"!", "#", "$"等
    std::string name;            // 完整信号名（包含层次路径）
    std::string basename;        // 基础信号名
    std::string reference;       // 引用名
    std::string scope;           // 作用域路径
    int width;                   // 位宽
    std::string type;            // 类型: "wire", "reg", "integer"
    
    VCDSignal() : width(1) {}
    
    std::string toString() const {
        std::ostringstream oss;
        oss << scope << "." << basename << " (ID: '" << identifier << "', Type: " 
            << type << ", Width: " << width << ")";
        return oss.str();
    }
    
    // 获取完整层次化名称
    std::string getFullName() const {
        if (scope.empty()) return basename;
        return scope + "." + basename;
    }
};

struct VCDValueChange {
    uint64_t timestamp;
    std::string identifier;
    VCDValue value;
    std::string vector_value;
    bool is_vector;
    
    VCDValueChange() : timestamp(0), value(VCDValue::VCD_X), is_vector(false) {}
};

struct VCDWaveformSample {
    uint64_t timestamp;
    std::unordered_map<std::string, VCDValue> signals;
    
    VCDWaveformSample(uint64_t ts = 0) : timestamp(ts) {}
    
    void addSignal(const std::string& id, VCDValue val) {
        signals[id] = val;
    }
    
    VCDValue getSignal(const std::string& id) const {
        auto it = signals.find(id);
        return it != signals.end() ? it->second : VCDValue::VCD_X;
    }
};

struct VCDCycle {
    int cycle_number;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t sampling_time;
    std::map<std::string, VCDValue> outputs;
    
    VCDCycle(int num = 0) : cycle_number(num), start_time(0), end_time(0), sampling_time(0) {}
    
    void addOutput(const std::string& signal, VCDValue value) {
        outputs[signal] = value;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Cycle " << cycle_number << ": [" << start_time << "-" << end_time << "]";
        for (const auto& kv : outputs) {
            oss << " " << kv.first << "=" << vcd_value_to_string(kv.second);
        }
        return oss.str();
    }
};

struct VCDIdealOutputVector {
    std::string node_name;
    int cycle;
    std::vector<double> prob_0;
    std::vector<double> prob_1;
    
    VCDIdealOutputVector(const std::string& name = "", int cyc = 0, int width = 1) 
        : node_name(name), cycle(cyc) {
        prob_0.resize(width, 0.0);
        prob_1.resize(width, 0.0);
    }
    
    void fromDeterministicValue(VCDValue val) {
        for (size_t i = 0; i < prob_0.size(); i++) {
            switch (val) {
                case VCDValue::VCD_0: prob_0[i] = 1.0; prob_1[i] = 0.0; break;
                case VCDValue::VCD_1: prob_0[i] = 0.0; prob_1[i] = 1.0; break;
                default: prob_0[i] = 0.5; prob_1[i] = 0.5; break;
            }
        }
    }
};

// ==================== VCD解析器主类 ====================

class VCDParser {
private:
    std::string filename_;
    std::ifstream file_;
    
    std::string date_;
    std::string version_;
    std::string timescale_;
    uint64_t timescale_multiplier_;
    
    // 信号存储
    std::unordered_map<std::string, VCDSignal> signals_by_id_;
    std::unordered_map<std::string, VCDSignal> signals_by_fullname_;
    
    // 值变化
    std::map<uint64_t, std::vector<VCDValueChange>> value_changes_;
    std::vector<VCDWaveformSample> waveform_;
    std::vector<VCDCycle> cycles_;
    
    // 配置
    std::string clock_signal_id_;
    VCDValue clock_active_edge_;
    VCDValue clock_inactive_state_;
    std::vector<std::string> output_signal_ids_;
    
    // 解析状态
    uint64_t current_timestamp_;
    std::vector<std::string> current_scope_;  // 当前作用域栈
    
    // 临时存储
    std::string current_date_content_;
    std::string current_version_content_;
    std::string current_timescale_content_;
    
    enum class ParserState {
        INITIAL,
        IN_DATE,
        IN_VERSION,
        IN_TIMESCALE,
        IN_SCOPE,
        IN_VAR,
        IN_DUMPVARS,
        IN_COMMENT,
        IN_BODY
    };
    
    ParserState current_state_;
    
    // ==================== 辅助函数 ====================
    
    void reset() {
        date_.clear();
        version_.clear();
        timescale_.clear();
        timescale_multiplier_ = 1;
        
        signals_by_id_.clear();
        signals_by_fullname_.clear();
        value_changes_.clear();
        waveform_.clear();
        cycles_.clear();
        
        clock_signal_id_.clear();
        output_signal_ids_.clear();
        current_scope_.clear();
        
        current_timestamp_ = 0;
        current_state_ = ParserState::INITIAL;
        
        current_date_content_.clear();
        current_version_content_.clear();
        current_timescale_content_.clear();
    }
    
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
    
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return result;
    }
    
    // 解析时间单位
    void parseTimescale(const std::string& str) {
        timescale_ = str;
        
        // 提取数字和单位
        std::string s = trim(str);
        size_t i = 0;
        
        // 跳过空白
        while (i < s.length() && std::isspace(s[i])) i++;
        
        // 提取数字
        size_t num_start = i;
        while (i < s.length() && std::isdigit(s[i])) i++;
        std::string num_str = s.substr(num_start, i - num_start);
        int num = num_str.empty() ? 1 : std::stoi(num_str);
        
        // 跳过空白
        while (i < s.length() && std::isspace(s[i])) i++;
        
        // 提取单位
        std::string unit = s.substr(i);
        
        // 转换为皮秒
        if (unit.find("fs") != std::string::npos) {
            timescale_multiplier_ = num;
        } else if (unit.find("ps") != std::string::npos) {
            timescale_multiplier_ = num * 1000;
        } else if (unit.find("ns") != std::string::npos) {
            timescale_multiplier_ = num * 1000000;
        } else if (unit.find("us") != std::string::npos) {
            timescale_multiplier_ = num * 1000000000;
        } else if (unit.find("ms") != std::string::npos) {
            timescale_multiplier_ = num * 1000000000000LL;
        } else if (unit.find("s") != std::string::npos) {
            timescale_multiplier_ = num * 1000000000000000LL;
        } else {
            timescale_multiplier_ = num;  // 默认ps
        }
        
        // 调试输出已注释
        // std::cout << "时间单位: " << timescale_ << " (乘数: " 
        //           << timescale_multiplier_ << " ps)" << std::endl;
    }
    
    // 获取当前作用域路径
    std::string getCurrentScopePath() const {
        if (current_scope_.empty()) return "";
        
        std::string path;
        for (const auto& scope : current_scope_) {
            if (!path.empty()) path += ".";
            path += scope;
        }
        return path;
    }
    
    // 解析变量定义行
    void parseVarLine(const std::string& line) {
        // 格式: $var type width identifier reference $end
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        
        while (iss >> token) {
            tokens.push_back(token);
        }
        
        if (tokens.size() < 6) {
            std::cerr << "警告：变量定义格式错误: " << line << std::endl;
            return;
        }
        
        if (tokens[0] != "$var") {
            return;
        }
        
        std::string type = tokens[1];
        int width = 1;
        try {
            width = std::stoi(tokens[2]);
        } catch (...) {
            std::cerr << "警告：无法解析宽度: " << tokens[2] << std::endl;
        }
        
        std::string identifier = tokens[3];
        
        // 构建引用名（可能是多个单词）
        std::string reference;
        for (size_t i = 4; i < tokens.size(); i++) {
            if (tokens[i] == "$end") break;
            if (!reference.empty()) reference += " ";
            reference += tokens[i];
        }
        
        VCDSignal signal;
        signal.identifier = identifier;
        signal.reference = reference;
        signal.basename = reference;
        signal.scope = getCurrentScopePath();
        signal.width = width;
        signal.type = type;
        signal.name = signal.getFullName();
        
        // 存储信号
        signals_by_id_[identifier] = signal;
        signals_by_fullname_[signal.name] = signal;
        
        // 调试输出已注释
        // std::cout << "解析信号: " << signal.toString() << std::endl;
    }
    
    // 解析初始值
    void parseInitialValue(const std::string& line) {
        if (line.empty()) return;
        
        if (line[0] == 'b' || line[0] == 'B') {
            // 二进制向量值
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos) {
                std::string value_str = line.substr(1, space_pos - 1);
                std::string identifier = trim(line.substr(space_pos + 1));
                
                VCDValueChange change;
                change.timestamp = 0;
                change.identifier = identifier;
                change.vector_value = value_str;
                change.is_vector = true;
                
                if (!value_str.empty()) {
                    change.value = char_to_vcd_value(value_str[0]);
                }
                
                value_changes_[0].push_back(change);
            }
        } else if (line[0] == 'r' || line[0] == 'R') {
            // 实数，跳过
        } else if (line[0] == 's' || line[0] == 'S') {
            // 字符串，跳过
        } else {
            // 标量值
            if (line.length() > 1) {
                char value_char = line[0];
                std::string identifier = trim(line.substr(1));
                
                VCDValueChange change;
                change.timestamp = 0;
                change.identifier = identifier;
                change.value = char_to_vcd_value(value_char);
                change.is_vector = false;
                
                value_changes_[0].push_back(change);
            }
        }
    }
    
    // 解析值变化
    bool parseValueChange(const std::string& line) {
        if (line.empty()) return true;
        
        // 时间戳
        if (line[0] == '#') {
            std::string ts_str = line.substr(1);
            try {
                current_timestamp_ = std::stoull(ts_str) * timescale_multiplier_;
            } catch (...) {
                std::cerr << "错误：无法解析时间戳: " << line << std::endl;
                return false;
            }
            return true;
        }
        
        // 值变化
        if (line[0] == 'b' || line[0] == 'B') {
            // 二进制向量
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos) {
                std::string value_str = line.substr(1, space_pos - 1);
                std::string identifier = trim(line.substr(space_pos + 1));
                
                VCDValueChange change;
                change.timestamp = current_timestamp_;
                change.identifier = identifier;
                change.vector_value = value_str;
                change.is_vector = true;
                
                if (!value_str.empty()) {
                    change.value = char_to_vcd_value(value_str[0]);
                }
                
                value_changes_[current_timestamp_].push_back(change);
            }
        } else if (line[0] == 'r' || line[0] == 'R') {
            // 实数，跳过
        } else if (line[0] == 's' || line[0] == 'S') {
            // 字符串，跳过
        } else {
            // 标量值
            if (line.length() > 1) {
                char value_char = line[0];
                std::string identifier = trim(line.substr(1));
                
                VCDValueChange change;
                change.timestamp = current_timestamp_;
                change.identifier = identifier;
                change.value = char_to_vcd_value(value_char);
                change.is_vector = false;
                
                value_changes_[current_timestamp_].push_back(change);
            }
        }
        
        return true;
    }
    
    // 处理VCD命令
    void processCommand(const std::string& line) {
        std::string trimmed = trim(line);
        
        if (trimmed.empty()) return;
        
        if (trimmed == "$date") {
            current_state_ = ParserState::IN_DATE;
        } else if (trimmed == "$end") {
            switch (current_state_) {
                case ParserState::IN_DATE:
                    date_ = current_date_content_;
                    current_date_content_.clear();
                    current_state_ = ParserState::INITIAL;
                    break;
                case ParserState::IN_VERSION:
                    version_ = current_version_content_;
                    current_version_content_.clear();
                    current_state_ = ParserState::INITIAL;
                    break;
                case ParserState::IN_TIMESCALE:
                    timescale_ = current_timescale_content_;
                    parseTimescale(timescale_);
                    current_timescale_content_.clear();
                    current_state_ = ParserState::INITIAL;
                    break;
                case ParserState::IN_SCOPE:
                    current_state_ = ParserState::INITIAL;
                    break;
                case ParserState::IN_VAR:
                    current_state_ = ParserState::INITIAL;
                    break;
                case ParserState::IN_COMMENT:
                    current_state_ = ParserState::INITIAL;
                    break;
                default:
                    current_state_ = ParserState::INITIAL;
            }
        } else if (trimmed == "$version") {
            current_state_ = ParserState::IN_VERSION;
        } else if (trimmed == "$timescale") {
            current_state_ = ParserState::IN_TIMESCALE;
        } else if (trimmed.find("$scope") == 0) {
            // 格式: $scope module uut $end
            std::istringstream iss(trimmed);
            std::string token;
            std::vector<std::string> tokens;
            while (iss >> token) tokens.push_back(token);
            
                if (tokens.size() >= 3) {
                // tokens[1] 是类型 (如 "module")
                // tokens[2] 是作用域名
                current_scope_.push_back(tokens[2]);
                // 调试输出已注释
                // std::cout << "进入作用域: " << getCurrentScopePath() << std::endl;
            }
            current_state_ = ParserState::IN_SCOPE;
        } else if (trimmed.find("$upscope") == 0) {
            if (!current_scope_.empty()) {
                // 调试输出已注释
                // std::cout << "退出作用域: " << current_scope_.back() << std::endl;
                current_scope_.pop_back();
            }
            current_state_ = ParserState::IN_SCOPE;
        } else if (trimmed.find("$var") == 0) {
            parseVarLine(trimmed);
            current_state_ = ParserState::IN_VAR;
        } else if (trimmed.find("$comment") == 0) {
            current_state_ = ParserState::IN_COMMENT;
        } else if (trimmed.find("$enddefinitions") == 0) {
            // 调试输出已注释
            // std::cout << "定义部分结束，开始解析值变化" << std::endl;
            current_state_ = ParserState::IN_BODY;
        } else if (trimmed.find("$dumpvars") == 0) {
            current_state_ = ParserState::IN_DUMPVARS;
            // 调试输出已注释
            // std::cout << "开始解析初始值..." << std::endl;
        } else {
            // 其他命令，根据当前状态处理
            switch (current_state_) {
                case ParserState::IN_DATE:
                    current_date_content_ += " " + trimmed;
                    break;
                case ParserState::IN_VERSION:
                    current_version_content_ += " " + trimmed;
                    break;
                case ParserState::IN_TIMESCALE:
                    current_timescale_content_ += " " + trimmed;
                    break;
                default:
                    // 忽略
                    break;
            }
        }
    }
    
    // 解析值变化主体部分
    bool parseBodyLine(const std::string& line) {
        if (line.empty()) return true;
        
        // 检查是否是新命令
        if (line[0] == '$') {
            if (line.find("$dumpvars") == 0) {
                current_state_ = ParserState::IN_DUMPVARS;
            } else if (line.find("$comment") == 0) {
                // 跳过注释
                current_state_ = ParserState::IN_COMMENT;
            } else if (line.find("$end") == 0) {
                // 结束当前段
                if (current_state_ == ParserState::IN_DUMPVARS) {
                    current_state_ = ParserState::IN_BODY;
                } else if (current_state_ == ParserState::IN_COMMENT) {
                    current_state_ = ParserState::IN_BODY;
                }
            }
            return true;
        }
        
        // 根据当前状态处理
        switch (current_state_) {
            case ParserState::IN_DUMPVARS:
                parseInitialValue(line);
                break;
            case ParserState::IN_BODY:
                return parseValueChange(line);
            default:
                // 忽略
                break;
        }
        
        return true;
    }

public:
    VCDParser() : timescale_multiplier_(1),
                  clock_active_edge_(VCDValue::VCD_1),
                  clock_inactive_state_(VCDValue::VCD_0),
                  current_timestamp_(0),
                  current_state_(ParserState::INITIAL) {}
    
    ~VCDParser() {
        if (file_.is_open()) file_.close();
    }
    
    // ==================== 主要接口 ====================
    
    bool parseFile(const std::string& filename) {
        filename_ = filename;
        file_.open(filename);
        if (!file_.is_open()) {
            std::cerr << "错误：无法打开VCD文件: " << filename << std::endl;
            return false;
        }
        
        // 调试输出已注释
        // std::cout << "开始解析VCD文件: " << filename << std::endl;
        reset();
        
        std::string line;
        bool in_definitions = true;
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim(line);
            
            if (in_definitions) {
                // 定义部分
                if (trimmed.empty()) continue;
                
                if (trimmed[0] == '$') {
                    processCommand(trimmed);
                    
                    // 检查是否结束定义部分
                    if (current_state_ == ParserState::IN_BODY) {
                        in_definitions = false;
                        // 调试输出已注释
                        // std::cout << "定义部分解析完成" << std::endl;
                    }
                } else {
                    // 非命令行的处理
                    switch (current_state_) {
                        case ParserState::IN_DATE:
                            current_date_content_ += " " + trimmed;
                            break;
                        case ParserState::IN_VERSION:
                            current_version_content_ += " " + trimmed;
                            break;
                        case ParserState::IN_TIMESCALE:
                            current_timescale_content_ += " " + trimmed;
                            break;
                        default:
                            // 忽略
                            break;
                    }
                }
            } else {
                // 值变化部分
                if (!parseBodyLine(trimmed)) {
                    std::cerr << "解析值变化失败: " << line << std::endl;
                    return false;
                }
            }
        }
        
        // 调试输出已注释
        // std::cout << "\nVCD文件解析完成" << std::endl;
        // std::cout << "找到 " << signals_by_id_.size() << " 个信号" << std::endl;
        // std::cout << "记录 " << value_changes_.size() << " 个时间戳的变化" << std::endl;
        
        // 显示所有信号
        // std::cout << "\n========== 所有信号 ==========" << std::endl;
        // for (const auto& kv : signals_by_fullname_) {
        //     std::cout << kv.second.toString() << std::endl;
        // }
        // std::cout << "===============================\n" << std::endl;
        
        // 自动重建波形数据
        // std::cout << "自动重建波形数据..." << std::endl;
        if (!reconstructWaveform()) {
            // std::cerr << "警告：波形数据重建失败，可能影响后续功能" << std::endl;
        } else {
            // std::cout << "波形数据重建完成，共 " << waveform_.size() << " 个采样点" << std::endl;
        }
        
        return true;
    }

    // ==================== 配置方法 ====================
    
    bool setClockSignal(const std::string& signal_name) {
        // 尝试精确匹配
        auto it = signals_by_fullname_.find(signal_name);
        if (it == signals_by_fullname_.end()) {
            // 尝试部分匹配
            for (const auto& kv : signals_by_fullname_) {
                if (kv.first.find(signal_name) != std::string::npos ||
                    kv.second.basename.find(signal_name) != std::string::npos) {
                    clock_signal_id_ = kv.second.identifier;
                    // std::cout << "设置时钟信号: " << kv.first 
                    //           << " (ID: " << clock_signal_id_ << ")" << std::endl;
                    return true;
                }
            }
            std::cerr << "错误：未找到时钟信号: " << signal_name << std::endl;
            return false;
        }
        
        clock_signal_id_ = it->second.identifier;
        // std::cout << "设置时钟信号: " << signal_name 
        //           << " (ID: " << clock_signal_id_ << ")" << std::endl;
        return true;
    }

    bool getIdealOutputForNode(const std::string& node_name, int cycle_num, 
                           std::vector<double>& prob_0, 
                           std::vector<double>& prob_1) const {
        // 查找节点对应的信号
        std::string signal_name;
                            
        // 尝试直接匹配
        auto it = signals_by_fullname_.find(node_name);
        if (it != signals_by_fullname_.end()) {
            signal_name = node_name;
        } else {
            // 尝试多种格式匹配
            std::vector<std::string> possible_patterns;

            // 1. 原始名称
            possible_patterns.push_back(node_name);

            // 2. 添加下划线格式（如signal9 -> signal_9）
            if (node_name.find("signal") == 0 && node_name.find("_") == std::string::npos) {
                std::string with_underscore = "signal_" + node_name.substr(6);
                possible_patterns.push_back(with_underscore);
            }

            // 3. 可能带有uut.前缀
            possible_patterns.push_back("uut." + node_name);
            if (node_name.find("signal") == 0 && node_name.find("_") == std::string::npos) {
                possible_patterns.push_back("uut.signal_" + node_name.substr(6));
            }

            // 4. 尝试部分匹配
            for (const auto& kv : signals_by_fullname_) {
                const std::string& full_name = kv.first;

                // 基础名匹配
                if (kv.second.basename == node_name) {
                    signal_name = full_name;
                    break;
                }

                // 部分匹配
                if (full_name.find(node_name) != std::string::npos) {
                    signal_name = full_name;
                    break;
                }

                // 基础名部分匹配（如signal9匹配signal_9）
                if (node_name.find("signal") == 0 && 
                    kv.second.basename.find("signal_") == 0) {
                    // 提取数字部分
                    std::string node_num = node_name.substr(6);
                    std::string sig_num = kv.second.basename.substr(7);
                    
                    if (node_num == sig_num) {
                        signal_name = full_name;
                        break;
                    }
                }
            }
        }

        if (signal_name.empty()) {
            // std::cerr << "错误：未找到节点 " << node_name << " 对应的信号" << std::endl;

            // // 显示所有可能的信号以供调试
            // std::cerr << "可用信号列表:" << std::endl;
            // for (const auto& kv : signals_by_fullname_) {
            //     if (kv.second.basename.find("signal") == 0 ||
            //         kv.second.basename.find("po") == 0) {
            //         std::cerr << "  " << kv.first << " (basename: " 
            //                   << kv.second.basename << ")" << std::endl;
            //     }
            // }
            return false;
        }

        // 查找周期
        for (const auto& cycle : cycles_) {
            if (cycle.cycle_number == cycle_num) {
                auto it = cycle.outputs.find(signal_name);
                if (it != cycle.outputs.end()) {
                    VCDValue val = it->second;

                    // 获取信号位宽
                    int width = 1;
                    auto sig_it = signals_by_fullname_.find(signal_name);
                    if (sig_it != signals_by_fullname_.end()) {
                        width = sig_it->second.width;
                    }

                    // 分配空间
                    prob_0.resize(width, 0.0);
                    prob_1.resize(width, 0.0);

                    // 设置概率值
                    for (int i = 0; i < width; i++) {
                        switch (val) {
                            case VCDValue::VCD_0:
                                prob_0[i] = 1.0;
                                prob_1[i] = 0.0;
                                break;
                            case VCDValue::VCD_1:
                                prob_0[i] = 0.0;
                                prob_1[i] = 1.0;
                                break;
                            case VCDValue::VCD_X:
                            case VCDValue::VCD_Z:
                                prob_0[i] = 0.5;
                                prob_1[i] = 0.5;
                                break;
                            default:
                                prob_0[i] = 0.5;
                                prob_1[i] = 0.5;
                        }
                    }

                    // std::cout << "成功获取节点 " << node_name << " -> " 
                    //           << signal_name << " 在周期 " << cycle_num 
                    //           << " 的值: " << vcd_value_to_string(val) << std::endl;
                    return true;
                }
                break;
            }
        }

        std::cerr << "错误：未找到节点 " << node_name 
                  << " (映射为 " << signal_name << ") 在周期 " 
                  << cycle_num << " 的输出" << std::endl;
        return false;
    }

    bool getPOOutputProbability(int po_index, int cycle_num,
                           std::vector<double>& prob_0,
                           std::vector<double>& prob_1) {
        // 构建可能的PO信号名
        std::vector<std::string> possible_names;
                            
        // 格式1: poX（如po0）
        possible_names.push_back("po" + std::to_string(po_index));
                            
        // 格式2: po_X
        possible_names.push_back("po_" + std::to_string(po_index));
                            
        // 格式3: 可能有uut.前缀
        possible_names.push_back("uut.po" + std::to_string(po_index));
        possible_names.push_back("uut.po_" + std::to_string(po_index));
                            
        // 格式4: 对于signalX格式
        possible_names.push_back("signal" + std::to_string(po_index));
        possible_names.push_back("signal_" + std::to_string(po_index));
        possible_names.push_back("uut.signal" + std::to_string(po_index));
        possible_names.push_back("uut.signal_" + std::to_string(po_index));
                                                      
        // 尝试所有可能的名称
        for (const auto& name : possible_names) {
            if (getIdealOutputForNode(name, cycle_num, prob_0, prob_1)) {
                // std::cout << "使用名称 '" << name << "' 成功获取PO" 
                //           << po_index << " 在周期 " << cycle_num 
                //           << " 的输出" << std::endl;
                return true;
            }
        }

        // 如果以上都失败，尝试在信号列表中查找匹配的
        // std::cout << "尝试在信号列表中查找PO" << po_index << "..." << std::endl;

        // 查找包含"po"或"cout"的信号
        for (const auto& kv : signals_by_fullname_) {
            const std::string& full_name = kv.first;
            const std::string& basename = kv.second.basename;

            // 检查是否是PO信号
            bool is_po_signal = false;

            // 检查基础名
            if (basename == "po" + std::to_string(po_index) ||
                basename == "po_" + std::to_string(po_index) ||
                basename == "signal_" + std::to_string(po_index) ||
                basename == "cout_" + std::to_string(po_index)) {
                is_po_signal = true;
            }

            // 检查是否是主输出（基于截图）
            if ((basename.find("po") == 0 || basename.find("cout") == 0) && 
                po_index == 0) {
                // 如果只有一个PO输出，使用第一个找到的PO/cout信号
                is_po_signal = true;
            }

            if (is_po_signal) {
                // std::cout << "找到可能的PO信号: " << full_name << std::endl;
                if (getIdealOutputForNode(full_name, cycle_num, prob_0, prob_1)) {
                    return true;
                }
            }
        }

        std::cerr << "错误：无法找到PO" << po_index << " 在周期 " 
                  << cycle_num << " 的输出" << std::endl;
        return false;
    }
    

    /**
 * 直接从波形获取PO输出在特定周期的值
 * @param po_index PO索引
 * @param cycle_num 周期编号
 * @param prob_0 输出0的概率
 * @param prob_1 输出1的概率
 * @return 是否成功
 */
    bool getPOOutputFromWaveform(int po_index, int cycle_num,
                                std::vector<double>& prob_0,
                                std::vector<double>& prob_1) {
        // 检查波形数据是否为空，如果为空则尝试重建
        if (waveform_.empty()) {
            // std::cout << "警告：波形数据为空，尝试重建..." << std::endl;
            if (!reconstructWaveform()) {
                // std::cerr << "错误：无法重建波形数据" << std::endl;
                return false;
            }
        }
        
        // 如果cycles_为空但waveform_不为空，尝试提取周期
        if (cycles_.empty() && !waveform_.empty()) {
            // std::cout << "警告：周期数据为空，尝试提取..." << std::endl;
            if (!extractClockCycles()) {
                // std::cerr << "错误：无法提取时钟周期" << std::endl;
                return false;
            }
        }
        
        if (cycles_.empty()) {
            std::cerr << "错误：没有周期数据" << std::endl;
            return false;
        }
        
        // 查找指定周期
        const VCDCycle* target_cycle = nullptr;
        for (const auto& cycle : cycles_) {
            if (cycle.cycle_number == cycle_num) {
                target_cycle = &cycle;
                break;
            }
        }
        
        if (!target_cycle) {
            std::cerr << "错误：未找到周期 " << cycle_num << std::endl;
            return false;
        }
        
        // 查找PO信号的标识符
        std::vector<std::string> possible_po_names = {
            "po" + std::to_string(po_index),
            "po_" + std::to_string(po_index)
        };
        
        // 如果有uut前缀，也检查
        std::vector<std::string> possible_po_names_with_uut;
        for (const auto& name : possible_po_names) {
            possible_po_names_with_uut.push_back("uut." + name);
            possible_po_names_with_uut.push_back("tb_top.uut." + name);
        }
        
        possible_po_names.insert(possible_po_names.end(), 
                                possible_po_names_with_uut.begin(), 
                                possible_po_names_with_uut.end());
        
        std::string po_identifier;
        for (const auto& po_name : possible_po_names) {
            // 查找信号
            for (const auto& kv : signals_by_id_) {
                if (kv.second.name.find(po_name) != std::string::npos) {
                    po_identifier = kv.first;
                    // std::cout << "找到PO信号: " << kv.second.name 
                    //           << " (ID: " << po_identifier << ")" << std::endl;
                    break;
                }
            }
            if (!po_identifier.empty()) break;
        }
        
        if (po_identifier.empty()) {
            // 尝试更宽松的匹配
            // std::cout << "尝试更宽松的信号匹配..." << std::endl;
            for (const auto& kv : signals_by_id_) {
                const std::string& name = kv.second.name;
                const std::string& basename = kv.second.basename;
                
                // 检查是否是PO信号
                if (basename.find("po") == 0 || basename.find("signal_") == 0) {
                    // 提取数字
                    std::string num_str;
                    if (basename.find("po") == 0) {
                        num_str = basename.substr(2);
                    } else if (basename.find("signal_") == 0) {
                        num_str = basename.substr(7);
                    }
                    
                    try {
                        int signal_num = std::stoi(num_str);
                        if (signal_num == po_index) {
                            po_identifier = kv.first;
                            // std::cout << "通过宽松匹配找到PO信号: " << name 
                            //           << " (ID: " << po_identifier << ")" << std::endl;
                            break;
                        }
                    } catch (...) {
                        // 转换失败，继续
                    }
                }
            }
        }
        
        if (po_identifier.empty()) {
            std::cerr << "错误：未找到PO" << po_index << " 的信号" << std::endl;
            
            // 显示所有可能的PO信号以供调试
            // std::cout << "可用信号列表 (可能包含PO):" << std::endl;
            for (const auto& kv : signals_by_fullname_) {
                if (kv.second.basename.find("po") == 0 || 
                    kv.second.basename.find("signal_") == 0) {
                    // std::cout << "  " << kv.first << " (basename: " 
                    //           << kv.second.basename << ")" << std::endl;
                }
            }
            return false;
        }
        
        // 查找周期采样时间点的波形
        uint64_t sample_time = target_cycle->sampling_time;
        const VCDWaveformSample* sample_at_time = nullptr;
        
        // 找到最接近的采样点
        uint64_t min_diff = UINT64_MAX;
        for (const auto& sample : waveform_) {
            uint64_t diff = (sample.timestamp > sample_time) ? 
                           (sample.timestamp - sample_time) : 
                           (sample_time - sample.timestamp);
            if (diff < min_diff) {
                min_diff = diff;
                sample_at_time = &sample;
            }
            
            // 如果找到精确匹配，直接使用
            if (sample.timestamp == sample_time) {
                sample_at_time = &sample;
                break;
            }
        }
        
        if (!sample_at_time) {
            // 使用最后一个波形点
            sample_at_time = &waveform_.back();
        }

        // 获取信号值
        VCDValue val = sample_at_time->getSignal(po_identifier);

        // 获取信号位宽
        int width = 1;
        auto sig_it = signals_by_id_.find(po_identifier);
        if (sig_it != signals_by_id_.end()) {
            width = sig_it->second.width;
        }

        // 设置概率
        prob_0.resize(width, 0.0);
        prob_1.resize(width, 0.0);

        for (int i = 0; i < width; i++) {
            switch (val) {
                case VCDValue::VCD_0:
                    prob_0[i] = 1.0;
                    prob_1[i] = 0.0;
                    break;
                case VCDValue::VCD_1:
                    prob_0[i] = 0.0;
                    prob_1[i] = 1.0;
                    break;
                case VCDValue::VCD_X:
                case VCDValue::VCD_Z:
                    prob_0[i] = 0.5;
                    prob_1[i] = 0.5;
                    break;
                default:
                    prob_0[i] = 0.5;
                    prob_1[i] = 0.5;
            }
        }

        // std::cout << "从波形获取 PO" << po_index << " 在周期 " << cycle_num 
        //           << " (时间 " << sample_at_time->timestamp << ") 的值: " 
        //           << vcd_value_to_string(val) << std::endl;

        return true;
    }


/**
 * 直接从波形获取所有节点在特定周期的输出值
 * @param cycle_num 周期编号
 * @param node_outputs 存储所有节点输出值的映射 [节点标识符 -> (prob_0, prob_1)]
 * @return 是否成功
 */
bool getAllNodeOutputsFromWaveform(int cycle_num,
                                  std::unordered_map<std::string, 
                                  std::pair<std::vector<double>, 
                                  std::vector<double>>>& node_outputs) {
    // 检查波形数据是否为空，如果为空则尝试重建
    if (waveform_.empty()) {
        // std::cout << "警告：波形数据为空，尝试重建..." << std::endl;
        if (!reconstructWaveform()) {
            // std::cerr << "错误：无法重建波形数据" << std::endl;
            return false;
        }
    }
    
    // 如果cycles_为空但waveform_不为空，尝试提取周期
    if (cycles_.empty() && !waveform_.empty()) {
        // std::cout << "警告：周期数据为空，尝试提取..." << std::endl;
        if (!extractClockCycles()) {
            // std::cerr << "错误：无法提取时钟周期" << std::endl;
            return false;
        }
    }
    
    if (cycles_.empty()) {
        std::cerr << "错误：没有周期数据" << std::endl;
        return false;
    }
    
    // 查找指定周期
    const VCDCycle* target_cycle = nullptr;
    for (const auto& cycle : cycles_) {
        if (cycle.cycle_number == cycle_num) {
            target_cycle = &cycle;
            break;
        }
    }
    
    if (!target_cycle) {
        std::cerr << "错误：未找到周期 " << cycle_num << std::endl;
        return false;
    }
    
    // 查找周期采样时间点的波形
    uint64_t sample_time = target_cycle->sampling_time;
    const VCDWaveformSample* sample_at_time = nullptr;
    
    // 找到最接近的采样点
    uint64_t min_diff = UINT64_MAX;
    for (const auto& sample : waveform_) {
        uint64_t diff = (sample.timestamp > sample_time) ? 
                       (sample.timestamp - sample_time) : 
                       (sample_time - sample.timestamp);
        if (diff < min_diff) {
            min_diff = diff;
            sample_at_time = &sample;
        }
        
        // 如果找到精确匹配，直接使用
        if (sample.timestamp == sample_time) {
            sample_at_time = &sample;
            break;
        }
    }
    
    if (!sample_at_time) {
        // 使用最后一个波形点
        sample_at_time = &waveform_.back();
    }
    
    // 调试输出已注释
    // std::cout << "获取周期 " << cycle_num << " (时间 " << sample_at_time->timestamp 
    //           << ") 的所有节点输出值" << std::endl;
    
    // 清空输出映射
    node_outputs.clear();
    
    // 获取所有节点的输出值
    int processed_count = 0;
    
    // 方法1: 遍历所有已知的信号，寻找包含"signal_"特征的信号
    for (const auto& kv : signals_by_id_) {
        const std::string& signal_id = kv.first;
        const VCDSignal& signal_info = kv.second;
        
        // 检查信号是否具有"signal_"特征
        if (signal_info.basename.find("signal_") == 0) {
            // 获取信号值
            VCDValue val = sample_at_time->getSignal(signal_id);
            
            // 获取信号位宽
            int width = signal_info.width;
            
            // 创建概率向量
            std::vector<double> prob_0(width, 0.0);
            std::vector<double> prob_1(width, 0.0);
            
            // 根据值设置概率
            for (int i = 0; i < width; i++) {
                switch (val) {
                    case VCDValue::VCD_0:
                        prob_0[i] = 1.0;
                        prob_1[i] = 0.0;
                        break;
                    case VCDValue::VCD_1:
                        prob_0[i] = 0.0;
                        prob_1[i] = 1.0;
                        break;
                    case VCDValue::VCD_X:
                    case VCDValue::VCD_Z:
                        prob_0[i] = 0.5;
                        prob_1[i] = 0.5;
                        break;
                    default:
                        prob_0[i] = 0.5;
                        prob_1[i] = 0.5;
                }
            }
            
            // === 关键修复：安全地构建节点名 ===
            std::string safe_node_name;
            
            // 方法A: 从basename重建完整名称
            if (!signal_info.basename.empty() && !signal_info.scope.empty()) {
                safe_node_name = signal_info.scope + "." + signal_info.basename;
            } 
            // 方法B: 使用原始名称的C字符串重建
            else if (!signal_info.name.empty()) {
                const char* name_cstr = signal_info.name.c_str();
                safe_node_name = name_cstr;  // 这会创建一个新的、安全的字符串
            }
            // 方法C: 直接使用signal_info.name
            else {
                safe_node_name = signal_info.name;
            }
            
            // 清理节点名中的非ASCII字符
            std::string clean_name;
            for (char c : safe_node_name) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                    (c >= '0' && c <= '9') || c == '_' || c == '.') {
                    clean_name += c;
                }
            }
            
            if (!clean_name.empty()) {
                // 存储到映射中，使用安全的节点名
                node_outputs[clean_name] = std::make_pair(prob_0, prob_1);
                
                processed_count++;
                
                // 输出调试信息
                // 调试输出已注释
                // std::cout << "  " << clean_name << " (" << signal_info.basename 
                //           << "): " << vcd_value_to_string(val) 
                //           << " (宽度: " << width << ")" << std::endl;
            } else {
                std::cerr << "警告: 节点名无效，跳过: " << signal_info.name << std::endl;
            }
        }
    }
    
    // 方法2: 如果通过方法1没找到任何signal_信号，尝试从波形样本中查找
    if (node_outputs.empty()) {
        // 调试输出已注释
        // std::cout << "在signals_by_id_中未找到signal_信号，尝试从波形样本中查找..." << std::endl;
        
        // 收集可能的signal_信号名称
        std::vector<std::string> possible_signal_names;
        
        // 首先检查信号是否具有"signal_"特征
        // 检查basename以"signal_"开头的信号
        for (const auto& kv : signals_by_id_) {
            if (kv.second.basename.find("signal_") == 0) {
                possible_signal_names.push_back(kv.second.name);
            }
        }
        
        // 如果有uut前缀，也检查
        std::vector<std::string> possible_signal_names_with_uut;
        for (const auto& kv : signals_by_id_) {
            if (kv.second.basename.find("signal_") == 0) {
                possible_signal_names_with_uut.push_back("uut." + kv.second.basename);
                possible_signal_names_with_uut.push_back("tb_top.uut." + kv.second.basename);
            }
        }
        
        possible_signal_names.insert(possible_signal_names.end(), 
                                    possible_signal_names_with_uut.begin(), 
                                    possible_signal_names_with_uut.end());
        
        // 现在从波形样本中获取这些信号的值
        for (const std::string& signal_name : possible_signal_names) {
            // 查找信号标识符
            std::string signal_identifier;
            
            // 查找信号
            for (const auto& kv : signals_by_id_) {
                if (kv.second.name == signal_name) {
                    signal_identifier = kv.first;
                    break;
                }
            }
            
            // 如果没有找到，尝试从signals_by_fullname_查找
            if (signal_identifier.empty()) {
                auto it = signals_by_fullname_.find(signal_name);
                if (it != signals_by_fullname_.end()) {
                    signal_identifier = it->second.identifier;
                }
            }
            
            if (!signal_identifier.empty()) {
                VCDValue val = sample_at_time->getSignal(signal_identifier);
                
                // 查找信号信息
                int width = 1;
                
                auto sig_it = signals_by_id_.find(signal_identifier);
                if (sig_it != signals_by_id_.end()) {
                    width = sig_it->second.width;
                } else {
                    // 尝试从signals_by_fullname_查找
                    for (const auto& fullname_kv : signals_by_fullname_) {
                        if (fullname_kv.second.identifier == signal_identifier) {
                            width = fullname_kv.second.width;
                            break;
                        }
                    }
                }
                
                // 创建概率向量
                std::vector<double> prob_0(width, 0.0);
                std::vector<double> prob_1(width, 0.0);
                
                // 根据值设置概率
                for (int i = 0; i < width; i++) {
                    switch (val) {
                        case VCDValue::VCD_0:
                            prob_0[i] = 1.0;
                            prob_1[i] = 0.0;
                            break;
                        case VCDValue::VCD_1:
                            prob_0[i] = 0.0;
                            prob_1[i] = 1.0;
                            break;
                        case VCDValue::VCD_X:
                        case VCDValue::VCD_Z:
                            prob_0[i] = 0.5;
                            prob_1[i] = 0.5;
                            break;
                        default:
                            prob_0[i] = 0.5;
                            prob_1[i] = 0.5;
                    }
                }
                
                // 存储到映射中
                node_outputs[signal_name] = std::make_pair(prob_0, prob_1);
                processed_count++;
                
                // 调试输出已注释
                // std::cout << "  " << signal_name << ": " << vcd_value_to_string(val) 
                //           << " (宽度: " << width << ")" << std::endl;
            }
        }
    }
    
    // 方法3: 如果仍然没有找到，尝试直接遍历所有信号寻找signal_特征
    if (node_outputs.empty()) {
        // 调试输出已注释
        // std::cout << "正在扫描所有可用信号寻找signal_特征..." << std::endl;
        
        // 显示所有可用信号以供参考
        // std::cout << "所有可用信号列表:" << std::endl;
        int signal_count = 0;
        for (const auto& kv : signals_by_id_) {
            if (signal_count < 20) { // 只显示前20个信号
                // 调试输出已注释
                // std::cout << "  ID: " << kv.first 
                //           << ", 名称: " << kv.second.name 
                //           << ", basename: " << kv.second.basename << std::endl;
            }
            signal_count++;
        }
        if (signal_count >= 20) {
            // 调试输出已注释
            // std::cout << "  ... (共 " << signal_count << " 个信号)" << std::endl;
        }
        
        // 构建可能的signal_信号名称列表
        std::vector<std::string> possible_signal_patterns = {
            "signal_",           // 基础格式
            "uut.signal_",       // 带uut前缀
            "tb_top.uut.signal_" // 带完整层次前缀
        };
        
        // 扫描所有信号，寻找匹配的模式
        for (const auto& kv : signals_by_id_) {
            const std::string& signal_id = kv.first;
            const VCDSignal& signal_info = kv.second;
            
            // 检查是否匹配任何signal_模式
            bool is_signal = false;
            for (const std::string& pattern : possible_signal_patterns) {
                // 检查完整名称是否以模式开头
                if (signal_info.name.find(pattern) == 0) {
                    is_signal = true;
                    break;
                }
            }
            
            if (is_signal) {
                VCDValue val = sample_at_time->getSignal(signal_id);
                
                // 获取信号位宽
                int width = signal_info.width;
                
                // 创建概率向量
                std::vector<double> prob_0(width, 0.0);
                std::vector<double> prob_1(width, 0.0);
                
                // 根据值设置概率
                for (int i = 0; i < width; i++) {
                    switch (val) {
                        case VCDValue::VCD_0:
                            prob_0[i] = 1.0;
                            prob_1[i] = 0.0;
                            break;
                        case VCDValue::VCD_1:
                            prob_0[i] = 0.0;
                            prob_1[i] = 1.0;
                            break;
                        case VCDValue::VCD_X:
                        case VCDValue::VCD_Z:
                            prob_0[i] = 0.5;
                            prob_1[i] = 0.5;
                            break;
                        default:
                            prob_0[i] = 0.5;
                            prob_1[i] = 0.5;
                    }
                }
                
                // 存储到映射中
                node_outputs[signal_info.name] = std::make_pair(prob_0, prob_1);
                processed_count++;
                
                // 调试输出已注释
                // std::cout << "  " << signal_info.name << " (" << signal_info.basename 
                //           << "): " << vcd_value_to_string(val) 
                //           << " (宽度: " << width << ")" << std::endl;
            }
        }
    }
    
    if (processed_count == 0) {
        // 调试输出已注释
        // std::cout << "警告：未找到任何具有'signal_'特征的信号" << std::endl;
        // 但仍然返回true，因为函数成功执行了，只是没有找到匹配的信号
    } else {
        // 调试输出已注释
        // std::cout << "成功获取 " << processed_count << " 个具有'signal_'特征的节点输出值" << std::endl;
    }
    
    return true;
}

    bool reconstructWaveform(double sampling_interval = 0.0) {
        if (value_changes_.empty()) {
            std::cerr << "错误：没有值变化数据" << std::endl;
            return false;
        }
        
        // 调试输出已注释
        // std::cout << "开始重建波形..." << std::endl;
        waveform_.clear();
        
        // 初始化所有信号的当前值
        std::unordered_map<std::string, VCDValue> current_values;
        for (const auto& kv : signals_by_id_) {
            current_values[kv.first] = VCDValue::VCD_X;
        }
        
        // 应用初始值
        auto initial_it = value_changes_.find(0);
        if (initial_it != value_changes_.end()) {
            for (const auto& change : initial_it->second) {
                current_values[change.identifier] = change.value;
            }
        }
        
        // 创建初始采样点（时间为0）
        if (!current_values.empty()) {
            VCDWaveformSample sample(0);
            for (const auto& cv : current_values) {
                sample.addSignal(cv.first, cv.second);
            }
            waveform_.push_back(sample);
        }
        
        // 按照时间顺序应用变化并创建采样点
        for (const auto& kv : value_changes_) {
            uint64_t timestamp = kv.first;
            if (timestamp == 0) continue;  // 初始值已处理
            
            // 应用此时间戳的所有变化
            for (const auto& change : kv.second) {
                current_values[change.identifier] = change.value;
            }
            
            // 创建采样点
            VCDWaveformSample sample(timestamp);
            for (const auto& cv : current_values) {
                sample.addSignal(cv.first, cv.second);
            }
            waveform_.push_back(sample);
        }
        
        // 调试输出已注释
        // std::cout << "重建波形完成，共 " << waveform_.size() << " 个采样点" << std::endl;
        return true;
    }




    bool addOutputSignal(const std::string& signal_name) {
        auto it = signals_by_fullname_.find(signal_name);
        if (it == signals_by_fullname_.end()) {
            // 尝试部分匹配
            for (const auto& kv : signals_by_fullname_) {
                if (kv.first.find(signal_name) != std::string::npos ||
                    kv.second.basename.find(signal_name) != std::string::npos) {
                    output_signal_ids_.push_back(kv.second.identifier);
                    // 调试输出已注释
                    // std::cout << "添加输出信号: " << kv.first << std::endl;
                    return true;
                }
            }
            std::cerr << "警告：未找到输出信号: " << signal_name << std::endl;
            return false;
        }
        
        output_signal_ids_.push_back(it->second.identifier);
        // 调试输出已注释
        // std::cout << "添加输出信号: " << signal_name << std::endl;
        return true;
    }
     
    
    bool extractClockCycles() {
        if (waveform_.empty()) {
            std::cerr << "错误：波形数据为空，请先重建波形" << std::endl;
            return false;
        }
        
        if (clock_signal_id_.empty()) {
            std::cerr << "错误：未设置时钟信号" << std::endl;
            return false;
        }
        
        // 调试输出已注释
        // std::cout << "开始提取时钟周期..." << std::endl;
        cycles_.clear();
        
        // 检测时钟边沿
        std::vector<uint64_t> clock_edges;
        VCDValue prev_clock_value = VCDValue::VCD_X;
        
        for (size_t i = 0; i < waveform_.size(); i++) {
            const auto& sample = waveform_[i];
            VCDValue clock_val = sample.getSignal(clock_signal_id_);
            
            // 检测上升沿（从0到1）
                if (prev_clock_value == VCDValue::VCD_0 && 
                clock_val == VCDValue::VCD_1) {
                clock_edges.push_back(sample.timestamp);
                // 调试输出已注释
                // std::cout << "检测到时钟上升沿 @ " << sample.timestamp << std::endl;
            }
            
            prev_clock_value = clock_val;
        }
        
        // 调试输出已注释
        // std::cout << "找到 " << clock_edges.size() << " 个时钟边沿" << std::endl;
        
        if (clock_edges.size() < 2) {
            std::cerr << "错误：时钟边沿不足，无法定义周期" << std::endl;
            return false;
        }
        
        // 根据边沿创建周期
        for (size_t i = 0; i < clock_edges.size() - 1; i++) {
            VCDCycle cycle(i + 1);
            cycle.start_time = clock_edges[i];
            cycle.end_time = clock_edges[i + 1];
            cycle.sampling_time = clock_edges[i];  // 在上升沿采样
            
            // 查找采样时间点的波形数据
            for (const auto& sample : waveform_) {
                if (sample.timestamp >= cycle.sampling_time) {
                    // 提取输出信号值
                    for (const auto& output_id : output_signal_ids_) {
                        VCDValue val = sample.getSignal(output_id);
                        const auto& signal = signals_by_id_[output_id];
                        cycle.addOutput(signal.name, val);
                    }
                    break;
                }
            }
            
            cycles_.push_back(cycle);
        }
        
        // 调试输出已注释
        // std::cout << "提取 " << cycles_.size() << " 个时钟周期" << std::endl;
        return true;
    }
    
    // ==================== 查询方法 ====================
    
    std::vector<std::string> getAllSignalNames() const {
        std::vector<std::string> names;
        for (const auto& kv : signals_by_fullname_) {
            names.push_back(kv.first);
        }
        return names;
    }
    
    std::vector<std::string> getAllSignalBaseNames() const {
        std::vector<std::string> names;
        for (const auto& kv : signals_by_fullname_) {
            names.push_back(kv.second.basename);
        }
        return names;
    }
    
    const std::vector<VCDCycle>& getCycles() const {
        return cycles_;
    }
    
    void printSummary() const {
        std::cout << "\n========== VCD文件信息摘要 ==========" << std::endl;
        std::cout << "文件: " << filename_ << std::endl;
        std::cout << "日期: " << date_ << std::endl;
        std::cout << "版本: " << version_ << std::endl;
        std::cout << "时间单位: " << timescale_ << std::endl;
        std::cout << "信号数量: " << signals_by_id_.size() << std::endl;
        std::cout << "时间变化点数: " << value_changes_.size() << std::endl;
        std::cout << "波形采样点数: " << waveform_.size() << std::endl;
        std::cout << "时钟周期数: " << cycles_.size() << std::endl;
        
        if (!clock_signal_id_.empty()) {
            auto it = signals_by_id_.find(clock_signal_id_);
            if (it != signals_by_id_.end()) {
                std::cout << "时钟信号: " << it->second.name 
                          << " (ID: " << clock_signal_id_ << ")" << std::endl;
            }
        }
        
        std::cout << "输出信号 (" << output_signal_ids_.size() << " 个):" << std::endl;
        for (const auto& id : output_signal_ids_) {
            auto it = signals_by_id_.find(id);
            if (it != signals_by_id_.end()) {
                std::cout << "  - " << it->second.name << std::endl;
            }
        }
        std::cout << "=====================================\n" << std::endl;
    }
    
    void printCycles() const {
        std::cout << "\n========== 时钟周期信息 ==========" << std::endl;
        for (const auto& cycle : cycles_) {
            std::cout << cycle.toString() << std::endl;
        }
        std::cout << "=================================\n" << std::endl;
    }
    
    
    std::vector<std::string> findMatchingSignals(const std::string& pattern) const {
        std::vector<std::string> matches;
        std::string pattern_lower = toLower(pattern);
        
        for (const auto& kv : signals_by_fullname_) {
            std::string name_lower = toLower(kv.first);
            std::string basename_lower = toLower(kv.second.basename);
            
            if (name_lower.find(pattern_lower) != std::string::npos ||
                basename_lower.find(pattern_lower) != std::string::npos) {
                matches.push_back(kv.first);
            }
        }
        
        return matches;
    }
    
    std::vector<std::string> autoDetectClockSignals() const {
        std::vector<std::string> patterns = {"clock", "clk"};
        std::vector<std::string> clocks;
        
        for (const auto& pattern : patterns) {
            auto matches = findMatchingSignals(pattern);
            clocks.insert(clocks.end(), matches.begin(), matches.end());
        }
        
        return clocks;
    }
    
    std::vector<std::string> autoDetectOutputSignals() const {
        std::vector<std::string> patterns = {"po", "out", "q", "dout", "cout"};
        std::vector<std::string> outputs;
        
        for (const auto& pattern : patterns) {
            auto matches = findMatchingSignals(pattern);
            for (const auto& match : matches) {
                // 排除明显不是输出的信号
                std::string match_lower = toLower(match);
                if (match_lower.find("input") == std::string::npos &&
                    match_lower.find("clock") == std::string::npos) {
                    outputs.push_back(match);
                }
            }
        }
        
        return outputs;
    }
    
    std::vector<VCDIdealOutputVector> getIdealOutputs() const {
        std::vector<VCDIdealOutputVector> ideal_outputs;
        
        for (const auto& cycle : cycles_) {
            for (const auto& output : cycle.outputs) {
                const std::string& signal_name = output.first;
                VCDValue value = output.second;
                
                // 查找信号定义以获取位宽
                int width = 1;
                auto it = signals_by_fullname_.find(signal_name);
                if (it != signals_by_fullname_.end()) {
                    width = it->second.width;
                }
                
                VCDIdealOutputVector iov(signal_name, cycle.cycle_number, width);
                iov.fromDeterministicValue(value);
                ideal_outputs.push_back(iov);
            }
        }
        
        return ideal_outputs;
    }
    
    // 调试函数：显示VCD文件结构
    void debugFileStructure() {
        std::ifstream debug_file(filename_);
        if (!debug_file.is_open()) {
            std::cerr << "无法打开文件" << std::endl;
            return;
        }
        
        // 调试输出已注释
        // std::cout << "\n========== VCD文件结构分析 ==========" << std::endl;
        std::string line;
        int line_num = 0;
        int var_count = 0;
        
        while (std::getline(debug_file, line) && line_num < 50) {
            line_num++;
            std::string trimmed = trim(line);
            
            if (trimmed.empty()) continue;
            
            if (trimmed[0] == '$') {
                // 调试输出已注释
                // std::cout << "行 " << line_num << ": " << trimmed << std::endl;
                
                if (trimmed.find("$var") != std::string::npos) {
                    var_count++;
                    
                    // 显示变量定义详情
                    std::istringstream iss(trimmed);
                    std::string token;
                    std::vector<std::string> tokens;
                    while (iss >> token) tokens.push_back(token);
                    
                    // 调试输出已注释
                    // std::cout << "  令牌数量: " << tokens.size() << std::endl;
                    for (size_t i = 0; i < tokens.size(); i++) {
                        // 调试输出已注释
                        // std::cout << "    token[" << i << "]: \"" << tokens[i] << "\"" << std::endl;
                    }
                }
            }
        }
        
        debug_file.close();
        // 调试输出已注释
        // std::cout << "找到 " << var_count << " 个变量定义" << std::endl;
        // std::cout << "=====================================\n" << std::endl;
    }
};

#endif // VCD_PARSER_HPP