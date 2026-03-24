#include "../include/ipc_receiver.hpp"
#include <iostream>
#include <string>

int main() {    
    IPCReceiver server;
    server.set_message_handler([](const IPCReceiver::Message& msg) {
        std::string text(msg.data.begin(), msg.data.end());

        std::cout << "Received message:\n";
        std::cout << "ID: " << msg.message_id << std::endl;
        std::cout << "Size: " << msg.data.size() << std::endl;
        std::cout << "Content: " << text << std::endl;
    });

    server.run();
    return 0;
}
