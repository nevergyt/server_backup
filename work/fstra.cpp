#include "fstra.h"
#include <iostream>
#include <algorithm>
#include <omp.h>

// #define INITDEBUG
#define FSTRADEBUG
// #define ITERDEBUG
#define DimensionReductionDebug
#define progressDebug

std::ofstream fstra_debug("fstra_debug.txt");
std::ofstream iter_debug("iter_debug.txt");
std::ofstream rel("rel.txt");
std::ofstream dim_red_debug("dim_red_debug.txt");
std::ofstream dim_red_progress("dim_red_progress.txt");

FSTRAAnalyzer::FSTRAAnalyzer(mockturtle::aig_network& circuit, IverilogSimulator& sim, VCDParser& vcd_parser)
    : circuit_(circuit), simulator_(sim), vcd_parser_(vcd_parser), faultRate_(0.01) ,nowCycle_(1){
    initializeMffMatrix();
}

void FSTRAAnalyzer::initializeMffMatrix() {
    // 初始化 Mff 矩阵
    Mff_ = Eigen::Matrix<double, 2, 2>::Zero();
    Mff_ << 0.99, 0.01,   // 状态0: 保持0的概率0.99，翻转到1的概率0.01
            0.01, 0.99;   // 状态1: 翻转到0的概率0.01，保持1的概率0.99
}


void FSTRAAnalyzer::initializeFSNodes(int cycle) {
    const int numNodes = circuit_.size();

    std::cout<<"numNodes : "<< numNodes <<std::endl;
    
    allFsNodes_.clear();
    allFsNodes_.resize(cycle+2);

    opVectors_.clear();
    opVectors_.resize(cycle+2);

#ifdef INITDEBUG
    std::cout << "Initializing FSNodes for " << cycle 
              << " time frames × " << numNodes << " nodes." << std::endl;
#endif

    for (int t = 1; t <= cycle+1; ++t) {
        allFsNodes_[t].resize(numNodes); // ← This calls FSNode() for each element
        opVectors_[t].resize(numNodes);
    }

    mockturtle::topo_view circuit_topo{circuit_};

    circuit_topo.foreach_node([&](auto node) {
        int idx = circuit_.node_to_index(node);

        // Initialize the FSNode at every time frame for this node index
        for (int t = 1; t <= cycle; ++t) {
            FSNode& fsNode = allFsNodes_[t][idx];

            fsNode.index = idx;
            // fsNode.hasFanoutBranch = (circuit_.fanout_size(node) != 1)||(circuit_.is_ro(node)) ;
            fsNode.hasFanoutBranch = (circuit_.fanout_size(node) != 1) ;
            fsNode.isSequential = circuit_.is_ro(node);
            fsNode.cycle = t; 

            fsNode.iptM.resize(0, 0);
            fsNode.optM.resize(0, 0);
            fsNode.ptm.resize(0, 0);

            if (fsNode.isSequential) {
                // fsNode.ptm = createPTMForFF(node);
                if(t==1){
                    fsNode.optM.resize(1, 2);
                    fsNode.optM << 1.0, 0.0;
                }
            } else if (!circuit_.is_pi(node) && !circuit_.is_constant(node)) {
                auto tt = circuit_.node_function(node);
                fsNode.ptm = createPTMFromTruthTable(tt, node);
            } else if (circuit_.is_pi(node)) {
                // Primary input: uniform prior
                fsNode.optM.resize(1, 2);
                fsNode.optM << 1.0, 0.0;
                opVectors_[t][idx]=Eigen::Vector2d(1.0, 0.0);;
            }

    #ifdef INITDEBUG
                if (t == 0) { // Print once per node
                    std::cout << "Node " << idx 
                              << " (seq=" << fsNode.isSequential 
                              << ", PI=" << circuit_.is_pi(node) 
                              << ") PTM: " << fsNode.ptm.rows() << "×" << fsNode.ptm.cols()
                              << ", hasFanout: " << fsNode.hasFanoutBranch << std::endl;
                }
    #endif
        }
    });

}

Eigen::MatrixXd FSTRAAnalyzer::createPTMForFF(mockturtle::aig_network::node node){
    Eigen::MatrixXd tmp(4,2);
    tmp <<  1-faultRate_,faultRate_,
            1-faultRate_,faultRate_,
            faultRate_,1-faultRate_,
            faultRate_,1-faultRate_;
    return tmp;
}

Eigen::MatrixXd FSTRAAnalyzer::createPTMFromTruthTable(const kitty::dynamic_truth_table& tt,mockturtle::aig_network::node node) {


    int num_vars = tt.num_vars();
    int num_rows = 1 << num_vars;
    Eigen::MatrixXd ptm(num_rows, 2);

    #ifdef INITDEBUG
            std::cout << "Initialized FSNode creating PTM !! " << std::endl;
            std::cout << "num_vars :" << num_vars <<std::endl;
            std::cout << "num_rows :" << num_rows <<std::endl;
            std::cout << "fanin size :" << circuit_.fanin_size(node) <<std::endl;
            // std::cout << "dynamic_truth_table: " << tt;
    #endif
    
    
    // Ignore complemented inputs: build PTM directly from truth table entries
    for (int i = 0; i < num_rows; ++i) {
        bool output_bit = kitty::get_bit(tt, i);
        if (output_bit) {
            ptm(i, 0) = 0.0;  // 输出0的概率
            ptm(i, 1) = 1.0;  // 输出1的概率
        } else {
            ptm(i, 0) = 1.0;  // 输出0的概率
            ptm(i, 1) = 0.0;  // 输出1的概率
        }

        // 应用故障率
        if (faultRate_ > 0.0) {
            ptm(i, 0) = ptm(i, 0) * (1 - faultRate_) + (1 - ptm(i, 0)) * faultRate_;
            ptm(i, 1) = ptm(i, 1) * (1 - faultRate_) + (1 - ptm(i, 1)) * faultRate_;
        }
    }
    

    #ifdef INITDEBUG
            std::cout << "finish  Initialized FSNode creating PTM   !! " << std::endl;
    #endif
    return ptm;
}

int FSTRAAnalyzer::adjustInputIndex(int original_index, 
                        const std::vector<bool>& complemented_inputs,
                        int num_vars) {
        int adjusted = 0;

        // Ensure complemented_inputs has at least num_vars entries
        std::vector<bool> comps = complemented_inputs;
        if ((int)comps.size() < num_vars) comps.resize(num_vars, false);

        // kitty uses LSB-first ordering for variables (var 0 -> LSB)
        for (int var = 0; var < num_vars; ++var) {
            // 获取原始位的值（LSB-first）
            bool bit = (original_index >> var) & 1;

            // 如果该输入被取反，则翻转该位
            if (comps[var]) {
                bit = !bit;
            }

            // 将调整后的位放回正确位置（LSB-first）
            adjusted |= (bit << var);
        }

        return adjusted;
    }


