// #include "../include/ipc_protocol.hpp"
// #include "server_send_queue.hpp"
// #include <iostream>
// #include <thread>
// #include <atomic>
// #include <chrono>
// #include <sstream>

// #ifdef _WIN32
// #include <windows.h>
// constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
// #endif


// class IPCReceiver {
// public:
//     IPCReceiver() : running_(true) {}
//     static std::atomic<uint32_t> next_client_id;

//     void run(IPC::MessageAssembler& assembler) {
// #ifdef _WIN32
//         std::cout << "[Receiver] Waiting for connections..." << std::endl;

//         while (running_) {
//             HANDLE pipe = CreateNamedPipeA(
//                 PIPE_NAME,
//                 PIPE_ACCESS_DUPLEX,
//                 PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
//                 PIPE_UNLIMITED_INSTANCES,
//                 64 * 1024,
//                 64 * 1024,
//                 0,
//                 NULL
//             );

//             if (pipe == INVALID_HANDLE_VALUE) {
//                 std::cerr << "CreateNamedPipe failed\n";
//                 continue;
//             }

//             BOOL connected = ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;

//             if (!connected) {
//                 CloseHandle(pipe);
//                 continue;
//             }

//             std::thread(&IPCReceiver::client_loop, this, pipe, std::ref(assembler)).detach();
//         }
// #endif
//     }

//     void stop() {
//         running_ = false;
//     }

// private:
// #ifdef _WIN32
//     void client_loop(HANDLE pipe, IPC::MessageAssembler& assembler) {
//         uint32_t client_id = next_client_id.fetch_add(1);
//         std::cout << "[Receiver] Client connected, assigned id = " << client_id << std::endl;

//         while (running_) {
//             IPC::Fragment fragment;
//             if (!receive_fragment(pipe, fragment))
//                 break;

//             if (fragment.header.flags & IPC::FLAG_ACK) {
//                 std::cout << "[ACK] Message delivered: " << fragment.header.message_id << "\n";
//                 continue;
//             }

//             if (assembler.add_fragment(fragment)) {
//                 auto msg = assembler.get_assembled_message(fragment.header.message_id);
//                 if (msg.complete) {
//                     std::string text(msg.data.begin(), msg.data.end());
                    
//                     // std::cout << "\n[Receiver] Message from client " << client_id << ": "  << text << "\n";
//                     std::cout << "\n[Receiver] Message from client " << client_id << "\n";

//                     IPC::Fragment ack;
//                     ack.header.sender_id = client_id;
//                     ack.header.message_id = fragment.header.message_id;
//                     ack.header.total_fragments = 1;
//                     ack.header.fragment_index = 0;
//                     ack.header.fragment_size = sizeof(IPC::AckPayload);
//                     ack.header.flags = IPC::FLAG_ACK | IPC::FLAG_LAST;

//                     IPC::AckPayload payload{ fragment.header.message_id };
//                     ack.data.resize(sizeof(payload));
//                     std::memcpy(ack.data.data(), &payload, sizeof(payload));

//                     send_ack_immediate(pipe, ack);
//                 }
//             }
//         }

//         CloseHandle(pipe);
//     }

//     bool receive_fragment(HANDLE pipe, IPC::Fragment& fragment) {
//         DWORD read = 0;
//         uint32_t packet_size = 0;

//         if (!ReadFile(pipe, &packet_size, sizeof(packet_size), &read, nullptr)) {
//             return false;
//         }

//         if (!ReadFile(pipe, &fragment.header, sizeof(fragment.header), &read, nullptr)) {
//             return false;
//         }

//         fragment.data.resize(fragment.header.fragment_size);
//         if (!fragment.data.empty()) {
//             if (!ReadFile(pipe, fragment.data.data(), fragment.header.fragment_size, &read, nullptr)) {
//                 return false;
//             }
//         }

//         return true;
//     }

//     bool send_ack_immediate(HANDLE pipe, const IPC::Fragment& ack) {
//         DWORD written = 0;
//         uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(ack.data.size());

//         if (!WriteFile(pipe, &packet_size, sizeof(packet_size), &written, nullptr))
//             return false;
//         if (!WriteFile(pipe, &ack.header, sizeof(ack.header), &written, nullptr))
//             return false;
//         if (!ack.data.empty()) {
//             if (!WriteFile(pipe, ack.data.data(), static_cast<DWORD>(ack.data.size()), &written, nullptr))
//                 return false;
//         }

