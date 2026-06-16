#include "../include/ipc_receiver.hpp"
#include <thread>
std::atomic<uint16_t> IPCReceiver::next_client_id{1};

#ifdef _WIN32
#include <sddl.h>
#endif

#ifdef _WIN32
namespace {

struct PipeSecurity {
    SECURITY_ATTRIBUTES sa{};
    PSECURITY_DESCRIPTOR sd = nullptr;

    PipeSecurity() {
        const char* sddl =
            "D:"
            "(A;;GA;;;SY)"
            "(A;;GA;;;BA)"
            "(A;;GA;;;OW)";

        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
                sddl,
                SDDL_REVISION_1,
                &sd,
                nullptr)) {
            sd = nullptr;
        }

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = sd;
        sa.bInheritHandle = FALSE;
    }

    ~PipeSecurity() {
        if (sd) {
            LocalFree(sd);
            sd = nullptr;
        }
    }

    SECURITY_ATTRIBUTES* attributes() {
        return sd ? &sa : nullptr;
    }
};

}
#endif

namespace {

bool read_all(
#ifdef _WIN32
    HANDLE pipe,
#else
    int pipe,
#endif
    void* data,
    size_t size)
{
    size_t total = 0;
    char* buf = static_cast<char*>(data);

    while (total < size) {
#ifdef _WIN32
        DWORD read = 0;

        if (!ReadFile(
                pipe,
                buf + total,
                static_cast<DWORD>(size - total),
                &read,
                nullptr))
            return false;

        total += read;
#else
        ssize_t r = ::read(pipe, buf + total, size - total);

        if (r <= 0)
            return false;

        total += r;
#endif
    }

    return true;
}

}

IPCReceiver::IPCReceiver(const std::string& channel_name) : channel_name_(channel_name), running_(true)
{
    std::filesystem::create_directories(SERVER_LOG_DIR);
    server_log.open(SERVER_LOG_FILE, std::ios::out | std::ios::trunc);
}

void IPCReceiver::run() {
    log_line("[Receiver] Waiting for connections....");

#ifdef _WIN32
    PipeSecurity pipe_security;
    while (running_) {
        std::string pipe_name = make_pipe_name(channel_name_);
        pipe = CreateNamedPipeA(
            pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            64 * 1024,
            64 * 1024,
            0,
            pipe_security.attributes()
        );

        if (pipe == INVALID_HANDLE_VALUE)
            continue;

        if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe);
            continue;
        }

        std::thread(&IPCReceiver::client_loop, this, pipe).detach();
    }

#else
    mkdir("/tmp/ipc_transport", 0700);
    std::string connect_pipe = get_connect_pipe_name();
    if (mkfifo(connect_pipe.c_str(), 0600) < 0 && errno != EEXIST) {
        perror("mkfifo connect");
        return;
    }

    while (running_) {
        int connect_fd = open(connect_pipe.c_str(), O_RDONLY);
        if (connect_fd < 0) {
            if (!running_)
                break;

            perror("open connect");
            continue;
        }

        log_line("[Receiver] Waiting for clients...");
        while (running_) {
            uint32_t len = 0;
            if (!read_all(connect_fd, &len, sizeof(len))) {
                close(connect_fd);
                break;
            }

            if (len == 0 || len > 256) {
                log_line("[Receiver] Invalid FIFO name length");
                continue;
            }

            std::vector<char> buf(len);

            if (!read_all(connect_fd, buf.data(), len))
                continue;

            std::string payload(buf.data());

            auto pos = payload.find('|');
            if (pos == std::string::npos) {
                log_line("[Receiver] Invalid handshake format");
                continue;
            }
            std::string in_fifo  = payload.substr(0, pos);
            std::string out_fifo = payload.substr(pos + 1);

            int read_fd = open(out_fifo.c_str(), O_RDONLY);
            if (read_fd < 0) {
                perror("open in_fifo");
                continue;
            }
            
            int write_fd = open(in_fifo.c_str(), O_WRONLY);
            if (write_fd < 0) {
                perror("open out_fifo");
                close(read_fd);
                continue;
            }

            std::thread(&IPCReceiver::client_loop_linux, this, read_fd, write_fd).detach();
        }
    }
#endif
}   


void IPCReceiver::stop()
{
    bool expected = true;

    if (!running_.compare_exchange_strong(expected, false))
        return;

#ifdef _WIN32
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
#else
    const std::string connect_pipe = get_connect_pipe_name();

    int fd = open(connect_pipe.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, "stop", 4);
        close(fd);
    }

    unlink(connect_pipe.c_str());
#endif
}

void IPCReceiver::subscribe(const std::string& channel_name, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[channel_name] = std::move(handler);
}

void IPCReceiver::unsubscribe(const std::string& channel_name) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.erase(channel_name);
}

void IPCReceiver::set_message_handler(MessageHandler handler) {
    subscribe("default", std::move(handler));
}


IPCReceiver::~IPCReceiver()
{
    stop();

#ifdef _WIN32
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
#endif
}

void IPCReceiver::log_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(log_mutex);
    // std::cout << line << std::endl;
    if (server_log.is_open()) {
        server_log << line << std::endl;
        server_log.flush();
    }
}


