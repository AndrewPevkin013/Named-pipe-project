#include "../include/ipc_protocol.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
    constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    constexpr const char* FIFO_PATH = "/tmp/ipc_test_fifo";
#endif

class IPCReceiver {
public:
    IPCReceiver() : running_(true) {}
    static std::atomic<uint32_t> next_client_id;

    void run(IPC::MessageAssembler& assembler) {
    #ifdef _WIN32
        std::cout << "[Receiver] Waiting for connections..." << std::endl;
            while (running_) {
                HANDLE pipe = CreateNamedPipeA(
                    PIPE_NAME,
                    PIPE_ACCESS_DUPLEX,
                    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                    PIPE_UNLIMITED_INSTANCES,
                    64 * 1024,
                    64 * 1024,
                    0,
                    NULL
                );

                if (pipe == INVALID_HANDLE_VALUE) {
                    std::cerr << "CreateNamedPipe failed\n";
                    continue;
                }

                BOOL connected = ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;

                if (!connected) {
                    CloseHandle(pipe);
                    continue;
                }

                std::thread(&IPCReceiver::client_loop, this, pipe, std::ref(assembler)).detach();
            }
    #else
            // Linux — FIFO НЕ поддерживает multi-client нормально
            // Тут пока оставлил single-reader модель
            run_fifo(assembler);
    #endif
    }

    void stop() {
        running_ = false;
    }

private:
    #ifdef _WIN32
        void client_loop(HANDLE pipe, IPC::MessageAssembler& assembler) {
            uint32_t client_id = next_client_id.fetch_add(1);
            std::cout << "[Receiver] Client connected, assigned id = " << client_id << std::endl;
              while (running_) {
                IPC::Fragment fragment;
                if (!receive_fragment(pipe, fragment)) {
                    break;
                }

                if (fragment.header.flags & IPC::FLAG_ACK) {
                    std::cout << "[ACK] Message delivered: " << fragment.header.message_id << "\n";
                    continue;
                }
                
                if (assembler.add_fragment(fragment)) {
                    auto msg = assembler.get_assembled_message(fragment.header.message_id);
                    if (msg.complete) {
                        std::string text(msg.data.begin(), msg.data.end());
                        std::cout << "\n[Receiver] Message from client " << client_id  << ":\n" << text << "\n";

                        IPC::Fragment ack;
                        ack.header.sender_id = client_id;
                        ack.header.message_id = fragment.header.message_id;
                        ack.header.total_fragments = 1;
                        ack.header.fragment_index = 0;
                        ack.header.fragment_size = sizeof(IPC::AckPayload);
                        ack.header.flags = IPC::FLAG_ACK | IPC::FLAG_LAST;

                        IPC::AckPayload payload{ fragment.header.message_id };
                        ack.data.resize(sizeof(payload));
                        std::memcpy(ack.data.data(), &payload, sizeof(payload));

                        if (!send_fragment(pipe, ack)) {
                            std::cerr << "[Receiver] Failed to send ACK\n";
                        }
                    }
                }

            }
            CloseHandle(pipe);
        }

        bool receive_fragment(HANDLE pipe, IPC::Fragment& fragment) {
            DWORD read = 0;
            uint32_t packet_size = 0;

            if (!ReadFile(pipe, &packet_size, sizeof(packet_size), &read, nullptr) || read != sizeof(packet_size)) {
                return false;
            }

            if (!ReadFile(pipe, &fragment.header, sizeof(IPC::FragmentHeader), &read, nullptr) || read != sizeof(IPC::FragmentHeader)) {
                return false;
            }

            fragment.data.resize(fragment.header.fragment_size);
            if (!fragment.data.empty()) {
                if (!ReadFile(pipe, fragment.data.data(), fragment.header.fragment_size, &read, nullptr) || read != fragment.header.fragment_size) {
                    return false;
                }
            }

            return true;
        }

        bool send_fragment(HANDLE pipe, const IPC::Fragment& fragment) {
            DWORD written = 0;

            uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(fragment.data.size());

            if (!WriteFile(pipe, &packet_size, sizeof(packet_size), &written, nullptr))
                return false;

            if (!WriteFile(pipe, &fragment.header, sizeof(fragment.header), &written, nullptr))
                return false;

            if (!fragment.data.empty()) {
                if (!WriteFile(pipe, fragment.data.data(), static_cast<DWORD>(fragment.data.size()), &written, nullptr))
                    return false;
            }

            return true;
        }


    #endif

    std::atomic<bool> running_;
};

std::atomic<uint32_t> IPCReceiver::next_client_id{1};


int main() {
    IPC::MessageAssembler assembler;
    IPCReceiver receiver;

    std::thread server([&] {
        receiver.run(assembler);
    });

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    receiver.stop();
    server.join();

    return 0;
}
