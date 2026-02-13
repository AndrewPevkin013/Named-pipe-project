#include "../include/ipc_sender.hpp"
#include <iostream>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#endif

IPCSender::IPCSender(uint32_t client_id) : client_id_(client_id) {
#ifdef _WIN32
    while (true) {
        pipe_ = CreateFileA(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (pipe_ != INVALID_HANDLE_VALUE)
            break;

        if (GetLastError() != ERROR_PIPE_BUSY) {
            std::cerr << "[Client] Failed to connect to pipe\n";
            return;
        }

        if (!WaitNamedPipeA(PIPE_NAME, 5000)) {
            std::cerr << "[Client] Pipe wait timeout\n";
            return;
        }
    }
#endif
}

IPCSender::~IPCSender() {
#ifdef _WIN32
    if (pipe_ != INVALID_HANDLE_VALUE)
        CloseHandle(pipe_);
#endif
}

bool IPCSender::send_fragment(IPC::Fragment& fragment) {
#ifdef _WIN32
    if (pipe_ == INVALID_HANDLE_VALUE)
        return false;

    fragment.header.sender_id = client_id_;

    uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(fragment.data.size());
    DWORD written = 0;

    if (!WriteFile(pipe_, &packet_size, sizeof(packet_size), &written, nullptr))
        return false;

    if (!WriteFile(pipe_, &fragment.header, sizeof(fragment.header), &written, nullptr))
        return false;

    if (!fragment.data.empty()) {
        if (!WriteFile(pipe_, fragment.data.data(), static_cast<DWORD>(fragment.data.size()), &written, nullptr))
            return false;
    }

    return true;
#endif
    return false;
}

bool IPCSender::wait_for_ack(uint64_t expected_message_id) {
#ifdef _WIN32
    if (pipe_ == INVALID_HANDLE_VALUE)
        return false;

    DWORD read = 0;
    uint32_t packet_size = 0;

    if (!ReadFile(pipe_, &packet_size, sizeof(packet_size), &read, nullptr))
        return false;

    IPC::FragmentHeader header{};
    if (!ReadFile(pipe_, &header, sizeof(header), &read, nullptr))
        return false;

    if (!(header.flags & IPC::FLAG_ACK)) {
        std::cerr << "[Client] Expected ACK\n";
        return false;
    }

    if (header.fragment_size != sizeof(IPC::AckPayload)) {
        std::cerr << "[Client] Invalid ACK payload size\n";
        return false;
    }

    IPC::AckPayload payload{};
    if (!ReadFile(pipe_, &payload, sizeof(payload), &read, nullptr))
        return false;

    if (payload.message_id != expected_message_id) {
        std::cerr << "[Client] ACK mismatch, expected " << expected_message_id << ", got " << payload.message_id << "\n";
        return false;
    }

    return true;
#endif
    return false;
}