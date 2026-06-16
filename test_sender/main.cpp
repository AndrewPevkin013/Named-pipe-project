#include "../include/ipc_sender.hpp"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    const std::string channel = "test_channel";

    std::cout << "IPC Sender\n";
    std::cout << "Channel: " << channel << "\n";
    std::cout << "Type ':q' to quit\n";

    IPCSender sender(channel);

    std::string msg;

    while (true) {
        std::cout << "> ";

        if (!std::getline(std::cin, msg))
            break;

        if (msg == ":q")
            break;

        std::vector<uint8_t> data(msg.begin(), msg.end());

        if (sender.send(data)) {
            std::cout << "[ACK] Message delivered\n";
        } else {
            std::cerr << "[Client] Send failed\n";
        }
    }

    return 0;
}