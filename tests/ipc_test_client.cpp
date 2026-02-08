#include "../include/ipc_sender.hpp"
#include "../include/ipc_protocol.hpp"
#include "test_config.hpp"

#include <thread>
#include <random>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>

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

int main() {
    using namespace TestConfig;

    std::filesystem::create_directories(LOG_DIR);

    std::ofstream log(CLIENT_LOG_FILE, std::ios::out | std::ios::trunc);
    if (!log) {
        std::cerr << "Failed to open log file\n";
        return 1;
    }

    IPC::MessageFragmenter fragmenter;
    IPCSender sender(0);

    int success = 0;
    int failed  = 0;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < MESSAGES_PER_CLIENT; ++i) {
        auto msg = random_message(MIN_MESSAGE_SIZE, MAX_MESSAGE_SIZE);
        auto fragments = fragmenter.fragment_message(msg);

        uint64_t msg_id = fragments.front().header.message_id;
        bool send_ok = true;

        for (auto& f : fragments) {
            if (!sender.send_fragment(f)) {
                log << "[ERROR] Send failed, msg_id=" << msg_id << "\n";
                send_ok = false;
                break;
            }
        }

        if (!send_ok) {
            failed++;
            continue;
        }

        if (sender.wait_for_ack(msg_id)) {
            success++;
            log << "[OK] msg_id=" << msg_id
                << " size=" << msg.size()
                << " fragments=" << fragments.size() << "\n";
        } else {
            failed++;
            log << "[FAIL] No ACK for msg_id=" << msg_id << "\n";
        }

        log.flush();
    }

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "\n=== TEST RESULT ===\n";
    std::cout << "Sent:    " << MESSAGES_PER_CLIENT << "\n";
    std::cout << "Success: " << success << "\n";
    std::cout << "Failed:  " << failed << "\n";
    std::cout << "Time(ms): " << ms << "\n";

    if (ms > 0) {
        std::cout << "Messages/sec: "
                  << (MESSAGES_PER_CLIENT * 1000.0 / ms) << "\n";
    }
    #ifdef _WIN32 
        system.pause();
    #endif
    return 0;
}
