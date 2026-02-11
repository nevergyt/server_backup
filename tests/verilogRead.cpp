#include <iostream>
#include <string>
#include <fstream>
#include <mockturtle/mockturtle.hpp>
#include <lorina/lorina.hpp>

int main() {
    // 创建一个简单的测试 Verilog 文件
    const std::string test_verilog = R"(
module simple_test(a, b, c, y);
  input a, b, c;
  output y;
  wire w1;
  
  and AND1(w1, a, b);
  or OR1(y, w1, c);
endmodule
)";
    
    // 写入临时文件
    std::ofstream file("simple_test.v");
    file << test_verilog;
    file.close();
    
    std::cout << "Created test file: simple_test.v" << std::endl;
    
    // 尝试读取 - 不使用诊断引擎
    mockturtle::aig_network aig;
    
    auto result = lorina::read_verilog("simple_test.v", mockturtle::verilog_reader(aig));
    
    std::cout << "Read result: " << static_cast<int>(result) << std::endl;
    
    if (result == lorina::return_code::success) {
        std::cout << "SUCCESS: Verilog file parsed!" << std::endl;
        std::cout << "Network stats:" << std::endl;
        std::cout << "  PIs: " << aig.num_pis() << std::endl;
        std::cout << "  POs: " << aig.num_pos() << std::endl;
        std::cout << "  Gates: " << aig.num_gates() << std::endl;
    } else {
        std::cout << "FAILED: Could not parse Verilog" << std::endl;
    }
    
    return 0;
}