std::pair<int, int> FSTRAAnalyzer::decomposeBinaryCode(int full_code, std::vector<int> nowFsL, std::vector<int> FsL1, std::vector<int> FsL2) const {
    int code1 = 0;
    int code2 = 0;

    #ifdef FSTRADEBUG
        fstra_debug<< "Decomposing full_code: " << full_code << std::endl;
        fstra_debug<< "nowFsL: ";
        for(auto e : nowFsL){
            fstra_debug << e <<" ";               
        }
        fstra_debug<< std::endl;
        fstra_debug<< "FsL1: ";
        for(auto e : FsL1){
            fstra_debug << e <<" ";               
        }
        fstra_debug<< std::endl;
        fstra_debug<< "FsL2: ";
        for(auto e : FsL2){
            fstra_debug << e <<" ";               
        }
        fstra_debug<< std::endl;
    #endif

    for(int i=0;i<FsL1.size();i++){
        int en = FsL1[i];
        auto it = std::find(nowFsL.begin(), nowFsL.end(), en);
        if (it != nowFsL.end()) {
            int pos = nowFsL.size() - 1 - std::distance(nowFsL.begin(), it);
            if (full_code & (1 << pos)) {
                code1 |= (1 << (FsL1.size()-1 - i));
            }
        }
    }

    for(int i=0;i<FsL2.size();i++){
        int en = FsL2[i];
        auto it = std::find(nowFsL.begin(), nowFsL.end(), en);
        if (it != nowFsL.end()) {
            int pos = nowFsL.size() - 1 - std::distance(nowFsL.begin(), it);
            if (full_code & (1 << pos)) {
                code2 |= (1 << (FsL2.size()-1 - i));
            }
        }
    }

    #ifdef FSTRADEBUG
        fstra_debug<< "Decomposing full_code: " << full_code 
                   << " into code1: " << code1 
                   << " and code2: " << code2 << std::endl;
    #endif

    return {code1, code2};
}

Eigen::VectorXd FSTRAAnalyzer::getRowByBinary(const Eigen::MatrixXd& matrix, const std::vector<int>& fsL, int binary_code) const {
    if (matrix.rows() == 0) {
        return Eigen::RowVectorXd::Ones(1);  
    }
    
    if (fsL.empty()) {
        return matrix.row(0).eval();
    }
    
    int row_index = binary_code % matrix.rows();
    return matrix.row(row_index).eval();
}

void FSTRAAnalyzer::removeDuplicateElements(Eigen::MatrixXd& nodeIptM, std::vector<int>& nodeFsL, 
                                          const std::vector<int>& tmpFsL, const Eigen::MatrixXd& tmpM) {

    #ifdef FSTRADEBUG

        fstra_debug<< "=============================" << std::endl;   
        fstra_debug<< "removeDuplicateElements start" << std::endl;
        fstra_debug<< "=============================" << std::endl; 
        fstra_debug<< "=============================" << std::endl;
        fstra_debug<< "Input nodeIptM: " << nodeIptM << std::endl;
        fstra_debug<< "Input tmpM: " << tmpM << std::endl;
        fstra_debug<< "nodeFsL size: " << nodeFsL.size() << std::endl;
        for(auto e : nodeFsL){
            fstra_debug << e <<" ";               
        }
        fstra_debug<< std::endl;
        fstra_debug<< "tmpFsL size: " << tmpFsL.size() << std::endl;
        for(auto e : tmpFsL){   
            fstra_debug << e <<" ";               
        }
        fstra_debug<< std::endl;
        fstra_debug<< "=============================" << std::endl;
    #endif


    std::vector<int> comFsL;
    comFsL.insert(comFsL.end(), nodeFsL.begin(), nodeFsL.end());
    comFsL.insert(comFsL.end(), tmpFsL.begin(), tmpFsL.end());
    
    // 去重，保留首次出现
    std::unordered_set<int> seen;
    size_t write_idx = 0;
    for (size_t i = 0; i < comFsL.size(); ++i) {
        int v = comFsL[i];
        if (seen.insert(v).second) {
            comFsL[write_idx++] = v;
        }
    }
    if (write_idx < comFsL.size()) {
        comFsL.erase(comFsL.begin() + write_idx, comFsL.end());
    }


    int dem = comFsL.size();
    int new_rows = 1 << dem;
    int new_cols = nodeIptM.cols() * tmpM.cols();



    Eigen::MatrixXd com_iptM = Eigen::MatrixXd::Zero(new_rows, new_cols);
    for (int binary_code = 0; binary_code < new_rows; binary_code++) {
        auto [binary1, binary2] = decomposeBinaryCode(binary_code, comFsL, nodeFsL, tmpFsL);
        
        Eigen::VectorXd row1 = getRowByBinary(nodeIptM, nodeFsL, binary1);
        Eigen::VectorXd row2 = getRowByBinary(tmpM, tmpFsL, binary2);
        
        Eigen::MatrixXd tensor_prod = Eigen::kroneckerProduct(row1, row2).transpose();

        com_iptM.row(binary_code) = tensor_prod;
    }


    #ifdef FSTRADEBUG
        fstra_debug<< "=============================" << std::endl;
        fstra_debug<< "com_iptM: " << com_iptM.rows() << "x" << com_iptM.cols() << std::endl;
        fstra_debug<< com_iptM<<std::endl;
        fstra_debug<< "=============================" << std::endl;
    #endif
    // 更新节点信息
    nodeFsL = std::move(comFsL);
    nodeIptM = std::move(com_iptM);
}

void FSTRAAnalyzer::del_rMr(const Eigen::MatrixXd& formoptM,const std::vector<int>& formFsL, 
                                const std::vector<int>& tb_rm_FsL,Eigen::MatrixXd& tmpM,std::vector<int>& tmpFsl){

    #ifdef DimensionReductionDebug
        dim_red_progress << "=============================" << std::endl;
        dim_red_progress << "delrMr  start"<< std::endl;

        dim_red_progress << "=============================" << std::endl;
        dim_red_progress << "tmpM: "<<tmpM<< std::endl;
        dim_red_progress << "formoptM: "<<formoptM<< std::endl;
        dim_red_progress << "formFsl lenthg: "<<formFsL.size()<<std::endl;
        for(auto e : formFsL){
            dim_red_progress << e <<" ";
        }
        dim_red_progress << std::endl;
        dim_red_progress << "tb_rm_FsL lenthg: "<<tb_rm_FsL.size()<<std::endl;
        for(auto e : tb_rm_FsL){
            dim_red_progress << e <<" ";
        }
        dim_red_progress << std::endl;
        dim_red_progress << "cycle: "<<nowCycle_ << std::endl;
        dim_red_progress << "=============================" << std::endl;
    #endif

    tmpM=Eigen::MatrixXd::Identity(1,1);

    for(auto en : formFsL){

        #ifdef DimensionReductionDebug
            dim_red_progress << "=============================" << std::endl;
            dim_red_progress << en << "  opV: "<<opVectors_[nowCycle_][en].transpose()<< std::endl;
            dim_red_progress << "=============================" << std::endl;
        #endif


        if(std::find(tb_rm_FsL.begin(), tb_rm_FsL.end(), en)!=tb_rm_FsL.end()){
            Eigen::MatrixXd tmpM2=Eigen::kroneckerProduct(tmpM,opVectors_[nowCycle_][en].transpose());
            tmpM=tmpM2;
        }
        else{
            Eigen::MatrixXd tmpM2=Eigen::kroneckerProduct(tmpM, Eigen::Matrix2d::Identity());
            tmpM=tmpM2;
            tmpFsl.push_back(en);
        }
    }

    tmpM=tmpM*formoptM;

    #ifdef DimensionReductionDebug
            dim_red_progress << "=============================" << std::endl;
            dim_red_progress << "tmpM: "<<tmpM.rows()<<"x"<<tmpM.cols() << std::endl;
            dim_red_progress << tmpM <<std::endl;
            dim_red_progress << "tmpFsl lenthg: "<<tmpFsl.size()<<std::endl;
            for(auto e : tmpFsl){
                dim_red_progress << e <<" ";
            }
            dim_red_progress << std::endl;
            dim_red_progress << "=============================" << std::endl;
    
            dim_red_progress << "=============================" << std::endl;
            dim_red_progress << "delrMr  finish"<< std::endl;
            dim_red_progress << "=============================" << std::endl;
    #endif

}

