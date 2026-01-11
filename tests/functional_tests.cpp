#include "../include/ipc_protocol.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

void test_variable_size() {
    std::cout << "Test 1: Variable Size Messages" << std::endl;
    
    IPC::MessageFragmenter fragmenter;
    std::vector<size_t> test_sizes = {500, 10240, 102400, 1048576, 10485760};
    
    for (size_t size : test_sizes) {
        std::vector<char> data(size, 'A');
        auto fragments = fragmenter.fragment_message(data);
        
        std::cout << "Size: " << size << " bytes -> " 
                  << fragments.size() << " fragments" << std::endl;
        
        IPC::MessageAssembler assembler;
        for (const auto& frag : fragments) {
            assembler.add_fragment(frag);
        }
        
        auto assembled = assembler.get_assembled_message(fragments[0].header.message_id);
        if (assembled.complete && assembled.data.size() == size) {
            std::cout << "Correctly assembled" << std::endl;
        } else {
            std::cout << "Assembly failed" << std::endl;
        }
    }
}

void test_nonblocking_behavior() {
    std::cout << "\nTest 2: Non-blocking Behavior" << std::endl;
    
    std::cout << "Testing send status handling..." << std::endl;
    
    // Здесь можно эмулировать различные сценарии:
    // 1. SUCCESS - нормальная отправка
    // 2. BUFFER_FULL - буфер переполнен
    // 3. NO_RECEIVER - получатель отключился
    // 4. PIPE_ERROR - ошибка канала
    
    std::cout << "  (Requires actual IPC connection to test fully)" << std::endl;
}

void test_ack_mechanism() {
    std::cout << "\nTest 3: ACK Mechanism" << std::endl;
    std::cout << "ACK packets defined but not yet implemented in transport layer" << std::endl;
    std::cout << "Structure: message_id + received_fragments + complete flag" << std::endl;
}

int main() {
    std::cout << "IPC Functional Tests" << std::endl;
    
    test_variable_size();
    test_nonblocking_behavior();
    test_ack_mechanism();
    
    std::cout << "\nAll tests completed" << std::endl;
    return 0;
}