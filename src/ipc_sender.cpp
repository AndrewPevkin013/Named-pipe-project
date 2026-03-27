#include "../include/ipc_sender.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <optional>

#ifdef _WIN32
    constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#else
    constexpr const char* CONNECT_PIPE = "/tmp/ipc_connect";
#endif

namespace {
    bool write_all(
#ifdef _WIN32
        HANDLE pipe,
#else
        int pipe,
#endif
        const void* data,
        size_t size
    ) {
        size_t total = 0;
        const char* buf = static_cast<const char*>(data);

        while (total < size) {
#ifdef _WIN32
            DWORD written = 0;
            if (!WriteFile(pipe, buf + total, size - total, &written, nullptr))
                return false;
            total += written;
#else
            ssize_t written = write(pipe, buf + total, size - total);
            if (written <= 0)
                return false;
            total += written;
#endif
        }
        return true;
    }

    bool read_all(
#ifdef _WIN32
        HANDLE pipe,
#else
        int pipe,
#endif
        void* data,
        size_t size
    ) {
        size_t total = 0;
        char* buf = static_cast<char*>(data);

        while (total < size) {
#ifdef _WIN32
            DWORD read = 0;
            if (!ReadFile(pipe, buf + total, size - total, &read, nullptr))
                return false;
            total += read;
#else
            ssize_t r = read(pipe, buf + total, size - total);
            if (r <= 0)
                return false;
            total += r;
#endif
        }
        return true;
    }
}

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
#else
    in_fifo  = "/tmp/ipc_in_"  + std::to_string(getpid());
    out_fifo = "/tmp/ipc_out_" + std::to_string(getpid());

    mkfifo(in_fifo.c_str(), 0666);
    mkfifo(out_fifo.c_str(), 0666);

    std::string payload = in_fifo + "|" + out_fifo;

    int connect_fd = open(CONNECT_PIPE, O_WRONLY);
    if (connect_fd < 0) {
        perror("[Client] open connect pipe");
        return false;
    }

    uint32_t len = payload.size() + 1;

    write_all(connect_fd, &len, sizeof(len));
    write_all(connect_fd, payload.c_str(), len);
    close(connect_fd);

    write_fd_ = open(out_fifo.c_str(), O_WRONLY);
    if (write_fd_ < 0) {
        perror("open out_fifo");
        return false;
    }

    read_fd_ = open(in_fifo.c_str(), O_RDONLY);
    if (read_fd_ < 0) {
        perror("open in_fifo");
        return false;
    }

    return true;
#endif
}

bool IPCSender::send(const std::string& message) {
#ifdef _WIN32
    if (IPCSender::pipe_ == INVALID_HANDLE_VALUE)
        return false;
#else
    if (IPCSender::write_fd_ == -1)
        return false;
#endif
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

    std::optional<uint64_t> message_id;
        bool success = fragmenter_.fragment_message_stream(full_message,
        [this, &message_id](const IPC::FragmentView& view) -> bool {
            if (!message_id) {
                message_id = view.header().message_id;
            }
            return send_fragment(view);
        });
    
    if (!success || !message_id) {
        return false;
    }
    return wait_for_ack(*message_id);
}



bool IPCSender::send_fragment(const IPC::FragmentView& view) {
#ifdef _WIN32
    if (pipe_ == INVALID_HANDLE_VALUE) return false;
    IPC::FragmentHeader header = view.header();
    header.sender_id = client_id_;

    uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(view.size());

    if (!write_all(pipe_, &packet_size, sizeof(packet_size)))
        return false;

    if (!write_all(pipe_, &header, sizeof(header)))
        return false;

    if (view.size() > 0) {
        if (!write_all(pipe_, view.data(), view.size()))
            return false;
    }
#else
    if (write_fd_ == -1) return false;
    IPC::FragmentHeader header = view.header();
    header.sender_id = client_id_;

    uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(view.size());

    if (!write_all(write_fd_, &packet_size, sizeof(packet_size)))
        return false;

    if (!write_all(write_fd_, &header, sizeof(header)))
        return false;

    if (view.size() > 0) {
        if (!write_all(write_fd_, view.data(), view.size()))
            return false;
    }
#endif

    return true;
}

IPCSender::~IPCSender() {
#ifdef _WIN32
    if (pipe_ != INVALID_HANDLE_VALUE)
        CloseHandle(pipe_);
#else
    if (read_fd_ != -1)
        close(read_fd_);
    if (write_fd_ != -1)
        close(write_fd_);
#endif
}

// bool IPCSender::send_fragment(IPC::Fragment& fragment) {
// #ifdef _WIN32
//     if (pipe_ == INVALID_HANDLE_VALUE)
//         return false;

//     fragment.header.sender_id = client_id_;

//     uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(fragment.data.size());
//     DWORD written = 0;

//     if (!WriteFile(pipe_, &packet_size, sizeof(packet_size), &written, nullptr))
//         return false;

//     if (!WriteFile(pipe_, &fragment.header, sizeof(fragment.header), &written, nullptr))
//         return false;

//     if (!fragment.data.empty()) {
//         if (!WriteFile(pipe_, fragment.data.data(), static_cast<DWORD>(fragment.data.size()), &written, nullptr))
//             return false;
//     }

//     return true;
// #endif
//     return false;
// }

bool IPCSender::wait_for_ack(uint64_t expected_message_id) {
#ifdef _WIN32
    if (pipe_ == INVALID_HANDLE_VALUE) return false;
    uint32_t packet_size = 0;
    if (!read_all(pipe_, &packet_size, sizeof(packet_size)))
        return false;

    IPC::FragmentHeader header{};
    if (!read_all(pipe_, &header, sizeof(header)))
        return false;

    if (!(header.flags & IPC::FLAG_ACK)) {
        std::cerr << "[Client] Expected ACK\n";
        return false;
    }

    IPC::AckPayload payload{};
    if (!read_all(pipe_, &payload, sizeof(payload)))
        return false;

    if (payload.message_id != expected_message_id) {
        std::cerr << "[Client] ACK mismatch\n";
        return false;
    }
#else
    if (read_fd_ == -1) return false;
    uint32_t packet_size = 0;
    if (!read_all(read_fd_, &packet_size, sizeof(packet_size)))
        return false;

    IPC::FragmentHeader header{};
    if (!read_all(read_fd_, &header, sizeof(header)))
        return false;

    if (!(header.flags & IPC::FLAG_ACK)) {
        std::cerr << "[Client] Expected ACK\n";
        return false;
    }

    IPC::AckPayload payload{};
    if (!read_all(read_fd_, &payload, sizeof(payload)))
        return false;

    if (payload.message_id != expected_message_id) {
        std::cerr << "[Client] ACK mismatch\n";
        return false;
    }
#endif

    return true;
}


bool IPCSender::send_with_fragment_size(const std::string& message, size_t fragment_size) {
#ifdef _WIN32
    if (IPCSender::pipe_ == INVALID_HANDLE_VALUE)
        return false;
#else
    if (write_fd_ == -1)
        return false;
#endif

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

    std::optional<uint64_t> message_id;
    bool success = fragmenter_.fragment_message_stream_with_size(full_message, fragment_size,
        [this, &message_id](const IPC::FragmentView& view) -> bool {
            if (!message_id) {
                message_id = view.header().message_id;
            }
            return send_fragment(view);
        });
    
    if (!success || !message_id) {
        return false;
    }
    return wait_for_ack(*message_id);
}