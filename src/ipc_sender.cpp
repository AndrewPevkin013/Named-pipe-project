#include "../include/ipc_sender.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <optional>
#include <cstring>

#ifdef _WIN32
    const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#else
    const char* CONNECT_PIPE = "/tmp/ipc_transport/ipc_connect";
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

std::atomic<uint32_t> IPCSender::next_sender_id_{1};

IPCSender::IPCSender(uint32_t client_id) {
    client_id_ = client_id == 0 ? next_sender_id_++ : client_id;
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
    static std::atomic<uint64_t> counter{0};

    uint64_t id = counter++;

    in_fifo  = "/tmp/ipc_transport/ipc_in_"  + std::to_string(getpid()) + "_" + std::to_string(id);
    out_fifo = "/tmp/ipc_transport/ipc_out_" + std::to_string(getpid()) + "_" + std::to_string(id);
    mkdir("/tmp/ipc_transport", 0700);
    mkfifo(in_fifo.c_str(), 0600);
    mkfifo(out_fifo.c_str(), 0600);

    std::string payload = in_fifo + "|" + out_fifo;

    int connect_fd = open(CONNECT_PIPE, O_WRONLY);
    if (connect_fd < 0) {
        perror("[Client] open connect pipe");
        return false;
    }

    uint32_t len = payload.size() + 1;
    std::vector<char> handshake(sizeof(len) + len);
    memcpy(handshake.data(), &len, sizeof(len));
    memcpy(handshake.data() + sizeof(len), payload.c_str(), len);
    write_all(connect_fd, handshake.data(), handshake.size());
    // write_all(connect_fd, &len, sizeof(len));
    // write_all(connect_fd, payload.c_str(), len);
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
    if (write_fd_ == -1)
        return false;
#endif
    std::lock_guard<std::mutex> lock(send_mutex_);
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

    std::optional<uint32_t> message_id;
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


void IPCSender::disconnect() {
#ifdef _WIN32
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
#else
    if (read_fd_ != -1) close(read_fd_);
    if (write_fd_ != -1) close(write_fd_);
    read_fd_ = write_fd_ = -1;
#endif
}

bool IPCSender::send_fragment(const IPC::FragmentView& view) {
#ifdef _WIN32
    if (pipe_ == INVALID_HANDLE_VALUE) return false;
#else
    if (write_fd_ == -1) return false;
#endif

    // IPC::FragmentHeader header = view.header();
    // header.sender_id = client_id_;

    // uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(view.size());
    // DWORD written = 0;

    // if (!WriteFile(pipe_, &packet_size, sizeof(packet_size), &written, nullptr))
    //     return false;

    // if (!WriteFile(pipe_, &header, sizeof(header), &written, nullptr))
    //     return false;

    // if (view.size() > 0) {
    //     if (!WriteFile(pipe_, view.data(), static_cast<DWORD>(view.size()), &written, nullptr))
    //         return false;
    // }

    // return true;
    IPC::FragmentHeader header = view.header();
    header.sender_id = client_id_;

    uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(view.size());

    std::vector<char> buffer;
    buffer.resize(sizeof(packet_size) + sizeof(header) + view.size());

    size_t offset = 0;
    memcpy(buffer.data() + offset, &packet_size, sizeof(packet_size));
    offset += sizeof(packet_size);

    memcpy(buffer.data() + offset, &header, sizeof(header));
    offset += sizeof(header);

    if (view.size() > 0) {
        memcpy(buffer.data() + offset, view.data(), view.size());
    }

#ifdef _WIN32
    return write_all(pipe_, buffer.data(), buffer.size());
#else
    return write_all(write_fd_, buffer.data(), buffer.size());
#endif
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

bool IPCSender::wait_for_ack(uint32_t expected_message_id) {

#ifdef _WIN32
    auto pipe = pipe_;
    if (pipe == INVALID_HANDLE_VALUE)
        return false;
#else
    auto pipe = read_fd_;
    if (pipe == -1)
        return false;
#endif

    uint32_t packet_size = 0;

    if (!read_all(pipe, &packet_size, sizeof(packet_size))) {
        std::cerr << "[Client] ACK read failed (packet_size)\n";
        return false;
    }

    if (packet_size < sizeof(IPC::FragmentHeader) ||
        packet_size > sizeof(IPC::FragmentHeader) + sizeof(IPC::AckPayload)) {
        std::cerr << "[Client] Invalid ACK packet size\n";
        return false;
    }

    IPC::FragmentHeader header{};

    if (!read_all(pipe, &header, sizeof(header))) {
        std::cerr << "[Client] ACK header read failed\n";
        return false;
    }

    if (!(header.flags & IPC::FLAG_ACK)) {
        std::cerr << "[Client] Expected ACK packet\n";
        return false;
    }

    IPC::AckPayload payload{};

    if (!read_all(pipe, &payload, sizeof(payload))) {
        std::cerr << "[Client] ACK payload read failed\n";
        return false;
    }

    if (payload.message_id != expected_message_id) {
        std::cerr << "[Client] ACK mismatch\n";
        return false;
    }

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

    std::lock_guard<std::mutex> lock(send_mutex_);

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

    std::optional<uint32_t> message_id;
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