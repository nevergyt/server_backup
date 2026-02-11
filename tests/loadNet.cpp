#include <lorina/aiger.hpp>
#include <lorina/verilog.hpp>
#include <mockturtle/mockturtle.hpp>
// #include <mockturtle/networks/sequential.hpp>
#include <Eigen/Dense>
#include <vector>
#include <unsupported/Eigen/KroneckerProduct>
#include <kitty/kitty.hpp>

typedef struct fsNode
{
  int in;
  int out;
  int index;
  int cycle;
  // mockturtle::aig_network::node node;
  Eigen::MatrixXd ptm;
  Eigen::MatrixXd iptM;
  Eigen::MatrixXd optM;
  std::vector<int> fsL;
  bool hasFanoutBranch;
  bool isSequential;
  std::vector<double> rel;
}fsNode,PfsNode;

mockturtle::aig_network aig;
std::vector<fsNode *> allFsNode;
Eigen::Matrix<double,4,2> Mff;
double faultRate;



std::pair<int, int> decompose_binary_code(int full_code, int total_bits, int bits1, int bits2) {
        
        int code1 = 0, code2 = 0;

        code1=full_code%(1<<bits1);
        code2=full_code>>(total_bits-bits2);
        
        return {code1, code2};
}

Eigen::VectorXd get_row_by_binary(const Eigen::MatrixXd& matrix, const std::vector<int>& fsL, int binary_code) {
    if (matrix.rows() == 0) {
        return Eigen::VectorXd::Ones(1);  
    }
    
    if (fsL.empty()) {
        return matrix.row(0);
    }
    
    int row_index = binary_code % matrix.rows();
    return matrix.row(row_index);
}


// Eigen::MatrixXd truthTableToMatrixWithInputs(const kitty::dynamic_truth_table& tt) {
//     int num_vars = tt.num_vars();
//     int num_rows = 1 << num_vars;
    
//     Eigen::MatrixXd matrix(num_rows, num_vars + 1);
    
//     for (int i = 0; i < num_rows; ++i) {
    
//         for (int j = 0; j < num_vars; ++j) {
//             matrix(i, j) = (i >> (num_vars - 1 - j)) & 1 ? 1.0 : 0.0;
//         }
        
//         matrix(i, num_vars) = kitty::get_bit(tt, i) ? 1.0 : 0.0;
//     }
    
//     return matrix;
// }

// void rm_dupElems(fsNode& fsnode ,std::vector<int>& tmpFsL,Eigen::MatrixXd  tmpM){

//   //get comFsL
//   std::vector<int>comFsL;

//   for(auto it = fsnode.fsL.rbegin(); it != fsnode.fsL.rend(); ++it){
//     if(std::find(comFsL.begin(),comFsL.end(),*it)==comFsL.end()){
//       comFsL.push_back(*it);
//     }
//   }

//   for(auto it = tmpFsL.rbegin(); it != tmpFsL.rend(); ++it){
//     if(std::find(comFsL.begin(),comFsL.end(),*it)==comFsL.end()){
//       comFsL.push_back(*it);
//     }
//   }
  
//   std::reverse(comFsL.begin(),comFsL.end());
  
//   //get comM
//   int dem=comFsL.size();
//   int new_rows = 1 << dem;
//   int new_cols = fsnode.iptM.cols()*tmpM.cols();
  
//   Eigen::MatrixXd com_iptM = Eigen::MatrixXd::Zero(new_rows, new_cols);

//   #pragma omp parallel for
//         for (int binary_code = 0; binary_code < new_rows; binary_code++) {
            
//             auto [binary1, binary2] = decompose_binary_code(
//                 binary_code, dem, fsnode.fsL.size(), tmpFsL.size());
            
            
//             Eigen::VectorXd row1 = get_row_by_binary(fsnode.iptM, fsnode.fsL, binary1);
//             Eigen::VectorXd row2 = get_row_by_binary(tmpM, tmpFsL, binary2);
            
            
//             Eigen::MatrixXd tensor_prod = Eigen::kroneckerProduct(row1, row2.transpose());
            
//             com_iptM.row(binary_code) = tensor_prod;
//         }

//   //update
//   fsnode.fsL=comFsL;
//   fsnode.iptM=com_iptM;
// }

void rm_dupElems(Eigen::MatrixXd &nodeIptM,std::vector<int> &nodeFsL,std::vector<int>& tmpFsL,Eigen::MatrixXd  tmpM){

  //get comFsL
  std::vector<int>comFsL;

  for(auto it = nodeFsL.rbegin(); it != nodeFsL.rend(); ++it){
    if(std::find(comFsL.begin(),comFsL.end(),*it)==comFsL.end()){
      comFsL.push_back(*it);
    }
  }

  for(auto it = tmpFsL.rbegin(); it != tmpFsL.rend(); ++it){
    if(std::find(comFsL.begin(),comFsL.end(),*it)==comFsL.end()){
      comFsL.push_back(*it);
    }
  }
  
  std::reverse(comFsL.begin(),comFsL.end());
  
  //get comM
  int dem=comFsL.size();
  int new_rows = 1 << dem;
  int new_cols = nodeIptM.cols()*tmpM.cols();
  
  Eigen::MatrixXd com_iptM = Eigen::MatrixXd::Zero(new_rows, new_cols);

  #pragma omp parallel for
        for (int binary_code = 0; binary_code < new_rows; binary_code++) {
            
            auto [binary1, binary2] = decompose_binary_code(
                binary_code, dem, nodeFsL.size(), tmpFsL.size());
            
            
            Eigen::VectorXd row1 = get_row_by_binary(nodeIptM, nodeFsL, binary1);
            Eigen::VectorXd row2 = get_row_by_binary(tmpM, tmpFsL, binary2);
            
            
            Eigen::MatrixXd tensor_prod = Eigen::kroneckerProduct(row1, row2.transpose());
            
            com_iptM.row(binary_code) = tensor_prod;
        }

  //update
  nodeFsL=comFsL;
  nodeIptM=com_iptM;
}