void FSTRAAnalyzer::generateTbRmFsL(std::vector<int>& tmpFsL, std::vector<int>& tb_rm_FsL,int Mn_fs){
    // 去重，保留首次出现
    std::unordered_set<int> seen;
    size_t write_idx = 0;
    for (size_t i = 0; i < tmpFsL.size(); ++i) {
        int v = tmpFsL[i];
        if (seen.insert(v).second) {
            tmpFsL[write_idx++] = v;
        }
    }
    if (write_idx < tmpFsL.size()) {
        tmpFsL.erase(tmpFsL.begin() + write_idx, tmpFsL.end());
    }

    int remove_count = tmpFsL.size() - Mn_fs;
    if(remove_count>0){
        std::priority_queue<std::pair<double, int>> max_heap;
        
        for (int node_id : tmpFsL) {
            double priority = node_priorities_[node_id];
            
            max_heap.emplace(priority, node_id);
            
            // 保持堆的大小为remove_count
            if (max_heap.size() > remove_count) {
                max_heap.pop();  // 移除优先级最高的，保留优先级最低的
            }
        }
        
        tb_rm_FsL.reserve(remove_count);
        
        while (!max_heap.empty()) {
            tb_rm_FsL.push_back(max_heap.top().second);
            max_heap.pop();
        }
    }

    tmpFsL = remove_elements_from_vector(tmpFsL, tb_rm_FsL);
}

std::vector<int> FSTRAAnalyzer::remove_elements_from_vector(
        const std::vector<int>& original,
        const std::vector<int>& elements_to_remove) {
        
        std::unordered_set<int> to_remove_set(
            elements_to_remove.begin(), elements_to_remove.end());
        
        std::vector<int> result;
        result.reserve(original.size() - elements_to_remove.size());
        
        for (int elem : original) {
            if (to_remove_set.find(elem) == to_remove_set.end()) {
                result.push_back(elem);
            }
        }
        
        return result;
    }

void FSTRAAnalyzer::fsTracking(FSNode& fsnode) {

    #ifdef FSTRADEBUG
        fstra_debug << "=============================" << std::endl;
        fstra_debug << "FS Tracking start on node "<<fsnode.index << std::endl;
        fstra_debug << "=============================" << std::endl;
    #endif
    // 初始化
    fsnode.fsL.clear();
    fsnode.iptM = Eigen::MatrixXd::Identity(1, 1);

    auto node = circuit_.index_to_node(fsnode.index);

    if(circuit_.is_ro(node)&&nowCycle_==1)return;

    //寄存器单独处理
    if(circuit_.is_ro(node)){
        
            auto rin=circuit_.ro_to_ri(circuit_.make_signal(node));
            auto fanin_node = circuit_.get_node(rin);
            int fanin_index = circuit_.node_to_index(fanin_node);

            FSNode& father = allFsNodes_[nowCycle_-1][fanin_index];

            #ifdef FSTRADEBUG
                fstra_debug << "=============================" << std::endl;
                fstra_debug << "fanin_node "<<fanin_index << std::endl;
                fstra_debug << "father optm "<<father.optM << std::endl;
                fstra_debug << "=============================" << std::endl;
            #endif

            if (!father.hasFanoutBranch) {
                removeDuplicateElements(fsnode.iptM, fsnode.fsL, father.fsL, father.optM);
            } else {
                std::vector<int> tmp;
                tmp.push_back(father.index);
                removeDuplicateElements(fsnode.iptM, fsnode.fsL, tmp, Eigen::Matrix2d::Identity());
            }

            FSNode& father2 = allFsNodes_[nowCycle_-1][fsnode.index];
            std::vector<int> tmp2;
            tmp2.push_back(father2.index);
            removeDuplicateElements(fsnode.iptM, fsnode.fsL, tmp2, Eigen::Matrix2d::Identity());
            fsnode.fsL.clear();
    }
    else{
        circuit_.foreach_fanin(node, [&](auto signal) {
            auto fanin_node = circuit_.get_node(signal);
            int fanin_index = circuit_.node_to_index(fanin_node);
            FSNode& father = allFsNodes_[nowCycle_][fanin_index];
            
            #ifdef FSTRADEBUG
                fstra_debug << "=============================" << std::endl;
                fstra_debug << "prepare rmdup on "<<father.index << std::endl;
            #endif

            if (!father.hasFanoutBranch) {
                removeDuplicateElements(fsnode.iptM, fsnode.fsL, father.fsL, father.optM);
            } else {
                std::vector<int> tmp;
                tmp.push_back(father.index);
                removeDuplicateElements(fsnode.iptM, fsnode.fsL, tmp, Eigen::Matrix2d::Identity());
            }
        });
    }
    

    fsnode.optM = fsnode.iptM * fsnode.ptm;

    #ifdef FSTRADEBUG
        fstra_debug << "=============================" << std::endl;
        fstra_debug << "=============================" << std::endl;
        fstra_debug << "=============================" << std::endl;
        fstra_debug<< "fsnode "<< fsnode.index <<"  iptM :"<<std::endl;
        fstra_debug << fsnode.iptM<< std::endl;
        fstra_debug << "=============================" << std::endl;

        fstra_debug << "=============================" << std::endl;
        fstra_debug<< "fsnode "<< fsnode.index <<"  ptM :"<<std::endl;
        fstra_debug << fsnode.ptm<< std::endl;
        fstra_debug << "=============================" << std::endl;

        fstra_debug << "=============================" << std::endl;
        fstra_debug<< "fsnode "<< fsnode.index <<"  outM :"<<std::endl;
        fstra_debug << fsnode.optM<< std::endl;
        fstra_debug << "=============================" << std::endl;
        fstra_debug << "=============================" << std::endl;
        fstra_debug << "=============================" << std::endl;
    #endif
}

void FSTRAAnalyzer::DimensionReduction(FSNode& fsnode,int Mn_fs){

    #ifdef DimensionReductionDebug
        dim_red_debug << "=============================" << std::endl;
        dim_red_debug << "Dimension Reduction start on node "<<fsnode.index << std::endl;
        dim_red_debug << "=============================" << std::endl;
    #endif

    fsnode.fsL.clear();
    fsnode.iptM = Eigen::MatrixXd::Identity(1, 1);
    std::vector<int> tmpFsl,tb_rm_fsl,updated_fsl;
    tmpFsl.clear();
    tb_rm_fsl.clear();
    updated_fsl.clear();

    auto node = circuit_.index_to_node(fsnode.index);



    if(circuit_.is_ro(node)){
        
            auto rin=circuit_.ro_to_ri(circuit_.make_signal(node));
            auto fanin_node = circuit_.get_node(rin);
            int fanin_index = circuit_.node_to_index(fanin_node);

            FSNode& father = allFsNodes_[nowCycle_-1][fanin_index];


            if (!father.hasFanoutBranch) {
                removeDuplicateElements(fsnode.iptM, fsnode.fsL, father.fsL, father.optM);
            } else {
                std::vector<int> tmp;
                tmp.push_back(father.index);
                removeDuplicateElements(fsnode.iptM, fsnode.fsL, tmp, Eigen::Matrix2d::Identity());
            }

            FSNode& father2 = allFsNodes_[nowCycle_-1][fsnode.index];
            std::vector<int> tmp2;
            tmp2.push_back(father2.index);
            removeDuplicateElements(fsnode.iptM, fsnode.fsL, tmp2, Eigen::Matrix2d::Identity());
            fsnode.fsL.clear();


            fsnode.optM = fsnode.iptM * fsnode.ptm;
            return;
    }


    circuit_.foreach_fanin(node,[&](auto signal) {
        auto fanin_node = circuit_.get_node(signal);
        int fanin_index = circuit_.node_to_index(fanin_node);
        FSNode& father = allFsNodes_[nowCycle_][fanin_index];

        if (!father.hasFanoutBranch) {
            tmpFsl.insert(tmpFsl.end(), father.fsL.begin(), father.fsL.end());
        } else {
            tmpFsl.push_back(father.index);
        }
    });

    // 去重，保留首次出现
    std::unordered_set<int> seen;
    size_t write_idx = 0;
    for (size_t i = 0; i < tmpFsl.size(); ++i) {
        int v = tmpFsl[i];
        if (seen.insert(v).second) {
            tmpFsl[write_idx++] = v;
        }
    }
    if (write_idx < tmpFsl.size()) {
        tmpFsl.erase(tmpFsl.begin() + write_idx, tmpFsl.end());
    }

    int remove_count = tmpFsl.size() - Mn_fs;

    if(remove_count>0){
        std::priority_queue<std::pair<double, int>> max_heap;
        
        for (int node_id : tmpFsl) {
            double priority = node_priorities_[node_id];
            
            max_heap.emplace(priority, node_id);
            
            // 保持堆的大小为remove_count
            if (max_heap.size() > remove_count) {
                max_heap.pop();  // 移除优先级最高的，保留优先级最低的
            }
        }
        
        tb_rm_fsl.reserve(remove_count);
        
        while (!max_heap.empty()) {
            tb_rm_fsl.push_back(max_heap.top().second);
            max_heap.pop();
        }
    }
    
    updated_fsl = remove_elements_from_vector(tmpFsl, tb_rm_fsl);
    
    circuit_.foreach_fanin(node,[&](auto signal) {
        auto fanin_node = circuit_.get_node(signal);
        int fanin_index = circuit_.node_to_index(fanin_node);
        FSNode& father = allFsNodes_[nowCycle_][fanin_index];

        Eigen::MatrixXd tmpM_for = Eigen::MatrixXd::Identity(1, 1);
        std::vector<int> tmpFsl_for;
        tmpFsl_for.clear();

        #ifdef DimensionReductionDebug
            dim_red_debug << "=============================" << std::endl;
            dim_red_debug << "fanin "<<fanin_index << std::endl;
            dim_red_debug << "=============================" << std::endl;
        #endif

        if (!father.hasFanoutBranch) {
            del_rMr(father.optM, father.fsL, tb_rm_fsl,tmpM_for,tmpFsl_for);
        } else {
            std::vector<int> tmp;
            tmp.push_back(father.index);
            del_rMr(Eigen::Matrix2d::Identity(),tmp,tb_rm_fsl,tmpM_for,tmpFsl_for);
        }

        removeDuplicateElements(fsnode.iptM, fsnode.fsL,tmpFsl_for,tmpM_for);

    });

    fsnode.optM = fsnode.iptM * fsnode.ptm;

}