//         return true;
//     }
// #endif

//     std::atomic<bool> running_;
// };

// std::atomic<uint32_t> IPCReceiver::next_client_id{1};

// int main() {
//     IPC::MessageAssembler assembler;
//     IPCReceiver receiver;

//     std::thread server([&] {
//         receiver.run(assembler);
//     });

//     while (true) {
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }

//     receiver.stop();
//     server.join();

//     return 0;
// }

#include "../include/ipc_protocol.hpp"
#include "server_send_queue.hpp"

#include <windows.h>
#include <atomic>
#include <thread>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <iostream>
#include <string>


constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";

constexpr const char* SERVER_LOG_DIR  = "logs";
constexpr const char* SERVER_LOG_FILE = "logs/server.log";

std::mutex log_mutex;
std::ofstream server_log;

void log_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << line << std::endl;
    if (server_log.is_open()) {
        server_log << line << std::endl;
        server_log.flush();
    }
}

class IPCReceiver {
public:
    static std::atomic<uint32_t> next_client_id;
    explicit IPCReceiver(ServerSendQueue& send_queue) : send_queue_(send_queue), running_(true) {}

    void run() {
        log_line("[Receiver] Waiting for connections...");

        while (running_) {
            HANDLE pipe = CreateNamedPipeA(
                PIPE_NAME,
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                64 * 1024,
                64 * 1024,
                0,
                nullptr
            );

            if (pipe == INVALID_HANDLE_VALUE)
                continue;

            if (!ConnectNamedPipe(pipe, nullptr) &&
                GetLastError() != ERROR_PIPE_CONNECTED) {
                CloseHandle(pipe);
                continue;
            }

            std::thread(
                &IPCReceiver::client_loop,
                this,
                pipe
            ).detach();
        }
    }

private:
    ServerSendQueue& send_queue_;
    std::atomic<bool> running_;
    void client_loop(HANDLE pipe) {
        uint32_t client_id = next_client_id++;
        IPC::MessageAssembler assembler;
        log_line("[CONNECT] client_id=" + std::to_string(client_id));

        while (running_) {
            IPC::Fragment fragment;
            if (!receive(pipe, fragment))
                break;

            if (assembler.add_fragment(fragment)) {
                auto msg = assembler.get_assembled_message(fragment.header.message_id);
                if (msg.complete) {
                    log_line(
                        "[RECEIVED] client=" + std::to_string(client_id) +
                        " msg_id=" + std::to_string(fragment.header.message_id) +
                        " size=" + std::to_string(msg.data.size())
                    );

                    send_ack(pipe, fragment.header.message_id, client_id);
                }
            }
        }

        log_line("[DISCONNECT] client_id=" + std::to_string(client_id));
        CloseHandle(pipe);
    }

    bool receive(HANDLE pipe, IPC::Fragment& fragment) {
        DWORD read = 0;
        uint32_t packet_size = 0;

        if (!ReadFile(pipe, &packet_size, sizeof(packet_size), &read, nullptr))
            return false;

        if (!ReadFile(pipe, &fragment.header, sizeof(fragment.header), &read, nullptr))
            return false;

        fragment.data.resize(fragment.header.fragment_size);
        if (!fragment.data.empty()) {
            if (!ReadFile(
                    pipe,
                    fragment.data.data(),
                    fragment.header.fragment_size,
                    &read,
                    nullptr))
                return false;
        }

        return true;
    }

    void send_ack(HANDLE pipe, uint64_t msg_id, uint32_t client_id) {
        IPC::Fragment ack{};
        ack.header.message_id = msg_id;
        ack.header.sender_id  = client_id;
        ack.header.flags      = IPC::FLAG_ACK | IPC::FLAG_LAST;
        ack.header.fragment_size = 0;

        send_queue_.push(pipe, std::move(ack));
    }
};

std::atomic<uint32_t> IPCReceiver::next_client_id{1};

int main() {
    ServerSendQueue send_queue;
    std::filesystem::create_directories(SERVER_LOG_DIR);
    server_log.open(SERVER_LOG_FILE, std::ios::out | std::ios::trunc);

    if (!server_log) {
        std::cerr << "[Server] Failed to open log file\n";
    }

    IPCReceiver receiver(send_queue);
    receiver.run();
    return 0;
}
