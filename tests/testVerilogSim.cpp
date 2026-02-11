#include "verilog_simulator.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <vector>

void test_basic_simulation() {
    std::cout << "\n=== Testing Basic Simulation ===" << std::endl;
    
    VerilogSimulator simulator;
    
    // simulator.setWorkingDirectory("..");
    
    std::cout << "Step 1: Compiling Verilog files..." << std::endl;
    bool compile_result = simulator.compile("../src/ISCAS89/s27.v", "../src/ISCAS89/s27_tb.v");
    
    if (!compile_result) {
        std::cerr << " Compilation failed!" << std::endl;
        std::cerr << "Compile errors: " << simulator.getCompileErrors() << std::endl;
        return;
    }
    
    // std::cout << " Compilation successful!" << std::endl;
    

    std::cout << "Step 2: Running simulation..." << std::endl;

    bool warningDetected = false;

    simulator.simulateRealtime(
        {"../src/ISCAS89/s27.v", "../src/ISCAS89/s27_tb.v"},
        [&warningDetected](const std::string& output) {
            std::cout << "[SIM] " << output << std::endl;
            
            if (output.find("Warning:") != std::string::npos ||
                output.find("warning") != std::string::npos) {
                warningDetected = true;
                std::cout << "  ->   WARNING DETECTED!" << std::endl;
            }
            
            if (output.find("Error:") != std::string::npos ||
                output.find("error") != std::string::npos) {
                std::cout << "  ->  ERROR DETECTED!" << std::endl;
            }
        },
        false
    );

    // auto result = simulator.runSimulation(false);
    

    // std::cout << "\n=== Simulation Results ===" << std::endl;
    // std::cout << "Success: " << (result.success ? " YES" : " NO") << std::endl;
    // std::cout << "Exit code: " << result.exitCode << std::endl;
    
    // if (!result.success) {
    //     std::cerr << "Error: " << result.error << std::endl;
    // }
    
    // std::cout << "\n=== Simulation Output ===" << std::endl;
    // std::cout << result.output << std::endl;
    

    // if (result.output.find("AND Gate Simulation Completed") != std::string::npos) {
    //     std::cout << " Simulation completed successfully!" << std::endl;
    // }
    

    remove("sim_output"); 
}


int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "    Verilog Simulator Comprehensive Test" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    try {
        test_basic_simulation();
        
        std::cout << "\n=========================================" << std::endl;
        std::cout << "        All Tests Completed!" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n Test failed with unknown exception" << std::endl;
        return 1;
    }
}