void FSTRAAnalyzer::DimensionReductionByCycle(FSNode& fsnode,int Mn_fs){
    #ifdef DimensionReductionDebug
        dim_red_debug << "=============================" << std::endl;
        dim_red_debug << "Dimension Reduction start on node "<<fsnode.index << std::endl;
        dim_red_debug << "=============================" << std::endl;
    #endif

    fsnode.fsL.clear();
    fsnode.iptM = Eigen::MatrixXd::Identity(1, 1);
    std::vector<int> tmpFsl,tb_rm_fsl,updated_fsl;
    tmpFsl.clear();
    tb_rm_fsl.clear();
    updated_fsl.clear();

    auto node = circuit_.index_to_node(fsnode.index);

    circuit_.foreach_fanin(node,[&](auto signal) {
        auto fanin_node = circuit_.get_node(signal);
        int fanin_index = circuit_.node_to_index(fanin_node);
        FSNode& father = allFsNodes_[nowCycle_][fanin_index];

        if (!father.hasFanoutBranch) {
            tmpFsl.insert(tmpFsl.end(), father.fsL.begin(), father.fsL.end());
        } else {
            tmpFsl.push_back(father.index);
        }
    });

    // 去重，保留首次出现
    std::unordered_set<int> seen;
    size_t write_idx = 0;
    for (size_t i = 0; i < tmpFsl.size(); ++i) {
        int v = tmpFsl[i];
        if (seen.insert(v).second) {
            tmpFsl[write_idx++] = v;
        }
    }
    if (write_idx < tmpFsl.size()) {
        tmpFsl.erase(tmpFsl.begin() + write_idx, tmpFsl.end());
    }    
    

    int remove_count=tmpFsl.size()-Mn_fs;

    #ifdef DimensionReductionDebug
        dim_red_debug << "=============================" << std::endl;
        dim_red_debug << "tmpFsl size before reduction: "<<tmpFsl.size() << std::endl;
        dim_red_debug << "Mn_fs: "<<Mn_fs << std::endl;
        dim_red_debug << "remove_count: "<<remove_count << std::endl;
        dim_red_debug << "=============================" << std::endl;
    #endif

    if(remove_count>0){
        std::priority_queue<std::pair<double, int>> max_heap;
        
        for (int node_id : tmpFsl) {
            double priority = node_priorities_[node_id];
            
            max_heap.emplace(priority, node_id);
            
            // 保持堆的大小为remove_count
            if (max_heap.size() > remove_count) {
                max_heap.pop();  // 移除优先级最高的，保留优先级最低的
            }
        }
        
        tb_rm_fsl.reserve(remove_count);
        
        while (!max_heap.empty()) {
            tb_rm_fsl.push_back(max_heap.top().second);
            max_heap.pop();
        }
    }
    
    updated_fsl = remove_elements_from_vector(tmpFsl, tb_rm_fsl);
    
    circuit_.foreach_fanin(node,[&](auto signal) {
        auto fanin_node = circuit_.get_node(signal);
        int fanin_index = circuit_.node_to_index(fanin_node);
        FSNode& father = allFsNodes_[nowCycle_][fanin_index];

        Eigen::MatrixXd tmpM_for = Eigen::MatrixXd::Identity(1, 1);
        std::vector<int> tmpFsl_for;
        tmpFsl_for.clear();

        #ifdef DimensionReductionDebug
            dim_red_debug << "=============================" << std::endl;
            dim_red_debug << "fanin "<<fanin_index << std::endl;
            dim_red_debug << "=============================" << std::endl;
        #endif

        if (!father.hasFanoutBranch) {
            if(circuit_.is_complemented(signal)){
                Eigen::MatrixXd father_optM=father.optM;
                father_optM.col(0).swap(father_optM.col(1));
                del_rMr(father_optM, father.fsL, tb_rm_fsl,tmpM_for,tmpFsl_for);
            } else {
                del_rMr(father.optM, father.fsL, tb_rm_fsl,tmpM_for,tmpFsl_for);
            }
        } else {
            std::vector<int> tmp;
            tmp.push_back(father.index);
            if(circuit_.is_complemented(signal)){
                del_rMr(Eigen::Matrix2d::Identity().rowwise().reverse(),tmp,tb_rm_fsl,tmpM_for,tmpFsl_for);
            }
            else{
                del_rMr(Eigen::Matrix2d::Identity(),tmp,tb_rm_fsl,tmpM_for,tmpFsl_for);
            }
        }

        removeDuplicateElements(fsnode.iptM, fsnode.fsL,tmpFsl_for,tmpM_for);

    });

    fsnode.optM = fsnode.iptM * fsnode.ptm;

    // //输出是否取反
    // auto signal_out=circuit_.make_signal(node);
    // if(circuit_.is_complemented(signal_out)){
    //     fsnode.optM.col(0).swap(fsnode.optM.col(1));
    // }

    #ifdef DimensionReductionDebug
        dim_red_debug << "=============================" << std::endl;
        dim_red_debug << "fsnode index: "<<fsnode.index << std::endl;
        dim_red_debug << "fsnode iptM: "<<fsnode.iptM << std::endl;
        dim_red_debug << "fsnode ptM: "<<fsnode.ptm << std::endl;
        dim_red_debug << "fsnode optM: "<<fsnode.optM << std::endl;
        dim_red_debug << "fsnode fsL size: "<<fsnode.fsL.size() << std::endl;

        dim_red_debug << "=============================" << std::endl;
        dim_red_debug << "Dimension Reduction finish on node "<<fsnode.index << std::endl;
        dim_red_debug << "=============================" << std::endl;
    #endif

}


