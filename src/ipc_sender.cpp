#include "../include/ipc_sender.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
    constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#endif

IPCSender::IPCSender() {
    connect();
}

bool IPCSender::connect() {
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
            return true;

        if (GetLastError() != ERROR_PIPE_BUSY) {
            std::cerr << "[Client] Failed to connect to pipe\n";
            return false;
        }

        if (!WaitNamedPipeA(PIPE_NAME, 5000)) {
            std::cerr << "[Client] Pipe wait timeout\n";
            return false;
        }
    }
#endif
    return false;
}

bool IPCSender::send(const std::string& message) {
    if (IPCSender::pipe_ == INVALID_HANDLE_VALUE)
        return false;

    std::vector<char> body(message.begin(), message.end());

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);

    std::ostringstream meta;
    meta << "type:text\n"
         << "size:" << body.size() << "\n"
         << "timestamp:" << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S") << "\n";

    std::string metadata = meta.str();

    std::vector<char> full_message;
    full_message.insert(full_message.end(), metadata.begin(), metadata.end());
    full_message.insert(full_message.end(), body.begin(), body.end());

    auto fragments = fragmenter_.fragment_message(full_message);
    if (fragments.empty())
        return false;

    uint64_t message_id = fragments.front().header.message_id;

    for (auto& fragment : fragments) {
        if (!send_fragment(fragment))
            return false;
    }

    return wait_for_ack(message_id);
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