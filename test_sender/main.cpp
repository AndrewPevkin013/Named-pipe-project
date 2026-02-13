#include "../include/ipc_protocol.hpp"
#include "../include/ipc_sender.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <ctime>

int main() {
    std::cout << "IPC Sender\n";
    std::cout << "Type ':q' to quit\n";

    uint32_t client_id = 0;
    IPC::MessageFragmenter fragmenter;
    IPCSender sender(client_id);

    std::string msg;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, msg)) break;
        if (msg == ":q") break;

        std::vector<char> body(msg.begin(), msg.end());
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now = *std::localtime(&time_t_now);

        std::ostringstream meta;
        meta << "type:text\n" << "size:" << body.size() << "\n" 
            << "timestamp:" << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S") << "\n";

        std::string metadata = meta.str();
        std::vector<char> full_message;
        full_message.insert(full_message.end(), metadata.begin(), metadata.end());
        full_message.insert(full_message.end(), body.begin(), body.end());

        auto fragments = fragmenter.fragment_message(full_message);
        std::cout << "Queued " << fragments.size() << " fragment(s)\n";
        uint64_t message_id = fragments.front().header.message_id;

        for (auto& fragment : fragments) {
            if (!sender.send_fragment(fragment)) {
                std::cerr << "[Client] Failed to send fragment\n";
                break;
            }
        }

        if (sender.wait_for_ack(message_id)) {
            std::cout << "[ACK] Message delivered: " << message_id << "\n";
        } else {
            std::cerr << "[Client] No ACK received for message " << message_id << "\n";
        }
    }

    return 0;
}