//normal
void FSTRAAnalyzer::iterativeReduction(std::vector<int> nodefsL, Eigen::MatrixXd& nodeOptM) {


    Eigen::MatrixXd com_redM = Eigen::MatrixXd::Identity(1, 1);
    
    while (!nodefsL.empty()) {
        int max_index = *std::max_element(nodefsL.begin(), nodefsL.end());
        FSNode& lsNode = allFsNodes_[nowCycle_][max_index];

        #ifdef ITERDEBUG
            iter_debug << "=============================" << std::endl;
            iter_debug << "Running Iterative Reduction max_index :"<< max_index << std::endl;
            iter_debug << "=============================" << std::endl;
        #endif
        
        Eigen::MatrixXd redM = Eigen::MatrixXd::Identity(1, 1);
        std::vector<int> tmp_fsL;
        
        for (auto it = nodefsL.begin(); it != nodefsL.end(); ++it) {
            if (*it != max_index) {
                std::vector<int> t;
                t.push_back(*it);
                removeDuplicateElements(redM, tmp_fsL, t, Eigen::MatrixXd::Identity(2, 2));
            }
            else{
                removeDuplicateElements(redM, tmp_fsL, lsNode.fsL,lsNode.optM);
            }
        }
        if(com_redM==Eigen::MatrixXd::Identity(1, 1)) com_redM=redM;
        else com_redM = redM * com_redM;

        #ifdef ITERDEBUG
            iter_debug << "=============================" << std::endl;
            iter_debug << "tmpFSL elements:";
            for (const auto& val : tmp_fsL) {
                iter_debug << val<<"  ";
            }
            iter_debug <<std::endl;
            iter_debug << "=============================" << std::endl;
        #endif

        nodefsL = std::move(tmp_fsL);
    }
    
    nodeOptM = com_redM * nodeOptM;

    #ifdef ITERDEBUG
            iter_debug << "=============================" << std::endl;
            iter_debug << "nodeOptM :" <<std::endl;
            iter_debug << nodeOptM <<std::endl;
            iter_debug << "=============================" << std::endl;
    #endif
}


void FSTRAAnalyzer::ProgramIterativeReduction(FSNode& fsnode, Eigen::MatrixXd& nodeOptM,int Mn_fs){

    #ifdef progressDebug
        dim_red_progress << "=============================" << std::endl;
        dim_red_progress << "Program Iterative Reduction start on node "<<fsnode.index << std::endl;
        dim_red_progress << "=============================" << std::endl;
    #endif
    
    Eigen::MatrixXd com_redM = Eigen::MatrixXd::Identity(1, 1);
    std::vector<int> fsnode_fsL_copy = fsnode.fsL;


    while(!fsnode_fsL_copy.empty()){
        int max_index = *std::max_element(fsnode_fsL_copy.begin(), fsnode_fsL_copy.end());
        FSNode& lsNode = allFsNodes_[nowCycle_][max_index];
        
        Eigen::MatrixXd redM = Eigen::MatrixXd::Identity(1, 1);
        Eigen::MatrixXd tmpM_for = Eigen::MatrixXd::Identity(1, 1);
        std::vector<int> tmp_fsL,tb_rm_fsL,tmp_fsL_for;

        tmp_fsL=fsnode_fsL_copy;

        auto it = std::find(tmp_fsL.begin(), tmp_fsL.end(), max_index);
    
        if (it != tmp_fsL.end()) {
            it = tmp_fsL.erase(it);
            tmp_fsL.insert(it, lsNode.fsL.begin(), lsNode.fsL.end());
        }

        generateTbRmFsL(tmp_fsL, tb_rm_fsL, Mn_fs);


        #ifdef progressDebug
            dim_red_progress << "=============================" << std::endl;
            dim_red_progress << "max_index :"<< max_index << std::endl;
            dim_red_progress << "tmpFSL elements:";
            for (const auto& val : tmp_fsL) {
                dim_red_progress << val<<"  ";
            }
            dim_red_progress <<std::endl;
            dim_red_progress << "tb_rm_fsL elements:";
            for (const auto& val : tb_rm_fsL) {
                dim_red_progress << val<<"  ";  
            }
            dim_red_progress <<std::endl;
            dim_red_progress << "fsnode fsl : ";
            for (const auto& val : fsnode_fsL_copy) {
                dim_red_progress << val<<"  ";
            }
            dim_red_progress << std::endl;
            dim_red_progress << "=============================" << std::endl;
        #endif

        for (auto it = fsnode_fsL_copy.begin(); it != fsnode_fsL_copy.end(); ++it) {
            if (*it != max_index) {
                std::vector<int> t;
                t.push_back(*it);
                del_rMr(Eigen::Matrix2d::Identity(), t, tb_rm_fsL, tmpM_for, tmp_fsL_for);
            }
            else{
                del_rMr(lsNode.optM, lsNode.fsL, tb_rm_fsL, tmpM_for, tmp_fsL_for);
            }
            removeDuplicateElements(redM, tmp_fsL, tmp_fsL_for, tmpM_for);
        }

        #ifdef progressDebug
            dim_red_progress << "=============================" << std::endl;
            dim_red_progress << "redM :" <<std::endl;
            dim_red_progress << redM <<std::endl;
            dim_red_progress << "=============================" << std::endl;
        #endif


        if(com_redM==Eigen::MatrixXd::Identity(1, 1)) com_redM=redM;
        else com_redM = redM * com_redM;

        #ifdef progressDebug
            dim_red_progress << "=============================" << std::endl;
            dim_red_progress << "com_redM :" <<std::endl;
            dim_red_progress << com_redM <<std::endl;
            dim_red_progress << "=============================" << std::endl;
        #endif

        fsnode_fsL_copy = std::move(tmp_fsL);
    }


    #ifdef progressDebug
        dim_red_progress << "=============================" << std::endl;
        dim_red_progress << "Final Results after Iterative Reduction :" << std::endl;
        dim_red_progress << "com_redM :" <<std::endl;
        dim_red_progress << com_redM <<std::endl;
        dim_red_progress << "fsnode optm :"<<std::endl;
        dim_red_progress << fsnode.optM <<std::endl;
        dim_red_progress << "=============================" << std::endl;
    #endif


    fsnode.REoptM = com_redM * fsnode.optM;

    #ifdef progressDebug
        dim_red_progress << "=============================" << std::endl;
        dim_red_progress << "Program Iterative Reduction finish on node "<<fsnode.index << std::endl;
        dim_red_progress << "=============================" << std::endl;
    #endif
}


//parallel
void FSTRAAnalyzer::iterativeReductionParallel(std::vector<int> nodefsL, 
                                              Eigen::MatrixXd& nodeOptM,
                                              int cycle, int current_node) {
    Eigen::MatrixXd com_redM = Eigen::MatrixXd::Identity(1, 1);
    
    while (!nodefsL.empty()) {
        int max_index = *std::max_element(nodefsL.begin(), nodefsL.end());
        
        // 获取节点数据（需要临界区保护）
        std::vector<int> ls_fsL;
        Eigen::MatrixXd ls_optM;
        
        #pragma omp critical
        {
            FSNode& lsNode = allFsNodes_[cycle][max_index];
            ls_fsL = lsNode.fsL;
            ls_optM = lsNode.optM;
        }
        
        #ifdef ITERDEBUG
            #pragma omp critical
            {
                iter_debug << "Thread " << omp_get_thread_num() 
                          << " processing max_index: " << max_index 
                          << " in cycle " << cycle << std::endl;
            }
        #endif
        
        Eigen::MatrixXd redM = Eigen::MatrixXd::Identity(1, 1);
        std::vector<int> tmp_fsL;
        
        for (auto it = nodefsL.begin(); it != nodefsL.end(); ++it) {
            if (*it != max_index) {
                std::vector<int> t;
                t.push_back(*it);
                removeDuplicateElements(redM, tmp_fsL, t, Eigen::MatrixXd::Identity(2, 2));
            } else {
                removeDuplicateElements(redM, tmp_fsL, ls_fsL, ls_optM);
            }
        }
        
        if(com_redM == Eigen::MatrixXd::Identity(1, 1)) {
            com_redM = redM;
        } else {
            com_redM = redM * com_redM;
        }

        nodefsL = std::move(tmp_fsL);
    }
    
    nodeOptM = com_redM * nodeOptM;
}

