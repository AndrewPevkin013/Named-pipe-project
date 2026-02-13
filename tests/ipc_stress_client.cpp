#include "../include/ipc_sender.hpp"
#include "../include/ipc_protocol.hpp"
#include "test_config.hpp"

#include <thread>
#include <random>
#include <vector>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iostream>

using namespace TestConfig;

std::mutex log_mutex;
std::atomic<int> success{0};
std::atomic<int> failed{0};

std::vector<char> random_message(size_t min, size_t max) {
    thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<size_t> size_dist(min, max);
    std::uniform_int_distribution<int> char_dist(32, 126);
    size_t size = size_dist(rng);
    std::vector<char> msg(size);
    for (auto& c : msg)
        c = static_cast<char>(char_dist(rng));

    return msg;
}

void client_worker(int client_id, std::ofstream& log) {
    IPC::MessageFragmenter fragmenter;
    IPCSender sender(client_id);

    for (int i = 0; i < MESSAGES_PER_CLIENT; ++i) {
        auto msg = random_message(MIN_MESSAGE_SIZE, MAX_MESSAGE_SIZE);
        auto fragments = fragmenter.fragment_message(msg);
        uint64_t msg_id = fragments.front().header.message_id;

        bool send_ok = true;
        for (auto& f : fragments) {
            if (!sender.send_fragment(f)) {
                send_ok = false;
                break;
            }
        }

        if (!send_ok) {
            failed++;
            std::lock_guard<std::mutex> lock(log_mutex);
            log << "[CLIENT " << client_id << "] send Failed msg_id=" << msg_id << "\n";
            continue;
        }

        if (sender.wait_for_ack(msg_id)) {
            success++;
            std::lock_guard<std::mutex> lock(log_mutex);
            log << "[CLIENT " << client_id << "] OK msg_id=" << msg_id
                << " size=" << msg.size()
                << " fragments=" << fragments.size() << "\n";
        } else {
            failed++;
            std::lock_guard<std::mutex> lock(log_mutex);
            log << "[CLIENT " << client_id << "] No ACK msg_id=" << msg_id << "\n";
        }
    }
}

int main() {
    std::filesystem::create_directories(LOG_DIR);

    std::ofstream log(CLIENT_LOG_FILE, std::ios::out | std::ios::trunc);
    if (!log) {
        std::cerr << "Failed to open client log\n";
        return 1;
    }

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> clients;
    for (int i = 0; i < CLIENT_COUNT; ++i)
        clients.emplace_back(client_worker, i + 1, std::ref(log));

    for (auto& t : clients)
        t.join();

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Test results:\n";
    std::cout << "Clients: " << CLIENT_COUNT << "\n";
    std::cout << "Messages/client: " << MESSAGES_PER_CLIENT << "\n";
    std::cout << "Success: " << success << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Time(ms): " << ms << "\n";
    std::cout << "Msg/sec: " << ((CLIENT_COUNT * MESSAGES_PER_CLIENT) * 1000.0 / ms) << "\n";

    #ifdef _WIN32
        system("pause");
    #endif
    return 0;
}
