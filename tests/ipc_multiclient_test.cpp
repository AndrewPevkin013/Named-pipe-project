#include "../include/ipc_sender.hpp"
#include "common/benchmark_utils.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

constexpr int CLIENT_COUNT = 5;
constexpr int MESSAGES_PER_CLIENT = 100;
constexpr size_t MESSAGE_SIZE = 200 * 1024;
constexpr size_t FRAGMENT_SIZE = 64 * 1024;

std::atomic<int> success_count{0};
std::atomic<int> failed_count{0};
std::mutex cout_mutex;

void client_worker(int client_id) {
    IPCSender sender(static_cast<uint32_t>(client_id));

    for (int i = 0; i < MESSAGES_PER_CLIENT; ++i) {
        auto data = bench::generate_data(MESSAGE_SIZE, static_cast<char>('a' + client_id));

        std::optional<uint64_t> message_id;

        bool ok = sender.get_fragmenter().fragment_message_stream_with_size(
            data,
            FRAGMENT_SIZE,
            [&](const IPC::FragmentView& view) -> bool {
                if (!message_id) {
                    message_id = view.header().message_id;
                }
                return sender.send_fragment(view);
            }
        );

        if (ok && message_id) {
            ok = sender.wait_for_ack(*message_id);
        } else {
            ok = false;
        }

        if (ok) {
            ++success_count;
        } else {
            ++failed_count;
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "[FAILED] client=" << client_id
                      << " message=" << i << "\n";
        }
    }
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> clients;
    clients.reserve(CLIENT_COUNT);

    for (int i = 0; i < CLIENT_COUNT; ++i) {
        clients.emplace_back(client_worker, i + 1);
    }

    for (auto& t : clients) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();

    int total = CLIENT_COUNT * MESSAGES_PER_CLIENT;
    double ms = bench::ms_since(start, end);

    std::cout << "IPC multiclient test\n";
    std::cout << "Clients: " << CLIENT_COUNT << "\n";
    std::cout << "Messages/client: " << MESSAGES_PER_CLIENT << "\n";
    std::cout << "Expected: " << total << "\n";
    std::cout << "Success: " << success_count.load() << "\n";
    std::cout << "Failed: " << failed_count.load() << "\n";
    std::cout << "Time(ms): " << ms << "\n";
    std::cout << "Messages/sec: " << (total * 1000.0 / ms) << "\n";

#ifdef _WIN32
    system("pause");
#endif
    return failed_count.load() == 0 ? 0 : 1;
}