void FSTRAAnalyzer::getIdealOutput() {
    


    // std::cout << "Calculating ideal outputs using simulator..." << std::endl;
}

double FSTRAAnalyzer::calculateOutputReliability(const Eigen::MatrixXd& optM, const Eigen::VectorXd& oIV,bool is_complemented) {
    double reliability = 0.0;
    if(is_complemented){
        for (int i = 0; i < oIV.rows(); ++i) {
            reliability += optM(i) * oIV((i+1)%2);
        }
        return reliability;
    }
    for (int i = 0; i < oIV.rows(); ++i) {
        reliability += optM(i) * oIV(i);
    }
    return reliability;
}

double FSTRAAnalyzer::calculateOutputReliability(const Eigen::MatrixXd& optM, const Eigen::VectorXd& oIV) {
    double reliability = 0.0;
    for (int i = 0; i < oIV.rows(); ++i) {
        reliability += optM(i) * oIV(i);
    }
    return reliability;
}


void FSTRAAnalyzer::runFSTracking() {
    #ifdef FSTRADEBUG
        fstra_debug << "=============================" << std::endl;
        fstra_debug << "RunFSTracking  start" << std::endl;
        fstra_debug << "=============================" << std::endl;
    #endif
    
    int processed_count = 0;

    mockturtle::topo_view circuit_topo{circuit_};
    circuit_topo.foreach_node([&](auto node) {
        int index = circuit_.node_to_index(node);
        FSNode& fsnode = allFsNodes_[nowCycle_][index];
        
        // 只为非输入节点运行 FS Tracking
        if (!circuit_.is_pi(node) && !circuit_.is_constant(node)) {
            fsTracking(fsnode);
            processed_count++;
        }
    });

    
    #ifdef FSTRADEBUG
        circuit_.foreach_po([&](auto signal) {
            auto po_node = circuit_.get_node(signal);
            int po_index = circuit_.node_to_index(po_node);
            FSNode& father = allFsNodes_[nowCycle_][po_index];

            fstra_debug << "=============================" << std::endl;
            fstra_debug << "finish tracking on node :" << po_index << std::endl;
            fstra_debug << "optm:"  << std::endl;
            fstra_debug << father.optM << std::endl;
            fstra_debug << "=============================" << std::endl;
            
        });
        fstra_debug << "FS Tracking completed for " << processed_count << " nodes" << std::endl;
    #endif
}


//normal
void FSTRAAnalyzer::runIterativeReduction() {
    #ifdef ITERDEBUG
        iter_debug << "=============================" << std::endl;
        iter_debug << "Running Iterative Reduction on primary outputs..." << std::endl;
        iter_debug << "=============================" << std::endl;
    #endif
    
    int processed_count = 0;
    circuit_.foreach_po([&](auto signal) {
        auto po_node = circuit_.get_node(signal);
        int po_index = circuit_.node_to_index(po_node);
        FSNode& father = allFsNodes_[nowCycle_][po_index];

        #ifdef ITERDEBUG
            iter_debug << "=============================" << std::endl;
            iter_debug << "perparing  iter on node" << po_index << std::endl;
            iter_debug << "=============================" << std::endl;
        #endif
        
        iterativeReduction(father.fsL, father.optM);

        #ifdef ITERDEBUG
            iter_debug << "=============================" << std::endl;
            iter_debug << "finish  iter on node" << po_index << std::endl;
            iter_debug << "=============================" << std::endl;
        #endif

    
        std::vector<double> prob_0, prob_1;
        if (vcd_parser_.getPOOutputFromWaveform(processed_count, nowCycle_, prob_0, prob_1)) {

            Eigen::VectorXd oIV(2);
            oIV(0) = prob_0.back();
            oIV(1) = prob_1.back();

            double reliability = calculateOutputReliability(father.optM, oIV, circuit_.is_complemented(signal));
            rel << "Cycle " << nowCycle_ << ", PO " << processed_count 
                << ", Reliability: " << reliability << std::endl;
        }

        processed_count++;
    });
    
    #ifdef ITERDEBUG
        iter_debug << "Iterative Reduction completed for " << processed_count << " primary outputs" << std::endl;
    #endif

}



//parallel
void FSTRAAnalyzer::runIterativeReductionParallel(int cycle) {
    #ifdef ITERDEBUG
        #pragma omp critical
        {
            iter_debug << "Thread " << omp_get_thread_num() 
                      << ": Running Iterative Reduction on cycle " << cycle << std::endl;
        }
    #endif
    
    // 先收集所有PO信息
    struct POData {
        mockturtle::aig_network::signal signal;
        int po_index;
        int sequential_index;
        bool is_complemented;
        std::vector<int> nodefsL;
        Eigen::MatrixXd nodeOptM;
    };
    
    std::vector<POData> po_data_list;
    int processed_count = 0;
    
    // 注意：circuit_应该是只读的，所以多个线程同时访问是安全的
    circuit_.foreach_po([&](auto signal) {
        auto po_node = circuit_.get_node(signal);
        int po_index = circuit_.node_to_index(po_node);
        FSNode& father = allFsNodes_[cycle][po_index];  // 使用传入的cycle
        
        po_data_list.push_back({
            signal, 
            po_index, 
            processed_count,
            circuit_.is_complemented(signal),
            father.fsL,
            father.optM
        });
        processed_count++;
    });
    
    
    // #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < po_data_list.size(); i++) {
        auto& po_data = po_data_list[i];
        int thread_id = omp_get_thread_num();
        
        #ifdef ITERDEBUG
            #pragma omp critical
            {
                iter_debug << "Thread " << thread_id 
                          << " (outer thread " << omp_get_ancestor_thread_num(1) << ")"
                          << " processing PO " << po_data.sequential_index 
                          << " in cycle " << cycle << std::endl;
            }
        #endif
        
        // 执行迭代约减
        iterativeReductionParallel(po_data.nodefsL, po_data.nodeOptM, cycle, po_data.po_index);
        
        // 更新结果
        #pragma omp critical
        {
            FSNode& father = allFsNodes_[cycle][po_data.po_index];
            father.optM = po_data.nodeOptM;
        }
        
        // 获取波形数据并计算可靠性
        std::vector<double> prob_0, prob_1;
        if (vcd_parser_.getPOOutputFromWaveform(po_data.sequential_index, cycle, prob_0, prob_1)) {
            Eigen::VectorXd oIV(2);
            oIV(0) = prob_0.back();
            oIV(1) = prob_1.back();
            
            double reliability = calculateOutputReliability(po_data.nodeOptM, oIV, po_data.is_complemented);
            
            #pragma omp critical
            {
                rel << "Cycle " << cycle << ", PO " << po_data.sequential_index 
                    << ", Reliability: " << reliability 
                    << " (thread " << thread_id << ")" << std::endl;
            }
        }
    }
    
    #ifdef ITERDEBUG
        #pragma omp critical
        {
            iter_debug << "Thread " << omp_get_thread_num() 
                      << ": Iterative Reduction completed for cycle " << cycle << std::endl;
        }
    #endif


}