#ifdef _WIN32
void IPCReceiver::client_loop(PipeHandle pipe) {
    uint16_t client_id = next_client_id++;
    IPC::MessageAssembler assembler;
    log_line("\n[CONNECT] client_id=" + std::to_string(client_id));

    while (running_) {
        IPC::Fragment fragment;
        if (!receive(pipe, fragment))
            break;

        if (assembler.add_fragment(fragment)) {
            auto msg = assembler.get_assembled_message(
                fragment.header.sender_id,
                fragment.header.message_id
            );

            if (msg.complete) {
                std::lock_guard<std::mutex> lock(handlers_mutex_);
                auto it = handlers_.find(channel_name_);
                if (it != handlers_.end()) {
                    it->second(msg);
                } else {
                    auto def = handlers_.find("default");
                    if (def != handlers_.end()) {
                        def->second(msg);
                    }
                }
                log_line(
                    "[RECEIVED] client=" + std::to_string(client_id) +
                    " msg_id=" + std::to_string(fragment.header.message_id) +
                    " size=" + std::to_string(msg.data.size())
                );

                send_ack(pipe, fragment.header.message_id, client_id);
            }
        }
    }

    log_line("\n[DISCONNECT] client_id=" + std::to_string(client_id));
    CloseHandle(pipe);
#else
void IPCReceiver::client_loop_linux(PipeHandle read_fd, PipeHandle write_fd) {
    uint16_t client_id = next_client_id++;
    IPC::MessageAssembler assembler;
    log_line("\n[CONNECT] client_id=" + std::to_string(client_id));

    while (running_) {
        IPC::Fragment fragment;
        if (!receive(read_fd, fragment))
            break;

        if (assembler.add_fragment(fragment)) {
            auto msg = assembler.get_assembled_message(
                fragment.header.sender_id,
                fragment.header.message_id
            );

            if (msg.complete) {
                std::lock_guard<std::mutex> lock(handlers_mutex_);
                auto it = handlers_.find(channel_name_);
                if (it != handlers_.end()) {
                    it->second(msg);
                } else {
                    auto def = handlers_.find("default");
                    if (def != handlers_.end()) {
                        def->second(msg);
                    }
                }
                log_line(
                    "[RECEIVED] client=" + std::to_string(client_id) +
                    " msg_id=" + std::to_string(fragment.header.message_id) +
                    " size=" + std::to_string(msg.data.size())
                );


                send_ack(write_fd, fragment.header.message_id, client_id);
            }
        }
    }

    log_line("\n[DISCONNECT] client_id=" + std::to_string(client_id));
    close(read_fd);
    close(write_fd);
#endif
}

std::string IPCReceiver::get_connect_pipe_name() const
{
    return "/tmp/ipc_transport/" + channel_name_ + "_connect";
}


bool IPCReceiver::receive(PipeHandle pipe, IPC::Fragment& fragment) {
    uint32_t packet_size = 0;

#ifdef _WIN32
    if (!read_all(pipe, &packet_size, sizeof(packet_size)))
        return false;
#else
    if (!read_all(pipe, &packet_size, sizeof(packet_size)))
        return false;
#endif

    if (packet_size < sizeof(IPC::FragmentHeader) ||
        packet_size > sizeof(IPC::FragmentHeader) + IPC::MAX_FRAGMENT_SIZE) {
        std::cerr << "[ERROR] Invalid packet_size: " << packet_size << std::endl;
        return false;
    }

    if (!read_all(pipe, &fragment.header, sizeof(fragment.header)))
        return false;

    if (fragment.header.fragment_size > IPC::MAX_FRAGMENT_SIZE) {
        std::cerr << "[ERROR] Invalid fragment_size: "
                  << fragment.header.fragment_size << std::endl;
        return false;
    }

    uint32_t expected_packet_size =
        static_cast<uint32_t>(sizeof(IPC::FragmentHeader) + fragment.header.fragment_size);

    if (packet_size != expected_packet_size) {
        std::cerr << "[ERROR] Packet size mismatch\n";
        return false;
    }

    if (fragment.header.total_fragments == 0) {
        std::cerr << "[ERROR] Invalid total_fragments\n";
        return false;
    }

    if (fragment.header.fragment_index >= fragment.header.total_fragments) {
        std::cerr << "[ERROR] Invalid fragment_index\n";
        return false;
    }

    fragment.data.resize(fragment.header.fragment_size);

    if (!fragment.data.empty()) {
        if (!read_all(pipe, fragment.data.data(), fragment.data.size()))
            return false;
    }

    return true;
}

void IPCReceiver::send_ack(PipeHandle pipe, uint32_t msg_id, uint16_t /*client_id*/) {
    IPC::Fragment ack{};
    ack.header.sender_id = 0;
    ack.header.message_id = msg_id;
    ack.header.total_size = sizeof(IPC::AckPayload);
    ack.header.total_fragments = 1;
    ack.header.fragment_index = 0;
    ack.header.message_checksum = 0;
    ack.header.fragment_size = sizeof(IPC::AckPayload);
    ack.header.flags = IPC::FLAG_ACK | IPC::FLAG_LAST;

    IPC::AckPayload payload{ msg_id };
    ack.data.resize(sizeof(payload));
    memcpy(ack.data.data(), &payload, sizeof(payload));

    send_queue_.push_immediate(pipe, ack);
}