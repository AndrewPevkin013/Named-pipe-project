#include "../include/ipc_protocol.hpp"
#include "../include/ipc_send_queue.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>

int main() {
    std::cout << "IPC Test Sender\n";
    std::cout << "Type ':q' to quit\n\n";

    uint32_t client_id = 0;
    IPC::MessageFragmenter fragmenter;
    IPCSendQueue sendQueue(client_id);

    std::string msg;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, msg)) {
            break;
        }

        if (msg == ":q") {
            break;
        }

        std::vector<char> body(msg.begin(), msg.end());

        std::ostringstream meta;
        meta << "type:text\n"
             << "size:" << body.size() << "\n"
             << "timestamp:"
             << std::chrono::system_clock::now().time_since_epoch().count()
             << "\n";

        std::string metadata = meta.str();

        std::vector<char> full_message;
        full_message.insert(full_message.end(), metadata.begin(), metadata.end());
        full_message.insert(full_message.end(), body.begin(), body.end());

        auto fragments = fragmenter.fragment_message(full_message);

        std::cout << "Queued " << fragments.size() << " fragment(s)\n";

        for (auto& fragment : fragments) {
            sendQueue.push(std::move(fragment));
        }
    }

    sendQueue.stop();
    return 0;
}