void FSTRAAnalyzer::runParallelReliabilityCalculation(const std::vector<int>& vec_int, int k) {
    std::cout << "Running Parallel Reliability Calculation for " << k << " cycles..." << std::endl;
    
    for (int i = 1; i <= k; i++) {

        nowCycle_=i;

        #ifdef FSTRADEBUG
                fstra_debug << "=============================" << std::endl;
                fstra_debug << "cycle  " << nowCycle_ ;
                fstra_debug << "  start :"  << std::endl;

                fstra_debug << "=============================" << std::endl;
        #endif

        

        runFSTracking();
        // circuit_.foreach_node([&](auto node) {
        //     int index = circuit_.node_to_index(node);
        //     FSNode& fsnode = allFsNodes_[nowCycle_][index];
        //     fsnode.cycle = i;
            
        //     // getIdealOutput();
        //     if (!circuit_.is_pi(node) && !circuit_.is_constant(node)) {
        //         fsTracking(fsnode);
        //     }
        // });
    }

    std::cout << "finish  fstra first " << std::endl;

    // #pragma omp parallel for schedule(dynamic)
    // for (int i = 1; i <= k; i++) {

    //     #ifdef ITERDEBUG
    //         #pragma omp critical
    //         {
    //             iter_debug << "=============================" << std::endl;
    //             iter_debug << "cycle  " << i ;
    //             iter_debug << "  start : (thread " << omp_get_thread_num() << ")" << std::endl;
    //             iter_debug << "=============================" << std::endl;
    //         }
    //     #endif

    //     // 使用线程安全的版本
    //     runIterativeReductionParallel(i);
    // }
    
    #pragma omp parallel for schedule(dynamic)
for (int i = 1; i <= k; i++) {
    int cycle = i;
    int thread_id = omp_get_thread_num();
    
    #ifdef ITERDEBUG
        #pragma omp critical
        {
            iter_debug << "=============================" << std::endl;
            iter_debug << "cycle  " << cycle ;
            iter_debug << "  start : (thread " << thread_id << ")" << std::endl;
            iter_debug << "=============================" << std::endl;
        }
    #endif

    // 内层循环串行处理
    int processed_count = 0;
    circuit_.foreach_po([&](auto signal) {
        auto po_node = circuit_.get_node(signal);
        int po_index = circuit_.node_to_index(po_node);
        FSNode& father = allFsNodes_[cycle][po_index];
        
        iterativeReduction(father.fsL, father.optM);
        
        std::vector<double> prob_0, prob_1;
        if (vcd_parser_.getPOOutputFromWaveform(processed_count, cycle, prob_0, prob_1)) {
            Eigen::VectorXd oIV(2);
            oIV(0) = prob_0.back();
            oIV(1) = prob_1.back();
            
            double reliability = calculateOutputReliability(father.optM, oIV, circuit_.is_complemented(signal));
            
            #pragma omp critical
            {
                rel << "Cycle " << cycle << ", PO " << processed_count 
                    << ", Reliability: " << reliability 
                    << " (thread " << thread_id << ")" << std::endl;
            }
        }
        
        processed_count++;
    });
}



    fstra_debug.close();
    iter_debug.close();
    std::cout << "Parallel Reliability Calculation completed" << std::endl;
}

void FSTRAAnalyzer::FS_TRAMethod(int cycle, int Mn_fs){
    
    getopVectors(cycle);

    for(int j=1; j <= cycle; ++j) {

        nowCycle_ = j;

        mockturtle::topo_view circuit_topo{circuit_};
        circuit_topo.foreach_node([&](auto node) {
            int index = circuit_.node_to_index(node);
            FSNode& fsnode = allFsNodes_[nowCycle_][index];

            if (!circuit_.is_pi(node) && !circuit_.is_constant(node)) {
                DimensionReduction(fsnode, Mn_fs);
            }
        });
    }


    
    for (int i = 1; i <= cycle; i++){
        nowCycle_=i;
        int processed_count = 0;
        circuit_.foreach_po([&](auto signal) {
            auto po_node = circuit_.get_node(signal);
            int po_index = circuit_.node_to_index(po_node);
            FSNode& father = allFsNodes_[nowCycle_][po_index];

            #ifdef progressDebug
                dim_red_progress << "=============================" << std::endl;
                dim_red_progress << "perparing  dim red on node" << po_index << std::endl;
                dim_red_progress << "=============================" << std::endl;
            #endif
            
            ProgramIterativeReduction(father, father.optM, Mn_fs);

            #ifdef progressDebug
                dim_red_progress << "=============================" << std::endl;
                dim_red_progress << "finish  dim red on node" << po_index << std::endl;
                dim_red_progress << "=============================" << std::endl;
            #endif

        
            std::vector<double> prob_0, prob_1;
            if (vcd_parser_.getPOOutputFromWaveform(processed_count, nowCycle_, prob_0, prob_1)) {

                Eigen::VectorXd oIV(2);
                oIV(0) = prob_0.back();
                oIV(1) = prob_1.back();

                double reliability = calculateOutputReliability(father.optM, oIV, circuit_.is_complemented(signal));
                rel << "Cycle " << nowCycle_ << ", PO " << processed_count 
                    << ", Reliability: " << reliability << std::endl;
            }

            processed_count++;
        });
    }
    

    // #pragma omp parallel for schedule(dynamic)
    // for (int i = 1; i <= k; i++) {

    //     #ifdef ITERDEBUG
    //         #pragma omp critical
    //         {
    //             iter_debug << "=============================" << std::endl;
    //             iter_debug << "cycle  " << i ;
    //             iter_debug << "  start : (thread " << omp_get_thread_num() << ")" << std::endl;
    //             iter_debug << "=============================" << std::endl;
    //         }
    //     #endif

    //     // 使用线程安全的版本
    //     runIterativeReductionParallel(i);
    // }

    fstra_debug.close();
    iter_debug.close();
    dim_red_debug.close();

}


