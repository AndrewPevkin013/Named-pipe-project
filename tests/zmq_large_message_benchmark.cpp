#include "common/benchmark_utils.hpp"

#include <zmq.hpp>

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef _WIN32
constexpr const char* ENDPOINT = "tcp://127.0.0.1:5558";
#else
constexpr const char* ENDPOINT = "ipc:///tmp/zmq_large_message_test";
#endif

constexpr size_t MESSAGE_SIZE = 1024ull * 1024ull * 1024ull;

void receiver_thread() {
    zmq::context_t context(1);
    zmq::socket_t receiver(context, zmq::socket_type::pair);

    receiver.set(zmq::sockopt::linger, 0);
    receiver.bind(ENDPOINT);

    zmq::message_t message;
    auto result = receiver.recv(message, zmq::recv_flags::none);

    if (!result) {
        std::cerr << "ZeroMQ receiver failed\n";
        return;
    }

    std::cout << "ZeroMQ receiver received(MB): "
              << message.size() / 1024 / 1024 << "\n";

    zmq::message_t ack("ACK", 3);
    receiver.send(ack, zmq::send_flags::none);
}

int main() {
    std::cout << "ZeroMQ large message benchmark\n";
    std::cout << "Message size(MB): " << MESSAGE_SIZE / 1024 / 1024 << "\n";

    std::thread receiver(receiver_thread);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    zmq::context_t context(1);
    zmq::socket_t sender(context, zmq::socket_type::pair);

    sender.set(zmq::sockopt::linger, 0);
    sender.connect(ENDPOINT);

    size_t mem_before = bench::memory_usage_bytes();

    zmq::message_t message(MESSAGE_SIZE);
    std::memset(message.data(), 'x', MESSAGE_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    sender.send(message, zmq::send_flags::none);

    zmq::message_t ack;
    auto ack_result = sender.recv(ack, zmq::recv_flags::none);

    auto end = std::chrono::high_resolution_clock::now();
    size_t mem_after = bench::memory_usage_bytes();

    receiver.join();

    bool ok = ack_result.has_value();

    double ms = bench::ms_since(start, end);
    double throughput = bench::mb_per_sec(MESSAGE_SIZE, ms);

    double mem_delta_mb =
        static_cast<double>(
            static_cast<long long>(mem_after) -
            static_cast<long long>(mem_before)
        ) / (1024.0 * 1024.0);

    std::cout << "Success: " << (ok ? "yes" : "no") << "\n";
    std::cout << "Time(ms): " << ms << "\n";
    std::cout << "Throughput(MB/s): " << throughput << "\n";
    std::cout << "Memory delta(MB): " << mem_delta_mb << "\n";
    std::cout << "Note: ACK is implemented at application level for fair comparison\n";

#ifdef _WIN32
    system("pause");
#endif
    return ok ? 0 : 1;
}