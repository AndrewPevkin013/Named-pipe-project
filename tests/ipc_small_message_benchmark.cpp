#include "../include/ipc_sender.hpp"
#include "common/benchmark_utils.hpp"

#include <chrono>
#include <iostream>

constexpr int MESSAGE_COUNT = 10000;
constexpr size_t MESSAGE_SIZE = 1024;

int main() {
//     IPCSender sender(1);
//     auto data = bench::generate_data(MESSAGE_SIZE);

//     auto start = std::chrono::high_resolution_clock::now();

//     int success = 0;
//     int failed = 0;

//     for (int i = 0; i < MESSAGE_COUNT; ++i) {
//         std::string msg(data.begin(), data.end());

//         if (sender.send(msg)) {
//             ++success;
//         } else {
//             ++failed;
//         }
//     }

//     auto end = std::chrono::high_resolution_clock::now();
//     double ms = bench::ms_since(start, end);

//     std::cout << "IPC small message benchmark\n";
//     std::cout << "Messages: " << MESSAGE_COUNT << "\n";
//     std::cout << "Message size(bytes): " << MESSAGE_SIZE << "\n";
//     std::cout << "Success: " << success << "\n";
//     std::cout << "Failed: " << failed << "\n";
//     std::cout << "Time(ms): " << ms << "\n";
//     std::cout << "Messages/sec: " << (MESSAGE_COUNT * 1000.0 / ms) << "\n";

// #ifdef _WIN32
//     system("pause");
// #endif
//     return failed == 0 ? 0 : 1;
    
}