void FSTRAAnalyzer::FS_TRAMethodByCycle(int cycle, int Mn_fs){
    
    getopVectors(cycle);

    for(int j=1; j <= cycle; ++j) {

        nowCycle_ = j;

        mockturtle::topo_view circuit_topo{circuit_};
        circuit_topo.foreach_node([&](auto node) {
            int index = circuit_.node_to_index(node);
            FSNode& fsnode = allFsNodes_[nowCycle_][index];

            if (!circuit_.is_pi(node) && !circuit_.is_constant(node) && !circuit_.is_ro(node)) {
                DimensionReductionByCycle(fsnode, Mn_fs);
            }
        });


        // Debug: 输出约简前的信息
        circuit_topo.foreach_node([&](auto node) {
            int index = circuit_.node_to_index(node);
            FSNode& fsnode = allFsNodes_[nowCycle_][index];

            if (!circuit_.is_pi(node) && !circuit_.is_constant(node) && !circuit_.is_ro(node)) {
                std::cout << "=============================" << std::endl;
                std::cout << "Cycle " << nowCycle_ << ", node index " << index << std::endl;
                std::cout << "Before Reduction - optM: " << fsnode.optM << std::endl;
                std::cout << "Before Reduction - fsL elements: ";
                for (const auto& val : fsnode.fsL) {    
                    std::cout << val << " ";
                }
                std::cout << std::endl;
                std::cout << "=============================" << std::endl;
            }
        });

        #ifdef FSTRADEBUG
                fstra_debug << "=============================" << std::endl;
                fstra_debug << "now ITER DEBUG " << std::endl;
                fstra_debug << "=============================" << std::endl;
        #endif

        #ifdef DimensionReductionDebug
                dim_red_debug << "=============================" << std::endl;
                dim_red_debug << "now ITER DEBUG " << std::endl;
                dim_red_debug << "=============================" << std::endl;
        #endif

        std::unordered_map<int, double> co_reliability;
        circuit_.foreach_co([&](auto signal,auto index) {

            auto co_node = circuit_.get_node(signal);
            int co_index = circuit_.node_to_index(co_node);
            FSNode& father = allFsNodes_[nowCycle_][co_index];

            std::cout << "Cycle " << nowCycle_ << ", processing CO index " << co_index << std::endl;

            // 如果之前未计算过该节点的可靠度，则执行约简并计算
            if (co_reliability.find(co_index) == co_reliability.end()) {
                ProgramIterativeReduction(father, father.optM, Mn_fs);
                if (index >= circuit_.num_cos() - circuit_.num_latches())//是寄存器输出
                {
                    auto ro_node =  circuit_.ri_to_ro( signal ) ;
                    int ro_index = circuit_.node_to_index(  ro_node ) ;
                    FSNode& ro_father = allFsNodes_[nowCycle_+1][ro_index];
                    ro_father.optM = father.REoptM;
                    
                    if(circuit_.is_complemented(signal)){
                        rel << "Cycle " << nowCycle_ << ", register " << index << " is complemented" << std::endl;
                        ro_father.optM.col(0).swap( ro_father.optM.col(1) );
                    }
                    co_reliability[co_index] = 1.0; //寄存器输出可靠度设为1.0 
                    rel << "Cycle " << nowCycle_ << ", register " << index 
                            << ", optM: " << ro_father.optM <<"   "<< co_index << std::endl;
                }
                else{                                  //主输出
                    std::vector<double> prob_0, prob_1;
                    if (vcd_parser_.getPOOutputFromWaveform(index, nowCycle_, prob_0, prob_1)) {
                    
                        Eigen::VectorXd oIV(2);
                        oIV(0) = prob_0.back();
                        oIV(1) = prob_1.back();
                    
                        double reliability = calculateOutputReliability(father.REoptM, oIV, circuit_.is_complemented(signal));
                        co_reliability[co_index] = reliability; // 缓存结果
                        rel << "Cycle " << nowCycle_ << ", PO " << index 
                            << ", Reliability: " << reliability << std::endl;
                        
                    }
                }
            }
            else{
                if (index >= circuit_.num_cos() - circuit_.num_latches())
                {
                    auto ro_node =  circuit_.ri_to_ro( signal ) ;
                    int ro_index = circuit_.node_to_index(  ro_node ) ;
                    FSNode& ro_father = allFsNodes_[nowCycle_+1][ro_index];
                    ro_father.optM = father.REoptM;
                    
                    if(circuit_.is_complemented(signal)){
                        ro_father.optM.col(0).swap( ro_father.optM.col(1) );
                    }     
                    rel << "Cycle " << nowCycle_ << ", register " << index 
                            << ", optM: " << ro_father.optM <<"  father node :"<< co_index << std::endl;
                }
                else{
                    std::vector<double> prob_0, prob_1;
                    if (vcd_parser_.getPOOutputFromWaveform(index, nowCycle_, prob_0, prob_1)) {
                    
                        Eigen::VectorXd oIV(2);
                        oIV(0) = prob_0.back();
                        oIV(1) = prob_1.back();
                    
                        double reliability = calculateOutputReliability(father.REoptM, oIV,circuit_.is_complemented(signal));
                        co_reliability[co_index] = reliability; // 缓存结果
                        rel << "Cycle " << nowCycle_ << ", PO " << index 
                            << ", Reliability: " << reliability << std::endl;
                        
                    }
                }
            }
        });
    }


}

std::vector<std::reference_wrapper<FSTRAAnalyzer::FSNode>> FSTRAAnalyzer::getPrimaryInputs() {
    std::vector<std::reference_wrapper<FSNode>> inputs;
    
    circuit_.foreach_pi([&](auto node) {
        int index = circuit_.node_to_index(node);
        inputs.emplace_back(allFsNodes_[nowCycle_][index]);
    });
    
    return inputs;
}

std::vector<std::reference_wrapper<FSTRAAnalyzer::FSNode>> FSTRAAnalyzer::getPrimaryOutputs() {
    std::vector<std::reference_wrapper<FSNode>> outputs;
    
    circuit_.foreach_po([&](auto signal) {
        auto node = circuit_.get_node(signal);
        int index = circuit_.node_to_index(node);
        outputs.emplace_back(allFsNodes_[nowCycle_][index]);
    });
    
    return outputs;
}

void FSTRAAnalyzer::printFSNodeInfo(int index) const {


    if (index < 0 || index >= allFsNodes_[nowCycle_].size()) {
        std::cout << "Invalid FS node index: " << index << std::endl;
        return;
    }
    
    const FSNode& node = allFsNodes_[nowCycle_][index];
    std::cout << "=== FS Node " << index << " ===" << std::endl;
    std::cout << "Has fanout branch: " << (node.hasFanoutBranch ? "Yes" : "No") << std::endl;
    std::cout << "Is sequential: " << (node.isSequential ? "Yes" : "No") << std::endl;
    std::cout << "Cycle: " << node.cycle << std::endl;
    std::cout << "FSL size: " << node.fsL.size() << std::endl;
    std::cout << "iptM size: " << node.iptM.rows() << "x" << node.iptM.cols() << std::endl;
    std::cout << "optM size: " << node.optM.rows() << "x" << node.optM.cols() << std::endl;
    std::cout << "ptm size: " << node.ptm.rows() << "x" << node.ptm.cols() << std::endl;
}

void FSTRAAnalyzer::getopVectors(int cycle) {


    for (int i = 1; i <= cycle; ++i) {

            std::unordered_map<std::string, std::pair<std::vector<double>, std::vector<double>>> all_outputs;
            if (vcd_parser_.getAllNodeOutputsFromWaveform(i, all_outputs)) {

                // 遍历所有节点
                for (const auto& kv : all_outputs) {
                    // std::cout << "Node: " << kv.first.c_str() << std::endl;
                    std::string name = kv.first.c_str();
                    const auto& probs = kv.second;

                    int signal_index = extractSignalIndex(name);
                    opVectors_[i][signal_index] = Eigen::Vector2d(probs.first.back(), probs.second.back());
                    // std::cout<<"cycle: "<<i << "  prob: " << opVectors_[i][signal_index].transpose() << std::endl;
                }
            }
    }
}

int FSTRAAnalyzer::extractSignalIndex(const std::string& name) {
    // 直接提取所有数字字符
    std::string digits;
    for (char c : name) {
        if (c >= '0' && c <= '9') {
            digits += c;
        }
    }
    
    // 如果没有数字，返回-1
    if (digits.empty()) {
        return -1;
    }
    
    // 尝试转换为整数
    try {
        return std::stoi(digits);
    } catch (...) {
        return -1;
    }
}


void FSTRAAnalyzer::calPriorities(int cycle){
    double theta=0.8;
    std::vector<double> py_pre,py_suc,py1,py2;
    runFSTracking();
    
    mockturtle::topo_view circuit_topo{circuit_};
    circuit_topo.foreach_node([&](auto node) {
        int index = circuit_.node_to_index(node);
        FSNode& fsnode = allFsNodes_[nowCycle_][index];
        
    
        if(circuit_.is_ci(node)){
            node_priorities_[index]=1.0;
        }
        else{
            circuit_.foreach_fanin(node,[&]( auto signal){
                auto fanin_node = circuit_.get_node(signal);
                int fanin_index = circuit_.node_to_index(fanin_node);
                py_pre[index]+=py_pre[fanin_index];
            });
            py_pre[index]*=theta;
            py_pre[index]+=fsnode.fsL.size();
        }
    });
    
    mockturtle::depth_view dep_cir{circuit_};

    circuit_.foreach_node([&](auto node) {
        int index = circuit_.node_to_index(node);
        FSNode& fsnode = allFsNodes_[nowCycle_][index];
        py_suc[index]=dep_cir.depth()-dep_cir.level(node);
        node_priorities_[index]=py_pre[index]+py_suc[index];
    });

    
    
}