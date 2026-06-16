#include "../include/ipc_receiver.hpp"

#include <iostream>
#include <string>

int main()
{
    const std::string channel = "test_channel";

    std::cout << "IPC Receiver\n";
    std::cout << "Channel: " << channel << "\n";

    IPCReceiver server(channel);

    server.set_message_handler([](const IPCReceiver::Message& msg) {
        std::string text(msg.data.begin(), msg.data.end());

        std::cout << "\nReceived message:\n";
        std::cout << "ID: " << msg.message_id << "\n";
        std::cout << "Size: " << msg.data.size() << "\n";
        std::cout << "Content: " << text << "\n";
    });

    server.run();

    return 0;
}