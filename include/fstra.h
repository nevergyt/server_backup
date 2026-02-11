#pragma once

#include <mockturtle/mockturtle.hpp>
#include "iverilog_simulator.h"
#include <Eigen/Dense>
#include <vector>
#include <unsupported/Eigen/KroneckerProduct>
#include <kitty/kitty.hpp>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include "vcd_parser.h"

class FSTRAAnalyzer {
private:
    mockturtle::aig_network& circuit_;
    IverilogSimulator& simulator_;
    VCDParser& vcd_parser_;
    
    struct FSNode {
        int in;
        int out;
        int index;
        int cycle;
        Eigen::MatrixXd ptm;
        Eigen::MatrixXd iptM;
        Eigen::MatrixXd optM;
        Eigen::MatrixXd REoptM;
        std::vector<int> fsL;
        bool hasFanoutBranch;
        bool isSequential;
        std::vector<double> rel;
        
        FSNode() : in(0), out(0), index(-1), cycle(0), 
                  hasFanoutBranch(false), isSequential(false) {
            iptM = Eigen::MatrixXd::Identity(1, 1);
            optM = Eigen::MatrixXd::Identity(1, 1);
            REoptM = Eigen::MatrixXd::Identity(1, 1);
            ptm = Eigen::MatrixXd::Identity(2, 2);
        }
        
        FSNode(int idx) : in(0), out(0), index(idx), cycle(0),
                         hasFanoutBranch(false), isSequential(false) {
            iptM = Eigen::MatrixXd::Identity(1, 1);
            optM = Eigen::MatrixXd::Identity(1, 1);
            REoptM = Eigen::MatrixXd::Identity(1, 1);
            ptm = Eigen::MatrixXd::Identity(2, 2);
        }
        
        FSNode(FSNode&& other) noexcept = default;
        FSNode& operator=(FSNode&& other) noexcept = default;
        
        FSNode(const FSNode&) = delete;
        FSNode& operator=(const FSNode&) = delete;
    };

    // 成员变量
    std::vector<std::vector<FSNode>> allFsNodes_;
    std::vector<std::vector<Eigen::Vector2d>> opVectors_;
    std::vector<double> node_priorities_;
    Eigen::Matrix<double, 2, 2> Mff_;
    double faultRate_;
    
    int nowCycle_;


public:
    FSTRAAnalyzer(mockturtle::aig_network& circuit, IverilogSimulator& sim, VCDParser& vcd_parser);
    ~FSTRAAnalyzer() = default;
    
    FSTRAAnalyzer(const FSTRAAnalyzer&) = delete;
    FSTRAAnalyzer& operator=(const FSTRAAnalyzer&) = delete;
    
    void initializeFSNodes(int cycle);
    void runFSTracking();
    void runIterativeReduction();
    void runIterativeReductionParallel(int cycle);
    void runParallelReliabilityCalculation(const std::vector<int>& vec_int, int k);
    void FS_TRAMethod(int cycle, int Mn_fs);
    void FS_TRAMethodByCycle(int cycle, int Mn_fs);
    double calculateOutputReliability(const Eigen::MatrixXd& optM, const Eigen::VectorXd& oIV,bool is_complemented);
    double calculateOutputReliability(const Eigen::MatrixXd& optM, const Eigen::VectorXd& oIV);
    
    // 配置函数
    void setFaultRate(double rate) { faultRate_ = rate; }
    void setMffMatrix(const Eigen::Matrix<double, 2, 2>& mff) { Mff_ = mff; }
    
    // 访问函数
    FSNode& getFSNode(int cycle,int index) { return allFsNodes_[cycle][index]; }
    const FSNode& getFSNode(int cycle,int index) const { return allFsNodes_[cycle][index]; }
    const std::vector<FSNode>& getAllFSNodes(int cycle) const { return allFsNodes_[cycle]; }
    
    // 工具函数
    std::vector<std::reference_wrapper<FSNode>> getPrimaryInputs();
    std::vector<std::reference_wrapper<FSNode>> getPrimaryOutputs();
    void printFSNodeInfo(int index) const;

private:
    // 核心算法函数
    std::pair<int, int> decomposeBinaryCode(int full_code, std::vector<int> nowFsL, std::vector<int> FsL1, std::vector<int> FsL2) const;
    Eigen::VectorXd getRowByBinary(const Eigen::MatrixXd& matrix, const std::vector<int>& fsL, int binary_code) const;
    void removeDuplicateElements(Eigen::MatrixXd& nodeIptM, std::vector<int>& nodeFsL, 
                                const std::vector<int>& tmpFsL, const Eigen::MatrixXd& tmpM);
    void generateTbRmFsL(std::vector<int>& tmpFsL, std::vector<int>& tb_rm_FsL,int Mn_fs);
    std::vector<int>  remove_elements_from_vector(
        const std::vector<int>& original,
        const std::vector<int>& elements_to_remove);
    void del_rMr(const Eigen::MatrixXd& formoptM, const std::vector<int>& formFsL, 
                                const std::vector<int>& tb_rm_FsL,Eigen::MatrixXd& tmpM,std::vector<int>& tmpFsl);
    void fsTracking(FSNode& fsnode);
    void iterativeReduction(std::vector<int> nodefsL, Eigen::MatrixXd& nodeOptM);
    void ProgramIterativeReduction(FSNode& fsnode, Eigen::MatrixXd& nodeOptM,int Mn_fs);
    void iterativeReductionParallel(std::vector<int> nodefsL, 
                                              Eigen::MatrixXd& nodeOptM,
                                              int cycle, int current_node);
    void DimensionReduction(FSNode& fsnode,int Mn_fs);
    void DimensionReductionByCycle(FSNode& fsnode,int Mn_fs);
    void getIdealOutput();
    void getopVectors(int cycle);
    void calPriorities(int cycle);
    int extractSignalIndex(const std::string& node_name);
    
    
    // 辅助函数
    Eigen::MatrixXd createPTMFromTruthTable(const kitty::dynamic_truth_table& tt,mockturtle::aig_network::node node);
    Eigen::MatrixXd createPTMForFF(mockturtle::aig_network::node node);
    int adjustInputIndex(int original_index, 
                        const std::vector<bool>& complemented_inputs,
                        int num_vars);
    void initializeMffMatrix();
};