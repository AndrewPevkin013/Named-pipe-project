#include <zmq.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
    zmq::context_t context(1);

    const std::string endpoint = "inproc://ack_semantics_test";

    std::atomic<bool> receiver_processed{false};

    std::thread receiver_thread([&]() {
        zmq::socket_t receiver(context, zmq::socket_type::pair);
        receiver.bind(endpoint);

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        zmq::message_t request;
        auto recv_result = receiver.recv(request, zmq::recv_flags::none);

        if (recv_result) {
            receiver_processed = true;

            std::string ack = "ACK";
            zmq::message_t reply(ack.data(), ack.size());
            receiver.send(reply, zmq::send_flags::none);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    zmq::socket_t sender(context, zmq::socket_type::pair);
    sender.connect(endpoint);

    std::string message = "test-message";
    zmq::message_t request(message.data(), message.size());

    auto send_start = std::chrono::high_resolution_clock::now();

    auto send_result = sender.send(request, zmq::send_flags::none);

    auto send_end = std::chrono::high_resolution_clock::now();

    bool processed_immediately_after_send = receiver_processed.load();

    zmq::message_t reply;
    auto ack_start = std::chrono::high_resolution_clock::now();

    auto recv_result = sender.recv(reply, zmq::recv_flags::none);

    auto ack_end = std::chrono::high_resolution_clock::now();

    double send_ms =
        std::chrono::duration<double, std::milli>(send_end - send_start).count();

    double ack_wait_ms =
        std::chrono::duration<double, std::milli>(ack_end - ack_start).count();

    std::cout << "ZeroMQ ACK semantics test\n";
    std::cout << "Send returned: "
              << (send_result ? "yes" : "no") << "\n";
    std::cout << "Send time(ms): " << send_ms << "\n";
    std::cout << "Receiver processed immediately after send: "
              << (processed_immediately_after_send ? "yes" : "no") << "\n";
    std::cout << "Application ACK received: "
              << (recv_result ? "yes" : "no") << "\n";
    std::cout << "ACK wait time(ms): " << ack_wait_ms << "\n";
    std::cout << "Conclusion: ZeroMQ send completion does not mean "
                 "application-level processing confirmation. "
                 "ACK must be implemented separately.\n";

    receiver_thread.join();

#ifdef _WIN32
    system("pause");
#endif

    return 0;
}