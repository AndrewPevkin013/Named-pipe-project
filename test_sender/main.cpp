#include "../include/ipc_sender.hpp"
#include <iostream>
#include <string>

int main() {
    std::cout << "IPC Sender\n";
    std::cout << "Type ':q' to quit\n";

    IPCSender sender;

    std::string msg;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, msg))
            break;

        if (msg == ":q")
            break;

        if (sender.send(msg)) {
            std::cout << "[ACK] Message delivered\n";
        } else {
            std::cerr << "[Client] Send failed\n";
        }
    }

    return 0;
}
