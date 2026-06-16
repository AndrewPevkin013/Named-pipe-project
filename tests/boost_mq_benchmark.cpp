#include "common/benchmark_utils.hpp"

#include <boost/interprocess/ipc/message_queue.hpp>

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

using namespace boost::interprocess;

constexpr const char* QUEUE_NAME = "boost_ipc_test_queue";
constexpr size_t MESSAGE_SIZE = 1024;
constexpr int MESSAGE_COUNT = 10000;
constexpr size_t MAX_MSG_SIZE = 1024;
constexpr size_t MAX_MSG_COUNT = 128;

void receiver_thread() {
    message_queue mq(open_only, QUEUE_NAME);

    std::vector<char> buffer(MAX_MSG_SIZE);
    size_t received_size = 0;
    unsigned priority = 0;

    for (int i = 0; i < MESSAGE_COUNT; ++i) {
        mq.receive(buffer.data(), buffer.size(), received_size, priority);
    }
}

int main() {
//     message_queue::remove(QUEUE_NAME);

//     message_queue mq(
//         create_only,
//         QUEUE_NAME,
//         MAX_MSG_COUNT,
//         MAX_MSG_SIZE
//     );

//     std::thread receiver(receiver_thread);

//     auto data = bench::generate_data(MESSAGE_SIZE);

//     auto start = std::chrono::high_resolution_clock::now();

//     for (int i = 0; i < MESSAGE_COUNT; ++i) {
//         mq.send(data.data(), data.size(), 0);
//     }

//     receiver.join();

//     auto end = std::chrono::high_resolution_clock::now();

//     double ms = bench::ms_since(start, end);

//     std::cout << "Boost message_queue benchmark\n";
//     std::cout << "Messages: " << MESSAGE_COUNT << "\n";
//     std::cout << "Message size(bytes): " << MESSAGE_SIZE << "\n";
//     std::cout << "Time(ms): " << ms << "\n";
//     std::cout << "Messages/sec: " << (MESSAGE_COUNT * 1000.0 / ms) << "\n";
//     std::cout << "Note: max message size is fixed at queue creation\n";

//     message_queue::remove(QUEUE_NAME);
// #ifdef _WIN32
//     system("pause");
// #endif
    return 0;
}