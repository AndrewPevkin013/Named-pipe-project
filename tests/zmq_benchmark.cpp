#include "common/benchmark_utils.hpp"

#include <zmq.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

constexpr const char* ENDPOINT =
#ifdef _WIN32
    "tcp://127.0.0.1:5557";
#else
    "ipc:///tmp/zmq_ipc_test";
#endif

constexpr int MESSAGE_COUNT = 10000;
constexpr size_t MESSAGE_SIZE = 1024;

void receiver_thread() {
    zmq::context_t context(1);
    zmq::socket_t receiver(context, zmq::socket_type::pair);

    receiver.bind(ENDPOINT);

    for (int i = 0; i < MESSAGE_COUNT; ++i) {
        zmq::message_t message;
        receiver.recv(message, zmq::recv_flags::none);

        zmq::message_t ack("ACK", 3);
        receiver.send(ack, zmq::send_flags::none);
    }
}

int main() {
    std::thread receiver(receiver_thread);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    zmq::context_t context(1);
    zmq::socket_t sender(context, zmq::socket_type::pair);
    sender.connect(ENDPOINT);

    auto data = bench::generate_data(MESSAGE_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MESSAGE_COUNT; ++i) {
        zmq::message_t message(data.data(), data.size());
        sender.send(message, zmq::send_flags::none);

        zmq::message_t ack;
        sender.recv(ack, zmq::recv_flags::none);
    }

    auto end = std::chrono::high_resolution_clock::now();

    receiver.join();

    double ms = bench::ms_since(start, end);

    std::cout << "ZeroMQ benchmark\n";
    std::cout << "Messages: " << MESSAGE_COUNT << "\n";
    std::cout << "Message size(bytes): " << MESSAGE_SIZE << "\n";
    std::cout << "Time(ms): " << ms << "\n";
    std::cout << "Messages/sec: " << (MESSAGE_COUNT * 1000.0 / ms) << "\n";
    std::cout << "Note: ACK is implemented at application level for fair comparison\n";
#ifdef _WIN32
    system("pause");
#endif
    return 0;
}