//algorithm 1
void fsTracking(fsNode& fsnode){
  //initial
  fsnode.fsL.clear();
  fsnode.iptM=Eigen::MatrixXd::Identity(1,1);


  aig.foreach_fanin(aig.index_to_node(fsnode.index),[&](auto signal){
    fsNode * father=allFsNode[aig.node_to_index(aig.get_node(signal))];
    if(father->hasFanoutBranch)rm_dupElems(fsnode.iptM,fsnode.fsL,father->fsL,father->optM);
    else {
      std::vector<int> tmp;
      tmp.push_back(father->index);
      rm_dupElems(fsnode.iptM,fsnode.fsL,tmp,Eigen::Matrix2d::Identity());
    }
  });

  fsnode.optM=fsnode.iptM*fsnode.ptm;

}


//algorithm 2
void iterReduction(std::vector<int> nodefsL,Eigen::MatrixXd nodeOptM){
    Eigen::MatrixXd com_redM=Eigen::MatrixXd::Identity(1,1);
    while(!nodefsL.empty()){
      int max=*max_element(nodefsL.begin(),nodefsL.end()); 
      fsNode *lsNode= allFsNode[max];  
      Eigen::MatrixXd redM=Eigen::MatrixXd::Identity(1,1);
      std::vector<int> tmp_fsL;
      for(auto it=nodefsL.begin();it!=nodefsL.end();it++){
        if(*it!=max){
          std::vector<int> t;
          t.push_back(max);
          rm_dupElems(redM,tmp_fsL,t,Eigen::MatrixXd::Identity(2,2));
        }
      }
      com_redM=redM*com_redM;
      nodefsL=tmp_fsL;

    }
    nodeOptM=com_redM*nodeOptM;
};

void getIdealOp(){

};

double calculate_output_reliability(Eigen::MatrixXd optM, Eigen::VectorXd oIV) {
      
        double reliability = 0.0;
        for (int i = 0; i < oIV.rows(); ++i) {
            reliability += optM(i) * oIV(i);
        }
        return reliability;
    
    std::vector<bool> current_input_vector;
};


//algorithm 3
void paraRelCal( std::vector<int> vec_int, int k){
    for(int i=1;i<=k;i++){
      aig.foreach_node([&](auto node){
        fsNode * fsnode=allFsNode[aig.node_to_index(node)];
        getIdealOp();                 //SCA or iverilog??
        fsTracking(*fsnode);
        
      } );
    }


    for(int i=1;i<=k;i++){
      aig.foreach_po([&] (auto signal) {
          fsNode * father=allFsNode[aig.node_to_index(aig.get_node(signal))];
          iterReduction(father->fsL,father->optM);
      } );

    }
  
};



void create_fsNode()
{
  aig.foreach_node([&](auto node){
    fsNode * newNode=new fsNode;
    newNode->index=aig.node_to_index(node);
    if(aig.fanout_size(node)!=1)newNode->hasFanoutBranch=true;
    else newNode->hasFanoutBranch=false;
    
    if(!aig.is_pi(node)){
      std::cout <<aig.node_to_index(node) << " Print function:"  << std::endl;
      // kitty::print_kmap(aig.node_function(node), std::cout);
      // std::cout<<truthTableToMatrixWithInputs(aig.node_function(node))<<std::endl;
    }

    allFsNode.push_back(newNode); 
  });
}

int main()
{
  
  auto const result = lorina::read_aiger( "../src/benchmarks89/tests/s27.aig", mockturtle::aiger_reader( aig ) );
  if ( result != lorina::return_code::success )
  {
    std::cout << "Read benchmark failed\n";
    return -1;
  }
  faultRate=0.01;

  Mff<< 1-faultRate,faultRate,
        1-faultRate,faultRate,
        1-faultRate,faultRate,
        faultRate,1-faultRate;

  std::cout<<Mff;

  mockturtle::topo_view aig2{aig};

  // create_fsNode();

  aig2.foreach_register(
    [&] (auto node){
      std::cout<<"\nregister:";
      std::cout<<aig2.node_to_index(node.second)<<", ";

    }
  );

  aig2.foreach_pi(
    [&] (auto node){
      std::cout<<"\ninput:";
      std::cout<<aig2.node_to_index(node)<<", ";
    }
  );

  aig2.foreach_po(
    [&] (auto node){
      std::cout<<"\noutput:";
      std::cout<<aig2.node_to_index(aig2.get_node(node))<<", ";
    }
  );


  // Eigen::MatrixXd m = Eigen::MatrixXd::Random(3, 3);
  // m = (m + Eigen::MatrixXd::Constant(3, 3, 1.2)) * 50;
  // std::cout << "m =" << std::endl << m << std::endl;
  // Eigen::VectorXd v(3);
  // v << 1, 2, 3;
  // std::cout << "m * v =" << std::endl << m * v << std::endl;
  
  // rm_dupElems();

  // std::cout<<aig.num_gates()<<"\n";
  // std::cout<<aig.size()<<"\n";

  // aig2.foreach_node(
  //   [&] (auto node){
  //     std::cout<<aig2.node_to_index(node)<<":  ";
  //     aig2.foreach_fanin(node,[&](auto signal){

  //       std::cout<<aig2.node_to_index(aig2.get_node(signal))<<" ";
  //     });
  //     std::cout<<"\n";
  //   }
  // );


  return 0;
}
