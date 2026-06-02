#include "common/benchmark_utils.hpp"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/exceptions.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace boost::interprocess;

constexpr const char* QUEUE_NAME = "boost_large_message_queue";

constexpr size_t LOGICAL_MESSAGE_SIZE = 1024ull * 1024ull * 1024ull;
constexpr size_t MAX_MSG_SIZE = 1024ull * 1024ull;
constexpr size_t MAX_MSG_COUNT = 64;
constexpr size_t CHUNK_COUNT = LOGICAL_MESSAGE_SIZE / MAX_MSG_SIZE;

void receiver_thread() {
    message_queue mq(open_only, QUEUE_NAME);

    std::vector<char> buffer(MAX_MSG_SIZE);
    size_t received_size = 0;
    unsigned priority = 0;

    size_t total_received = 0;

    for (size_t i = 0; i < CHUNK_COUNT; ++i) {
        mq.receive(buffer.data(), buffer.size(), received_size, priority);
        total_received += received_size;
    }

    std::cout << "Boost receiver total received(MB): "
              << total_received / 1024 / 1024 << "\n";
}

int main() {
    message_queue::remove(QUEUE_NAME);

    message_queue mq(
        create_only,
        QUEUE_NAME,
        MAX_MSG_COUNT,
        MAX_MSG_SIZE
    );

    std::cout << "Boost message_queue large message benchmark\n";
    std::cout << "Logical message size(MB): "
              << LOGICAL_MESSAGE_SIZE / 1024 / 1024 << "\n";
    std::cout << "Queue max message size(KB): "
              << MAX_MSG_SIZE / 1024 << "\n";

    try {
        std::vector<char> oversized(MAX_MSG_SIZE + 1, 'x');
        mq.send(oversized.data(), oversized.size(), 0);

        std::cout << "Unexpected: oversized message was sent\n";
    } catch (const interprocess_exception& ex) {
        std::cout << "Expected limitation: cannot send message larger than max_msg_size\n";
        std::cout << "Boost exception: " << ex.what() << "\n";
    }

    std::thread receiver(receiver_thread);

    std::vector<char> chunk(MAX_MSG_SIZE, 'x');

    size_t mem_before = bench::memory_usage_bytes();
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < CHUNK_COUNT; ++i) {
        mq.send(chunk.data(), chunk.size(), 0);
    }

    receiver.join();

    auto end = std::chrono::high_resolution_clock::now();
    size_t mem_after = bench::memory_usage_bytes();

    double ms = bench::ms_since(start, end);
    double throughput = bench::mb_per_sec(LOGICAL_MESSAGE_SIZE, ms);

    double mem_delta_mb =
        static_cast<double>(
            static_cast<long long>(mem_after) -
            static_cast<long long>(mem_before)
        ) / (1024.0 * 1024.0);

    std::cout << "Manual chunk transfer result\n";
    std::cout << "Chunks: " << CHUNK_COUNT << "\n";
    std::cout << "Chunk size(KB): " << MAX_MSG_SIZE / 1024 << "\n";
    std::cout << "Time(ms): " << ms << "\n";
    std::cout << "Throughput(MB/s): " << throughput << "\n";
    std::cout << "Memory delta(MB): " << mem_delta_mb << "\n";
    std::cout << "Note: fragmentation is implemented manually in benchmark code\n";

    message_queue::remove(QUEUE_NAME);
#ifdef _WIN32
    system("pause");
#endif
    return 0;
}